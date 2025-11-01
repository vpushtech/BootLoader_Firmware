#ifndef BOARD_CONFIG_BSP_H_
#define BOARD_CONFIG_BSP_H_

/* ==================== INCLUDES ==================== */

#include "clock.h"
#include "clockMan1.h"
#include "drv_timer.h"
#include "drv_can.h"
#include "drv_wdg.h"
#include "common_types.h"
#include "BL_flash.h"
#include "drv_uart.h"
#include "S32K144.h"

typedef enum {
    LPTI_TIMER0_CH0,
    MAX_TIMER_PIN,
} TimerglobleTable_en;

#define BOOT_FLAG_ADDRESS 			0x0007F000
#define BOOT_FLAG_VALUE 			0xABCDABCD
#define APP_STATUS_ADDRESS	 		0x0007F008
#define APP_SIZE_ADDRESS	 		0x0007F010
#define APP_VERSION_ADDRESS			0x0007F018
#define FLASH_WRITE_CMD				49
#define APP_STATUS_OK				1
#define CANCEL_FLASH_WRITE				50

/* ==================== EXTERN VARIABLES ==================== */
/* Configuration Tables */
extern DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN];


/* CAN Frame Buffers */
/* ==================== FUNCTION DECLARATIONS ==================== */

void can_testing(void);
/* Board Support Package Initialization */
extern void BSP_Init(void);
void BSP_DRV_Config_gv(void);

void JumpToUserApp(void);
void JumpToBootloader(void);
flash_status BL_Handle_FlashWrite(void);
uint32_t crc32_flash(void);
uint32_t crc32_calculate(uint8_t* data, uint32_t length);
void set_boot_flag(void);
uint32_t check_boot_flag(void);
uint8_t Read_app_status(void);
flash_status update_data(uint8_t app_status);
void SystemReset(void);
void delay(uint32_t cycles);

extern void BSP_TimerDelay(U32 delay_argu32, DRV_TimerDelayUnit_En unit_argen);

#endif /* BOARD_CONFIG_BSP_H_ */
