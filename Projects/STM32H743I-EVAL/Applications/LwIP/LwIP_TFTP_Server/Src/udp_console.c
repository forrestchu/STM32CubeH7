/**
  ******************************************************************************
  * @file    LwIP/LwIP_UDP_Echo_Client/Src/udp_echoclient.c
  * @author  MCD Application Team
  * @brief   UDP echo client
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/def.h"
#include <string.h>
#include <stdio.h>

#include "udp_console.h"
#include "httpc_loader.h"
void on_data(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
static struct udp_pcb *local_pcb = NULL;
static ip_addr_t re_addr = {0};
static u16_t re_port = 0;
void udp_console_init(void)
{
    struct udp_pcb *upcb;
    err_t err;
    
    /* Create a new UDP control block  */
    upcb = udp_new();
    
    if (upcb)
    {
        /* Bind the upcb to the UDP_PORT port */
        /* Using IP_ADDR_ANY allow the upcb to be used by any local interface */
        err = udp_bind(upcb, IP_ADDR_ANY, UDP_SERVER_PORT);
    
        if(err == ERR_OK)
        {
            /* Set a receive callback for the upcb */
            udp_recv(upcb, on_data, NULL);
        }else{
            udp_remove(upcb);
        }
    }
    local_pcb = upcb;
}

#define CMD_START "start "
#define CMD_STAT  "stat"
#define CMD_STOP  "stop"
#define BUFFER_LEN  512

void udp_console_printf(struct udp_pcb *upcb, const ip_addr_t *addr, u16_t port, char * format, ...)
{
    char buf[BUFFER_LEN] = {0};
    struct pbuf *p;
    va_list args;
    int n = 0;
    va_start(args,  format);
    n = vsprintf((char *)buf, format, args);//take care of the length, we have no check here!
    va_end(args);
    
    /* allocate pbuf from pool*/
    p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    
    if (p != NULL)
    {
        /* copy data to pbuf */
        pbuf_take(p,  buf, n);
        udp_sendto(upcb, p, addr, port);
    
        /* free pbuf */
        pbuf_free(p);
    }
}

void on_data(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    int i = 0, j = 0, ret = 0;
    char url[BUFFER_LEN] = {0};
    download_stat_t stat = {0};
    char *data = (char *)p->payload;
    if(strncmp(data, CMD_START, strlen(CMD_START)) == 0){
        i += strlen(CMD_START);
        while(data[i] == ' ') i++;
        
        while(i < p->len){
            url[j++] = data[i++];
            if(j >= BUFFER_LEN){
                printf("error: url toooo long, plz check\r\n");
                break;
            }
        }
        
        ret = download_start(url);
        //printf("start: ret=%d\r\n", ret);
        if(ret == 0){
            re_addr = *addr;
            re_port = port;
            //ret= udp_connect(upcb, addr, port);
            //printf("start: udp_connect ret =%d\r\n", ret);
        }
        udp_console_printf(upcb, addr, port, "start: ret=%d\r\n", ret);
    }else if(strncmp(p->payload, CMD_STAT, strlen(CMD_STAT)) == 0){
        ret = download_stat(&stat);
        if(ret < 0){
            udp_console_printf(upcb, addr, port, "stat: not found or finished, ret=%d\r\n", ret);
            //printf("stat: not found or finished, ret=%d\r\n", ret);
        }else{
            if(stat.total_len == 0){
                udp_console_printf(upcb, addr, port, "stat: not connected\r\n");
                //printf("stat: not connected\r\n");
            }else{
                
                udp_console_printf(upcb, addr, port, "stat: %u/%u, speed=%u B/s\r\n", stat.recved_len, stat.total_len, stat.speed);
                //printf("stat: %u/%u, speed=%u B/s\r\n", stat.recved_len, stat.total_len, stat.speed);
            }
        }
    }else if(strncmp(p->payload, CMD_STOP, strlen(CMD_STOP)) == 0){
        ret = download_stop();
        udp_console_printf(upcb, addr, port, "stop: ret=%d\r\n", ret);
        //printf("stop: ret=%d\r\n", ret);
    }else{
        udp_console_printf(upcb, addr, port, "Commond Unknown!\r\n");
        //printf("Commond Unknown!\r\n");
    }
    
    pbuf_free(p);
}

err_t udp_console_send(char *data, int len){
    struct pbuf *p = NULL;
    err_t ret = ERR_OK;
    if(data == NULL || len <= 0){
        return ERR_ARG;
    }

    p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    
    if (p != NULL)
    {
        /* copy data to pbuf */
        pbuf_take(p,  data, len);
        ret = udp_sendto(local_pcb,  p, &re_addr, re_port); 
    
        /* free pbuf */
        pbuf_free(p);
    }
	
    return ret;
}