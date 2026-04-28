#ifndef APP_CONFIG_HTTP_H
#define APP_CONFIG_HTTP_H

#include "lwip/api.h"

void app_config_http_send_form(struct netconn *conn);
void app_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len);

#endif
