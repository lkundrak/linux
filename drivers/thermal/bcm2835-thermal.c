/*
 * Copyright 2011 Broadcom Corporation.	All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/dma-mapping.h>
#include <linux/mailbox.h>

#define VC_TAG_GET_TEMP		0x00030006
#define VC_TAG_GET_MAX_TEMP	0x0003000A

/* tag part of the message */
struct bcm2835_msg_tag {
	uint32_t tag_id;	/* the tag ID for the temperature */
	uint32_t buffer_size;	/* size of the buffer (should be 8) */
	uint32_t request_code;	/* identifies message as a request
				 * (should be 0) */
	uint32_t id;		/* extra ID field (should be 0) */
	uint32_t val;		/* returned value of the temperature */
} __packed;

/* message structure to be sent to videocore */
struct bcm2835_msg {
	uint32_t msg_size;	/* simply, sizeof(struct bcm2835_msg) */
	uint32_t request_code;	/* holds various information like the
				 * success and number of bytes returned */
	struct bcm2835_msg_tag tag;	/* the tag structure above to make */
	uint32_t end_tag;	/* an end identifier, should be set to NULL */
} __packed;

static int bcm2835_thermal_get_temp_or_max(struct thermal_zone_device *thermal,
					unsigned long *temp, unsigned tag_id)
{
	int result = -1, retry = 3;
	dma_addr_t msg_bus;
	struct bcm2835_msg *msg;
	struct device *mbox = (struct device *)thermal->devdata;

	msg = dma_alloc_coherent(NULL, PAGE_ALIGN(sizeof(*msg)), &msg_bus,
								GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	*temp = 0;
	while (result != 0 && retry-- > 0) {

		/* wipe all previous message data */
		memset(msg, 0, sizeof(*msg));

		/* prepare message */
		msg->msg_size = sizeof(*msg);
		msg->tag.buffer_size = 8;
		msg->tag.tag_id = tag_id;

		/* send the message */
		result = bcm2835_mbox_property(mbox, msg_bus);
		if (!(msg->request_code & 0x80000000))
			result = -1;
	}

	/* check if it was all ok and return the rate in milli degrees C */
	if (result == 0)
		*temp = (uint)msg->tag.val;
	else
		dev_err(&thermal->device, "Failed to get temperature\n");

	dma_free_coherent(NULL, PAGE_ALIGN(sizeof(*msg)), msg, msg_bus);

	return result;
}

static int bcm2835_thermal_get_temp(struct thermal_zone_device *thermal,
						unsigned long *temp)
{
	return bcm2835_thermal_get_temp_or_max(thermal, temp, VC_TAG_GET_TEMP);
}

static int bcm2835_thermal_get_max_temp(struct thermal_zone_device *thermal,
					int trip_num, unsigned long *temp)
{
	return bcm2835_thermal_get_temp_or_max(thermal, temp,
					VC_TAG_GET_MAX_TEMP);
}

static int bcm2835_thermal_get_trip_type(struct thermal_zone_device *thermal,
			int trip_num, enum thermal_trip_type *trip_type)
{
	*trip_type = THERMAL_TRIP_HOT;
	return 0;
}

static int bcm2835_thermal_get_mode(struct thermal_zone_device *thermal,
				enum thermal_device_mode *dev_mode)
{
	*dev_mode = THERMAL_DEVICE_ENABLED;
	return 0;
}

static struct thermal_zone_device_ops ops  = {
	.get_temp = bcm2835_thermal_get_temp,
	.get_trip_temp = bcm2835_thermal_get_max_temp,
	.get_trip_type = bcm2835_thermal_get_trip_type,
	.get_mode = bcm2835_thermal_get_mode,
};

static int bcm2835_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal;
	struct device *dev = &pdev->dev;
	struct device *mbox;
	int ret;

	ret = bcm2835_mbox_init(&mbox);
	if (ret != 0)
		return ret;

	thermal = thermal_zone_device_register(
				"bcm2835_thermal", 1, 0, mbox,
				&ops, NULL, 1000, 1000);
	if (!thermal) {
		dev_err(dev, "Unable to register the thermal device\n");
		return -ENODEV;
	}

	dev_info(dev, "Broadcom BCM2835 thermal sensor\n");
	platform_set_drvdata(pdev, thermal);

	return 0;
}

static int bcm2835_thermal_remove(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal = platform_get_drvdata(pdev);
	thermal_zone_device_unregister(thermal);

	return 0;
}

static const struct of_device_id bcm2835_thermal_of_match[] = {
	{ .compatible = "brcm,bcm2835-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2835_thermal_of_match);

/* Thermal Driver */
static struct platform_driver bcm2835_thermal_driver = {
	.probe = bcm2835_thermal_probe,
	.remove = bcm2835_thermal_remove,
	.driver = {
		.name = "bcm2835-thermal",
		.owner = THIS_MODULE,
		.of_match_table = bcm2835_thermal_of_match,
	},
};
module_platform_driver(bcm2835_thermal_driver);

MODULE_AUTHOR("Dorian Peake and Lubomir Rintel");
MODULE_DESCRIPTION("BCM2835 thermal sensor driver");
MODULE_LICENSE("GPL v2");
