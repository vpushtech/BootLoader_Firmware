/*
 ******************************************************************************
 * @file         common.h
 * @brief        Common Types, Macros, and Utility Function Declarations
 * @author       Rushikesh
 * @date         03-Sep-2025
 * @version      2.0
 *
 * Modification History:
 * Date        Author      Description
 * ----------------------------------------------------------------------------
 * 03-Sep-2025 RUSHIKESH   Common Utility Functions Implementation
 * 29-Jan-2026 RUSHIKESH   Added validation, status tracking, and error handling
 * 01-May-2026 RUSHIKESH   Applied MISRA C:2012 compliance
 ******************************************************************************
 */

/* MISRA C:2012 Rule 4.10 – Include guard must be present in every header */
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/* ==================== HEADER FILE INCLUDES ==================== */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==================== MACRO DEFINITIONS ==================== */
#define COMMON_MAX_FLAGS             (32U)
#define COMMON_MEMORY_NULL_CHECK(ptr)   ((ptr) == NULL)

/* ==================== TYPE DEFINITIONS ==================== */

/* ---- Fixed-width unsigned integer types ---------------------------------- */
typedef uint8_t   U8;
typedef uint16_t  U16;
typedef uint32_t  U32;
typedef uint64_t  U64;

/* ---- Fixed-width signed integer types ------------------------------------ */
typedef int8_t    I8;
typedef int16_t   I16;
typedef int32_t   I32;
typedef int64_t   I64;

/* ---- Floating-point types ------------------------------------------------ */
typedef float     F32;
typedef double    F64;

/* ---- Boolean type -------------------------------------------------------- */
typedef bool      BIN;

/* ---- Character type ------------------------------------------------------ */
typedef char      CH;

/* ==================== STATUS ENUMERATION ==================== */
typedef enum
{
    COMMON_STATUS_OK               = 0,
    COMMON_STATUS_ERROR            = 1,
    COMMON_STATUS_NULL_POINTER     = 2,
    COMMON_STATUS_INVALID_SIZE     = 3,
    COMMON_STATUS_INVALID_INDEX    = 4,
    COMMON_STATUS_BUFFER_OVERFLOW  = 5
} COMMON_Status_ten;

/* ==================== APPLICATION FLAG ENUMERATION ==================== */
typedef enum
{
    APP_CAN_FLAG                    =  0,
    APP_BMS_ALERT_FAULT             =  1,
    APP_AFE_COMMUNICATION_FAULT     =  2,
    APP_SD_CARD_COMMUNICATION_FAULT =  3,
    APP_TESTCANMODE                 =  4,
    APP_BMS_CONFIG_FLAG             =  5,
    APP_BMS_CALIBRATION_FLAG        =  6,
    APP_SDCARD                      =  7,
    APP_TRANSIT_IOT                 =  8,
    APP_CAN_BMS_TEST                =  9,
    APP_TOTAL_FLAGS                 = 10
} APP_Flag_En;

/* ==================== GLOBAL VARIABLES ==================== */
extern BIN APP_Set_Flag[APP_TOTAL_FLAGS];

/* ==================== FUNCTION DECLARATIONS ==================== */

COMMON_Status_ten Common_Memcpy_gst(void * dest_argptrmv,
                                     const void * src_argptrmv,
                                     size_t size_argstr);
COMMON_Status_ten Common_Memset_gst(void * ptr_argptrv,
                                     U8 value_argu8,
                                     size_t size_argst);
COMMON_Status_ten APP_SetFlag_gst(U8 Set_Idx_argu8);
COMMON_Status_ten APP_ResetFlag_gst(U8 Reset_Idx_argu8);
BIN APP_GetFlag_gv(U8 Get_Idx_argu8);

#endif /* COMMON_TYPES_H */
