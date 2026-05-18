/*
 *****************************************************************************
 * drv_nvic.h
 *
 *  Description     : NVIC Driver Header - MISRA C:2012 Compliant
 *  Author          : Rushikesh
 *  Created On      : 15-Jul-2025
 *  Version         : 3.0
 *  Modification History:
 *  Date        Author      Description
 *  ---------------------------------------------------------------------------
 *  08-Jul-2025 RUSHIKESH   NVIC Driver Architecture Implementation
 *  31-Jul-2025 RUSHIKESH   NVIC Testing Done with WDG, GPIOs and Timers
 *  11-Aug-2025 RUSHIKESH   Guidelines Followed Naming Architecture
 *  29-Jan-2026 RUSHIKESH   Added validation, status tracking, error handling
 *  01-May-2026 RUSHIKESH   MISRA C:2012 Compliance
 */

#ifndef DRIVERS_DRV_NVIC_H
#define DRIVERS_DRV_NVIC_H

/* ==================== INCLUDE FILES ==================== */
#include "common_types.h"
#include "interrupt_manager.h"

/* ==================== MACRO DEFINITIONS ==================== */
#define DRV_NVIC_MAX_PRIORITY   (15U)

/* ==================== TYPE DEFINITIONS ==================== */

/** Software index of each managed interrupt source. */
typedef enum
{
    /* GPIO Interrupts */
    NVIC_GPIOA_IRQ          = 0,
    NVIC_GPIOB_IRQ          = 1,
    NVIC_GPIOC_IRQ          = 2,
    NVIC_GPIOD_IRQ          = 3,
    NVIC_GPIOE_IRQ          = 4,
    /* CAN Interrupts */
    NVIC_CAN0_0_15_IRQ      = 5,
    NVIC_CAN0_16_31_IRQ     = 6,
    NVIC_CAN1_0_15_IRQ      = 7,
    NVIC_CAN2_0_15_IRQ      = 8,
    /* Timer Interrupts */
    NVIC_LPIT0_CH0_IRQ      = 9,
    NVIC_LPIT0_CH1_IRQ      = 10,
    NVIC_LPIT0_CH2_IRQ      = 11,
    NVIC_LPIT0_CH3_IRQ      = 12,
    /* ADC Interrupts */
    NVIC_ADC0_IRQ           = 13,
    NVIC_ADC1_IRQ           = 14,
    /* Flash Memory Interrupts */
	NVIC_FTFC_IRQ = 15,
    NVIC_FLASH_FAULT_IRQ    = 16,
    /* I2C Interrupts */
    NVIC_LPI2C0_MASTER_IRQ  = 17,
    NVIC_LPI2C0_SLAVE_IRQ   = 18,
    NVIC_FLEXIO_IRQ         = 19,
    /* Watchdog Interrupt */
    NVIC_WDG_IRQ            = 20,
    /* UART Interrupts */
    NVIC_LPUART0_IRQ        = 21,
    NVIC_LPUART1_IRQ        = 22,
    /* SPI Interrupts */
    NVIC_LPSPI0_IRQ         = 23,
    NVIC_LPSPI1_IRQ         = 24,
    NVIC_LPSPI2_IRQ         = 25,
    MAX_NVIC_IRQ             = 26
} DRV_NVIC_Irq_ten;

/** Driver-level status codes returned by NVIC functions. */
typedef enum
{
    DRV_NVIC_STATUS_OK                  = 0,
    DRV_NVIC_STATUS_ERR                 = 1,
    DRV_NVIC_STATUS_INVALID_IRQ_INDEX   = 2,
    DRV_NVIC_STATUS_INVALID_PRIORITY    = 3,
    DRV_NVIC_STATUS_IRQ_ALREADY_ENABLED = 4,
    DRV_NVIC_STATUS_IRQ_ALREADY_DISABLED= 5
} DRV_NVIC_Status_ten;

/** Maps a software IRQ index to its hardware IRQn_Type number. */
typedef struct
{
    IRQn_Type DRV_irqNumber_en;
} DRV_NVIC_IrqConfig_tst;

/* ==================== FUNCTION DECLARATIONS ==================== */
DRV_NVIC_Status_ten DRV_NVIC_EnableIRQ_gen(DRV_NVIC_Irq_ten IRQIdx_argen);
DRV_NVIC_Status_ten DRV_NVIC_DisableIRQ_gen(DRV_NVIC_Irq_ten IRQIdx_argen);
DRV_NVIC_Status_ten DRV_NVIC_EnableAll_gen(void);
DRV_NVIC_Status_ten DRV_NVIC_DisableAll_gen(void);
DRV_NVIC_Status_ten DRV_NVIC_IRQConfig_gen(DRV_NVIC_Irq_ten IRQIdx_argen,
                                            U8               priority_argu8);
BIN DRV_NVIC_IsIRQEnabled_gb(DRV_NVIC_Irq_ten IRQIdx_argen);

#endif /* DRIVERS_DRV_NVIC_H */
