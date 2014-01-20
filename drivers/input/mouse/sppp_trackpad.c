/*
 * Copyright (C) 2011 Genesi USA, Inc. All Rights Reserved.
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


#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/delay.h>

#include <linux/sppp.h>

static struct input_dev *track_dev;

static struct sppp_client sppp_trackpad_client;

/* Decode trackpad data, which is encoded as PS/2.
 * PS/2 data is send as a 2 byte value, first one is a sequence number
 * second one is the PS/2 data. The sequence number will change for independant
 * PS/2 data. A single byte value from a PS/2 device will have one sequence number,
 * as well as every bytes out of a multi byte message (e.g. streaming data) will
 * have the same sequence number. We use this to correctly identify the message
 * start and ending of a multibyte message (streaming data from pointing device)
 */
static void trackpad_decode(sppp_rx_t *packet)
{
	static uint8_t ps2_byte[3];
	static uint8_t ps2_seq;
	static int     ps2_count;
	int i;


	/* note: a PS/2 packet can have incomplete and/or multiple messages */
	for (i = 0; (i+1) < packet->pos; i += 2) {
		if (packet->input[i] != ps2_seq) {
			ps2_seq = packet->input[i];
			ps2_count = 0;
		}

		ps2_byte[ps2_count++] = packet->input[i+1];

		if (ps2_count == 3) {
			/* Check for overflow, and discard if so */
			if (!((ps2_byte[0]&0x80) || (ps2_byte[0]&0x40))) {

				/* Actually send ps2_byte to input dev */
				input_report_key(track_dev, BTN_LEFT,   ps2_byte[0] & 0x01);
				input_report_key(track_dev, BTN_RIGHT,  ps2_byte[0] & 0x02);

				input_report_rel(track_dev, REL_X, ps2_byte[1] ? (int) ps2_byte[1] - (int) ((ps2_byte[0] << 4) & 0x100) : 0);
				input_report_rel(track_dev, REL_Y, ps2_byte[2] ? (int) ((ps2_byte[0] << 3) & 0x100) - (int) ps2_byte[2] : 0);

				input_sync(track_dev);
			}
			ps2_count = 0;
		}
	}
}

static int __init sppp_trackpad_init(void)
{
	int error;
	sppp_tx_t sppp_tx_g;

	track_dev = input_allocate_device();

	track_dev->name = "Efika SB Trackpad";
	track_dev->phys = "efikasb/input1";
	track_dev->uniq = "efikasb_trackpad";
	track_dev->id.bustype = BUS_I8042;
	track_dev->id.vendor  = 0x0002;
	track_dev->id.product = 1;
	track_dev->id.version = 0;

	track_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	track_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT);
	track_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);

	error = input_register_device(track_dev);
	if (error) {
		printk(KERN_ERR "Keyboard input device registration failed.\n");
		input_free_device(track_dev);
		return -1;
	}

	sppp_trackpad_client.id = TRACKPAD;
	sppp_trackpad_client.decode = trackpad_decode;

	/* Low Level PS/2 trackpad init */
	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0xFF); /* reset */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0xF3); /* going to set sample rate */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0x50);  /* sample rate */
	/* Valid sample rates are 10, 20, 40, 60, 80, 100, and 200 samples/sec */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0xE6);  /* Scaling 2:1; 1:1 = 0xE6 */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0xE8);  /* Going to set resolution */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	/* 00,01,02,03 -> 1, 2, 4, 8 counts/mm */
	sppp_data(&sppp_tx_g, 0x03); /* set resolution */
	sppp_stop(&sppp_tx_g);

	sppp_start(&sppp_tx_g, SPPP_PS2_ID);
	sppp_data(&sppp_tx_g, 0xF4); /* enable data reporting */
	sppp_stop(&sppp_tx_g);

	sppp_client_register(&sppp_trackpad_client);

	return 0;
}

static void __exit sppp_trackpad_exit(void)
{
	sppp_client_remove(&sppp_trackpad_client);
}

module_init(sppp_trackpad_init);
module_exit(sppp_trackpad_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP trackpad client");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

