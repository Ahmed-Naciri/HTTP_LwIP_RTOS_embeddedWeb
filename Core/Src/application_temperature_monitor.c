/*
 * application_temperature_monitor.c
 *
 *  Created on: Apr 10, 2026
 *      Author: bahmdan
 */

#include "application_temperature_monitor.h"
#include "modbus_rtu_master.h"
#include "project_settings.h"

static tempMonitorData_t tempMonitorData;

void temperature_monitor_init(void)
{
	tempMonitorData.rawTemp = 0;
	tempMonitorData.tempCelsius = 0.0f;
	tempMonitorData.communicationOK = 0;
	tempMonitorData.alarmeActive = 0;

}

void temperature_monitor_task(void)
{
	uint16_t rawValue;
	projectSettings_t* projectSettings = projectSettings_get();

	if(modbusMaster_GetState() == MODBUS_MASTER_IDLE)
	{
		modbusMaster_readHoldingRegister( projectSettings->tempSlaveAddress , projectSettings->tempRegisterAddress, 1);
	}

	if(modbusMaster_GetState() == MODBUS_MASTER_RESPONSE_READY)
	{
		if(modbusMaster_GetLastRegisterValue( &rawValue) == HAL_OK)
		{
			tempMonitorData.rawTemp = 	rawValue;
			tempMonitorData.tempCelsius = rawValue / 10.0f;
			tempMonitorData.communicationOK = 1;

			if(tempMonitorData.tempCelsius > projectSettings->tempThreshold)
			{
				tempMonitorData.alarmeActive = 1;
			}
			else
			{
			    tempMonitorData.alarmeActive = 0;
			}
		}
	}
	else if((modbusMaster_GetState() == MODBUS_MASTER_ERROR) || (modbusMaster_GetState() == MODBUS_MASTER_TIMEOUT))
	{
		tempMonitorData.communicationOK = 0;
		tempMonitorData.alarmeActive = 1;
	}

}

tempMonitorData_t* temperature_monitor_getData(void)
{

   return &tempMonitorData;

}










