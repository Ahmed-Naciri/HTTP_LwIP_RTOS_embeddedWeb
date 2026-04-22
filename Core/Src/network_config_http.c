#include "network_config_http.h"

#include "network_config.h"
#include "lwip.h"
#include "lwip/ip4_addr.h"

#include <stdio.h>
#include <string.h>

/* Keep large HTTP page buffer out of thread stack to avoid hard faults. */
static char g_config_page_buffer[1400];

static const char CONFIG_PAGE_TEMPLATE[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"\r\n"
"<!doctype html>"
"<html><head><meta charset=\"utf-8\"><title>Network Config</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Network Configuration</h1>"
"<form method=\"POST\" action=\"/save_config\">"
"<p><label>IP Address<br><input name=\"ip\" value=\"%u.%u.%u.%u\" style=\"width:260px\"></label></p>"
"<p><label>Netmask<br><input name=\"netmask\" value=\"%u.%u.%u.%u\" style=\"width:260px\"></label></p>"
"<p><label>Gateway<br><input name=\"gateway\" value=\"%u.%u.%u.%u\" style=\"width:260px\"></label></p>"
"<p><button type=\"submit\">Save</button></p>"
"</form>"
"<p><a href=\"/\">Back to main page</a></p>"
"</body></html>";

static const char SAVE_OK_PAGE[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"\r\n"
"<!doctype html><html><head><meta charset=\"utf-8\"><title>Saved</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Configuration Saved</h1>"
"<p>Reboot required: new network settings will apply after reset.</p>"
"<p><a href=\"/config.html\">Back to configuration page</a></p>"
"</body></html>";

static const char SAVE_BAD_REQUEST[] =
"HTTP/1.1 400 Bad Request\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"\r\n"
"<!doctype html><html><head><meta charset=\"utf-8\"><title>Invalid</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Invalid Configuration</h1>"
"<p>One or more fields are invalid. Please check IP, netmask, and gateway.</p>"
"<p><a href=\"/config.html\">Back to configuration page</a></p>"
"</body></html>";

static int parse_octet(const char *start, const char **next)
{
  unsigned value = 0;
  int digits = 0;
  const char *p = start;

  while ((*p >= '0') && (*p <= '9')) {
    value = (value * 10u) + (unsigned)(*p - '0');
    if (value > 255u) {
      return -1;
    }
    p++;
    digits++;
  }

  if (digits == 0) {
    return -1;
  }

  *next = p;
  return (int)value;
}

static int parse_ipv4(const char *text, uint8_t out[4])
{
  const char *p = text;
  const char *next = text;
  int i;
  int octet;

  while ((*p == ' ') || (*p == '\t')) {
    p++;
  }

  for (i = 0; i < 4; i++) {
    octet = parse_octet(p, &next);
    if (octet < 0) {
      return 0;
    }
    out[i] = (uint8_t)octet;
    p = next;
    if (i < 3) {
      if (*p != '.') {
        return 0;
      }
      p++;
    }
  }

  while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) {
    p++;
  }

  return (*p == '\0') ? 1 : 0;
}

static int hex_to_nibble(char c)
{
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }
  return -1;
}

static void url_decode_inplace(char *text)
{
  char *src = text;
  char *dst = text;
  int hi;
  int lo;

  while (*src != '\0') {
    if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else if ((*src == '%') && (src[1] != '\0') && (src[2] != '\0')) {
      hi = hex_to_nibble(src[1]);
      lo = hex_to_nibble(src[2]);
      if ((hi >= 0) && (lo >= 0)) {
        *dst++ = (char)((hi << 4) | lo);
        src += 3;
      } else {
        *dst++ = *src++;
      }
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
}

static int get_param(const char *body, const char *key, char *out, unsigned out_size)
{
  char pattern[24];
  const char *k;
  const char *v;
  unsigned i = 0;
  unsigned key_len;

  if ((body == NULL) || (key == NULL) || (out == NULL) || (out_size == 0u)) {
    return 0;
  }

  key_len = (unsigned)strlen(key);
  if ((key_len + 2u) >= sizeof(pattern)) {
    return 0;
  }

  (void)snprintf(pattern, sizeof(pattern), "%s=", key);
  k = strstr(body, pattern);
  if (k == NULL) {
    return 0;
  }

  v = k + strlen(pattern);
  while ((v[i] != '\0') && (v[i] != '&') && (i < (out_size - 1u))) {
    out[i] = v[i];
    i++;
  }
  out[i] = '\0';

  url_decode_inplace(out);

  while ((i > 0u) && ((out[i - 1u] == ' ') || (out[i - 1u] == '\t') || (out[i - 1u] == '\r') || (out[i - 1u] == '\n'))) {
    out[i - 1u] = '\0';
    i--;
  }

  return i > 0u;
}

void network_config_http_send_form(struct netconn *conn)
{
  uint8_t disp_ip[4];
  uint8_t disp_mask[4];
  uint8_t disp_gw[4];

  disp_ip[0] = g_network_config.ip[0];
  disp_ip[1] = g_network_config.ip[1];
  disp_ip[2] = g_network_config.ip[2];
  disp_ip[3] = g_network_config.ip[3];

  disp_mask[0] = g_network_config.netmask[0];
  disp_mask[1] = g_network_config.netmask[1];
  disp_mask[2] = g_network_config.netmask[2];
  disp_mask[3] = g_network_config.netmask[3];

  disp_gw[0] = g_network_config.gateway[0];
  disp_gw[1] = g_network_config.gateway[1];
  disp_gw[2] = g_network_config.gateway[2];
  disp_gw[3] = g_network_config.gateway[3];

  if (netif_is_up(&gnetif) && netif_is_link_up(&gnetif)) {
    disp_ip[0] = (uint8_t)ip4_addr1_16(netif_ip4_addr(&gnetif));
    disp_ip[1] = (uint8_t)ip4_addr2_16(netif_ip4_addr(&gnetif));
    disp_ip[2] = (uint8_t)ip4_addr3_16(netif_ip4_addr(&gnetif));
    disp_ip[3] = (uint8_t)ip4_addr4_16(netif_ip4_addr(&gnetif));

    disp_mask[0] = (uint8_t)ip4_addr1_16(netif_ip4_netmask(&gnetif));
    disp_mask[1] = (uint8_t)ip4_addr2_16(netif_ip4_netmask(&gnetif));
    disp_mask[2] = (uint8_t)ip4_addr3_16(netif_ip4_netmask(&gnetif));
    disp_mask[3] = (uint8_t)ip4_addr4_16(netif_ip4_netmask(&gnetif));

    disp_gw[0] = (uint8_t)ip4_addr1_16(netif_ip4_gw(&gnetif));
    disp_gw[1] = (uint8_t)ip4_addr2_16(netif_ip4_gw(&gnetif));
    disp_gw[2] = (uint8_t)ip4_addr3_16(netif_ip4_gw(&gnetif));
    disp_gw[3] = (uint8_t)ip4_addr4_16(netif_ip4_gw(&gnetif));
  }

  (void)snprintf(g_config_page_buffer, sizeof(g_config_page_buffer), CONFIG_PAGE_TEMPLATE,
                 disp_ip[0], disp_ip[1], disp_ip[2], disp_ip[3],
                 disp_mask[0], disp_mask[1], disp_mask[2], disp_mask[3],
                 disp_gw[0], disp_gw[1], disp_gw[2], disp_gw[3]);

  netconn_write(conn, g_config_page_buffer, strlen(g_config_page_buffer), NETCONN_COPY);
}

void network_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len)
{
  const char *body;
  network_config_t new_cfg;
  char ip_str[20];
  char mask_str[20];
  char gw_str[20];

  (void)request_len;

  if (request == NULL) {
    netconn_write(conn, SAVE_BAD_REQUEST, strlen(SAVE_BAD_REQUEST), NETCONN_COPY);
    return;
  }

  body = strstr(request, "\r\n\r\n");
  if (body == NULL) {
    netconn_write(conn, SAVE_BAD_REQUEST, strlen(SAVE_BAD_REQUEST), NETCONN_COPY);
    return;
  }
  body += 4;

  if ((!get_param(body, "ip", ip_str, sizeof(ip_str))) ||
      (!get_param(body, "netmask", mask_str, sizeof(mask_str))) ||
      (!get_param(body, "gateway", gw_str, sizeof(gw_str)))) {
    netconn_write(conn, SAVE_BAD_REQUEST, strlen(SAVE_BAD_REQUEST), NETCONN_COPY);
    return;
  }

  new_cfg = g_network_config;
  if ((!parse_ipv4(ip_str, new_cfg.ip)) ||
      (!parse_ipv4(mask_str, new_cfg.netmask)) ||
      (!parse_ipv4(gw_str, new_cfg.gateway))) {
    netconn_write(conn, SAVE_BAD_REQUEST, strlen(SAVE_BAD_REQUEST), NETCONN_COPY);
    return;
  }

  if (!network_config_save(&new_cfg)) {
    netconn_write(conn, SAVE_BAD_REQUEST, strlen(SAVE_BAD_REQUEST), NETCONN_COPY);
    return;
  }

  netconn_write(conn, SAVE_OK_PAGE, strlen(SAVE_OK_PAGE), NETCONN_COPY);
}
