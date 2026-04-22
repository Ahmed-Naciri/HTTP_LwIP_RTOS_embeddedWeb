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
#include "cmsis_os.h"

#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define WEBSERVER_THREAD_PRIO    ( osPriorityAboveNormal )

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
u32_t nPageHits = 0;

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
  struct netbuf *inbuf;
  err_t recv_err;
  char* buf;
  u16_t buflen;
  
  /* Read the data from the port, blocking if nothing yet there. 
   We assume the request (the part we care about) is in one netbuf */
  recv_err = netconn_recv(conn, &inbuf);
  
  if (recv_err == ERR_OK)
  {
    if (netconn_err(conn) == ERR_OK) 
    {
      netbuf_data(inbuf, (void**)&buf, &buflen);
    
      /* Is this an HTTP GET command? (only check the first 5 chars, since
      there are other formats for GET, and we're keeping it very simple )*/
      if ((buflen >=5) && (strncmp(buf, "GET /", 5) == 0))
      {
        if((strncmp(buf, "GET /test ", 10) == 0) ||
           (strncmp(buf, "GET / ", 6) == 0))
        {
          netconn_write(conn, TEST_PAGE, strlen(TEST_PAGE), NETCONN_COPY);
        }
        else 
        {
          netconn_write(conn, TEST_PAGE_1, strlen(TEST_PAGE), NETCONN_COPY);
        }
      }
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);
  
  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
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
