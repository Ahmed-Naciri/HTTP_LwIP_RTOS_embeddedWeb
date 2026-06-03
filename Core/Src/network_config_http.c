/*
 * @brief  This file manages the web page configuration
 *
 */

#include "network_config_http.h"

#include "network_config.h"
#include "main.h"
#include "lwip.h"
#include "lwip/ip4_addr.h"

#include <stdio.h>
#include <string.h>

static void http_write(struct netconn *conn, const char *s)
{
  if ((s != NULL) && (conn != NULL)) {
    netconn_write(conn, s, strlen(s), NETCONN_COPY);
  }
}

static void http_write_uint(struct netconn *conn, unsigned long v)
{
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "%lu", v);
  if (n > 0) {
    http_write(conn, buf);
  }
}

static void http_write_ipv4(struct netconn *conn, const uint8_t ip[4])
{
  http_write_uint(conn, (unsigned long)ip[0]);
  http_write(conn, ".");
  http_write_uint(conn, (unsigned long)ip[1]);
  http_write(conn, ".");
  http_write_uint(conn, (unsigned long)ip[2]);
  http_write(conn, ".");
  http_write_uint(conn, (unsigned long)ip[3]);
}

static const char SAVE_OK_PAGE[] =
"<!doctype html><html><head><meta charset=\"utf-8\"><title>Saved</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Configuration Saved</h1>"
"<p>Reboot required: new network settings will apply after reset.</p>"
"<p><a href=\"/config.html\">Back to configuration page</a></p>"
"</body></html>";

static const char SAVE_REBOOT_PAGE[] =
"<!doctype html><html><head><meta charset=\"utf-8\"><title>Rebooting</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Configuration Saved</h1>"
"<p>The device will reboot software now and come back on the new IP address.</p>"
"<p>If the page stops responding, reconnect to the new IP after a few seconds.</p>"
"</body></html>";

static const char SAVE_BAD_REQUEST[] =
"<!doctype html><html><head><meta charset=\"utf-8\"><title>Invalid</title></head>"
"<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
"<h1>Invalid Configuration</h1>"
"<p>One or more fields are invalid. Please check IP, netmask, and gateway.</p>"
"<p><a href=\"/config.html\">Back to configuration page</a></p>"
"</body></html>";

static void send_http_response_with_status(struct netconn *conn,
                                           const char *status,
                                           const char *body)
{
  char header[160];
  int n;
  size_t body_len = strlen(body);

  n = snprintf(header, sizeof(header),
               "HTTP/1.1 %s\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: %lu\r\n"
               "Cache-Control: no-cache\r\n"
               "Connection: close\r\n"
               "\r\n",
               status,
               (unsigned long)body_len);

  if (n > 0) {
    netconn_write(conn, header, (size_t)n, NETCONN_COPY);
  }
  netconn_write(conn, body, body_len, NETCONN_COPY);
}

static int parse_ipv4(const char *text, uint8_t out[4])
{
  unsigned a;
  unsigned b;
  unsigned c;
  unsigned d;

  /* Keep this parser short and strict enough for normal IPv4 input. */
  if (sscanf(text, " %u.%u.%u.%u ", &a, &b, &c, &d) != 4) {
    return 0;
  }

  if ((a > 255u) || (b > 255u) || (c > 255u) || (d > 255u)) {
    return 0;
  }

  out[0] = (uint8_t)a;
  out[1] = (uint8_t)b;
  out[2] = (uint8_t)c;
  out[3] = (uint8_t)d;
  return 1;
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
    /* HTML form encoding: '+' means space. */
    if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else if ((*src == '%') && (src[1] != '\0') && (src[2] != '\0')) {
      /* Percent decoding for URL-encoded values (e.g. %2E => '.'). */
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

    /* Start with persisted values, then override with live netif values when
       link is up so UI shows current runtime state.Because the page then shows the actual running network state,
       not just the last saved config (last config will be displayed after reboot). */

  //g_network_config = what you configured / saved
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

//gnetif = what the network actually uses
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

  /* Stream the page directly to avoid building a large dynamic buffer. */
  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!doctype html>"
    "<html><head><meta charset=\"utf-8\"><title>Network Config</title></head>"
    "<body style=\"font-family:Arial,sans-serif;max-width:640px;margin:20px auto;\">"
    "<h1>Network Configuration</h1>"
    "<form method=\"POST\" action=\"/save_config\">"
    "<p><label>IP Address<br><input name=\"ip\" value=\""
  );
  http_write_ipv4(conn, disp_ip);
  http_write(conn, "\" style=\"width:260px\"></label></p>");

  http_write(conn, "<p><label>Netmask<br><input name=\"netmask\" value=\"");
  http_write_ipv4(conn, disp_mask);
  http_write(conn, "\" style=\"width:260px\"></label></p>");

  http_write(conn, "<p><label>Gateway<br><input name=\"gateway\" value=\"");
  http_write_ipv4(conn, disp_gw);
  http_write(conn,
    "\" style=\"width:260px\"></label></p>"
    "<p style=\"display:flex;gap:12px;flex-wrap:wrap;\">"
    "<button type=\"submit\" name=\"reboot\" value=\"1\" style=\"background:#b00020;color:#fff;border:none;padding:8px 14px;border-radius:4px;cursor:pointer;\">Apply &amp; Reboot</button>"
    "</p>"
    "</form>"
    "<p><a href=\"/\">Back to main page</a></p>"
    "</body></html>"
  );
}

void network_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len)
{
  const char *body;
  network_config_t new_cfg;
  char ip_str[20];
  char mask_str[20];
  char gw_str[20];
  char reboot_str[8];
  int reboot_requested;

  (void)request_len;

  if (request == NULL) {
    send_http_response_with_status(conn, "400 Bad Request", SAVE_BAD_REQUEST);
    return;
  }

  /* Body starts after the blank line that terminates HTTP headers.because when the user save the form , the browser sent a request that contain both Header and Body separated by this : \r\n\r\n */
  body = strstr(request, "\r\n\r\n");
  if (body == NULL) {
    send_http_response_with_status(conn, "400 Bad Request", SAVE_BAD_REQUEST);
    return;
  }
  body += 4;

  /* Read and decode required fields from application/x-www-form-urlencoded body. */
  if ((!get_param(body, "ip", ip_str, sizeof(ip_str))) ||
      (!get_param(body, "netmask", mask_str, sizeof(mask_str))) ||
      (!get_param(body, "gateway", gw_str, sizeof(gw_str)))) {
    send_http_response_with_status(conn, "400 Bad Request", SAVE_BAD_REQUEST);
    return;
  }

  reboot_requested = get_param(body, "reboot", reboot_str, sizeof(reboot_str));

  /* Parse into numeric octets and reject invalid values early. */
  new_cfg = g_network_config;
  if ((!parse_ipv4(ip_str, new_cfg.ip)) ||
      (!parse_ipv4(mask_str, new_cfg.netmask)) ||
      (!parse_ipv4(gw_str, new_cfg.gateway))) {
    send_http_response_with_status(conn, "400 Bad Request", SAVE_BAD_REQUEST);
    return;
  }

  /* Persist to flash-backed EEPROM emulation. */
  if (!network_config_save(&new_cfg)) {
    send_http_response_with_status(conn, "400 Bad Request", SAVE_BAD_REQUEST);
    return;
  }

  if (reboot_requested) {
    send_http_response_with_status(conn, "200 OK", SAVE_REBOOT_PAGE);
    HAL_Delay(200u);
    NVIC_SystemReset();
  } else {
    send_http_response_with_status(conn, "200 OK", SAVE_OK_PAGE);
  }
}
