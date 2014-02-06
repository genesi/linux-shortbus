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

#include <linux/sppp.h>

#define VERBOSE		1	/* show some (not all) debug */
#define KBD_SYNC	0xAA	/* keyboard inserts a SYNC after each full matrix scan */
#define MAXROW		8
#define MAXCOL		16

static struct input_dev *keyb_dev;

static struct sppp_client sppp_keyboard_client;

char keyboard_g[MAXCOL][MAXROW];	/* raw keys from keyboard */
char keypress_g[MAXCOL][MAXROW];	/* current keys reported to the system */
char keyvalid_g[MAXCOL][MAXROW] = {	/* valid key and scancodes on current keyboard */
#if 0
	{ 66, 12, 82, 83,  0,  8,  0, 52 },          /* COL 0 a '0' indicates none existing==invalid */
	{  0,  0,  0, 25,  0,  0,  0, 13 },          /* COL 1 */
	{  0,  0,  9,  0,  0,  2,  0,  0 },
	{  1, 32, 67, 54, 39, 14,  0, 26 },
	{  0,  0,  0,  0,  7,  0,  0,  0 },
	{  0,  0, 73, 61, 62, 34, 20, 47 },
	{  0,  0, 74, 75, 63, 35, 21, 48 },
	{ 31, 18, 72, 60, 46, 33, 19, 59 },
	{  0, 24, 78, 79, 65, 37, 23, 50 },
	{  0,  0,  0,  0,  0,  0,  3,  0 },
	{ 71, 45, 70, 58, 43, 30, 17, 44 },
	{  0,  0, 69, 56, 57, 29, 16, 42 },
	{  0, 11, 80, 81, 53, 38,  0, 51 },
	{  4,  6,  0,  0,  0,  0,  0,  0 },
	{  5, 10, 76, 77, 64, 36, 22, 49 },          /* COL 14 */
	{ 15,  0, 68, 55, 40, 27, 28, 41 }           /* COL 15 */
#else

	/* some KEY_xxx use fancy names. Pls. check and correct. Some keys are missing using a KEY_NUxxx (xxx=index from datasheet) */

#define KEY_NU14  0
#define KEY_NU29  0
#define KEY_NU56  0
#define KEY_NU129 0

#define KEY_WINLEFT  0
#define KEY_WINRIGHT  0
#define KEY_FUNCTION  0
#define KEY_PRINTSRC  0
#define KEY_P1R  0
#define KEY_L1R  0
#define KEY_L2R  0

#define KEY_RETURN1U  0
#define KEY_01R  0
#define KEY_OBENRECHT  0
#define KEY_COM  0
#define KEY_NK28  0
#define KEY_NU45  0
#define KEY_GROSKLEIN  0

	{  KEY_PAUSE,  KEY_WINLEFT,  KEY_WINRIGHT,            0,  KEY_RIGHTCTRL,            0,  KEY_LEFTCTRL,  KEY_F5       }, /* COL 0 */
	{          0,            0,  KEY_FUNCTION,  KEY_LEFTALT,              0,            0,  KEY_RIGHTALT,  KEY_PRINTSRC }, /* COL 1 */
	{      KEY_P,      KEY_P1R,       KEY_L1R,      KEY_L2R,   KEY_RETURN1U,  KEY_UNKNOWN,       KEY_01R,  KEY_0        },
	{  KEY_NU14,KEY_BACKSPACE,       KEY_NU29, KEY_VOLUMEUP,      KEY_ENTER,      KEY_F12,        KEY_F9,KEY_VOLUMEDOWN },
	{          0,KEY_LEFTSHIFT,KEY_RIGHTSHIFT,            0,              0,            0,             0,  0            },
	{          0,            0,              0,   KEY_SPACE,              0,     KEY_DOWN, KEY_OBENRECHT,  0            }, /* COL 5 */
	{          0,            0,              0,           0,              0,    KEY_RIGHT,    KEY_INSERT,  0            },
	{          0,            0,              0,           0,              0,            0,             0,  0            },
	{          0,            0,              0,      KEY_UP,              0,     KEY_LEFT,       KEY_COM,  0            },
	{      KEY_O,       KEY_F7,          KEY_L,           0,        KEY_DOT,    KEY_NU129,        KEY_F9,  KEY_9        },
	{      KEY_I,     KEY_NK28,          KEY_K,      KEY_F6,      KEY_COMMA,     KEY_NU56,         KEY_2,  KEY_8        },
	{      KEY_U,        KEY_Y,          KEY_J,       KEY_H,          KEY_M,        KEY_N,         KEY_6,  KEY_7        },
	{      KEY_R,        KEY_T,          KEY_F,       KEY_G,          KEY_V,        KEY_B,         KEY_5,  KEY_4        },
	{      KEY_E,       KEY_F3,          KEY_D,      KEY_F4,          KEY_C,            0,        KEY_F2,  KEY_3        },
	{      KEY_W, KEY_CAPSLOCK,          KEY_S,    KEY_NU45,          KEY_X,            0,        KEY_F1,  KEY_2        }, /* COL 14 */
	{      KEY_Q,      KEY_TAB,          KEY_A,     KEY_ESC,          KEY_Z,            0, KEY_GROSKLEIN,  KEY_1        }  /* COL 15 */

#endif
};

int key_sync_g;
int key_ghost_g;

/* Reset 'raw' and 'clean' versions of the key matix */
static void kbd_clear(void)
{
	int i, j;

	for (i = 0; i < MAXCOL ; i++)
		for (j = 0; j < MAXROW; j++) {
			keyboard_g[i][j] = 0;
			keypress_g[i][j] = 0;
		}
}

/* Key ghosting is checked by looking for a rectangle of four keys. Due too
 * the HW limits the diagonal key pair can NOT be diferenciated from real
 * keypress or being a ghost key. This filter will remove any of those
 * key combinations.
 */
static int kbd_is_ghost(int col, int row)	/* check for ghosting */
{
	int i, j;

	for (j = 0; j < MAXROW; j++) {	/* the 'ghosting' rectangle must have one more at same row */
		if (j == row)			/* don't test ourself */
			continue;
		if (keyboard_g[col][j] != 0) {			/* we are at col/row found second key at col/j */
			for (i = 0; i < MAXCOL; i++) {		/* now walk down each col */
				if (i == col)			/* don't test ourself */
					continue;
				if (keyboard_g[i][row] != 0 &&		/* we found a rectangle with */
				    keyboard_g[i][j] != 0) {		/* col/row , col/j , i/j , i/row */
#if 1
					if (keyvalid_g[col][j] != 0 && /* complex check tests other three */
					    keyvalid_g[i][j] != 0 &&  /* corners to be a valid key */
					    keyvalid_g[i][row] != 0)
#endif
						return 1;	/* simple check returns true here */
				}
			}
		}
	}
	return 0;
}


/* keyboard handler
 * Takes the scancode and make/break value and passes it on to
 * Linux input
 */
static void key_do(unsigned char scancode)
{
	int makebreak;

	makebreak = scancode & 0x80 ? 0 : 1;
	scancode  = scancode & 0x7f;	/* NOTE: scancode is limited to 128 entries here */

#if VERBOSE
	printk(KERN_ERR "%s: scancode %d (0x%02x)", makebreak==1 ? "pressed " : "released", scancode, scancode);
#endif

	input_report_key(keyb_dev, scancode, makebreak);
	input_sync(keyb_dev);
}

/* Due too HW limitations (no diodes at col/row connections) the 'raw' data
 * show ghost/phantom keys, a special 'find key rectangle' algorithm is
 * used to remove those keys. A clean version of the data is created at
 * keypress_g[][] and coresponding scancodes with make/break bit at pos 7
 * are send to a keyboard handler/driver
 */
static void kbd_handle(void)
{
	int i, j;

	for (i = 0; i < MAXCOL; i++) {
		for (j = 0 ; j < MAXROW; j++) {
			if (keyboard_g[i][j] == 0) {				/* mark key up */
				if (keyvalid_g[i][j] != 0 &&			/* a valid key changed its state */
				    keypress_g[i][j] == 1) {
					key_do(keyvalid_g[i][j]|0x80);		/* send break code to handler */
				}
				keypress_g[i][j] = 0;		/* key is up */
			} else {
				if (keyvalid_g[i][j] != 0) {
					if (!kbd_is_ghost(i, j)) {
						if (keyvalid_g[i][j] != 0 &&		/* a valid key changed its state */
						    keypress_g[i][j] == 0) {
							key_do(keyvalid_g[i][j]);	/* send break code to handler */
						}
						keypress_g[i][j] = 1;		/* key is down */
					} else {
						key_ghost_g++;		/* statistics only */
					}
				}
			}
		}
	}
}

/* Insert delta changes to our 'raw' copy of the matrix. The 'raw' data
 * records all delta keys and is a copy of the physical keyboard matix.
 */
static void kbd_add_col(int col, int key)
{
	int j;

	if (!(col < MAXCOL))		/* sanity check */
		return;

	for (j = 0; j < MAXROW; j++) {
		keyboard_g[col][j] = key&1 ? 0 : 1;		/* note: keyboard reports active lo signals */
		key >>= 1;
	}
}

/* Take the payload of the keyboard event packet and do some meanfull things.
 * The data comes as 2 bytes pair with first byte=column index (0-15) and
 * second byte as the row read out value (note: this is active lo bitfield).
 * The data comes as 'endless' stream with a SYNC byte inserted after each
 * full scan of the key matix. The keyboard ONLY reports the key deltas
 * between two consecutive scans.
 */
static void kbd_decode(sppp_rx_t *packet)
{
	unsigned char *keys = packet->input;
	int keyc = packet->pos;
#if VERBOSE
	int i;

	printk(KERN_ERR "decode %d bytes (", keyc);
	for (i = 0; i < keyc; i++)
		printk(KERN_ERR" 0x%02x", keys[i]);
	printk(KERN_ERR " )\n");
#endif

	while (keyc > 1) {			/* two or more bytes left */
		if (keys[0] == KBD_SYNC) {	/* found SYNC, this also indicates next KBD event */
			key_sync_g++;		/* statistics only */
			kbd_handle();		/* handle keys of this event */
#if VERBOSE
			printk(KERN_ERR "event %d\n", key_sync_g);
#endif
		} else {
			kbd_add_col(keys[0], keys[1]);	/* insert changes into raw matrix */
		}
		keyc -= 2;
		keys += 2;
	}
}

#define PCI_VENDOR_ID_FREESCALE     0x1957

static int __init sppp_kbd_init(void)
{
	int i, j;
	int error;

	keyb_dev = input_allocate_device();

	keyb_dev->name = "Efika SB Keyboard";
	keyb_dev->phys = "efikasb/input0";
	keyb_dev->uniq = "efikasb_keyboard";
	keyb_dev->id.bustype = BUS_HOST;
	keyb_dev->id.vendor  = PCI_VENDOR_ID_FREESCALE;

	keyb_dev->evbit[0] = BIT_MASK(EV_KEY);

	for (i = 0; i < MAXCOL; i++)
		for (j = 0; j < MAXROW; j++)
			set_bit(keyvalid_g[i][j], keyb_dev->keybit);

	error = input_register_device(keyb_dev);
	if (error) {
		printk(KERN_ERR "Keyboard input device registration failed.\n");
		input_free_device(keyb_dev);
		return -1;
	}

	kbd_clear();

	sppp_keyboard_client.id = KEYBOARD;
	sppp_keyboard_client.decode = kbd_decode;

	sppp_client_register(&sppp_keyboard_client);

	printk(KERN_ERR "Keyboard input device registration done.\n");
	return 0;
}

static void __exit sppp_kbd_exit(void)
{
	sppp_client_remove(&sppp_keyboard_client);
}

module_init(sppp_kbd_init);
module_exit(sppp_kbd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP keyboard client");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

