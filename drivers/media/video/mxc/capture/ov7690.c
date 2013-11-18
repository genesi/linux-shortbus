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
#include <linux/string.h>
#include <media/ov7690.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>

#include "mxc_v4l2_capture.h"
#include "ov7690.h"


/*!
 * Misc defs
 */

/* For future implementation of V4L2 Controls */
#define OV7690_V4L2_CTRL_ENABLE 0

/*
 * Low-level device defs
 */
#define ov7690_VOLTAGE_ANALOG               2800000
#define ov7690_VOLTAGE_DIGITAL_CORE         1500000
#define ov7690_VOLTAGE_DIGITAL_IO           1800000

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
	
	enum ov7690_mode mode;				/*	TODO add code to keep track of this	*/
	enum ov7690_frame_rate frame_rate;	/*										*/

	u32 mclk;
	int csi;
} ov7690_data;


/*!
* Module global variables
*/
static struct regulator *io_regulator;
static struct regulator *core_regulator;
static struct regulator *analog_regulator;
static struct regulator *gpo_regulator;
static struct fsl_mxc_camera_platform_data *camera_plat;


/*!
*	Function prototypes
*/
static int ov7690_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int ov7690_remove(struct i2c_client *client);

/* Helper register functions*/
static s32 ov7690_read_reg_raw(u8 reg, u8 *val);
static s32 ov7690_write_reg_raw(u8 reg, u8 val);

static s32 
ov7690_write_reg_control(const struct ov7690_reg_control ctrl, const u8 value);
static s32
ov7690_read_reg_control(const struct ov7690_reg_control ctrl, u8 *value);

static s32 ov7690_write_regs(const struct ov7690_reg * reglist);

/* Keeps v4l2 image data synced with registers */
static s32
ov7690_sync_data_and_regs(void);

/* Mode functions */
static s32
ov7690_init_mode(enum ov7690_frame_rate frame_rate,	enum ov7690_mode mode);
static enum ov7690_frame_rate
ov7690_mode_get_closest_fps(struct v4l2_fract * timeperframe);
static inline struct ov7690_mode_info
ov7690_mode_get_best_fps_mode(enum ov7690_mode);


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


/*! 
* Helper register functions (definitions)
*/
static s32 ov7690_read_reg_raw(u8 reg, u8 *val)
{
        int ret = 0;

        struct i2c_client *client = ov7690_data.i2c_client;

        if (!client->adapter)
                return -ENODEV;

        ret = i2c_smbus_read_byte_data(client, reg);
        pr_debug("ov7690.c:%s: %x %x\n",
         	__func__, reg, ret);

        if (ret >= 0) {
                *val = (uint8_t) ret;
                ret = 0;
        }

        return ret;
}

static s32 ov7690_write_reg_raw(u8 reg, u8 val)
{
        int ret = 0;

        struct i2c_client *client = ov7690_data.i2c_client;

        if (!client->adapter)
                return -ENODEV;

        ret = i2c_smbus_write_byte_data(client, reg, val);
        pr_debug("ov7690.c:%s: %x %x\n", 
            __func__, reg, val);

        if (reg == REG_COM12 && (val & COM12_RESET))
                msleep(2); /*Wait for reset to run*/

        return ret;
}


/*
 * ov7690_read_reg_control - helper function to put reg control value 
 *  in human-readable format
 *
 * @ctrl	- the desired control to read. Should have addr, mask set
 * @value	- location to write control value
 *
 * returns success status, does not touch value on failure
 */
static s32
ov7690_read_reg_control(const struct ov7690_reg_control ctrl, u8 *value)
{
	s32 ret = 0;
	u8 tmp_val = 0;

	ret = ov7690_read_reg_raw(ctrl.addr, &tmp_val);

	if (ret < 0) /* Exit on error */
		return ret;


	if (!IS_MASKED(ctrl)) {
		*value = tmp_val;
	}
	else {
		tmp_val &= ctrl.mask; /* Mask out control bits */
		*value = /* Divide by power of two to right shift appropriately */
			tmp_val / (ctrl.mask & ~(ctrl.mask << 1)); /* TODO performance impact? */
	}

	return ret;
}

/*
 * ov7690_write_reg_control - helper function to write human-readable value
 *	to register
 *
 * @ctrl	- the desired control to write. Should have addr, mask set
 * @value	- value to write
 *
 * returns success status
 */
static s32
ov7690_write_reg_control(const struct ov7690_reg_control ctrl, const u8 value)
{
	s32 ret = 0;
	u8 tmp_val = 0;

	if (!IS_MASKED(ctrl)) {
		ret = ov7690_write_reg_raw(ctrl.addr, value);
	}
	else {
		u8 shifted_value = /* Multiply by power of two to left shift appropriately */
			value * (ctrl.mask & ~(ctrl.mask << 1));
		ret = ov7690_read_reg_raw(ctrl.addr, &tmp_val);

		if (ret < 0) /* Error, exit without writing */
			goto read_err;

		tmp_val &= ~(ctrl.mask); /* clear bits */
		tmp_val |= (ctrl.mask & shifted_value); /* set bits */

		ret |= ov7690_write_reg_raw(ctrl.addr, tmp_val);

		if (ret < 0)
			goto write_err;
	}

	return ret;

read_err:
	pr_err("%s: could not read register %x",
		__func__, ctrl.addr);
	return ret;

write_err:
	pr_err("%s: could not write %d to control in register %x",
		__func__, value, ctrl.addr);
	return ret;
}

/* Writes list of register structs */
static s32 ov7690_write_regs(const struct ov7690_reg *reglist)
{
	int error = 0;
	const struct ov7690_reg *reg_rover = &reglist[0];
	struct ov7690_reg_control temp_ctrl = {0, 0};

	temp_ctrl.addr = reg_rover->addr;
	temp_ctrl.mask = reg_rover->mask;

	while (!((reg_rover->addr == I2C_REG_TERM) 
		&& (reg_rover->val == I2C_VAL_TERM))) {

		error = ov7690_write_reg_control(temp_ctrl, reg_rover->val);
		if (error < 0)
			goto err;

		msleep(1); /* TODO ? */

		/* update loop variables */
		++reg_rover;
		temp_ctrl.addr = reg_rover->addr;
		temp_ctrl.mask = reg_rover->mask;
	}

	return 0;

err:
	pr_err("%s: unable to write registers!", __func__);
	return error;
}


static s32 /* TODO finish */
ov7690_sync_data_and_regs()
{
	u8 tmp = 0;
	s32 ret = 0;
	struct ov7690_reg_control tmp_reg_ctrl = {0,0};


	/* Colors */
	ret |= ov7690_read_reg_raw(REG_BLUE, &tmp);
	ov7690_data.blue = tmp;

	ret |= ov7690_read_reg_raw(REG_RED, &tmp);
	ov7690_data.red = tmp;

	ret |= ov7690_read_reg_raw(REG_GREEN, &tmp);
	ov7690_data.green = tmp;


	/* Brightness */
	ret |= ov7690_read_reg_raw(REG_YBRIGHT, &tmp); /* TODO probably wrong! */
	ov7690_data.brightness = tmp;


	/* Contrast */
	ret |= ov7690_read_reg_raw(REG_YOFFSET, &tmp);
	ov7690_data.contrast = tmp;

	/* Automatic exposure */
	tmp_reg_ctrl.addr = REG_COM13; tmp_reg_ctrl.mask = COM13_AEC;
	ret |= ov7690_read_reg_control(tmp_reg_ctrl, &tmp);
	ov7690_data.ae_mode = tmp;

	/*TODO: saturation, hue*/

	return ret;
}

/*!
 *	Functions for modes
 */

/*
 * ov7690_init_mode - Set the camera to the given mode of operation & frame rate
 * @frame_rate: frame rate
 * @mode: camera mode
 *
 * Will always reset to default register values before setting mode.
 * TODO: performance impact? unlikely
 */
static s32
ov7690_init_mode(enum ov7690_frame_rate frame_rate, enum ov7690_mode mode)
{
	int error = 0;
	const struct ov7690_mode_info * my_mode_info = NULL;
	const struct ov7690_reg * my_mode_reg_list = NULL;

	/* check bounds */
	if (!IS_VALID_MODE(mode)) {
		pr_err("%s: bad mode detected\n", __func__);
		return -1;
	}

	/* fetch mode */
	my_mode_info = &ov7690_mode_info_data[frame_rate][mode];
	my_mode_reg_list = my_mode_info->reg_list;

	/* check sane mode */
	if (MODE_FPS_NOT_SUPPORTED(*my_mode_info))
		goto err_fps_mode_unsup;

	/* write mode */
	error = ov7690_write_regs(ov7690_default_regs);
	error |= ov7690_write_regs(my_mode_reg_list);

	if (error < 0)
		goto err_bad_writes;

	/* update ov7690_data */
	ov7690_data.pix.width = my_mode_info->width;
	ov7690_data.pix.height = my_mode_info->height;
	ov7690_data.mode = mode;
	ov7690_data.frame_rate = frame_rate;
	ov7690_sync_data_and_regs();

	pr_debug("ov7690:%s: success\n", __func__);
	return 0;

err_fps_mode_unsup:
	pr_err("ov7690.c:%s: frame-rate %d not supported for mode %s!\n", __func__,
		ov7690_frame_rate_values[frame_rate], ov7690_mode_names[mode]);
	return -EINVAL;

err_bad_writes:
	pr_err("%s: something went horribly wrong writing registers!\n", __func__);
	return error;
}

/*
* ov790_mode_get_closest_fps - helper function
* TODO: it's ugly to both return a value and change the value of your parameter
*/
static enum ov7690_frame_rate 
ov7690_mode_get_closest_fps(struct v4l2_fract * timeperframe)
{
	u32 tgt_fps = 0; /* the fps asked for */
	u32 act_fps = 0; /* the actual frame rate */

	/* Check that framerate is allowed */
	if (timeperframe->numerator == 0 || timeperframe->denominator == 0) {
		timeperframe->numerator = 1;
		timeperframe->denominator = DEFAULT_FPS;
	}

	tgt_fps = timeperframe->denominator / timeperframe->numerator;

	if (tgt_fps > MAX_FPS) {
		timeperframe->denominator = MAX_FPS;
		timeperframe->numerator = 1;
	}
	else if (tgt_fps < MIN_FPS) {
		timeperframe->denominator = MIN_FPS;
		timeperframe->numerator = 1;
	}

	/* The actual frame rate we will use */
	act_fps = timeperframe->denominator / timeperframe->numerator;

	pr_debug("\ttarget fps: %d; actual fps: %d\n", tgt_fps, act_fps);

	switch(act_fps) {
		case 30:
			return ov7690_30_fps;
			break;
		case 60:
			return ov7690_60_fps;
			break;
		default:
			return -EINVAL;
	}
}

static inline struct ov7690_mode_info
ov7690_mode_get_best_fps_mode(enum ov7690_mode requested_mode)
{
	struct ov7690_mode_info requested_mode_info;

	/* prefer 60fps */
	requested_mode_info = 
		ov7690_mode_info_data[ov7690_60_fps][requested_mode];

	/* ... but go to lowest if not supported */
	if (MODE_FPS_NOT_SUPPORTED(requested_mode_info))
		requested_mode_info = 
			ov7690_mode_info_data[ov7690_30_fps][requested_mode];

	return requested_mode_info;
}


/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

/*!
* ioctl_g_ifparm - V4L2 sensort interface handler for VIDIOC_G_IFPARM ioctl
* @s: pointer to standard V4L2 device structure
* @p: pointer to structure containing low-level device parameters
*
* Provides the low level device paramters for this device
*/
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	pr_debug("%s: getting low-level parameters on device\n", __func__);

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

	printk("ov7690.c:%s: setting power mode\n", __func__);

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

	printk("%s: Getting video capture parameters\n", __func__);

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
	enum ov7690_frame_rate frame_rate;
	int ret = 0;

	printk("ov7690.c:%s: Setting video capture parameters\n", __func__);

	/* Make sure power on */
	if (camera_plat->pwdn)
		camera_plat->pwdn(0);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("\tcase V4L2_BUF_TYPE_VIDEO_CAPTURE\n");

		frame_rate = 
			ov7690_mode_get_closest_fps(timeperframe);

		if (frame_rate < 0)
			goto err_frame_rate;

		if (!IS_VALID_MODE((u32)a->parm.capture.capturemode) )
			goto err_invalid_mode;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		pr_debug("\tabout to initialize mode: fps: %d, mode: %d\n",
			frame_rate, sensor->streamcap.capturemode);
		ret = ov7690_init_mode(ov7690_30_fps,			/* TODO temporary hack to test hypothesis */
				       sensor->streamcap.capturemode);
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

	pr_debug("ov7690.c:%s: returing code %d\n",
		__func__, ret);
	return ret;

err_frame_rate:
	pr_err("ov7690.c:%s: could not match desired to available fps\n", __func__);
	return -EINVAL;

err_invalid_mode:
	pr_err("ov7690.c:%s: unsupported mode %d. mode must be between %d and %d (inclusive)\n", 
		__func__, (u32)a->parm.capture.capturemode, ov7690_mode_MIN+1, ov7690_mode_MAX-1);
	return -EINVAL;
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

	pr_debug("ov7690.c:%s: getting sensor pixel format\n", __func__);

	f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
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

	printk("%s: getting value of camera control\n", __func__);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = ov7690_data.brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = ov7690_data.hue; /* TODO not implemented */
		break;
	case V4L2_CID_CONTRAST:
		vc->value = ov7690_data.contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = ov7690_data.saturation; /* TODO not implemented */
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
 /* TODO Implement! */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

	pr_debug("ov7690.c:%s: %d\n", __func__, vc->id);


#if OV7690_V4L2_CTRL_ENABLE

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = ov7690_write_reg_raw(REG_YBRIGHT, (u8) vc->value); /* TODO this is probably wrong! */
		ov7690_data.brightness = vc->value;
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
	{
		u8 write_me = (vc->value != 0);
		struct ov7690_reg_control awb = { REG_COM13, COM13_AWB };
		ret = ov7690_write_reg_control(awb, write_me);
		break;
	}
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		u8 write_me = (u8) vc->value;
		ret = ov7690_write_reg_raw(REG_RED, write_me);
		ov7690_data.red = write_me;
		break;
	case V4L2_CID_BLUE_BALANCE:
		u8 write_me = (u8) vc->value;
		ret = ov7690_write_reg_raw(REG_BLUE, write_me);
		ov7690_data.blue = write_me;
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
	{
		u8 write_me = (vc->value != 0);
		struct ov7690_reg_control agc = { REG_COM13, COM13_AGC };
		ret = ov7690_write_reg_control(agc, write_me);
		break;
	}
	case V4L2_CID_GAIN: /* TODO only analog gain is supported */
	{
		u8 gain_value = min(max(0, vc->value + 40), 80); /* TODO magic numbers */
		u8 gain_value_big_part = (gain_value / 16);
		u8 gain_value_small_part = gain_value % 16;
		u8 write_me_big_part = ((1 << gain_value_big_part) - 1) << 4; /* TODO unreadable */
		u8 write_me_small_part = gain_value_small_part;
		u8 write_me = write_me_small_part + write_me_big_part;

		ret = ov7690_write_reg_raw(REG_GAIN, write_me);
		break;
	}
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		ret = -EPERM;
		break;
	}

#else /* OV7690_V4L2_CTRL_ENABLE */

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
        ret = -EPERM;
        break;
    }

#endif /* OV7690_V4L2_CTRL_ENABLE */

	return ret;
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
	struct ov7690_mode_info requested_mode_info;
	enum ov7690_mode requested_mode = fsize->index;

	pr_debug("ov7690.c:%s: asked to describe frame sizes for mode %d\n",
		__func__, fsize->index);

	if (!IS_VALID_MODE(requested_mode)) {
		pr_debug("ov7690.c:%s: mode %d is invalid\n",
			__func__, requested_mode);
		return -EINVAL;
	}

	requested_mode_info = ov7690_mode_get_best_fps_mode(requested_mode);

	fsize->pixel_format = requested_mode_info.pixel_format;
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width =
		requested_mode_info.width;
	fsize->discrete.height = 
		requested_mode_info.height;

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
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	struct ov7690_mode_info requested_mode_info;
	enum ov7690_mode requested_mode = fmt->index;

	pr_debug("ov7690.c:%s: asked to enumerate mode %d\n", 
		__func__, fmt->index);

	if (!IS_VALID_MODE(requested_mode)) {
		pr_debug("ov7690.c:%s: mode %d is invalid\n",
			__func__, requested_mode);
		return -EINVAL;
	}

	requested_mode_info = ov7690_mode_get_best_fps_mode(requested_mode);

	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->pixelformat = requested_mode_info.pixel_format;
	strncpy(fmt->description,
		requested_mode_info.description, sizeof(fmt->description));

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
	enum ov7690_frame_rate frame_rate;

	int ret = 0;

	printk("ov7690.c:%s: initializing ov7690\n", __func__);

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

	if (tgt_fps == 30) /* TODO refactor into "get_closest_fps" */
		frame_rate = ov7690_30_fps;
	else if (tgt_fps == 60)
		frame_rate = ov7690_60_fps;
	else { /* Only support 15fps or 30fps now. */
		printk("ov7690.c:%s: fps %d not supported!\n", 
			__func__, tgt_fps);
		return -EINVAL; 
	}

	/* Initialize the mode for the camera at given fps */
	ret = ov7690_init_mode(frame_rate, 
		sensor->streamcap.capturemode);
	ov7690_sync_data_and_regs();

	pr_debug("ov7690 picture settings:\n");
	pr_debug("brightness:\t%d\n", ov7690_data.brightness);
	pr_debug("contrast:\t%d\n", ov7690_data.contrast);

	pr_debug("blue:\t%d\n", ov7690_data.blue);
	pr_debug("red:\t%d\n", ov7690_data.red);
	pr_debug("green:\t%d\n", ov7690_data.green);

	pr_debug("exposure:\t%d\n", ov7690_data.ae_mode);

	return ret;
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
 /* TODO neither this nor ioctl_dev_init do anything! */
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

#undef OV7690_V4L2_CTRL_ENABLE

module_init(ov7690_init);
module_exit(ov7690_clean);

MODULE_AUTHOR("Genesi, Inc.");
MODULE_DESCRIPTION("ov7690 Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
