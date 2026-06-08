/*
 * @brief  This file manages the web page configuration
 *
 */

#include "network_config_http.h"

#include "network_config.h"
#include "main.h"
#include "lwip.h"
#include "lwip/ip4_addr.h"
#include "lwip/sys.h"

#include <stdio.h>
#include <string.h>

static void http_write(struct netconn *conn, const char *s)
{
  if ((s == NULL) || (conn == NULL)) {
    return;
  }

  {
    const char *p = s;
    size_t remaining = strlen(s);

    while (remaining > 0u) {
      size_t chunk = (remaining > 256u) ? 256u : remaining;
      err_t err;
      uint16_t retry = 0u;

      do {
        err = netconn_write(conn, p, chunk, NETCONN_COPY);
        if (err == ERR_MEM) {
          sys_msleep(2u);
        }
        retry++;
      } while ((err == ERR_MEM) && (retry < 200u));

      if (err != ERR_OK) {
        break;
      }

      p += chunk;
      remaining -= chunk;
    }
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

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='utf-8'/>"
    "<meta content='width=device-width, initial-scale=1.0' name='viewport'/>"
    "<title>Clinical Precision | Network</title>"
    "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&display=swap' rel='stylesheet'/>"
    "<link href='https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:wght,FILL@100..700,0..1&display=swap' rel='stylesheet'/>"
    "<style>"
    ":root{--primary:#00478d;--primary-container:#005eb8;--secondary:#5c5f63;--tertiary:#ba1a1a;--surface:#f7faf9;--surface-container-low:#f1f4f3;--surface-container-lowest:#ffffff;--on-surface:#181c1c;--on-surface-variant:#424752;--outline-variant:#c2c6d4;--error-container:#ffdad6;--on-error-container:#93000a;--text-main:#181c1c;--text-secondary:#64748b;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Inter',sans-serif;background-color:var(--surface);color:var(--text-main);display:flex;min-height:100vh;overflow:hidden;}"
    "aside{width:256px;background-color:var(--surface-container-low);border-right:1px solid var(--outline-variant);display:flex;flex-direction:column;padding:24px 16px;}"
    ".brand{margin-bottom:40px;padding-left:8px;}"
    ".brand h1{font-size:20px;font-weight:900;color:var(--primary);letter-spacing:-0.025em;}"
    ".brand p{font-size:10px;text-transform:uppercase;letter-spacing:0.1em;color:var(--secondary);font-weight:700;opacity:0.7;}"
    "nav{flex:1;}"
    ".nav-item{display:flex;align-items:center;gap:12px;padding:10px 12px;text-decoration:none;color:#475569;font-size:14px;font-weight:500;border-radius:8px;margin-bottom:4px;transition:background 0.2s;}"
    ".nav-item:hover{background-color:#e2e8f0;}"
    ".nav-item.active{background-color:var(--surface-container-lowest);color:var(--primary);font-weight:600;box-shadow:0 1px 3px rgba(0,0,0,0.05);}"
    ".sidebar-footer{padding-top:16px;border-top:1px solid var(--outline-variant);}"
    ".status-pill{display:flex;align-items:center;gap:12px;padding:8px;}"
    ".status-icon{width:32px;height:32px;border-radius:50%;background-color:var(--primary-container);color:white;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:700;}"
    ".status-text p:first-child{font-size:12px;font-weight:700;}.status-text p:last-child{font-size:10px;color:#16a34a;font-weight:600;}"
    "main{flex:1;display:flex;flex-direction:column;overflow-y:auto;min-height:0;}"
    "header{height:64px;display:flex;align-items:center;padding:0 32px;background-color:var(--surface);position:sticky;top:0;z-index:10;}"
    ".header-left{display:flex;align-items:center;gap:16px;}.header-left h2{font-size:18px;font-weight:700;color:#0f172a;}.divider{width:1px;height:16px;background-color:var(--outline-variant);}.breadcrumb{font-size:14px;font-weight:500;color:var(--text-secondary);}"
    ".content-body{padding:32px;max-width:1200px;}"
    ".page-header{display:flex;justify-content:space-between;align-items:flex-end;margin-bottom:24px;gap:16px;}.page-title h2{font-size:30px;font-weight:900;letter-spacing:-0.025em;}.button-group{display:flex;gap:8px;flex-wrap:wrap;}.btn{padding:8px 16px;border-radius:8px;font-size:12px;font-weight:700;display:flex;align-items:center;gap:8px;cursor:pointer;border:1px solid transparent;text-decoration:none;}.btn-outline{background:var(--surface-container-lowest);border-color:var(--outline-variant);color:#475569;}.btn-primary{background:var(--primary);color:white;box-shadow:0 4px 6px -1px rgba(0,71,141,0.2);}"
    ".section-grid{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:16px;margin-bottom:24px;}.card{background:var(--surface-container-lowest);border-radius:16px;border:1px solid rgba(0,0,0,0.05);padding:18px;box-shadow:0 4px 12px rgba(0,71,141,0.04);}.card h3{font-size:12px;text-transform:uppercase;letter-spacing:0.08em;color:#64748b;margin-bottom:8px;}.summary-card,.form-card{grid-column:1 / -1;}.field-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;margin-top:8px;}label{display:block;font-size:12px;font-weight:800;letter-spacing:0.06em;text-transform:uppercase;color:#64748b;margin-bottom:6px;}input{width:100%;padding:12px 14px;border:1px solid var(--outline-variant);border-radius:10px;background:#fff;font:inherit;color:#0f172a;outline:none;}input:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(0,78,141,0.12);}.full{grid-column:1 / -1;}.actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:8px;}.value-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px;}.value-box{background:var(--surface-container-low);border-radius:14px;padding:16px;}.value-box .label{font-size:10px;font-weight:900;text-transform:uppercase;letter-spacing:0.08em;color:#64748b;margin-bottom:6px;}.value-box .value{font-size:22px;font-weight:900;color:#0f172a;}.muted{font-size:13px;color:#64748b;font-weight:500;margin-top:6px;}"
    ".footer{margin-top:32px;padding-top:24px;border-top:1px solid var(--outline-variant);display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap;}.back-link{text-decoration:none;color:var(--primary);font-size:14px;font-weight:700;display:flex;align-items:center;gap:8px;}.session-info{font-size:10px;color:#94a3b8;font-weight:500;}.material-symbols-outlined{font-size:20px;vertical-align:middle;}"
    "@media (max-width:1024px){body{flex-direction:column;overflow:auto;}aside{width:100%;border-right:none;border-bottom:1px solid var(--outline-variant);}main{overflow:visible;}.content-body{padding:20px;}.section-grid{grid-template-columns:1fr;}.page-header{align-items:flex-start;flex-direction:column;}.field-grid,.value-grid{grid-template-columns:1fr;}.button-group{width:100%;}.btn{justify-content:center;flex:1 1 140px;}}"
    "@media (max-width:640px){header{padding:14px 16px;}.content-body{padding:16px;}.page-title h2{font-size:24px;}}"
    "</style></head><body>"
     "<aside>"
	"<div class='brand'>"
		  "<img src='/STM32F4xx_files/Meier.png' style='width:150px;height:auto;'>"
    "</div>"
	"<nav>"
	"<a class='nav-item' href='/'><span class='material-symbols-outlined'>home</span>Home</a>"
    "<a class='nav-item active' href='/config.html'><span class='material-symbols-outlined'>hub</span>Network</a>"
    "<a class='nav-item' href='/modbus_values.html'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>chat</span>Live Readings</a>"
    "<a class='nav-item' href='/modbus_config_port.html'><span class='material-symbols-outlined'>build</span>Maintenance</a>"
    "</nav>"
    "<div class='sidebar-footer'><div class='status-pill'><div class='status-icon'>SN04</div><div class='status-text'><p>System Status</p><p>Nominal</p></div></div></div>"
    "</aside>"
    "<main><header><div class='header-left'><h2>Clinical Precision Home</h2><div class='divider'></div><span class='breadcrumb'>Configuration / Ethernet</span></div></header><div class='content-body'>"
    "<div class='page-header'><div class='page-title'><h2>Network Configuration</h2></div><div class='button-group'><a class='btn btn-outline' href='/modbus_config_port.html'>Port</a><a class='btn btn-outline' href='/modbus_config_slaves.html'>Slaves</a><a class='btn btn-outline' href='/modbus_config_registers.html'>Registers</a></div></div>"
    "<div class='section-grid'><div class='card summary-card'><div class='value-grid'>"
    "<div class='value-box'><div class='label'>IP Address</div><div class='value'>");
  http_write_ipv4(conn, disp_ip);
  http_write(conn,
    "</div></div><div class='value-box'><div class='label'>Netmask</div><div class='value'>");
  http_write_ipv4(conn, disp_mask);
  http_write(conn,
    "</div></div><div class='value-box'><div class='label'>Gateway</div><div class='value'>");
  http_write_ipv4(conn, disp_gw);
  http_write(conn,
    "</div></div></div></div>"
    "<div class='card form-card'><form method='POST' action='/save_config' class='field-grid'>"
    "<div><label for='ip'>IP Address</label><input id='ip' name='ip' value='"
  );
  http_write_ipv4(conn, disp_ip);
  http_write(conn,
    "' autocomplete='off'/></div><div><label for='netmask'>Netmask</label><input id='netmask' name='netmask' value='"
  );
  http_write_ipv4(conn, disp_mask);
  http_write(conn,
    "' autocomplete='off'/></div><div class='full'><label for='gateway'>Gateway</label><input id='gateway' name='gateway' value='"
  );
  http_write_ipv4(conn, disp_gw);
  http_write(conn,
    "' autocomplete='off'/></div><div class='full actions'><button class='btn btn-primary' type='submit' name='reboot' value='1'><span class='material-symbols-outlined'>restart_alt</span>Apply &amp; Reboot</button></div></form></div></div>"
    "<div class='footer'><a class='back-link' href='/'><span class='material-symbols-outlined'>arrow_back</span>Back to main page</a><p class='session-info'>Configuration is persisted in flash-backed storage.</p></div></main></body></html>"
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
