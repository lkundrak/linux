#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/poll.h>
#include <linux/pci.h>

#include "seeprom.h"
#include "other.h"

int debug = 0;

module_param(debug, int, 0);

#define REBOOT2x00_MAJOR 61

/* use my own dbg macro */
#undef dbg
#define dbg(format, arg...) do { if (debug) printk(KERN_INFO "reboot2x00: " format "\n" , ## arg); } while (0)

#define bnvram ((boot_nvramDef*) (&(nvramShadow.boot_nvram)))

static int is_platform_hondo = 0;
static int is_platform_badger = 0;

static int unregister_notifier = 1;


static int reboot2x00_execute(int poweroff)
{
    char temp;
    dbg("execute: poweroff=%d", poweroff);
    
    if (is_platform_hondo)
    {
	if(poweroff)
	{
	    temp=inb(0xCF9);
	    temp|=0x02;
	    outb(temp, 0xCF9);
	    outb(temp|0x04, 0xCF9);
	}
	else
	{
	    outb(0x05, 0x70);
	    outb(0x00, 0x71);

	    temp=inb(0xCF9);
	    temp|=0x02;
	    outb(temp, 0xCF9);
	    outb(temp|0x04, 0xCF9);
	}
    }

    if (is_platform_badger)
    {
	nvram_init();
	bnvram->BadgerPowerState = (uchar)(poweroff); // 0x00 or 0x01
	nvram_update();
	write_byte_ioexpander(0xDF&read_byte_ioexpander());
	PCI_Write_CFG_Reg(0x9000, 0x90, 0x10|PCI_Read_CFG_Reg(0x9000,0x90, 1), 1);
	PCI_Write_CFG_Reg(0x9000, 0x91, 0x00, 1);
    }

    return 0;
}


static int reboot2x00_open(struct inode *inode, struct file *file)
{
    dbg("open");
    return 0;
}


static int reboot2x00_release(struct inode *inode, struct file *file)
{
    dbg("release");
    return 0;
}


static int reboot2x00_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
    int rv;
    unsigned char *kbuf;
    
    dbg("write. size=%d", count);

    /* allow root-only, independent of device permissions */
    if (!capable(CAP_SYS_RAWIO))
	return -EPERM;
    
    if (count < 0)
	return 0;
	
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
	return -ENOMEM;

    rv = count;
    
    if (copy_from_user(kbuf, buf, count)) 
    {
	kfree(kbuf);
	return -EFAULT;
    }
    
    if (count) 
    {
	switch(kbuf[0]) 
	{
	    case '0':
		dbg("reboot command received.");
		reboot2x00_execute(0);
		break;
	    case '1':
		dbg("power-off command received.");
		reboot2x00_execute(1);
		break;
	    default:
		dbg("invalid command received.");
	}
    }
    
    *off += rv;
    kfree(kbuf);
        
    return rv;
}


static int reboot2x00_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
    dbg("read: size=%d", count);
    
    // copy_to_user(buf, "wtf do you want from me ?", xxx); *off += xxx; return xxx
     
    return 0;
}


static int reboot2x00_notify_callback(struct notifier_block *nblk, unsigned long code, void *data)
{
    switch (code)
    {
	    case SYS_RESTART:
		dbg("got notified about system restart.");
		reboot2x00_execute(0);
		break;
	    case SYS_HALT:
	    case SYS_POWER_OFF:
		dbg("got notified about system poweroff.");
		reboot2x00_execute(1);
		break;
	    default:
	    	dbg("unhandled reboot code: 0x%08X", code);
    }
    
    return NOTIFY_DONE;
}



static struct notifier_block reboot2x00_notifier =
{
    reboot2x00_notify_callback,
    NULL,
    0
};

static struct file_operations reboot2x00_fops =
{
    .owner = THIS_MODULE,
    .read = reboot2x00_read,
    .write = reboot2x00_write,
    .open = reboot2x00_open,
    .release = reboot2x00_release
};


static int __init reboot2x00_init(void)
{
    int rv;
    struct pci_dev *dev = NULL;
    
    dbg("init");

    /* 
    The debug/reboot code in here does only work if the platform was booted 
    in "nsboot" mode -  e.g. hondo also supports a "WoD" pc bios mode) !!
    Our own multi stage boot loader (which is loaded in nsboot mode only) 
    will always add a fixed parameter to the kernel commandline: nsboot=1
    */ 
    if(!strstr(saved_command_line, "nsboot=1"))
    {
	dbg("platform not booted in nsboot mode. aborting.");
	return -ENXIO;  
    }
    
    dev = pci_find_device(0x1078, 0x0100, dev);
    if(dev) 
    {
	dbg("platform is badger.");
	is_platform_badger = 1; 
    }
    else 
    {
	dev = pci_find_device(0x8086, 0x7113, dev);
	if(dev) 
	{
	    
	    dbg("platform is hondo.");
	    is_platform_hondo = 1; 
	}
	else
	{
	    dbg("cannot find suitable device.");
	    return -ENXIO;  
	}
    }
    
    rv = register_reboot_notifier(&reboot2x00_notifier);
    if (rv<0) 
    {
	dbg("cannot register reboot notifyer.");
	unregister_notifier = 0;
    }
    
    rv = register_chrdev (REBOOT2x00_MAJOR, "reboot2x00", &reboot2x00_fops);
    if (rv<0) 
    {
	dbg("cannot get major number.");
	return rv;
    }
    
    return 0;
}


static void __exit reboot2x00_exit(void)
{
    dbg("exit");
    
    if (unregister_notifier) {
	unregister_reboot_notifier(&reboot2x00_notifier);
    }
    
    unregister_chrdev (REBOOT2x00_MAJOR, "reboot2x00");
    return;
}



module_init(reboot2x00_init);
module_exit(reboot2x00_exit);

MODULE_LICENSE("GPL");
