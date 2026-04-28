/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Netconn_RTOS/Src/httpser-netconn.c 
  * @author  MCD Application Team
  * @brief   Basic http server implementation using LwIP netconn API  
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
#include "string.h"
#include "httpserver-netconn.h"
#include "app_config_http.h"
#include "network_config_http.h"
#include "cmsis_os.h"

#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define WEBSERVER_THREAD_PRIO    ( osPriorityAboveNormal )

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
u32_t nPageHits = 0;
static char g_http_request_buffer[4096];

static int parse_content_length(const char *request)
{
  const char *p;
  int value = 0;

  if (request == NULL)
  {
    return -1;
  }

    /* Look up HTTP Content-Length so POST body can be assembled completely
      even when TCP data arrives in multiple segments. */
    p = strstr(request, "Content-Length:");
  if (p == NULL)
  {
    return -1;
  }

  p += 15;
  while ((*p == ' ') || (*p == '\t'))
  {
    p++;
  }

  while ((*p >= '0') && (*p <= '9'))
  {
    value = (value * 10) + (int)(*p - '0');
    p++;
  }

  return value;
}

/* Format of dynamic web page: the page header */
static const char TEST_PAGE[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"\r\n"
"<!doctype html><html><head><meta charset=\"utf-8\"><title>ETH Test</title></head>"
"<body style=\"font-family:Arial,sans-serif\"><h1>ETH OK</h1>"
"<p>If you can read this, the STM32 HTTP server path is working.</p>"
"</body></html>";

static const char TEST_PAGE_1[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"\r\n"
"<!doctype html><html><head><meta charset=\"utf-8\"><title>ETH Test</title></head>"
"<body style=\"font-family:Arial,sans-serif\"><h1>ETH OK</h1>"
"<p>If you can read this, the STM32 HTTP server path is working anyway hhhhh.</p>"
"</body></html>";

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief serve tcp connection  
  * @param conn: pointer on connection structure 
  * @retval None
  */
static void http_server_serve(struct netconn *conn) 
{
  struct netbuf *inbuf = NULL;
  struct netbuf *nextbuf = NULL;
  err_t recv_err;
  u16_t total_len;
  u16_t next_len;
  u16_t copy_len;
  u16_t body_len;
  int content_len;
  u16_t request_len;
  char *body_start;
  
  /* Read the first TCP segment for this request. */
  recv_err = netconn_recv(conn, &inbuf);
  
  if (recv_err == ERR_OK)
  {
    if (netconn_err(conn) == ERR_OK) 
    {
      total_len = netbuf_len(inbuf);
      /* Copy request into a bounded, null-terminated buffer for safe parsing. */
      request_len = (total_len < (sizeof(g_http_request_buffer) - 1u)) ? total_len : (sizeof(g_http_request_buffer) - 1u);
      (void)netbuf_copy(inbuf, g_http_request_buffer, request_len);
      g_http_request_buffer[request_len] = '\0';

      if ((request_len >= 5) && (strncmp(g_http_request_buffer, "POST ", 5) == 0))
      {
        /* POST payload may be split across multiple netconn_recv calls.
           Keep reading until body length reaches Content-Length. */
        content_len = parse_content_length(g_http_request_buffer);
        body_start = strstr(g_http_request_buffer, "\r\n\r\n");
        body_len = 0u;
        if (body_start != NULL)
        {
          body_len = (u16_t)(request_len - (u16_t)((body_start + 4) - g_http_request_buffer));
        }

        while ((content_len >= 0) && (body_start != NULL) && (body_len < (u16_t)content_len) && (request_len < (sizeof(g_http_request_buffer) - 1u)))
        {
          recv_err = netconn_recv(conn, &nextbuf);
          if ((recv_err != ERR_OK) || (nextbuf == NULL))
          {
            break;
          }

          next_len = netbuf_len(nextbuf);
          copy_len = ((u16_t)(sizeof(g_http_request_buffer) - 1u - request_len) < next_len)
                     ? (u16_t)(sizeof(g_http_request_buffer) - 1u - request_len)
                     : next_len;

          (void)netbuf_copy(nextbuf, &g_http_request_buffer[request_len], copy_len);
          request_len = (u16_t)(request_len + copy_len);
          g_http_request_buffer[request_len] = '\0';

          body_start = strstr(g_http_request_buffer, "\r\n\r\n");
          if (body_start != NULL)
          {
            body_len = (u16_t)(request_len - (u16_t)((body_start + 4) - g_http_request_buffer));
          }

          netbuf_delete(nextbuf);
          nextbuf = NULL;
        }
      }
    
      /* Route dispatch: save endpoint, config page, and test pages. */
      if ((request_len >= 17) && (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0))
      {
        network_config_http_handle_save(conn, g_http_request_buffer, request_len);
      }
      else if ((request_len >= 24) && (strncmp(g_http_request_buffer, "POST /save_modbus_config", 24) == 0))
      {
        app_config_http_handle_save(conn, g_http_request_buffer, request_len);
      }
      else if ((request_len >= 5) && (strncmp(g_http_request_buffer, "GET /", 5) == 0))
      {
        if (strncmp(g_http_request_buffer, "GET /config.html", 16) == 0)
        {
          network_config_http_send_form(conn);
        }
        else if (strncmp(g_http_request_buffer, "GET /modbus_config.html", 23) == 0)
        {
          app_config_http_send_form(conn);
        }
        else if ((strncmp(g_http_request_buffer, "GET /test ", 10) == 0) ||
                 (strncmp(g_http_request_buffer, "GET / ", 6) == 0))
        {
          netconn_write(conn, TEST_PAGE, strlen(TEST_PAGE), NETCONN_COPY);
        }
        else
        {
          netconn_write(conn, TEST_PAGE_1, strlen(TEST_PAGE_1), NETCONN_COPY);
        }
      }
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);
  
  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  if (inbuf != NULL)
  {
    netbuf_delete(inbuf);
  }

  if (nextbuf != NULL)
  {
    netbuf_delete(nextbuf);
  }
}


/**
  * @brief  http server thread 
  * @param arg: pointer on argument(not used here) 
  * @retval None
  */
static void http_server_netconn_thread(void *arg)
{ 
  struct netconn *conn, *newconn;
  err_t err, accept_err;
  
  /* Create a new TCP connection handle */
  conn = netconn_new(NETCONN_TCP);
  
  if (conn!= NULL)
  {
    /* Bind to port 80 (HTTP) with default IP address */
    err = netconn_bind(conn, NULL, 80);
    
    if (err == ERR_OK)
    {
      /* Put the connection into LISTEN state */
      netconn_listen(conn);
  
      while(1) 
      {
        /* accept any incoming connection */
        accept_err = netconn_accept(conn, &newconn);
        if(accept_err == ERR_OK)
        {
          /* serve connection */
          http_server_serve(newconn);

          /* delete connection */
          netconn_delete(newconn);
        }
      }
    }
  }
}

/**
  * @brief  Initialize the HTTP server (start its thread) 
  * @param  none
  * @retval None
  */
void http_server_netconn_init()
{
  sys_thread_new("HTTP", http_server_netconn_thread, NULL, DEFAULT_THREAD_STACKSIZE, WEBSERVER_THREAD_PRIO);
}

/**
  * @brief  Create and send a dynamic Web Page. This page contains the list of 
  *         running tasks and the number of page hits. 
  * @param  conn pointer on connection structure 
  * @retval None
  */
//#if 0
//void DynWebPage(struct netconn *conn)
//{
//  portCHAR PAGE_BODY[512];
//  portCHAR pagehits[10] = {0};
//
//  memset(PAGE_BODY, 0,512);
//
//  /* Update the hit count */
//  nPageHits++;
//  sprintf(pagehits, "%d", (int)nPageHits);
//  strcat(PAGE_BODY, pagehits);
//  strcat((char *)PAGE_BODY, "<pre><br>Name          State  Priority  Stack   Num" );
//  strcat((char *)PAGE_BODY, "<br>---------------------------------------------<br>");
//
//  /* The list of tasks and their status */
//  osThreadList((unsigned char *)(PAGE_BODY + strlen(PAGE_BODY)));
//  strcat((char *)PAGE_BODY, "<br><br>---------------------------------------------");
//  strcat((char *)PAGE_BODY, "<br>B : Blocked, R : Ready, D : Deleted, S : Suspended<br>");
//
//  /* Send the dynamically generated page */
//  netconn_write(conn, PAGE_START, strlen((char*)PAGE_START), NETCONN_COPY);
//  netconn_write(conn, PAGE_BODY, strlen(PAGE_BODY), NETCONN_COPY);
//}
//#endif
