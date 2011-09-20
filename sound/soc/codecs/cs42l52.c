/*
 * cs42l52.c -- CS42L52 ALSA SoC audio driver
 *
 * Copyright 2011 CirrusLogic, Inc. 
 *
 * Author: Brian Austin <brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "cs42l52.h"

struct sp_config {
	u8 spc, format, spfs;
	u32 srate;
};

struct  cs42l52_private {
	enum snd_soc_control_type control_type;
	void *control_data;
	u8 reg_cache[CS42L52_CACHEREGNUM];
	u32 sysclk;		/* external MCLK */
	u8 mclksel;		/* MCLKx */
	u32 mclk;		/* internal MCLK */
	u8 flags;
	struct sp_config config;
};

/*
 * CS42L52 register default value
 */

static const u8 cs42l52_reg[] = {
	0x00, 0xE0, 0x01, 0x07, 0x05, /*4*/
	0xa0, 0x00, 0x00, 0x81, /*8*/
	0x81, 0xa5, 0x00, 0x00, /*12*/
	0x60, 0x02, 0x00, 0x00, /*16*/
	0x00, 0x00, 0x00, 0x00, /*20*/
	0x00, 0x00, 0x00, 0x80, /*24*/
	0x80, 0x00, 0x00, 0x00, /*28*/
	0x00, 0x00, 0x88, 0x00, /*32*/
	0x00, 0x00, 0x00, 0x00, /*36*/
	0x00, 0x00, 0x00, 0x7f, /*40*/
	0xc0, 0x00, 0x3f, 0x00, /*44*/
	0x00, 0x00, 0x00, 0x00, /*48*/
	0x00, 0x3b, 0x00, 0x5f, /*52*/
};

static inline int cs42l52_read_reg_cache(struct snd_soc_codec *codec,
		u_int reg)
{
	u8 *cache = codec->reg_cache;

	return reg > CS42L52_CACHEREGNUM ? -EINVAL : cache[reg];
}

static inline void cs42l52_write_reg_cache(struct snd_soc_codec *codec,
		u_int reg, u_int val)
{
	u8 *cache = codec->reg_cache;
	
	if(reg > CS42L52_CACHEREGNUM)
		return;
	cache[reg] = val & 0xff;
}

static inline int cs42l52_get_revison(struct snd_soc_codec *codec)
{
	u8 data;
        u8 addr;
	int ret;

	struct cs42l52_private *info = snd_soc_codec_get_drvdata(codec);

		
	if(codec->hw_write(codec->control_data, &addr, 1) == 1)
	{
		if(codec->hw_read(codec->control_data, 1) == 1)
		{
			if((data & CHIP_ID_MASK) != CHIP_ID )
			{
				ret = -ENODEV;
			}
		}
		else
			ret = -EIO;
	}
	else
		ret = -EIO;

	return ret < 0 ? ret : data;
}

/**
 * snd_soc_get_volsw - single mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int cs42l52_get_volsw(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;

	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	int min = mc->min;
	int mmax = (max > min) ? max:min;
	unsigned int mask = (1 << fls(mmax)) - 1;

	ucontrol->value.integer.value[0] =
	    ((snd_soc_read(codec, reg) >> shift) - min) & mask;
	if (shift != rshift)
		ucontrol->value.integer.value[1] =
		    ((snd_soc_read(codec, reg) >> rshift) - min) & mask;

	return 0;
}

/**
 * snd_soc_put_volsw - single mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int cs42l52_put_volsw(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;

	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	int min = mc->min;
	int mmax = (max > min) ? max:min;
	unsigned int mask = (1 << fls(mmax)) - 1;
	unsigned short val, val2, val_mask;

	val = ((ucontrol->value.integer.value[0] + min) & mask);

	val_mask = mask << shift;
	val = val << shift;
	if (shift != rshift) {
		val2 = ((ucontrol->value.integer.value[1] + min) & mask);
		val_mask |= mask << rshift;
		val |= val2 << rshift;
	}
	return snd_soc_update_bits(codec, reg, val_mask, val);
}

/**
 * snd_soc_info_volsw_2r - double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double mixer control that
 * spans 2 codec registers.
 *
 * Returns 0 for success.
 */
int cs42l52_info_volsw_2r(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;

	int max = mc->max;

	if (max == 1)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;
	return 0;
}

/**
 * snd_soc_get_volsw_2r - double mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int cs42l52_get_volsw_2r(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;

	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	int max = mc->max;
	int min = mc->min;
	int mmax = (max > min) ? max:min;
	unsigned int mask = (1 << fls(mmax)) - 1;
	int val, val2;

	val = snd_soc_read(codec, reg);
	val2 = snd_soc_read(codec, reg2);
	ucontrol->value.integer.value[0] = (val - min) & mask;
	ucontrol->value.integer.value[1] = (val2 - min) & mask;
	return 0;
}

/**
 * snd_soc_put_volsw_2r - double mixer set callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int cs42l52_put_volsw_2r(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;

	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	int max = mc->max;
	int min = mc->min;
	int mmax = (max > min) ? max:min;
	unsigned int mask = (1 << fls(mmax)) - 1;
	int err;
	unsigned short val, val2;

	val = (ucontrol->value.integer.value[0] + min) & mask;
	val2 = (ucontrol->value.integer.value[1] + min) & mask;

	if ((err = snd_soc_update_bits(codec, reg, mask, val)) < 0)
		return err;

	return snd_soc_update_bits(codec, reg2, mask, val2);
}

#define SOC_SINGLE_S8_C_TLV(xname, xreg, xshift, xmax, xmin, tlv_array) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
        .access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
                  SNDRV_CTL_ELEM_ACCESS_READWRITE, \
        .info = snd_soc_info_volsw, .get = cs42l52_get_volsw,\
        .put = cs42l52_put_volsw, .tlv.p  = (tlv_array),\
        .private_value = (unsigned long)&(struct soc_mixer_control) \
		{.reg = xreg, .shift = xshift, .rshift = xshift, \
		.max = xmax, .min = xmin} }

#define SOC_DOUBLE_R_S8_C_TLV(xname, xreg, xrreg, xmax, xmin, tlv_array) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
        .access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
                  SNDRV_CTL_ELEM_ACCESS_READWRITE, \
        .info = cs42l52_info_volsw_2r, \
        .get = cs42l52_get_volsw_2r, .put = cs42l52_put_volsw_2r, \
        .tlv.p  = (tlv_array), \
        .private_value = (unsigned long)&(struct soc_mixer_control) \
		{.reg = xreg, .rreg = xrreg, .max = xmax, .min = xmin} }

/* Analog Input PGA Mux */
static const char *cs42l52_pgaa_text[] = { "INPUT1A", "INPUT2A", "INPUT3A", "INPUT4A", "MICA" };
static const char *cs42l52_pgab_text[] = { "INPUT1B", "INPUT2B", "INPUT3B", "INPUT4B", "MICB" };

static const struct soc_enum pgaa_enum = 
	SOC_ENUM_SINGLE(ADC_PGA_A, 0, 
		ARRAY_SIZE(cs42l52_pgaa_text), cs42l52_pgaa_text);

static const struct soc_enum pgab_enum = 
	SOC_ENUM_SINGLE(ADC_PGA_B, 0,
		ARRAY_SIZE (cs42l52_pgab_text), cs42l52_pgab_text);

static const struct snd_kcontrol_new pgaa_mux =
SOC_DAPM_ENUM("Left Analog Input Capture Mux", pgaa_enum);

static const struct snd_kcontrol_new pgab_mux =
SOC_DAPM_ENUM("Right Analog Input Capture Mux", pgab_enum);

/*
    HP,LO Analog Volume TLV
    -76dB ... -50 dB in 2dB steps 
    -50dB ... 12dB in 1dB steps 
*/
static const unsigned int hpaloa_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 13, TLV_DB_SCALE_ITEM(-7600, 200, 0),
	14,75, TLV_DB_SCALE_ITEM(-4900, 100, 0),
};

/* -102dB ... 12 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(hl_tlv, -10200, 50, 0);

/* -96dB ... 12 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(ipd_tlv, -9600, 100, 0);

/* -6dB ... 12 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(micpga_tlv, -600, 50, 0);

/* 
    HL, ESL, SPK, Limiter Threshold/Cushion TLV
    0dB -12 dB in -3dB steps 
    -12dB -30dB in -6dB steps 
*/
static const unsigned int limiter_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(-3000, 600, 0),
	3, 7, TLV_DB_SCALE_ITEM(-1200, 300, 0),
};

/*
    * Stereo Mixer Input Attenuation (regs 35h-54h) TLV
    * Mono Mixer Input Attenuation (regs 56h-5Dh)

    -62dB ... 0dB in 1dB steps, < -62dB = mute
*/
static const DECLARE_TLV_DB_SCALE(attn_tlv, -6300, 100, 1);

/* NG */
static const char *cs42l52_ng_delay_text[] = 
	{ "50ms", "100ms", "150ms", "200ms" };

static const struct soc_enum ng_delay_enum = 
	SOC_ENUM_SINGLE(NOISE_GATE_CTL, 0,
		ARRAY_SIZE (cs42l52_ng_delay_text), cs42l52_ng_delay_text);
	
static const char *cs42l52_ng_type_text[] = 
	{"Apply Specific", "Apply All"};
	
static const struct soc_enum ng_type_enum = 
	SOC_ENUM_SINGLE(NOISE_GATE_CTL, 6,
		ARRAY_SIZE (cs42l52_ng_type_text), cs42l52_ng_type_text);


static const struct snd_kcontrol_new cs42l52_snd_controls[] = {
  
SOC_DOUBLE_R_S8_C_TLV("Master Playback Volume", MASTERA_VOL,
		       MASTERB_VOL, 0xe4, 0x34, hl_tlv),

/* Headphone */		   
SOC_DOUBLE_R_S8_C_TLV("HP Digital Playback Volume", HPA_VOL, 
		       HPB_VOL, 0xff, 0x1, hl_tlv),
		       
SOC_SINGLE_S8_C_TLV("HP Analog Playback Volume", 
		     PB_CTL1, 5, 7, 0, hpaloa_tlv),
		     
SOC_SINGLE_S8_C_TLV("HP Digital Playback Switch", 
		     PB_CTL2, 6, 7, 1, 1),

/* Speaker */
SOC_DOUBLE_R_S8_C_TLV("Speaker Playback Volume", SPKA_VOL,
		       SPKB_VOL, 0xff, 0x1, hl_tlv),
SOC_SINGLE_S8_C_TLV("Speaker Playback Switch", 
		     PB_CTL2, 4, 5, 1, 1),		       

/* Passthrough */		       
SOC_DOUBLE_R_S8_C_TLV("Passthru Playback Volume", PASSTHRUA_VOL, 
		      PASSTHRUB_VOL, 0x90, 0x88, hl_tlv),
SOC_SINGLE("Passthru Playback Switch", MISC_CTL, 4, 5, 1),		    
	    
/* Mic LineIN */		      
SOC_DOUBLE_R_S8_C_TLV("Mic Gain Capture Volume", MICA_CTL, 
		       MICB_CTL, 0, 31, micpga_tlv),
	      
/* ADC */	      
SOC_DOUBLE_R_S8_C_TLV("ADC Capture Volume", ADCA_VOL, 
		       ADCB_VOL, 0x80, 0xA0, ipd_tlv),	    
SOC_DOUBLE_R_S8_C_TLV("ADC Mixer Capture Volume", 
		       ADCA_MIXER_VOL, ADCB_MIXER_VOL, 0x7f, 0x19, ipd_tlv),
		       
SOC_DOUBLE("ADC Switch", ADC_MISC_CTL, 0, 1, 1, 1),

SOC_DOUBLE_R("ADC Mixer Switch", 
	      ADCA_MIXER_VOL, ADCB_MIXER_VOL, 7, 1, 1),		
	      
SOC_DOUBLE_R_S8_C_TLV("PGA Volume", PGAA_CTL, PGAB_CTL, 0x30, 0x18, micpga_tlv),		       


SOC_DOUBLE_R("PCM Mixer Playback Switch",
	      PCMA_MIXER_VOL, PCMB_MIXER_VOL, 7, 1, 1),


		       
SOC_DOUBLE_R_S8_C_TLV("PCM Mixer Playback Volume", 
		      PCMA_MIXER_VOL, PCMB_MIXER_VOL, 0x7f, 0x19, hl_tlv),
		       
SOC_SINGLE_S8_C_TLV("Beep Volume", BEEP_VOL, 0, 0x1f, 0x07, hl_tlv),
		     
SOC_SINGLE_S8_C_TLV("Treble Gain Playback Volume",
		     TONE_CTL, 4, 15, 1, hl_tlv),
SOC_SINGLE_S8_C_TLV("Bass Gain Playback Volume", 
		     TONE_CTL, 0, 15, 1, hl_tlv),


		     

		     
/* Limiter */		     
SOC_SINGLE_TLV("Limiter Max Threshold Volume", 
	    LIMITER_CTL1, 5, 7, 0, limiter_tlv),
SOC_SINGLE_TLV("Limiter Cushion Threshold Volume", 
	    LIMITER_CTL1, 2, 7, 0, limiter_tlv),
SOC_SINGLE_TLV("Limiter Release Rate Volume",
	    LIMITER_CTL2, 0, 63, 0, limiter_tlv),
SOC_SINGLE_TLV("Limiter Attack Rate Volume", 
	    LIMITER_AT_RATE, 0, 63, 0, limiter_tlv),    
	    
SOC_SINGLE("Limiter SR Switch", 
	    LIMITER_CTL1, 1, 1, 0),
SOC_SINGLE("Limiter ZC Switch", 
	    LIMITER_CTL1, 0, 1, 0),
SOC_SINGLE("Limiter Switch", 
	    LIMITER_CTL2, 7, 1, 0),
	    
/* ALC */	    
SOC_SINGLE_TLV("ALC Attack Rate Volume", 
		ALC_CTL, 0, 63, 0, limiter_tlv),
SOC_SINGLE_TLV("ALC Release Rate Volume", 
		ALC_RATE, 0, 63, 0, limiter_tlv),
SOC_SINGLE_TLV("ALC Max Threshold Volume", 
		ALC_THRESHOLD, 5, 7, 0, limiter_tlv),
SOC_SINGLE_TLV("ALC Min Threshold Volume", 
		ALC_THRESHOLD, 2, 7, 0, limiter_tlv),
		
SOC_DOUBLE_R("ALC SR Capture Switch", PGAA_CTL,
	      PGAB_CTL, 7, 1, 1), /*20*/
SOC_DOUBLE_R("ALC ZC Capture Switch", PGAA_CTL, 
	      PGAB_CTL, 6, 1, 1),
SOC_DOUBLE("ALC Capture Switch",ALC_CTL,
	      6, 7, 1, 0),
		
/* Noise gate */
SOC_ENUM("NG Type Switch", ng_type_enum),
SOC_SINGLE("NG Enable Switch", NOISE_GATE_CTL, 6, 1, 0),
SOC_SINGLE("NG Boost Switch", NOISE_GATE_CTL, 5, 1, 1),
SOC_SINGLE("NG Threshold", NOISE_GATE_CTL, 2, 7, 0),
SOC_ENUM("NG Delay", ng_delay_enum),
	    

SOC_DOUBLE("HPF Switch", ANALOG_HPF_CTL, 5, 7, 1, 0),

SOC_DOUBLE("Analog SR Switch", ANALOG_HPF_CTL, 1, 3, 1, 1),
SOC_DOUBLE("Analog ZC Switch", ANALOG_HPF_CTL, 0, 2, 1, 1),

SOC_SINGLE("Batt Compensation Switch", BATT_COMPEN, 7, 1, 0),
SOC_SINGLE("Batt VP Monitor Switch", BATT_COMPEN, 6, 1, 0),
SOC_SINGLE("Batt VP ref", BATT_COMPEN, 0, 0x0f, 0),
SOC_SINGLE("Playback Charge Pump Freq", CHARGE_PUMP, 4, 15, 0),

};

static const struct snd_soc_dapm_widget cs42l52_dapm_widgets[] = {
	
	SND_SOC_DAPM_INPUT("INPUT1A"),
	SND_SOC_DAPM_INPUT("INPUT2A"),
	SND_SOC_DAPM_INPUT("INPUT3A"),
	SND_SOC_DAPM_INPUT("INPUT4A"),
	SND_SOC_DAPM_INPUT("INPUT1B"),
	SND_SOC_DAPM_INPUT("INPUT2B"),
	SND_SOC_DAPM_INPUT("INPUT3B"),
	SND_SOC_DAPM_INPUT("INPUT4B"),
	SND_SOC_DAPM_INPUT("MICA"),
	SND_SOC_DAPM_INPUT("MICB"),

	/* Input path */
	SND_SOC_DAPM_ADC("ADC Left", "Capture", PWRCTL1, 1, 1),
	SND_SOC_DAPM_ADC("ADC Right", "Capture", PWRCTL1, 2, 1),
	/* PGA Power */
	SND_SOC_DAPM_PGA("PGA Left", PWRCTL1, 3, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA Right", PWRCTL1, 4, 1, NULL, 0),
	

	/* MIC PGA Power */
	SND_SOC_DAPM_PGA("PGA MICA", PWRCTL2, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA MICB", PWRCTL2, 2, 1, NULL, 0),
	/* MIC bias */
	SND_SOC_DAPM_MICBIAS("Mic-Bias", PWRCTL2, 0, 1),
	
	SND_SOC_DAPM_DAC("DAC Left", "Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Right", "Playback", SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("HP Amp Left", PWRCTL3, 4, 1, NULL, 0),
	SND_SOC_DAPM_PGA("HP Amp Right", PWRCTL3, 6, 1, NULL, 0),

	SND_SOC_DAPM_PGA("SPK Pwr Left", PWRCTL3, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("SPK Pwr Right", PWRCTL3, 2, 1, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HPA"),
	SND_SOC_DAPM_OUTPUT("HPB"),
	SND_SOC_DAPM_OUTPUT("SPKA"),
	SND_SOC_DAPM_OUTPUT("SPKB"),

};

static const struct snd_soc_dapm_route cs42l52_audio_map[] = {

	/* adc select path */
	{"ADC Left", "AIN1", "INPUT1A"},
	{"ADC Right", "AIN1", "INPUT1B"},
	{"ADC Left", "AIN2", "INPUT2A"},
        {"ADC Right", "AIN2", "INPUT2B"},
	{"ADC Left", "AIN3", "INPUT3A"},
        {"ADC Right", "AIN3", "INPUT3B"},
	{"ADC Left", "AIN4", "INPUT4A"},
        {"ADC Right", "AIN4", "INPUT4B"},

	/* left capture part */
        {"AIN1A Switch", NULL, "INPUT1A"},
        {"AIN2A Switch", NULL, "INPUT2A"},
        {"AIN3A Switch", NULL, "INPUT3A"},
        {"AIN4A Switch", NULL, "INPUT4A"},
        {"MICA Switch",  NULL, "MICA"},
        {"PGA MICA", NULL, "MICA Switch"},

        {"PGA Left", NULL, "AIN1A Switch"},
        {"PGA Left", NULL, "AIN2A Switch"},
        {"PGA Left", NULL, "AIN3A Switch"},
        {"PGA Left", NULL, "AIN4A Switch"},
        {"PGA Left", NULL, "PGA MICA"},

	/* right capture part */
        {"AIN1B Switch", NULL, "INPUT1B"},
        {"AIN2B Switch", NULL, "INPUT2B"},
        {"AIN3B Switch", NULL, "INPUT3B"},
        {"AIN4B Switch", NULL, "INPUT4B"},
        {"MICB Switch",  NULL, "MICB"},
        {"PGA MICB", NULL, "MICB Switch"},

        {"PGA Right", NULL, "AIN1B Switch"},
        {"PGA Right", NULL, "AIN2B Switch"},
        {"PGA Right", NULL, "AIN3B Switch"},
        {"PGA Right", NULL, "AIN4B Switch"},
        {"PGA Right", NULL, "PGA MICB"},

	{"ADC Mux Left", "PGA", "PGA Left"},
        {"ADC Mux Right", "PGA", "PGA Right"},
	{"ADC Left", NULL, "ADC Mux Left"},
	{"ADC Right", NULL, "ADC Mux Right"},

/* Output map */
	/* Headphone */
	{"HP Amp Left",  NULL, "Passthrough Left"},
	{"HP Amp Right", NULL, "Passthrough Right"},
	{"HPA", NULL, "HP Amp Left"},
	{"HPB", NULL, "HP Amp Right"},

	/* Speakers */
	
	{"SPK Pwr Left",  NULL, "DAC Left"},
	{"SPK Pwr Right", NULL, "DAC Right"},
	{"SPKA", NULL, "SPK Pwr Left"},
	{"SPKB", NULL, "SPK Pwr Right"},

};

static int cs42l52_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
  
	snd_soc_dapm_new_controls(dapm, cs42l52_dapm_widgets,
				  ARRAY_SIZE(cs42l52_dapm_widgets));

	snd_soc_dapm_add_routes(dapm,cs42l52_audio_map,
				  ARRAY_SIZE(cs42l52_audio_map));	

        return 0;
}

#define SOC_CS42L52_RATES ( SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 | \
                            SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
                            SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
                            SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
                            SNDRV_PCM_RATE_96000 ) /*refer to cs42l52 datasheet page35*/

#define SOC_CS42L52_FORMATS ( SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
                              SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
                              SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
                              SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE )


struct cs42l52_clk_para {
	u32 mclk;
	u32 rate;
	u8 speed;
	u8 group;
	u8 videoclk;
	u8 ratio;
	u8 mclkdiv2;
};

static const struct cs42l52_clk_para clk_map_table[] = {
	/*8k*/
	{12288000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 0}, /*4*/

	/*11.025k*/
	{11289600, 11025, CLK_CTL_S_QS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 11025, CLK_CTL_S_QS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	
	/*16k*/
	{12288000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},/*9*/
	{24000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 1},

	/*22.05k*/
	{11289600, 22050, CLK_CTL_S_HS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 22050, CLK_CTL_S_HS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	
	/* 32k */
	{12288000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*14*/
	{18432000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 0},

	/* 44.1k */
	{11289600, 44100, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*19*/
	{16934400, 44100, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},

	/* 48k */
	{12288000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},/*24*/
	{27000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 1},

	/* 88.2k */
	{11289600, 88200, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 88200, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},

	/* 96k */
	{12288000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*29*/
	{12000000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
};

static int cs42l52_get_clk(int mclk, int rate)
{

	int i , ret = 0;
	u_int mclk1, mclk2 = 0;

	for(i = 0; i < ARRAY_SIZE(clk_map_table); i++)
	{
		if(clk_map_table[i].rate == rate)
		{
			mclk1 = clk_map_table[i].mclk;
			{
				if(abs(mclk - mclk1) < abs(mclk - mclk2))
				{
					mclk2 = mclk1;
					ret = i;
				}
			}
		}
	}

	return ret < ARRAY_SIZE(clk_map_table) ? ret : -EINVAL;
}


static int cs42l52_set_sysclk(struct snd_soc_dai *codec_dai,
			int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l52_private *info = snd_soc_codec_get_drvdata(codec);

	if((freq >= CS42L52_MIN_CLK) && (freq <= CS42L52_MAX_CLK))
	{
		info->sysclk = freq;
		dev_info(codec->dev,"sysclk %d\n", info->sysclk);
	}
	else{
		dev_err(codec->dev,"invalid paramter\n");
		return -EINVAL;
	}
	return 0;
}

static int cs42l52_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l52_private *priv = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	u8 iface = 0;
	
	dev_info(codec->dev,"Enter soc_cs42l52_set_fmt\n"); 
	/* set master/slave audio interface */
        switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

	case SND_SOC_DAIFMT_CBM_CFM:
		dev_info(codec->dev,"codec dai fmt master\n");
		iface = IFACE_CTL1_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		dev_info(codec->dev,"codec dai fmt slave\n");
		break;
	default:
		dev_err(codec->dev,"invaild formate\n");
		return -EINVAL;
	}

	 /* interface format */
        switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		iface |= (IFACE_CTL1_ADC_FMT_I2S | IFACE_CTL1_DAC_FMT_I2S);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= IFACE_CTL1_DAC_FMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= (IFACE_CTL1_ADC_FMT_LEFT_J | IFACE_CTL1_DAC_FMT_LEFT_J);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= IFACE_CTL1_DSP_MODE_EN;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		dev_err(codec->dev,"unsupported format\n");
		return -EINVAL;
	default:
		dev_err(codec->dev,"invaild format\n");
		return -EINVAL;
	}

	/* clock inversion */
        switch (fmt & SND_SOC_DAIFMT_INV_MASK) { /*tonyliu*/

	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		dev_err(codec->dev,"unsupported format\n");
		ret = -EINVAL;
	}

	priv->config.format = iface;

	return 0;

}

static int cs42l52_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *soc_codec = dai->codec;
	u8 mute_val = snd_soc_read(soc_codec, PB_CTL1) & PB_CTL1_MUTE_MASK;

	if(mute)
	{
		snd_soc_write(soc_codec, PB_CTL1, mute_val | PB_CTL1_MSTB_MUTE | PB_CTL1_MSTA_MUTE);
	}
	else{
		snd_soc_write(soc_codec, PB_CTL1, mute_val );
	}

	return 0;
}

#define CONFIG_MANUAL_CLK
static int cs42l52_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct cs42l52_private *priv = snd_soc_codec_get_drvdata(codec);
	u32 clk = 0;
	int index = cs42l52_get_clk(priv->sysclk, params_rate(params));

	if(index >= 0)
	{
		priv->sysclk = clk_map_table[index].mclk;
		clk |= (clk_map_table[index].speed << CLK_CTL_SPEED_SHIFT) | 
		      (clk_map_table[index].group << CLK_CTL_32K_SR_SHIFT) | 
		      (clk_map_table[index].videoclk << CLK_CTL_27M_MCLK_SHIFT) | 
		      (clk_map_table[index].ratio << CLK_CTL_RATIO_SHIFT) | 
		      clk_map_table[index].mclkdiv2;
#ifdef CONFIG_MANUAL_CLK
		snd_soc_write(codec, CLK_CTL, clk);
#else
		snd_soc_write(codec, CLK_CTL, 0xa0);
#endif
		snd_soc_write(codec, IFACE_CTL1, priv->config.format );

	}
	else{
		dev_err(codec->dev,"can't find out right mclk\n");
		return -EINVAL;
	}

	return 0;
}

static int cs42l52_set_bias_level(struct snd_soc_codec *codec,
					enum snd_soc_bias_level level)
{

	u8 pwrctl1 = snd_soc_read(codec, PWRCTL1) & 0x9f;
	u8 pwrctl2 = snd_soc_read(codec, PWRCTL2) & 0x07;

	switch (level) {
        case SND_SOC_BIAS_ON: /* full On */
		break;
        case SND_SOC_BIAS_PREPARE: /* partial On */
		pwrctl1 &= ~(PWRCTL1_PDN_CHRG | PWRCTL1_PDN_CODEC);
                snd_soc_write(codec, PWRCTL1, pwrctl1);
                break;
        case SND_SOC_BIAS_STANDBY: /* Off, with power */
		pwrctl1 &= ~(PWRCTL1_PDN_CHRG | PWRCTL1_PDN_CODEC);
                snd_soc_write(codec, PWRCTL1, pwrctl1);
                break;
        case SND_SOC_BIAS_OFF: /* Off, without power */
                snd_soc_write(codec, PWRCTL1, pwrctl1 | 0x9f);
		snd_soc_write(codec, PWRCTL2, pwrctl2 | 0x07);
                break;
        }
        codec->dapm.bias_level = level;
        return 0;
}

#define CS42L52_RATES ( SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 | \
                            SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
                            SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
                            SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
                            SNDRV_PCM_RATE_96000 ) /*refer to cs42l52 datasheet page35*/

#define CS42L52_FORMATS ( SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
                              SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
                              SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
                              SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE )

static struct snd_soc_dai_ops cs42l52_ops = {
	.hw_params	= cs42l52_pcm_hw_params,
	.digital_mute	= cs42l52_digital_mute,
	.set_fmt	= cs42l52_set_fmt,
	.set_sysclk	= cs42l52_set_sysclk,
};

struct snd_soc_dai_driver cs42l52_dai = {
                .name = "cs42l52-hifi",
                .playback = {
                        .stream_name = "Playback",
                        .channels_min = 1,
                        .channels_max = 2,
                        .rates = CS42L52_RATES,
                        .formats = CS42L52_FORMATS,
                },
                .capture = {
                        .stream_name = "Capture",
                        .channels_min = 1,
                        .channels_max = 2,
                        .rates = CS42L52_RATES,
                        .formats = CS42L52_FORMATS,
                },
                .ops = &cs42l52_ops,
};


static int cs42l52_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int cs42l52_resume(struct snd_soc_codec *codec)
{
	
	int i, reg;
	u8 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = PWRCTL1; i < ARRAY_SIZE(cs42l52_reg); i++) {
		snd_soc_write(codec, i, cache[i]);
	}

	cs42l52_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	
	return 0;

}

/* page 37 from cs42l52 datasheet */
static void cs42l52_required_setup(struct snd_soc_codec *codec)
{
	u8 data;
	snd_soc_write(codec, 0x00, 0x99);
	snd_soc_write(codec, 0x3e, 0xba);
	snd_soc_write(codec, 0x47, 0x80);
	data = snd_soc_read(codec, 0x32);
	snd_soc_write(codec, 0x32, data | 0x80);
	snd_soc_write(codec, 0x32, data & 0x7f);
	snd_soc_write(codec, 0x00, 0x00);
}


static int cs42l52_probe(struct snd_soc_codec *codec)
{
	
	struct cs42l52_private *info = snd_soc_codec_get_drvdata(codec);
	int i, ret = 0;
	
	info->sysclk = CS42L52_DEFAULT_CLK;
	info->config.format = CS42L52_DEFAULT_FORMAT;

	cs42l52_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	
	cs42l52_required_setup(codec);
	
	info->flags |= CS42L52_CHIP_SWICTH;
	/*initialize codec*/
/*	
	for(i = 0; i < codec->num_dai; i++)
	{
		info->flags |= i;
		
		ret = cs42l52_get_revison(codec);
		if(ret < 0)
		{
			dev_err(codec->dev,"get chip id failed\n");
			return 0;
		}
		
		dev_info(codec->dev,"Cirrus CS42L52 codec , revision %d\n", ret & CHIP_REV_MASK);
		

	}
*/
	info->flags |= 0;

	info->flags &= ~(CS42L52_CHIP_SWICTH);
	info->flags |= CS42L52_ALL_IN_ONE;

	/*init done*/
	snd_soc_add_controls(codec, cs42l52_snd_controls,
			     ARRAY_SIZE(cs42l52_snd_controls));
	
	cs42l52_add_widgets(codec);

	return 0;
}

/* power down chip */
static int cs42l52_remove(struct snd_soc_codec *codec)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}


struct i2c_client *cs_control;

static inline unsigned int cs42l52_read(struct snd_soc_codec *codec, unsigned int reg){
        return i2c_smbus_read_byte_data(cs_control, reg);
}
static inline int cs42l52_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int val){
        i2c_smbus_write_byte_data(cs_control, reg, val);
        return 0;
}

struct snd_soc_codec_driver soc_codec_dev_cs42l52 = {
	.probe = cs42l52_probe,
	.remove = cs42l52_remove,
	.suspend = cs42l52_suspend,
	.resume = cs42l52_resume,
	.set_bias_level = cs42l52_set_bias_level,
	.reg_cache_size = CS42L52_CACHEREGNUM,
	.reg_cache_default = cs42l52_reg,
	.read = cs42l52_read,
	.write = cs42l52_write,
};

static int cs42l52_i2c_probe(struct i2c_client *i2c_client,
			     const struct i2c_device_id *id)
{

	struct cs42l52_private *cs42l52;
	int ret;

/* DEBUG
	ret = i2c_smbus_read_byte_data(i2c_client, CS42L52_CHIP);
	printk(KERN_ERR "We have %x from CS42L52_CHIP \n", ret);
*/
	cs42l52 = kzalloc(sizeof(struct cs42l52_private), GFP_KERNEL);
	if (!cs42l52) {
		dev_err(&i2c_client->dev, "could not allocate codec\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c_client, cs42l52);
	cs42l52->control_data = i2c_client;
 	cs42l52->control_type = SND_SOC_I2C;
	
	cs_control = i2c_client;

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs42l52, &cs42l52_dai, 1);
	if (ret < 0){
		kfree(cs42l52);
	}
	return ret;
}

static int cs42l52_i2c_remove(struct i2c_client *client)
{
	struct cs42l52_private *cs42l52 = i2c_get_clientdata(client);
 	snd_soc_unregister_codec(&client->dev);
	kfree(cs42l52);

        return 0;
}

static const struct i2c_device_id cs42l52_id[] = {
	{ "cs42l52", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs42l52_id);

static struct i2c_driver cs42l52_i2c_driver = {
	.driver = {
		.name = "cs42l52-codec",
		.owner = THIS_MODULE,
	},
	.probe =    cs42l52_i2c_probe,
	.remove =   __devexit_p(cs42l52_i2c_remove),
	.id_table = cs42l52_id,

};

static int __init cs42l52_modinit(void)
{
	int ret;
	ret = i2c_add_driver(&cs42l52_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "%s: can't add i2c driver\n", __func__);
		return ret;
	}
	return 0;
}

module_init(cs42l52_modinit);

static void __exit cs42l52_exit(void)
{
	i2c_del_driver(&cs42l52_i2c_driver);
}

module_exit(cs42l52_exit);

MODULE_DESCRIPTION("ASoC CS42L52 driver");
MODULE_AUTHOR("Georgi Vlaev, Nucleus Systems Ltd, <office@nucleusys.com>");
MODULE_LICENSE("GPL");
