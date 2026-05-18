/*
 * can_bootloader.h
 *
 *  Created on: 24-Apr-2026
 *  Author: PandurangaKarapothul
 *  Description: CAN Bootloader Header - Function Declarations for Firmware Update
 *  Version: 1.0
 *  Modification History:
 *  Date            Author              Description
 *  ----------------------------------------------------------------------------
 *  24-Apr-2026     PandurangaKarapothul Initial CAN Bootloader Header
 *  24-Apr-2026     PandurangaKarapothul Added bootloader status definitions
 ******************************************************************************/

#ifndef BOOTLOADER_CAN_BOOTLOADER_H_
#define BOOTLOADER_CAN_BOOTLOADER_H_

#include <can_communication.h>
#include "bsp_config.h"
#include "drv_flash.h"

/* CAN command and response definitions */
#define CAN_RECEIVE_BIN_CMD     (0xAAU)  // Command to receive binary firmware
#define CAN_FLASH_WRITING_ERROR (0xABU)  // Flash write error response
#define CAN_WRONG_APP_ADDRESS   (0xACU)  // Invalid application address response
#define CAN_BIN_FLASHED         (0xADU)  // Firmware flashed successfully response

extern void BSP_ProcessCanComm_gv(void);  // Main CAN bootloader processing function

#endif /* BOOTLOADER_CAN_BOOTLOADER_H_ */
