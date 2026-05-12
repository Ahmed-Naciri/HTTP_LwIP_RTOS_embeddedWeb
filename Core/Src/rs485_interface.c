/*
 * rs485_interface.c
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */

#include "rs485_interface.h"

extern UART_HandleTypeDef huart3;

/*
 * ROLE:
 *  Take an exact snapshot of current UART hardware configuration.
 *
 * WHY:
 *  Before applying a new UART config, we keep a backup to allow rollback.
 *
 * EXAMPLE FLOW:
 *  old = rs485_uart_get_current_config();
 *  if (apply fails) rs485_interface_restore_config(old);
 */
UART_InitTypeDef rs485_uart_get_current_config(void)
{
	/* Return the current UART init structure exactly as it is now. */
	return huart3.Init;
}

/*
 * ROLE:
 *  Apply a new UART configuration to real hardware (USART3).
 *
 * WHAT IT DOES:
 *  1) Validate input values.
 *  2) Build a new HAL UART_InitTypeDef.
 *  3) Abort current UART activity.
 *  4) DeInit + Init with new settings.
 *  5) Force RS485 back to RX mode.
 *
 * RETURN:
 *  HAL_OK on success, HAL_ERROR/HAL status on failure.
 */
HAL_StatusTypeDef rs485_interface_apply_config(uint32_t baudRate, uint8_t stopBits, parityType_t parity, uint32_t timeout_ms)
{
	UART_InitTypeDef newInit;
	HAL_StatusTypeDef status;

	/* The timeout is kept in the signature for future extension, but not used yet. */
	(void)timeout_ms;

	/* Reject only the essential invalid values. */
	if ((baudRate == 0u) || (baudRate > 2000000u) || ((stopBits != 1u) && (stopBits != 2u)) ||
		(parity > PARITY_ODD))
	{
		return HAL_ERROR;
	}

	/* Start from the current UART settings and change only the needed fields. */
	newInit = huart3.Init;
	/* Update the speed. */
	newInit.BaudRate = baudRate;
	/* Convert the simple 1/2 value into the HAL constant. */
	newInit.StopBits = (stopBits == 2u) ? UART_STOPBITS_2 : UART_STOPBITS_1;
	/* Convert the simple parity enum into the HAL constant. */
	newInit.Parity = (parity == PARITY_NONE) ? UART_PARITY_NONE :
					 ((parity == PARITY_EVEN) ? UART_PARITY_EVEN : UART_PARITY_ODD);
	/* HAL usually expects 8 bits when parity is off, 9 bits when parity is enabled. */
	newInit.WordLength = (parity == PARITY_NONE) ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;

	/* Stop the current UART activity before changing the setup. */
	(void)HAL_UART_Abort(&huart3);
	/* Fully de-initialize the UART so the new setup can be applied cleanly. */
	status = HAL_UART_DeInit(&huart3);
	if (status != HAL_OK)
	{
		return status;
	}

	/* Write the new settings into the live handle. */
	huart3.Init = newInit;
	/* Re-initialize the peripheral with the new settings. */
	status = HAL_UART_Init(&huart3);
	if (status != HAL_OK)
	{
		return status;
	}

	/* Put the RS485 line back in receive mode after the update. */
	rs485_interface_enableRxMode();
	return HAL_OK;
}

/*
 * ROLE:
 *  Restore old UART hardware configuration after a failed reconfiguration.
 *
 * WHAT IT DOES:
 *  1) Abort current UART activity.
 *  2) DeInit current setup.
 *  3) Re-apply previousHw.
 *  4) Init again.
 *  5) Return to RS485 RX mode.
 *
 * NOTE:
 *  This is the rollback safety path used by appConfig_updatePort().
 */
HAL_StatusTypeDef rs485_interface_restore_config(UART_InitTypeDef previousHw)
{
	HAL_StatusTypeDef status;

	/* Stop the current UART activity before restoring the old setup. */
	(void)HAL_UART_Abort(&huart3);
	/* De-initialize the peripheral so the old setup can be reloaded safely. */
	status = HAL_UART_DeInit(&huart3);
	if (status != HAL_OK)
	{
		return status;
	}

	/* Put back the previous UART configuration. */
	huart3.Init = previousHw;
	/* Re-start the UART with the old settings. */
	status = HAL_UART_Init(&huart3);
	if (status != HAL_OK)
	{
		return status;
	}

	/* Restore the normal receive direction after the rollback. */
	rs485_interface_enableRxMode();
	return HAL_OK;
}

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


