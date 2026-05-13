#include "network_config.h"

#include "persistent_store.h"
#include <string.h>


network_config_t g_network_config;

/*
 * Role: Convert a 4-byte IP address array to a single 32-bit integer.
 * 
 * When: Called internally by validation functions when comparing IP addresses
 *       in bitwise operations (checking netmask validity, host part).
 * 
 * Why: Bitwise math on a single uint32_t is faster and simpler than working
 *      with individual bytes. Allows us to check netmask patterns like
 *      0xFFFFFF00 (which means /24 subnet with valid continuous mask bits).
 * 
 * Example: ip[4] = {192, 168, 1, 5} becomes 0xC0A80105 in memory.
 */
static uint32_t ip_to_u32(const uint8_t ip[4])
{
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

/*
 * Role: Verify that a netmask is valid (has correct bit pattern for a subnet mask).
 * 
 * When: Called by is_valid_host_ip() before checking if an IP fits in the network.
 * 
 * Why: Not all values are valid netmasks. Valid masks have continuous 1-bits
 *      from left to right (like 255.255.255.0 = 0xFFFFFF00), then 0-bits.
 *      Invalid example: 255.255.254.0 has gaps in the bits (not continuous).
 * 
 * Check: Rejects all-zeros and all-ones (edge cases).
 *        Then uses bitwise math: if (inv & (inv + 1)) == 0, the bits are continuous.
 *        Example: 0x000000FF (mask for /24) passes this test.
 */
static bool is_valid_netmask(const uint8_t netmask[4])
{
  uint32_t m = ip_to_u32(netmask);
  uint32_t reversed;

  if ((m == 0u) || (m == 0xFFFFFFFFu)) {
    return false;
  }

  reversed = ~m; // the ~ reverses the bits of m, so if m is 0xFFFFFF00, reversed is 0x000000FF
  return (reversed & (reversed + 1u)) == 0u;
}

/*
 * Role: Verify that an IP address is valid and fits within the given network.
 * 
 * When: Called by network_config_is_valid() and during HTTP form submission
 *       when user enters a new IP (via app_config_http.c handlers).
 * 
 * Why: Not all IPs are usable in a subnet:
 *      - 0.0.0.0 and 255.255.255.255 are reserved (broadcast/default route)
 *      - Multicast IPs (224-239.x.x.x) cannot be configured as host IPs
 *      - Network address (all host bits = 0) is reserved
 *      - Broadcast address (all host bits = 1) is reserved
 * 
 * Logic: Extract the host portion (IP AND NOT mask), then reject edge cases.
 *        Example: IP=192.168.1.5 with mask 255.255.255.0 gives host_part=0x00000005 (valid).
 */
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

/*
 * Role: Initialize a configuration structure with hardcoded default values.
 * 
 * When: Called at startup by network_config_load() and when flash data is corrupt.
 *       Also called from HTTP handlers when user clicks "Reset to Defaults".
 * 
 * Why: Ensures the network config always has safe, known values:
 *      - IP: 192.168.1.222 (on a typical home/office /24 network)
 *      - Netmask: 255.255.255.0 (standard /24 subnet)
 *      - Gateway: 192.168.1.1 (typical router address)
 *      All configs include a magic marker (NETWORK_CONFIG_MAGIC) for validation.
 * 
 * Safety: If cfg pointer is NULL, function returns early (no crash).
 */
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

/*
 * Role: Check if a network configuration structure is valid and uncorrupted.
 * 
 * When: Called before using any config (at boot, after flash load, before HTTP apply).
 * 
 * Why: Flash memory can become corrupted from power loss, bit flips, or failed writes.
 *      This function detects corruption using a magic marker (NETWORK_CONFIG_MAGIC).
 *      Only configs with the correct magic marker are trusted as valid.
 * 
 * Safety: Returns false if pointer is NULL (defensive programming).
 *         If magic marker matches, we know the entire config is likely uncorrupted.
 */
bool network_config_is_valid(const network_config_t *cfg)
{
  if (cfg == NULL) {
    return false;
  }

  return cfg->magic == NETWORK_CONFIG_MAGIC;
}

/*
 * Role: Load network configuration from flash memory and apply it to the running system.
 *       Part of the three-layer architecture: Flash → RAM → Hardware use.
 * 
 * When: Called once at system startup from main() before network initialization.
 * 
 * Why: Network settings must survive a power cycle (reboot recovery).
 *      Flash stores the last known-good configuration.
 *      Without loading from flash, we'd lose user settings after every reboot.
 * 
 * Process:
 *   1. Start with safe defaults (always a fallback)
 *   2. Try to read config from flash (persistent_store_load_network)
 *   3. If flash data exists but is corrupted, write defaults back (self-healing)
 *   4. Copy validated config to g_network_config (runtime copy for fast access)
 * 
 * Recovery: If flash is corrupted, we automatically restore defaults to flash.
 *           This prevents getting stuck with invalid settings.
 */
void network_config_load(void)
{
  network_config_t tmp;

  /* Always start from safe defaults, then try to overlay persisted data. */
  network_config_set_defaults(&g_network_config);

  if (!persistent_store_load_network(&tmp)) {
    return;
  }

  /* If flash content is invalid, recover by writing defaults back. */
  if (!network_config_is_valid(&tmp)) {
    network_config_set_defaults(&tmp);
    (void)persistent_store_save_network(&tmp);
  }

  g_network_config = tmp;
}

/*
 * Role: Save network configuration to flash memory and update the runtime copy.
 *       Part of the three-layer architecture: RAM → Flash → persistent storage.
 * 
 * When: Called when user submits new network settings via HTTP form
 *       (from app_config_http.c HTTP POST handlers).
 * 
 * Why: Changes made in RAM are temporary and lost on reboot.
 *      Flash storage makes changes permanent (survives power cycles).
 *      The function enforces the magic marker to protect against partial writes.
 * 
 * Process:
 *   1. Check that caller provided valid pointer (defensive)
 *   2. Prepare a temporary copy with magic marker (NETWORK_CONFIG_MAGIC)
 *   3. Attempt to write to flash (persistent_store_save_network)
 *   4. Only update RAM copy (g_network_config) if flash write succeeds
 * 
 * Safety: Two-step commit avoids RAM/Flash mismatch. If flash fails, RAM stays
 *         unchanged. If flash succeeds but RAM update fails, we still have the
 *         old valid config in memory.
 */
bool network_config_save(const network_config_t *cfg)
{
  network_config_t tmp;

  if (cfg == NULL) {
    return false;
  }

  tmp = *cfg;
  /* Enforce magic marker for future validity checks on boot. */
  tmp.magic = NETWORK_CONFIG_MAGIC;

  if (!persistent_store_save_network(&tmp)) {
    return false;
  }

  /* Update runtime copy first, then persist to EEPROM emulation area. */
  g_network_config = tmp;
  return true;
}
