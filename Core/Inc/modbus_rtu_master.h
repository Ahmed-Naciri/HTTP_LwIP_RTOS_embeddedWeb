/*
 * modbus_rtu_master.h
 *
 *  Created on: Apr 10, 2026
 *      Author: bahmdan
 */

#ifndef INC_MODBUS_RTU_MASTER_H_
#define INC_MODBUS_RTU_MASTER_H_

#include "main.h"
#define MODBUS_MAX_RESPONSE_LENGHT 32

typedef enum
{
	MODBUS_MASTER_IDEL = 0,
	MODBUS_MASTER_WAITING_RESPONSE,
	MODBUS_MASTER_RESPONSE_READY,
	MODBUS_MASTER_ERROR,
	MODBUS_MASTER_TIMEOUT

}modbusMasterStatus_t ;

void modbus_master_init(void);
HAL_StatusTypeDef readHoldingRegister(uint8_t slaveAddress ,uint16_t firstRegister,uinit16_t registerCount );
void modbus_master_process(void);
modbusMasterStatus_t modbus_master_GetSate(void);
HAL_StatusTypeDef modbus_master_GetLastRegisterValue(uint8_t* registerValue);
void modbus_master_cpltCallBack(UART_HandleTypeDef* huart);


#endif /* INC_MODBUS_RTU_MASTER_H_ */
