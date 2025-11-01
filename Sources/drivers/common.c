/*
 * common.c
 *
 *  Created on: 03-Sep-2025
 *      Author: RushikeshNitinKamble
 */

#include "common_types.h"

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : Common_Memcpy_gv
*   Description   : Copies a specified number of bytes from source to destination
*   Parameters    : dest_argptrmv - Pointer to destination memory location
*                   src_argptrmv  - Pointer to source memory location
*                   size_argstr   - Number of bytes to copy
*   Return Value  : void* - Pointer to destination memory location
*   Note          : Returns destination pointer if source/destination is NULL or size is zero
*  --------------------------------------------------------------------------- */
void *Common_Memcpy_gv(void *dest_argptrmv, const void *src_argptrmv, size_t size_argstr)
{
    if (dest_argptrmv == NULL || src_argptrmv == NULL || size_argstr == 0)
    {
        return dest_argptrmv;
    }
    U8 *dest_argptr_u8 = (uint8_t *)dest_argptrmv;
    const U8 *src_ptru8 = (const U8 *)src_argptrmv;
    for (size_t i = 0; i < size_argstr; i++) {
        dest_argptr_u8[i] = src_ptru8[i];
    }
    return dest_argptrmv;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : Common_Memcmp_gi32
*   Description   : Compares two memory blocks for specified number of bytes
*   Parameters    : ptr1_argptrmv - Pointer to first memory block
*                   ptr2_argptrmv - Pointer to second memory block
*                   size_argst    - Number of bytes to compare
*   Return Value  : I32 - 0 if equal, positive if ptr1 > ptr2, negative if ptr1 < ptr2
*   Note          : Returns 0 if either pointer is NULL or size is zero
*  --------------------------------------------------------------------------- */
I32 Common_Memcmp_gi32(const void *ptr1_argptrmv, const void *ptr2_argptrmv, size_t size_argst)
{
    if (ptr1_argptrmv == NULL || ptr2_argptrmv == NULL || size_argst == 0)
    {
        return 0;
    }

    const U8 *ptr1_mu8 = (const U8 *)ptr1_argptrmv;
    const U8 *ptr2_mu8 = (const U8 *)ptr2_argptrmv;

    for (size_t i = 0; i < size_argst; i++) {
        if (ptr1_mu8[i] != ptr2_mu8[i])
        {
            return (I32)(ptr1_mu8[i] - ptr2_mu8[i]);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : Common_Memset_gv
*   Description   : Fills a memory block with specified byte value
*   Parameters    : ptr_argptrv   - Pointer to memory block to fill
*                   value_argu8   - Value to set each byte to
*                   size_argst    - Number of bytes to set
*   Return Value  : void* - Pointer to filled memory block
*   Note          : Returns original pointer if pointer is NULL or size is zero
*  --------------------------------------------------------------------------- */
void *Common_Memset_gv(void *ptr_argptrv, U8 value_argu8, size_t size_argst)
{
    if (ptr_argptrv == NULL || size_argst == 0)
    {
        return ptr_argptrv;
    }
    U8 *ptr_u8 = (U8 *)ptr_argptrv;
    U8 byte_value_u8 = (U8)value_argu8;

    for (size_t i = 0; i < size_argst; i++)
    {
        ptr_u8[i] = byte_value_u8;
    }
    return ptr_argptrv;
}

