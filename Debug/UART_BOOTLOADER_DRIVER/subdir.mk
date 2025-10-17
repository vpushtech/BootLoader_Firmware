################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.c 

OBJS += \
./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.o 

C_DEPS += \
./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.d 


# Each subdirectory must supply rules for building sources it contributes
UART_BOOTLOADER_DRIVER/%.o UART_BOOTLOADER_DRIVER/%.su UART_BOOTLOADER_DRIVER/%.cyclo: ../UART_BOOTLOADER_DRIVER/%.c UART_BOOTLOADER_DRIVER/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-UART_BOOTLOADER_DRIVER

clean-UART_BOOTLOADER_DRIVER:
	-$(RM) ./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.cyclo ./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.d ./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.o ./UART_BOOTLOADER_DRIVER/UART_BOOTLOADER_DRIVER.su

.PHONY: clean-UART_BOOTLOADER_DRIVER

