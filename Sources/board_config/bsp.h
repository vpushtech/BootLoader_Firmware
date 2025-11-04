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
#define APP_STATUS_OK				1
#define UART_COMMAND 				49
#define CAN_COMMAND					50
#define FLASH_WRITE_CMD				51
#define CANCEL_FLASH_WRITE			52

/* ==================== EXTERN VARIABLES ==================== */
/* Configuration Tables */
extern DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN];


/* CAN Frame Buffers */
/* ==================== FUNCTION DECLARATIONS ==================== */

/* Board Support Package Initialization */
extern void BSP_Init(void);
void BSP_DRV_Config_gv(void);

void JumpToUserApp(void);
void JumpToBootloader(void);
void can_deinit(void);
void uart_deinit(void);
void UART_Comm(void);
void CAN_Comm(void);
DRV_Uart_Status UART_FlashWrite(void);
DRV_CanStatus_En CAN_FlashWrite(void);
uint32_t crc32_flash(void);
uint32_t crc32_calculate(uint8_t* data, uint32_t length);
void set_boot_flag(void);
uint32_t check_boot_flag(void);
uint8_t Read_app_status(void);
flash_status update_data(uint8_t app_status);
void SystemReset(void);
void can_RxConfig(void);
void uart_RxConfig(void);

extern void BSP_TimerDelay(U32 delay_argu32, DRV_TimerDelayUnit_En unit_argen);

#endif /* BOARD_CONFIG_BSP_H_ */
