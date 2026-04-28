# STM32 Web and Network Debug Postmortem (Last 2 Days)

## Objective
Build a stable web page on STM32 that lets you:
1. View current network settings.
2. Change IP, netmask, and gateway.
3. Save to flash EEPROM emulation.
4. Apply changes after reboot.

## Final Outcome
The project now has a stable web path and a working save workflow, with known causes identified for each failure mode seen during debugging.

## Beginner Full Walkthrough (Network Part From Start To End)

This section is written for you as a complete beginner. It shows the real code path in execution order.

Execution order at runtime:
1. Boot task loads saved config and starts networking.
2. lwIP applies IP/netmask/gateway.
3. HTTP server listens on port 80.
4. Browser GET opens config page.
5. Browser POST sends save request.
6. Parser validates fields.
7. EEPROM emulation stores the config.
8. After reboot, saved values are loaded and applied.

### Step 1: Boot Sequence (start networking)
Source: [Core/Src/main.c](Core/Src/main.c)

```c
void StartDefaultTask(void *argument)
{
   network_config_load();
   MX_LWIP_Init();
   http_server_netconn_init();
   for(;;)
   {
      osDelay(1);
   }
}
```

Simple explanation:
1. `StartDefaultTask` is the FreeRTOS task that runs after scheduler start.
2. `network_config_load()` reads saved IP settings from EEPROM emulation into RAM.
3. `MX_LWIP_Init()` starts lwIP and applies those settings.
4. `http_server_netconn_init()` starts web server thread.
5. Infinite loop keeps task alive and yields CPU.

### Step 2: Network Config Data Model (what is saved)
Source: [Core/Inc/network_config.h](Core/Inc/network_config.h)

```c
typedef struct {
   uint32_t magic;
   uint8_t ip[4];
   uint8_t netmask[4];
   uint8_t gateway[4];
} network_config_t;
```

Simple explanation:
1. `magic`: special marker to know flash data is valid.
2. `ip[4]`: four numbers of IPv4 (example: 192,168,1,222).
3. `netmask[4]`: subnet mask.
4. `gateway[4]`: default gateway.

### Step 3: Load and Save to EEPROM Emulation
Source: [Core/Src/network_config.c](Core/Src/network_config.c)

```c
void network_config_load(void)
{
   network_config_t tmp;

   network_config_set_defaults(&g_network_config);

   if (!network_config_ee_init()) {
      return;
   }

   ee_read();

   if (!network_config_is_valid(&g_network_config)) {
      network_config_set_defaults(&tmp);
      (void)network_config_save(&tmp);
   }
}

bool network_config_save(const network_config_t *cfg)
{
   network_config_t tmp;

   if (cfg == NULL) {
      return false;
   }

   tmp = *cfg;
   tmp.magic = NETWORK_CONFIG_MAGIC;

   if (!network_config_ee_init()) {
      return false;
   }

   g_network_config = tmp;
   return ee_write();
}
```

Simple explanation:
1. Load starts with safe defaults first.
2. It initializes EEPROM emulation backend.
3. It reads persisted bytes from flash.
4. If marker is invalid, defaults are written back.
5. Save copies input config, forces valid marker, updates RAM copy, writes to flash.

### Step 4: Apply Network Values into lwIP
Source: [LWIP/App/lwip.c](LWIP/App/lwip.c)

```c
void MX_LWIP_Init(void)
{
   tcpip_init(NULL, NULL);

   IP4_ADDR(&ipaddr,
                g_network_config.ip[0],
                g_network_config.ip[1],
                g_network_config.ip[2],
                g_network_config.ip[3]);
   IP4_ADDR(&netmask,
                g_network_config.netmask[0],
                g_network_config.netmask[1],
                g_network_config.netmask[2],
                g_network_config.netmask[3]);
   IP4_ADDR(&gw,
                g_network_config.gateway[0],
                g_network_config.gateway[1],
                g_network_config.gateway[2],
                g_network_config.gateway[3]);

   netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
   netif_set_default(&gnetif);
   netif_set_up(&gnetif);
   netif_set_link_callback(&gnetif, ethernet_link_status_updated);

   memset(&attributes, 0x0, sizeof(osThreadAttr_t));
   attributes.name = "EthLink";
   attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
   attributes.priority = osPriorityBelowNormal;
   osThreadNew(ethernet_link_thread, &gnetif, &attributes);
}
```

Simple explanation:
1. `tcpip_init` starts lwIP core thread.
2. `IP4_ADDR` builds IP, mask, and gateway from persisted config bytes.
3. `netif_add` registers ethernet interface.
4. `netif_set_default` makes it default route.
5. `netif_set_up` marks interface up in software.
6. Link callback and link thread manage physical cable/link changes.

### Step 5: HTTP Server Receive and Route
Source: [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c)

```c
static int parse_content_length(const char *request)
{
   const char *p;
   int value = 0;

   if (request == NULL) {
      return -1;
   }

   p = strstr(request, "Content-Length:");
   if (p == NULL) {
      return -1;
   }

   p += 15;
   while ((*p == ' ') || (*p == '\t')) {
      p++;
   }

   while ((*p >= '0') && (*p <= '9')) {
      value = (value * 10) + (int)(*p - '0');
      p++;
   }

   return value;
}
```

Simple explanation:
1. Finds `Content-Length` header.
2. Skips spaces/tabs.
3. Converts text digits to integer length.
4. Returns body size needed for POST parsing.

```c
if ((request_len >= 17) && (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0))
{
   content_len = parse_content_length(g_http_request_buffer);
   body_start = strstr(g_http_request_buffer, "\r\n\r\n");
   ...
   while ((content_len >= 0) && (body_start != NULL) && (body_len < (u16_t)content_len) && (request_len < (sizeof(g_http_request_buffer) - 1u)))
   {
      recv_err = netconn_recv(conn, &nextbuf);
      ...
      (void)netbuf_copy(nextbuf, &g_http_request_buffer[request_len], copy_len);
      request_len = (u16_t)(request_len + copy_len);
      g_http_request_buffer[request_len] = '\0';
   }
}
```

Simple explanation:
1. Detects POST `/save_config`.
2. Computes required body length.
3. Keeps receiving TCP chunks until full body is available.
4. This avoids broken parsing when browser sends segmented packets.

### Step 6: Form Parser and Input Validation
Source: [Core/Src/network_config_http.c](Core/Src/network_config_http.c)

```c
while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) {
   p++;
}
return (*p == '\0') ? 1 : 0;
```

Word-by-word explanation of this exact line:
1. `while`: repeat loop while condition is true.
2. `(` `)`: group condition expression.
3. `*p`: value of character currently pointed to by pointer `p`.
4. `==`: comparison operator (equals).
5. `' '`: normal space character.
6. `||`: logical OR (if any side is true, whole condition is true).
7. `'\t'`: tab character (horizontal tab).
8. `'\r'`: carriage return (CR, ASCII 13).
9. `'\n'`: line feed (LF, ASCII 10).
10. `p++`: move pointer to next character.
11. Meaning of whole loop: skip all trailing whitespace characters.
12. `(*p == '\0') ? 1 : 0`: after skipping spaces/newlines, valid only if end of string reached.

Important text-separator meaning in HTTP:
1. `\r\n` is one line ending in HTTP headers.
2. `\r\n\r\n` means header section ended, body starts next.

Parser helpers:
```c
static void url_decode_inplace(char *text)
{
   ...
   if (*src == '+') {
      *dst++ = ' ';
   } else if ((*src == '%') && (src[1] != '\0') && (src[2] != '\0')) {
      ...
   }
}
```

Simple explanation:
1. `+` in form data means space.
2. `%xx` means hex-encoded character.
3. Function decodes URL-encoded data before parsing IP text.

Save path:
```c
void network_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len)
{
   const char *body;
   network_config_t new_cfg;
   char ip_str[20];
   char mask_str[20];
   char gw_str[20];

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
```

Simple explanation:
1. Finds HTTP body.
2. Extracts `ip`, `netmask`, `gateway` fields.
3. Parses all into numeric bytes.
4. Saves to EEPROM emulation.
5. Returns success page if everything passed.

### Step 7: Ethernet Link Stability (PHY debounce)
Source: [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c)

```c
uint32_t down_confirm_count = 0U;

if (PHYLinkState < DP83848_STATUS_LINK_DOWN)
{
   down_confirm_count = 0U;
}
else if(netif_is_link_up(netif) && (PHYLinkState == DP83848_STATUS_LINK_DOWN))
{
   down_confirm_count++;
   if (down_confirm_count >= 3U)
   {
      HAL_ETH_Stop_IT(&heth);
      netif_set_down(netif);
      netif_set_link_down(netif);
   }
}
```

Simple explanation:
1. Do not drop link on one bad PHY sample.
2. Require 3 consecutive down readings.
3. This avoids unstable toggling due to transient PHY read glitches.

### Step 8: Critical Configuration Constants

lwIP thread stack size source: [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h)

```c
#define DEFAULT_THREAD_STACKSIZE 2048
```

Simple explanation:
1. Gives enough stack for HTTP/netconn processing under browser traffic.

EEPROM mapping source: [Core/Inc/ee_config.h](Core/Inc/ee_config.h)

```c
#define EE_SELECTED_PAGE_SECTOR_NUMBER    11
#define EE_SELECTED_PAGE_SECTOR_SIZE      EE_PAGE_SECTOR_SIZE_128K
#define EE_SELECTED_BANK                  FLASH_BANK_1
#define EE_SELECTED_ADDRESS               0x080E0000
```

Simple explanation:
1. These 4 values must describe one coherent flash region.
2. If one value is wrong, save/load can silently fail.

### C Language Mini Dictionary (for this network code)

Core symbols:
1. `*p`: dereference pointer `p` (read character at address).
2. `&x`: address of variable `x`.
3. `p++`: increment pointer/variable by one.
4. `==`: equal comparison.
5. `!=`: not equal comparison.
6. `&&`: logical AND (all conditions true).
7. `||`: logical OR (any condition true).
8. `!`: logical NOT.
9. `?:`: ternary operator (`condition ? true_value : false_value`).

Character escapes:
1. `'\0'`: string terminator (end of C string).
2. `'\t'`: horizontal tab.
3. `'\r'`: carriage return (move cursor to line start).
4. `'\n'`: line feed (new line).
5. `"\r\n\r\n"`: HTTP header end marker.

Common network functions you see here:
1. `netconn_recv`: receive TCP data from lwIP netconn socket.
2. `netconn_write`: send HTTP response.
3. `netif_set_up`: software-up network interface.
4. `netif_set_link_up`: physical link-up indication.
5. `IP4_ADDR`: build IPv4 address object from four bytes.
6. `strstr`: find substring inside a C string.
7. `strncmp`: compare string prefix.
8. `snprintf`: format text safely with output size limit.

### Full Process Summary (mental model)
1. Boot task loads config from EEPROM emulation.
2. lwIP starts with loaded static IP settings.
3. HTTP server accepts browser request.
4. GET returns HTML form.
5. POST parser assembles full body and decodes values.
6. IPv4 parser validates each octet.
7. Save writes struct to flash-backed EEPROM.
8. Reboot applies new values because load runs before lwIP init.

## Problem Timeline and Root Cause Analysis

### Phase 1: HTTP server integration and page serving instability
1. Symptom
1. Ping might work.
2. Opening page sometimes did not match expected content.
3. Runtime became unstable during browser access.

2. Cause
1. HTTP request handling was too simplistic for segmented TCP payloads.
2. Runtime stack usage was too high in web path for configured thread stack sizes.

3. Code for cause
1. Segmented request handling and POST body assembly in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
static char g_http_request_buffer[4096];

static int parse_content_length(const char *request)
{
   const char *p;
   int value = 0;
   ...
}

if ((request_len >= 17) && (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0))
{
   content_len = parse_content_length(g_http_request_buffer);
   ...
   while ((content_len >= 0) && ...)
   {
      recv_err = netconn_recv(conn, &nextbuf);
      ...
      netbuf_copy(nextbuf, &g_http_request_buffer[request_len], copy_len);
   }
}
```

2. Thread stack increase in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h).

```c
#define DEFAULT_THREAD_STACKSIZE 2048
```

4. Solution
1. Improve request handling in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).
2. Increase lwIP default thread stack in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h).
3. Move large temporary buffers out of stack in web handlers.

5. Code for solution
1. Request handling in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
if ((request_len >= 17) && (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0))
{
   network_config_http_handle_save(conn, g_http_request_buffer, request_len);
}
else if ((request_len >= 5) && (strncmp(g_http_request_buffer, "GET /", 5) == 0))
{
   ...
}
```

2. Stack size tuning in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h).

```c
#define DEFAULT_THREAD_STACKSIZE 2048
```

3. Moved large page buffer to static storage in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
static char g_config_page_buffer[1400];
```

6. Why this fix
1. Browser traffic is more complex than ping.
2. HTTP body parsing must handle multi-segment receive.
3. Stack overflows in RTOS tasks can look like random network failures.

### Phase 2: Browser access triggered debugger halt or timeout after a few seconds
1. Symptom
1. Device responds for a short time.
2. Browser request causes halt and Resume button becomes active.
3. Ping drops after web access.

2. Cause
1. Stack pressure from large local buffers in web request and page generation paths.
2. Link handling path could overreact to transient PHY status.

3. Code for cause
1. Removed dangerous per-request stack usage and guarded netbuf cleanup in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
static void http_server_serve(struct netconn *conn)
{
   struct netbuf *inbuf = NULL;
   ...
   if (inbuf != NULL)
   {
      netbuf_delete(inbuf);
   }
}
```

2. Added PHY down debounce in [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c).

```c
uint32_t down_confirm_count = 0U;
...
down_confirm_count++;
if (down_confirm_count >= 3U)
{
   HAL_ETH_Stop_IT(&heth);
   netif_set_down(netif);
   netif_set_link_down(netif);
}
```

4. Solution
1. Buffer placement and sizing fixes in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c) and [Core/Src/network_config_http.c](Core/Src/network_config_http.c).
2. Thread stack sizing updates in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h).
3. Link handling hardening in [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c).

5. Code for solution
1. Buffer sizing in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
static char g_http_request_buffer[4096];
```

2. Large HTML buffer moved in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
static char g_config_page_buffer[1400];
```

3. Thread stack and link hardening in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h), [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c).

```c
#define DEFAULT_THREAD_STACKSIZE 2048
#define INTERFACE_THREAD_STACK_SIZE ( 1024 )
```

6. Why this fix
1. Hard faults from stack overflow often appear only under web traffic.
2. PHY read glitches should not immediately force interface down.

### Phase 3: Save page showed Invalid Configuration repeatedly
1. Symptom
1. Form submit to save endpoint returned invalid.
2. Same values looked valid to the user.

2. Cause
1. POST body parsing did not always capture full body.
2. Parameter extraction needed URL decode and whitespace handling.
3. Validation logic and save behavior changed during iterations and became inconsistent at times.

3. Code for cause
1. Full POST body receive logic in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
if ((request_len >= 17) && (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0))
{
   content_len = parse_content_length(g_http_request_buffer);
   body_start = strstr(g_http_request_buffer, "\r\n\r\n");
   ...
}
```

2. URL decode and trim in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
url_decode_inplace(out);
while ((i > 0u) && ((out[i - 1u] == ' ') || (out[i - 1u] == '\t') ||
                              (out[i - 1u] == '\r') || (out[i - 1u] == '\n')))
{
   out[i - 1u] = '\0';
   i--;
}
```

3. Validation policy stabilization in [Core/Src/network_config.c](Core/Src/network_config.c).

```c
bool network_config_is_valid(const network_config_t *cfg)
{
   if (cfg == NULL) {
      return false;
   }
   return cfg->magic == NETWORK_CONFIG_MAGIC;
}
```

4. Solution
1. Full body receive logic and Content-Length based handling in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).
2. URL decode and trim handling in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).
3. Save path cleanup in [Core/Src/network_config.c](Core/Src/network_config.c).

5. Code for solution
1. Content-Length parser in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c).

```c
static int parse_content_length(const char *request)
{
   const char *p;
   int value = 0;
   ...
}
```

2. IPv4 parse with whitespace tolerance in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
while ((*p == ' ') || (*p == '\t')) {
   p++;
}
...
while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) {
   p++;
}
return (*p == '\0') ? 1 : 0;
```

3. Save path in [Core/Src/network_config.c](Core/Src/network_config.c).

```c
bool network_config_save(const network_config_t *cfg)
{
   network_config_t tmp;
   ...
   g_network_config = tmp;
   return ee_write();
}
```

6. Why this fix
1. Embedded HTTP forms often fail on parser edge cases, not on user input.
2. Robust parsing removes false invalid results.

### Phase 4: Save success page appeared, but reboot kept same IP
1. Symptom
1. Save page says success.
2. After reboot, IP did not change.

2. Cause
1. Startup was still in forced recovery mode in some revisions, overriding saved values.
2. EEPROM manual configuration became inconsistent in some revisions.

3. Code for cause
1. Startup path fixed in [Core/Src/main.c](Core/Src/main.c).

```c
void StartDefaultTask(void *argument)
{
   network_config_load();
   MX_LWIP_Init();
   http_server_netconn_init();
   ...
}
```

2. EEPROM mapping normalized in [Core/Inc/ee_config.h](Core/Inc/ee_config.h).

```c
#define EE_SELECTED_PAGE_SECTOR_NUMBER    11
#define EE_SELECTED_PAGE_SECTOR_SIZE      EE_PAGE_SECTOR_SIZE_128K
#define EE_SELECTED_BANK                  FLASH_BANK_1
#define EE_SELECTED_ADDRESS               0x080E0000
```

4. Solution
1. Restore load-on-boot flow in [Core/Src/main.c](Core/Src/main.c).
2. Correct EEPROM sector/bank/address/size mapping in [Core/Inc/ee_config.h](Core/Inc/ee_config.h).

5. Code for solution
1. Load-on-boot in [Core/Src/main.c](Core/Src/main.c).

```c
network_config_load();
MX_LWIP_Init();
```

2. Coherent flash EEPROM mapping in [Core/Inc/ee_config.h](Core/Inc/ee_config.h).

```c
#define EE_SELECTED_PAGE_SECTOR_NUMBER    11
#define EE_SELECTED_PAGE_SECTOR_SIZE      EE_PAGE_SECTOR_SIZE_128K
#define EE_SELECTED_BANK                  FLASH_BANK_1
#define EE_SELECTED_ADDRESS               0x080E0000
```

6. Why this fix
1. If startup forces defaults, saved values can never apply.
2. If EEPROM mapping is inconsistent, erase and write may not target same region.

### Phase 5: Form showed previous saved IP, not currently working live IP
1. Symptom
1. Input fields did not match active runtime interface values.
2. User confusion between live values and saved values.

2. Cause
1. Page initially rendered from saved config structure only.
2. Runtime netif values can differ until reboot.

3. Code for cause
1. Exposed live netif symbols in [LWIP/App/lwip.h](LWIP/App/lwip.h).

```c
extern struct netif gnetif;
extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;
```

2. Live-value rendering logic in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
if (netif_is_up(&gnetif) && netif_is_link_up(&gnetif)) {
   disp_ip[0] = (uint8_t)ip4_addr1_16(netif_ip4_addr(&gnetif));
   ...
}
```

4. Solution
1. Expose live netif globals in [LWIP/App/lwip.h](LWIP/App/lwip.h).
2. Render active values when link is up in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

5. Code for solution
1. Header exports in [LWIP/App/lwip.h](LWIP/App/lwip.h).

```c
extern struct netif gnetif;
extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;
```

2. Form render source selection in [Core/Src/network_config_http.c](Core/Src/network_config_http.c).

```c
disp_ip[0] = g_network_config.ip[0];
...
if (netif_is_up(&gnetif) && netif_is_link_up(&gnetif)) {
   disp_ip[0] = (uint8_t)ip4_addr1_16(netif_ip4_addr(&gnetif));
   ...
}
```

6. Why this fix
1. UI should reflect actual current state to avoid misleading behavior.

## Key Design Decisions and Why
1. Apply after reboot, not immediately:
   Prevents in-session disconnect and avoids link reset while request is active.
2. Recovery mode was used temporarily:
   Helped regain access when config path was unstable.
3. Then recovery mode was removed:
   Necessary so saved values can actually apply on reboot.
4. Robust HTTP parser was prioritized:
   Most invalid form failures were parser and body assembly issues.
5. EEPROM mapping was treated as critical:
   Wrong mapping makes all save logic appear broken even when code is correct.

## Current File-by-File Responsibilities

### Boot and startup
1. [Core/Src/main.c](Core/Src/main.c)
   Controls startup order. Must load saved network config before LwIP init when normal mode is enabled.

### Network apply layer
1. [LWIP/App/lwip.c](LWIP/App/lwip.c)
   Applies g_network_config values into ipaddr, netmask, gw for gnetif.
2. [LWIP/App/lwip.h](LWIP/App/lwip.h)
   Exposes relevant globals used by HTTP display logic.

### Low-level Ethernet and link behavior
1. [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c)
   PHY status handling, RX/TX callbacks, link up/down management.
2. [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h)
   Thread stack sizes and lwIP behavior configuration.

### HTTP server and form handling
1. [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c)
   Netconn server loop, route handling, full request/body assembly.
2. [Core/Src/network_config_http.c](Core/Src/network_config_http.c)
   Form rendering, field extraction, URL decode, IPv4 parsing, save response.

### Config and persistence
1. [Core/Inc/network_config.h](Core/Inc/network_config.h)
   Data model and APIs for network config.
2. [Core/Src/network_config.c](Core/Src/network_config.c)
   Defaults, load, save path tied to EEPROM emulation buffer.
3. [Core/Inc/ee_config.h](Core/Inc/ee_config.h)
   Manual flash mapping for EEPROM emulation target area.
4. [Core/Src/ee.c](Core/Src/ee.c)
   Flash erase, write, read, verify implementation.

### Build integration files
1. [Debug/Core/Src/subdir.mk](Debug/Core/Src/subdir.mk)
2. [Debug/objects.list](Debug/objects.list)

These must include all new config and HTTP source files so changes are actually compiled and linked.

## Final Stable Behavior Expected
1. Device boots.
2. Startup loads saved config.
3. LwIP applies loaded values.
4. HTTP page opens and remains stable.
5. Save endpoint accepts valid form values.
6. EEPROM write succeeds.
7. Reboot applies new IP.

## Quick Validation Checklist
1. Flash firmware and hard reset board.
2. Open page and verify values shown are expected live values.
3. Save a new IP.
4. Confirm success page.
5. Reboot board.
6. Ping new IP.
7. Open page on new IP and confirm values persisted.

## Known High-Risk Areas for Future Changes
1. Editing [Core/Inc/ee_config.h](Core/Inc/ee_config.h) without keeping bank, sector number, sector size, and address coherent.
2. Reducing thread stack sizes in [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h).
3. Replacing robust request assembly in [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c) with single-chunk assumptions.
4. Forcing defaults at boot in [Core/Src/main.c](Core/Src/main.c) while expecting saved values to apply.

## Practical Lessons
1. Ping stability does not guarantee HTTP path stability.
2. Browser requests reveal parser, stack, and runtime issues that ping alone will not.
3. In embedded systems, one visible symptom can come from multiple independent layers.
4. Startup order is as important as save logic.
5. Persistence correctness depends as much on flash mapping as on application code.

---

## Code Change Audit (Old vs Correct)

<div style="padding:12px 14px;border-left:6px solid #0a7a3d;background:#eefaf2;margin:10px 0;">
<strong>Scope:</strong> Exact implementation-level fixes made during this debug cycle.
</div>

<div style="display:flex;gap:8px;flex-wrap:wrap;margin:6px 0 14px 0;">
<span style="background:#e7f0ff;color:#0b4aa2;padding:4px 10px;border-radius:999px;">Boot + Persistence</span>
<span style="background:#fff4e6;color:#8a4b00;padding:4px 10px;border-radius:999px;">HTTP Parsing</span>
<span style="background:#efe9ff;color:#5b2ca0;padding:4px 10px;border-radius:999px;">Runtime Stability</span>
<span style="background:#e8f7ff;color:#045a84;padding:4px 10px;border-radius:999px;">Link + PHY</span>
</div>

### 1) Boot loads saved config before LwIP
Where: [Core/Src/main.c](Core/Src/main.c#L339)
Why: Without this, saved IP/netmask/gateway could not apply after reboot.

```c
// OLD
//network_config_load();
MX_LWIP_Init();
http_server_netconn_init();

// CORRECT
network_config_load();
MX_LWIP_Init();
http_server_netconn_init();
```

### 2) Default task stack increased
Where: [Core/Src/main.c](Core/Src/main.c#L55)
Why: Browser/netconn path caused stack pressure and unstable behavior.

```c
// OLD
.stack_size = 128 * 4,

// CORRECT
.stack_size = 1024 * 4,
```

### 3) LwIP uses persisted config instead of hardcoded IP
Where: [LWIP/App/lwip.c](LWIP/App/lwip.c#L67)
Why: Hardcoded static IP blocked persistence behavior.

```c
// OLD
IP4_ADDR(&ipaddr, 192, 168, 1, 222);
IP4_ADDR(&netmask, 255, 255, 255, 0);
IP4_ADDR(&gw, 192, 168, 1, 1);

// CORRECT
IP4_ADDR(&ipaddr,
             g_network_config.ip[0], g_network_config.ip[1],
             g_network_config.ip[2], g_network_config.ip[3]);
IP4_ADDR(&netmask,
             g_network_config.netmask[0], g_network_config.netmask[1],
             g_network_config.netmask[2], g_network_config.netmask[3]);
IP4_ADDR(&gw,
             g_network_config.gateway[0], g_network_config.gateway[1],
             g_network_config.gateway[2], g_network_config.gateway[3]);
```

### 4) HTTP request buffering hardened for POST body
Where: [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c#L33)
Why: POST parsing failed when request body arrived in multiple TCP segments.

```c
// OLD
if ((buflen >= 17) && (strncmp(buf, "POST /save_config", 17) == 0)) {
   network_config_http_handle_save(conn, buf, buflen);
}

// CORRECT
static char g_http_request_buffer[4096];
static int parse_content_length(const char *request) { ... }

if ((request_len >= 17) &&
      (strncmp(g_http_request_buffer, "POST /save_config", 17) == 0)) {
   content_len = parse_content_length(g_http_request_buffer);
   /* keep receiving chunks until full body length is present */
   network_config_http_handle_save(conn, g_http_request_buffer, request_len);
}
```

### 5) netbuf cleanup made safe
Where: [Core/Src/httpserver-netconn.c](Core/Src/httpserver-netconn.c)
Why: Avoid deleting invalid/null buffer on receive failure.

```c
// OLD
struct netbuf *inbuf;
...
netbuf_delete(inbuf);

// CORRECT
struct netbuf *inbuf = NULL;
...
if (inbuf != NULL) {
   netbuf_delete(inbuf);
}
```

### 6) URL decode + whitespace trim + robust IPv4 parse
Where: [Core/Src/network_config_http.c](Core/Src/network_config_http.c#L78)
Why: Values looked valid to user but parser rejected due to formatting/encoding.

```c
// OLD
/* no URL decode, no trim around parsed values */

// CORRECT
static void url_decode_inplace(char *text) { ... }

static int parse_ipv4(const char *text, uint8_t out[4]) {
   while ((*p == ' ') || (*p == '\t')) { p++; }
   ...
   while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) { p++; }
   return (*p == '\0') ? 1 : 0;
}
```

### 7) Config page buffer moved to static storage
Where: [Core/Src/network_config_http.c](Core/Src/network_config_http.c#L11)
Why: Keep large HTML generation buffer off task stack.

```c
// OLD
char page[1200];

// CORRECT
static char g_config_page_buffer[1400];
```

### 8) Config form can show live netif values
Where: [Core/Src/network_config_http.c](Core/Src/network_config_http.c#L193), [LWIP/App/lwip.h](LWIP/App/lwip.h#L50)
Why: UI confusion when saved values differed from active runtime interface.

```c
// OLD
/* always rendered from g_network_config only */

// CORRECT
if (netif_is_up(&gnetif) && netif_is_link_up(&gnetif)) {
   disp_ip[0] = (uint8_t)ip4_addr1_16(netif_ip4_addr(&gnetif));
   ...
}
```

### 9) network_config validity policy simplified
Where: [Core/Src/network_config.c](Core/Src/network_config.c#L81)
Why: Prevent false invalid-reject loops from overly strict checks during bring-up.

```c
// OLD
/* strict validation path that rejected some intended values during iteration */

// CORRECT
bool network_config_is_valid(const network_config_t *cfg) {
   if (cfg == NULL) {
      return false;
   }
   return cfg->magic == NETWORK_CONFIG_MAGIC;
}
```

### 10) EEPROM emulation mapping corrected
Where: [Core/Inc/ee_config.h](Core/Inc/ee_config.h#L50)
Why: Wrong bank/sector/address makes save/load appear broken.

```c
// OLD
#define EE_SELECTED_PAGE_SECTOR_NUMBER    23
#define EE_SELECTED_BANK                  FLASH_BANK_2
#define EE_SELECTED_ADDRESS               0x081E0000

// CORRECT
#define EE_SELECTED_PAGE_SECTOR_NUMBER    11
#define EE_SELECTED_PAGE_SECTOR_SIZE      EE_PAGE_SECTOR_SIZE_128K
#define EE_SELECTED_BANK                  FLASH_BANK_1
#define EE_SELECTED_ADDRESS               0x080E0000
```

### 11) Ethernet thread stack + stable MAC storage
Where: [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c#L46), [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c#L105)
Why: Reduced runtime instability under load and ensured stable MAC pointer lifetime.

```c
// OLD
#define INTERFACE_THREAD_STACK_SIZE ( 350 )
uint8_t MACAddr[6];

// CORRECT
#define INTERFACE_THREAD_STACK_SIZE ( 1024 )
static uint8_t g_eth_mac_addr[6] = {0x02, 0x80, 0xE1, 0x42, 0x00, 0x01};
```

### 12) PHY transient down debounce
Where: [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c#L798)
Why: Prevent false link-down on temporary PHY read glitches.

```c
// OLD
/* link could drop on a single transient down status */

// CORRECT
uint32_t down_confirm_count = 0U;
...
down_confirm_count++;
if (down_confirm_count >= 3U) {
   HAL_ETH_Stop_IT(&heth);
   netif_set_down(netif);
   netif_set_link_down(netif);
}
```

### 13) lwIP default thread stack increased
Where: [LWIP/Target/lwipopts.h](LWIP/Target/lwipopts.h#L87)
Why: Added stack headroom for HTTP + netconn processing.

```c
// OLD
#define DEFAULT_THREAD_STACKSIZE 1024

// CORRECT
#define DEFAULT_THREAD_STACKSIZE 2048
```

### 14) Build lists updated for new modules
Where: [Debug/Core/Src/subdir.mk](Debug/Core/Src/subdir.mk#L12), [Debug/objects.list](Debug/objects.list#L8)
Why: Ensures new sources are compiled and linked.

```make
# OLD
# network_config.o and network_config_http.o missing

# CORRECT
../Core/Src/network_config.c \
../Core/Src/network_config_http.c \

"./Core/Src/network_config.o"
"./Core/Src/network_config_http.o"
```

<div style="padding:12px 14px;border-left:6px solid #1d4ed8;background:#edf4ff;margin-top:14px;">
<strong>Result:</strong> This set of changes addresses layered failures across startup order, parsing robustness, stack sizing, PHY behavior, and flash mapping.
</div>
