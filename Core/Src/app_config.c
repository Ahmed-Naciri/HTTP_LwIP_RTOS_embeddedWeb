/*
 * app_config.c
 *
 *  Created on: Apr 15, 2026
 *      Author: bahmdan
 */
#include "app_config.h"

#include "persistent_store.h"


appDataBase_t appDb;

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
			if ((db->slaveConfig[i].slaveAddress == 0u) || (db->slaveConfig[i].slaveAddress > 247u))
			{
				return false;
			}

			if ((uint32_t)db->slaveConfig[i].portId >= (uint32_t)MAX_UART_PORTS)
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

}

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

int appConfig_addSlave(uint8_t slaveAddress,uartPortId_t portId)
{
	uint8_t i;
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
			appDb.slaveConfig[i].used = 1;
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

			if (slave->registerCount > 0u)
			{
				slave->registerCount--;
			}

			return 0;
		}
	}

	return -3;
}


int appConfig_updatePort(uartPortId_t portId,uint32_t baudRate,uint8_t sotpBits,parityType_t parity)
{
	if(portId >= MAX_UART_PORTS)
	{
		return -1;
	}

	appDb.ports[portId].used = 1;
	appDb.ports[portId].portId = portId;
	appDb.ports[portId].baudRate = baudRate;
	appDb.ports[portId].stopBits = sotpBits;
	appDb.ports[portId].parity = parity;

	return 0;




}




