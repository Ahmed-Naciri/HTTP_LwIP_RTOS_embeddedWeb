################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app_config.c \
../Core/Src/app_config_http.c \
../Core/Src/ee.c \
../Core/Src/freertos.c \
../Core/Src/fs.c \
../Core/Src/httpserver-netconn.c \
../Core/Src/main.c \
../Core/Src/modbus_rtu_master.c \
../Core/Src/network_config.c \
../Core/Src/network_config_http.c \
../Core/Src/persistent_store.c \
../Core/Src/polling_engine.c \
../Core/Src/rs485_interface.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c 

OBJS += \
./Core/Src/app_config.o \
./Core/Src/app_config_http.o \
./Core/Src/ee.o \
./Core/Src/freertos.o \
./Core/Src/fs.o \
./Core/Src/httpserver-netconn.o \
./Core/Src/main.o \
./Core/Src/modbus_rtu_master.o \
./Core/Src/network_config.o \
./Core/Src/network_config_http.o \
./Core/Src/persistent_store.o \
./Core/Src/polling_engine.o \
./Core/Src/rs485_interface.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o 

C_DEPS += \
./Core/Src/app_config.d \
./Core/Src/app_config_http.d \
./Core/Src/ee.d \
./Core/Src/freertos.d \
./Core/Src/fs.d \
./Core/Src/httpserver-netconn.d \
./Core/Src/main.d \
./Core/Src/modbus_rtu_master.d \
./Core/Src/network_config.d \
./Core/Src/network_config_http.d \
./Core/Src/persistent_store.d \
./Core/Src/polling_engine.d \
./Core/Src/rs485_interface.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F429xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/BSP/Components/dp83848 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_config.cyclo ./Core/Src/app_config.d ./Core/Src/app_config.o ./Core/Src/app_config.su ./Core/Src/app_config_http.cyclo ./Core/Src/app_config_http.d ./Core/Src/app_config_http.o ./Core/Src/app_config_http.su ./Core/Src/ee.cyclo ./Core/Src/ee.d ./Core/Src/ee.o ./Core/Src/ee.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/fs.cyclo ./Core/Src/fs.d ./Core/Src/fs.o ./Core/Src/fs.su ./Core/Src/httpserver-netconn.cyclo ./Core/Src/httpserver-netconn.d ./Core/Src/httpserver-netconn.o ./Core/Src/httpserver-netconn.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/modbus_rtu_master.cyclo ./Core/Src/modbus_rtu_master.d ./Core/Src/modbus_rtu_master.o ./Core/Src/modbus_rtu_master.su ./Core/Src/network_config.cyclo ./Core/Src/network_config.d ./Core/Src/network_config.o ./Core/Src/network_config.su ./Core/Src/network_config_http.cyclo ./Core/Src/network_config_http.d ./Core/Src/network_config_http.o ./Core/Src/network_config_http.su ./Core/Src/persistent_store.cyclo ./Core/Src/persistent_store.d ./Core/Src/persistent_store.o ./Core/Src/persistent_store.su ./Core/Src/polling_engine.cyclo ./Core/Src/polling_engine.d ./Core/Src/polling_engine.o ./Core/Src/polling_engine.su ./Core/Src/rs485_interface.cyclo ./Core/Src/rs485_interface.d ./Core/Src/rs485_interface.o ./Core/Src/rs485_interface.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su

.PHONY: clean-Core-2f-Src

