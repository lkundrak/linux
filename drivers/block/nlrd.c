/*
 * nlrd.c -- Neoware Linux Ram Disk Driver
 *
 * Copyright (C) 2006 Neoware Inc. 
 * http://www.neoware.com
 *
 * Author: Norbert Federa <nfedera@neoware.com>
 * Inspired by the linux Loopback device driver and the sbull driver 
 * example from the book "Linux Device Drivers" by Alessandro Rubini
 * and Jonathan Corbet, published by O'Reilly & Associates.
 *
 */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>


MODULE_LICENSE("GPL");

#define NLRD_DISKS		1

#define NLRD_MAJOR		185

#define NLRD_IOCTL_STATS	0x5300
#define NLRD_IOCTL_INIT_DISK	0x5301
#define NLRD_IOCTL_INIT_ZBYTE	0x5302

#define ZEROBYTE		CONFIG_BLK_DEV_NLRD_ZEROBYTE

struct nlrd_dev 
{
	int size;
	unsigned long nsectors;
	short users;
	spinlock_t lock;
	struct semaphore ctl_mutex;
	struct semaphore sec_mutex;
	struct request_queue *queue;
	struct gendisk *gd;
	u8 zerobyte;
	u8 zerosect[512];
	u8 **secs;
};

kmem_cache_t *nlrd_sector_cache = NULL;

static struct nlrd_dev disks[NLRD_DISKS];


// forward declarations
static int nlrd_open(struct inode *inode, struct file *filp);
static int nlrd_release(struct inode *inode, struct file *filp);
static int nlrd_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

static struct block_device_operations nlrd_ops = 
{
	.owner   = THIS_MODULE,
	.open 	 = nlrd_open,
	.release = nlrd_release,
	.ioctl	 = nlrd_ioctl,
};

#if 0
static void
nlrd_hexdump(unsigned char *buf, unsigned int len)
{
	while(len--) {
		printk("%02X", *buf++);
	}
	printk("\n");
}
#endif

static int
nlrd_transfer(struct nlrd_dev *dev, unsigned long sector, unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector*512;
	unsigned long nbytes = nsect*512;
	unsigned long s;

	if ((offset + nbytes) > dev->size) {
		printk (KERN_NOTICE "nlrd: attempt to access beyond end of device (%ld %ld)\n", offset, nbytes);
		return 1;
	}

	if (write) 
	{
		for(s=0; s<nsect; s++) 
		{
			u8 *src = buffer+s*512;

			// if the sector data consists entirely of dev->zerobyte bytes we may safely free it
			// because we return dev->zerobyte data anyway for reads if the requested sector 
			// isn't cached yet
			if (memcmp(dev->zerosect, src, 512)==0)  {
				if(dev->secs[sector+s]!=NULL) {
					kmem_cache_free(nlrd_sector_cache, dev->secs[sector+s]);
					dev->secs[sector+s]=NULL;
				}
			}
			else {
				if(dev->secs[sector+s]==NULL) {
					printk(KERN_ERR "nlrd_transfer: error no memory got preallocated for sector %lu (idx=%lu)!\n", sector+s, s);
					return 1;
				}
				memcpy(dev->secs[sector+s], src, 512);
			}
		}
	}
	else 
	{
		for(s=0; s<nsect; s++) 
		{
			if(dev->secs[sector+s]==NULL) {
				memset(buffer+s*512, dev->zerobyte, 512);
			}
			else {
				memcpy(buffer+s*512, dev->secs[sector+s], 512);
			}
		}
	}

	return 0;
}

static void 
nlrd_request(request_queue_t *q)
{
	struct request *req;
	int terr, write;
	unsigned long sector, nsect, s;


	while ((req = elv_next_request(q)) != NULL) {
		struct nlrd_dev *dev = req->rq_disk->private_data;
		if (! blk_fs_request(req)) {
			printk (KERN_NOTICE "nlrd: ignoring non blk_fs_request\n");
			end_request(req, 0);
			continue;
		}
		// we are running in atomic context, therefore we would have to do a GFP_ATOMIC allocation 
		// for the writes inside nlrd_transfer and those might fail under heavy load 
		// therefore we leave the atomic context and preallocate those sectors before calling the transfer function
		nsect = req->current_nr_sectors;
		sector = req->sector;
		write = rq_data_dir(req);
		spin_unlock_irq(q->queue_lock);
		down(&dev->sec_mutex);
		if(write) 
		{
			for(s=0; s<nsect; s++) {
				if(dev->secs[sector+s]==NULL) {
					dev->secs[sector+s] = kmem_cache_alloc(nlrd_sector_cache, GFP_KERNEL);
				}
			}
		}
		spin_lock_irq(q->queue_lock);
		terr = nlrd_transfer(dev, req->sector, req->current_nr_sectors, req->buffer, rq_data_dir(req));
		up(&dev->sec_mutex);
		end_request(req, terr==0);
	}
}


static int 
nlrd_open(struct inode *inode, struct file *filp)
{
	struct nlrd_dev *dev = inode->i_bdev->bd_disk->private_data;

	// store a pointer to the private data field of the file structure 
	// for easier access in the future (eg in nlrd_ioctl)
	filp->private_data = dev;

	// increment usage counter
	spin_lock(&dev->lock);
	dev->users++;
	spin_unlock(&dev->lock);
	return 0;
}

static int 
nlrd_release(struct inode *inode, struct file *filp)
{
	struct nlrd_dev *dev = inode->i_bdev->bd_disk->private_data;

	// decrement usage counter
	spin_lock(&dev->lock);
	dev->users--;
	spin_unlock(&dev->lock);

	return 0;
}


static unsigned long 
nlrd_get_usedsecs(struct nlrd_dev *dev)
{
	unsigned long s;
	unsigned long usedsecs = 0;
	spin_lock(&dev->lock);
	if (dev->secs) for(s=0; s<dev->nsectors; s++) if(dev->secs[s]) usedsecs++;
	spin_unlock(&dev->lock);
	return usedsecs;
}


static int 
nlrd_init_disk(struct nlrd_dev *dev, unsigned long nsectors)
{
	unsigned long secssize;
	void *mem;
	if(nsectors<1) {
		return 1;
	}

	//disallow multiple initializations
	if(dev->nsectors>0) {
		return -EBUSY;
	}
	
	memset(dev->zerosect, dev->zerobyte, 512); 

	//allocate secsize bytes for our sector hash 
	secssize = nsectors*sizeof(u8*);
	mem = vmalloc(secssize);
	if (mem == NULL) {
		printk (KERN_ERR "nlrd: error allocating memory for sector hash.\n");
		return -ENOMEM;
	}
	
	spin_lock(&dev->lock);

	dev->secs = mem;
	memset(dev->secs, 0, secssize);

	//our size field is stored in bytes
	dev->nsectors = nsectors;
	dev->size = nsectors*512;
	
	//announce new disk capacity
	set_capacity(dev->gd, nsectors);

	printk("nlrd: disk initialized: sectors=%lu size=%u zerobyte=0x%02X)\n", dev->nsectors, dev->size, dev->zerobyte);

	spin_unlock(&dev->lock);
	return 0;
}


static int 
nlrd_ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long secs=0;
	u8 zbyte;
	int err = 0;
	struct nlrd_dev *dev = filp->private_data;

	down(&dev->ctl_mutex);
	switch(cmd) 
	{
	case NLRD_IOCTL_STATS:
		secs = nlrd_get_usedsecs(dev);
		if (copy_to_user((void __user *) arg, &secs, sizeof(secs)))
			err = -EFAULT;
		break;
	case NLRD_IOCTL_INIT_ZBYTE:
		if (copy_from_user(&zbyte, (void __user *) arg, sizeof(zbyte)))
			err = -EFAULT;
		else
			dev->zerobyte = zbyte;
		break;
	case NLRD_IOCTL_INIT_DISK:
		if (copy_from_user(&secs, (void __user *) arg, sizeof(secs)))
			err = -EFAULT;
		else
			err = nlrd_init_disk(dev, secs);
		break;
	default:
		err = -ENOTTY;
	}
	up(&dev->ctl_mutex);

	return err;
}


static int __init nlrd_init(void)
{
	int i;
	printk("nlrd: driver initializing (ZB: 0x%02X) ...\n", ZEROBYTE);

	memset (disks, 0, sizeof(disks));


	// register the blockdevice. If nlrd_major is set to 0 initally
	// a dynamic major number would get assigned for us
	if (register_blkdev(NLRD_MAJOR, "nlrd")) {
		printk(KERN_ERR "nlrd: unable to register block device\n");
		return -EIO;
	}

	// create a lookaside cache from which we are going to allocate the 
	// memory for the cached sectors
	nlrd_sector_cache = kmem_cache_create("nlrd_cache", 512, 0, 0, NULL, NULL);
	if (nlrd_sector_cache == NULL) {
		printk (KERN_ERR "nlrd: error creating lookaside sector cache.\n");
		goto fail;
	}
	for(i=0; i<NLRD_DISKS; i++)
	{
		struct nlrd_dev *dev = &disks[i];

		// allocate and initalize the spinlock for the request queue
		spin_lock_init(&dev->lock);

		// initalize the ioctl mutex
		init_MUTEX(&dev->ctl_mutex);

		// initalize the mutex used for protecting the allocation routine in the request function
		init_MUTEX(&dev->sec_mutex);

		// this can be overwritten using an ioctl
		dev->zerobyte = ZEROBYTE;
		
		// set the request function to our nlrd_request
		dev->queue = blk_init_queue(nlrd_request, &dev->lock);
		if (dev->queue == NULL) {
			printk (KERN_ERR "nlrd: error initalizing request queue.\n");
			goto fail;
		}

		// our "hardware" sector size is the same as the kernel sector size: 512
		blk_queue_hardsect_size(dev->queue, 512);

		// set the queuedata field - the request queue's equivalent to the 
		// "private_data" pointer in other structures.
		dev->queue->queuedata = dev;
	
		//allocate gendisk structures
		dev->gd = alloc_disk(1);
		if (dev->gd==NULL) {
			printk (KERN_ERR "nlrd: error allocating disk.\n");
			goto fail;
		}
		dev->gd->major = NLRD_MAJOR;
		dev->gd->first_minor = i;
		dev->gd->fops = &nlrd_ops;
		dev->gd->queue = dev->queue;
		dev->gd->private_data = dev;
		
		snprintf(dev->gd->disk_name, 32, "nlrd");
	}
	
	for(i=0; i<NLRD_DISKS; i++) {
		add_disk(disks[i].gd);
	}
    
	return 0;

fail:
	if(nlrd_sector_cache) {
		kmem_cache_destroy(nlrd_sector_cache);
	}
	for(i=0; i<NLRD_DISKS; i++)
	{
		if(disks[i].gd) {
			del_gendisk(disks[i].gd);
			put_disk(disks[i].gd);
		}
		if(disks[i].queue) {
			blk_cleanup_queue(disks[i].queue);
		}
	}
	return -ENOMEM;
}

static void nlrd_exit(void)
{
	unsigned long s;
	int i;

	printk("nlrd: driver exiting ...\n");

	for(i=0; i<NLRD_DISKS; i++)
	{
		struct nlrd_dev *dev = &disks[i];
		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		blk_cleanup_queue(dev->queue);

		for(s=0; s<dev->nsectors; s++) {
			if(dev->secs[s]!=NULL) {
				kmem_cache_free(nlrd_sector_cache, dev->secs[s]);
			}
		}
		if (dev->secs) {
			vfree(dev->secs);
		}
	}
	kmem_cache_destroy(nlrd_sector_cache);
	unregister_blkdev(NLRD_MAJOR, "nlrd");
}
	
module_init(nlrd_init);
module_exit(nlrd_exit);
