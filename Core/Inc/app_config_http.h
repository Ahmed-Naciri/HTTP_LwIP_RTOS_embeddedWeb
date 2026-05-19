#ifndef APP_CONFIG_HTTP_H
#define APP_CONFIG_HTTP_H

#include "lwip/api.h"

/* Combined page (backward compatibility - redirects to port config) */
void app_config_http_send_form(struct netconn *conn);

/* Separate configuration pages */
void app_config_http_send_port_form(struct netconn *conn);
void app_config_http_send_slaves_form(struct netconn *conn);
void app_config_http_send_registers_form(struct netconn *conn);

/* Handle POST actions */
void app_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len);

/* Values display page */
void app_config_http_send_values(struct netconn *conn);

#endif
