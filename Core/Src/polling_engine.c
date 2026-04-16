/*
 * polling_engine.c
 *
 *  Created on: Apr 16, 2026
 *      Author: bahmdan
 */

#include "polling_engine.h"
#include "app_config.h"
#include "modbus_rtu_master.h"

static uint8_t currentSlaveIndex;
static uint16_t currentRegisterIndex;

static void pollingEngine_moveToNextRegister(void)
{
	currentRegisterIndex++;
	if(currentRegisterIndex >= MAX_REGISTERS_PER_SLAVE )
	{
		currentRegisterIndex = 0;
		currentSlaveIndex++;

		if(currentSlaveIndexc >= MAX_SLAVE)
		{
			currentSlaveIndex = 0;

		}
	}


}

void pollingEngine_init(void)
{
	 currentSlaveIndex = 0;
	 currentRegisterIndex = 0;

}

void pollingEngine_task(void)
{

  uint16_t registerValue;
  modbusMasterStatus_t modbusState;

  modbusState = modbusMaster_GetState();

  if(appDb.slaveConfig[currentSlaveIndex].used == 0)
  {
	  pollingEngine_moveToNextRegister();
	  return;
  }

  if(appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].used == 0)
  {
	  pollingEngine_moveToNextRegister();
	  return;
  }

 if(modbusState == MODBUS_MASTER_IDLE)
 {
	 modbusMaster_readHoldingRegister(
			 appDb.slaveConfig[currentSlaveIndex].slaveAddress,
			 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].regAddress,
			 1);
	 return;
 }

 if(modbusState == MODBUS_MASTER_RESPONSE_READY)
 {
	 if(modbusMaster_GetLastRegisterValue(&registerValue) == HAL_OK)
	 {
		 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].lastValue = (float)registerValue;
		 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].valid = 1;
	 }
	 else
	 {
		 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].valid = 0;
	 }
	 pollingEngine_moveToNextRegister();
	 return;

 }








}
