/*
 *  Copyright (C) 2010 Broadcom
 *  Copyright (C) 2013 Lubomir Rintel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This device provides a mechanism for writing to the mailboxes,
 * that are shared between the ARM and the VideoCore processor
 *
 * Parts of the driver are based on arch/arm/mach-bcm2708/vcio.c file
 * written by Gray Girling that was obtained from branch "rpi-3.6.y"
 * of git://github.com/raspberrypi/linux.git as well as documentation
 * available on the following web site:
 * https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
 */

#include <linux/module.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define BCM2835_MBOX_TIMEOUT HZ

/* Mailboxes */
#define ARM_0_MAIL0	0x00
#define ARM_0_MAIL1	0x20

/* Mailbox registers. We basically only support mailbox 0. */
#define MAIL0_RD	(ARM_0_MAIL0 + 0x00)
#define MAIL0_WRT	(ARM_0_MAIL1 + 0x00)
#define MAIL0_STA	(ARM_0_MAIL0 + 0x18)
#define MAIL0_CNF	(ARM_0_MAIL0 + 0x1C)

/* Read/write Channels. Currently we just need the property one. */
#define MBOX_CHAN_PROPERTY	8
#define MBOX_CHAN_COUNT		9

/* Status register: FIFO state. */
#define ARM_MS_FULL		0x80000000
#define ARM_MS_EMPTY		0x40000000

/* Configuration register: Enable interrupts. */
#define ARM_MC_IHAVEDATAIRQEN	0x00000001

#define MBOX_MSG(chan, data28)		(((data28) & ~0xf) | ((chan) & 0xf))
#define MBOX_CHAN(msg)			((msg) & 0xf)
#define MBOX_DATA28(msg)		((msg) & ~0xf)

static struct device *bcm2835_mbox_dev;	/* we assume there's only one! */

struct bcm2835_mbox {
	struct device *dev;
	void __iomem *regs;
	struct {
		u32 msg;
		struct completion comp;
		struct mutex lock;
	} chan[MBOX_CHAN_COUNT];
};

static irqreturn_t bcm2835_mbox_irq(int irq, void *dev_id)
{
	struct bcm2835_mbox *mbox = (struct bcm2835_mbox *)dev_id;
	struct device *dev = mbox->dev;
	int ret = IRQ_NONE;

	/* wait for the mailbox FIFO to have some data in it */
	while (!(readl(mbox->regs + MAIL0_STA) & ARM_MS_EMPTY)) {
		u32 msg = readl(mbox->regs + MAIL0_RD);
		unsigned int chan = MBOX_CHAN(msg);

		if (chan > MBOX_CHAN_COUNT) {
			dev_err(dev, "invalid channel (%d)\n", chan);
			continue;
		}

		if (mbox->chan[chan].msg) {
			dev_err(dev, "overflow on channel (%d)\n", chan);
			continue;
		}

		mbox->chan[chan].msg = msg;
		complete(&mbox->chan[chan].comp);

		ret = IRQ_HANDLED;
	}

	return ret;
}

/**
 * bcm2835_mbox_io() - send a message to BCM2835 mailbox and read a reply
 * @chan:	Channel number
 * @in28:	Message to send
 * @out28:	Where to put the response
 *
 * Sends a message to the BCM2835 mailbox.
 *
 * If the last argument is non-NULL, a response from VideoCore is awaited
 * for and written to memory pointed to by it.
 *
 * The I/O to property mailbox is more conveniently dealt with by using
 * bcm2835_mbox_property() function.
 *
 * Return: 0 in case of success, otherwise an error code
 */

int bcm2835_mbox_io(unsigned chan, u32 in28, u32 *out28)
{
	struct bcm2835_mbox *mbox;
	int timeout;
	int ret = 0;

	if (!bcm2835_mbox_dev)
		return -ENODEV;

	mbox = dev_get_drvdata(bcm2835_mbox_dev);

	device_lock(bcm2835_mbox_dev);
	/* wait for the mailbox FIFO to have some space in it */
	while (readl(mbox->regs + MAIL0_STA) & ARM_MS_FULL)
		cpu_relax();

	mutex_lock(&mbox->chan[chan].lock);
	writel(MBOX_MSG(chan, in28), mbox->regs + MAIL0_WRT);
	device_unlock(bcm2835_mbox_dev);

	/* Don't wait for a response. */
	if (out28 == NULL)
		goto out;

	timeout = wait_for_completion_timeout(&mbox->chan[chan].comp,
						BCM2835_MBOX_TIMEOUT);
	if (timeout  == 0) {
		dev_warn(bcm2835_mbox_dev, "Channel %d timeout\n", chan);
		ret = -ETIMEDOUT;
		goto out;
	}
	*out28 = MBOX_DATA28(mbox->chan[chan].msg);

out:
	mbox->chan[chan].msg = 0;
	mutex_unlock(&mbox->chan[chan].lock);

	return ret;
}
EXPORT_SYMBOL_GPL(bcm2835_mbox_io);

/**
 * bcm2835_mbox_property() - Call a BCM2835 Property mailbox service
 * @mem_bus:	DMA address of message
 *
 * Sends a message to the BCM2835 property mailbox and wait for VideoCore to
 * respond. The message memory should be obtained with dma_alloc_coherent()
 * and be filled with a properly formatted mailbox message.
 *
 * Return: 0 in case of success, otherwise an error code
 */

int bcm2835_mbox_property(dma_addr_t mem_bus)
{
	int ret;
	int val;

	wmb();
	ret = bcm2835_mbox_io(MBOX_CHAN_PROPERTY, (u32)mem_bus, (u32 *)&val);
	rmb();
	if (mem_bus != val) {
		dev_warn(bcm2835_mbox_dev, "Bad response from property mailbox\n");
		return -EIO;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(bcm2835_mbox_property);

static int bcm2835_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct bcm2835_mbox *mbox;
	int irq;
	int ret;
	int i;

	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (mbox == NULL) {
		dev_err(dev, "Failed to allocate mailbox memory\n");
		return -ENOMEM;
	}

	/* Initialize the channels */
	for (i = 0; i < MBOX_CHAN_COUNT; i++) {
		mbox->chan[i].msg = 0;
		init_completion(&mbox->chan[i].comp);
		mutex_init(&mbox->chan[i].lock);
	}

	platform_set_drvdata(pdev, mbox);
	mbox->dev = dev;

	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		dev_err(dev, "Can't get IRQ number for mailbox\n");
		return -ENODEV;
	}
	ret = devm_request_irq(dev, irq, bcm2835_mbox_irq, IRQF_SHARED,
						dev_name(dev), mbox);
	if (ret) {
		dev_err(dev, "Failed to register a mailbox IRQ handler\n");
		return -ENODEV;
	}

	mbox->regs = of_iomap(np, 0);
	if (!mbox->regs) {
		dev_err(dev, "Failed to remap mailbox regs\n");
		return -ENODEV;
	}

	/* Enable the interrupt on data reception */
	writel(ARM_MC_IHAVEDATAIRQEN, mbox->regs + MAIL0_CNF);

	dev_info(dev, "Broadcom BCM2835 mailbox IPC");
	bcm2835_mbox_dev = dev;

	return 0;
}

static int bcm2835_mbox_remove(struct platform_device *pdev)
{
	bcm2835_mbox_dev = NULL;
	return 0;
}

static const struct of_device_id bcm2835_mbox_of_match[] = {
	{ .compatible = "brcm,bcm2835-mbox", },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2835_mbox_of_match);

static struct platform_driver bcm2835_mbox_driver = {
	.driver = {
		.name = "bcm2835-mbox",
		.owner = THIS_MODULE,
		.of_match_table = bcm2835_mbox_of_match,
	},
	.probe		= bcm2835_mbox_probe,
	.remove		= bcm2835_mbox_remove,
};
module_platform_driver(bcm2835_mbox_driver);

static int __init bcm2835_mbox_init(void)
{
	return platform_driver_register(&bcm2835_mbox_driver);
}
arch_initcall(bcm2835_mbox_init);

MODULE_AUTHOR("Lubomir Rintel");
MODULE_DESCRIPTION("BCM2835 mailbox IPC driver");
MODULE_LICENSE("GPL v2");
