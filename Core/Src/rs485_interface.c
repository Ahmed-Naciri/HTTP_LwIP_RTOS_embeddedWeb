/*
 * rs485_interface.c
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */

#include "rs485_interface.h"

extern UART_HandleTypeDef huart3;

void rs485_interface_init(void)
{
    rs485_interface_enableRxMode();
}

void rs485_interface_enableTxMode(void)
{

	HAL_GPIO_WritePin(GPIO_DERICTION_PORT, GPIO_DERICTION_PIN, GPIO_PIN_SET);

}

void rs485_interface_enableRxMode(void)
{
	HAL_GPIO_WritePin(GPIO_DERICTION_PORT, GPIO_DERICTION_PIN, GPIO_PIN_RESET);
}

HAL_StatusTypeDef rs485_interface_send(uint8_t* Txbuffer, uint16_t TxbufferLength)
{
	rs485_interface_enableTxMode();
	HAL_StatusTypeDef transmissionStatus = HAL_UART_Transmit(&huart3, Txbuffer, TxbufferLength,1000);
	 rs485_interface_enableRxMode();
    return transmissionStatus;
}

HAL_StatusTypeDef rs485_interface_receive(uint8_t* Rxbuffer, uint16_t RxbufferLength)
{
  rs485_interface_enableRxMode();
  return HAL_UART_Receive_DMA(&huart3, Rxbuffer, RxbufferLength);


}


