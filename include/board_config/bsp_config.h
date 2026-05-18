/*
 * bsp_config.h
 * Author: PandurangaKarapothul
 * Description: Board Support Package Configuration - Memory Map and Peripheral Definitions
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 * PandurangaKarapothul Initial BSP configuration
 * PandurangaKarapothul Added CAN and BLE protocol definitions
 * PandurangaKarapothul Added flash memory map constants
 ******************************************************************************/

#ifndef BOARD_CONFIG_BSP_CONFIG_H_
#define BOARD_CONFIG_BSP_CONFIG_H_

/* ==================== INCLUDES ==================== */
#include <bootloader/can_bootloader.h>
#include <bootloader/ota_bootloader.h>
#include "drv_flash.h"
#include "clock.h"
#include "clockMan1.h"
#include "drv_timer.h"
#include "drv_can.h"
#include "drv_wdg.h"
#include "common_types.h"
#include "drv_uart.h"
#include "S32K144.h"

#ifndef CUSTUM_BOARD_PIN_CONFIG
#define CUSTUM_BOARD_PIN_CONFIG (1U)
#endif
/* ==================== ENUMERATIONS ==================== */
typedef enum
{
    LPTI_TIMER0_CH0 = 0,
    MAX_TIMER_PIN
} TimerglobleTable_en;

/* ==================== FLASH MEMORY MAP ==================== */
#define APP_VERSION_ADDRESS   (0x0007F018U)
#define PROTOCOL_ADDRESS      (0x0007F020U)
#define APP_STATUS_OK         (1U)
#define CAN_PROTOCOL          (0x35U)
#define BLE_OTA_PROTOCOL      (0x34U)

/* Command definitions */
#define UART_COMMAND          (49U)
#define CAN_COMMAND           (50U)
#define FLASH_WRITE_CMD       (51U)

/* ==================== FLASH MEMORY LAYOUT ==================== */
/* Bootloader region */
#define BOOT_START_ADDRESS    (0x00000000U)
#define BOOT_SIZE             (32U  * 1024U)
#define BOOT_END_ADDRESS      (BOOT_START_ADDRESS + BOOT_SIZE - 1U)

/* Application region */
#define APP_START_ADDRESS     (0x00008000U)
#define APP_SIZE              (480U * 1024U)
#define APP_END_ADDRESS       (0x0007EFFFU)

/* Meta sector region (stores boot configuration) */
#define META_START_ADDRESS    (0x0007F000U)  /* Meta sector start address */
#define META_SIZE             (4U * 1024U)   /* Meta sector size: 4 KB */
#define META_END_ADDRESS      (META_START_ADDRESS + META_SIZE - 1U)  /* Meta sector end address */

/* ==================== EXTERN VARIABLES ==================== */
extern U32 BytesWritten_garg32;   /* Total bytes written to flash */
extern U8  FlashWriteFlag_garg8;  /* Flash write operation flag */

/* ==================== FUNCTION DECLARATIONS ==================== */
extern void BSP_Init(void);
extern void BSP_DRV_Config_gv(void);
extern void BSP_writeProtocol_gv(U8 Value);
extern void BSP_ProcessOtaSubCommand_mv(uint8_t *payload_pu8, uint16_t len_u16);
extern void BSP_ProcessBootloaderComm_gv(void);

#endif /* BOARD_CONFIG_BSP_CONFIG_H_ */
