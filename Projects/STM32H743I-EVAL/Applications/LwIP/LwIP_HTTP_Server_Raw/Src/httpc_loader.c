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
#define ACCEPT_RANGES "Accept-Ranges"

typedef struct loader_mgt{
	uint8_t is_running;
	uint32_t worker_cnt;
}loader_mgt_t;

typedef struct loader_worker{
	httpc_connection_t conn_settings;
	httpc_state_t *conn_state;
	char host[MAX_HOST_LEN];
    char uri[MAX_URI_LEN];
	uint32_t total_len;
    uint32_t recved_len;
    uint32_t start_time;
	uint32_t conn_start_time;
	uint32_t conn_id;
	uint32_t worker_id;
}loader_worker_t;

void worker_restart(loader_worker_t *worker);

loader_mgt_t g_mgr = {0};
loader_worker_t g_worker = {0};

loader_mgt_t *get_mgr_singleton(){
	return &g_mgr;
}

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

uint32_t time_span(uint32_t start){
	return sys_now() - start;
}

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
void on_httpc_finish(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err){
	loader_worker_t *worker = (loader_worker_t *)arg;
	printf("a conn finished, result=%d, http_code=%d, rx_content_len=%d,time=%dms\r\n", httpc_result, srv_res, rx_content_len, time_span(worker->conn_start_time));
    if(worker->total_len > 0 && worker->recved_len >= worker->total_len){
		printf("download finished, total_len=%uByte, spent=%ums\r\n", worker->total_len, time_span(worker->start_time));
		get_mgr_singleton()->is_running = 0;
		start_download("http://commonuser-1256223703.cos.ap-beijing.myqcloud.com/bs/mtlun001/eee.mp4");
	}else{
		sys_timeout(3000, worker_restart, worker);
	}
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
err_t on_httpc_headers(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len){
	loader_worker_t *worker = (loader_worker_t *)arg;
	printf("header recved, content len=%d\r\n", content_len);
	if(worker->total_len == 0 && content_len > 0){
		worker->total_len = content_len;
	}
	
	worker->conn_start_time = sys_now();
	return ERR_OK;
}

err_t on_httpc_data(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err){
	loader_worker_t *worker = (loader_worker_t *)arg;
    if(!p){
	    printf("peer conn closed\r\n");
        altcp_close(conn);
    }else{
        //printf("recv len=%d\r\n", p->tot_len);
        worker->recved_len += p->tot_len;
        altcp_recved(conn, p->tot_len);
        printf("[%u-%u]:%u->%u/%u\r\n", worker->worker_id, worker->conn_id, p->tot_len, worker->recved_len, worker->total_len);
        pbuf_free(p);
    }

    return ERR_OK;
}

loader_worker_t *worker_new(){
	//currently only support one worker
	memset(&g_worker, 0, sizeof(loader_worker_t));
	return &g_worker;
}
void worker_free(loader_worker_t *worker){
	memset(worker, 0, sizeof(loader_worker_t));
}

err_t worker_start(loader_worker_t *worker){
    err_t err = ERR_OK;
    worker->conn_settings.use_proxy = 0;
    worker->conn_settings.headers_done_fn = on_httpc_headers;
    worker->conn_settings.result_fn = on_httpc_finish;
    worker->start_time = sys_now();
	
    err = httpc_get_file_dns(worker->host, 80, worker->uri, &worker->conn_settings, on_httpc_data, worker, &worker->conn_state);
    return err;
}

void worker_restart(loader_worker_t *worker){
	err_t err = ERR_OK;
	worker->conn_settings.start_pos = worker->recved_len;
	worker->conn_id++;
	
	err = httpc_get_file_dns(worker->host, 80, worker->uri, &worker->conn_settings, on_httpc_data, worker, &worker->conn_state);
	printf("worker_restart result=%d\r\n", err);

	if(err != ERR_OK){
	    sys_timeout(5000, worker_restart, worker);
	}
}

int start_download(char * url)
{
    int ret = 0;
	
	loader_mgt_t * m= get_mgr_singleton();
	if(m->is_running){
		printf("request canceled, a task is still running\r\n");
		return -1;
	}
	
	loader_worker_t *worker = worker_new();
    ret = get_host_and_uri(url, worker->host, worker->uri);
    if(ret < 0 || !strlen(worker->host) || !strlen(worker->uri)){
        printf("get host and url error,url=%s\r\n", url);
		worker_free(worker);
        return -2;
    }
    m->worker_cnt++;
    worker->worker_id = m->worker_cnt;
    printf("extract info in url: host=%s, uri=%s\r\n", worker->host, worker->uri);
    ret = worker_start(worker);
	if(ret != ERR_OK){
		printf("start worker error = %d\r\n", ret);
		worker_free(worker);
        return -3;
	}
    
	m->is_running = 1;
	return 0;
}


