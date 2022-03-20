/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Raw/Src/httpd_cg_ssi.c
  * @author  MCD Application Team
  * @brief   Webserver SSI and CGI handlers
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
#include "lwip/debug.h"
#include "lwip/tcp.h"
#include "lwip/apps/http_client.h"
#include "httpc_loader.h"

#include <string.h>
#include <stdlib.h>

#define MAX_HOST_LEN 64
#define MAX_URI_LEN 128
#define HTTP_PREFIX "http://"
#define HTTPS_PREFIX "https://"

int get_host_and_uri(char *url, char *host, char *uri)
{
	int h_len = 0, u_len = 0, url_idx = 0;
	int url_len = strlen(url);
	if(!url_len || !host || !uri){
		return -1;
	}
	
	if(strncmp(HTTPS_PREFIX, url, strlen(HTTPS_PREFIX)) == 0){
		return -2;
	}
	
	if(strncmp(HTTP_PREFIX, url, strlen(HTTP_PREFIX)) == 0){
		url_idx += strlen(HTTP_PREFIX);
	}
	
	if(strchr(&url[url_idx], ':') != NULL){
		return -3;
	}
	
	while(url_idx < url_len){
		if(u_len >= MAX_URI_LEN || h_len >= MAX_HOST_LEN){
			return -4;
		}
		
		if(u_len > 0 || url[url_idx] == '/'){
			uri[u_len] = url[url_idx];
			u_len++;
			url_idx++;
			continue;
		}
		
		host[h_len] = url[url_idx];
		h_len++;
		url_idx++;

	}
	
	uri[u_len] = 0;
	host[h_len] = 0;
	return 0;
}

int total_len = 0;
int recved_len = 0;
uint32_t start_time = 0;
/**
 * @ingroup httpc 
 * Prototype of a http client callback function
 *
 * @param arg argument specified when initiating the request
 * @param httpc_result result of the http transfer (see enum httpc_result_t)
 * @param rx_content_len number of bytes received (without headers)
 * @param srv_res this contains the http status code received (if any)
 * @param err an error returned by internal lwip functions, can help to specify
 *            the source of the error but must not necessarily be != ERR_OK
 */
void httpc_result(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err){
	printf("http finish, conn result=%d, http_code=%d, rx_content_len=%d,time=%dms\r\n", httpc_result, srv_res, rx_content_len, HAL_GetTick() - start_time);
}

/**
 * @ingroup httpc 
 * Prototype of http client callback: called when the headers are received
 *
 * @param connection http client connection
 * @param arg argument specified when initiating the request
 * @param hdr header pbuf(s) (may contain data also)
 * @param hdr_len length of the heders in 'hdr'
 * @param content_len content length as received in the headers (-1 if not received)
 * @return if != ERR_OK is returned, the connection is aborted
 */
err_t httpc_headers_done(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len){
	printf("header recved, content len=%d\r\n", content_len);
	total_len = content_len;
	start_time = HAL_GetTick();
	return ERR_OK;
}

err_t data_recv_fn(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err){
    if(!p){
	printf("connection closed\r\n");
        altcp_close(conn);
    }else{
        printf("recv len=%d\r\n", p->tot_len);
	recved_len += p->tot_len;
	altcp_recved(conn, p->tot_len);
	printf("recv len=%d, %d/%d\r\n", p->tot_len, recved_len, total_len);
	pbuf_free(p);
    }

    return ERR_OK;
}

httpc_connection_t conn_settings;
httpc_state_t *conn_state;

err_t start_httpc_loader(char* host, char* uri){
    err_t err = ERR_OK;
    conn_settings.use_proxy = 0;
    conn_settings.headers_done_fn = httpc_headers_done;
    conn_settings.result_fn = httpc_result;
    err = httpc_get_file_dns(host, 80, uri, &conn_settings, data_recv_fn, NULL, &conn_state);
    return err;
}
char host[MAX_HOST_LEN] = {0};
char uri[MAX_URI_LEN] = {0};
void httpc_loader_create(char * url)
{
    int ret = 0;
    memset(host, 0, MAX_HOST_LEN);
    memset(uri, 0, MAX_URI_LEN);
    ret = get_host_and_uri(url, host, uri);
    if(ret < 0 || !strlen(host) || !strlen(uri)){
        printf("get host and url error,url=%s\r\n", url);
        return;
    }
    printf("start httpc host=%s, uri=%s\r\n", host, uri);
    ret = start_httpc_loader( host, uri);
    printf("start httpc loader ret = %d", ret);
}


