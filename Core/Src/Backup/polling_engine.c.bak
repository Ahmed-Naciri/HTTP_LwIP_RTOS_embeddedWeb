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
		registerConfig_t *reg = &appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex];

		/* Interpret raw 16-bit value according to the register type. */
		switch (reg->registerType)
		{
			case REG_TYPE_I16:
				/* signed 16-bit */
				reg->lastValue = (float)((int16_t)registerValue);
				break;

			case REG_TYPE_U16:
			default:
				/* unsigned 16-bit (default behavior) */
				reg->lastValue = (float)registerValue;
				break;
		}

		reg->valid = 1;
		pollingEngine_updateAlarmState(reg);

		/* Persist signed registers after a successful read so the last value survives reboot. */
		if (reg->registerType == REG_TYPE_I16)
		{
			(void)appConfig_save();
		}
	}
	 else
	 {
		 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].valid = 0;
		 appDb.slaveConfig[currentSlaveIndex].registerConfig[currentRegisterIndex].alarmActive = 0u;
	 }
	 pollingEngine_moveToNextRegister();
	 return;

 }








}
