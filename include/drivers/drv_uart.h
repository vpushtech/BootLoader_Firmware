/*******************************************************************************
 *  Description     : UART Driver Header
 *  Author          : Rushikesh
 *  Created On      : 08-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  18-Jul-2025 RUSHIKESH   UART Driver Architecture Implementation
 *  31-Jul-2025 RUSHIKESH   UART Driver Testing completed
 *  11-Aug-2025 RUSHIKESH   Naming Architecture Implementation
 *  26-Jan-2026 RUSHIKESH   Added validation, init tracking, error handling
 *  01-May-2026 RUSHIKESH   MISRA C 2012 Compliance
 */

#ifndef DRV_UART_H
#define DRV_UART_H

/* ==================== INCLUDE FILES ==================== */
#include "common_types.h"
#include "lpuart_driver.h"
#include "lpuart1.h"
#include "drv_nvic.h"

/* ==================== MACROS ==================== */
#define DRV_UART_TIMEOUT_MS            (100U)
#define DRV_UART_DEFAULT_IRQ_PRIORITY  (3U)
#define DRV_UART_MAX_DATA_SIZE         (1024U)

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum
{
    DRV_UART_INSTANCE_0   = 0,
    DRV_UART_INSTANCE_1   = 1,
    DRV_MAX_UART_INSTANCE = 2
} DRV_uartInstance_ten;
typedef enum
{
    DRV_UART_SUCCESS               = 0,
    DRV_UART_FAILED                = 1,
    DRV_UART_BUSY                  = 2,
    DRV_UART_TIMEOUT               = 3,
    DRV_UART_INVALID_INSTANCE      = 4,
    DRV_UART_INVALID_DATA_SIZE     = 5,
    DRV_UART_NULL_POINTER          = 6,
    DRV_UART_NOT_INITIALIZED       = 7,
    DRV_UART_INTERRUPT_NOT_ENABLED = 8,
    DRV_UART_ALREADY_INITIALIZED   = 9,
    DRV_UART_INIT_STATUS           = 10
} DRV_uartStatus_ten;

/* ==================== GLOBAL VARIABLE DECLARATIONS ==================== */
extern volatile BIN DRV_uartInitStatus_mb[DRV_MAX_UART_INSTANCE];

/* ==================== FUNCTION PROTOTYPES ==================== */
extern DRV_uartStatus_ten DRV_UART_Init_gen(U8 uartPinIdx_argu8);
extern DRV_uartStatus_ten DRV_UART_DeInit_gen(U8 uartPinIdx_argu8);
extern DRV_uartStatus_ten DRV_UART_WriteBlock_gen(
    U8        uartPinIdx_argu8,
    const U8 *data_argptru8,
    U32       size_argu32);

extern DRV_uartStatus_ten DRV_UART_ReadBlock_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32);

extern DRV_uartStatus_ten DRV_UART_ReadNonBlock_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32);

extern DRV_uartStatus_ten DRV_UART_SetRxBuffer_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32);
extern void uart_callback0_gv(
    void          *driverState_argp,
    uart_event_t   event_arg,
    void          *userData_argp);

extern void uart_callback1_gv(
    void          *driverState_argp,
    uart_event_t   event_arg,
    void          *userData_argp);

#endif /* DRV_UART_H */
