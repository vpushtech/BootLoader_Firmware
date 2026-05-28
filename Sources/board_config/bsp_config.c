/*
 * bsp.c
 *
 * Created on: [Date]
 * Author: PandurangaKarapothul
 * Description: Handles OTA firmware update sub-commands received via UART from EFR32.
 *              No handshake. All commands ACK-based.
 * Version: 6.0
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 * PandurangaKarapothul Initial bootloader implementation
 * PandurangaKarapothul Added CAN bootloader support
 * PandurangaKarapothul Added CRC-32 verification
 ******************************************************************************/

/* ==================== INCLUDES ==================== */
#include "can_communication.h"
#include "nvm.h"
#include "bsp_config.h"
#include "S32K144.h"
#include <string.h>
#include "drv_wdg.h"
#include "drv_can.h"

/* ==================== MACROS & DEFINITIONS ==================== */
#ifndef __SET_MSP
#define __SET_MSP(x)  __asm volatile ("MSR msp, %0" : : "r" (x))  /* Set main stack pointer */
#endif


#define CRC32_INIT_VALUE  (0xFFFFFFFFUL)
#define CRC32_POLYNOMIAL  (0xEDB88320UL)
#define OTA_ACK_LEN       (1U)
#define FILE_SIZE_PAYLOAD (5U)
#define CRC_PAYLOAD_LEN   (5U)
#define CHUNK_MIN_LEN     (8U)
#define CHUNK_ALIGN       (8U)

/* ==================== GLOBAL VARIABLES ==================== */
U32 BytesWritten_garg32  = 0U;
U8  FlashWriteFlag_garg8 = 0x00U;
/* ==================== STATIC VARIABLES ==================== */
static U32  s_otaFlashAddr_u32 = APP_START_ADDRESS;
static U32  s_otaFileSize_u32  = 0U;
static U32  s_otaBytesWr_u32   = 0U;
static bool s_otaSession_b     = false;
static bool s_otaSizeRx_b      = false;

/* ==================== STATIC PROTOTYPES ==================== */
static void     BSP_BootloaderCheck_mv(void);
static void     BSP_JumpToUserApp_mv(void);
static uint32_t BSP_Crc32_u32(const uint8_t *data, uint32_t len);
static void     BSP_CAN_RxConfig_mv(void);
static void     BSP_ProcessUartComm_gv(void);
static void     BSP_HandleSubCmd_Trigger_mv(void);
static void     BSP_HandleSubCmd_FlashCmd_mv(void);
static void     BSP_HandleSubCmd_FileSize_mv(const uint8_t *payload_pu8, uint16_t len_u16);
static void     BSP_HandleSubCmd_Chunk_mv(const uint8_t *payload_pu8, uint16_t len_u16);
static void     BSP_HandleSubCmd_Crc_mv(const uint8_t *payload_pu8, uint16_t len_u16);
#define DEVELOPEMENT_BOARD_PIN_NUM  19
pin_settings_config_t dev_board_mux_InitConfigArr[NUM_OF_CONFIGURED_PINS] =
{
    {
        .base          = PORTD,
        .pinPortIdx    = 0u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 11u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT6,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 10u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT6,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 5u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT5,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = true,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 4u,
        .pullConfig    = PORT_INTERNAL_PULL_UP_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT5,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTD,
        .pinPortIdx    = 16u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTD,
        .pinPortIdx    = 15u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 3u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 1u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 0u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 14u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_PIN_DISABLED,
        .pinLock       = false,
        .intConfig     = PORT_INT_RISING_EDGE,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 13u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_INT_RISING_EDGE,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 12u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 17u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 16u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 15u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 14u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 7u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 6u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
};

#define CUSTUM_BOARD_PIN_NUM  20
pin_settings_config_t custum_board_mux_InitConfigArr[NUM_OF_CONFIGURED_PINS] =
{
    {
        .base          = PORTD,
        .pinPortIdx    = 0u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 11u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT6,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 10u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT6,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 5u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_PIN_DISABLED,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = true,
    },
    {
        .base          = PORTE,
        .pinPortIdx    = 4u,
        .pullConfig    = PORT_INTERNAL_PULL_UP_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_PIN_DISABLED,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTD,
        .pinPortIdx    = 16u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTD,
        .pinPortIdx    = 15u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 3u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 2u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 1u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 0u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 0u,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 14u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_PIN_DISABLED,
        .pinLock       = false,
        .intConfig     = PORT_INT_RISING_EDGE,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 13u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_INT_RISING_EDGE,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 12u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTC,
        .direction     = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 17u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 16u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 15u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTB,
        .pinPortIdx    = 14u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT3,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 7u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    {
        .base          = PORTC,
        .pinPortIdx    = 6u,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
};
/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_Init
 * ----------------------------------------------------------------------------
 * Description   : Bootloader entry point - initializes peripherals and starts
 * Parameters    : void
 * Return Value  : void
 * Notes         : Called at system startup after reset
 * --------------------------------------------------------------------------*/
void BSP_Init(void)
{
    (void)CLOCK_DRV_Init(&clockMan1_InitConfig0);
#if CUSTUM_BOARD_PIN_CONFIG
    (void)PINS_DRV_Init(CUSTUM_BOARD_PIN_NUM, custum_board_mux_InitConfigArr);

#else
    (void)PINS_DRV_Init(DEVELOPEMENT_BOARD_PIN_NUM, dev_board_mux_InitConfigArr);
#endif
    DRV_FLASH_Init_gen();

    DRV_CAN_Init_gen(DRV_CAN_INSTANCE_0);
    OTA_Init_gv();
    DRV_Timer_Init_gst(DRV_TIMER0);
    DRV_Timer_StartChannel_gst(DRV_TIMER0,LPTI_TIMER0_CH0);

    DRV_WDG_Init_gen(DRV_WDG_INSTANCE_1);
    DRV_NVIC_IRQConfig_gen(NVIC_WDG_IRQ, 4);
    DRV_NVIC_IRQConfig_gen(NVIC_CAN0_0_15_IRQ, 2);
    DRV_NVIC_IRQConfig_gen(NVIC_FTFC_IRQ, 2);
    DRV_NVIC_IRQConfig_gen(NVIC_LPIT0_CH0_IRQ, 3);
    NVM_Init_gv();
    BSP_BootloaderCheck_mv();

    BSP_ProcessBootloaderComm_gv();
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_BootloaderCheck_mv
 * ----------------------------------------------------------------------------
 * Description   : Checks if valid application exists and jumps to it
 * Parameters    : void
 * Return Value  : void
 * Notes         : Called at startup before entering bootloader mode
 * --------------------------------------------------------------------------*/
static void BSP_BootloaderCheck_mv(void)
{
    bool appStatusOk;
    bool bootFlagOk;
    DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
    appStatusOk = ((NVM_FlshStoredData_st.APP_appStatus_u16 == 0x5650U) ||
                   (NVM_FlshStoredData_st.APP_appStatus_u16 == 0x5651U));
    bootFlagOk  = (NVM_FlshStoredData_st.APP_bootFlag_u16 == 0xAAAAU);

    /* If boot flag is not set AND app status is OK, jump to application */
    if (appStatusOk && bootFlagOk)
    {
        BSP_JumpToUserApp_mv();
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_ProcessBootloaderComm_gv
 * ----------------------------------------------------------------------------
 * Description   : Determines communication protocol and starts appropriate handler
 * Parameters    : void
 * Return Value  : void
 * Notes         : Checks protocol (BLE/CAN) from flash and processes accordingly
 * --------------------------------------------------------------------------*/
void BSP_ProcessBootloaderComm_gv(void)
{
    U8      rxbuf[8]={0};
/*    uint8_t i;*/


    BSP_CAN_RxConfig_mv();

    if (NVM_FlshStoredData_st.APP_bootFlag_u16 == 0x5650U)
    {
        BSP_ProcessUartComm_gv();
    }
    else if (NVM_FlshStoredData_st.APP_bootFlag_u16 == 0x5651U)
    {
       /* BSP_CAN_RxConfig_mv();*/
        BSP_ProcessCanComm_gv();
    }
    else
    {
      /*  BSP_CAN_RxConfig_mv();*/
        for (;;)
        {
            if(uart_flag_b){
               DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
               NVM_FlshStoredData_st.APP_bootFlag_u16=0x5650;
               NVM_FlshStoredData_st.APP_appStatus_u16 =0xAAAA;
               NVM_BootFlagAppStatus_Update(NVM_FlshStoredData_st.APP_bootFlag_u16,NVM_FlshStoredData_st.APP_appStatus_u16);
               }
            if (CAN_ProcessReceiveFrame_mv((U8 *)&rxbuf[0]) == DRV_CAN_STATUS_SUCCESS)
            {

                if (rxbuf[0] == (U8)CAN_COMMAND)
                {
                    BSP_ProcessCanComm_gv();
                    break;
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_ProcessUartComm_gv
 * -----------------------------------------------------------------------------
 * Description   : Initializes and runs UART OTA communication
 * Parameters    : None
 * Return Value  : void
 * Notes         : Enters infinite loop processing OTA frames
 * -----------------------------------------------------------------------------*/
static void BSP_ProcessUartComm_gv(void)
{
	U8 rxbuf[8]={0};


    for (;;)
    {
        if (CAN_ProcessReceiveFrame_mv((U8 *)&rxbuf[0]) == DRV_CAN_STATUS_SUCCESS)
         {
             if (rxbuf[0] == (U8)CAN_COMMAND)
             {
            	   DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
            	 NVM_FlshStoredData_st.APP_bootFlag_u16=0x5651;
            	 NVM_FlshStoredData_st.APP_appStatus_u16 =0xAAAA;
            	NVM_BootFlagAppStatus_Update(NVM_FlshStoredData_st.APP_bootFlag_u16,NVM_FlshStoredData_st.APP_appStatus_u16);
             }
         }
        OTA_TxScheduler_gv();
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_CAN_RxConfig_mv
 * -----------------------------------------------------------------------------
 * Description   : Configures CAN RX buffer for receiving messages
 * Parameters    : None
 * Return Value  : void
 * Notes         : Configures buffer for CAN ID 0x1B0
 * -----------------------------------------------------------------------------*/
static void BSP_CAN_RxConfig_mv(void)
{
    DRV_CAN_ConfigRxBuffer_gen(DRV_CAN_INSTANCE_0, CAN_Buffer_Idx_2,
                               &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St, 0x1B0U);
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_JumpToUserApp_mv
 * ----------------------------------------------------------------------------
 * Description   : Jumps to user application by loading reset vector and MSP
 * Parameters    : void
 * Return Value  : void (does not return)
 * Notes         : Disables interrupts and de-initializes peripherals before jump
 * --------------------------------------------------------------------------*/
static void BSP_JumpToUserApp_mv(void)
{
    void (*app_reset_handler)(void);
    U32 mainStackPointerValue;
    U32 resetHandlerAddress;

    /* De-initialize UART and Timer used by bootloader */
    DRV_UART_DeInit_gen(OTA_UART_INSTANCE);
    DRV_Timer_DeInit_gst(DRV_TIMER0);
    DRV_CAN_Deinit_gen(DRV_CAN_INSTANCE_0);

    /* Disable all global interrupts */
    INT_SYS_DisableIRQGlobal();

    /* Read application stack pointer and reset handler from vector table */
    mainStackPointerValue = *(volatile U32 *)APP_START_ADDRESS;
    resetHandlerAddress   = *(volatile U32 *)(APP_START_ADDRESS + 4U);

    /* Set vector table offset to application */
    S32_SCB->VTOR = APP_START_ADDRESS;

    /* Set main stack pointer */
    __SET_MSP(mainStackPointerValue);

    /* Jump to application reset handler */
    app_reset_handler = (void (*)(void))resetHandlerAddress;
    app_reset_handler();
}

/* ----------------------------------------------------------------------------
 * Sub-command handler: OTA_SUBCMD_TRIGGER
 * --------------------------------------------------------------------------*/
static void BSP_HandleSubCmd_Trigger_mv(void)
{
    uint8_t ack[OTA_ACK_LEN];

    s_otaSession_b       = false;
    s_otaSizeRx_b        = false;
    s_otaFlashAddr_u32   = APP_START_ADDRESS;
    s_otaFileSize_u32    = 0U;
    s_otaBytesWr_u32     = 0U;
    BytesWritten_garg32  = 0U;
    FlashWriteFlag_garg8 = 0x00U;
    ack[0]               = OTA_ACK_OK;

    OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
}

/* ----------------------------------------------------------------------------
 * Sub-command handler: OTA_SUBCMD_FLASH_CMD
 * --------------------------------------------------------------------------*/
static void BSP_HandleSubCmd_FlashCmd_mv(void)
{
    uint8_t             ack[OTA_ACK_LEN];
    DRV_flashStatus_ten fst;

    fst = DRV_FLASH_EraseSector_gen((U32)APP_START_ADDRESS, (U32)APP_SIZE);

    if (fst == DRV_FLASH_SUCCESS)
    {
        s_otaFlashAddr_u32   = APP_START_ADDRESS;
        s_otaBytesWr_u32     = 0U;
        BytesWritten_garg32  = 0U;
        FlashWriteFlag_garg8 = 0x00U;
        s_otaSession_b       = true;

        NVM_FlshStoredData_st.APP_appStatus_u16 = 0xAAAAU;
        NVM_FlshStoredData_st.APP_bootFlag_u16  = 0x5650U;
        NVM_BootFlagAppStatus_Update(NVM_FlshStoredData_st.APP_bootFlag_u16,
                                     NVM_FlshStoredData_st.APP_appStatus_u16);
        ack[0] = OTA_ACK_OK;
    }
    else
    {
        s_otaSession_b = false;
        ack[0]         = OTA_ACK_FAIL;
    }

    OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
}

/* ----------------------------------------------------------------------------
 * Sub-command handler: OTA_SUBCMD_FILE_SIZE
 * --------------------------------------------------------------------------*/
static void BSP_HandleSubCmd_FileSize_mv(const uint8_t *payload_pu8, uint16_t len_u16)
{
    uint8_t ack[OTA_ACK_LEN];

    if (len_u16 >= (uint16_t)FILE_SIZE_PAYLOAD)
    {
        s_otaFileSize_u32 = ((U32)payload_pu8[1])
                          | ((U32)payload_pu8[2] <<  8U)
                          | ((U32)payload_pu8[3] << 16U)
                          | ((U32)payload_pu8[4] << 24U);
        s_otaSizeRx_b = true;
        ack[0]        = OTA_ACK_OK;
    }
    else
    {
        ack[0] = OTA_ACK_FAIL;
    }

    OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
}

/* ----------------------------------------------------------------------------
 * Sub-command handler: OTA_SUBCMD_CHUNK
 * --------------------------------------------------------------------------*/
static void BSP_HandleSubCmd_Chunk_mv(const uint8_t *payload_pu8, uint16_t len_u16)
{
    uint8_t             ack[OTA_ACK_LEN];
    DRV_flashStatus_ten fst;
    uint16_t            dataLen;
    bool                guardOk;

    dataLen = len_u16 - 1U;
    guardOk = s_otaSession_b
           && s_otaSizeRx_b
           && (dataLen >= (uint16_t)CHUNK_MIN_LEN)
           && ((dataLen % (uint16_t)CHUNK_ALIGN) == 0U)
           && (s_otaFileSize_u32 > 0U)
           && ((s_otaBytesWr_u32 + (U32)dataLen) <= s_otaFileSize_u32);

    if (!guardOk)
    {
        ack[0] = OTA_ACK_FAIL;
        OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
    }
    else
    {
        fst = DRV_FLASH_WriteBlock_gen(s_otaFlashAddr_u32,
                                       &payload_pu8[1],
                                       (U32)dataLen);

        FlashWriteFlag_garg8 = 0x01U;

        if (fst == DRV_FLASH_SUCCESS)
        {
            s_otaFlashAddr_u32  += (U32)dataLen;
            s_otaBytesWr_u32    += (U32)dataLen;
            BytesWritten_garg32  = s_otaBytesWr_u32;
            ack[0]               = OTA_ACK_OK;
        }
        else
        {
            ack[0] = OTA_ACK_FAIL;
        }

        OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
    }
}

/* ----------------------------------------------------------------------------
 * Sub-command handler: OTA_SUBCMD_CRC
 * --------------------------------------------------------------------------*/
static void BSP_HandleSubCmd_Crc_mv(const uint8_t *payload_pu8, uint16_t len_u16)
{
    uint8_t        ack[OTA_ACK_LEN];
    uint32_t       expectedCrc;
    uint32_t       actualCrc;
    const uint8_t *flashPtr;
    U32            resetHandlerAddress;
    bool           precondOk;

    precondOk = s_otaSession_b
             && (FlashWriteFlag_garg8 != 0U)
             && (len_u16 >= (uint16_t)CRC_PAYLOAD_LEN);
    DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
    if (!precondOk)
    {
        ack[0] = OTA_ACK_FAIL;
        OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
    }
    else
    {
        expectedCrc = ((uint32_t)payload_pu8[1])
                    | ((uint32_t)payload_pu8[2] <<  8U)
                    | ((uint32_t)payload_pu8[3] << 16U)
                    | ((uint32_t)payload_pu8[4] << 24U);

        flashPtr  = (const uint8_t *)APP_START_ADDRESS;
        actualCrc = BSP_Crc32_u32(flashPtr, s_otaFileSize_u32);
        DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
        if (actualCrc != expectedCrc)
        {
            ack[0] = OTA_ACK_FAIL;
            OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
        }
        else
        {
            resetHandlerAddress = *(volatile U32 *)(APP_START_ADDRESS + 4U);
            DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
            if ((resetHandlerAddress < APP_START_ADDRESS) ||
                (resetHandlerAddress > APP_END_ADDRESS))
            {
                ack[0] = OTA_ACK_FAIL;
                OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
            }
            else
            {
                NVM_FlshStoredData_st.APP_appStatus_u16 = 0x5650U;
                NVM_FlshStoredData_st.APP_bootFlag_u16  = 0xAAAAU;
                NVM_BootFlagAppStatus_Update(NVM_FlshStoredData_st.APP_bootFlag_u16,
                                             NVM_FlshStoredData_st.APP_appStatus_u16);
                ack[0] = OTA_ACK_OK;
                OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, 1U, ack);
                DRV_Timer_Delay_gst(DRV_TIMER0,LPTI_TIMER0_CH0, DRV_DELAY_UNITS_MILLISECOND, 2000U);
                SystemSoftwareReset();
            }
        }
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_ProcessOtaSubCommand_mv
 * ----------------------------------------------------------------------------
 * Description   : Processes OTA sub-commands for firmware update
 * Parameters    : payload_pu8 - Pointer to payload data
 *                 len_u16 - Length of payload in bytes
 * Return Value  : void
 * Notes         : Handles TRIGGER, FLASH_CMD, FILE_SIZE, CHUNK, CRC commands
 * --------------------------------------------------------------------------*/
void BSP_ProcessOtaSubCommand_mv(uint8_t *payload_pu8, uint16_t len_u16)
{
    uint8_t ack[OTA_ACK_LEN];

    if ((payload_pu8 == NULL) || (len_u16 == 0U))
    {
        return;
    }

    switch (payload_pu8[0])
    {
        case OTA_SUBCMD_TRIGGER:
            BSP_HandleSubCmd_Trigger_mv();
            break;

        case OTA_SUBCMD_FLASH_CMD:
            BSP_HandleSubCmd_FlashCmd_mv();
            break;

        case OTA_SUBCMD_FILE_SIZE:
            BSP_HandleSubCmd_FileSize_mv(payload_pu8, len_u16);
            break;

        case OTA_SUBCMD_CHUNK:
            BSP_HandleSubCmd_Chunk_mv(payload_pu8, len_u16);
            break;

        case OTA_SUBCMD_CRC:
            BSP_HandleSubCmd_Crc_mv(payload_pu8, len_u16);
            break;

        default:
            ack[0] = OTA_ACK_FAIL;
            OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, (uint16_t)OTA_ACK_LEN, ack);
            break;
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * BSP_Crc32_u32
 *
 * Reflected CRC-32, polynomial 0xEDB88320 (ISO 3309 / Ethernet).
 * Initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF.
 * ════════════════════════════════════════════════════════════════════════ */
static uint32_t BSP_Crc32_u32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = CRC32_INIT_VALUE;
    uint32_t i;
    uint32_t j;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (j = 0U; j < 8U; j++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}
