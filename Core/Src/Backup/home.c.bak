#include "home.h"

#include "app_config.h"
#include "network_config.h"

#include "lwip/api.h"
#include "lwip/sys.h"
#include <stdio.h>
#include <string.h>

/* Stream writer with small chunks to match the logic used in other HTTP pages. */
static void home_write(struct netconn *conn, const char *s)
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

static void home_write_uint(struct netconn *conn, unsigned long v)
{
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "%lu", v);
  if (n > 0) {
    home_write(conn, buf);
  }
}

void home_send_page(struct netconn *conn)
{
  static unsigned long s_home_hits = 0u;

  uint8_t i;
  uint8_t j;
  unsigned long slave_count = 0u;
  unsigned long reg_count = 0u;
  unsigned long alarm_count = 0u;
  char ipbuf[32];
  char maskbuf[32];
  char gwbuf[32];

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used) {
      slave_count++;
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used) {
          reg_count++;
          if (appDb.slaveConfig[i].registerConfig[j].alarmActive) {
            alarm_count++;
          }
        }
      }
    }
  }

  s_home_hits++;

  (void)snprintf(ipbuf,
                 sizeof(ipbuf),
                 "%u.%u.%u.%u",
                 g_network_config.ip[0],
                 g_network_config.ip[1],
                 g_network_config.ip[2],
                 g_network_config.ip[3]);

  (void)snprintf(maskbuf,
                 sizeof(maskbuf),
                 "%u.%u.%u.%u",
                 g_network_config.netmask[0],
                 g_network_config.netmask[1],
                 g_network_config.netmask[2],
                 g_network_config.netmask[3]);

  (void)snprintf(gwbuf,
                 sizeof(gwbuf),
                 "%u.%u.%u.%u",
                 g_network_config.gateway[0],
                 g_network_config.gateway[1],
                 g_network_config.gateway[2],
                 g_network_config.gateway[3]);

  home_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='utf-8'/>"
    "<meta content='width=device-width, initial-scale=1.0' name='viewport'/>"
    "<title>Clinical Precision | Home</title>"
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
    "main{flex:1;display:flex;flex-direction:column;overflow-y:auto;}"
    "header{height:64px;display:flex;justify-content:space-between;align-items:center;padding:0 32px;background-color:var(--surface);position:sticky;top:0;z-index:10;}"
    ".header-left h2{font-size:18px;font-weight:700;color:#0f172a;}"
    ".content-body{padding:32px;max-width:1200px;}"
    ".section-title{font-size:11px;text-transform:uppercase;letter-spacing:0.1em;color:#64748b;margin-bottom:10px;font-weight:800;}"
    ".grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px;margin-bottom:24px;}"
    ".card{background:var(--surface-container-lowest);border-radius:16px;border:1px solid rgba(0,0,0,0.05);padding:18px;box-shadow:0 4px 12px rgba(0,71,141,0.04);}"
    ".card h3{font-size:12px;text-transform:uppercase;letter-spacing:0.08em;color:#64748b;margin-bottom:8px;}"
    ".card p{font-size:22px;font-weight:900;color:#0f172a;}"
    ".muted{font-size:13px;color:#64748b;font-weight:500;margin-top:6px;}"
    "@media(max-width:980px){aside{display:none;}body{overflow:auto;}.grid{grid-template-columns:1fr 1fr;}}"
    "@media(max-width:640px){.grid{grid-template-columns:1fr;}.content-body{padding:16px;}}"
    "</style></head><body>"
    "<aside>"
    "<div class='brand'><h1>HealthSystems</h1><p>Precision Node 04</p></div>"
    "<nav>"
    "<a class='nav-item active' href='/'><span class='material-symbols-outlined'>home</span>Home</a>"
    "<a class='nav-item' href='/config.html'><span class='material-symbols-outlined'>hub</span>Network</a>"
    "<a class='nav-item' href='/modbus_values.html'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>chat</span>Live Readings</a>"
    "<a class='nav-item' href='/modbus_config_port.html'><span class='material-symbols-outlined'>build</span>Maintenance</a>"
    "</nav>"
    "<div class='sidebar-footer'><div class='status-pill'><div class='status-icon'>SN04</div><div class='status-text'><p>System Status</p><p>Nominal</p></div></div></div>"
    "</aside>");



  home_write(conn,
    "<main><header><div class='header-left'><h2>Clinical Precision Home</h2></div></header><div class='content-body'>"
    "<div class='section-title'>Network</div>"
    "<div class='grid'><div class='card'><h3>Network IP</h3><p>");

  home_write(conn, ipbuf);

  home_write(conn,
    "</p><div class='muted'>Current runtime/saved IPv4 address</div></div>"
    "<div class='card'><h3>Netmask</h3><p>");

  home_write(conn, maskbuf);

  home_write(conn,
    "</p><div class='muted'>Subnet mask</div></div>"
    "<div class='card'><h3>Gateway</h3><p>");

  home_write(conn, gwbuf);

  home_write(conn,
    "</p><div class='muted'>Default route gateway</div></div></div>"
    "<div class='section-title'>Modbus</div>"
    "<div class='grid'>"
    "<div class='card'><h3>Configured Slaves</h3><p>");

  home_write_uint(conn, slave_count);

  home_write(conn,
    "</p><div class='muted'>Modbus RTU slave devices configured</div></div>"
    "<div class='card'><h3>Configured Registers</h3><p>");

  home_write_uint(conn, reg_count);

  home_write(conn,
    "</p><div class='muted'>Total active registers in polling set</div></div>"
    "<div class='card'><h3>Active Alarms</h3><p>");

  home_write_uint(conn, alarm_count);

  home_write(conn,
    "</p><div class='muted'>Threshold alarms currently active</div></div>"
    "<div class='card'><h3>RS485 / UART</h3><p>");

  if (appDb.ports[0].used == 1u) {
    home_write(conn, "READY");
  } else {
    home_write(conn, "NOT SET");
  }

  home_write(conn,
    "</p><div class='muted'>Physical link status from current app config</div></div>"
    "<div class='card'><h3>System Role</h3><p>MODBUS MASTER</p><div class='muted'>STM32 polling and supervision</div></div></div>"
    "<p class='muted'>Home hits: ");

  home_write_uint(conn, s_home_hits);

  home_write(conn,
    "</p></div></main></body></html>");
}
