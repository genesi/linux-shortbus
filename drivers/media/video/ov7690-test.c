/*
 * A V4L2 driver for OmniVision OV7690 cameras.
 *
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>

MODULE_AUTHOR("Matt Sealey");
MODULE_DESCRIPTION("Low-level test framework for OV7960");
MODULE_LICENSE("GPL");

static int ov7690_read_smbus(struct i2c_client *client, unsigned char reg,
		unsigned char *value)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret >= 0) {
		*value = (unsigned char) ret;
		ret = 0;
	}
	return ret;
}


static int ov7690_write_smbus(struct i2c_client *client, unsigned char reg,
		unsigned char value)
{
	int ret = i2c_smbus_write_byte_data(client, reg, value);

//	if (reg == REG_COM7 && (value & COM7_RESET))
//		msleep(5);

	return ret;
}

static int ov7690_read_i2c(struct i2c_client *client, unsigned char reg,
		unsigned char *value)
{
	u8 data = reg;
	struct i2c_msg msg;
	int ret;

	/*
	 * Send out the register address...
	 */
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		printk(KERN_ERR "Error %d on register write\n", ret);
		return ret;
	}
	/*
	 * ...then read back the result.
	 */
	msg.flags = I2C_M_RD;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0) {
		*value = data;
		ret = 0;
	}
	return ret;
}


static int ov7690_write_i2c(struct i2c_client *client, unsigned char reg,
		unsigned char value)
{
	struct i2c_msg msg;
	unsigned char data[2] = { reg, value };
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;
//	if (reg == REG_COM7 && (value & COM7_RESET))
//		msleep(5);
	return ret;
}

static int ov7690_read(struct i2c_client *client, unsigned char reg,
		unsigned char *value)
{
//		return ov7690_read_smbus(client, reg, value);
		return ov7690_read_i2c(client, reg, value);
}

static int ov7690_write(struct i2c_client *client, unsigned char reg,
		unsigned char value)
{
//		return ov7690_write_smbus(client, reg, value);
		return ov7690_write_i2c(client, reg, value);
}

static int ov7690_write_array(struct i2c_client *client, struct regval_list *vals)
{
	while (vals->reg_num != 0xff || vals->value != 0xff) {
		int ret = ov7690_write(client, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}



static int ov7690_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;

	return 0;
}


static int ov7690_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ov7690_id[] = {
	{ "ov7690-test", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov7690_id);

static struct i2c_driver ov7690_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov7690-test",
	},
	.probe		= ov7690_probe,
	.remove		= ov7690_remove,
	.id_table	= ov7690_id,
};

static __init int init_ov7690(void)
{
	return i2c_add_driver(&ov7690_driver);
}

static __exit void exit_ov7690(void)
{
	i2c_del_driver(&ov7690_driver);
}

module_init(init_ov7690);
module_exit(exit_ov7690);
