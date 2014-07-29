#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#include <linux/sppp.h>
#include "sppp_priv.h"

#define MAX_ACTIVE_MSGS (2*MAX_PKG_SIZE)
static DEFINE_KFIFO(sppp_active_msgs, uint8_t, MAX_ACTIVE_MSGS);
static DEFINE_SPINLOCK(sppp_active_msgs_spinlock);
static struct workqueue_struct *msgs_wqueue;
static struct delayed_work msgs_work;

/*
 * sppp_msgs - code for handling concurrent SPPP messages from
 *             multiple sources
 */

/* helper functions to track length of messages */

static inline void enqueue_len_for_active_msg(const int msg_len)
{
	uint8_t tmp;

	tmp = (uint8_t)msg_len;
	kfifo_put(&sppp_active_msgs, &tmp);

	tmp = (uint8_t)(msg_len >> 8);
	kfifo_put(&sppp_active_msgs, &tmp);
}

static inline int dequeue_len_for_active_msg(int *msg_len)
{
	uint8_t tmp = 0;
	int ret;
	
	ret = kfifo_get(&sppp_active_msgs, &tmp);
	*msg_len = tmp;

	ret &= kfifo_get(&sppp_active_msgs, &tmp);
	*msg_len |= ((uint16_t)tmp) << 8;

	return ret;
}

static void sppp_msgs_hard_reset(sppp_tx_t *sppp_tx)
{
	unsigned long flags;

	spin_lock_irqsave(&sppp_active_msgs_spinlock, flags);

	kfifo_reset(&sppp_active_msgs);
	sppp_stop(sppp_tx); /* whatever we were doing, stop */

	spin_unlock_irqrestore(&sppp_active_msgs_spinlock, flags);	
}

/* function to send periodically the accumulated messages to STM */
void sppp_msgs_work(struct work_struct *work)
{
	const int interval = 1 * HZ;
	uint8_t tmp = 0;
	int msg_len = 0;
	sppp_tx_t sppp_tx;

/* boilerplate */
#define KFIFO_GET_GUARDED(kfifo_get_exp) \
if ((kfifo_get_exp) != 1) { \
	printk(KERN_ERR "fatal error in SPPP TX\n"); \
	sppp_msgs_hard_reset(&sppp_tx); \
	goto out; \
}

#define KFIFO_GET_MSG() \
KFIFO_GET_GUARDED(kfifo_get(&sppp_active_msgs, &tmp)); \
--msg_len;

	while (kfifo_len(&sppp_active_msgs) > 0) {
		KFIFO_GET_GUARDED(dequeue_len_for_active_msg(&msg_len));

		KFIFO_GET_MSG();
		sppp_start(&sppp_tx, tmp);

		while (msg_len > 0) {
			KFIFO_GET_MSG();
			sppp_data(&sppp_tx, tmp);
		}

		sppp_stop(&sppp_tx);
	}

out:
	queue_delayed_work(msgs_wqueue, &msgs_work, interval);
}

/* enqueue a complete message for the STM without interruption */
void sppp_msgs_set_active(struct kfifo *cl_fifo)
{
	unsigned long flags;
	const int cl_len = kfifo_len(cl_fifo);
	uint8_t tmp;

	spin_lock_irqsave(&sppp_active_msgs_spinlock, flags);

	/* TODO magic number */
	if (kfifo_len(&sppp_active_msgs) + cl_len + 2 >
	    MAX_ACTIVE_MSGS) {
		printk(KERN_ERR "too much traffic to STM!");
		goto out;
	}

	enqueue_len_for_active_msg(cl_len);
	while (kfifo_get(cl_fifo, &tmp)) {
		kfifo_put(&sppp_active_msgs, &tmp);
	}

out:
	spin_unlock_irqrestore(&sppp_active_msgs_spinlock, flags);
}

int sppp_priv_init()
{
	int retval = 0;

	INIT_DELAYED_WORK(&msgs_work, sppp_msgs_work);
	msgs_wqueue = create_singlethread_workqueue("sppp_core_driver");
	if (!msgs_wqueue) {
		retval = -ESRCH;
		goto workqueue_failed;
	}
	queue_delayed_work(msgs_wqueue, &msgs_work, 1 * HZ);
	return 0;

workqueue_failed:
	printk(KERN_ERR "%s unable to create workqueue for STM messages",
	       __func__);
	return retval;
}

void sppp_priv_exit()
{
	cancel_delayed_work_sync(&msgs_work);
	destroy_workqueue(msgs_wqueue);
}
