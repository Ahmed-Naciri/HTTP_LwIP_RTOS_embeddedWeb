/*
 * modbus_config_types.h
 *
 *  Created on: Apr 14, 2026
 *      Author: bahmdan
 */

#ifndef INC_MODBUS_CONFIG_TYPES_H_
#define INC_MODBUS_CONFIG_TYPES_H_

#define MAX_UART_PORTS           1
#define MAX_SLAVES               247
#define MAX_REGISTERS_PER_SLAVE   32
#define MAX_NAME_LEN             32
#define MAX_UNIT_LEN             10


typedef enum
{
    PORT_USART3 = 0,

} PortId;

typedef enum
{
    PARITY_NONE = 0,
    PARITY_EVEN,
    PARITY_ODD

} ParityType;

typedef struct
{
    PortId portId;
    uint32_t baudRate;
    uint8_t dataBits;
    ParityType parity;
    uint8_t stopBits;

} PortConfig;



typedef enum
{
    DATA_TYPE_UINT16 = 0,
    DATA_TYPE_INT16,
    DATA_TYPE_UINT32,
    DATA_TYPE_INT32,
    DATA_TYPE_FLOAT
} DataType;

typedef enum
{
    MODBUS_FC_READ_COILS = 1,
    MODBUS_FC_READ_DISCRETE_INPUTS = 2,
    MODBUS_FC_READ_HOLDING_REGISTERS = 3,
    MODBUS_FC_READ_INPUT_REGISTERS = 4
} ModbusFunctionCode;

typedef struct
{

    uint16_t registerId;
    char registerName[MAX_NAME_LEN];
    ModbusFunctionCode functionCode;
    uint16_t registerAddress;
    DataType dataType;
    float scale;
    float offset;
    char unit[MAX_UNIT_LEN];
} RegisterConfig;

typedef struct
{

    uint8_t slaveAddress;
    PortId portId;
    char deviceName[MAX_NAME_LEN];
    RegisterConfig registers[MAX_REGISTERS_PER_SLAVE];

} SlaveConfig;

typedef struct
{
    PortConfig ports[MAX_UART_PORTS];
    SlaveConfig slaves[MAX_SLAVES];
    //RegisterPointRuntime runtimeValues[MAX_SLAVES][MAX_POINTS_PER_SLAVE];
} ProjectDatabase;

#endif /* INC_MODBUS_CONFIG_TYPES_H_ */
