#ifndef _SPPP_H_
#define _SPPP_H_

#define MAX_RECV_PKG_SIZE 100

#define MAX_PKG_SIZE  100
#define MAX_LOOP_COUNT  MAX_RECV_PKG_SIZE

#define SPPP_PKT_ID_MASK 0x3F
#define SPPP_PKT_START   0x80
#define SPPP_PKT_STOP    0xC0
#define ISBITSET(val, bit) ((val) & (1 << (bit)))

#define SPPP_SHORT_ID   0
#define SPPP_KEY_ID     1
#define SPPP_PS2_ID     2
#define SPPP_RTC_ID     3
#define SPPP_PWR_ID     4
#define SPPP_STRING_ID  60
#define SPPP_BINARY_ID  61

#define SPPP_NOSYNC 0x00
#define SPPP_SYNC   0xff

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

int sppp_recv(int comd, sppp_rx_t *sppp_rx);
void sppp_start(sppp_tx_t *sppp_tx, uint8_t pkg_id);
void sppp_data(sppp_tx_t *sppp_tx, uint8_t data);
void sppp_stop(sppp_tx_t *sppp_tx);

void sppp_send(sppp_tx_t *sppp_tx, unsigned char *buf, int size, int pkg_id);

#endif /* _SPPP_H_ */
