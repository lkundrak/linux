/*
 * Copyright 2011 Broadcom Corporation.  All rights reserved.
 * Copyright 2013 Lubomir Rintel
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available at
 * http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/mailbox.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>

#define VC_TAG_GET_TEMP		0x00030006
#define VC_TAG_GET_MAX_TEMP	0x0003000A

/* tag part of the message */
struct vc_msg_tag {
	u32 tag_id;		/* the tag ID for the temperature */
	u32 buffer_size;	/* size of the buffer (should be 8) */
	u32 request_code;	/* identifies message as a request
				 * (should be 0) */
	u32 id;			/* extra ID field (should be 0) */
	u32 val;		/* returned value of the temperature */
} __packed;

/* message structure to be sent to videocore */
struct vc_msg {
	u32 msg_size;		/* simply, sizeof(struct vc_msg) */
	u32 request_code;	/* holds various information like the
				 * success and number of bytes returned
				 * (refer to mailboxes wiki) */
	struct vc_msg_tag tag;	/* the tag structure above to make */
	u32 end_tag;		/* an end identifier, should be set to NULL */
} __packed;

enum {
	TEMP,
	MAX_TEMP,
};

static ssize_t bcm2835_get_temp(struct device *dev,
			struct device_attribute *attr,
			char *buf);
static ssize_t bcm2835_get_name(struct device *dev,
			struct device_attribute *attr,
			char *buf);

static struct platform_driver bcm2835_hwmon_driver;

static SENSOR_DEVICE_ATTR(name, S_IRUGO, bcm2835_get_name, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, bcm2835_get_temp, NULL, TEMP);
static SENSOR_DEVICE_ATTR(temp1_max, S_IRUGO, bcm2835_get_temp, NULL, MAX_TEMP);

static struct attribute *bcm2835_attributes[] = {
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	NULL,
};

static struct attribute_group bcm2835_attr_group = {
	.attrs = bcm2835_attributes,
};

static ssize_t bcm2835_get_name(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "bcm2835_hwmon\n");
}

static ssize_t bcm2835_get_temp(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int result;
	uint temp = 0;
	int index = to_sensor_dev_attr(attr)->index;
	dma_addr_t msg_bus;
	struct vc_msg *msg;
	int tag_id;

	/* determine the message type */
	if (index == TEMP)
		tag_id = VC_TAG_GET_TEMP;
	else if (index == MAX_TEMP)
		tag_id = VC_TAG_GET_MAX_TEMP;
	else
		return -EINVAL;

	msg = dma_alloc_coherent(NULL, PAGE_ALIGN(sizeof(*msg)), &msg_bus,
								GFP_KERNEL);

	/* construct the message */
	memset(&msg, 0, sizeof(*msg));
	msg->tag.tag_id = tag_id;
	msg->msg_size = sizeof(*msg);
	msg->tag.buffer_size = 8;

	/* send the message */
	result = bcm2835_mbox_property(msg_bus);

	/* check if it was all ok and return the rate in milli degrees C */
	if (result == 0 && (msg->request_code & 0x80000000))
		temp = (uint)msg->tag.val;

	dma_free_coherent(NULL, PAGE_ALIGN(sizeof(*msg)), msg, msg_bus);

	return sprintf(buf, "%u\n", temp);
}

static int bcm2835_hwmon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *hwmon_dev;

	/* create the sysfs files */
	if (sysfs_create_group(&dev->kobj, &bcm2835_attr_group)) {
		dev_err(dev, "Could not create sysfs group\n");
		return -EFAULT;
	}

	/* register the hwmon device */
	hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(hwmon_dev)) {
		dev_err(dev, "Could not register hwmon device\n");
		sysfs_remove_group(&dev->kobj, &bcm2835_attr_group);
		return PTR_ERR(hwmon_dev);
	}
	platform_set_drvdata(pdev, hwmon_dev);

	dev_info(dev, "Broadcom BCM2835 sensors\n");
	return 0;
}

static int bcm2835_hwmon_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *hwmon_dev = platform_get_drvdata(pdev);

	hwmon_device_unregister(hwmon_dev);
	sysfs_remove_group(&dev->kobj, &bcm2835_attr_group);

	return 0;
}

static const struct of_device_id bcm2835_hwmon_of_match[] = {
	{ .compatible = "brcm,bcm2835-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2835_hwmon_of_match);

static struct platform_driver bcm2835_hwmon_driver = {
	.probe = bcm2835_hwmon_probe,
	.remove = bcm2835_hwmon_remove,
	.driver = {
		.name = "bcm2835-hwmon",
		.owner = THIS_MODULE,
		.of_match_table = bcm2835_hwmon_of_match,
	},
};
module_platform_driver(bcm2835_hwmon_driver);

MODULE_AUTHOR("Dorian Peake and Lubomir Rintel");
MODULE_DESCRIPTION("BCM2835 sensors driver");
MODULE_LICENSE("GPLv2");
