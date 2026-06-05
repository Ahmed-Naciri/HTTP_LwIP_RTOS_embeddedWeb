/*
 * app_config.c
 *
 *  Created on: Apr 15, 2026
 *      Author: bahmdan
 */
#include "app_config.h"

#include <stdio.h>
#include <string.h>

/* The master must be reset when the UART changes. */
#include "modbus_rtu_master.h"
/* The UART layer applies the real hardware settings and can roll them back. */
#include "rs485_interface.h"
#include "persistent_store.h"


appDataBase_t appDb;

static void appConfig_setSlaveName(slaveConfig_t *slave, uint8_t fallbackIndex)
{
	if (slave == 0) {
		return;
	}

	if ((memchr(slave->slaveName, '\0', MAX_SLAVE_NAME_LEN) == 0) ||
		(slave->slaveName[0] == '\0') || (slave->slaveName[0] == '\xff')) {
		memset(slave->slaveName, 0, MAX_SLAVE_NAME_LEN);
		(void)snprintf(slave->slaveName, MAX_SLAVE_NAME_LEN, "Slave %u", (unsigned int)(fallbackIndex + 1u));
	}
}

static int appConfig_isSlaveNameUnique(const appDataBase_t *db, const char *name, uint8_t selfIndex)
{
	uint8_t i;

	if ((db == 0) || (name == 0) || (name[0] == '\0')) {
		return 0;
	}

	for (i = 0; i < MAX_SLAVES; i++) {
		if ((i != selfIndex) && (db->slaveConfig[i].used == 1u) &&
			(memchr(db->slaveConfig[i].slaveName, '\0', MAX_SLAVE_NAME_LEN) != 0) &&
			(strcmp(db->slaveConfig[i].slaveName, name) == 0)) {
			return 0;
		}
	}

	return 1;
}

void appConfig_setDefaults(appDataBase_t* db)
{
	uint8_t i,j;

	if (db == 0)
	{
		return;
	}

	for( i = 0; i < MAX_UART_PORTS; i++)
	{
		db->ports[i].used = 0;
		db->ports[i].portId = USART_PORT_3;
		db->ports[i].baudRate = 9600;
		db->ports[i].stopBits = 1;
		db->ports[i].parity = PARITY_NONE;
	}

	for(i = 0; i < MAX_SLAVES ; i++)
	{
		db->slaveConfig[i].used = 0;
		memset(db->slaveConfig[i].slaveName, 0, sizeof(db->slaveConfig[i].slaveName));
		db->slaveConfig[i].slaveAddress = 0;
		db->slaveConfig[i].portId = USART_PORT_3;
		db->slaveConfig[i].registerCount = 0;

	  for(j = 0 ; j < MAX_REGISTERS_PER_SLAVE ; j++)
	  {
		db->slaveConfig[i].registerConfig[j].used = 0;
		db->slaveConfig[i].registerConfig[j].regAddress = 0;
		db->slaveConfig[i].registerConfig[j].registerType = REG_TYPE_U16;
		db->slaveConfig[i].registerConfig[j].lastValue = 0;
		db->slaveConfig[i].registerConfig[j].valid = 0;
		db->slaveConfig[i].registerConfig[j].alarmEnabled = 0;
		db->slaveConfig[i].registerConfig[j].alarmThreshold = 0.0f;
		db->slaveConfig[i].registerConfig[j].alarmActive = 0;

	  }

	}

}

void appConfig_defaultInit(void)
{
	appConfig_setDefaults(&appDb);

}

bool appConfig_isValid(const appDataBase_t* db)
{
	uint8_t i;
	uint8_t j;
	uint16_t used_registers;

	if (db == 0)
	{
		return false;
	}

	for (i = 0; i < MAX_UART_PORTS; i++)
	{
		if (db->ports[i].used > 1u)
		{
			return false;
		}
		if ((uint32_t)db->ports[i].portId >= (uint32_t)MAX_UART_PORTS)
		{
			return false;
		}
		if ((db->ports[i].stopBits != 1u) && (db->ports[i].stopBits != 2u))
		{
			return false;
		}
		if ((uint32_t)db->ports[i].parity > (uint32_t)PARITY_ODD)
		{
			return false;
		}
	}

	for (i = 0; i < MAX_SLAVES; i++)
	{
		if (db->slaveConfig[i].used > 1u)
		{
			return false;
		}

		if (db->slaveConfig[i].used == 1u)
		{
			if ((memchr(db->slaveConfig[i].slaveName, '\0', MAX_SLAVE_NAME_LEN) == 0) ||
				(db->slaveConfig[i].slaveName[0] == '\0'))
			{
				return false;
			}

			if ((db->slaveConfig[i].slaveAddress == 0u) || (db->slaveConfig[i].slaveAddress > 247u))
			{
				return false;
			}

			if ((uint32_t)db->slaveConfig[i].portId >= (uint32_t)MAX_UART_PORTS)
			{
				return false;
			}

			if (!appConfig_isSlaveNameUnique(db, db->slaveConfig[i].slaveName, i))
			{
				return false;
			}
		}

		used_registers = 0u;
		for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++)
		{
			if (db->slaveConfig[i].registerConfig[j].used > 1u)
			{
				return false;
			}

			if (db->slaveConfig[i].registerConfig[j].used == 1u)
			{
				if (db->slaveConfig[i].used == 0u)
				{
					return false;
				}

				if ((uint32_t)db->slaveConfig[i].registerConfig[j].registerType > (uint32_t)REG_TYPE_FLOAT)
				{
					return false;
				}

				if ((db->slaveConfig[i].registerConfig[j].alarmEnabled > 1u) ||
					(db->slaveConfig[i].registerConfig[j].alarmActive > 1u))
				{
					return false;
				}

				if ((db->slaveConfig[i].registerConfig[j].alarmEnabled == 1u) &&
					(db->slaveConfig[i].registerConfig[j].alarmThreshold < 0.0f))
				{
					return false;
				}

				used_registers++;
			}
		}

		if (used_registers != db->slaveConfig[i].registerCount)
		{
			return false;
		}
	}

	return true;

}

/*
 * ROLE:
 *  Load configuration from Flash into RAM, then re-apply UART settings to hardware.
 *
 * UART + FLASH FLOW:
 *  1) Start from defaults in RAM.
 *  2) Try loading saved DB from Flash.
 *  3) Validate loaded content.
 *  4) Copy valid data to appDb (RAM).
 *  5) Apply saved UART setup on real hardware so RAM and hardware match.
 */
void appConfig_load(void)
{
	appDataBase_t loaded;

	appConfig_setDefaults(&appDb);

	if (!persistent_store_load_app(&loaded))
	{
		return;
	}

	if (!appConfig_isValid(&loaded))
	{
		(void)appConfig_save();
		return;
	}

	appDb = loaded;
	for (uint8_t i = 0; i < MAX_SLAVES; i++) {
		if (appDb.slaveConfig[i].used == 1u) {
			appConfig_setSlaveName(&appDb.slaveConfig[i], i);
		}
	}
	/* Re-apply the saved UART settings so the hardware matches RAM after boot. */
	if (MAX_UART_PORTS > 0u)
	{
		(void)appConfig_updatePort(appDb.ports[0].portId, appDb.ports[0].baudRate,
							   appDb.ports[0].stopBits, appDb.ports[0].parity);
	}

}

/*
 * ROLE:
 *  Save current RAM database (appDb) into Flash.
 *
 * NOTE:
 *  Save is allowed only if appDb is valid, to avoid persisting corrupted data.
 */
int appConfig_save(void)
{
	if (!appConfig_isValid(&appDb))
	{
		return -1;
	}

	if (!persistent_store_save_app(&appDb))
	{
		return -1;
	}

	return 0;

}

int appConfig_addSlave(uint8_t slaveAddress,uartPortId_t portId, const char *slaveName)
{
	uint8_t i;
	const char *name = slaveName;
	char defaultName[MAX_SLAVE_NAME_LEN];
	if((slaveAddress == 0u) || (slaveAddress > 247u))
	{
		return -1; // invalid address
	}

	for(i = 0; i < MAX_SLAVES ; i++)
	{
		if((appDb.slaveConfig[i].used == 1) &&
		   (appDb.slaveConfig[i].slaveAddress == slaveAddress) &&
		   (appDb.slaveConfig[i].portId == portId) )
		{
			return -2; // slave exist
		}
	}

	for(i = 0; i < MAX_SLAVES ; i++)
	{
		if(appDb.slaveConfig[i].used == 0)
		{
			if ((name == 0) || (name[0] == '\0')) {
				(void)snprintf(defaultName, sizeof(defaultName), "Slave %u", (unsigned int)(slaveAddress));
				name = defaultName;
			}

			if (!appConfig_isSlaveNameUnique(&appDb, name, MAX_SLAVES))
			{
				return -4; // duplicate name
			}

			appDb.slaveConfig[i].used = 1;
			(void)snprintf(appDb.slaveConfig[i].slaveName, MAX_SLAVE_NAME_LEN, "%s", name);
			appDb.slaveConfig[i].slaveAddress = slaveAddress;
			appDb.slaveConfig[i].portId = portId;
			appDb.slaveConfig[i].registerCount = 0;//slave has 0 register in the moment of its creation

			return i; // return the index of the created salve
		}
	}

	return -3; // means there is no place to add a new slave, the array is full

}

int appConfig_addRegister(uint8_t slaveIndex, uint16_t regAddress,registerType_t registerType)
{

	uint8_t i ;
	slaveConfig_t* slave;

	if ((uint32_t)registerType > (uint32_t)REG_TYPE_FLOAT)
	{
		return -1;
	}

	if(slaveIndex  >= MAX_SLAVES)
	{
		return -1;
	}

	slave = &appDb.slaveConfig[slaveIndex];

	if(slave->used == 0)
	{
		return -2; //verify if the slave exists , to be exist it must be used(we can't create registers for a slave if the slave not exists,that why we should verify)
	}

	for(i = 0; i < MAX_REGISTERS_PER_SLAVE; i++)
	{
		if((slave->registerConfig[i].used == 1)&&
		   (slave->registerConfig[i].regAddress == regAddress))
		{
			return -3; // verify if a register of that salve  has the same regAddress and  is used
		}
	}

	for(i = 0; i < MAX_REGISTERS_PER_SLAVE; i++)
	{
		if(slave->registerConfig[i].used == 0)
		{
			slave->registerConfig[i].used = 1;
			slave->registerConfig[i].regAddress = regAddress;
			slave->registerConfig[i].registerType = registerType;
			slave->registerConfig[i].lastValue = 0.0f;
			slave->registerConfig[i].valid = 0;
				slave->registerConfig[i].alarmEnabled = 0;
				slave->registerConfig[i].alarmThreshold = 0.0f;
				slave->registerConfig[i].alarmActive = 0;
			slave->registerCount++;
			return i; // return the register index if the creation success
		}
	}

	return -4; // if it is not one of the previous cases , it means that  the array is full

}

int appConfig_removeSlave(uint8_t slaveIndex)
{
	uint8_t i;
	slaveConfig_t* slave;

	if (slaveIndex >= MAX_SLAVES)
	{
		return -1;
	}

	slave = &appDb.slaveConfig[slaveIndex];
	if (slave->used == 0u)
	{
		return -2;
	}

	for (i = 0; i < MAX_REGISTERS_PER_SLAVE; i++)
	{
		slave->registerConfig[i].used = 0u;
		slave->registerConfig[i].regAddress = 0u;
		slave->registerConfig[i].registerType = REG_TYPE_U16;
		slave->registerConfig[i].lastValue = 0.0f;
		slave->registerConfig[i].valid = 0u;
	}

	slave->used = 0u;
		memset(slave->slaveName, 0, sizeof(slave->slaveName));
	slave->slaveAddress = 0u;
	slave->portId = USART_PORT_3;
	slave->registerCount = 0u;

	return 0;
}

int appConfig_removeRegister(uint8_t slaveIndex, uint16_t regAddress)
{
	uint8_t i;
	slaveConfig_t* slave;

	if (slaveIndex >= MAX_SLAVES)
	{
		return -1;
	}

	slave = &appDb.slaveConfig[slaveIndex];
	if (slave->used == 0u)
	{
		return -2;
	}

	for (i = 0; i < MAX_REGISTERS_PER_SLAVE; i++)
	{
		if ((slave->registerConfig[i].used == 1u) &&
			(slave->registerConfig[i].regAddress == regAddress))
		{
			slave->registerConfig[i].used = 0u;
			slave->registerConfig[i].regAddress = 0u;
			slave->registerConfig[i].registerType = REG_TYPE_U16;
			slave->registerConfig[i].lastValue = 0.0f;
			slave->registerConfig[i].valid = 0u;
			slave->registerConfig[i].alarmEnabled = 0u;
			slave->registerConfig[i].alarmThreshold = 0.0f;
			slave->registerConfig[i].alarmActive = 0u;

			if (slave->registerCount > 0u)
			{
				slave->registerCount--;
			}

			return 0;
		}
	}

	return -3;
}


/*
 * ROLE:
 *  Update UART configuration safely in both hardware and RAM.
 *
 * SAFE UPDATE SEQUENCE:
 *  1) Validate new values.
 *  2) Snapshot old RAM config and old hardware UART config.
 *  3) Try hardware apply first.
 *  4) If hardware apply fails: rollback hardware + rollback RAM.
 *  5) If success: commit new values in RAM and reset Modbus state.
 *
 * IMPORTANT:
 *  This function does NOT write Flash directly.
 *  Persistence is done by calling appConfig_save() after success.
 */
int appConfig_updatePort(uartPortId_t portId, uint32_t baudRate, uint8_t stopBits, parityType_t parity)
{
	portConfig_t previousRam;
	UART_InitTypeDef previousHw;
	HAL_StatusTypeDef hwStatus;

	/* Keep only one UART in this project, so the port index must stay in range. */
	if ((portId >= MAX_UART_PORTS) || (baudRate == 0u) || (baudRate > 2000000u) ||
		((stopBits != 1u) && (stopBits != 2u)) || (parity > PARITY_ODD))
	{
		return -1;
	}

	/* Save the old RAM config in case we need to go back. */
	previousRam = appDb.ports[portId];
	/* Save the old hardware config so rollback is possible. */
	previousHw = rs485_uart_get_current_config();

	/* Try to apply the new UART settings on the real hardware first. */
	hwStatus = rs485_interface_apply_config(baudRate, stopBits, parity, 1000u);
	if (hwStatus != HAL_OK)
	{
		/* If the update fails, restore the old hardware and old RAM state. */
		(void)rs485_interface_restore_config(previousHw);
		appDb.ports[portId] = previousRam;
		return -2;
	}

	/* Mark the port as used in RAM. */
	appDb.ports[portId].used = 1u;
	/* Store the selected port index. */
	appDb.ports[portId].portId = portId;
	/* Store the new speed. */
	appDb.ports[portId].baudRate = baudRate;
	/* Store the new stop bits. */
	appDb.ports[portId].stopBits = stopBits;
	/* Store the new parity. */
	appDb.ports[portId].parity = parity;

	/* Reset the Modbus master because the serial link changed. */
	modbusMaster_onUartReconfig();

	return 0;




}




