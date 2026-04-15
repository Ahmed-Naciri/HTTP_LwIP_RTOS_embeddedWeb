/*
 * app_config.h
 *
 *  Created on: Apr 15, 2026
 *      Author: bahmdan
 */

#ifndef INC_APP_CONFIG_H_
#define INC_APP_CONFIG_H_

#define MAX_UART_PORTS 1
#define MAX_SLAVES 255
#define MAX_REGISTERS_PER_SLAVE 1024


typedef enum
{
 USART_PORT_3 = 0

}uartPortId_t;

typedef enum
{
  PARITY_NONE = 0,
  PARITY_EVEN,
  PARITY_ODD

}parityType_t;


typedef enum
{
	REG_TYPE_U16,
	REG_TYPE_I16,
	REG_TYPE_FLOAT

}registerType_t;

typedef struct
{
	uint8_t used;
	uartPortId_t portId;
	uint32_t baudRate;
	uint8_t sotpBits;
	parityType_t parity;

}portConfig_t;


typedef struct
{
	uint8_t used;
	uint16_t regAddress;
	registerType_t registerType;
	float lastValue;

}registerConfig_t;




typedef struct
{
	uint8_t used;       //Indicates whether this entry is used
	uint8_t slaveAddress; //between 1..247
	uartPortId_t portId;
	uint8_t registerCount; //Number of registers currently configured for this slave
	registerConfig_t registerConfig[MAX_REGISTERS_PER_SLAVE];

}slaveConfig_t;

typedef struct
{
	portConfig_t ports[MAX_UART_PORTS];
	slaveConfig_t slaveConfig[MAX_SLAVES];

}appDataBase_t;

extern appDataBase_t appDb ;

void appConfig_defaultInit(void);

int appConfig_addSlave(uint8_t slaveAddress,uartPortId_t portId);
int appConfig_removeSlave(uint8_t slaveAddress);

int appConfig_addRegister(uint8_t slaveIndex, uint16_t regAddress,registerType_t registerType);
int appConfig_removeRegister(uint8_t slaveIndex, uint16_t regAddress);

int appConfig_updatePort(uartPortId_t portId,uint32_t baudRate,uint8_t sotpBits,parityType_t parity);













#endif /* INC_APP_CONFIG_H_ */
