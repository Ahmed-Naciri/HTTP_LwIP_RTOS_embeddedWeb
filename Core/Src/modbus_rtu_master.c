/*
 * modbus_rtu_master.c
 *
 *  Created on: Apr 10, 2026
 *      Author: bahmdan
 */

#include "project_settings.h"
#include "rs485_interface.h"
#include "modbus_rtu_master.h"
#include <string.h>

static uint8_t modbusMasterRequest [8];
static uint8_t modbusMasterResponse[MODBUS_MAX_RESPONSE_LENGHT];
static modbusMasterStatus_t modbusMasterStatus = MODBUS_MASTER_IDLE;
static uint16_t modbusExpectedResponseLength = 0;
static uint16_t modbusLastRegisterValue = 0;

extern UART_HandleTypeDef huart3;


static uint16_t modbusMaster_Crc16(uint8_t* frameBuffer, uint16_t frameLength)
{

	uint16_t crcValue=0xFFFF;

	for (uint16_t position=0;position<frameLength;position++)
	{
	crcValue^=frameBuffer[position];

	  for (uint8_t bitIndex=0;bitIndex<8;bitIndex++)
	  {

	    if (crcValue&0x0001)
	    {
	      crcValue>>=1;
	      crcValue^=0xA001;
	    }
	    else
	    {
	      crcValue>>=1;
	    }
	  }
	 }

	return crcValue;
}



void modbusMaster_init(void)
{
	modbusMasterStatus = MODBUS_MASTER_IDLE;
	modbusExpectedResponseLength=0;
	modbusLastRegisterValue =0 ;

	memset(modbusMasterRequest,0,sizeof(modbusMasterRequest));
	memset(modbusMasterResponse,0,sizeof(modbusMasterResponse));

}


HAL_StatusTypeDef modbusMaster_readHoldingRegister(uint8_t slaveAddress ,uint16_t firstRegister,uint16_t registerCount )
{

	uint16_t crcValue;

	if(modbusMasterStatus == MODBUS_MASTER_WAITING_RESPONSE)
	{
		return HAL_BUSY;
	}

	if(registerCount == 0)
	{
		return HAL_ERROR;
	}

	if((5 + (2*registerCount)) > MODBUS_MAX_RESPONSE_LENGHT)
	{
		return HAL_ERROR;
	}


	modbusMasterRequest[0] = slaveAddress;
    modbusMasterRequest[1] = 0x03;
    modbusMasterRequest[2] = (firstRegister >> 8) & 0xff;
    modbusMasterRequest[3] = firstRegister & 0xff;
    modbusMasterRequest[4] = (registerCount >> 8) & 0xff;
    modbusMasterRequest[5] = registerCount & 0xff;

    crcValue = modbusMaster_Crc16( modbusMasterRequest, 6);
    modbusMasterRequest[6] =  crcValue & 0xff;
    modbusMasterRequest[7] = (crcValue >> 8) & 0xff;

    modbusExpectedResponseLength=5+ (2*registerCount);


    memset(modbusMasterResponse,0,sizeof(modbusMasterResponse));

    if(rs485_interface_send( modbusMasterRequest,  8)!= HAL_OK)
    {
    	modbusMasterStatus = MODBUS_MASTER_ERROR;
    	return HAL_ERROR;
    }

    if(rs485_interface_receive(modbusMasterResponse, modbusExpectedResponseLength) !=HAL_OK)
    {
    	modbusMasterStatus = MODBUS_MASTER_ERROR;
    	return HAL_ERROR;
    }

    modbusMasterStatus = MODBUS_MASTER_WAITING_RESPONSE;

    return HAL_OK;

}


modbusMasterStatus_t modbusMaster_GetState(void)
{
  return modbusMasterStatus;
}



HAL_StatusTypeDef modbusMaster_GetLastRegisterValue(uint8_t* registerValue)
{

	if(registerValue == NULL)
	{
		return HAL_ERROR;
	}
	if(modbusMasterStatus != MODBUS_MASTER_RESPONSE_READY)
	{
		return HAL_ERROR;
	}

	*registerValue = modbusLastRegisterValue;
	modbusMasterStatus = MODBUS_MASTER_IDLE;

	return HAL_OK;

}

void modbusMaster_cpltCallBack(UART_HandleTypeDef* huart)
{
  uint16_t calculatedCrc;
  uint16_t receivedCrc;

  if(huart -> Instance != USART3)
  {
	  return;
  }

  if(modbusMasterStatus != MODBUS_MASTER_WAITING_RESPONSE)
  {
      return;
  }

  calculatedCrc = modbusMaster_Crc16( modbusMasterResponse, modbusExpectedResponseLength - 2 );
  receivedCrc = (uint16_t)modbusMasterResponse[modbusExpectedResponseLength - 2] | ((uint16_t)modbusMasterResponse[modbusExpectedResponseLength - 1]<<8);

  if(calculatedCrc != receivedCrc)
  {
	  modbusMasterStatus = MODBUS_MASTER_ERROR;
	  return;
  }
  modbusLastRegisterValue = ((uint16_t)modbusMasterResponse[3] << 8) | modbusMasterResponse[4];
  modbusMasterStatus = MODBUS_MASTER_RESPONSE_READY;



}













