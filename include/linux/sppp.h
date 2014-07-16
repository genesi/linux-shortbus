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


#ifndef _SPPP_H_
#define _SPPP_H_

#define MAX_RECV_PKG_SIZE 1024
#define MAX_PKG_SIZE  MAX_RECV_PKG_SIZE

#define SPPP_PKT_ID_MASK 0x3F
#define SPPP_PKT_START   0x80
#define SPPP_PKT_STOP    0xC0
#define IsBitSet(val, bit) ((val) & (1 << (bit)))

#define SPPP_IDENTIFICATION_ID              0
#define SPPP_KEY_ID                         1
#define SPPP_PS2_ID                         2
#define SPPP_RTC_ID                         3
#define RTC_ID_SET              0
#define RTC_ID_GET              1
#define RTC_ID_SET_ALARM        2
#define RTC_ID_GET_ALARM        3
#define SPPP_BKL_ID                         4
#define SPPP_STATUS_ID                      5
#define STATUS_ID_EVENT           0
#define STATUS_ID_SET             1
#define STATUS_ID_CLEAR           2
#define STATUS_ID_GET             3


#define SPPP_FLASH_WRITE_BUF_ID             6
#define SPPP_FLASH_READ_BUF_ID              7
#define SPPP_FLASH_WRITE_ID                 8
#define SPPP_FLASH_READ_ID                  9

#define SPPP_VBAT_ID                       10


#define SPPP_NOSYNC 0x00
#define SPPP_SYNC   0xff

#define FLASH_FW_START_PAGE   8


/* OLD stuff*/
/*
#define SPPP_PWR_ID                         4
#define PWR_ID_GOTO_STANDY         4
*/
typedef struct {
	uint8_t input[MAX_RECV_PKG_SIZE];
	uint8_t id;
	uint8_t crc;
	uint8_t pos;
	uint8_t num;
	uint8_t carry;
	uint8_t sync;
} sppp_rx_t;

typedef struct {
	uint8_t crc;
	uint8_t pos;
	uint8_t carry;
	int comd;
} sppp_tx_t;

/* Possible SPPP clients */
enum clients {
	KEYBOARD = 0,
	TRACKPAD,
	RTC,
	POWER,
	ANY,
};

/* Each SPPP client has these */
struct sppp_client {
	unsigned int id;
	void (*decode)(const sppp_rx_t *);
};

int sppp_recv(int comd, sppp_rx_t *sppp_rx);
void sppp_start(sppp_tx_t *sppp_tx, uint8_t pkg_id);
void sppp_data(sppp_tx_t *sppp_tx, uint8_t data);
void sppp_stop(sppp_tx_t *sppp_tx);

void sppp_send(sppp_tx_t *sppp_tx, unsigned char *buf, int size, int pkg_id);

void sppp_client_send_start(struct sppp_client *client,
	sppp_tx_t *sppp_tx, uint8_t pkg_id);
void sppp_client_send_data(struct sppp_client *client,
	sppp_tx_t *sppp_tx, uint8_t data);
void sppp_client_send_stop(struct sppp_client *client,
	sppp_tx_t *sppp_tx);

void sppp_client_status_listen(struct sppp_client *client);
void sppp_client_register(struct sppp_client *client);
void sppp_client_remove(struct sppp_client *client);

void dummy_status_requested(void);

#endif /* _SPPP_H_ */
