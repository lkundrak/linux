/*
 * DesignWare HS OTG Controller driver for RaspberryPi
 *
 * Copyright (C) 2004-2013 Synopsys, Inc.
 * Copyright (C) 2013 Stephen Warren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Provides the initialization and cleanup entry points for the DWC_otg
 * platform driver
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <linux/usb/hcd.h>
#include <linux/usb/ch11.h>

#include "core.h"
#include "hcd.h"

static const struct dwc2_core_params dwc2_bcm2835_params = {
	.otg_cap			= 0,	/* HNP/SRP capable */
	.otg_ver			= 0,	/* 1.3 */
	.dma_enable			= 1,
	.dma_desc_enable		= 0,
	.speed				= 0,	/* High Speed */
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= 1,
	.host_rx_fifo_size		= 774,	/* 774 DWORDs */
	.host_nperio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.host_perio_tx_fifo_size	= 512,	/* 512 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= 511,
	.host_channels			= 8,
	.phy_type			= 1,	/* UTMI */
	.phy_utmi_width			= 8,	/* 8 bits */
	.phy_ulpi_ddr			= 0,	/* Single */
	.phy_ulpi_ext_vbus		= 0,
	.i2c_enable			= 0,
	.ulpi_fs_ls			= 0,
	.host_support_fs_ls_low_power	= 0,
	.host_ls_low_power_phy_clk	= 0,	/* 48 MHz */
	.ts_dline			= 0,
	.reload_ctl			= 0,
	.ahb_single			= 0,
};

/**
 * dwc2_driver_remove() - Called when the DWC_otg core is unregistered with the
 * DWC_otg driver
 *
 * @dev: Platform device
 *
 * This routine is called, for example, when the rmmod command is executed. The
 * device may or may not be electrically present. If it is present, the driver
 * stops device processing. Any resources used on behalf of this device are
 * freed.
 */
static int dwc2_driver_remove(struct platform_device *pdev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(%p)\n", __func__, pdev);

	dwc2_hcd_remove(hsotg);

	return 0;
}

static const struct platform_device_id dwc2_platform_ids[] = {
	{
		.name = "bcm2835-usb",
		.driver_data = (kernel_ulong_t)&dwc2_bcm2835_params
	},
	{}
};
MODULE_DEVICE_TABLE(platform, dwc2_platform_ids);

static const struct of_device_id dwc2_of_match[] = {
	{ .compatible = "brcm,bcm2835-usb", .data = &dwc2_bcm2835_params },
	{}
};
MODULE_DEVICE_TABLE(of, dwc2_of_match);

/**
 * dwc2_driver_probe() - Called when the DWC_otg core is bound to the DWC_otg
 * driver
 *
 * @dev: Platform device
 *
 * This routine creates the driver components required to control the device
 * (core, HCD, and PCD) and initializes the device. The driver components are
 * stored in a dwc2_hsotg structure. A reference to the dwc2_hsotg is saved
 * in the device private data. This allows the driver to access the dwc2_hsotg
 * structure on subsequent calls to driver methods for this device.
 */
static int dwc2_driver_probe(struct platform_device *pdev)
{
	const struct dwc2_core_params *params = NULL;
	struct dwc2_hsotg *hsotg;
	struct resource *res;
	int retval;
	int irq;

	dev_dbg(&pdev->dev, "%s(%p)\n", __func__, pdev);

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_device(dwc2_of_match, &pdev->dev);
		if (match)
			params = match->data;
	} else {
		params = (void *)pdev->id_entry->driver_data;
	}
	if (!params)
		return -ENODEV;

	hsotg = devm_kzalloc(&pdev->dev, sizeof(*hsotg), GFP_KERNEL);
	if (!hsotg)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		return -EINVAL;
	}

	hsotg->dev = &pdev->dev;
	hsotg->regs = devm_ioremap_nocache(&pdev->dev, res->start,
					   resource_size(res));
	if (!hsotg->regs)
		return -ENOMEM;

	dev_dbg(&pdev->dev, "mapped PA %08lx to VA %p\n",
		(unsigned long)res->start, hsotg->regs);

	/* Set device flags indicating whether the HCD supports DMA */
	if (params->dma_enable > 0) {
		if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(31)) < 0)
			dev_warn(&pdev->dev,
				 "can't enable workaround for >2GB RAM\n");
		if (dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(31)) < 0)
			dev_warn(&pdev->dev,
				 "can't enable workaround for >2GB RAM\n");
	} else {
		dma_set_mask(&pdev->dev, 0);
		dma_set_coherent_mask(&pdev->dev, 0);
	}

	/* Initialize the HCD */
	retval = dwc2_hcd_init(hsotg, irq, params);
	if (retval)
		return retval;

	platform_set_drvdata(pdev, hsotg);
	dev_dbg(&pdev->dev, "hsotg=%p\n", hsotg);

	return 0;
}

static struct platform_driver dwc2_platform_driver = {
	.id_table	= dwc2_platform_ids,
	.probe		= dwc2_driver_probe,
	.remove		= dwc2_driver_remove,
	.driver		= {
		.name		= "dwc2-drd",
		.of_match_table	= dwc2_of_match,
	},
};
module_platform_driver(dwc2_platform_driver);

MODULE_DESCRIPTION("DESIGNWARE HS OTG Platform Bus Glue");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_LICENSE("Dual BSD/GPL");
