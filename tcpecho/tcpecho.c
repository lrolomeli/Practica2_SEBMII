/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#include "tcpecho.h"
#include "udpecho/udpecho.h"
#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/sys.h"
#include "lwip/api.h"
#include "fsl_pit.h"

enum{
	STOP_DAC = 0, RUN_DAC
};

typedef struct state
{
	void (*ptr)(void); /**pointer to function*/
} State;


volatile static unsigned char port_flag;
static unsigned char characteristics;
unsigned char perc_quality[3];
unsigned char statisticsflag;

void waitfunc(void);
void play_stop(void);
void select_audio(void);
void digiToAscii(unsigned char data);
void statistics(void);

static const State menu_state[4] = {
		{waitfunc},
		{play_stop},
		{select_audio},
		{statistics} };


void waitfunc(void)
{

}

void play_stop()
{
	static unsigned char dac_run = 0;
	if (RUN_DAC == dac_run)
	{
		PIT_StopTimer(PIT, kPIT_Chnl_0);
		dac_run = STOP_DAC;
	}
	else
	{
		PIT_StartTimer(PIT, kPIT_Chnl_0);
		dac_run = RUN_DAC;
	}
}

void digiToAscii(unsigned char data)
{
	if (data >= 10 && data <= 99)
	{

		perc_quality[2] = (data / 10) + '0';
		perc_quality[1] = ((data % 10)) + '0';
		perc_quality[0] = '%';
	}

	else if (data >= 0 && data <= 9)
	{
		perc_quality[2] = '0';
		perc_quality[1] = data + '0';
		perc_quality[0] = '%';
	}
}

void select_audio(void)
{
	port_flag = 1;
}


void statistics(void)
{
	statisticsflag = 1;
	characteristics = get_quality() + '0';
	digiToAscii(characteristics);
}

static void tcpecho_thread(void *arg)
{
	struct netconn *conn, *newconn;
	err_t err;
	LWIP_UNUSED_ARG(arg);

	/* Create a new connection identifier. */
	/* Bind connection to well known port number 7. */
#if LWIP_IPV6
	conn = netconn_new(NETCONN_TCP_IPV6);
	netconn_bind(conn, IP6_ADDR_ANY, 7);
#else /* LWIP_IPV6 */
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, IP_ADDR_ANY, 50002);
#endif /* LWIP_IPV6 */
	LWIP_ERROR("tcpecho: invalid conn", (conn != NULL), return;);

	/* Tell connection to go into listening mode. */
	netconn_listen(conn);
	while (1)
	{

		/* Grab new connection. */
		err = netconn_accept(conn, &newconn);
		/*printf("accepted new connection %p\n", newconn);*/
		/* Process the new connection. */
		if (err == ERR_OK)
		{
			struct netbuf *buf;
			unsigned char *data;
			static unsigned char menu[45] = "1) play/stop\n"
					"2) select audio\n"
					"3) statistics\n";

			static unsigned char no_menu;
			u16_t len;

			while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
			{
				/*printf("Recved\n");*/
				do
				{
					netbuf_data(buf, (void*) &data, &len);
					no_menu = *data;
					no_menu = no_menu - '0';

					menu_state[no_menu].ptr();
					if (statisticsflag)
					{
						statisticsflag = 0;
						err = netconn_write(newconn, (void *) perc_quality, 3, NETCONN_COPY);
					}
					err = netconn_write(newconn, menu, 45, NETCONN_COPY);
#if 0
					if (err != ERR_OK)
					{
						printf("tcpecho: netconn_write: error \"%s\"\n", lwip_strerr(err));
					}
#endif
				} while (netbuf_next(buf) >= 0);
				netbuf_delete(buf);
			}
			/*printf("Got EOF, looping\n");*/
			/* Close connection and discard connection identifier. */
			netconn_close(newconn);
			netconn_delete(newconn);
		}
	}
}

void tcpecho_init(void)
{
	sys_thread_new("tcpecho_thread", tcpecho_thread, NULL, 200, 1);
}

unsigned char  get_port_flag(void)
{
	return port_flag;
}

void set_port_flag(void)
{
	port_flag = 0;
}


#endif /* LWIP_NETCONN */
