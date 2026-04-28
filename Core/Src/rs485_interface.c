/*
 * rs485_interface.c
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */

#include "rs485_interface.h"

extern UART_HandleTypeDef huart3;

//HAL_StatusTypeDef rs485_interface_apply_config(uint32_t baudRate, uint8_t stopBits,parityType_t parity)
//{
//	UART_InitTypeDef uartInit;
//	HAL_StatusTypeDef status;
//
//	if ((baudRate == 0u) || ((stopBits != 1u) && (stopBits != 2u)) ||
//		(parity > PARITY_ODD))
//	{
//		return HAL_ERROR;
//	}
//
//	uartInit = huart3.Init;
//	uartInit.BaudRate = baudRate;
//	uartInit.StopBits = (stopBits == 2u) ? UART_STOPBITS_2 : UART_STOPBITS_1;
//	uartInit.Parity = (parity == PARITY_NONE) ? UART_PARITY_NONE :
//					  ((parity == PARITY_EVEN) ? UART_PARITY_EVEN : UART_PARITY_ODD);
//	uartInit.WordLength = (parity == PARITY_NONE) ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
//
//	(void)HAL_UART_Abort(&huart3);
//	status = HAL_UART_DeInit(&huart3);
//	if (status != HAL_OK)
//	{
//		return status;
//	}
//
//	huart3.Init = uartInit;
//	status = HAL_UART_Init(&huart3);
//	if (status != HAL_OK)
//	{
//		return status;
//	}
//
//	rs485_interface_enableRxMode();
//	return HAL_OK;
//}

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


