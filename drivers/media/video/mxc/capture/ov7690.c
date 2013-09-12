/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*Misc defs*/
#define DEBUG
#define OV7690_MODE_ENABLE 0
#define OV7690_FIX 1

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <media/ov7690.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>

#include "mxc_v4l2_capture.h"
#include "ov7690_regs.h"


#define ov7690_VOLTAGE_ANALOG               2800000
#define ov7690_VOLTAGE_DIGITAL_CORE         1500000
#define ov7690_VOLTAGE_DIGITAL_IO           1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define ov7690_XCLK_MIN 6000000
#define ov7690_XCLK_MAX 27000000

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define QVGA_WIDTH	320
#define QVGA_HEIGHT	240
#define CIF_WIDTH	352
#define CIF_HEIGHT	288
#define QCIF_WIDTH	176
#define	QCIF_HEIGHT	144

/*
	i2c defs
	Terminating list entry for reg, val, len
*/
#define I2C_REG_TERM 	0xFF
#define I2C_VAL_TERM 	0xFF
#define I2C_LEN_TERM	0xFF


struct ov7690_reg {
	u8 addr;
	u8 val;
};

#if OV7690_MODE_ENABLE
enum ov7690_mode {
	ov7690_mode_MIN = 0,
	ov7690_mode_VGA_640_480 = 0,
	ov7690_mode_QVGA_320_240 = 1,
	ov7690_mode_NTSC_720_480 = 2,
	ov7690_mode_PAL_720_576 = 3,
	ov7690_mode_720P_1280_720 = 4,
	ov7690_mode_1080P_1920_1080 = 5,
	ov7690_mode_QSXGA_2592_1944 = 6,
	ov7690_mode_MAX =6
};

enum ov7690_frame_rate {
	ov7690_15_fps,
	ov7690_30_fps
};

struct ov7690_mode_info {
	enum ov7690_mode mode;
	u32 width;
	u32 height;
	struct ov7690_reg *init_data_ptr;
	u32 init_data_size;
};
#endif /*OV7690_MODE_ENABLE*/


/*!
 * Maintains the information on the current state of the sesor.
 */
struct sensor {
	const struct ov7690_platform_data *platform_data;
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_captureparm streamcap;
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	u32 mclk;
	int csi;
} ov7690_data;

/* attempts to solve high chroma noise
	1) low frame rate, high exposure time
*/
static struct ov7690_reg low_fr_high_exp[] = {
	/* decrease frame rate */
	{ REG_CLKRC, 	0x03 & CLK_SCALE_MASK },
	{ REG_PLL,	PLL_DIV_4 | PLL_4X },

	/* increase exposure */
	{ REG_AECH,	0x02 },
	{ REG_AECL,	0xFF },

	/*END MARKER*/
	{ I2C_REG_TERM,	I2C_VAL_TERM }
};

/*
	2) compromise: mid frame rate, mid exposure time
*/
static struct ov7690_reg mid_fr_mid_exp[] = {
	/* decrease frame rate */
	{ REG_CLKRC,	0x03 & CLK_SCALE_MASK },
	{ REG_PLL,	PLL_DIV_1 | PLL_4X },

	/* increase exposure */
	{ REG_AECH,	0x02 },
	{ REG_AECL,	0xFF },

	/*END MARKER*/
	{I2C_REG_TERM,	I2C_VAL_TERM }
};

/*! Manual white balance modes
* To use these, you must first enable manual mode for AWB
* i.e. bit 0 in REG_COM8 must be 0
*
* Taken and modified from:
* https://github.com/omegamoon/rockchip-rk30xx-mk808/blob/master/drivers/media/video/ov7690.c
*/

/* Cloudy Colour Temperature : 6500K - 8000K */
static struct ov7690_reg awb_cloudy[]=
{
	{ REG_BLUE,	0x40 },
	{ REG_RED, 	0x5d },
	{ REG_GREEN, 	0x40 },
	{ I2C_REG_TERM,	I2C_VAL_TERM }
};

/* Clear Day Colour Temperature : 5000K - 6500K */
static struct ov7690_reg awb_clear[]=
{
	{ REG_BLUE,	0x44 },
	{ REG_RED,	0x55 },
	{ REG_GREEN,	0x40 },
	{ I2C_REG_TERM,	I2C_VAL_TERM }
};

/* Office Colour Temperature : 3500K - 5000K */
static struct ov7690_reg awb_tungsten[]=
{
	{ REG_BLUE,	0x5b },
	{ REG_RED,	0x4c },
	{ REG_GREEN,	0x40 },	
	{ I2C_REG_TERM,	I2C_VAL_TERM }

};

static struct ov7690_reg *awb_manual_mode_list[] = { 
	awb_tungsten, awb_clear, awb_cloudy, NULL
};

/* Taken from https://gitorious.org/flow-g1-5/kernel_flow */
/*	TODO Output is weird - 4 screens, first column duplicated QVGA, 
second two green areas 
*/
static const struct ov7690_reg ov7690_qvga_regs[] = {
	{ 0x16, 	0x03 },
	{ 0x17, 	0x69 },
	{ 0x18, 	0xa4 },
	{ 0x19, 	0x06 },
	{ 0x1a, 	0xf6 },
	{ 0x22, 	0x10 },
	{ 0xc8, 	0x02 },
	{ 0xc9, 	0x80 },
	{ 0xca, 	0x00 },
	{ 0xcb, 	0xf0 },
	{ 0xcc, 	0x01 },
	{ 0xcd, 	0x40 },
	{ 0xce, 	0x00 },
	{ 0xcf, 	0xf0 },

	/*END MARKER*/
	{ I2C_REG_TERM, I2C_VAL_TERM }
};

#if OV7690_FIX
static struct ov7690_reg ov7690_default_regs[] = {
	{ REG_COM12,	COM12_RESET }, 			/*Reset registers*/

	{ REG_COMB4, 	COMB4_BEST_EDGE }, 		/*best edge!*/
	{ REG_COM13,	COM13_DEFAULT & (~COM13_AEC) },	/*manual exposure*/

	/*END MARKER*/
	{ I2C_REG_TERM,	I2C_VAL_TERM }
};

#else /* OV7690_FIX */
static struct ov7690_reg ov7690_default_regs[] = {
	{ REG_COM7, COM7_RESET },
/*
 * Clock scale: 3 = 15fps
 *              2 = 20fps
 *              1 = 30fps
 */
	{ REG_CLKRC, 0x1 },	/* OV: clock scale (30 fps) */
	{ REG_TSLB,  0x04 },	/* OV */
	{ REG_COM7, 0 },	/* VGA */
	/*
	 * Set the hardware window.  These values from OV don't entirely
	 * make sense - hstop is less than hstart.  But they work...
	 */
	{ REG_HSTART, 0x13 },	{ REG_HSTOP, 0x01 },
	{ REG_HREF, 0xb6 },	{ REG_VSTART, 0x02 },
/*	{ REG_VSTOP, 0x7a },	{ REG_VREF, 0x0a }, */
	{ REG_VSTOP, 0x7a },	{ REG_GREEN, 0x0a }, /* TODO Terrible mistake */

	{ REG_COM0C, 0 },	{ REG_COM14, 0 },
	/* Mystery scaling numbers */
	{ 0x70, 0x3a },		{ 0x71, 0x35 },
	{ 0x72, 0x11 },		{ 0x73, 0xf0 },
	{ 0xa2, 0x02 },		{ REG_COM10, 0x0 },

	/* Gamma curve values */
	{ 0x7a, 0x20 },		{ 0x7b, 0x10 },
	{ 0x7c, 0x1e },		{ 0x7d, 0x35 },
	{ 0x7e, 0x5a },		{ 0x7f, 0x69 },
	{ 0x80, 0x76 },		{ 0x81, 0x80 },
	{ 0x82, 0x88 },		{ 0x83, 0x8f },
	{ 0x84, 0x96 },		{ 0x85, 0xa3 },
	{ 0x86, 0xaf },		{ 0x87, 0xc4 },
	{ 0x88, 0xd7 },		{ 0x89, 0xe8 },

	/* AGC and AEC parameters.  Note we start by disabling those features,
	   then turn them only after tweaking the values. */
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT },
	{ REG_GAIN, 0 },	{ REG_AECH, 0 },
	{ REG_COM4, 0x40 }, /* magic reserved bit */
	{ REG_COM9, 0x18 }, /* 4x gain + magic rsvd bit */
	{ REG_BD50MAX, 0x05 },	{ REG_BD60MAX, 0x07 },
	{ REG_AEW, 0x95 },	{ REG_AEB, 0x33 },
	{ REG_VPT, 0xe3 },	{ REG_HAECC1, 0x78 },
	{ REG_HAECC2, 0x68 },	{ 0xa1, 0x03 }, /* magic */
	{ REG_HAECC3, 0xd8 },	{ REG_HAECC4, 0xd8 },
	{ REG_HAECC5, 0xf0 },	{ REG_HAECC6, 0x90 },
	{ REG_HAECC7, 0x94 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC },

	/* Almost all of these are magic "reserved" values.  */
	{ REG_COM5, 0x61 },	{ REG_AECH, 0x4b },
	{ 0x16, 0x02 },		{ REG_MVFP, 0x07 },
	{ 0x21, 0x02 },		{ 0x22, 0x91 },
	{ 0x29, 0x07 },		{ 0x33, 0x0b },
	{ 0x35, 0x0b },		{ 0x37, 0x1d },
	{ 0x38, 0x71 },		{ 0x39, 0x2a },
	{ REG_COM12, 0x78 },	{ 0x4d, 0x40 },
	{ 0x4e, 0x20 },		{ REG_GFIX, 0 },
	{ 0x6b, 0x4a },		{ 0x74, 0x10 },
	{ 0x8d, 0x4f },		{ 0x8e, 0 },
	{ 0x8f, 0 },		{ 0x90, 0 },
	{ 0x91, 0 },		{ 0x96, 0 },
	{ 0x9a, 0 },		{ 0xb0, 0x84 },
	{ 0xb1, 0x0c },		{ 0xb2, 0x0e },
	{ 0xb3, 0x82 },		{ 0xb8, 0x0a },

	/* More reserved magic, some of which tweaks white balance */
	{ 0x43, 0x0a },		{ 0x44, 0xf0 },
	{ 0x45, 0x34 },		{ 0x46, 0x58 },
	{ 0x47, 0x28 },		{ 0x48, 0x3a },
	{ 0x59, 0x88 },		{ 0x5a, 0x88 },
	{ 0x5b, 0x44 },		{ 0x5c, 0x67 },
	{ 0x5d, 0x49 },		{ 0x5e, 0x0e },
	{ 0x6c, 0x0a },		{ 0x6d, 0x55 },
	{ 0x6e, 0x11 },		{ 0x6f, 0x9f }, /* "9e for advance AWB" */
	{ 0x6a, 0x40 },		{ REG_BLUE, 0x40 },
	{ REG_RED, 0x60 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC|COM8_AWB },

	/* Matrix coefficients */
	{ 0x4f, 0x80 },		{ 0x50, 0x80 },
	{ 0x51, 0 },		{ 0x52, 0x22 },
	{ 0x53, 0x5e },		{ 0x54, 0x80 },
	{ 0x58, 0x9e },

	{ REG_COM16, COM16_AWBGAIN },	{ REG_EDGE, 0 },
	{ 0x75, 0x05 },		{ 0x76, 0xe1 },
	{ 0x4c, 0 },		{ 0x77, 0x01 },
	{ REG_COM13, 0xc3 },	{ 0x4b, 0x09 },
	{ 0xc9, 0x60 },		{ REG_COM16, 0x38 },
	{ 0x56, 0x40 },

	{ 0x34, 0x11 },		{ REG_COM11, COM11_EXP|COM11_HZAUTO },
	{ 0xa4, 0x88 },		{ 0x96, 0 },
	{ 0x97, 0x30 },		{ 0x98, 0x20 },
	{ 0x99, 0x30 },		{ 0x9a, 0x84 },
	{ 0x9b, 0x29 },		{ 0x9c, 0x03 },
	{ 0x9d, 0x4c },		{ 0x9e, 0x3f },
	{ 0x78, 0x04 },

	/* Extra-weird stuff.  Some sort of multiplexor register */
	{ 0x79, 0x01 },		{ 0xc8, 0xf0 },
	{ 0x79, 0x0f },		{ 0xc8, 0x00 },
	{ 0x79, 0x10 },		{ 0xc8, 0x7e },
	{ 0x79, 0x0a },		{ 0xc8, 0x80 },
	{ 0x79, 0x0b },		{ 0xc8, 0x01 },
	{ 0x79, 0x0c },		{ 0xc8, 0x0f },
	{ 0x79, 0x0d },		{ 0xc8, 0x20 },
	{ 0x79, 0x09 },		{ 0xc8, 0x80 },
	{ 0x79, 0x02 },		{ 0xc8, 0xc0 },
	{ 0x79, 0x03 },		{ 0xc8, 0x40 },
	{ 0x79, 0x05 },		{ 0xc8, 0x30 },
	{ 0x79, 0x26 },

	/* END MARKER */
	{ I2C_REG_TERM,	I2C_VAL_TERM },
};
#endif /* OV7690_FIX */

static struct regulator *io_regulator;
static struct regulator *core_regulator;
static struct regulator *analog_regulator;
static struct regulator *gpo_regulator;
static struct fsl_mxc_camera_platform_data *camera_plat;

static int ov7690_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int ov7690_remove(struct i2c_client *client);

static s32 ov7690_read_reg(u8 reg, u8 *val);
static s32 ov7690_write_reg(u8 reg, u8 val);

static const struct i2c_device_id ov7690_id[] = {
	{"ov7690", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov7690_id);

static struct i2c_driver ov7690_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "ov7690",
		  },
	.probe  = ov7690_probe,
	.remove = ov7690_remove,
	.id_table = ov7690_id,
};

#if OV7690_FIX
static s32 ov7690_read_reg(u8 reg, u8 *val)
{
        int ret = 0;

        struct i2c_client *client = ov7690_data.i2c_client;

        if (!client->adapter)
                return -ENODEV;

        ret = i2c_smbus_read_byte_data(client, reg);

        if (ret >= 0) {
                *val = (uint8_t) ret;
                ret = 0;
        }

        return ret;
}

static s32 ov7690_write_reg(u8 reg, u8 val)
{
        int ret = 0;

        struct i2c_client *client = ov7690_data.i2c_client;

        if (!client->adapter)
                return -ENODEV;

        ret = i2c_smbus_write_byte_data(client, reg, val);

        if (reg == REG_COM12 && (val & COM12_RESET))
                msleep(2); /*Wait for reset to run*/

        return ret;
}

#else

static s32 ov7690_write_reg(u16 reg, u8 val)
{
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(ov7690_data.i2c_client, au8Buf, 3) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 ov7690_read_reg(u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(ov7690_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (1 != i2c_master_recv(ov7690_data.i2c_client, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

	*val = u8RdVal;

	return u8RdVal;
}

#endif /* OV7690_FIX */

static s32 ov7690_write_regs(const struct ov7690_reg *reglist)
{
	int err = 0;
	const struct ov7690_reg *reg_rover = &reglist[0];

	while (!((reg_rover->addr == I2C_REG_TERM) 
		&& (reg_rover->val == I2C_VAL_TERM))) {

		err = ov7690_write_reg(reg_rover->addr, reg_rover->val);

		if (err)
			return err;

		msleep(1); /* TODO ? */
		++reg_rover;
	}

	return 0;
}

static s32 ov7690_init_regs()
{
	s32 err = 0;

	err = ov7690_write_regs(ov7690_default_regs);/*System reset, wait 2ms*/
	err |=ov7690_write_regs(mid_fr_mid_exp);

	return err;
}

#if OV7690_MODE_ENABLE 
static int ov7690_init_mode(enum ov7690_frame_rate frame_rate,
			    enum ov7690_mode mode)
{
	struct ov7690_reg *pModeSetting = NULL;
	s32 i = 0;
	s32 iModeSettingArySize = 0;
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u8 Mask = 0;
	register u8 Val = 0;
	u8 RegVal = 0;
	int retval = 0;

	if (mode > ov7690_mode_MAX || mode < ov7690_mode_MIN) {
		pr_err("Wrong ov7690 mode detected!\n");
		return -1;
	}

	pModeSetting = ov7690_mode_info_data[frame_rate][mode].init_data_ptr;
	iModeSettingArySize =
		ov7690_mode_info_data[frame_rate][mode].init_data_size;

	ov7690_data.pix.width = ov7690_mode_info_data[frame_rate][mode].width;
	ov7690_data.pix.height = ov7690_mode_info_data[frame_rate][mode].height;

	if (ov7690_data.pix.width == 0 || ov7690_data.pix.height == 0 ||
	    pModeSetting == NULL || iModeSettingArySize == 0)
		return -EINVAL;

	for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
		Delay_ms = pModeSetting->u32Delay_ms;
		RegAddr = pModeSetting->addr;
		Val = pModeSetting->val;
		Mask = pModeSetting->u8Mask;

		if (Mask) {
			retval = ov7690_read_reg(RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			RegVal &= ~(u8)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = ov7690_write_reg(RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			msleep(Delay_ms);
	}
err:
	return retval;
}
#endif /* OV7690_MODE_ENABLE */
/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	printk("%s: setting parameters on device\n", __func__);

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = ov7690_data.mclk;
	printk("   clock_curr=mclk=%d\n", ov7690_data.mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = ov7690_XCLK_MIN;
	p->u.bt656.clock_max = ov7690_XCLK_MAX;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor *sensor = s->priv;

	printk("%s: setting power mode\n", __func__);

	if (on && !sensor->on) {
		if (io_regulator)
			if (regulator_enable(io_regulator) != 0)
				return -EIO;
		if (core_regulator)
			if (regulator_enable(core_regulator) != 0)
				return -EIO;
		if (gpo_regulator)
			if (regulator_enable(gpo_regulator) != 0)
				return -EIO;
		if (analog_regulator)
			if (regulator_enable(analog_regulator) != 0)
				return -EIO;
		/* Make sure power on */
		if (camera_plat->pwdn)
			camera_plat->pwdn(0);

	} else if (!on && sensor->on) {
		if (analog_regulator)
			regulator_disable(analog_regulator);
		if (core_regulator)
			regulator_disable(core_regulator);
		if (io_regulator)
			regulator_disable(io_regulator);
		if (gpo_regulator)
			regulator_disable(gpo_regulator);
	}

	sensor->on = on;

	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	printk("%s: Trying to parameterize video capture settings\n", __func__);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		printk("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
//	enum ov7690_frame_rate frame_rate;
	int ret = 0;

	printk("%s: Trying to parameterize settings\n", __func__);

	/* Make sure power on */
	if (camera_plat->pwdn)
		camera_plat->pwdn(0);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

#if OV7690_MODE_ENABLE
		if (tgt_fps == 15)
			frame_rate = ov7690_15_fps;
		else if (tgt_fps == 30)
			frame_rate = ov7690_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			return -EINVAL;
		}
#endif /* OV7690_MODE_ENABLE */

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		//ret = ov7690_init_mode(frame_rate,
		//		       sensor->streamcap.capturemode);
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		printk("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		printk("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor *sensor = s->priv;

	printk("%s: Setting image g format(?)\n", __func__);

	f->fmt.pix = sensor->pix;

	return 0;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

	printk("%s: called(?)\n", __func__);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = ov7690_data.brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = ov7690_data.hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = ov7690_data.contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = ov7690_data.saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = ov7690_data.red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = ov7690_data.blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = ov7690_data.ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;

	printk("In ov7690:ioctl_s_ctrl %d\n",
		 vc->id);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
		break;
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		retval = -EPERM;
		break;
	}

	return retval;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
//	if (fsize->index > ov7690_mode_MAX)
//		return -EINVAL;

	fsize->pixel_format = ov7690_data.pix.pixelformat;
	fsize->discrete.width = 640;
			//max(ov7690_mode_info_data[0][fsize->index].width,
			//    ov7690_mode_info_data[1][fsize->index].width);
	fsize->discrete.height = 480;
			//max(ov7690_mode_info_data[0][fsize->index].height,
			//    ov7690_mode_info_data[1][fsize->index].height);

	printk("%s: setting framesize\n", __func__);

	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "ov7690_camera");

	printk("%s: called(?)\n", __func__);

	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	printk("%s: (again) initializing device\n", __func__);

	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
//	if (fmt->index > ov7690_mode_MAX)
//		return -EINVAL;

	printk("%s: handling video fmt\n", __func__);

	fmt->pixelformat = ov7690_data.pix.pixelformat;

	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct sensor *sensor = s->priv;
	u32 tgt_xclk;	/* target xclk */
	u32 tgt_fps;	/* target frames per secound */
	//enum ov7690_frame_rate frame_rate;

	printk("%s: initializing ov7690\n", __func__);

	ov7690_data.on = true;

	/* mclk */
	tgt_xclk = ov7690_data.mclk;
	tgt_xclk = min(tgt_xclk, (u32)ov7690_XCLK_MAX);
	tgt_xclk = max(tgt_xclk, (u32)ov7690_XCLK_MIN);
	ov7690_data.mclk = tgt_xclk;

	printk("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	set_mclk_rate(&ov7690_data.mclk, ov7690_data.csi);

	/* Default camera frame rate is set in probe */
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

#if OV7690_MODE_ENABLE
	if (tgt_fps == 15)
		frame_rate = ov7690_15_fps;
	else if (tgt_fps == 30)
		frame_rate = ov7690_30_fps;
	else
		return -EINVAL; /* Only support 15fps or 30fps now. */
#endif /* OV7690_MODE_ENABLE */

	ov7690_init_regs();

	return 0;//ov7690_init_mode(frame_rate,
		//		sensor->streamcap.capturemode);
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	printk("%s: exiting ov7690\n", __func__);

	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc ov7690_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *)ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *)ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *)ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *)ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},
/*	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_g_fmt_cap},
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_s_fmt_cap}, */
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *)ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *)ioctl_s_parm},
/*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *)ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *)ioctl_g_chip_ident},
};

static struct v4l2_int_slave ov7690_slave = {
	.ioctls = ov7690_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(ov7690_ioctl_desc),
};

static struct v4l2_int_device ov7690_int_device = {
	.module = THIS_MODULE,
	.name = "ov7690",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &ov7690_slave,
	},
};

/*!
 * ov7690 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int ov7690_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int retval;
	struct fsl_mxc_camera_platform_data *plat_data = client->dev.platform_data;

	printk("%s: probe started\n", __func__);

	/* Set initial values for the sensor struct. */
	memset(&ov7690_data, 0, sizeof(ov7690_data));
	ov7690_data.mclk = 24000000; /* 6 - 54 MHz, typical 24MHz */
	ov7690_data.mclk = plat_data->mclk;
	ov7690_data.csi = plat_data->csi;

	ov7690_data.i2c_client = client;
	ov7690_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	ov7690_data.pix.width = 640;
	ov7690_data.pix.height = 480;
	ov7690_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	ov7690_data.streamcap.capturemode = 0;
	ov7690_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
	ov7690_data.streamcap.timeperframe.numerator = 1;

	if (plat_data->io_regulator) {
		io_regulator = regulator_get(&client->dev,
					     plat_data->io_regulator);
		if (!IS_ERR(io_regulator)) {
			regulator_set_voltage(io_regulator,
					      ov7690_VOLTAGE_DIGITAL_IO,
					      ov7690_VOLTAGE_DIGITAL_IO);
			if (regulator_enable(io_regulator) != 0) {
				pr_err("%s:io set voltage error\n", __func__);
				goto err1;
			} else {
				dev_dbg(&client->dev,
					"%s:io set voltage ok\n", __func__);
			}
		} else
			io_regulator = NULL;
	}

	if (plat_data->core_regulator) {
		core_regulator = regulator_get(&client->dev,
					       plat_data->core_regulator);
		if (!IS_ERR(core_regulator)) {
			regulator_set_voltage(core_regulator,
					      ov7690_VOLTAGE_DIGITAL_CORE,
					      ov7690_VOLTAGE_DIGITAL_CORE);
			if (regulator_enable(core_regulator) != 0) {
				pr_err("%s:core set voltage error\n", __func__);
				goto err2;
			} else {
				dev_dbg(&client->dev,
					"%s:core set voltage ok\n", __func__);
			}
		} else
			core_regulator = NULL;
	}

	if (plat_data->analog_regulator) {
		analog_regulator = regulator_get(&client->dev,
						 plat_data->analog_regulator);
		if (!IS_ERR(analog_regulator)) {
			regulator_set_voltage(analog_regulator,
					      ov7690_VOLTAGE_ANALOG,
					      ov7690_VOLTAGE_ANALOG);
			if (regulator_enable(analog_regulator) != 0) {
				pr_err("%s:analog set voltage error\n",
					__func__);
				goto err3;
			} else {
				dev_dbg(&client->dev,
					"%s:analog set voltage ok\n", __func__);
			}
		} else
			analog_regulator = NULL;
	}

	if (plat_data->pwdn)
		plat_data->pwdn(0);

	camera_plat = plat_data;

	ov7690_int_device.priv = &ov7690_data;
	retval = v4l2_int_device_register(&ov7690_int_device);

	return retval;

err3:
	if (core_regulator) {
		regulator_disable(core_regulator);
		regulator_put(core_regulator);
	}
err2:
	if (io_regulator) {
		regulator_disable(io_regulator);
		regulator_put(io_regulator);
	}
err1:
	return -1;
}

/*!
 * ov7690 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov7690_remove(struct i2c_client *client)
{
	v4l2_int_device_unregister(&ov7690_int_device);

	if (gpo_regulator) {
		regulator_disable(gpo_regulator);
		regulator_put(gpo_regulator);
	}

	if (analog_regulator) {
		regulator_disable(analog_regulator);
		regulator_put(analog_regulator);
	}

	if (core_regulator) {
		regulator_disable(core_regulator);
		regulator_put(core_regulator);
	}

	if (io_regulator) {
		regulator_disable(io_regulator);
		regulator_put(io_regulator);
	}

	return 0;
}

/*!
 * ov7690 init function
 * Called by insmod ov7690_camera.ko.
 *
 * @return  Error code indicating success or failure
 */
static __init int ov7690_init(void)
{
	u8 err;

	printk("%s: registration started\n", __func__);

	err = i2c_add_driver(&ov7690_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d \n",
			__func__, err);

	return err;
}

/*!
 * ov7690 cleanup function
 * Called on rmmod ov7690_camera.ko
 *
 * @return  Error code indicating success or failure
 */
static void __exit ov7690_clean(void)
{
	i2c_del_driver(&ov7690_i2c_driver);
}

#undef OV7690_MODE_ENABLE

module_init(ov7690_init);
module_exit(ov7690_clean);

MODULE_AUTHOR("Genesi, Inc.");
MODULE_DESCRIPTION("ov7690 Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
