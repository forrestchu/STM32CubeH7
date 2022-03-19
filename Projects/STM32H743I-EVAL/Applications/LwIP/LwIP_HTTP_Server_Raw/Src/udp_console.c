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
#include <string.h>
#include <stdio.h>
#include "udp_console.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

u8_t   data[100];
__IO uint32_t message_count = 0;
struct udp_pcb *upcb = NULL;


/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Connect to UDP echo server
  * @param  None
  * @retval None
  */
void udp_console_connect(void)
{
  ip_addr_t DestIPaddr;
  err_t err;
  
  /* Create a new UDP control block  */
  upcb = udp_new();
  
  if (upcb!=NULL)
  {
    /*assign destination IP address */
    IP4_ADDR( &DestIPaddr, DEST_IP_ADDR0, DEST_IP_ADDR1, DEST_IP_ADDR2, DEST_IP_ADDR3 );
    
    /* configure destination IP address and port */
    err= udp_connect(upcb, &DestIPaddr, UDP_SERVER_PORT);
    
    if (err == ERR_OK)
    {
      /* Set a receive callback for the upcb */
      udp_recv(upcb, udp_receive_callback, NULL);  
    }
  }
}

/**
  * @brief This function is called when an UDP datagrm has been received on the port UDP_PORT.
  * @param arg user supplied argument (udp_pcb.recv_arg)
  * @param pcb the udp_pcb which received data
  * @param p the packet buffer that was received
  * @param addr the remote IP address from which the packet was received
  * @param port the remote port from which the packet was received
  * @retval None
  */
void udp_console_send(void)
{
  struct pbuf *p;
  
  sprintf((char*)data, "sending udp client message %d", (int)message_count);
  
  /* allocate pbuf from pool*/
  p = pbuf_alloc(PBUF_TRANSPORT,strlen((char*)data), PBUF_RAM);
  
  if (p != NULL)
  {
    /* copy data to pbuf */
    pbuf_take(p, (char*)data, strlen((char*)data));
    
    /* send udp data */
    udp_send(upcb, p); 
    
    /* free pbuf */
    pbuf_free(p);
  }
}

/**
  * @brief This function is called when an UDP datagrm has been received on the port UDP_PORT.
  * @param arg user supplied argument (udp_pcb.recv_arg)
  * @param pcb the udp_pcb which received data
  * @param p the packet buffer that was received
  * @param addr the remote IP address from which the packet was received
  * @param port the remote port from which the packet was received
  * @retval None
  */
void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  /*increment message count */
  message_count++;

  udp_sendto(upcb, p, addr, port);
  
  /* Free receive pbuf */
  pbuf_free(p);
}

void udp_console_printf(char * format, ...){
  struct pbuf *p;
  va_list args;
  char mydata[100];
  int n = 0;
  va_start(args,  format);
  n = vsprintf((char *)mydata, format, args);
  va_end(args);

 	
  /* allocate pbuf from pool*/
  p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
  
  if (p != NULL)
  {
    /* copy data to pbuf */
    pbuf_take(p,  mydata, n);
    
    /* send udp data */
    udp_send(upcb, p); 
    
    /* free pbuf */
    pbuf_free(p);
  }
}

