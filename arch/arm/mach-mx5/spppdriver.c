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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/serial_core.h>
#include <linux/delay.h>

#include <mach/mx53.h>
#include <mach/hardware.h>

#include "spppdriver.h"
#include <linux/sppp.h>

/* Read and write registers */
#define __REG(x)     (*((volatile u32 *)(x)))

/* Maximum number of SPPP clients */
#define MAX_CLIENTS  10

/* Simple device structure for the SPPP driver */
struct sppp_device {
        struct uart_port port;
        unsigned int baud;
        struct clk *clk;
};

/* Array od SPPP clients */
static struct sppp_client *array_of_clients[MAX_CLIENTS] = {NULL};

/* baseint contains the address as a 32 bit integer */
static volatile u32 *base, baseint;

/* Actual SPPP device */
static struct sppp_device sppp __initdata;

/* Receive structure */
static sppp_rx_t sppp_rx_g;

/* Registers an SPPP client with the driver */
void sppp_client_register(struct sppp_client *client)
{
        if (client->id <= MAX_CLIENTS)
                array_of_clients[client->id] = client;
        else
                printk(KERN_ERR "Wrong client id\n");
}
EXPORT_SYMBOL(sppp_client_register);

/* Removes an SPPP client from the driver */
void sppp_client_remove(struct sppp_client *client)
{
        if (client->id <= MAX_CLIENTS)
                array_of_clients[client->id] = NULL;
        else
                printk(KERN_ERR "Wrong client id\n");
}
EXPORT_SYMBOL(sppp_client_remove);

/* Process, identify and decode packet after full encapulated packet received */
static int decode(void)
{
        int i;

        switch (sppp_rx_g.id) {
        case SPPP_IDENTIFICATION_ID:
                printk(KERN_ERR "IDENTIFICATION\n");
                for (i=0; i<sppp_rx_g.pos; i++)
                        printk(KERN_ERR "%02x",sppp_rx_g.input[i]);
                printk(KERN_ERR "\n");
                break;
        case SPPP_PS2_ID:
                //printk(KERN_ERR "PS2\n");
                //for (i=0; i<sppp_rx_g.pos; i++)
                //        printk(KERN_ERR "%02x",sppp_rx_g.input[i]);
                //printk(KERN_ERR "\n");
                if (array_of_clients[TRACKPAD] != NULL)
                        array_of_clients[TRACKPAD]->decode(&sppp_rx_g);
                break;
        case SPPP_KEY_ID:
                //printk(KERN_ERR "KEY\n");
                //for (i=0; i<sppp_rx_g.pos; i++)
                //        printk(KERN_ERR "%02x",sppp_rx_g.input[i]);
                //printk(KERN_ERR "\n");
                if (array_of_clients[KEYBOARD] != NULL)
                        array_of_clients[KEYBOARD]->decode(&sppp_rx_g);
                break;

        case SPPP_RTC_ID:
                //printk(KERN_ERR "RTC\n");
                //for (i=0; i<sppp_rx_g.pos; i++)
                //        printk(KERN_ERR "%02x",sppp_rx_g.input[i]);
                //printk(KERN_ERR "\n");
                if (array_of_clients[RTC] != NULL)
                        array_of_clients[RTC]->decode(&sppp_rx_g);
                break;
        case SPPP_PWR_ID:
                //printk(KERN_ERR "PWR\n");
                //for (i=0; i<sppp_rx_g.pos; i++)
                //        printk(KERN_ERR "%02x",sppp_rx_g.input[i]);
                //printk(KERN_ERR "\n");
                if (array_of_clients[POWER] != NULL)
                        array_of_clients[POWER]->decode(&sppp_rx_g);
                break;
        case SPPP_STRING_ID:
                printk(KERN_ERR "STRING\n");
                for (i=0; i<sppp_rx_g.pos; i++)
                        printk(KERN_ERR "%c",sppp_rx_g.input[i]);
                printk(KERN_ERR "\n");
                break;
        case 10:
                return 0;
        default:
                break;
        }

        return 0;
}

/* Process incoming encapsulated data */
static void recv(sppp_rx_t *sppp_rx, uint8_t data)
{

        //printk(KERN_ERR "decode byte: 0x%02x\n",data);

        if ( sppp_rx->sync == SPPP_SYNC ) {
                switch (data & ~SPPP_PKT_ID_MASK) {
                case SPPP_PKT_START:
                        goto sppp_start;
                case SPPP_PKT_STOP:
                        sppp_rx->sync = SPPP_NOSYNC; //mark the end of the current packet
                        /* check for valid CRC */
                        if ( !(( sppp_rx->crc^data) & SPPP_PKT_ID_MASK) ) {
                                //printk(KERN_ERR "valid crc\n");
                                /* More processing */
                                decode();
                        } else {
                                printk(KERN_ERR "no valid crc=0x%02x != 0x%02x\n",(sppp_rx->crc& SPPP_PKT_ID_MASK), (data& SPPP_PKT_ID_MASK) );
                        }
                        break;
                default:
                        /* max input size reached */
                        if ( sppp_rx->pos < MAX_RECV_PKG_SIZE) {
                                /* add to CRC sum */
                                sppp_rx->crc += data;
                                if (sppp_rx->num != 0) {
                                        //printk(KERN_ERR "input=%c carry=0x%x\n",sppp_rx->input[sppp_rx->pos], sppp_rx->carry);
                                        /* Shift current byte on right pos */
                                        sppp_rx->input[sppp_rx->pos] = ( (data >>(7-sppp_rx->num)) | sppp_rx->carry);
                                        /* Input size */
                                        sppp_rx->pos++;
                                }
                                /* shift carry byte on right pos for next run */
                                sppp_rx->carry = (  data <<(1+sppp_rx->num));
                                //printk(KERN_ERR "carry=0x%x\n",sppp_rx->carry);
                                sppp_rx->num++;
                                if (sppp_rx->num>7)
                                        sppp_rx->num=0;
                        } else {
                                /* size is to big, drop this one */
                                sppp_rx->sync = SPPP_NOSYNC;
                        }
                }
        } else {
                if ( (data & ~SPPP_PKT_ID_MASK) == SPPP_PKT_START ) {
sppp_start:
                        sppp_rx->id    = data & SPPP_PKT_ID_MASK;
                        sppp_rx->crc   = data;
                        sppp_rx->pos   = 0;
                        sppp_rx->num   = 0;
                        sppp_rx->carry = 0;
                        sppp_rx->sync  = SPPP_SYNC;
                }
        }
}


/* Interrupt service routine for serial interface */
static irqreturn_t sppp_int(int irq, void *dev_id)
{

        uint8_t rx;
        volatile unsigned int sr2, sr1, cr1;

        sr1 = __REG(baseint + USR1);
        sr2 = __REG(baseint + USR2);
        cr1 = __REG(baseint + UCR1);

        /* Clear interrupt bits */
        __REG(baseint + USR1) = sr1;
        __REG(baseint + USR2) = sr2;

        /* Receive interrupt */
        if (sr2 & USR2_RDR) {
                while (__REG(baseint + USR2) & USR2_RDR) {

                        /* Read data from the receive data register and mask out any status bits */
                        rx = (__REG(baseint + URXD) & URXD_RX_DATA);

                        /* Process incoming data */
                        recv(&sppp_rx_g, rx);
                }

        }

        return IRQ_HANDLED;
}

/* Polling send function */
void serial_putc(const char c)
{
        __REG(baseint + UTXD) = c;

        /* wait for transmitter to be ready */
        while (!(__REG(baseint + USR1) & USR1_TRDY));

        /* STM needs time for processing... */
        msleep(1);
}

/* Writing a string, uses polled serial_putc() */
void serial_puts(const char *s)
{
        while (*s) {
                serial_putc (*s++);
        }
}

#define  UFCR_RFDIV_REG(x)      (((x) < 7 ? 6 - (x) : 6) << 7)
#define TXTL 2 /* reset default */
#define RXTL 1 /* reset default */

static int imx_setup_ufcr(struct uart_port *port, unsigned int mode)
{
        unsigned int val;
        unsigned int ufcr_rfdiv;

        /* set receiver / transmitter trigger level.
         * RFDIV is set such way to satisfy requested uartclk value
        	 * 24000000 is the value of the oscillator, see mx53_clocks_init()
         */
        val = TXTL << 10 | RXTL;
        ufcr_rfdiv = (clk_get_rate(clk_get_sys("imx-uart.1", NULL)) + 24000000 / 2)
                     / 24000000;

        if (!ufcr_rfdiv)
                ufcr_rfdiv = 1;

        val |= UFCR_RFDIV_REG(ufcr_rfdiv);

        __REG(baseint + UFCR) = val;

        return 0;
}


/* Initialise the serial port with the baudrate. */
static int sppp_setup(void)
{
        int retval;
        struct sppp_device *device = &sppp;
        struct uart_port *port = &device->port;

        port->mapbase = MX53_UART2_BASE_ADDR; /* UART 2 is connected to the STM */
        port->irq = MX53_INT_UART2; /* Our interrupt */

        /* Currently at 115200 */
        device->baud = 115200;

        /* Get base address of our remapped uart */
        base = (u32 *)ioremap(port->mapbase, 0xD4);

        /* Make it available as an integer for use with __REG(x) macro */
        baseint = (u32)base;

        /* Enable the uart clock */
        clk_enable(clk_get_sys("imx-uart.1", NULL));
        port->uartclk = clk_get_rate(clk_get_sys("imx-uart.1", NULL));

        /* Set clock divider */
        imx_setup_ufcr(port, 0);

        /* Initialise registers to zero, clears any possible flags that shouldn't be set */
        __REG(baseint + UCR1) = 0x0;
        __REG(baseint + UCR2) = 0x0;

        /* Wait for SW reset */
        while (!(__REG(baseint + UCR2) & UCR2_SRST));

        /* disable the DREN bit (Data Ready interrupt enable) before
         * requesting IRQs
         */
        __REG(baseint + UCR4) &= ~UCR4_DREN;

        /* Get the irq */
        retval = request_irq(port->irq, sppp_int, 0, "spppinterrupt", port);
        if (retval != 0) {
                printk(KERN_ERR "Could not get interrupt... \n");
        }

        /* Set Baudrate */
        __REG(baseint + UBIR) = 0xf;
        __REG(baseint + UBMR) = port->uartclk / (2 * device->baud);

        /* And finally enable the port and interrupts in the control registers */
        __REG(baseint + UCR2) = UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST;
        __REG(baseint + UCR1) = UCR1_UARTEN | UCR1_RRDYEN; /* The last one enables receive ready interrupt */

        return 0;
}

/* SPPP send operations */

/* Low Level write */
void _sppp_write(sppp_tx_t *sppp_tx, uint8_t data)
{
        sppp_tx->crc += data;
        serial_putc(data);
}

/* Send Start */
void sppp_start(sppp_tx_t *sppp_tx, uint8_t pkg_id)
{
        sppp_tx->crc=0;
        sppp_tx->pos=0;
        sppp_tx->carry=0;
        _sppp_write(sppp_tx, SPPP_PKT_START | (pkg_id&0x3F)) ;
}
EXPORT_SYMBOL(sppp_start);

/* Encode data for sending */
void sppp_data(sppp_tx_t *sppp_tx, uint8_t data)
{
        _sppp_write(sppp_tx, (data >>(1+sppp_tx->pos)) | sppp_tx->carry );    // send
        sppp_tx->carry = (data <<(6-sppp_tx->pos))&0x7F;      // save carry
        sppp_tx->pos++;
        //printk(KERN_ERR "carry=0x%02x\n",sppp_tx->carry);
        if (sppp_tx->pos>=7) {
                //printk(KERN_ERR "carry=0x%02x\n",sppp_tx->carry);
                _sppp_write(sppp_tx, sppp_tx->carry);     // send this byte
                sppp_tx->carry=0;
                sppp_tx->pos=0;
        }
}
EXPORT_SYMBOL(sppp_data);

/* Send Stop */
void sppp_stop(sppp_tx_t *sppp_tx)
{
        if (sppp_tx->pos < 7)
                _sppp_write(sppp_tx, sppp_tx->carry);
        _sppp_write(sppp_tx, SPPP_PKT_STOP | (~SPPP_PKT_STOP & sppp_tx->crc));    // send EOP
}
EXPORT_SYMBOL(sppp_stop);

/* General all-in-one send function */
void sppp_send(sppp_tx_t *sppp_tx, unsigned char *buf, int size, int pkg_id)
{
        int i;
        sppp_tx->crc=0;
        sppp_tx->pos=0;
        sppp_tx->carry=0;

        printk(KERN_ERR "send SOP\n");
        _sppp_write(sppp_tx, (SPPP_PKT_START | (pkg_id&0x3F)) ) ;  //send SOP and ID
        for (i=0; i<size; i++)
                sppp_data(sppp_tx, buf[i]);
        printk(KERN_ERR "send EOP\n");
        if (sppp_tx->pos < 7)
                _sppp_write(sppp_tx, sppp_tx->carry);   // send only carry if no more bytes send
        _sppp_write(sppp_tx, SPPP_PKT_STOP | (~SPPP_PKT_STOP & sppp_tx->crc));    // send EOP
}
EXPORT_SYMBOL(sppp_send);

static int __init sppp_init(void)
{

        printk(KERN_ALERT "Inserting SPPP driver.\n");
        sppp_setup();
        sppp_rx_g.sync = SPPP_NOSYNC;

        return 0;
}

static void __exit sppp_exit(void)
{
//        struct sppp_device *device = &sppp;
//        struct uart_port *port = &device->port;

        printk(KERN_ALERT "Removing SPPP driver.\n");

        //free_irq(port->irq, NULL); //--> needs fixing, blows up with segfault...
        clk_disable(clk_get_sys("imx-uart.1", NULL));
        iounmap(base);
}


module_init(sppp_init);
module_exit(sppp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP device driver");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

