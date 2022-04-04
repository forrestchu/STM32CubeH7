/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Raw/Inc/http_cgi_ssi.h 
  * @author  MCD Application Team
  * @brief   Header for http_cgi_ssi.c module
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
#ifndef __HTTPC_LOADER_H
#define __HTTPC_LOADER_H

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct download_stat{
    uint32_t total_len;
    uint32_t recved_len;
    uint32_t speed;
}download_stat_t;
/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/   
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
int download_start(char * url, int enable_md5);//start download url, return 0 if success
int download_stat(download_stat_t *stat);
int download_stop(void);
#ifdef __cplusplus
}
#endif

#endif /* __HTTPC_LOADER_H */




