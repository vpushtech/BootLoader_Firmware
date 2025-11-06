/* bsp.c – S32K144 OTA bootloader – NO LOCKUP / DEFAULT HANDLER */

/* ==================== INCLUDE FILES ==================== */
#include "bsp.h"
#include "S32K144.h"
#include "device_registers.h"
#include <string.h>
#include"CAN_Communication.h"

#ifndef __SET_MSP
#define __SET_MSP(x) __asm volatile ("MSR msp, %0" : : "r" (x))
#endif

/* ==================== EXTERN VARIABLES ==================== */
extern CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID];
extern flash_ssd_config_t flashSSDConfig;

/* ==================== GLOBAL VARIABLES ==================== */
U32 BytesWritten_garg8 = 0;
U8 rxBuffer_garg8[8] = {0};
U8 UART_RXbuff_garg8[8] = {0};
U8 FlashWriteFlag_garg8 = 0x00;
U8 command_garg8;

/* ==================== GLOBAL CONFIGURATIONS ==================== */
DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN]={
		{DRV_TIMER0,DRV_CHANNEL_0},
};

/* ==================== STATIC FUNCTIONS ==================== */
static void BSP_HardwareInit(void);
static void BSP_BootloaderCheck_mv(void);
static void BSP_JumpToUserApp_mv(void);
static void BSP_HandleBootloaderComm_mv(void);
static void BSP_HandleUartComm_mv(void);
static void BSP_HandleCanComm_mv(void);
static DRV_Uart_Status_ten BSP_UARTFlashWrite_men(void);
static DRV_CanStatus_En BSP_CANFlashWrite_men(void);
static U32 BSP_CRCFlash_mu32(void);
static U32 BSP_CalculateCRC_mu32(U8* data_argptr8, U32 length_arg32);
static U32 BSP_ReadBootFlag_mu32(void);
static U8 BSP_ReadAppStatus_mu8(void);
static U8 BSP_ReadCommStatus_mu8(void);
static DRV_FlashStatus_en BSP_UpdateAppStatus_men(U8 appStatus_arg8);
static void BSP_CAN_RxConfig_mv(void);
static void BSP_UART_RxConfig_mv(void);

/* ==================== PRIVATE FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : BSP_HardwareInit
*   Description   : This function initializes all hardware peripherals including clocks, pins,
* 					CAN, timers, flash, and UART modules.
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
static void BSP_HardwareInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
    DRV_CAN_Init_gen(DRV_CAN_INSTANCE_1);
    DRV_Timer_Init_gv(LPTI_TIMER0_CH0);
    DRV_FLASH_Init_gen();
    DRV_FlashStatus_en status = DRV_UART_Init(DRV_UART_INSTANCE_1);
    if(status != FLASH_OK)
    	while(1);
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bsp_init
*   Description   : Initializes hardware, UART and CAN receive configurations, and performs
*					bootloader check to determine whether to jump to application or stay in bootloader.
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void BSP_Init(void)
{
    BSP_HardwareInit();
    BSP_UART_RxConfig_mv();
    BSP_CAN_RxConfig_mv();

    //DRV_FLASH_SectorErase_gen((U32)APP_START_ADDRESS,(U32)APP_SIZE);
    BSP_BootloaderCheck_mv();
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_BootloaderCheck_mv
 *  Description   : Checks boot flag and application status to determine whether to jump to
 * 					user application or remain in bootloader mode.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_BootloaderCheck_mv(void)
{
	if(BSP_ReadBootFlag_mu32() != BOOT_FLAG_VALUE && BSP_ReadAppStatus_mu8() == APP_STATUS_OK)
	{
		BSP_JumpToUserApp_mv();
	}
	BSP_HandleBootloaderComm_mv();

	while(1);
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_JumpToUserApp_mv
 *  Description   : Deinitializes peripherals, sets vector table to application start address,
 * 					and transfers control to user application.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_JumpToUserApp_mv(void)
{
	DRV_UART_Deinit(DRV_UART_INSTANCE_1);
	DRV_CAN_Deinit_gen(DRV_CAN_INSTANCE_1);
	DRV_Timer_DeInit_gv(LPTI_TIMER0_CH0);

	INT_SYS_DisableIRQGlobal();

	U32 mainStackPointerValue = *(volatile U32 *)APP_START_ADDRESS;
	U32 resetHandlerAddress = *(volatile U32 *)(APP_START_ADDRESS + 4U);

	S32_SCB->VTOR = APP_START_ADDRESS;

	__SET_MSP(mainStackPointerValue);

	void (*app_reset_handler)(void) = (void (*)(void))resetHandlerAddress;
	app_reset_handler();
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_HandleBootloaderComm_mv
 *  Description   : It handles the Bootloader and calls the function
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_HandleBootloaderComm_mv(void)
{
	U8 rxbuf[8] = {0};
	U8 status = BSP_ReadCommStatus_mu8();

	if(status == UART_PROTOCOL)
	{
		BSP_HandleUartComm_mv();
	}
	else if(status == CAN_PROTOCOL)
	{
		BSP_HandleCanComm_mv();
	}
	else if(status == PROTOCOL_ERROR)
	{
		while(1)
		{
			command_garg8 = UART_RXbuff_garg8[0];
			if(command_garg8 == UART_COMMAND)
			{
				DRV_CAN_Deinit_gen(DRV_CAN_INSTANCE_1);
				DRV_UART_Deinit(DRV_UART_INSTANCE_1);
				DRV_UART_Init(DRV_UART_INSTANCE_1);
				BSP_HandleUartComm_mv();
				break;
			}
			else if(CAN_Rx_0x1B0_mv((U8 *)&rxbuf) == DRV_CAN_STATUS_SUCCESS)
			{
				if(rxbuf[0] == CAN_COMMAND)
				{
					DRV_UART_Deinit(DRV_UART_INSTANCE_1);
					BSP_HandleCanComm_mv();
					break;
				}
			}
		}
	}
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_HandleUartComm_mv
 *  Description   : Handles UART communication for firmware update process including
 * 					flash erase, firmware writing, and CRC verification.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_HandleUartComm_mv(void)
{
	U32 rxCRC = 5;
	while(1)
	{
		DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,&command_garg8,sizeof(command_garg8));
		if(command_garg8 == FLASH_WRITE_CMD)
		{
			DRV_FLASH_SectorErase_gen((U32)APP_START_ADDRESS,(U32)APP_SIZE);

			BSP_UpdateAppStatus_men(0x00);
			if(BSP_UARTFlashWrite_men() != DRV_UART_STATUS_SUCCESS)
			{
				SystemSoftwareReset();
			}

			DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(U8 *)&rxCRC, sizeof(rxCRC));
			U32 crc = BSP_CRCFlash_mu32();
			if(crc == rxCRC)
			{
				DRV_UART_SendDataBlocking(DRV_UART_INSTANCE_1,(U8*)"FLASH\n", 6);
				BSP_UpdateAppStatus_men(0x01);
				SystemSoftwareReset();
			}
			else if(FlashWriteFlag_garg8)
			{
				DRV_UART_SendDataBlocking(DRV_UART_INSTANCE_1,(U8*)"ERROR\n", 6);
				FlashWriteFlag_garg8 = 0x00;
			}
		}
	}
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_HandleCanComm_mv
 *  Description   : Handles CAN communication for firmware update process including
 * 					flash erase, firmware writing, and CRC verification.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_HandleCanComm_mv(void)
{
	U32 rxCRC = 5;
	U8 rxbuf[8] = {0};
	while(1)
	{
		CAN_Rx_0x1B0_mv((U8 *)&rxbuf);
		if(rxbuf[0] == FLASH_WRITE_CMD)
		{
			DRV_FLASH_SectorErase_gen((U32)APP_START_ADDRESS,(U32)APP_SIZE);

			BSP_UpdateAppStatus_men(0x00);
			if(BSP_CANFlashWrite_men() != DRV_CAN_STATUS_SUCCESS)
			{
				SystemSoftwareReset();
			}

			CAN_Rx_0x1B0_mv((U8 *)&rxbuf);
			rxCRC = rxbuf[0] |(rxbuf[1] << 8) |(rxbuf[2] << 16) |(rxbuf[3] << 24);
			U32 crc = BSP_CRCFlash_mu32();

			if(crc == rxCRC)
			{
				CAN_ID_0x1A0_mv("FLASH\n",6);
				BSP_UpdateAppStatus_men(0x01);
				SystemSoftwareReset();
			}
			else if(FlashWriteFlag_garg8)
			{
				CAN_ID_0x1A0_mv("ERROR\n",6);
			}
		}
	}
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_UARTFlashWrite_men
 *  Description   : Receives firmware data through UART and writes it to flash memory.
 *  Parameters    : None
 *  Return Value  : DRV_CanStatus_En - Status of the firmware write operation
 *  -----------------------------------------------------------------------------*/
static DRV_Uart_Status_ten BSP_UARTFlashWrite_men(void)
{
	U32 FlashAddress   = APP_START_ADDRESS;
	const U32 ChunkSize  = 8;
	U8 recSize[8] = {0};
	BytesWritten_garg8 = 0;

	DRV_UART_SendDataBlocking(DRV_UART_INSTANCE_1,(U8*)"OK\n", 3);

	DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(U8 *)&recSize, 4);
	U32 FileSize = recSize[0] |(recSize[1] << 8) |(recSize[2] << 16) |(recSize[3] << 24);

	while (BytesWritten_garg8 < FileSize)
	{
		if(DRV_UART_ReceiveDataBlocking(DRV_UART_INSTANCE_1,(U8 *)&rxBuffer_garg8, ChunkSize) == DRV_UART_STATUS_ERROR)
			return DRV_UART_STATUS_ERROR;

		DRV_FLASH_Write_gen(FlashAddress, rxBuffer_garg8, ChunkSize);
		FlashAddress     += ChunkSize;
		BytesWritten_garg8  += ChunkSize;
		FlashWriteFlag_garg8 = 0x01;
	}
	return DRV_UART_STATUS_SUCCESS;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_CANFlashWrite_men
 *  Description   : Receives firmware data through CAN and writes it to flash memory.
 *  Parameters    : None
 *  Return Value  : DRV_CanStatus_En - Status of the firmware write operation
 *  -----------------------------------------------------------------------------*/
static DRV_CanStatus_En BSP_CANFlashWrite_men(void)
{
	U32 FlashAddress   = APP_START_ADDRESS;
	U32 ChunkSize  = 8;
	BytesWritten_garg8 = 0;

	CAN_ID_0x1A0_mv("OK\n",3);
	CAN_Rx_0x1B0_mv((U8 *)&rxBuffer_garg8);
	U32 FileSize = rxBuffer_garg8[0] |(rxBuffer_garg8[1] << 8) |(rxBuffer_garg8[2] << 16) |(rxBuffer_garg8[3] << 24);

	while (BytesWritten_garg8 < FileSize)
	{
		if(CAN_Rx_0x1B0_mv((U8 *)&rxBuffer_garg8) == DRV_CAN_STATUS_ERROR)
			return DRV_CAN_STATUS_ERROR;
		DRV_FLASH_Write_gen(FlashAddress, rxBuffer_garg8, ChunkSize);
		FlashAddress		+= ChunkSize;
		BytesWritten_garg8	+= ChunkSize;
		FlashWriteFlag_garg8 = 0x01;
	}
	return DRV_CAN_STATUS_SUCCESS;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_CRCFlash_mu32
 *  Description   : Calculates CRC32 checksum for the flashed firmware data.
 *  Parameters    : None
 *  Return Value  : return the CRC value
 *  -----------------------------------------------------------------------------*/
static U32 BSP_CRCFlash_mu32(void)
{
    U8* ptr = (U8*)APP_START_ADDRESS;
    return BSP_CalculateCRC_mu32(ptr, BytesWritten_garg8);
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_CalculateCRC_mu32
 *  Description   : Computes CRC32 checksum for the given data buffer using polynomial 0x04C11DB7.
 *  Parameters    : data_argptr8 - pointer that points to the flash starting address
 *  				length_arg32 - Length of data in bytes
 *  Return Value  : U32 - Calculated CRC32 value
 *  -----------------------------------------------------------------------------*/
static U32 BSP_CalculateCRC_mu32(U8* data_argptr8, U32 length_arg32)
{
    U32 crc_arg8 = 0xFFFFFFFF;
    U32 poly_arg8 = 0x04C11DB7;

    for (U32 i = 0; i < length_arg32; i++)
    {
        crc_arg8 ^= ((U32)data_argptr8[i]) << 24;
        for (U8 bit = 0; bit < 8; bit++)
        {
            if (crc_arg8 & 0x80000000)
                crc_arg8 = (crc_arg8 << 1) ^ poly_arg8;
            else
                crc_arg8 <<= 1;
        }
    }
    return crc_arg8 ^ 0xFFFFFFFF;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_ReadBootFlag_mu32
 *  Description   : Reads the Bootloader flag from the flash memory
 *  Parameters    : None
 *  Return Value  : return the value of Bootloader flag
 *  -----------------------------------------------------------------------------*/
static U32 BSP_ReadBootFlag_mu32(void)
{
    return *(volatile U32*)BOOT_FLAG_ADDRESS;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_ReadAppStatus_mu8
 *  Description   : Reads the application status from the flash memory
 *  Parameters    : None
 *  Return Value  : return the value of application status
 *  -----------------------------------------------------------------------------*/
static U8 BSP_ReadAppStatus_mu8(void)
{
    return *(volatile U8*)APP_STATUS_ADDRESS;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_ReadCommStatus_mu8
 *  Description   : Reads the communication status from the flash
 *  Parameters    : None
 *  Return Value  : return the communication status
 *  -----------------------------------------------------------------------------*/
static U8 BSP_ReadCommStatus_mu8(void)
{
	return *(volatile U8*)PROTOCOL_ADDRESS;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_UpdateAppStatus_men
 *  Description   : updates the application status and size information in flash memory.
 *  Parameters    : appStatus_arg8 - New application status
 *  Return Value  : DRV_FlashStatus_en - status of the Update flash write
 *  -----------------------------------------------------------------------------*/
static DRV_FlashStatus_en BSP_UpdateAppStatus_men(U8 appStatus_arg8)
{
	DRV_FlashStatus_en status;

	DRV_FLASH_SectorErase_gen((U32)BOOT_FLAG_ADDRESS,(U32)META_SIZE);

	U8 appStatusData_arg8[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	appStatusData_arg8[0] = appStatus_arg8;
	status = DRV_FLASH_Write_gen(APP_STATUS_ADDRESS, (U8 *)&appStatusData_arg8, 8);
	if (status != FLASH_OK) goto end;

	uint64_t appSize_arg64 = 0xFFFFFFFFFFFFFFFF;
	((U32*)&appSize_arg64)[0] = BytesWritten_garg8;
	status = DRV_FLASH_Write_gen(APP_SIZE_ADDRESS, (U8 *)&BytesWritten_garg8, 8);
	if (status != FLASH_OK) goto end;

end:
	return status;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_CAN_RxConfig_mv
 *  Description   : Initializes and configures the CAN receive buffer for
 *                  CAN instance 1. Sets the buffer address and Frame configuration
 *                  , and Message ID to receive, starts CAN reception.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_CAN_RxConfig_mv(void)
{
	DRV_CAN_ConfigRxBuffer_gen(DRV_CAN_INSTANCE_1, CAN_Buffer_Idx_2, &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St, 0x1B0);
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *  Function Name : BSP_UART_RxConfig_mv
 *  Description   : Initializes and configures the UART receive buffer for
 *                  UART instance 1. Sets the buffer address and length_arg32, and
 *                  starts UART reception.
 *  Parameters    : None
 *  Return Value  : None
 *  -----------------------------------------------------------------------------*/
static void BSP_UART_RxConfig_mv(void)
{
	DRV_UART_SetRxBuffer(DRV_UART_INSTANCE_1,(U8 *)&UART_RXbuff_garg8,8);
	DRV_UART_ReceiveData(DRV_UART_INSTANCE_1,(U8 *)&UART_RXbuff_garg8,8);
}

void BSP_DRV_Config_gv(void)
{
   /*DRV_NVIC_IRQConfig_gen(NVIC_FTFC_IRQ,0);
   DRV_NVIC_IRQConfig_gen(NVIC_LPUART1_IRQ,5);
   DRV_Timer_InterruptConfig_gst(LPTI_TIMER0_CH0, NVIC_LPIT0_CH0_IRQ, 2, 1, DRV_DELAY_UNITS_MILLISECOND);
   DRV_Timer_Start_gv(NVIC_LPIT0_CH0_IRQ);*/
}
