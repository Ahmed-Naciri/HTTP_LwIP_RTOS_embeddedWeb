#include "persistent_store.h"

#include "ee.h"

#define PERSISTENT_STORE_MAGIC 0x50435354u
#define APP_CONFIG_MAGIC 0x41505043u

/* One Flash-backed blob keeps both the network and Modbus runtime data. */
typedef struct {
  uint32_t magic;
  network_config_t network;
  uint32_t app_magic;
  appDataBase_t app;
} persistent_store_data_t;

static persistent_store_data_t g_store;
static bool g_store_ready = false;

/* Fill the persistent blob with safe default values. */
static void persistent_store_set_defaults(persistent_store_data_t *store)
{
  if (store == NULL) {
    return;
  }

  store->magic = PERSISTENT_STORE_MAGIC;
  network_config_set_defaults(&store->network);
  appConfig_setDefaults(&store->app);
  store->app_magic = APP_CONFIG_MAGIC;
}

/* Load the Flash blob once, validate it, and repair defaults if needed. */
static bool persistent_store_ensure_loaded(void)
{
  bool dirty = false;

  if (g_store_ready) {
    return true;
  }

  if (!ee_init(&g_store, sizeof(g_store))) {
    return false;
  }

  ee_read();

  if (g_store.magic != PERSISTENT_STORE_MAGIC) {
    persistent_store_set_defaults(&g_store);
    dirty = true;
  } else {
    if (!network_config_is_valid(&g_store.network)) {
      network_config_set_defaults(&g_store.network);
      dirty = true;
    }

    if ((g_store.app_magic != APP_CONFIG_MAGIC) || (!appConfig_isValid(&g_store.app))) {
      appConfig_setDefaults(&g_store.app);
      g_store.app_magic = APP_CONFIG_MAGIC;
      dirty = true;
    }
  }

  if (dirty) {
    if (!ee_write()) {
      return false;
    }
  }

  g_store_ready = true;
  return true;
}

/* Copy the persisted network settings into the caller-owned structure. */
bool persistent_store_load_network(network_config_t *cfg)
{
  if (cfg == NULL) {
    return false;
  }

  if (!persistent_store_ensure_loaded()) {
    return false;
  }

  *cfg = g_store.network;
  return true;
}

/* Update the persisted network settings and commit them to Flash. */
bool persistent_store_save_network(const network_config_t *cfg)
{
  if (cfg == NULL) {
    return false;
  }

  if (!persistent_store_ensure_loaded()) {
    return false;
  }

  g_store.network = *cfg;
  g_store.network.magic = NETWORK_CONFIG_MAGIC;

  if (!ee_write()) {
    return false;
  }

  return true;
}

/* Copy the persisted Modbus database into the caller-owned structure. */
bool persistent_store_load_app(appDataBase_t *db)
{
  if (db == NULL) {
    return false;
  }

  if (!persistent_store_ensure_loaded()) {
    return false;
  }

  *db = g_store.app;
  return true;
}

/* Update the persisted Modbus database and commit it to Flash. */
bool persistent_store_save_app(const appDataBase_t *db)
{
  if (db == NULL) {
    return false;
  }

  if (!persistent_store_ensure_loaded()) {
    return false;
  }

  g_store.app = *db;
  g_store.app_magic = APP_CONFIG_MAGIC;

  if (!ee_write()) {
    return false;
  }

  return true;
}
