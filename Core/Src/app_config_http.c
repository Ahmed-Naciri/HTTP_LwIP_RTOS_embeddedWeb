#include "app_config_http.h"

#include "app_config.h"
#include "main.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_app_config_page_buffer[12000];
static size_t g_page_used = 0u;

/* Convert one hexadecimal character to its numeric nibble value. */
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

/* Decode application/x-www-form-urlencoded data in place. */
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

/* Extract a named form field from the HTTP body. */
static int get_param(const char *body, const char *key, char *out, unsigned out_size)
{
  char pattern[40];
  const char *k;
  const char *v;
  unsigned i = 0;

  if ((body == NULL) || (key == NULL) || (out == NULL) || (out_size == 0u)) {
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

  while ((i > 0u) &&
         ((out[i - 1u] == ' ') || (out[i - 1u] == '\t') || (out[i - 1u] == '\r') ||
          (out[i - 1u] == '\n'))) {
    out[i - 1u] = '\0';
    i--;
  }

  return 1;
}

/* Skip leading whitespace before parsing a numeric string. */
static char *trim_left(char *text)
{
  while ((*text == ' ') || (*text == '\t') || (*text == '\r')) {
    text++;
  }
  return text;
}

/* Parse an unsigned decimal number and reject trailing garbage. */
static int parse_ulong(const char *text, unsigned long *value)
{
  char *endptr;

  if ((text == NULL) || (value == NULL)) {
    return 0;
  }

  *value = strtoul(text, &endptr, 10);
  if ((endptr == text) || (*trim_left(endptr) != '\0')) {
    return 0;
  }

  return 1;
}

/* Reset the reusable HTML response buffer. */
static void page_reset(void)
{
  g_page_used = 0u;
  g_app_config_page_buffer[0] = '\0';
}

/* Append raw text to the reusable HTML response buffer. */
static void page_append(const char *text)
{
  size_t text_len;
  size_t remaining;

  if (text == NULL) {
    return;
  }

  if (g_page_used >= (sizeof(g_app_config_page_buffer) - 1u)) {
    return;
  }

  text_len = strlen(text);
  remaining = (sizeof(g_app_config_page_buffer) - 1u) - g_page_used;
  if (text_len > remaining) {
    text_len = remaining;
  }

  if (text_len > 0u) {
    (void)memcpy(&g_app_config_page_buffer[g_page_used], text, text_len);
    g_page_used += text_len;
    g_app_config_page_buffer[g_page_used] = '\0';
  }
}

/* Append a formatted string to the reusable HTML response buffer. */
static void page_appendf(const char *fmt, ...)
{
  char line[320];
  va_list args;

  va_start(args, fmt);
  (void)vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  page_append(line);
}

/* Append an unsigned integer without using heap allocation. */
static void page_append_uint(unsigned long value)
{
  char tmp[11];
  size_t i = 0u;
  size_t j;

  if (value == 0ul) {
    page_append("0");
    return;
  }

  while ((value > 0ul) && (i < sizeof(tmp))) {
    tmp[i] = (char)('0' + (value % 10ul));
    value /= 10ul;
    i++;
  }

  for (j = i; j > 0u; j--) {
    char c[2];
    c[0] = tmp[j - 1u];
    c[1] = '\0';
    page_append(c);
  }
}

/* Render the Modbus register type as short user-facing text. */
static const char *register_type_to_text(registerType_t t)
{
  if (t == REG_TYPE_U16) {
    return "U16";
  }
  if (t == REG_TYPE_I16) {
    return "I16";
  }
  return "FLOAT";
}

/* Build and send a small status page after a POST action. */
static void send_status_page(struct netconn *conn, int ok, const char *message)
{
  page_reset();
  page_append("HTTP/1.1 ");
  page_append(ok ? "200 OK\r\n" : "400 Bad Request\r\n");
  page_append("Content-Type: text/html\r\nConnection: close\r\n\r\n");
  page_append("<!doctype html><html><head><meta charset=\"utf-8\"><title>Modbus</title></head>");
  page_append("<body style=\"font-family:Arial,sans-serif;max-width:900px;margin:20px auto;\">");
  page_append(ok ? "<h1>Operation completed</h1>" : "<h1>Operation failed</h1>");
  page_append("<p>");
  page_append((message != NULL) ? message : "No details");
  page_append("</p>");
  page_append("<p><a href=\"/modbus_config.html\">Back to Modbus configuration</a></p>");
  page_append("</body></html>");

  netconn_write(conn, g_app_config_page_buffer, g_page_used, NETCONN_COPY);
}

/* Render the UART port configuration section. */
static void render_port_section(void)
{
  uint8_t i;

  /* Keep the global save action documented but disabled in the UI. */
//   page_append("<p><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0 0 12px 0\">"
//               "<input type=\"hidden\" name=\"action\" value=\"save_all\">"
//               "<button type=\"submit\">Save all changes to Flash</button>"
//               "</form></p>");

  page_append("<h2>Port</h2>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">\n");
  page_append("<input type=\"hidden\" name=\"action\" value=\"save_port\">\n");
  page_append("<p><label>Port<br><select name=\"port_id\">");
  for (i = 0; i < MAX_UART_PORTS; i++) {
    page_append("<option value=\"");
    page_append_uint((unsigned long)i);
    page_append("\" ");
    if (appDb.ports[0].portId == (uartPortId_t)i) {
      page_append("selected");
    }
    page_append(">UART");
    page_append_uint((unsigned long)(i + 1u));
    page_append("</option>");
  }
  page_append("</select></label></p>");

  page_append("<p><label>Baud rate<br><input name=\"baud\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].baudRate);
  page_append("\" style=\"width:220px\"></label></p>");

  page_append("<p><label>Stop bits (1 or 2)<br><input name=\"stop_bits\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].stopBits);
  page_append("\" style=\"width:220px\"></label></p>");

  page_append("<p><label>Parity (0=None,1=Even,2=Odd)<br><input name=\"parity\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].parity);
  page_append("\" style=\"width:220px\"></label></p>");
  page_append("<p><button type=\"submit\">Save port</button></p></form>");

  if (MAX_UART_PORTS == 1u) {
    page_append("<p style=\"color:#666\">Runtime is currently wired to one UART in firmware.</p>");
  }
}

/* Render the slave list and the add-slave form. */
static void render_slave_section(void)
{
  uint8_t i;

  page_append("<h2>Slaves</h2>");
  page_append("<table border=\"1\" cellpadding=\"6\" cellspacing=\"0\" style=\"border-collapse:collapse;width:100%\">"
              "<tr><th>Index</th><th>Address</th><th>Port</th><th>Registers</th><th>Action</th></tr>");
  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      page_append("<tr>");
      page_append("<td>");
      page_append_uint((unsigned long)i);
      page_append("</td>");
      page_append("<td>");
      page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
      page_append("</td>");
      page_append("<td>UART");
      page_append_uint((unsigned long)appDb.slaveConfig[i].portId + 1u);
      page_append("</td>");
      page_append("<td>");
      page_append_uint((unsigned long)appDb.slaveConfig[i].registerCount);
      page_append("</td>");
      page_append("<td><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0\">"
                  "<input type=\"hidden\" name=\"action\" value=\"delete_slave\">");
      page_append("<input type=\"hidden\" name=\"slave_index\" value=\"");
      page_append_uint((unsigned long)i);
      page_append("\">");
      page_append("<button type=\"submit\">Delete slave</button></form></td>");
      page_append("</tr>");
    }
  }
  page_append("</table>");

  page_append("<h3>Add slave</h3>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">"
              "<input type=\"hidden\" name=\"action\" value=\"add_slave\">"
              "<p><label>Slave address (1..247)<br><input name=\"slave_address\" style=\"width:220px\"></label></p>"
              "<p><label>Port<br><select name=\"slave_port\">");
  for (i = 0; i < MAX_UART_PORTS; i++) {
    page_append("<option value=\"");
    page_append_uint((unsigned long)i);
    page_append("\">UART");
    page_append_uint((unsigned long)(i + 1u));
    page_append("</option>");
  }
  page_append("</select></label></p><p><button type=\"submit\">Add slave</button></p></form>");
}

/* Render the register list and the add-register form. */
static void render_register_section(void)
{
  uint8_t i;
  uint8_t j;
  int has_slave = 0;

  page_append("<h2>Registers</h2>");
  page_append("<table border=\"1\" cellpadding=\"6\" cellspacing=\"0\" style=\"border-collapse:collapse;width:100%\">"
              "<tr><th>Slave index</th><th>Slave address</th><th>Register address</th><th>Type</th><th>Action</th></tr>");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          page_append("<tr>");
          page_append("<td>");
          page_append_uint((unsigned long)i);
          page_append("</td>");
          page_append("<td>");
          page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
          page_append("</td>");
          page_append("<td>");
          page_append_uint((unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          page_append("</td>");
          page_append("<td>");
          page_append(register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          page_append("</td>");
          page_append("<td><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0\">"
                      "<input type=\"hidden\" name=\"action\" value=\"delete_register\">");
          page_append("<input type=\"hidden\" name=\"reg_slave_index\" value=\"");
          page_append_uint((unsigned long)i);
          page_append("\">");
          page_append("<input type=\"hidden\" name=\"reg_address\" value=\"");
          page_append_uint((unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          page_append("\">");
          page_append("<button type=\"submit\">Delete register</button></form></td>");
          page_append("</tr>");
        }
      }
    }
  }
  page_append("</table>");

  page_append("<h3>Add register</h3>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">"
              "<input type=\"hidden\" name=\"action\" value=\"add_register\">"
              "<p><label>Slave<br><select name=\"reg_slave_index\">");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      has_slave = 1;
      page_append("<option value=\"");
      page_append_uint((unsigned long)i);
      page_append("\">idx ");
      page_append_uint((unsigned long)i);
      page_append(" - addr ");
      page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
      page_append("</option>");
    }
  }
  page_append("</select></label></p>");

  page_append("<p><label>Register address (0..65535)<br><input name=\"reg_address\" style=\"width:220px\"></label></p>");
  page_append("<p><label>Type<br><select name=\"reg_type\">"
              "<option value=\"0\">U16</option>"
              "<option value=\"1\">I16</option>"
              "<option value=\"2\">FLOAT</option>"
              "</select></label></p>");

  if (has_slave == 0) {
    page_append("<p style=\"color:#900\">No active slave. Add a slave first.</p>");
  }

  page_append("<p><button type=\"submit\">Add register</button></p></form>");
}

/* Build the complete Modbus configuration page and send it to the client. */
void app_config_http_send_form(struct netconn *conn)
{
  page_reset();
  page_append("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
  page_append("<!doctype html><html><head><meta charset=\"utf-8\"><title>Modbus Config</title></head>");
  page_append("<body style=\"font-family:Arial,sans-serif;max-width:980px;margin:20px auto;\">"
              "<h1>Modbus Configuration</h1>");

  render_port_section();
  render_slave_section();
  render_register_section();

  page_append("<p><a href=\"/config.html\">Network config</a></p>");
  page_append("<p><a href=\"/\">Back to main page</a></p>");
  page_append("</body></html>");

  netconn_write(conn, g_app_config_page_buffer, g_page_used, NETCONN_COPY);
}

/* Handle every POST action for the Modbus configuration page. */
void app_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len)
{
  const char *body;
  char action[32];
  unsigned long n1;
  unsigned long n2;
  unsigned long n3;

  (void)request_len;

  if (request == NULL) {
    send_status_page(conn, 0, "Request is empty");
    return;
  }

  body = strstr(request, "\r\n\r\n");
  if (body == NULL) {
    send_status_page(conn, 0, "HTTP body is missing");
    return;
  }
  body += 4;

  if (!get_param(body, "action", action, sizeof(action))) {
    send_status_page(conn, 0, "Missing action field");
    return;
  }

  if (strcmp(action, "save_port") == 0) {
    char v_baud[20];
    char v_stop[8];
    char v_parity[8];
    char v_port[8];
    unsigned long baud;
    unsigned long stop_bits;
    unsigned long parity;
    unsigned long port_id;

    if ((!get_param(body, "baud", v_baud, sizeof(v_baud))) ||
        (!get_param(body, "stop_bits", v_stop, sizeof(v_stop))) ||
        (!get_param(body, "parity", v_parity, sizeof(v_parity))) ||
        (!get_param(body, "port_id", v_port, sizeof(v_port)))) {
      send_status_page(conn, 0, "Port fields are missing");
      return;
    }

    if ((!parse_ulong(v_baud, &baud)) || (!parse_ulong(v_stop, &stop_bits)) ||
        (!parse_ulong(v_parity, &parity))) {
      send_status_page(conn, 0, "Port fields are invalid numbers");
      return;
    }

    if (!parse_ulong(v_port, &port_id)) {
      send_status_page(conn, 0, "Port id is invalid");
      return;
    }

    if ((port_id >= (unsigned long)MAX_UART_PORTS) || (baud == 0ul) || (baud > 2000000ul) ||
        ((stop_bits != 1ul) && (stop_bits != 2ul)) || (parity > (unsigned long)PARITY_ODD) ||
        (appConfig_updatePort((uartPortId_t)port_id, (uint32_t)baud, (uint8_t)stop_bits,
                              (parityType_t)parity) < 0)) {
      send_status_page(conn, 0, "Port update failed");
      return;
    }
    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after port update");
      return;
    }
    send_status_page(conn, 1, "Port updated and saved");
    return;
  }

  if (strcmp(action, "add_slave") == 0) {
    char v_addr[8];
    char v_port[8];

    if ((!get_param(body, "slave_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "slave_port", v_port, sizeof(v_port)))) {
      send_status_page(conn, 0, "Slave fields are missing");
      return;
    }

    if ((!parse_ulong(v_addr, &n1)) || (!parse_ulong(v_port, &n2))) {
      send_status_page(conn, 0, "Slave fields are invalid numbers");
      return;
    }

    if ((n1 == 0ul) || (n1 > 247ul) || (n2 >= (unsigned long)MAX_UART_PORTS)) {
      send_status_page(conn, 0, "Slave fields are out of range");
      return;
    }

    if (appConfig_addSlave((uint8_t)n1, (uartPortId_t)n2) < 0) {
      send_status_page(conn, 0, "Cannot add slave (duplicate or full table)");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after slave add");
      return;
    }

    send_status_page(conn, 1, "Slave added and saved");
    return;
  }

  if (strcmp(action, "delete_slave") == 0) {
    char v_idx[8];

    if (!get_param(body, "slave_index", v_idx, sizeof(v_idx))) {
      send_status_page(conn, 0, "Missing slave index");
      return;
    }

    if (!parse_ulong(v_idx, &n1)) {
      send_status_page(conn, 0, "Slave index is invalid");
      return;
    }

    if (appConfig_removeSlave((uint8_t)n1) < 0) {
      send_status_page(conn, 0, "Cannot remove slave");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after slave delete");
      return;
    }

    send_status_page(conn, 1, "Slave deleted with cascade register cleanup");
    return;
  }

  if (strcmp(action, "add_register") == 0) {
    char v_slave[8];
    char v_addr[10];
    char v_type[8];

    if ((!get_param(body, "reg_slave_index", v_slave, sizeof(v_slave))) ||
        (!get_param(body, "reg_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "reg_type", v_type, sizeof(v_type)))) {
      send_status_page(conn, 0, "Register fields are missing");
      return;
    }

    if ((!parse_ulong(v_slave, &n1)) || (!parse_ulong(v_addr, &n2)) || (!parse_ulong(v_type, &n3))) {
      send_status_page(conn, 0, "Register fields are invalid numbers");
      return;
    }

    if ((n1 >= (unsigned long)MAX_SLAVES) || (n2 > 65535ul) || (n3 > (unsigned long)REG_TYPE_FLOAT)) {
      send_status_page(conn, 0, "Register fields are out of range");
      return;
    }

    if (appConfig_addRegister((uint8_t)n1, (uint16_t)n2, (registerType_t)n3) < 0) {
      send_status_page(conn, 0, "Cannot add register (duplicate, invalid slave, or full table)");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after register add");
      return;
    }

    send_status_page(conn, 1, "Register added and saved");

    return;
  }

  if (strcmp(action, "delete_register") == 0) {
    char v_slave[8];
    char v_addr[10];

    if ((!get_param(body, "reg_slave_index", v_slave, sizeof(v_slave))) ||
        (!get_param(body, "reg_address", v_addr, sizeof(v_addr)))) {
      send_status_page(conn, 0, "Missing register delete fields");
      return;
    }

    if ((!parse_ulong(v_slave, &n1)) || (!parse_ulong(v_addr, &n2))) {
      send_status_page(conn, 0, "Register delete fields are invalid numbers");
      return;
    }

    if (appConfig_removeRegister((uint8_t)n1, (uint16_t)n2) < 0) {
      send_status_page(conn, 0, "Cannot remove register");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after register delete");
      return;
    }

    send_status_page(conn, 1, "Register deleted and saved");
    return;
  }

  send_status_page(conn, 0, "Unknown action");
}
