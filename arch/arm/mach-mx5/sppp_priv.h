#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <linux/sppp.h>

void sppp_msgs_work(struct work_struct *);

void sppp_msgs_set_active(struct kfifo *);

int sppp_priv_init(void);
void sppp_priv_exit(void);
