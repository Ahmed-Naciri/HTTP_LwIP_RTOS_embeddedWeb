#ifndef PERSISTENT_STORE_H
#define PERSISTENT_STORE_H

#include <stdbool.h>

#include "app_config.h"
#include "network_config.h"

bool persistent_store_load_network(network_config_t *cfg);
bool persistent_store_save_network(const network_config_t *cfg);

bool persistent_store_load_app(appDataBase_t *db);
bool persistent_store_save_app(const appDataBase_t *db);

#endif
