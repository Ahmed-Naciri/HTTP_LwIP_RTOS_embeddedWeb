/*
 * modbus_rtu_master.h
 *
 *  Created on: Apr 10, 2026
 *      Author: bahmdan
 */

#ifndef INC_MODBUS_RTU_MASTER_H_
#define INC_MODBUS_RTU_MASTER_H_

#define MODBUS_MAX_RESPONSE_LENGHT 32

#include "main.h"


typedef enum
{
	MODBUS_MASTER_IDLE = 0,
	MODBUS_MASTER_WAITING_RESPONSE,
	MODBUS_MASTER_RESPONSE_READY,
	MODBUS_MASTER_ERROR,
	MODBUS_MASTER_TIMEOUT

}modbusMasterStatus_t ;

void modbusMaster_init(void);
HAL_StatusTypeDef modbusMaster_readHoldingRegister(uint8_t slaveAddress ,uint16_t firstRegister,uint16_t registerCount );
void modbusMaster_process(void);
modbusMasterStatus_t modbusMaster_GetState(void);
HAL_StatusTypeDef modbusMaster_GetLastRegisterValue(uint16_t* registerValue);
void modbusMaster_cpltCallBack(UART_HandleTypeDef* huart);
/* Reset the Modbus master state after the UART settings change. 
 * Resets Modbus internal state to avoid staying in WAITING_RESPONSE with old timing.
 */
void modbusMaster_onUartReconfig(void);

// void modbusMaster_tickTimeout(void);
// void modbusMaster_checkTimeout(void);


#endif /* INC_MODBUS_RTU_MASTER_H_ */
