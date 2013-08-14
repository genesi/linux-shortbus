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

#define SOC_SINGLE_EXT_SX_TLV(xname, xreg, xshift, \
	xmin, xmax, xhandler_info, xhandler_get, xhandler_put, tlv_array) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
		SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.tlv.p = (tlv_array), \
	.info = xhandler_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
		{.reg = xreg, .shift = xshift, \
		.min = xmin, .max = xmax} }

struct sp_config {
	u8 spc, format, spfs;
	u32 srate;
};

#define CS42L52_CACHEREGNUM 56

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

/* info/get/put callbacks because the kernel doesn't seem to 
   know how to handle variable ranges for mixers on 
   single registers (but two registers it can do...) */

static int cs42l52_snd_soc_info_volsw_sx(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_info *uinfo);

static int cs42l52_snd_soc_get_volsw_sx(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol);

static int cs42l52_snd_soc_put_volsw_sx(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol);

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

static DECLARE_TLV_DB_SCALE(hl_tlv, -10200, 50, 0);

static DECLARE_TLV_DB_SCALE(hpd_tlv, -9600, 50, 0);

static DECLARE_TLV_DB_SCALE(ipd_tlv, -9600, 100, 0);

static DECLARE_TLV_DB_SCALE(mic_tlv, 1600, 100, 0);

static DECLARE_TLV_DB_SCALE(pga_tlv, -600, 50, 0);

static DECLARE_TLV_DB_SCALE(pass_tlv, -6000, 50, 0);

static DECLARE_TLV_DB_SCALE(beep_tlv, -5600, 200, 0);

static DECLARE_TLV_DB_SCALE(mix_tlv, -5150, 50, 0);

static DECLARE_TLV_DB_SCALE(tbg_tlv, -1050, 150, 0);

static const unsigned int limiter_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(-3000, 600, 0),
	3, 7, TLV_DB_SCALE_ITEM(-1200, 300, 0),
};

static const char * cs42l52_pdn_adc_mic[] = {
	"Internal + External Microphone", "External Microphone", "Internal Microphone", "None"
};

static const struct soc_enum pdn_adc_mic = 
	SOC_ENUM_SINGLE(CS42L52_PWRCTL1, 1, 
		ARRAY_SIZE(cs42l52_pdn_adc_mic), cs42l52_pdn_adc_mic);

static const char * cs42l52_adca_text[] = {
	"Input1A", "Input2A", "Input3A", "Input4A", "PGA Input Left"};

static const char * cs42l52_adcb_text[] = {
	"Input1B", "Input2B", "Input3B", "Input4B", "PGA Input Right"};

static const struct soc_enum adca_enum =
	SOC_ENUM_SINGLE(CS42L52_ADC_PGA_A, 5,
		ARRAY_SIZE(cs42l52_adca_text), cs42l52_adca_text);

static const struct soc_enum adcb_enum =
	SOC_ENUM_SINGLE(CS42L52_ADC_PGA_B, 5,
		ARRAY_SIZE(cs42l52_adcb_text), cs42l52_adcb_text);

static const struct snd_kcontrol_new adca_mux =
	SOC_DAPM_ENUM("Left ADC Input Capture Mux", adca_enum);

static const struct snd_kcontrol_new adcb_mux =
	SOC_DAPM_ENUM("Right ADC Input Capture Mux", adcb_enum);

static const char * mic_bias_level_text[] = {
	"0.5 +VA", "0.6 +VA", "0.7 +VA",
	"0.8 +VA", "0.83 +VA", "0.91 +VA"
};

static const struct soc_enum mic_bias_level_enum =
	SOC_ENUM_SINGLE(CS42L52_IFACE_CTL2, 0,
			ARRAY_SIZE(mic_bias_level_text), mic_bias_level_text);

static const char * cs42l52_mic_text[] = { "Single", "Differential" };

static const struct soc_enum mica_enum =
	SOC_ENUM_SINGLE(CS42L52_MICA_CTL, 5,
			ARRAY_SIZE(cs42l52_mic_text), cs42l52_mic_text);

static const struct soc_enum micb_enum =
	SOC_ENUM_SINGLE(CS42L52_MICB_CTL, 5,
			ARRAY_SIZE(cs42l52_mic_text), cs42l52_mic_text);

static const char * cs42l52_micsel_text[] = { "MIC1", "MIC2" };

static const struct soc_enum mica_sel_enum =
	SOC_ENUM_SINGLE(CS42L52_MICA_CTL, 6,
			ARRAY_SIZE(cs42l52_micsel_text), cs42l52_micsel_text);

static const struct soc_enum micb_sel_enum =
	SOC_ENUM_SINGLE(CS42L52_MICB_CTL, 6,
			ARRAY_SIZE(cs42l52_micsel_text), cs42l52_micsel_text);

/*static const struct snd_kcontrol_new mica_mux =
	SOC_DAPM_ENUM("Left Mic Input Capture Mux", mica_enum);

static const struct snd_kcontrol_new micb_mux =
	SOC_DAPM_ENUM("Right Mic Input Capture Mux", micb_enum);

static const struct snd_kcontrol_new mica_sel_mux =
	SOC_DAPM_ENUM("Left Mic Input Sel Capture Mux", mica_sel_enum);

static const struct snd_kcontrol_new micb_sel_mux =
	SOC_DAPM_ENUM("Right Mic Input Sel Capture Mux", micb_sel_enum);*/

static const char * digital_output_mux_text[] = {"ADC", "DSP"};

static const struct soc_enum digital_output_mux_enum =
	SOC_ENUM_SINGLE(CS42L52_ADC_MISC_CTL, 6,
			ARRAY_SIZE(digital_output_mux_text),
			digital_output_mux_text);

static const struct snd_kcontrol_new digital_output_mux =
	SOC_DAPM_ENUM("Digital Output Mux", digital_output_mux_enum);

static const char * hp_gain_num_text[] = {
	"0.3959", "0.4571", "0.5111", "0.6047",
	"0.7099", "0.8399", "1.000", "1.1430"
};

static const struct soc_enum hp_gain_enum =
	SOC_ENUM_SINGLE(CS42L52_PB_CTL1, 5,
		ARRAY_SIZE(hp_gain_num_text), hp_gain_num_text);

static const char * beep_pitch_text[] = {
	"C4", "C5", "D5", "E5", "F5", "G5", "A5", "B5",
	"C6", "D6", "E6", "F6", "G6", "A6", "B6", "C7"
};

static const struct soc_enum beep_pitch_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_FREQ, 4,
			ARRAY_SIZE(beep_pitch_text), beep_pitch_text);

static const char * beep_ontime_text[] = {
	"86 ms", "430 ms", "780 ms", "1.20 s", "1.50 s",
	"1.80 s", "2.20 s", "2.50 s", "2.80 s", "3.20 s",
	"3.50 s", "3.80 s", "4.20 s", "4.50 s", "4.80 s", "5.20 s"
};

static const struct soc_enum beep_ontime_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_FREQ, 0,
			ARRAY_SIZE(beep_ontime_text), beep_ontime_text);

static const char * beep_offtime_text[] = {
	"1.23 s", "2.58 s", "3.90 s", "5.20 s",
	"6.60 s", "8.05 s", "9.35 s", "10.80 s"
};

static const struct soc_enum beep_offtime_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_VOL, 5,
			ARRAY_SIZE(beep_offtime_text), beep_offtime_text);

static const char * beep_config_text[] = {
	"Off", "Single", "Multiple", "Continuous"
};

static const struct soc_enum beep_config_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_TONE_CTL, 6,
			ARRAY_SIZE(beep_config_text), beep_config_text);

static const char * beep_bass_text[] = {
	"50 Hz", "100 Hz", "200 Hz", "250 Hz"
};

static const struct soc_enum beep_bass_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_TONE_CTL, 1,
			ARRAY_SIZE(beep_bass_text), beep_bass_text);

static const char * beep_treble_text[] = {
	"5 kHz", "7 kHz", "10 kHz", " 15 kHz"
};

static const struct soc_enum beep_treble_enum =
	SOC_ENUM_SINGLE(CS42L52_BEEP_TONE_CTL, 3,
			ARRAY_SIZE(beep_treble_text), beep_treble_text);

static const char * ng_threshold_text[] = {
	"-34dB", "-37dB", "-40dB", "-43dB",
	"-46dB", "-52dB", "-58dB", "-64dB"
};

static const struct soc_enum ng_threshold_enum =
	SOC_ENUM_SINGLE(CS42L52_NOISE_GATE_CTL, 2,
		ARRAY_SIZE(ng_threshold_text), ng_threshold_text);

static const char * cs42l52_ng_delay_text[] = {
	"50ms", "100ms", "150ms", "200ms"};

static const struct soc_enum ng_delay_enum =
	SOC_ENUM_SINGLE(CS42L52_NOISE_GATE_CTL, 0,
		ARRAY_SIZE(cs42l52_ng_delay_text), cs42l52_ng_delay_text);

static const char * cs42l52_ng_type_text[] = {
	"Apply Specific", "Apply All"
};

static const struct soc_enum ng_type_enum =
	SOC_ENUM_SINGLE(CS42L52_NOISE_GATE_CTL, 7, 
		ARRAY_SIZE(cs42l52_ng_type_text), cs42l52_ng_type_text);

static const char * cs42l52_limiter_type_text[] = {
	"Apply Specific", "Apply All"
};

static const struct soc_enum limiter_type_enum =
	SOC_ENUM_SINGLE(CS42L52_LIMITER_CTL2, 6, 
	ARRAY_SIZE(cs42l52_limiter_type_text), cs42l52_limiter_type_text);

static const char * cs42l52_vpref_text[] = {
	"1.5V (1.8)", "2.0V (1.8)", "2.5V (1.8)", "3.0V (1.8)", 
		"3.5V (1.8)", "4.0V (1.8)", "4.5V (1.8)", "5.0V (1.8)",
	"1.5V (2.5)", "2.0V (2.5)", "2.5V (2.5)", "3.0V (2.5)", 
		"3.5V (2.5)", "4.0V (2.5)", "4.5V (2.5)",  "5.0V (2.5)" 
};

static const struct soc_enum vpref_enum = 
	SOC_ENUM_SINGLE(CS42L52_BATT_COMPEN, 0,
	ARRAY_SIZE(cs42l52_vpref_text), cs42l52_vpref_text);

static const char * left_swap_text[] = {
	"Left", "LR 2", "Right"};

static const char * right_swap_text[] = {
	"Right", "LR 2", "Left"};

static const unsigned int swap_values[] = { 0, 1, 3 };

static const struct soc_enum adca_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER, 2, 1,
			      ARRAY_SIZE(left_swap_text),
			      left_swap_text,
			      swap_values);

static const struct snd_kcontrol_new adca_mixer =
	SOC_DAPM_ENUM("Route", adca_swap_enum);

static const struct soc_enum pcma_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER, 6, 1,
			      ARRAY_SIZE(left_swap_text),
			      left_swap_text,
			      swap_values);

static const struct snd_kcontrol_new pcma_mixer =
	SOC_DAPM_ENUM("Route", pcma_swap_enum);

static const struct soc_enum adcb_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER, 0, 1,
			      ARRAY_SIZE(right_swap_text),
			      right_swap_text,
			      swap_values);

static const struct snd_kcontrol_new adcb_mixer =
	SOC_DAPM_ENUM("Route", adcb_swap_enum);

static const struct soc_enum pcmb_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER, 4, 1,
			      ARRAY_SIZE(right_swap_text),
			      right_swap_text,
			      swap_values);

static const struct snd_kcontrol_new pcmb_mixer =
	SOC_DAPM_ENUM("Route", pcmb_swap_enum);


static const struct snd_kcontrol_new passthrul_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_MISC_CTL, 6, 1, 0);

static const struct snd_kcontrol_new passthrur_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_MISC_CTL, 7, 1, 0);

static const struct snd_kcontrol_new spkl_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_PWRCTL3, 0, 1, 1);

static const struct snd_kcontrol_new spkr_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_PWRCTL3, 2, 1, 1);

static const struct snd_kcontrol_new hpl_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_PWRCTL3, 4, 1, 1);

static const struct snd_kcontrol_new hpr_ctl =
	SOC_DAPM_SINGLE("Switch", CS42L52_PWRCTL3, 6, 1, 1);

static const struct snd_kcontrol_new cs42l52_snd_controls[] = {

	SOC_DOUBLE_R_SX_TLV("Master Volume", CS42L52_MASTERA_VOL,
		CS42L52_MASTERB_VOL, 8, 0x34, 0x118, hl_tlv),

/*Data sheet for headphone contradicts itself, 
		no guarantee the dB values are correct*/
	SOC_DOUBLE_R_SX_TLV("Headphone Volume", CS42L52_HPA_VOL,
		CS42L52_HPB_VOL, 8, 0x40, 0x100, hpd_tlv), 

	SOC_ENUM("Headphone Analog Gain", hp_gain_enum),

	SOC_DOUBLE("Headphone Mute", CS42L52_PB_CTL2, 6, 7, 1, 0),

/*Data sheet contradicts extant TLV Scale, 
		no guarantee the dB values are correct*/
	SOC_DOUBLE_R_SX_TLV("Speaker Volume", CS42L52_SPKA_VOL,
			      CS42L52_SPKB_VOL, 8, 0x34, 0x100, hl_tlv), 

	SOC_DOUBLE("Speaker Mute", CS42L52_PB_CTL2, 4, 5, 1, 0),

	SOC_DOUBLE_R_SX_TLV("Bypass Volume", CS42L52_PASSTHRUA_VOL, /* ? */
			      CS42L52_PASSTHRUB_VOL, 8, 0x88, 0x118, pass_tlv),

	SOC_DOUBLE("Bypass Mute", CS42L52_MISC_CTL, 4, 5, 1, 0), /* ? TODO */

	SOC_ENUM("Mic Config", pdn_adc_mic),

/* A white lie - this doesn't *exactly* match the datasheet*/
	SOC_DOUBLE_R_TLV("MIC Gain Volume", CS42L52_MICA_CTL,
			      CS42L52_MICB_CTL, 0, 0x10, 0, mic_tlv), 

	SOC_ENUM("MIC Bias Level", mic_bias_level_enum), 
	SOC_ENUM("MICA Top Mux", mica_enum),
	SOC_ENUM("MICA Sel Mux", mica_sel_enum),
	SOC_ENUM("MICB Top Mux", micb_enum),
	SOC_ENUM("MICB Sel Mux", micb_sel_enum),

	SOC_DOUBLE_R_SX_TLV("ADC Volume", CS42L52_ADCA_VOL,
			      CS42L52_ADCB_VOL, 8, 0xA0, 0x118, ipd_tlv),
	SOC_DOUBLE_R_SX_TLV("ADC Mixer Volume",
 /* TODO ipd_tlv has wrong range of values for control! */
			     CS42L52_ADCA_MIXER_VOL, CS42L52_ADCB_MIXER_VOL,
				7, 0x19, 0x98, ipd_tlv),

/* TODO showing as control but not simple mixer */
	SOC_DOUBLE("ADC Switch", CS42L52_ADC_MISC_CTL, 0, 1, 1, 0), 

	SOC_DOUBLE_R("ADC Mixer Switch", CS42L52_ADCA_MIXER_VOL,
		     CS42L52_ADCB_MIXER_VOL, 7, 1, 1),

	SOC_DOUBLE_R_SX_TLV("PGA Volume", CS42L52_PGAA_CTL,
			    CS42L52_PGAB_CTL, 6, 0x28, 0x58, pga_tlv),

	SOC_DOUBLE_R_SX_TLV("PCM Mixer Volume",
			    CS42L52_PCMA_MIXER_VOL, CS42L52_PCMB_MIXER_VOL,
				7, 0x19, 0x98, mix_tlv), 
	SOC_DOUBLE_R("PCM Mixer Switch",
		     CS42L52_PCMA_MIXER_VOL, CS42L52_PCMB_MIXER_VOL, 7, 1, 1),

	/* Beep */
/* TODO Slightly weird behavior when setting from alsamixer - 
	Beep Multiple won't cancel Beep Continuous */
	SOC_ENUM("Beep Config", beep_config_enum), 
	SOC_ENUM("Beep Pitch", beep_pitch_enum),
	SOC_ENUM("Beep on Time", beep_ontime_enum),
	SOC_ENUM("Beep off Time", beep_offtime_enum),

	SOC_SINGLE_EXT_SX_TLV("Beep Volume", CS42L52_BEEP_VOL, 5, 0x07, 0x26,
		cs42l52_snd_soc_info_volsw_sx,
		cs42l52_snd_soc_get_volsw_sx, 
		cs42l52_snd_soc_put_volsw_sx,
		beep_tlv),
	SOC_SINGLE("Beep Mixer Switch", CS42L52_BEEP_TONE_CTL, 5, 1, 1),
	SOC_ENUM("Beep Treble Corner Freq", beep_treble_enum),
	SOC_ENUM("Beep Bass Corner Freq", beep_bass_enum),
	SOC_SINGLE("Tone Control Switch", CS42L52_BEEP_TONE_CTL, 0, 1, 0),

	SOC_SINGLE_TLV("Treble Gain Volume",
			    CS42L52_TONE_CTL, 4, 15, 1, tbg_tlv),
	SOC_SINGLE_TLV("Bass Gain Volume",
			    CS42L52_TONE_CTL, 0, 15, 1, tbg_tlv),

	/* Limiter */
	SOC_SINGLE_TLV("Limiter Max Threshold Volume",
		       CS42L52_LIMITER_CTL1, 5, 7, 1, limiter_tlv),
	SOC_SINGLE_TLV("Limiter Cushion Threshold Volume",
		       CS42L52_LIMITER_CTL1, 2, 7, 1, limiter_tlv),

	SOC_SINGLE("Limiter Release Rate",
		       CS42L52_LIMITER_CTL2, 0, 63, 0),
	SOC_SINGLE("Limiter Attack Rate",
		       CS42L52_LIMITER_AT_RATE, 0, 63, 0),

	SOC_SINGLE("Limiter Soft Ramp Switch", CS42L52_LIMITER_CTL1, 1, 1, 0),
	SOC_SINGLE("Limiter Zero Cross Switch", CS42L52_LIMITER_CTL1, 0, 1, 0),
	SOC_SINGLE("Limiter Switch", CS42L52_LIMITER_CTL2, 7, 1, 0),
	SOC_ENUM("Limiter Type Switch", limiter_type_enum),

	/* ALC */
	/*Same as Limiter Attack/Release rates*/
	SOC_SINGLE("ALC Attack Rate", CS42L52_ALC_CTL, 0, 63, 0),
	SOC_SINGLE("ALC Release Rate", CS42L52_ALC_RATE, 0, 63, 0),
	SOC_SINGLE_TLV("ALC Max Threshold Volume", CS42L52_ALC_THRESHOLD,
		       5, 7, 1, limiter_tlv),
	SOC_SINGLE_TLV("ALC Min Threshold Volume", CS42L52_ALC_THRESHOLD,
		       2, 7, 1, limiter_tlv),

	SOC_DOUBLE_R("ALC Soft Ramp Capture Switch", CS42L52_PGAA_CTL,
		     CS42L52_PGAB_CTL, 7, 1, 1),
	SOC_DOUBLE_R("ALC Zero Cross Capture Switch", CS42L52_PGAA_CTL,
		     CS42L52_PGAB_CTL, 6, 1, 1),
	SOC_DOUBLE("ALC Capture Switch", CS42L52_ALC_CTL, 6, 7, 1, 0),

	/* Noise gate */
	SOC_ENUM("NG Type Switch", ng_type_enum),
	SOC_SINGLE("NG Enable Switch", CS42L52_NOISE_GATE_CTL, 6, 1, 0),
	SOC_SINGLE("NG Boost Switch", CS42L52_NOISE_GATE_CTL, 5, 1, 0),
	SOC_ENUM("NG Threshold", ng_threshold_enum),
	SOC_ENUM("NG Delay", ng_delay_enum),

	SOC_DOUBLE("HPF Switch", CS42L52_ANALOG_HPF_CTL, 5, 7, 1, 0),

	SOC_DOUBLE("Analog SR Switch", CS42L52_ANALOG_HPF_CTL, 1, 3, 1, 0),
	SOC_DOUBLE("Analog ZC Switch", CS42L52_ANALOG_HPF_CTL, 0, 2, 1, 0),
	SOC_SINGLE("Digital SR Switch", CS42L52_MISC_CTL, 1, 1, 0),
	SOC_SINGLE("Digital ZC Switch", CS42L52_MISC_CTL, 0, 1, 0),
	SOC_SINGLE("Deemphasis Switch", CS42L52_MISC_CTL, 2, 1, 0),

	SOC_SINGLE("Batt Compensation Switch", CS42L52_BATT_COMPEN, 7, 1, 0),
	SOC_SINGLE("Batt VP Monitor Switch", CS42L52_BATT_COMPEN, 6, 1, 0),
	SOC_ENUM("Batt VP ref", vpref_enum),

	SOC_SINGLE("PGA AIN1L Switch", CS42L52_ADC_PGA_A, 0, 1, 0),
	SOC_SINGLE("PGA AIN1R Switch", CS42L52_ADC_PGA_B, 0, 1, 0),
	SOC_SINGLE("PGA AIN2L Switch", CS42L52_ADC_PGA_A, 1, 1, 0),
	SOC_SINGLE("PGA AIN2R Switch", CS42L52_ADC_PGA_B, 1, 1, 0),

	SOC_SINGLE("PGA AIN3L Switch", CS42L52_ADC_PGA_A, 2, 1, 0),
	SOC_SINGLE("PGA AIN3R Switch", CS42L52_ADC_PGA_B, 2, 1, 0),

	SOC_SINGLE("PGA AIN4L Switch", CS42L52_ADC_PGA_A, 3, 1, 0),
	SOC_SINGLE("PGA AIN4R Switch", CS42L52_ADC_PGA_B, 3, 1, 0),

	SOC_SINGLE("PGA MICA Switch", CS42L52_ADC_PGA_A, 4, 1, 0),
	SOC_SINGLE("PGA MICB Switch", CS42L52_ADC_PGA_B, 4, 1, 0),
};

static const struct snd_soc_dapm_widget cs42l52_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("AIN1L"),
	SND_SOC_DAPM_INPUT("AIN1R"),
	SND_SOC_DAPM_INPUT("AIN2L"),
	SND_SOC_DAPM_INPUT("AIN2R"),
	SND_SOC_DAPM_INPUT("AIN3L"),
	SND_SOC_DAPM_INPUT("AIN3R"),
	SND_SOC_DAPM_INPUT("AIN4L"),
	SND_SOC_DAPM_INPUT("AIN4R"),
	SND_SOC_DAPM_INPUT("MICA"),
	SND_SOC_DAPM_INPUT("MICB"),
	//SND_SOC_DAPM_SIGGEN("Beep"),

	SND_SOC_DAPM_AIF_OUT("AIFOUTL", NULL,  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIFOUTR", NULL,  0,
			SND_SOC_NOPM, 0, 0),

//	SND_SOC_DAPM_MUX("MICA Top Mux", SND_SOC_NOPM, 0, 0, &mica_mux),
//	SND_SOC_DAPM_MUX("MICB Top Mux", SND_SOC_NOPM, 0, 0, &micb_mux),
//	SND_SOC_DAPM_MUX("MICA Sel Mux", SND_SOC_NOPM, 0, 0, &mica_sel_mux),
//	SND_SOC_DAPM_MUX("MICB Sel Mux", SND_SOC_NOPM, 0, 0, &micb_sel_mux),

	SND_SOC_DAPM_ADC("ADC Left", NULL, CS42L52_PWRCTL1, 1, 1),
	SND_SOC_DAPM_ADC("ADC Right", NULL, CS42L52_PWRCTL1, 2, 1),
	SND_SOC_DAPM_PGA("PGA Left", CS42L52_PWRCTL1, 3, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA Right", CS42L52_PWRCTL1, 4, 1, NULL, 0),

	SND_SOC_DAPM_MUX("ADC Left Mux", SND_SOC_NOPM, 0, 0, &adca_mux),
	SND_SOC_DAPM_MUX("ADC Right Mux", SND_SOC_NOPM, 0, 0, &adcb_mux),

	SND_SOC_DAPM_MUX("ADC Left Swap", SND_SOC_NOPM,
			 0, 0, &adca_mixer),
	SND_SOC_DAPM_MUX("ADC Right Swap", SND_SOC_NOPM,
			 0, 0, &adcb_mixer),

	SND_SOC_DAPM_MUX("Output Mux", SND_SOC_NOPM,
			 0, 0, &digital_output_mux),

	SND_SOC_DAPM_PGA("PGA MICA", CS42L52_PWRCTL2, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA MICB", CS42L52_PWRCTL2, 2, 1, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Mic Bias", CS42L52_PWRCTL2, 0, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Charge Pump", CS42L52_PWRCTL1, 7, 1, NULL, 0),

	SND_SOC_DAPM_AIF_IN("AIFINL", NULL,  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIFINR", NULL,  0,
			SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DAC Left", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Right", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SWITCH("Bypass Left", CS42L52_MISC_CTL,
			    6, 0, &passthrul_ctl),
	SND_SOC_DAPM_SWITCH("Bypass Right", CS42L52_MISC_CTL,
			    7, 0, &passthrur_ctl),

	SND_SOC_DAPM_MUX("PCM Left Swap", SND_SOC_NOPM,
			 0, 0, &pcma_mixer),
	SND_SOC_DAPM_MUX("PCM Right Swap", SND_SOC_NOPM,
			 0, 0, &pcmb_mixer),

	SND_SOC_DAPM_SWITCH("HP Left Amp", SND_SOC_NOPM, 0, 0, &hpl_ctl),
	SND_SOC_DAPM_SWITCH("HP Right Amp", SND_SOC_NOPM, 0, 0, &hpr_ctl),

	SND_SOC_DAPM_SWITCH("SPK Left Amp", SND_SOC_NOPM, 0, 0, &spkl_ctl),
	SND_SOC_DAPM_SWITCH("SPK Right Amp", SND_SOC_NOPM, 0, 0, &spkr_ctl),

	SND_SOC_DAPM_OUTPUT("HPOUTA"),
	SND_SOC_DAPM_OUTPUT("HPOUTB"),
	SND_SOC_DAPM_OUTPUT("SPKOUTA"),
	SND_SOC_DAPM_OUTPUT("SPKOUTB"),
};

static const struct snd_soc_dapm_route cs42l52_audio_map[] = {
	//{"Capture", NULL, "AIFOUTL"},
	//{"Capture", NULL, "AIFOUTL"},

	{"AIFOUTL", NULL, "Output Mux"},
	{"AIFOUTR", NULL, "Output Mux"},

	{"Output Mux", "ADC", "ADC Left"},
	{"Output Mux", "ADC", "ADC Right"},

	{"ADC Left", NULL, "Charge Pump"},
	{"ADC Right", NULL, "Charge Pump"},

	{"Charge Pump", NULL, "ADC Left Mux"},
	{"Charge Pump", NULL, "ADC Right Mux"},

	{"ADC Left Mux", "Input1A", "AIN1L"},
	{"ADC Right Mux", "Input1B", "AIN1R"},
	{"ADC Left Mux", "Input2A", "AIN2L"},
	{"ADC Right Mux", "Input2B", "AIN2R"},
	{"ADC Left Mux", "Input3A", "AIN3L"},
	{"ADC Right Mux", "Input3B", "AIN3R"},
	{"ADC Left Mux", "Input4A", "AIN4L"},
	{"ADC Right Mux", "Input4B", "AIN4R"},
	{"ADC Left Mux", "PGA Input Left", "PGA Left"},
	{"ADC Right Mux", "PGA Input Right" , "PGA Right"},

	{"PGA Left", "Switch", "AIN1L"},
	{"PGA Right", "Switch", "AIN1R"},
	{"PGA Left", "Switch", "AIN2L"},
	{"PGA Right", "Switch", "AIN2R"},
	{"PGA Left", "Switch", "AIN3L"},
	{"PGA Right", "Switch", "AIN3R"},
	{"PGA Left", "Switch", "AIN4L"},
	{"PGA Right", "Switch", "AIN4R"},

	{"PGA Left", "Switch", "PGA MICA"},
	{"PGA MICA", NULL, "MICA"},

	{"PGA Right", "Switch", "PGA MICB"},
	{"PGA MICB", NULL, "MICB"},

	{"HPOUTA", NULL, "HP Left Amp"},
	{"HPOUTB", NULL, "HP Right Amp"},
	{"HP Left Amp", NULL, "Bypass Left"},
	{"HP Right Amp", NULL, "Bypass Right"},
	{"Bypass Left", "Switch", "PGA Left"},
	{"Bypass Right", "Switch", "PGA Right"},
	{"HP Left Amp", "Switch", "DAC Left"},
	{"HP Right Amp", "Switch", "DAC Right"},

	{"SPKOUTA", NULL, "SPK Left Amp"},
	{"SPKOUTB", NULL, "SPK Right Amp"},

/*TODO "Playback" isn't considered a DAPM widget in kernel 2.6.38.
	These paths will cause soc_dapm_add_routes to exit with error */
	//{"SPK Left Amp", NULL, "Beep"},
	//{"SPK Right Amp", NULL, "Beep"},
	{"SPK Left Amp", "Switch", "Playback"},
	{"SPK Right Amp", "Switch", "Playback"},

	//{"DAC Left", NULL, "Beep"},
	//{"DAC Right", NULL, "Beep"},
	{"DAC Left", NULL, "Playback"},
	{"DAC Right", NULL, "Playback"},

	{"Output Mux", "DSP", "Playback"},
	{"Output Mux", "DSP", "Playback"},

	{"AIFINL", NULL, "Playback"},
	{"AIFINR", NULL, "Playback"},
};

/*TODO this function is not enabled. It's helpful for power management
	but doesn't seem to affect functionality too much */
static int cs42l52_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_new_controls(dapm, cs42l52_dapm_widgets,
				  ARRAY_SIZE(cs42l52_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, cs42l52_audio_map,
				  ARRAY_SIZE(cs42l52_audio_map));

	return 0;
}

static int cs42l52_snd_soc_info_volsw_sx(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
                (struct soc_mixer_control *)kcontrol->private_value;
        int max = mc->max;
        int min = mc->min;

        uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
        uinfo->count = 1;
        uinfo->value.integer.min = 0;
        uinfo->value.integer.max = max-min;

        return 0;
}

static int cs42l52_snd_soc_get_volsw_sx(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
        struct soc_mixer_control *mc =
                (struct soc_mixer_control *)kcontrol->private_value;
        struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
        unsigned int mask = (1<<mc->shift)-1;
        int min = mc->min;
        int val = snd_soc_read(codec, mc->reg) & mask;

        ucontrol->value.integer.value[0] = ((val & 0xff)-min) & mask;
        return 0;

}

static int cs42l52_snd_soc_put_volsw_sx(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
        struct soc_mixer_control *mc =
                (struct soc_mixer_control *)kcontrol->private_value;
        struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
        unsigned int mask = (1<<mc->shift)-1;
        int min = mc->min;
        int ret;
        unsigned int val, oval;

        val = ((ucontrol->value.integer.value[0]+min) & 0xff);
        val &= mask;

        oval = snd_soc_read(codec, mc->reg) & mask;

        ret = 0;
        if (oval != val) {
                ret = snd_soc_write(codec, mc->reg, val);
                if (ret < 0)
                        return ret;
        }

        return 0;

}

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
	{12288000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 8000, CLK_QS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/*11.025k*/
	{11289600, 11025, CLK_QS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 11025, CLK_QS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},

	/*16k*/
	{12288000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 16000, CLK_HS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 1},

	/*22.05k*/
	{11289600, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},

	/* 32k */
	{12288000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 32000, CLK_SS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/* 44.1k */
	{11289600, 44100, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 44100, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},

	/* 48k */
	{12288000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_125, 1},

	/* 88.2k */
	{11289600, 88200, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 88200, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},

	/* 96k */
	{12288000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 1},
};

static int cs42l52_get_clk(int mclk, int rate)
{
	int i , ret = 0;
	u_int mclk1, mclk2 = 0;

	for (i = 0; i < ARRAY_SIZE(clk_map_table); i++) {
		if (clk_map_table[i].rate == rate) {
			mclk1 = clk_map_table[i].mclk;
			{
				if (abs(mclk - mclk1) < abs(mclk - mclk2)) {
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

	if ((freq >= CS42L52_MIN_CLK) && (freq <= CS42L52_MAX_CLK)) {
		info->sysclk = freq;
	} else {
		dev_err(codec->dev, "invalid paramter\n");
		return -EINVAL;
	}
	return 0;
}

static int cs42l52_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	u8 iface = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = CS42L52_IFACE_CTL1_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = CS42L52_IFACE_CTL1_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	 /* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= CS42L52_IFACE_CTL1_ADC_FMT_I2S |
				CS42L52_IFACE_CTL1_DAC_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= CS42L52_IFACE_CTL1_DAC_FMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= CS42L52_IFACE_CTL1_ADC_FMT_LEFT_J |
				CS42L52_IFACE_CTL1_DAC_FMT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= CS42L52_IFACE_CTL1_DSP_MODE_EN;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= CS42L52_IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= CS42L52_IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}
	cs42l52->config.format = iface;
	snd_soc_write(codec, CS42L52_IFACE_CTL1, cs42l52->config.format);

	return 0;
}

static int cs42l52_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	if (mute)
		snd_soc_update_bits(codec, CS42L52_PB_CTL1,
				    CS42L52_PB_CTL1_MUTE_MASK,
				CS42L52_PB_CTL1_MUTE);
	else
		snd_soc_update_bits(codec, CS42L52_PB_CTL1,
				    CS42L52_PB_CTL1_MUTE_MASK,
				CS42L52_PB_CTL1_UNMUTE);

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

	if (index >= 0) {
		priv->sysclk = clk_map_table[index].mclk;
		clk |= (clk_map_table[index].speed << CLK_SPEED_SHIFT) |
		      (clk_map_table[index].group << CLK_32K_SR_SHIFT) |
		      (clk_map_table[index].videoclk << CLK_27M_MCLK_SHIFT) |
		      (clk_map_table[index].ratio << CLK_RATIO_SHIFT) |
		      clk_map_table[index].mclkdiv2;
#ifdef CONFIG_MANUAL_CLK
		snd_soc_write(codec, CS42L52_CLK_CTL, clk);
#else
		snd_soc_write(codec, CS42L52_CLK_CTL, 0xa0);
#endif
	} else {
		dev_err(codec->dev, "can't find out right mclk\n");
		return -EINVAL;
	}

	return 0;
}

static int cs42l52_set_bias_level(struct snd_soc_codec *codec,
					enum snd_soc_bias_level level)
{
	u8 pwrctl1 = snd_soc_read(codec, CS42L52_PWRCTL1) & 0x9f;
	u8 pwrctl2 = snd_soc_read(codec, CS42L52_PWRCTL2) & 0x07;

	switch (level) {
	case SND_SOC_BIAS_ON: /* full On */
		break;
	case SND_SOC_BIAS_PREPARE: /* partial On */
		pwrctl1 &= ~(CS42L52_PWRCTL1_PDN_CHRG | CS42L52_PWRCTL1_PDN_CODEC);
		snd_soc_write(codec, CS42L52_PWRCTL1, pwrctl1);
		break;
	case SND_SOC_BIAS_STANDBY: /* Off, with power */
		pwrctl1 &= ~(CS42L52_PWRCTL1_PDN_CHRG | CS42L52_PWRCTL1_PDN_CODEC);
		snd_soc_write(codec, CS42L52_PWRCTL1, pwrctl1);
		break;
	case SND_SOC_BIAS_OFF: /* Off, without power */
		snd_soc_write(codec, CS42L52_PWRCTL1, pwrctl1 | 0x9f);
		snd_soc_write(codec, CS42L52_PWRCTL2, pwrctl2 | 0x07);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

//#define CS42L52_RATES (SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 | \
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000) /*refer to cs42l52 datasheet page35*/

#define CS42L52_RATES (SNDRV_PCM_RATE_48000)

#define CS42L52_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE)

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
		.symmetric_rates = 1,
};

static int cs42l52_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int cs42l52_resume(struct snd_soc_codec *codec)
{
	int i;		/*, reg; */
	u8 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = CS42L52_PWRCTL1; i < ARRAY_SIZE(cs42l52_reg); i++)
		snd_soc_write(codec, i, cache[i]);

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
	/* int i, ret = 0; */

	info->sysclk = CS42L52_DEFAULT_CLK;
	info->config.format = CS42L52_DEFAULT_FORMAT;

	cs42l52_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	cs42l52_required_setup(codec);

	info->flags |= CS42L52_CHIP_SWITCH;
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

	info->flags &= ~(CS42L52_CHIP_SWITCH);
	info->flags |= CS42L52_ALL_IN_ONE;

	/*init done*/
	snd_soc_add_controls(codec, cs42l52_snd_controls,
			     ARRAY_SIZE(cs42l52_snd_controls));

/*TODO make widgets visible to DAPM*/
/*	cs42l52_add_widgets(codec);	*/

	/* Override power control default settings
		(SPK/HP pin detection in hardware is fixed to LO)*/
/*TODO Doing this elsewhere might be cleaner. Make snd_kcontrol_new structs 
	if you wish to give the user control of these settings
*/
	snd_soc_write(codec, CS42L52_PWRCTL2, 0x00);
	u8 pwrctl2 = snd_soc_read(codec, CS42L52_PWRCTL2);

	u8 pwrctl3 = snd_soc_read(codec, CS42L52_PWRCTL3);
	snd_soc_write(codec, CS42L52_PWRCTL3, 0xAA);

	return 0;
}

/* power down chip */
static int cs42l52_remove(struct snd_soc_codec *codec)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

struct i2c_client *cs_control;

static inline unsigned int cs42l52_read(struct snd_soc_codec *codec, unsigned int reg)
{
	return i2c_smbus_read_byte_data(cs_control, reg);
}

static inline int cs42l52_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int val)
{
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
	printk(KERN_ERR "We have %x from CS42L52_CHIP\n", ret);
*/
	cs42l52 = kzalloc(sizeof(struct cs42l52_private), GFP_KERNEL);
	if (!cs42l52) {
		dev_err(&i2c_client->dev, "could not allocate codec\n");
		return -ENOMEM;
	}
//
	i2c_set_clientdata(i2c_client, cs42l52);
	cs42l52->control_data = i2c_client;
	cs42l52->control_type = SND_SOC_I2C;

	cs_control = i2c_client;

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs42l52, &cs42l52_dai, 1);
	if (ret < 0)
		kfree(cs42l52);

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
