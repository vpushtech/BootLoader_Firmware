/*
 ******************************************************************************
 * @file         common.c
 * @brief        Common Utility Functions Implementation
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

/* ==================== INCLUDE FILES ==================== */
#include "common_types.h"

/* ==================== GLOBAL VARIABLES ==================== */
BIN APP_Set_Flag[APP_TOTAL_FLAGS] =
{
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false,
    (BIN)false
};

/* ==================== FUNCTION DEFINITIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function   : Common_Memcpy_gst
 *  Description: Copies size_argstr bytes from src_argptrmv to dest_argptrmv.
 *               Handles overlapping regions by copying in reverse order.
 *  Parameters : dest_argptrmv – non-NULL destination pointer
 *               src_argptrmv  – non-NULL source pointer
 *               size_argstr   – number of bytes to copy (must be > 0)
 *  Returns    : COMMON_STATUS_OK on success; error code otherwise
 * ---------------------------------------------------------------------------*/
COMMON_Status_ten Common_Memcpy_gst(void * dest_argptrmv,
                                     const void * src_argptrmv,
                                     size_t size_argstr)
{
    COMMON_Status_ten retStatus_en;
    if (dest_argptrmv == NULL)
    {
        retStatus_en = COMMON_STATUS_NULL_POINTER;
    }
    else if (src_argptrmv == NULL)
    {
        retStatus_en = COMMON_STATUS_NULL_POINTER;
    }
    else if (size_argstr == (size_t)0U)
    {
        retStatus_en = COMMON_STATUS_INVALID_SIZE;
    }
    else if (dest_argptrmv == src_argptrmv)
    {
        retStatus_en = COMMON_STATUS_OK;
    }
    else
    {

        U8 * const       destByte_pu8 = (U8 *)dest_argptrmv;
        const U8 * const srcByte_pu8  = (const U8 *)src_argptrmv;
        if ((destByte_pu8 > srcByte_pu8) &&
            (destByte_pu8 < (srcByte_pu8 + size_argstr)))
        {
            size_t idx_str = size_argstr;
            while (idx_str > (size_t)0U)
            {
                idx_str--;
                destByte_pu8[idx_str] = srcByte_pu8[idx_str];
            }
        }
        else
        {
            size_t idx_str;
            for (idx_str = (size_t)0U; idx_str < size_argstr; idx_str++)
            {
                destByte_pu8[idx_str] = srcByte_pu8[idx_str];
            }
        }

        retStatus_en = COMMON_STATUS_OK;
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : Common_Memset_gst
 *  Description: Fills size_argst bytes starting at ptr_argptrv with value_argu8.
 *  Parameters : ptr_argptrv  – non-NULL pointer to memory block
 *               value_argu8  – byte value to write
 *               size_argst   – number of bytes to set (must be > 0)
 *  Returns    : COMMON_STATUS_OK on success; error code otherwise
 * ---------------------------------------------------------------------------*/
COMMON_Status_ten Common_Memset_gst(void * ptr_argptrv,
                                     U8 value_argu8,
                                     size_t size_argst)
{
    COMMON_Status_ten retStatus_en;
    if (ptr_argptrv == NULL)
    {
        retStatus_en = COMMON_STATUS_NULL_POINTER;
    }
    else if (size_argst == (size_t)0U)
    {
        retStatus_en = COMMON_STATUS_INVALID_SIZE;
    }
    else
    {
        U8 * const ptr_pu8 = (U8 *)ptr_argptrv;
        size_t     idx_str;

        for (idx_str = (size_t)0U; idx_str < size_argst; idx_str++)
        {
            ptr_pu8[idx_str] = value_argu8;
        }

        retStatus_en = COMMON_STATUS_OK;
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : APP_SetFlag_gst
 *  Description: Sets the flag at index Set_Idx_argu8 to true.
 *  Parameters : Set_Idx_argu8 – flag index (must be < APP_TOTAL_FLAGS)
 *  Returns    : COMMON_STATUS_OK on success; COMMON_STATUS_INVALID_INDEX otherwise
 * ---------------------------------------------------------------------------*/
COMMON_Status_ten APP_SetFlag_gst(U8 Set_Idx_argu8)
{
    COMMON_Status_ten retStatus_en;
    if (Set_Idx_argu8 >= (U8)APP_TOTAL_FLAGS)
    {
        retStatus_en = COMMON_STATUS_INVALID_INDEX;
    }
    else
    {
        APP_Set_Flag[Set_Idx_argu8] = (BIN)true;
        retStatus_en = COMMON_STATUS_OK;
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : APP_ResetFlag_gst
 *  Description: Resets the flag at index Reset_Idx_argu8 to false.
 *  Parameters : Reset_Idx_argu8 – flag index (must be < APP_TOTAL_FLAGS)
 *  Returns    : COMMON_STATUS_OK on success; COMMON_STATUS_INVALID_INDEX otherwise
 * ---------------------------------------------------------------------------*/
COMMON_Status_ten APP_ResetFlag_gst(U8 Reset_Idx_argu8)
{
    COMMON_Status_ten retStatus_en;

    if (Reset_Idx_argu8 >= (U8)APP_TOTAL_FLAGS)
    {
        retStatus_en = COMMON_STATUS_INVALID_INDEX;
    }
    else
    {
        APP_Set_Flag[Reset_Idx_argu8] = (BIN)false;
        retStatus_en = COMMON_STATUS_OK;
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : APP_GetFlag_gv
 *  Description: Returns the current state of the flag at index Get_Idx_argu8.
 *  Parameters : Get_Idx_argu8 – flag index (must be < APP_TOTAL_FLAGS)
 *  Returns    : BIN – true if flag set; false if clear or index invalid
 * ---------------------------------------------------------------------------*/
BIN APP_GetFlag_gv(U8 Get_Idx_argu8)
{
    BIN retFlag_b;

    if (Get_Idx_argu8 >= (U8)APP_TOTAL_FLAGS)
    {
        retFlag_b = (BIN)false;
    }
    else
    {
        retFlag_b = APP_Set_Flag[Get_Idx_argu8];
    }

    return retFlag_b;
}
