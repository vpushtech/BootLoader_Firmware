/*
 * common_types_interface.h
 *
 *  Created on: 08-Jul-2025
 *  Author: DELL
 */

#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_
/*******************************************************************************
 *  HEADER FILE INCLUDES
 *******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "stddef.h"
/*******************************************************************************
 *  TYPE DEFINITIONS
 *******************************************************************************/

/* ==================== FIXED-WIDTH UNSIGNED INTEGER TYPES ==================== */
typedef uint8_t   U8;
typedef uint16_t  U16;
typedef uint32_t  U32;
typedef uint64_t  U64;

/* ==================== FIXED-WIDTH SIGNED INTEGER TYPES ==================== */
typedef int8_t    I8;
typedef int16_t   I16;
typedef int32_t   I32;
typedef int64_t   I64;

/* ==================== FLOATING-POINT TYPES ==================== */
typedef float     F32;
typedef double    F64;

/* ==================== BOOLEAN TYPE ==================== */
typedef bool      BIN;

/* ==================== CHARACTER TYPE ==================== */
typedef char      CH;

/*******************************************************************************
 *  MEMORY OPERATION FUNCTION DECLARATIONS
 *******************************************************************************/
void *Common_Memcpy_gv(void *dest_argptrmv, const void *src_argptrmv, size_t size_argstr);
I32 Common_Memcmp_gi32(const void *ptr1_argptrmv, const void *ptr2_argptrmv, size_t size_argst);
void *Common_Memset_gv(void *ptr_argptrv, U8 value_argu8, size_t size_argst);
#endif

