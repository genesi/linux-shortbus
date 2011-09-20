/*
 * imx-cs42l52.c  --  SoC audio for IMX / Cirrus platform
 *
 * Author: Johan Dams, <jdmm@genesi-usa.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/soc-dai.h>

#include <mach/hardware.h>
#include <mach/clock.h>
#include <mach/audmux.h>

#include "imx-ssi.h"

#include "../codecs/cs42l52.h"


static struct imx_cs42l52_priv {
	int sysclk;
	struct platform_device *pdev;
} card_priv;

static int imx_efikasb_cs42l52_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	/* TODO: The SSI driver should figure this out for us */
	unsigned int channels = params_channels(params);
	switch (channels) {
	case 2:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xfffffffc, 0xfffffffc, 2, 0);
		break;
	case 1:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xfffffffe, 0xfffffffe, 1, 0);
		break;
	default:
		return -EINVAL;
	}

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
							SND_SOC_DAIFMT_I2S |
							SND_SOC_DAIFMT_NB_NF |
							SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0) {
		printk(KERN_ERR "can't set codec DAI configuration\n");
		return ret;
	}

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
							SND_SOC_DAIFMT_I2S |
							SND_SOC_DAIFMT_NB_NF |
							SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return ret;
	}

	/* Set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, CS42L52_SYSCLK,
							card_priv.sysclk,
							SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}
	
	return ret;
}

/* EfikaSB I2S */
static struct snd_soc_ops efikasbcs42l52_ops = {
	.hw_params = imx_efikasb_cs42l52_hw_params,
};


/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link efikasb_cs42l52_dai = {
	.name = "cs42l52",
	.stream_name = "CS42L52",
	.cpu_dai_name = "imx-ssi.1",
	.platform_name = "imx-pcm-audio.1",
	.codec_dai_name = "cs42l52-hifi",
	.codec_name = "cs42l52-codec.0-004a",
	.ops = &efikasbcs42l52_ops,
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_efikasb_cs42l52 = {
	.name = "efikasb-cs42l52",
	.dai_link = &efikasb_cs42l52_dai,
	.num_links = 1,
};

/* Configure the AUDMUX */
static int imx_audmux_config(int slave, int master)
{
	unsigned int ptcr, pdcr;
	slave = slave - 1;
	master = master - 1;

	ptcr = MXC_AUDMUX_V2_PTCR_SYN |
		MXC_AUDMUX_V2_PTCR_TFSDIR |
		MXC_AUDMUX_V2_PTCR_TFSEL(master) |
		MXC_AUDMUX_V2_PTCR_TCLKDIR |
		MXC_AUDMUX_V2_PTCR_TCSEL(master);
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(master);
	mxc_audmux_v2_configure_port(slave, ptcr, pdcr);

	ptcr = MXC_AUDMUX_V2_PTCR_SYN;
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(slave);
	mxc_audmux_v2_configure_port(master, ptcr, pdcr);

	return 0;
}

static int __devinit imx_cs42l52_probe(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;

	card_priv.pdev = pdev;

	imx_audmux_config(plat->src_port, plat->ext_port);

	card_priv.sysclk = plat->sysclk;


	return 0;
}

static int imx_cs42l52_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver imx_cs42l52_audio_driver = {
	.probe = imx_cs42l52_probe,
	.remove = imx_cs42l52_remove,
	.driver = {
		   .name = "imx-cs42l52",
		   },
};

static struct platform_device *efikasb_cs42l52_snd_device;

static int __init efikasb_cs42l52_soc_init(void)
{
	int ret;

	ret = platform_driver_register(&imx_cs42l52_audio_driver);
	if (ret)
		return -ENOMEM;
	efikasb_cs42l52_snd_device = platform_device_alloc("soc-audio", -1);

	if (!efikasb_cs42l52_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(efikasb_cs42l52_snd_device,
						&snd_soc_efikasb_cs42l52);

	ret = platform_device_add(efikasb_cs42l52_snd_device);
	if (ret)
		goto err1;

	return 0;

err1:
	pr_err("Unable to add platform device\n");
	platform_device_put(efikasb_cs42l52_snd_device);

	return ret;
}

static void __exit efikasb_cs42l52_soc_exit(void)
{
	platform_device_unregister(efikasb_cs42l52_snd_device);
}

module_init(efikasb_cs42l52_soc_init);
module_exit(efikasb_cs42l52_soc_exit);


MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");
MODULE_DESCRIPTION("ALSA SoC IMX / Cirrus");
MODULE_LICENSE("GPL");
