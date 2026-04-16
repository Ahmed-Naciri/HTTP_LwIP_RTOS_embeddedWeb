/*
 * rs485_interface.h
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */
#ifndef INC_RS485_INTERFACE_H_
#define INC_RS485_INTERFACE_H_

#include "main.h"

#define GPIO_DERICTION_PORT GPIOD
#define GPIO_DERICTION_PIN GPIO_PIN_12

void rs485_interface_init(void);
void rs485_interface_enableTxMode(void);
void rs485_interface_enableRxMode(void);


HAL_StatusTypeDef rs485_interface_send(uint8_t* Txbuffer, uint16_t TxbufferLength);
HAL_StatusTypeDef rs485_interface_receive(uint8_t* Rxbuffer, uint16_t RxbufferLength);


#endif /* INC_RS485_INTERFACE_H_ */
