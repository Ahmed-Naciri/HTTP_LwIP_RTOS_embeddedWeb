/*
 * polling_engine.c
 *
 *  Created on: Apr 16, 2026
 *      Author: bahmdan
 */

#include "polling_engine.h"
#include "app_config.h"
#include "modbus_rtu_master.h"
#include <stdint.h>

static uint8_t currentSlaveIndex;
static uint16_t currentRegisterIndex;

static void pollingEngine_moveToNextRegister(void)
{
	currentRegisterIndex++;
	if(currentRegisterIndex >= MAX_REGISTERS_PER_SLAVE )
	{
		currentRegisterIndex = 0;
		currentSlaveIndex++;

		if(currentSlaveIndex >= MAX_SLAVES)
		{
			currentSlaveIndex = 0;

		}
	}


}

static void pollingEngine_updateAlarmState(registerConfig_t *registerConfig)
{
	if (registerConfig == 0)
	{
		return;
	}

	if ((registerConfig->valid == 0u) || (registerConfig->alarmEnabled == 0u))
	{
		registerConfig->alarmActive = 0u;
		return;
	}

	if (registerConfig->lastValue > registerConfig->alarmThreshold)
	{
		registerConfig->alarmActive = 1u;
	}
	else
	{
		registerConfig->alarmActive = 0u;
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
	uint16_t registerValueHi;
	uint16_t registerValueLo;
	union {
		uint32_t raw;
		float value;
	} floatConverter;
  modbusMasterStatus_t modbusState;
	uint16_t registerCount = 1u;

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

  if (appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].registerType == REG_TYPE_FLOAT)
  {
    registerCount = 2u;
  }

 if(modbusState == MODBUS_MASTER_IDLE)
 {
	 modbusMaster_readHoldingRegister(
			 appDb.slaveConfig[currentSlaveIndex].slaveAddress,
			 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].regAddress,
			 registerCount);
	 return;
 }

 if(modbusState == MODBUS_MASTER_RESPONSE_READY)
 {
	registerConfig_t *reg = &appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex];

	if (reg->registerType == REG_TYPE_FLOAT)
	{
		if (modbusMaster_GetLastRegisterValues(&registerValueHi, &registerValueLo) == HAL_OK)
		{
			floatConverter.raw = ((uint32_t)registerValueHi << 16) | (uint32_t)registerValueLo;
			reg->lastValue = floatConverter.value;
		}
		else
		{
			reg->valid = 0u;
			reg->alarmActive = 0u;
			pollingEngine_moveToNextRegister();
			return;
		}
	}
	else if(modbusMaster_GetLastRegisterValue(&registerValue) == HAL_OK)
	{
		if (reg->registerType == REG_TYPE_I16)
		{
			/* signed 16-bit */
			reg->lastValue = (float)((int16_t)registerValue);
		}
		else
		{
			/* unsigned 16-bit (default behavior) */
			reg->lastValue = (float)registerValue;
		}
	}
	else
	{
		reg->valid = 0u;
		reg->alarmActive = 0u;
		pollingEngine_moveToNextRegister();
		return;
	}

	reg->valid = 1;
	pollingEngine_updateAlarmState(reg);

	 pollingEngine_moveToNextRegister();
	 return;

 }

 if ((modbusState == MODBUS_MASTER_ERROR) || (modbusState == MODBUS_MASTER_TIMEOUT))
 {
	 /* Recover from a failed transaction so polling does not stay blocked forever. */
	 modbusMaster_init();
	 pollingEngine_moveToNextRegister();
	 return;
 }








}
