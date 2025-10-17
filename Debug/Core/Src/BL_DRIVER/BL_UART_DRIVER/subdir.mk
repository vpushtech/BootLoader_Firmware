################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.c 

OBJS += \
./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.o 

C_DEPS += \
./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/BL_DRIVER/BL_UART_DRIVER/%.o Core/Src/BL_DRIVER/BL_UART_DRIVER/%.su Core/Src/BL_DRIVER/BL_UART_DRIVER/%.cyclo: ../Core/Src/BL_DRIVER/BL_UART_DRIVER/%.c Core/Src/BL_DRIVER/BL_UART_DRIVER/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-BL_DRIVER-2f-BL_UART_DRIVER

clean-Core-2f-Src-2f-BL_DRIVER-2f-BL_UART_DRIVER:
	-$(RM) ./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.cyclo ./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.d ./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.o ./Core/Src/BL_DRIVER/BL_UART_DRIVER/BL_UART.su

.PHONY: clean-Core-2f-Src-2f-BL_DRIVER-2f-BL_UART_DRIVER

