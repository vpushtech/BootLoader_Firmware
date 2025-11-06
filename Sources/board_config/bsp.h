#ifndef BOARD_CONFIG_BSP_H_
#define BOARD_CONFIG_BSP_H_

/* ==================== INCLUDES ==================== */

#include "drv_flash.h"
#include "clock.h"
#include "clockMan1.h"
#include "drv_timer.h"
#include "drv_can.h"
#include "drv_wdg.h"
#include "common_types.h"
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
#define PROTOCOL_ADDRESS			0x0007F020
#define PROTOCOL_ERROR				0xFF
#define UART_PROTOCOL				0x01
#define CAN_PROTOCOL				0x02
#define APP_STATUS_OK				1
#define UART_COMMAND 				49
#define CAN_COMMAND					50
#define FLASH_WRITE_CMD				51

/* ==================== EXTERN VARIABLES ==================== */
/* Configuration Tables */
extern DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN];


/* CAN Frame Buffers */
/* ==================== FUNCTION DECLARATIONS ==================== */

/* Board Support Package Initialization */
extern void BSP_Init(void);
void BSP_DRV_Config_gv(void);

extern void BSP_TimerDelay(U32 delay_argu32, DRV_TimerDelayUnit_En unit_argen);

#endif /* BOARD_CONFIG_BSP_H_ */
