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
#include "lwip/def.h"
#include "lwip/timeouts.h"
#include "httpc_loader.h"
#include "udp_console.h"
#include "ff.h"
#include <string.h>
#include <stdlib.h>

#define MAX_HOST_LEN 128
#define MAX_URI_LEN 512
#define MAX_NAME_LEN 128
#define MAX_MAX_CACHE_LEN (512 - 6)*1024
#define CACHE_ADDR 0x24000000
//#define MAX_MAX_CACHE_LEN (128-10)*1024
//#define CACHE_ADDR 0x30020000
#define HTTP_PREFIX "http://"
#define HTTPS_PREFIX "https://"
#define ACCEPT_RANGES "Accept-Ranges:"

typedef struct loader_worker{
    httpc_connection_t conn_settings;
    httpc_state_t *conn_state;
    FATFS fs;
    DIR dir;
    FIL fd;
    uint8_t *cache_data;
    char host[MAX_HOST_LEN];
    char uri[MAX_URI_LEN];
    char filename[MAX_NAME_LEN];
    uint32_t total_len;
    uint32_t recved_len;
    uint32_t start_time;
    uint32_t conn_start_time;
    uint32_t conn_id;
    uint32_t cache_len;
    uint8_t worker_id;
    uint8_t support_range;
    uint8_t is_cancelled;
    uint8_t fd_valid;
}loader_worker_t;

typedef struct loader_mgt{
    loader_worker_t *worker;
    uint8_t is_running;
    uint8_t worker_cnt;
}loader_mgt_t;

void worker_restart(loader_worker_t *worker);
void worker_open_file(loader_worker_t *worker);
void worker_close_file(loader_worker_t *worker);
void worker_write_file(loader_worker_t *worker, uint8_t *data, uint32_t len);
extern u32_t sys_now(void);
err_t httpc_close(httpc_state_t* req, httpc_result_t result, u32_t server_response, err_t err);

loader_mgt_t g_mgr = {0};
loader_worker_t g_worker = {0};
static char file_name_prefix = 'a';//add a prefix char to file name, only for test

loader_mgt_t *get_mgr_singleton(){
    return &g_mgr;
}

void mgr_release(loader_mgt_t * mgr){
    mgr->is_running = 0;
}

int get_host_and_uri(char *url, char *host, char *uri, char *fn)
{
    int h_len = 0, u_len = 0, f_len = 0, url_idx = 0, last = 0;
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
        if(url[url_idx] == '/'){
            last = url_idx;
        }
        
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

    fn[f_len++] = file_name_prefix++;
    if(last > 0 && last < url_len - 1){
        last++;
        while(last < url_len){
            fn[f_len++] = url[last++];
            if(f_len >= MAX_NAME_LEN) return -5;
        }
        
        fn[f_len] = 0;
    }
    
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
    int is_finished = 0;
    if(worker->total_len > 0 && worker->recved_len >= worker->total_len){
        is_finished += 1;
    }
    
    if(worker->is_cancelled){
        is_finished += 2;
    }
    
    printf("a conn finished, result=%d, http_code=%d, rx_content_len=%d,time=%dms\r\n", httpc_result, srv_res, rx_content_len, time_span(worker->conn_start_time));
    worker->conn_state = NULL;
    if(is_finished){
        printf("download stop code[%d], %u/%u, spent=%ums\r\n", is_finished, worker->recved_len, worker->total_len, time_span(worker->start_time));
        worker_close_file(worker);
        mgr_release(get_mgr_singleton());
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
    if(0xFFFFFFFF == content_len || 0 == content_len){//-1, there is no content-length in header
        worker->is_cancelled = 1;
        printf("abort, no content-length in header\r\n");
        return ERR_ARG;
    }
    
    if(worker->total_len == 0 && content_len > 0){
        worker->total_len = content_len;
        
        if(lwip_strnstr((char *)hdr->payload, ACCEPT_RANGES, hdr_len) != NULL){
            printf("header: range supported\r\n");
            worker->support_range = 1;
        }
        
        worker_open_file(worker);
    }
    
    worker->conn_start_time = sys_now();
    return ERR_OK;
}

err_t on_httpc_data(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err){
    err_t ret = ERR_OK;
    struct pbuf *q = NULL;
    loader_worker_t *worker = (loader_worker_t *)arg;
    if(!p){
        printf("peer conn closed\r\n");
        altcp_close(conn);
    }else{
        //printf("recv len=%d\r\n", p->tot_len);
        //if(p->tot_len != p->len){
            //printf("recv first node len=%d\r\n", p->len);
        //}
        
        q = p;
       while(q){
            worker_write_file(worker, (uint8_t *)q->payload, q->len);
            q = q->next;
       }
        
        //ret = udp_console_send(p);
        //if(ret != ERR_OK){
        //    printf("udp_console_send err=%d\r\n", ret);
        //}
        worker->recved_len += p->tot_len;
        altcp_recved(conn, p->tot_len);
        //printf("[%u-%u]:%u->%u/%u\r\n", worker->worker_id, worker->conn_id, p->tot_len, worker->recved_len, worker->total_len);
        pbuf_free(p);
    }
    
    return ERR_OK;
}

loader_worker_t *worker_new(){
    //currently only support one worker
    memset(&g_worker, 0, sizeof(loader_worker_t));
    g_worker.cache_data = (uint8_t*)CACHE_ADDR;
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
    if(worker->is_cancelled){
        return;
    }
    
    worker->conn_settings.start_pos = worker->support_range?worker->recved_len:0;
    worker->conn_id++;
    
    if(worker->support_range == 0){
        worker->total_len = 0;
        worker->recved_len = 0;
    }
    
    err = httpc_get_file_dns(worker->host, 80, worker->uri, &worker->conn_settings, on_httpc_data, worker, &worker->conn_state);
    printf("worker_restart result=%d\r\n", err);
    
    if(err != ERR_OK){
        sys_timeout(5000, worker_restart, worker);
    }
}

void worker_stat(loader_worker_t *worker, download_stat_t* stat){
    uint32_t span = 0;
    if(worker->total_len){	
        span = time_span(worker->start_time)/1000;//ms -> S
        span = span>0?span : 1;
        stat->total_len = worker->total_len;
        stat->recved_len = worker->recved_len;
        stat->speed = worker->recved_len/span;
    }
}

void worker_stop(loader_worker_t *worker){
    worker->is_cancelled = 1;
    
    if(worker->conn_state) httpc_close(worker->conn_state, HTTPC_RESULT_LOCAL_ABORT, 0, -17);
    else sys_untimeout(worker_restart, worker);//there may be pending event, cancel it 

    printf("stopped by user\r\n");
}

void worker_open_file(loader_worker_t *worker){
    
    if(strlen(worker->filename) <= 0){
        printf("file name empty\r\n");
        return;
    }
    
    if(worker->fd_valid){
        printf("file already openned\r\n");
        return;
    }
    
    if(f_mount(&worker->fs, (TCHAR const*)"", 0) != FR_OK)
    {
        printf("f_mount error\r\n");
        return;
    }
    
    if (f_opendir(&worker->dir, "/") != FR_OK)
    {
        f_mount(NULL, (TCHAR const*)"",0);
        printf("f_opendir error\r\n");
        return;
    }
    
    if (f_open(&worker->fd, (const TCHAR*)worker->filename, FA_CREATE_ALWAYS|FA_WRITE) != FR_OK)
    {
        printf("f_open error\r\n");
        f_mount(NULL, (TCHAR const*)"",0);
        return;
    }
  
    worker->fd_valid = 1;

    //for test
    SDMMC_Clock_Set(6);
}

void worker_close_file(loader_worker_t *worker){
    uint32_t n = 0;
    if(!worker->fd_valid){
        printf("file not openned\r\n");
        return;
    }

    //save remaining data
    if(worker->cache_len > 0){
        f_write(&worker->fd, worker->cache_data, worker->cache_len, (UINT*)&n);
        if(worker->cache_len != n){
            printf("file flush error, %u/%u\r\n", n, worker->cache_len);
        }
        
        worker->cache_len = 0;
    }

    if (f_close(&worker->fd) != FR_OK)
    {
        printf("wanning: f_close error\r\n");
    }
    
    if (f_mount(NULL, (TCHAR const*)"",0) != FR_OK)
    {
        printf("wanning: unmount error\r\n");
    }
  
    worker->fd_valid = 0;
}

void worker_write_file(loader_worker_t *worker, uint8_t *data, uint32_t len){
    uint32_t index = 0;
    uint32_t n = 0;
    uint32_t cur_time = 0;
    uint32_t wlen = 0, other = 0;
    #define SECTOR_SIZE 512
    if(!worker->fd_valid){
        printf("file not openned, [%d] dropped\r\n", len);
        return;
    }
    
    if(len > MAX_MAX_CACHE_LEN){
        printf("data too long, [%d] dropped\r\n", len);
        return;
    }

    while(index < len){
        worker->cache_data[worker->cache_len] = data[index];
        index++;
        worker->cache_len++;
    }
    
    if(worker->cache_len >= MAX_MAX_CACHE_LEN){
        other = worker->cache_len%SECTOR_SIZE;
        wlen = worker->cache_len - other;

        cur_time = sys_now();
        f_write(&worker->fd, worker->cache_data, wlen, (UINT*)&n);
        if(wlen != n){
            printf("file write error, %u/%u\r\n", n, wlen);
        }
        printf("len=%u, time=%ums\r\n",  wlen, time_span(cur_time));

        worker->cache_len = 0;
        for(index = 0;index < other; index++){
            worker->cache_data[worker->cache_len] = worker->cache_data[wlen+index];
            worker->cache_len++;
        }
    }

}

int download_start(char * url)
{
    int ret = 0;
    
    loader_mgt_t * m= get_mgr_singleton();
    if(m->is_running){
        printf("request canceled, a task is still running\r\n");
        return -1;
    }
    
    loader_worker_t *worker = worker_new();
    ret = get_host_and_uri(url, worker->host, worker->uri, worker->filename);
    if(ret < 0 || !strlen(worker->host) || !strlen(worker->uri)|| !strlen(worker->filename)){
        printf("get host or url or filename error,ret=%d, url=%s\r\n", ret, url);
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
    
    m->worker = worker;
    m->is_running = 1;
    return 0;
}

int download_stat(download_stat_t *stat)
{
    loader_mgt_t * m= get_mgr_singleton();
    if(!stat){
        return -1;
    }
    
    if(!m->is_running){
        return -2;
    }
    
    worker_stat(m->worker, stat);
    return 0;
}

int download_stop(void)
{
    loader_mgt_t * m= get_mgr_singleton();
    
    if(!m->is_running){
        return -1;
    }
	
    worker_stop(m->worker);
    m->is_running = 0;
    return 0;
}
