/**
  ******************************************************************************
  * @file    LwIP/LwIP_UDP_Echo_Client/Inc/udp_echoclient.h
  * @author  MCD Application Team
  * @brief   Header file for udp_echoclient.c
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

/* Define to prevent recursive inclusion -------------------------------------*/  
#ifndef __UDP_CONSOLE_H__
#define __UDP_CONSOLE_H__
#include <stdio.h>
#include <stdarg.h>
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/def.h"
/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void udp_console_init(void);
err_t udp_console_send(char *data, int len);
#endif /* __UDP_CONSOLE_H__ */



