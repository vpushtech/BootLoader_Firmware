/*
 *****************************************************************************
 * drv_nvic.c
 *
 *  Description     : NVIC Driver - MISRA C:2012 Compliant
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
 *  01-May-2026 RUSHIKESH   MISRA C:2012 Compliance Applied
 ******************************************************************************/
/* ==================== INCLUDE FILES ==================== */
#include "drv_nvic.h"
#include "interrupt_manager.h"

/* ==================== PRIVATE CONSTANTS ==================== */
static const DRV_NVIC_IrqConfig_tst DRV_NVIC_IrqTable_arrst[MAX_NVIC_IRQ] =
{
    [NVIC_GPIOA_IRQ]          = { PORTA_IRQn              },
    [NVIC_GPIOB_IRQ]          = { PORTB_IRQn              },
    [NVIC_GPIOC_IRQ]          = { PORTC_IRQn              },
    [NVIC_GPIOD_IRQ]          = { PORTD_IRQn              },
    [NVIC_GPIOE_IRQ]          = { PORTE_IRQn              },

    [NVIC_CAN0_0_15_IRQ]      = { CAN0_ORed_0_15_MB_IRQn  },
    [NVIC_CAN0_16_31_IRQ]     = { CAN0_ORed_16_31_MB_IRQn },
    [NVIC_CAN1_0_15_IRQ]      = { CAN1_ORed_0_15_MB_IRQn  },
    [NVIC_CAN2_0_15_IRQ]      = { CAN2_ORed_0_15_MB_IRQn  },

    [NVIC_LPIT0_CH0_IRQ]      = { LPIT0_Ch0_IRQn          },
    [NVIC_LPIT0_CH1_IRQ]      = { LPIT0_Ch1_IRQn          },
    [NVIC_LPIT0_CH2_IRQ]      = { LPIT0_Ch2_IRQn          },
    [NVIC_LPIT0_CH3_IRQ]      = { LPIT0_Ch3_IRQn          },

    [NVIC_ADC0_IRQ]           = { ADC0_IRQn               },
    [NVIC_ADC1_IRQ]           = { ADC1_IRQn               },

    [NVIC_FTFC_IRQ]            = { FTFC_IRQn               },
    [NVIC_FLASH_FAULT_IRQ]    = { FTFC_Fault_IRQn         },

    [NVIC_LPI2C0_MASTER_IRQ]  = { LPI2C0_Master_IRQn      },
    [NVIC_LPI2C0_SLAVE_IRQ]   = { LPI2C0_Slave_IRQn       },
    [NVIC_FLEXIO_IRQ]         = { FLEXIO_IRQn             },

    [NVIC_WDG_IRQ]            = { WDOG_EWM_IRQn           },

    [NVIC_LPUART0_IRQ]        = { LPUART0_RxTx_IRQn       },
    [NVIC_LPUART1_IRQ]        = { LPUART1_RxTx_IRQn       },

    [NVIC_LPSPI0_IRQ]         = { LPSPI0_IRQn             },
    [NVIC_LPSPI1_IRQ]         = { LPSPI1_IRQn             },
    [NVIC_LPSPI2_IRQ]         = { LPSPI2_IRQn             }
};

/* ==================== PRIVATE STATE ==================== */

static volatile BIN DRV_nvicIrqEnabledStatus_mb[MAX_NVIC_IRQ] = { (BIN)0U };

/* ==================== PRIVATE FUNCTION DECLARATIONS ==================== */
static BIN               DRV_NVIC_IsIdxValid_prv(DRV_NVIC_Irq_ten IRQIdx_argen);
static BIN               DRV_NVIC_IsPriorityValid_prv(U8 priority_argu8);
static DRV_NVIC_Status_ten DRV_NVIC_SetPriority_prv(DRV_NVIC_Irq_ten IRQIdx_argen,
                                                     U8               priority_argu8);

/* ==================== PRIVATE FUNCTIONS ==================== */

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_IsIdxValid_prv
 *  Description   : Returns (BIN)1U when IRQIdx_argen is a valid table index.
 * --------------------------------------------------------------------------- */
static BIN DRV_NVIC_IsIdxValid_prv(DRV_NVIC_Irq_ten IRQIdx_argen)
{
    BIN result_l;
    if((U8)IRQIdx_argen < (U8)MAX_NVIC_IRQ)
    {
        result_l = (BIN)1U;
    }
    else
    {
        result_l = (BIN)0U;
    }

    return result_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_IsPriorityValid_prv
 *  Description   : Returns (BIN)1U when priority_argu8 is within 0..15.
 * --------------------------------------------------------------------------- */
static BIN DRV_NVIC_IsPriorityValid_prv(U8 priority_argu8)
{
    BIN result_l;

    if(priority_argu8 <= (U8)DRV_NVIC_MAX_PRIORITY)
    {
        result_l = (BIN)1U;
    }
    else
    {
        result_l = (BIN)0U;
    }

    return result_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_SetPriority_prv
 *  Description   : Sets the hardware priority for the specified IRQ index.
 *
 *  MISRA Notes:
 *    Rule  2.2 : Removed duplicate index/priority checks that were already
 *               guaranteed valid by the single call-site (IRQConfig_gen).
 *               Guards retained here as a defensive layer for direct calls.
 * --------------------------------------------------------------------------- */
static DRV_NVIC_Status_ten DRV_NVIC_SetPriority_prv(DRV_NVIC_Irq_ten IRQIdx_argen,
                                                     U8               priority_argu8)
{
    DRV_NVIC_Status_ten status_l = DRV_NVIC_STATUS_OK;

    if((BIN)0U == DRV_NVIC_IsIdxValid_prv(IRQIdx_argen))
    {
        status_l = DRV_NVIC_STATUS_INVALID_IRQ_INDEX;
    }
    else if((BIN)0U == DRV_NVIC_IsPriorityValid_prv(priority_argu8))
    {
        status_l = DRV_NVIC_STATUS_INVALID_PRIORITY;
    }
    else
    {
        INT_SYS_SetPriority(
            DRV_NVIC_IrqTable_arrst[IRQIdx_argen].DRV_irqNumber_en,
            priority_argu8);
    }

    return status_l;
}

/* ==================== PUBLIC FUNCTIONS ==================== */

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_EnableIRQ_gen
 *  Description   : Enables the specified interrupt and updates the status flag.
 *
 * --------------------------------------------------------------------------- */
DRV_NVIC_Status_ten DRV_NVIC_EnableIRQ_gen(DRV_NVIC_Irq_ten IRQIdx_argen)
{
    DRV_NVIC_Status_ten status_l = DRV_NVIC_STATUS_OK;

    if((BIN)0U == DRV_NVIC_IsIdxValid_prv(IRQIdx_argen))
    {
        status_l = DRV_NVIC_STATUS_INVALID_IRQ_INDEX;
    }
    else
    {
        if(1U == INT_SYS_GetActive(
                     DRV_NVIC_IrqTable_arrst[IRQIdx_argen].DRV_irqNumber_en))
        {
            status_l = DRV_NVIC_STATUS_IRQ_ALREADY_ENABLED;
        }
        else
        {
            INT_SYS_EnableIRQ(
                DRV_NVIC_IrqTable_arrst[IRQIdx_argen].DRV_irqNumber_en);

            DRV_nvicIrqEnabledStatus_mb[IRQIdx_argen] = (BIN)1U;
        }
    }

    return status_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_DisableIRQ_gen
 *  Description   : Disables the specified interrupt and clears the status flag.
 * --------------------------------------------------------------------------- */
DRV_NVIC_Status_ten DRV_NVIC_DisableIRQ_gen(DRV_NVIC_Irq_ten IRQIdx_argen)
{
    DRV_NVIC_Status_ten status_l = DRV_NVIC_STATUS_OK;

    if((BIN)0U == DRV_NVIC_IsIdxValid_prv(IRQIdx_argen))
    {
        status_l = DRV_NVIC_STATUS_INVALID_IRQ_INDEX;
    }
    else
    {
        if(0U == INT_SYS_GetActive(
                     DRV_NVIC_IrqTable_arrst[IRQIdx_argen].DRV_irqNumber_en))
        {
            status_l = DRV_NVIC_STATUS_IRQ_ALREADY_DISABLED;
        }
        else
        {
            INT_SYS_DisableIRQ(
                DRV_NVIC_IrqTable_arrst[IRQIdx_argen].DRV_irqNumber_en);

            DRV_nvicIrqEnabledStatus_mb[IRQIdx_argen] = (BIN)0U;
        }
    }

    return status_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_EnableAll_gen
 *  Description   : Enables every managed interrupt in sequence.
 * --------------------------------------------------------------------------- */
DRV_NVIC_Status_ten DRV_NVIC_EnableAll_gen(void)
{
    DRV_NVIC_Status_ten status_l = DRV_NVIC_STATUS_OK;
    U8                  idx_l;
    for(idx_l = (U8)NVIC_GPIOA_IRQ;
        idx_l < (U8)MAX_NVIC_IRQ;
        idx_l++)
    {
        status_l = DRV_NVIC_EnableIRQ_gen((DRV_NVIC_Irq_ten)idx_l);
        if((DRV_NVIC_STATUS_OK             != status_l) &&
           (DRV_NVIC_STATUS_IRQ_ALREADY_ENABLED != status_l))
        {
            break;
        }
        else
        {
            status_l = DRV_NVIC_STATUS_OK;
        }
    }

    return status_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_DisableAll_gen
 *  Description   : Disables every managed interrupt in sequence.
 * --------------------------------------------------------------------------- */
DRV_NVIC_Status_ten DRV_NVIC_DisableAll_gen(void)
{
    DRV_NVIC_Status_ten status_l = DRV_NVIC_STATUS_OK;
    U8                  idx_l;

    for(idx_l = (U8)NVIC_GPIOA_IRQ;
        idx_l < (U8)MAX_NVIC_IRQ;
        idx_l++)
    {
        status_l = DRV_NVIC_DisableIRQ_gen((DRV_NVIC_Irq_ten)idx_l);

        if((DRV_NVIC_STATUS_OK                  != status_l) &&
           (DRV_NVIC_STATUS_IRQ_ALREADY_DISABLED != status_l))
        {
            break;
        }
        else
        {
            status_l = DRV_NVIC_STATUS_OK;
        }
    }

    return status_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_IRQConfig_gen
 *  Description   : Sets interrupt priority then enables the interrupt.
 * --------------------------------------------------------------------------- */
DRV_NVIC_Status_ten DRV_NVIC_IRQConfig_gen(DRV_NVIC_Irq_ten IRQIdx_argen,
                                            U8               priority_argu8)
{
    DRV_NVIC_Status_ten status_l;
    status_l = DRV_NVIC_SetPriority_prv(IRQIdx_argen, priority_argu8);

    if(DRV_NVIC_STATUS_OK == status_l)
    {
        status_l = DRV_NVIC_EnableIRQ_gen(IRQIdx_argen);
    }

    return status_l;
}

/* ---------------------------------------------------------------------------
 *  Function Name : DRV_NVIC_IsIRQEnabled_gb
 *  Description   : Returns the software-tracked enabled state of the IRQ.
 * --------------------------------------------------------------------------- */
BIN DRV_NVIC_IsIRQEnabled_gb(DRV_NVIC_Irq_ten IRQIdx_argen)
{
    BIN isEnabled_l = (BIN)0U;
    if((BIN)1U == DRV_NVIC_IsIdxValid_prv(IRQIdx_argen))
    {
        isEnabled_l = DRV_nvicIrqEnabledStatus_mb[IRQIdx_argen];
    }

    return isEnabled_l;
}
