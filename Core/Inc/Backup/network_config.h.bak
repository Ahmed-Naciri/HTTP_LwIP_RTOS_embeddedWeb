/*
 * @brief  This file manage persistent network configuration
 *
 */


#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define NETWORK_CONFIG_MAGIC 0x4E455443u

typedef struct {
  /* Valid-data marker used to detect erased/corrupted flash content on boot. */
  uint32_t magic;
  /* Static IPv4 address in 4 octets. */
  uint8_t ip[4];
  /* Static subnet mask in 4 octets. */
  uint8_t netmask[4];
  /* Static default gateway in 4 octets. */
  uint8_t gateway[4];
} network_config_t;

extern network_config_t g_network_config;

void network_config_set_defaults(network_config_t *cfg);
bool network_config_is_valid(const network_config_t *cfg);
void network_config_load(void);
bool network_config_save(const network_config_t *cfg);

#endif
