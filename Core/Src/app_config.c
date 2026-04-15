/*
 * app_config.c
 *
 *  Created on: Apr 15, 2026
 *      Author: bahmdan
 */
#include "app_config.h"


appDataBase_t appDb;

void appConfig_defaultInit(void)
{
	uint8_t i,j;

	for( i = 0; i < MAX_UART_PORTS; i++)
	{
		appDb.ports[i].used = 0;
		appDb.ports[i].portId = USART_PORT_3;
		appDb.ports[i].baudRate = 9600;
		appDb.ports[i].sotpBits = 1;
		appDb.ports[i].parity = PARITY_NONE;
	}

	for(i = 0; i < MAX_SLAVES ; i++)
	{
		appDb.slaveConfig[i].used = 0;
		appDb.slaveConfig[i].slaveAddress = 0;
		appDb.slaveConfig[i].portId = USART_PORT_3;
		appDb.slaveConfig[i].registerCount = 0;

	  for(j = 0 ; j < MAX_REGISTERS_PER_SLAVE ; j++)
	  {
		appDb.slaveConfig[i].registerConfig[j].used = 0;
		appDb.slaveConfig[i].registerConfig[j].regAddress = 0;
		appDb.slaveConfig[i].registerConfig[j].registerType = REG_TYPE_U16;
		appDb.slaveConfig[i].registerConfig[j].lastValue = 0;

	  }

	}

}

int appConfig_addSlave(uint8_t slaveAddress,uartPortId_t portId)
{
	uint8_t i;
	if(slaveAddress == 0 || slaveAddress > 255 )
	{
		return -1;
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
			appDb.slaveConfig[i].registerCount = 0;//slave has 0 register in the moment of his creation

			return i; // return the index of the created salve
		}
	}

	return -3; // means there is no place to add a new slave, the array is full

}

int appConfig_addRegister(uint8_t slaveIndex, uint16_t regAddress,registerType_t registerType)
{

	uint8_t i ;
	slaveConfig_t* slave;

	if(slaveIndex  >= MAX_SLAVES)
	{
		return -1;
	}

	slave = &appDb.slaveConfig[slaveIndex];

	if(slave->used == 0)
	{
		return -2;
	}

	for(i = 0; i < MAX_REGISTERS_PER_SLAVE; i++)
	{
		if((slave->registerConfig[i].used == 1)&&
		   (slave->registerConfig.regAddress == regAddress))
		{
			return -3;
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
			slave->registerCount++;
			return i;
		}
	}

	return -4;

}







