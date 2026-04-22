#include "network_config.h"

#include "ee.h"
#include <string.h>

network_config_t g_network_config;

static bool g_ee_ready = false;

static bool network_config_ee_init(void)
{
  if (!g_ee_ready) {
    g_ee_ready = ee_init(&g_network_config, sizeof(g_network_config));
  }
  return g_ee_ready;
}

static uint32_t ip_to_u32(const uint8_t ip[4])
{
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

static bool is_valid_netmask(const uint8_t netmask[4])
{
  uint32_t m = ip_to_u32(netmask);
  uint32_t inv;

  if ((m == 0u) || (m == 0xFFFFFFFFu)) {
    return false;
  }

  inv = ~m;
  return (inv & (inv + 1u)) == 0u;
}

static bool is_valid_host_ip(const uint8_t ip[4], const uint8_t netmask[4])
{
  uint32_t ip_u32 = ip_to_u32(ip);
  uint32_t mask_u32 = ip_to_u32(netmask);
  uint32_t host_part = ip_u32 & ~mask_u32;

  if ((ip_u32 == 0u) || (ip_u32 == 0xFFFFFFFFu)) {
    return false;
  }

  if ((ip[0] >= 224u) && (ip[0] <= 239u)) {
    return false;
  }

  if ((host_part == 0u) || (host_part == (~mask_u32))) {
    return false;
  }

  return true;
}

void network_config_set_defaults(network_config_t *cfg)
{
  if (cfg == NULL) {
    return;
  }

  cfg->magic = NETWORK_CONFIG_MAGIC;

  cfg->ip[0] = 192u;
  cfg->ip[1] = 168u;
  cfg->ip[2] = 1u;
  cfg->ip[3] = 222u;

  cfg->netmask[0] = 255u;
  cfg->netmask[1] = 255u;
  cfg->netmask[2] = 255u;
  cfg->netmask[3] = 0u;

  cfg->gateway[0] = 192u;
  cfg->gateway[1] = 168u;
  cfg->gateway[2] = 1u;
  cfg->gateway[3] = 1u;
}

bool network_config_is_valid(const network_config_t *cfg)
{
  if (cfg == NULL) {
    return false;
  }

  return cfg->magic == NETWORK_CONFIG_MAGIC;
}

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
