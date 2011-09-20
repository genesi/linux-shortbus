/*
 * cs42l52.h -- CS42L52 ALSA SoC audio driver
 *
 * Copyright 2010 CirrusLogic, Inc. 
 *
 * Author: Brian Austin <brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * Revision history
 * Nov  2007   Initial version.
 * Feb 2010 	Update to latest asoc git tree
 */

#ifndef __CS42L52_H__
#define __CS42L52_H__

#include <linux/types.h>
#include <sound/pcm.h>


#define CS42L52_NAME 		"CS42L52"
#define CS42L52_DEFAULT_CLK		12000000
#define CS42L52_MIN_CLK		11000000
#define CS42L52_MAX_CLK		27000000
#define CS42L52_DEFAULT_FORMAT 	SNDRV_PCM_FMTBIT_S16_LE
#define CS42L52_DEFAULT_MAX_CHANS	2
#define CS42L52_SYSCLK  		1


#define CS42L52_CHIP_SWICTH 	(1 << 17)
#define CS42L52_ALL_IN_ONE 		(1 << 16)
#define CS42L52_CHIP_ONE    	0x00
#define CS42L52_CHIP_TWO    	0x01
#define CS42L52_CHIP_THR    	0x02
#define CS42L52_CHIP_MASK   	0x0f

/*
 *CS42L52 internal registers
 */
#define CS42L52_CHIP	0x01
#define CHIP_ID   		0xE0
#define CHIP_ID_MASK		0xF8
#define CHIP_REV_A0		0x00
#define CHIP_REV_A1		0x01
#define CHIP_REV_B0		0x02
#define CHIP_REV_MASK		0x03

#define PWRCTL1	0x02  //0x01
#define PWRCTL1_PDN_CHRG		(1 << 7)
#define PWRCTL1_PDN_PGAB		(1 << 4)
#define PWRCTL1_PDN_PGAB_SHIFT	4
#define PWRCTL1_PDN_PGAA   	(1 << 3)
#define PWRCTL1_PDN_PGAA_SHIFT	3
#define PWRCTL1_PDN_ADCB		(1 << 2)
#define PWRCTL1_PDN_ADCB_SHIFT	2
#define PWRCTL1_PDN_ADCA   	(1 << 1)
#define PWRCTL1_PDN_ADCA_SHIFT	1
#define PWRCTL1_PDN_CODEC	(1 << 0)

#define PWRCTL2	0x03 //0x07
#define PWRCTL2_OVRDB		(1 << 4)
#define PWRCTL2_OVRDA		(1 << 3)
#define	PWRCTL2_PDN_MICB		(1 << 2)
#define PWRCTL2_PDN_MICB_SHIFT	2
#define PWRCTL2_PDN_MICA         (1 << 1)
#define PWRCTL2_PDN_MICA_SHIFT	1
#define PWRCTL2_PDN_MICBIAS	(1 << 0)
#define PWRCTL2_PDN_MICBIAS_SHIFT	0

#define PWRCTL3	0x04 //0x08
#define PWRCTL3_HPB_PDN_SHIFT	6
#define PWRCTL3_HPB_ON_LOW	0x00
#define PWRCTL3_HPB_ON_HIGH	0x01
#define PWRCTL3_HPB_ALWAYS_ON	0x02
#define PWRCTL3_HPB_ALWAYS_OFF	0x03
#define PWRCTL3_HPA_PDN_SHIFT    4
#define PWRCTL3_HPA_ON_LOW       0x00
#define PWRCTL3_HPA_ON_HIGH      0x01
#define PWRCTL3_HPA_ALWAYS_ON    0x02
#define PWRCTL3_HPA_ALWAYS_OFF   0x03
#define PWRCTL3_SPKB_PDN_SHIFT   2
#define PWRCTL3_SPKB_ON_LOW       0x00
#define PWRCTL3_SPKB_ON_HIGH      0x01
#define PWRCTL3_SPKB_ALWAYS_ON    0x02
#define PWRCTL3_SPKB_ALWAYS_OFF   0x03
#define PWRCTL3_SPKA_PDN_SHIFT   0
#define PWRCTL3_SPKA_ON_LOW       0x00
#define PWRCTL3_SPKA_ON_HIGH      0x01
#define PWRCTL3_SPKA_ALWAYS_ON    0x02
#define PWRCTL3_SPKA_ALWAYS_OFF   0x03
#define DEFAULT_OUTPUT_STATE	 0x05
#define PWRCTL3_CONF_MASK	0x03



#define CLK_CTL 	0x05 //0xa0
#define CLK_CTL_AUTODECT_ENABLE	(1 << 7)
#define CLK_CTL_SPEED_SHIFT	5
#define CLK_CTL_S_DS_MODE	0x00
#define CLK_CTL_S_SS_MODE	0x01
#define CLK_CTL_S_HS_MODE	0x02
#define CLK_CTL_S_QS_MODE 	0x03
#define CLK_CTL_32K_SR_SHIFT	4
#define CLK_CTL_32K_SR		1
#define CLK_CTL_NOT_32K		0
#define CLK_CTL_27M_MCLK_SHIFT	3
#define CLK_CTL_27M_MCLK	1
#define CLK_CTL_NOT_27M		0
#define CLK_CTL_RATIO_SHIFT	1
#define CLK_CTL_RATIO_128	0x00
#define CLK_CTL_RATIO_125	0x01
#define CLK_CTL_RATIO_132	0x02
#define CLK_CTL_RATIO_136	0x03
#define CLK_CTL_MCLKDIV2	1
#define CLK_CTL_NOT_MCLKDIV2	0


#define IFACE_CTL1 0x06 //0
#define IFACE_CTL1_MASTER		(1 << 7)
#define IFACE_CTL1_INV_SCLK		(1 << 6)
#define IFACE_CTL1_ADC_FMT_I2S		(1 << 5)
#define IFACE_CTL1_ADC_FMT_LEFT_J 	(0 << 5)
#define IFACE_CTL1_DSP_MODE_EN		(1 << 4)
#define IFACE_CTL1_DAC_FMT_LEFT_J 	(0 << 2)
#define IFACE_CTL1_DAC_FMT_I2S  	(1 << 2)
#define IFACE_CTL1_DAC_FMT_RIGHT_J 	(2 << 2)
#define IFACE_CTL1_WL_32BIT		(0x00)
#define IFACE_CTL1_WL_24BIT		(0x01)
#define IFACE_CTL1_WL_20BIT		(0x02)
#define IFACE_CTL1_WL_16BIT		(0x03)
#define IFACE_CTL1_WL_MASK		0xFFFF


#define IFACE_CTL2 0x07 //0
#define IFACE_CTL2_SC_MC_EQ 		(1 << 6)
#define IFACE_CTL2_LOOPBACK		(1 << 5)
#define IFACE_CTL2_S_MODE_OUTPUT_EN 	(0 << 4)
#define IFACE_CTL2_S_MODE_OUTPUT_HIZ 	(1 << 4)
#define IFACE_CTL2_HP_SW_INV		(1 << 3)
#define IFACE_CTL2_MIC_BIAS_5X 	0x00
#define IFACE_CTL2_MIC_BIAS_6X  0x01
#define IFACE_CTL2_MIC_BIAS_7X	0x02
#define IFACE_CTL2_MIC_BIAS_8X	0x03
#define IFACE_CTL2_MIC_BIAS_83X	0x04
#define	IFACE_CTL2_MIC_BIAS_91X	0x05

#define ADC_PGA_A	0x08 //0x81
#define ADC_PGA_B 	0x09 //0x81
#define ADC_SEL_SHIFT		5
#define PGA_SEL_SHIFT           0
#define ADC_SEL_AIN1		0x00
#define ADC_SEL_AIN2		0x01
#define ADC_SEL_AIN3		0x02
#define ADC_SEL_AIN4		0x03
#define ADC_SEL_PGA		0x04
#define PGA_SEL_NONE		0x00
#define PGA_SEL_AIN1		0x01
#define PGA_SEL_AIN2		0x02
#define PGA_SEL_AIN3		0x04
#define PGA_SEL_AIN4		0x08
#define PGA_SEL_MIC		0x10
#define PGA_SEL_MIC_AIN1	0x11
#define PGA_SEL_MIC_AIN1_AIN2	0x13

#define ANALOG_HPF_CTL	0x0A //0xa5
#define HPF_CTL_ANLGSFTB		(1 << 3)
#define HPF_CTL_ANLGSFTA                (1 << 0)


#define ADC_HPF_FREQ	0x0B //0
#define ADC_MISC_CTL	0x0C //0
#define ADC_MISC_CTL_SOURCE_DSP		(1 << 6)


#define PB_CTL1	0x0D //0x60
#define PB_CTL1_HP_GAIN_SHIFT	5
#define PB_CTL1_HP_GAIN_03959	0x00
#define PB_CTL1_HP_GAIN_04571	0x01
#define PB_CTL1_HP_GAIN_05111	0x02
#define PB_CTL1_HP_GAIN_06047	0x03
#define PB_CTL1_HP_GAIN_07099	0x04
#define PB_CTL1_HP_GAIN_08399	0x05
#define PB_CTL1_HP_GAIN_10000 	0x06
#define PB_CTL1_HP_GAIN_11430	0x07
#define PB_CTL1_INV_PCMB	(1 << 3)
#define PB_CTL1_INV_PCMA	(1 << 2)
#define PB_CTL1_MSTB_MUTE	(1 << 1)
#define PB_CTL1_MSTA_MUTE	(1 << 0)
#define PB_CTL1_MUTE_MASK	0xFFFD

#define MISC_CTL	0x0E //0x02
#define MISC_CTL_DEEMPH		(1 << 2)
#define MISC_CTL_DIGSFT		(1 << 1)
#define MISC_CTL_DIGZC		(1 << 0)


#define PB_CTL2	0x0F //0
#define PB_CTL2_HPB_MUTE	(1 << 7)
#define PB_CTL2_HPA_MUTE	(1 << 6)
#define PB_CTL2_SPKB_MUTE	(1 << 5)
#define PB_CTL2_SPKA_MUTE	(1 << 4)
#define PB_CTL2_SPK_SWAP	(1 << 2)
#define PB_CTL2_SPK_MONO	(1 << 1)
#define PB_CTL2_SPK_MUTE50	(1 << 0)

#define	MICA_CTL	0x10 //0
#define MICB_CTL  0x11 //0
#define	MIC_CTL_SEL_MIC1	(0 << 6)
#define MIC_CTL_SEL_MIC2	(1 << 6)
#define MIC_CTL_SEL_DIFF	(1 << 5)

#define PGAA_CTL	0x12 //0
#define PGAB_CTL 	0x13 //0
#define PGAX_CTL_VOL_12DB	24
#define PGAX_CTL_VOL_6DB	12 /*step size 0.5db*/

#define PASSTHRUA_VOL	0x14 //0
#define PASSTHRUB_VOL     0x15 //0

#define ADCA_VOL	0x16 //0
#define ADCB_VOL	0x17 //0
#define ADCX_VOL_24DB		24 /*step size 1db*/
#define ADCX_VOL_12DB		12
#define ADCX_VOL_6DB		6

#define ADCA_MIXER_VOL	0x18 // 0x80
#define ADCB_MIXER_VOL	0x19 //0x80
#define ADC_MIXER_VOL_12DB	0x18

#define PCMA_MIXER_VOL	0x1A //0
#define PCMB_MIXER_VOL	0x1B //0

#define BEEP_FREQ	0x1C //0
#define BEEP_VOL	0x1D //0
#define BEEP_VOL_12DB	0x06


#define BEEP_TONE_CTL	0x1E //0

#define TONE_CTL	0x1F //0x88

#define MASTERA_VOL	0x20 //0
#define MASTERB_VOL	0x21 //0

#define HPA_VOL	0x22 //0
#define HPB_VOL	0x23 //0
#define DEFAULT_HP_VOL	0xF0

#define SPKA_VOL	0x24 //0
#define SPKB_VOL	0x25 //0
#define DEFAULT_SPK_VOL	0xF0

#define ADC_PCM_MIXER	0x26 //0

#define LIMITER_CTL1	0x27 //0
#define LIMITER_CTL2	0x28 //0x7f
#define LIMITER_AT_RATE	0x29 //0xc0

#define ALC_CTL	0x2A //0
#define ALC_CTL_ALCB_ENABLE	(1 << 7)
#define ALC_CTL_ALCA_ENABLE	(1 << 6)
#define ALC_CTL_FASTEST_ATTACK	0

#define ALC_RATE	0x2B //0x3f
#define ALC_SLOWEST_RELEASE	0x3F

#define ALC_THRESHOLD	0x2C //0
#define ALC_MAX_RATE_SHIFT	5
#define ALC_MIN_RATE_SHIFT	2
#define ALC_RATE_0DB	0
#define ALC_RATE_3DB	1
#define ALC_RATE_6DB	2

#define NOISE_GATE_CTL	0x2D //0
#define NG_ENABLE	(1 << 6)
#define NG_THRESHOLD_SHIFT	2
#define NG_MIN_70DB	2
#define NG_DELAY_SHIFT	0
#define NG_DELAY_100MS	1

#define CLK_STATUS	0x2E //0
#define BATT_COMPEN	0x2F //0

#define BATT_LEVEL	0x30 //0
#define SPK_STATUS	0x31 //0
#define SPK_STATUS_PIN_SHIFT		3
#define SPK_STATUS_PIN_HIGH		1

#define TEM_CTL	0x32 //0x3b
#define THE_FOLDBACK	0x33 //0
#define CHARGE_PUMP	0x34 //0x5f


#define CS42L52_CACHEREGNUM	56

#endif
