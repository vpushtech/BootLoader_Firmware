/* bsp.c – S32K144 OTA bootloader – NO LOCKUP / DEFAULT HANDLER */

#include "bsp.h"
#include "S32K144.h"
#include "device_registers.h"
#include <string.h>
#include"CAN_Communication.h"

#ifndef __SET_MSP
#define __SET_MSP(x) __asm volatile ("MSR msp, %0" : : "r" (x))
#endif

extern CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID];
extern flash_ssd_config_t flashSSDConfig;

uint32_t bytes_written = 0;
uint8_t cmd;
uint8_t rx_buff[8] = {0};
uint8_t uart_rxBuff[8] = {0};

DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN]={
		{  DRV_TIMER0,DRV_CHANNEL_0},
};

static void BSP_HardwareInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
    DRV_CAN_Init_gen(DRV_CAN_INSTANCE_1);
    DRV_Timer_Init_gv(LPTI_TIMER0_CH0);
    Flash_init();
    DRV_UART_Init(DRV_UART_INSTANCE_1);
}

void BSP_Init(void)
{
    BSP_HardwareInit();
    uart_RxConfig();
    can_RxConfig();

    //Flash_Sector_Erase((uint32_t)APP_START_ADDRESS,(uint32_t)APP_SIZE);

    if(check_boot_flag() != BOOT_FLAG_VALUE && Read_app_status() == APP_STATUS_OK)
	{
    	JumpToUserApp();
	}
	JumpToBootloader();

	while(1);
}

void JumpToUserApp(void)
{
	DRV_UART_Deinit(DRV_UART_INSTANCE_1);
	DRV_CAN_Deinit_gen(DRV_CAN_INSTANCE_1);
	DRV_Timer_DeInit_gv(LPTI_TIMER0_CH0);

	INT_SYS_DisableIRQGlobal();

	uint32_t mspValue = *(volatile uint32_t *)APP_START_ADDRESS;
	uint32_t reset_handler = *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

	S32_SCB->VTOR = APP_START_ADDRESS;

	__SET_MSP(mspValue);

	void (*app_reset_handler)(void) = (void (*)(void))reset_handler;
	app_reset_handler();
}

void JumpToBootloader(void)
{
	uint8_t cmd;
	uint8_t rxbuf[8] = {0};
	while(1)
	{
		cmd = uart_rxBuff[0];
		if(cmd == UART_COMMAND)
		{
			can_deinit();
			DRV_UART_Deinit(DRV_UART_INSTANCE_1);
			DRV_UART_Init(DRV_UART_INSTANCE_1);
			UART_Comm();
			break;
		}
		else if(CAN_Rx_0x1B0_mv((uint8_t *)&rxbuf) == DRV_CAN_STATUS_SUCCESS)
		{
			if(rxbuf[0] == CAN_COMMAND)
			{
				uart_deinit();
				CAN_Comm();
				break;
			}
		}
	}
}

void can_deinit(void)
{
	DRV_CAN_Deinit_gen(DRV_CAN_INSTANCE_1);
}

void uart_deinit(void)
{
	DRV_UART_Deinit(DRV_UART_INSTANCE_1);
}

void UART_Comm(void)
{
	uint8_t cmd;
	uint32_t Py_CRC = 5;
	while(1)
	{
		DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,&cmd,sizeof(cmd));
		if(cmd == FLASH_WRITE_CMD)
		{
			Flash_Sector_Erase((uint32_t)APP_START_ADDRESS,(uint32_t)APP_SIZE);
			update_data(0x00);
			if(UART_FlashWrite() != DRV_UART_STATUS_SUCCESS)
			{
				SystemReset();
			}
			DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(uint8_t *)&Py_CRC, sizeof(Py_CRC));
			uint32_t crc = crc32_flash();
			if(crc == Py_CRC)
			{
				update_data(0x01);
				SystemReset();
			}
		}
		else if(cmd == CANCEL_FLASH_WRITE)
		{
			set_boot_flag();
			SystemReset();
		}
	}
}

void CAN_Comm(void)
{
	uint32_t Py_CRC = 5;
	uint8_t rxbuf[8] = {0};
	while(1)
	{
		CAN_Rx_0x1B0_mv((uint8_t *)&rxbuf);
		if(rxbuf[0] == FLASH_WRITE_CMD)
		{
			Flash_Sector_Erase((uint32_t)APP_START_ADDRESS,(uint32_t)APP_SIZE);
			update_data(0x00);
			if(CAN_FlashWrite() != DRV_CAN_STATUS_SUCCESS)
			{
				SystemReset();
			}
			CAN_Rx_0x1B0_mv((uint8_t *)&rxbuf);
			Py_CRC = rxbuf[0] |(rxbuf[1] << 8) |(rxbuf[2] << 16) |(rxbuf[3] << 24);
			uint32_t crc = crc32_flash();
			if(crc == Py_CRC)
			{
				update_data(0x01);
				SystemReset();
			}
		}
		else if(rxbuf[0] == CANCEL_FLASH_WRITE)
		{
			set_boot_flag();
			SystemReset();
		}
	}
}

DRV_Uart_Status UART_FlashWrite(void)
{
	uint32_t flash_addr   = APP_START_ADDRESS;
	const uint32_t chunk  = 8;
	uint8_t recSize[8] = {0};
	bytes_written = 0;

	DRV_UART_SendDataBlocking(DRV_UART_INSTANCE_1,(uint8_t*)"OK\n", 3);
	DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(uint8_t *)&recSize, 4);
	uint32_t filesize = recSize[0] |(recSize[1] << 8) |(recSize[2] << 16) |(recSize[3] << 24);

	while (bytes_written < filesize)
	{
		if(DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(uint8_t *)&rx_buff, chunk) == DRV_UART_STATUS_ERROR)
		{
			return DRV_UART_STATUS_ERROR;
		}
		Flash_Write_Byte(flash_addr, rx_buff, chunk);
		flash_addr     += chunk;
		bytes_written  += chunk;
	}
	return DRV_UART_STATUS_SUCCESS;
}

DRV_CanStatus_En CAN_FlashWrite(void)
{
	uint32_t flash_addr   = APP_START_ADDRESS;
	const uint32_t chunk  = 8;
	uint8_t recSize[8] = {0};
	bytes_written = 0;

	DRV_Timer_Delay_gv(LPTI_TIMER0_CH0,DRV_DELAY_UNITS_MILLISECOND,100);
	CAN_ID_0x1A0_mv("OK\n",3);
	CAN_Rx_0x1B0_mv((uint8_t *)&recSize);
	uint32_t filesize = recSize[0] |(recSize[1] << 8) |(recSize[2] << 16) |(recSize[3] << 24);

	while (bytes_written < filesize)
	{
		if(CAN_Rx_0x1B0_mv((uint8_t *)&rx_buff) == DRV_CAN_STATUS_ERROR)
			return DRV_CAN_STATUS_ERROR;
		Flash_Write_Byte(flash_addr, rx_buff, chunk);
		flash_addr     += chunk;
		bytes_written  += chunk;
	}
	return DRV_CAN_STATUS_SUCCESS;
}

uint32_t crc32_flash(void)
{
    uint8_t* ptr = (uint8_t*)APP_START_ADDRESS;
    return crc32_calculate(ptr, bytes_written);
}

uint32_t crc32_calculate(uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t poly = 0x04C11DB7;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i]) << 24;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

void set_boot_flag(void)
{
	uint32_t address = BOOT_FLAG_ADDRESS;
	uint32_t flag_value = BOOT_FLAG_VALUE;
	Flash_Write_Byte(address,(uint8_t *)&flag_value,8);
}

uint32_t check_boot_flag(void)
{
    return *(volatile uint32_t*)BOOT_FLAG_ADDRESS;
}

uint8_t Read_app_status(void)
{
    uint8_t status;
    status = *(uint8_t *)APP_STATUS_ADDRESS;
    return status;
}

flash_status update_data(uint8_t app_status)
{
	flash_status status;

	Flash_Sector_Erase((uint32_t)BOOT_FLAG_ADDRESS,(uint32_t)META_SIZE);

	uint8_t app_status_data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	app_status_data[0] = app_status;
	status = Flash_Write_Byte(APP_STATUS_ADDRESS, (uint8_t *)&app_status_data, 8);
	if (status != FLASH_OK) goto end;

	uint64_t app_size_data = 0xFFFFFFFFFFFFFFFFULL;
	((uint32_t*)&app_size_data)[0] = bytes_written;
	status = Flash_Write_Byte(APP_SIZE_ADDRESS, (uint8_t *)&bytes_written, 8);
	if (status != FLASH_OK) goto end;

end:
	return status;
}

void SystemReset(void)
{
	S32_SCB->AIRCR = (0x5FA << 16) | (1 << 2);
    while(1);
}

void can_RxConfig(void)
{
	DRV_CAN_ConfigRxBuffer_gen(DRV_CAN_INSTANCE_1, CAN_Buffer_Idx_2, &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St, 0x1B0);
}

void uart_RxConfig(void)
{
	DRV_UART_SetRxBuffer(DRV_UART_INSTANCE_1,(uint8_t *)&uart_rxBuff,8);
	DRV_UART_ReceiveData(DRV_UART_INSTANCE_1,(uint8_t *)&uart_rxBuff,8);
}

void BSP_DRV_Config_gv(void)
{
   /*DRV_NVIC_IRQConfig_gen(NVIC_FTFC_IRQ,0);
   DRV_NVIC_IRQConfig_gen(NVIC_LPUART1_IRQ,5);
   DRV_Timer_InterruptConfig_gst(LPTI_TIMER0_CH0, NVIC_LPIT0_CH0_IRQ, 2, 1, DRV_DELAY_UNITS_MILLISECOND);
   DRV_Timer_Start_gv(NVIC_LPIT0_CH0_IRQ);*/
}
