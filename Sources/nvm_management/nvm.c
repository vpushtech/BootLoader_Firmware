/*
 * nvm.c
 *
 *  Description     : Non-Volatile Memory Manager
 *                    Manages all BMS variables in D-Flash using three strategies:
 *                      Section 1 - SoC + SoH           : Dual-block sequential
 *                                                         wear-levelling. Records
 *                                                         are appended; the active
 *                                                         block rotates when full.
 *                      Section 2 - BootFlag + AppStatus : Single-block sequential
 *                                                         wear-levelling. Block is
 *                                                         erased and restarted when
 *                                                         all 256 phrases are used.
 *                      Section 3 - U32 Shared           : Fixed 8-byte slot per
 *                                                         variable. Entire sector is
 *                                                         erased and rewritten on
 *                                                         every value change.
 *  Author          : Rushikesh
 *  Created On      : 10-Mar-2026
 *  Version         : 2.3
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  10-Mar-2026 RUSHIKESH   Initial implementation – SoC/SoH dual-block layout
 *  20-Mar-2026 RUSHIKESH   Added U32 shared slot block
 *  01-Apr-2026 RUSHIKESH   Added BootFlag + AppStatus single-block region
 *  15-Apr-2026 RUSHIKESH   Removed BootFlag/AppStatus from U32 shared block
 *  01-May-2026 RUSHIKESH   MISRA C:2012 compliance, version 2.3
 ******************************************************************************/

/* ==================== INCLUDE FILES ==================== */

#include <nvm.h>
#include "math.h"

/* ==================== STATIC VARIABLES ==================== */

static U32 NVM_SlotShadow_au32[NVM_U32_SLOT_COUNT];

static NvmBlockControl_tst NVM_SoCCtrl_st =
{
    NVM_SOC_SOH_BLK1_ADDR,
    NVM_SOC_SOH_BLK1_SIZE,
    NVM_SOC_SOH_BLK2_ADDR,
    NVM_SOC_SOH_BLK2_SIZE,
    NVM_SOC_SOH_BLK1_ADDR,
    (NVM_SOC_SOH_BLK1_ADDR + NVM_SOC_SOH_BLK1_SIZE),
    -1.0,
    -1.0
};

static NvmBlockControl_tst NVM_FlagCtrl_st =
{
    NVM_FLAG_BLK_ADDR,
    NVM_FLAG_BLK_SIZE,
    0U,
    0U,
    NVM_FLAG_BLK_ADDR,
    (NVM_FLAG_BLK_ADDR + NVM_FLAG_BLK_SIZE),
    -1.0,
    -1.0
};

static double NVM_CachedSoC_d          = NVM_DEFAULT_SOC_SOH;
static double NVM_CachedSoH_d          = NVM_DEFAULT_SOC_SOH;
static U16    NVM_CachedBootFlag_u16   = 0U;
static U16    NVM_CachedAppStatus_u16  = 0U;

/* ==================== GLOBAL VARIABLES ==================== */

NVM_FlshStoredData_tst NVM_FlshStoredData_st = {0};

/* ==================== SECTION 0 : SHARED LOW-LEVEL HELPERS ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_CalculateCrc16_gen
 *   Description   : Computes CRC-16/ARC (polynomial 0xA001, init 0xFFFF) over
 *                   the supplied byte buffer.
 *   Parameters    : data_argpcu8   - Pointer to the data buffer. Must not be NULL.
 *                   length_argu32  - Number of bytes to process.
 *   Return Value  : U16 - Computed CRC-16 value.
 * -------------------------------------------------------------------------- */
U16 NVM_CalculateCrc16_gen(const U8 *data_argpcu8, U32 length_argu32)
{
    U16       crc_u16  = 0xFFFFU;
    const U16 poly_u16 = 0xA001U;
    U32       i_u32;
    U32       j_u32;

    for (i_u32 = 0U; i_u32 < length_argu32; i_u32++)
    {
        crc_u16 ^= (U16)data_argpcu8[i_u32];

        for (j_u32 = 0U; j_u32 < 8U; j_u32++)
        {
            if ((crc_u16 & 1U) != 0U)
            {
                crc_u16 = (U16)((crc_u16 >> 1U) ^ poly_u16);
            }
            else
            {
                crc_u16 = (U16)(crc_u16 >> 1U);
            }
        }
    }

    return crc_u16;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_EraseFullBlock_gen
 *   Description   : Erases one or more consecutive D-Flash sectors starting at
 *                   blockStart_argu32. Stops immediately on the first sector
 *                   erase failure.
 *   Parameters    : blockStart_argu32 - Start address of the region to erase.
 *                   blockSize_argu32  - Total size in bytes. Must be a multiple
 *                                       of DRV_FLASH_DF_SECTOR_SIZE.
 *   Return Value  : U32 - 0 on success, 1 on erase failure.
 * -------------------------------------------------------------------------- */
static U32 NVM_EraseFullBlock_gen(U32 blockStart_argu32, U32 blockSize_argu32)
{
    U32       addr_u32       = blockStart_argu32;
    U32       remaining_u32  = blockSize_argu32;
    U32       status_u32     = 0U;
    const U32 sectorSize_u32 = DRV_FLASH_DF_SECTOR_SIZE;

    while ((remaining_u32 >= sectorSize_u32) && (status_u32 == 0U))
    {
        if (DRV_FLASH_EraseSector_gen(addr_u32, sectorSize_u32) != DRV_FLASH_SUCCESS)
        {
            status_u32 = 1U;
        }
        else
        {
            addr_u32      += sectorSize_u32;
            remaining_u32 -= sectorSize_u32;
        }
    }

    return status_u32;
}

/* ==================== SECTION 1 : SOC + SOH (DUAL-BLOCK WEAR-LEVELLED) ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_ScanBlock_gen
 *   Description   : Scans a single flash block from startAddr_argu32 for valid
 *                   8-byte records. Stops at the first blank phrase. Returns the
 *                   address, value-A, and value-B of the last valid record found.
 *                   Used by both the SoC/SoH section and the BootFlag/AppStatus
 *                   section because the record format is identical.
 *   Parameters    : startAddr_argu32      - Start address of the block to scan.
 *                   blockSize_argu32      - Size of the block in bytes.
 *                   lastAddr_argptru32    - Output: address of the last valid record.
 *                   lastValA_argptru16    - Output: value-A of the last valid record.
 *                   lastValB_argptru16    - Output: value-B of the last valid record.
 *                   hasBlankAfter_argptrb - Output: true if a blank phrase was found,
 *                                           indicating unused space remains.
 *   Return Value  : bool - true if at least one valid record was found.
 * -------------------------------------------------------------------------- */
static bool NVM_ScanBlock_gen(U32   startAddr_argu32,
                               U32   blockSize_argu32,
                               U32  *lastAddr_argptru32,
                               U16  *lastValA_argptru16,
                               U16  *lastValB_argptru16,
                               bool *hasBlankAfter_argptrb)
{
    U32  addr_u32;
    U8   rec_au8[NVM_RECORD_SIZE];
    U16  calcCrc_u16;
    U16  storedCrc_u16;
    bool found_b      = false;
    bool readOk_b     = true;
    bool crcA_ok_b;
    bool crcB_ok_b;
    bool isBlank_b;

    *hasBlankAfter_argptrb = false;
    *lastAddr_argptru32    = 0U;
    *lastValA_argptru16    = 0U;
    *lastValB_argptru16    = 0U;

    addr_u32 = startAddr_argu32;

    while ((addr_u32 < (startAddr_argu32 + blockSize_argu32)) && (readOk_b == true))
    {
        if (DRV_FLASH_ReadBlock_gen(addr_u32, rec_au8, NVM_RECORD_SIZE) != DRV_FLASH_SUCCESS)
        {
            readOk_b = false;
        }
        else
        {
            isBlank_b = ((rec_au8[0U] == 0xFFU) && (rec_au8[1U] == 0xFFU) &&
                         (rec_au8[2U] == 0xFFU) && (rec_au8[3U] == 0xFFU) &&
                         (rec_au8[4U] == 0xFFU) && (rec_au8[5U] == 0xFFU) &&
                         (rec_au8[6U] == 0xFFU) && (rec_au8[7U] == 0xFFU));

            if (isBlank_b == true)
            {
                *hasBlankAfter_argptrb = true;
                readOk_b = false;
            }
            else
            {
                calcCrc_u16   = NVM_CalculateCrc16_gen(&rec_au8[0U], 2U);
                storedCrc_u16 = (U16)(((U16)rec_au8[2U] << 8U) | (U16)rec_au8[3U]);
                crcA_ok_b     = (calcCrc_u16 == storedCrc_u16);

                calcCrc_u16   = NVM_CalculateCrc16_gen(&rec_au8[4U], 2U);
                storedCrc_u16 = (U16)(((U16)rec_au8[6U] << 8U) | (U16)rec_au8[7U]);
                crcB_ok_b     = (calcCrc_u16 == storedCrc_u16);

                if ((crcA_ok_b == true) && (crcB_ok_b == true))
                {
                    *lastAddr_argptru32 = addr_u32;
                    *lastValA_argptru16 = (U16)(((U16)rec_au8[0U] << 8U) | (U16)rec_au8[1U]);
                    *lastValB_argptru16 = (U16)(((U16)rec_au8[4U] << 8U) | (U16)rec_au8[5U]);
                    found_b = true;
                }

                addr_u32 += NVM_RECORD_SIZE;
            }
        }
    }

    return found_b;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_SoCSoH_SwitchBlock_gen
 *   Description   : Rotates the SoC/SoH write pointer to the alternate block
 *                   when the active block is full, and erases the newly active
 *                   block before use.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void NVM_SoCSoH_SwitchBlock_gen(void)
{
    U32 nextStart_u32;
    U32 nextEnd_u32;
    U32 nextSize_u32;

    if (NVM_SoCCtrl_st.currentWriteAddr_u32 >= NVM_SoCCtrl_st.currentBlockEnd_u32)
    {
        if ((NVM_SoCCtrl_st.currentWriteAddr_u32 >= NVM_SoCCtrl_st.block1Addr_u32) &&
            (NVM_SoCCtrl_st.currentWriteAddr_u32 <
             (NVM_SoCCtrl_st.block1Addr_u32 + NVM_SoCCtrl_st.block1Size_u32)))
        {
            nextStart_u32 = NVM_SoCCtrl_st.block2Addr_u32;
            nextSize_u32  = NVM_SoCCtrl_st.block2Size_u32;
        }
        else
        {
            nextStart_u32 = NVM_SoCCtrl_st.block1Addr_u32;
            nextSize_u32  = NVM_SoCCtrl_st.block1Size_u32;
        }

        nextEnd_u32 = nextStart_u32 + nextSize_u32;

        (void)NVM_EraseFullBlock_gen(nextStart_u32, nextSize_u32);
        NVM_SoCCtrl_st.currentWriteAddr_u32 = nextStart_u32;
        NVM_SoCCtrl_st.currentBlockEnd_u32  = nextEnd_u32;
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_SoCSoH_ReadInternal
 *   Description   : Scans both SoC/SoH blocks on startup, selects the most
 *                   recent valid record, restores the cached values, and
 *                   repositions the write pointer to the next free phrase.
 *                   If no valid record is found in either block, both blocks
 *                   are erased and default values are applied.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void NVM_SoCSoH_ReadInternal(void)
{
    U32  addr1_u32    = 0U;
    U32  addr2_u32    = 0U;
    U16  soc1_u16     = 0U;
    U16  soh1_u16     = 0U;
    U16  soc2_u16     = 0U;
    U16  soh2_u16     = 0U;
    bool blank1_b     = false;
    bool blank2_b     = false;
    bool found1_b;
    bool found2_b;
    U32  bestAddr_u32 = 0U;
    U16  bestSoC_u16  = 0U;
    U16  bestSoH_u16  = 0U;
    bool fromBlock1_b = false;

    found1_b = NVM_ScanBlock_gen(NVM_SoCCtrl_st.block1Addr_u32,
                                  NVM_SoCCtrl_st.block1Size_u32,
                                  &addr1_u32, &soc1_u16, &soh1_u16, &blank1_b);

    found2_b = NVM_ScanBlock_gen(NVM_SoCCtrl_st.block2Addr_u32,
                                  NVM_SoCCtrl_st.block2Size_u32,
                                  &addr2_u32, &soc2_u16, &soh2_u16, &blank2_b);

    if ((found1_b == false) && (found2_b == false))
    {
        NVM_CachedSoC_d                     = NVM_DEFAULT_SOC_SOH;
        NVM_CachedSoH_d                     = NVM_DEFAULT_SOC_SOH;
        NVM_SoCCtrl_st.prevValueA_d         = NVM_DEFAULT_SOC_SOH;
        NVM_SoCCtrl_st.prevValueB_d         = NVM_DEFAULT_SOC_SOH;

        (void)NVM_EraseFullBlock_gen(NVM_SoCCtrl_st.block1Addr_u32,
                                     NVM_SoCCtrl_st.block1Size_u32);
        (void)NVM_EraseFullBlock_gen(NVM_SoCCtrl_st.block2Addr_u32,
                                     NVM_SoCCtrl_st.block2Size_u32);

        NVM_SoCCtrl_st.currentWriteAddr_u32 = NVM_SoCCtrl_st.block1Addr_u32;
        NVM_SoCCtrl_st.currentBlockEnd_u32  =
            NVM_SoCCtrl_st.block1Addr_u32 + NVM_SoCCtrl_st.block1Size_u32;
    }
    else
    {
        if ((found1_b == true) && (found2_b == false))
        {
            bestAddr_u32 = addr1_u32;
            bestSoC_u16  = soc1_u16;
            bestSoH_u16  = soh1_u16;
            fromBlock1_b = true;
        }
        else if ((found1_b == false) && (found2_b == true))
        {
            bestAddr_u32 = addr2_u32;
            bestSoC_u16  = soc2_u16;
            bestSoH_u16  = soh2_u16;
            fromBlock1_b = false;
        }
        else if ((blank1_b == true) && (blank2_b == false))
        {
            bestAddr_u32 = addr1_u32;
            bestSoC_u16  = soc1_u16;
            bestSoH_u16  = soh1_u16;
            fromBlock1_b = true;
        }
        else if ((blank1_b == false) && (blank2_b == true))
        {
            bestAddr_u32 = addr2_u32;
            bestSoC_u16  = soc2_u16;
            bestSoH_u16  = soh2_u16;
            fromBlock1_b = false;
        }
        else if (addr1_u32 > addr2_u32)
        {
            bestAddr_u32 = addr1_u32;
            bestSoC_u16  = soc1_u16;
            bestSoH_u16  = soh1_u16;
            fromBlock1_b = true;
        }
        else
        {
            bestAddr_u32 = addr2_u32;
            bestSoC_u16  = soc2_u16;
            bestSoH_u16  = soh2_u16;
            fromBlock1_b = false;
        }

        NVM_CachedSoC_d             = (double)bestSoC_u16 / NVM_VALUE_SCALE_FACTOR;
        NVM_CachedSoH_d             = (double)bestSoH_u16 / NVM_VALUE_SCALE_FACTOR;
        NVM_SoCCtrl_st.prevValueA_d = NVM_CachedSoC_d;
        NVM_SoCCtrl_st.prevValueB_d = NVM_CachedSoH_d;

        NVM_SoCCtrl_st.currentWriteAddr_u32 = bestAddr_u32 + NVM_RECORD_SIZE;

        if (fromBlock1_b == true)
        {
            NVM_SoCCtrl_st.currentBlockEnd_u32 =
                NVM_SoCCtrl_st.block1Addr_u32 + NVM_SoCCtrl_st.block1Size_u32;

            if (NVM_SoCCtrl_st.currentWriteAddr_u32 >= NVM_SoCCtrl_st.currentBlockEnd_u32)
            {
                (void)NVM_EraseFullBlock_gen(NVM_SoCCtrl_st.block2Addr_u32,
                                             NVM_SoCCtrl_st.block2Size_u32);
                NVM_SoCCtrl_st.currentWriteAddr_u32 = NVM_SoCCtrl_st.block2Addr_u32;
                NVM_SoCCtrl_st.currentBlockEnd_u32  =
                    NVM_SoCCtrl_st.block2Addr_u32 + NVM_SoCCtrl_st.block2Size_u32;
            }
        }
        else
        {
            NVM_SoCCtrl_st.currentBlockEnd_u32 =
                NVM_SoCCtrl_st.block2Addr_u32 + NVM_SoCCtrl_st.block2Size_u32;

            if (NVM_SoCCtrl_st.currentWriteAddr_u32 >= NVM_SoCCtrl_st.currentBlockEnd_u32)
            {
                (void)NVM_EraseFullBlock_gen(NVM_SoCCtrl_st.block1Addr_u32,
                                             NVM_SoCCtrl_st.block1Size_u32);
                NVM_SoCCtrl_st.currentWriteAddr_u32 = NVM_SoCCtrl_st.block1Addr_u32;
                NVM_SoCCtrl_st.currentBlockEnd_u32  =
                    NVM_SoCCtrl_st.block1Addr_u32 + NVM_SoCCtrl_st.block1Size_u32;
            }
        }
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_SoC_Read
 *   Description   : Returns the cached State-of-Charge percentage. Value is
 *                   populated by NVM_Init_gv() at startup.
 *   Parameters    : None.
 *   Return Value  : double - SoC percentage; NVM_DEFAULT_SOC_SOH when blank.
 * -------------------------------------------------------------------------- */
double NVM_SoC_Read(void)
{
    return NVM_CachedSoC_d;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_SoH_Read
 *   Description   : Returns the cached State-of-Health percentage. Value is
 *                   populated by NVM_Init_gv() at startup.
 *   Parameters    : None.
 *   Return Value  : double - SoH percentage; NVM_DEFAULT_SOC_SOH when blank.
 * -------------------------------------------------------------------------- */
double NVM_SoH_Read(void)
{
    return NVM_CachedSoH_d;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_SoCSoH_Update
 *   Description   : Appends a new 8-byte record to the active SoC/SoH block
 *                   only when at least one value has changed by more than
 *                   NVM_VALUE_CHANGE_THRESHOLD. Rotates to the alternate block
 *                   automatically when the active block is full.
 *   Parameters    : soc_d - New State-of-Charge value (percentage, 0.0..100.0).
 *                   soh_d - New State-of-Health value (percentage, 0.0..100.0).
 *   Return Value  : NvmStatus_ten - NVM_STATUS_OK on success or no change
 *                                   needed. NVM_STATUS_WRITE_FAILED on flash
 *                                   write error.
 * -------------------------------------------------------------------------- */
NvmStatus_ten NVM_SoCSoH_Update(double soc_d, double soh_d)
{
    U8            phrase_au8[NVM_RECORD_SIZE] =
                  { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };
    U16           socValue_u16;
    U16           sohValue_u16;
    U16           crc_u16;
    NvmStatus_ten retStatus;
    double        socDelta_d;
    double        sohDelta_d;

    socDelta_d = fabs(soc_d - NVM_SoCCtrl_st.prevValueA_d);
    sohDelta_d = fabs(soh_d - NVM_SoCCtrl_st.prevValueB_d);

    if ((socDelta_d < NVM_VALUE_CHANGE_THRESHOLD) &&
        (sohDelta_d < NVM_VALUE_CHANGE_THRESHOLD))
    {
        retStatus = NVM_STATUS_OK;
    }
    else
    {
        socValue_u16   = (U16)(soc_d * NVM_VALUE_SCALE_FACTOR);
        sohValue_u16   = (U16)(soh_d * NVM_VALUE_SCALE_FACTOR);

        phrase_au8[0U] = (U8)((socValue_u16 >> 8U) & 0xFFU);
        phrase_au8[1U] = (U8)(socValue_u16          & 0xFFU);
        crc_u16        = NVM_CalculateCrc16_gen(&phrase_au8[0U], 2U);
        phrase_au8[2U] = (U8)((crc_u16 >> 8U) & 0xFFU);
        phrase_au8[3U] = (U8)(crc_u16          & 0xFFU);

        phrase_au8[4U] = (U8)((sohValue_u16 >> 8U) & 0xFFU);
        phrase_au8[5U] = (U8)(sohValue_u16          & 0xFFU);
        crc_u16        = NVM_CalculateCrc16_gen(&phrase_au8[4U], 2U);
        phrase_au8[6U] = (U8)((crc_u16 >> 8U) & 0xFFU);
        phrase_au8[7U] = (U8)(crc_u16          & 0xFFU);

        NVM_SoCSoH_SwitchBlock_gen();

        if (DRV_FLASH_WriteBlock_gen(NVM_SoCCtrl_st.currentWriteAddr_u32,
                                      phrase_au8, NVM_RECORD_SIZE) == DRV_FLASH_SUCCESS)
        {
            NVM_SoCCtrl_st.currentWriteAddr_u32 += NVM_RECORD_SIZE;
            NVM_SoCCtrl_st.prevValueA_d          = soc_d;
            NVM_SoCCtrl_st.prevValueB_d          = soh_d;
            NVM_CachedSoC_d                      = soc_d;
            NVM_CachedSoH_d                      = soh_d;
            retStatus = NVM_STATUS_OK;
        }
        else
        {
            retStatus = NVM_STATUS_WRITE_FAILED;
        }
    }

    return retStatus;
}

/* ==================== SECTION 2 : BOOTFLAG + APPSTATUS (SINGLE-BLOCK) ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_FlagBlock_WrapIfNeeded
 *   Description   : Erases the BootFlag/AppStatus block and resets the write
 *                   pointer to the block start when all 256 phrases have been
 *                   consumed.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void NVM_FlagBlock_WrapIfNeeded(void)
{
    if (NVM_FlagCtrl_st.currentWriteAddr_u32 >= NVM_FlagCtrl_st.currentBlockEnd_u32)
    {
        (void)NVM_EraseFullBlock_gen(NVM_FlagCtrl_st.block1Addr_u32,
                                     NVM_FlagCtrl_st.block1Size_u32);
        NVM_FlagCtrl_st.currentWriteAddr_u32 = NVM_FlagCtrl_st.block1Addr_u32;
        NVM_FlagCtrl_st.currentBlockEnd_u32  =
            NVM_FlagCtrl_st.block1Addr_u32 + NVM_FlagCtrl_st.block1Size_u32;
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_FlagBlock_ReadInternal
 *   Description   : Scans the BootFlag/AppStatus block on startup, restores
 *                   cached values from the last valid record, and repositions
 *                   the write pointer. If no valid record is found the block
 *                   is erased and both values are defaulted to 0.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void NVM_FlagBlock_ReadInternal(void)
{
    U32  lastAddr_u32 = 0U;
    U16  bootVal_u16  = 0U;
    U16  appVal_u16   = 0U;
    bool hasBlank_b   = false;
    bool found_b;

    found_b = NVM_ScanBlock_gen(NVM_FlagCtrl_st.block1Addr_u32,
                                 NVM_FlagCtrl_st.block1Size_u32,
                                 &lastAddr_u32,
                                 &bootVal_u16,
                                 &appVal_u16,
                                 &hasBlank_b);

    if (found_b == false)
    {
        NVM_CachedBootFlag_u16               = 0U;
        NVM_CachedAppStatus_u16              = 0U;
        NVM_FlagCtrl_st.prevValueA_d         = 0.0;
        NVM_FlagCtrl_st.prevValueB_d         = 0.0;

        (void)NVM_EraseFullBlock_gen(NVM_FlagCtrl_st.block1Addr_u32,
                                     NVM_FlagCtrl_st.block1Size_u32);

        NVM_FlagCtrl_st.currentWriteAddr_u32 = NVM_FlagCtrl_st.block1Addr_u32;
        NVM_FlagCtrl_st.currentBlockEnd_u32  =
            NVM_FlagCtrl_st.block1Addr_u32 + NVM_FlagCtrl_st.block1Size_u32;
    }
    else
    {
        NVM_CachedBootFlag_u16       = bootVal_u16;
        NVM_CachedAppStatus_u16      = appVal_u16;
        NVM_FlagCtrl_st.prevValueA_d = (double)bootVal_u16;
        NVM_FlagCtrl_st.prevValueB_d = (double)appVal_u16;

        NVM_FlagCtrl_st.currentWriteAddr_u32 = lastAddr_u32 + NVM_RECORD_SIZE;
        NVM_FlagCtrl_st.currentBlockEnd_u32  =
            NVM_FlagCtrl_st.block1Addr_u32 + NVM_FlagCtrl_st.block1Size_u32;

        if (NVM_FlagCtrl_st.currentWriteAddr_u32 >= NVM_FlagCtrl_st.currentBlockEnd_u32)
        {
            (void)NVM_EraseFullBlock_gen(NVM_FlagCtrl_st.block1Addr_u32,
                                         NVM_FlagCtrl_st.block1Size_u32);
            NVM_FlagCtrl_st.currentWriteAddr_u32 = NVM_FlagCtrl_st.block1Addr_u32;
        }
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_BootFlag_Read
 *   Description   : Returns the cached BootFlag value. Value is populated by
 *                   NVM_Init_gv() at startup.
 *   Parameters    : None.
 *   Return Value  : U16 - BootFlag value; 0 when flash is blank.
 * -------------------------------------------------------------------------- */
U16 NVM_BootFlag_Read(void)
{
    return NVM_CachedBootFlag_u16;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_AppStatus_Read
 *   Description   : Returns the cached AppStatus value. Value is populated by
 *                   NVM_Init_gv() at startup.
 *   Parameters    : None.
 *   Return Value  : U16 - AppStatus value; 0 when flash is blank.
 * -------------------------------------------------------------------------- */
U16 NVM_AppStatus_Read(void)
{
    return NVM_CachedAppStatus_u16;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_BootFlagAppStatus_Update
 *   Description   : Appends a new 8-byte record to the BootFlag/AppStatus block
 *                   when either value has changed. Erases and restarts the block
 *                   from offset 0 when all 256 phrases are consumed.
 *   Parameters    : bootFlag_u16  - New BootFlag value  (U16 range).
 *                   appStatus_u16 - New AppStatus value (U16 range).
 *   Return Value  : NvmStatus_ten - NVM_STATUS_OK on success or no change
 *                                   needed. NVM_STATUS_WRITE_FAILED on flash
 *                                   write error.
 * -------------------------------------------------------------------------- */
NvmStatus_ten NVM_BootFlagAppStatus_Update(U16 bootFlag_u16, U16 appStatus_u16)
{
    U8            phrase_au8[NVM_RECORD_SIZE] =
                  { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };
    U16           crc_u16;
    NvmStatus_ten retStatus;
    double        bootNew_d;
    double        appNew_d;

    bootNew_d = (double)bootFlag_u16;
    appNew_d  = (double)appStatus_u16;

    if ((fabs(bootNew_d - NVM_FlagCtrl_st.prevValueA_d) < 1.0) &&
        (fabs(appNew_d  - NVM_FlagCtrl_st.prevValueB_d) < 1.0))
    {
        retStatus = NVM_STATUS_OK;
    }
    else
    {
        phrase_au8[0U] = (U8)((bootFlag_u16 >> 8U) & 0xFFU);
        phrase_au8[1U] = (U8)(bootFlag_u16          & 0xFFU);
        crc_u16        = NVM_CalculateCrc16_gen(&phrase_au8[0U], 2U);
        phrase_au8[2U] = (U8)((crc_u16 >> 8U) & 0xFFU);
        phrase_au8[3U] = (U8)(crc_u16          & 0xFFU);

        phrase_au8[4U] = (U8)((appStatus_u16 >> 8U) & 0xFFU);
        phrase_au8[5U] = (U8)(appStatus_u16          & 0xFFU);
        crc_u16        = NVM_CalculateCrc16_gen(&phrase_au8[4U], 2U);
        phrase_au8[6U] = (U8)((crc_u16 >> 8U) & 0xFFU);
        phrase_au8[7U] = (U8)(crc_u16          & 0xFFU);

        NVM_FlagBlock_WrapIfNeeded();

        if (DRV_FLASH_WriteBlock_gen(NVM_FlagCtrl_st.currentWriteAddr_u32,
                                      phrase_au8, NVM_RECORD_SIZE) == DRV_FLASH_SUCCESS)
        {
            NVM_FlagCtrl_st.currentWriteAddr_u32 += NVM_RECORD_SIZE;
            NVM_FlagCtrl_st.prevValueA_d          = bootNew_d;
            NVM_FlagCtrl_st.prevValueB_d          = appNew_d;
            NVM_CachedBootFlag_u16                = bootFlag_u16;
            NVM_CachedAppStatus_u16               = appStatus_u16;
            retStatus = NVM_STATUS_OK;
        }
        else
        {
            retStatus = NVM_STATUS_WRITE_FAILED;
        }
    }

    return retStatus;
}

/* ==================== SECTION 3 : U32 SHARED BLOCK (SLOT-BASED) ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_U32SlotAddr
 *   Description   : Calculates the flash address of the given slot index within
 *                   the U32 shared block.
 *   Parameters    : slotIndex_argu32 - Slot index (0..NVM_U32_SLOT_COUNT-1).
 *   Return Value  : U32 - Absolute flash address of the slot.
 * -------------------------------------------------------------------------- */
static U32 NVM_U32SlotAddr(U32 slotIndex_argu32)
{
    return (NVM_U32_SHARED_ADDR + (slotIndex_argu32 * (U32)NVM_RECORD_SIZE));
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_ReadSlotRaw
 *   Description   : Reads one U32 slot directly from flash. Returns 0 on a
 *                   blank slot, read failure, or CRC mismatch.
 *   Parameters    : slotIndex_argu32 - Slot index (0..NVM_U32_SLOT_COUNT-1).
 *   Return Value  : U32 - Stored value on success, 0 on any error.
 * -------------------------------------------------------------------------- */
static U32 NVM_ReadSlotRaw(U32 slotIndex_argu32)
{
    U8   rec_au8[NVM_RECORD_SIZE];
    U16  calcCrc_u16;
    U16  storedCrc_u16;
    U32  addr_u32     = NVM_U32SlotAddr(slotIndex_argu32);
    U32  result_u32;
    bool isBlank_b;

    if (DRV_FLASH_ReadBlock_gen(addr_u32, rec_au8, NVM_RECORD_SIZE) != DRV_FLASH_SUCCESS)
    {
        result_u32 = 0U;
    }
    else
    {
        isBlank_b = ((rec_au8[0U] == 0xFFU) && (rec_au8[1U] == 0xFFU) &&
                     (rec_au8[2U] == 0xFFU) && (rec_au8[3U] == 0xFFU));

        if (isBlank_b == true)
        {
            result_u32 = 0U;
        }
        else
        {
            calcCrc_u16   = NVM_CalculateCrc16_gen(&rec_au8[0U], 4U);
            storedCrc_u16 = (U16)(((U16)rec_au8[4U] << 8U) | (U16)rec_au8[5U]);

            if (calcCrc_u16 != storedCrc_u16)
            {
                result_u32 = 0U;
            }
            else
            {
                result_u32 = (((U32)rec_au8[0U] << 24U) |
                              ((U32)rec_au8[1U] << 16U) |
                              ((U32)rec_au8[2U] <<  8U) |
                               (U32)rec_au8[3U]);
            }
        }
    }

    return result_u32;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_WriteSlotRaw
 *   Description   : Builds an 8-byte slot record (4-byte big-endian value,
 *                   2-byte CRC-16, 2-byte 0xFF padding) and writes it to flash
 *                   at the address of the given slot.
 *   Parameters    : slotIndex_argu32 - Slot index (0..NVM_U32_SLOT_COUNT-1).
 *                   value_argu32     - U32 value to store.
 *   Return Value  : NvmStatus_ten - NVM_STATUS_OK on success,
 *                                   NVM_STATUS_WRITE_FAILED on flash error.
 * -------------------------------------------------------------------------- */
static NvmStatus_ten NVM_WriteSlotRaw(U32 slotIndex_argu32, U32 value_argu32)
{
    U8  rec_au8[NVM_RECORD_SIZE] =
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };
    U16           crc_u16;
    U32           addr_u32 = NVM_U32SlotAddr(slotIndex_argu32);
    NvmStatus_ten retStatus;

    rec_au8[0U] = (U8)((value_argu32 >> 24U) & 0xFFU);
    rec_au8[1U] = (U8)((value_argu32 >> 16U) & 0xFFU);
    rec_au8[2U] = (U8)((value_argu32 >>  8U) & 0xFFU);
    rec_au8[3U] = (U8)(value_argu32           & 0xFFU);

    crc_u16     = NVM_CalculateCrc16_gen(&rec_au8[0U], 4U);
    rec_au8[4U] = (U8)((crc_u16 >> 8U) & 0xFFU);
    rec_au8[5U] = (U8)(crc_u16          & 0xFFU);

    if (DRV_FLASH_WriteBlock_gen(addr_u32, rec_au8, NVM_RECORD_SIZE) == DRV_FLASH_SUCCESS)
    {
        retStatus = NVM_STATUS_OK;
    }
    else
    {
        retStatus = NVM_STATUS_WRITE_FAILED;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_U32SharedInit
 *   Description   : Reads all U32 slots from flash into the RAM shadow table
 *                   NVM_SlotShadow_au32 at startup.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void NVM_U32SharedInit(void)
{
    U32 i_u32;

    for (i_u32 = 0U; i_u32 < NVM_U32_SLOT_COUNT; i_u32++)
    {
        NVM_SlotShadow_au32[i_u32] = NVM_ReadSlotRaw(i_u32);
    }
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_U32_WriteSlot
 *   Description   : Updates one U32 slot. When the new value differs from the
 *                   shadow, the entire sector is erased and all non-zero slots
 *                   are rewritten. The shadow table is updated regardless of
 *                   the write outcome so it remains consistent with flash.
 *   Parameters    : slotIndex_argu32 - Slot index (0..NVM_U32_SLOT_COUNT-1).
 *                   newValue_argu32  - New U32 value to store.
 *   Return Value  : NvmStatus_ten - NVM_STATUS_OK on success or no change
 *                                   needed. NVM_STATUS_ERASE_FAILED or
 *                                   NVM_STATUS_WRITE_FAILED on flash error.
 * -------------------------------------------------------------------------- */
static NvmStatus_ten NVM_U32_WriteSlot(U32 slotIndex_argu32, U32 newValue_argu32)
{
    U32           i_u32;
    U32           cache_au32[NVM_U32_SLOT_COUNT];
    NvmStatus_ten retStatus;

    if (newValue_argu32 == NVM_SlotShadow_au32[slotIndex_argu32])
    {
        retStatus = NVM_STATUS_OK;
    }
    else
    {
        for (i_u32 = 0U; i_u32 < NVM_U32_SLOT_COUNT; i_u32++)
        {
            cache_au32[i_u32] = NVM_SlotShadow_au32[i_u32];
        }

        cache_au32[slotIndex_argu32] = newValue_argu32;

        if (NVM_EraseFullBlock_gen(NVM_U32_SHARED_ADDR, NVM_U32_SHARED_SIZE) != 0U)
        {
            retStatus = NVM_STATUS_ERASE_FAILED;
        }
        else
        {
            retStatus = NVM_STATUS_OK;

            for (i_u32 = 0U;
                 (i_u32 < NVM_U32_SLOT_COUNT) && (retStatus == NVM_STATUS_OK);
                 i_u32++)
            {
                if (cache_au32[i_u32] != 0U)
                {
                    retStatus = NVM_WriteSlotRaw(i_u32, cache_au32[i_u32]);
                }
            }
        }

        for (i_u32 = 0U; i_u32 < NVM_U32_SLOT_COUNT; i_u32++)
        {
            NVM_SlotShadow_au32[i_u32] = cache_au32[i_u32];
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_U32_ReadSlot
 *   Description   : Returns the value of the requested slot from the RAM
 *                   shadow table. No flash access is performed at runtime.
 *   Parameters    : slotIndex_argu32 - Slot index (0..NVM_U32_SLOT_COUNT-1).
 *   Return Value  : U32 - Cached slot value.
 * -------------------------------------------------------------------------- */
static U32 NVM_U32_ReadSlot(U32 slotIndex_argu32)
{
    return NVM_SlotShadow_au32[slotIndex_argu32];
}

/* ── Public U32 READ wrappers ── */
U32 NVM_AccumChargeCap_Read(void)       { return NVM_U32_ReadSlot(NVM_SLOT_ACCUM_CHG_CAP);  }
U32 NVM_AccumChargeEnergy_Read(void)    { return NVM_U32_ReadSlot(NVM_SLOT_ACCUM_CHG_ENRG); }
U32 NVM_BatteryCycle_Read(void)         { return NVM_U32_ReadSlot(NVM_SLOT_BAT_CYCLE);       }
U32 NVM_AccumDischargeCap_Read(void)    { return NVM_U32_ReadSlot(NVM_SLOT_ACCUM_DIS_CAP);  }
U32 NVM_AccumDischargeEnergy_Read(void) { return NVM_U32_ReadSlot(NVM_SLOT_ACCUM_DIS_ENRG); }
U32 NVM_PackCapacity_Read(void)         { return NVM_U32_ReadSlot(NVM_SLOT_PACK_CAPACITY);   }
U32 NVM_SdCardCurrentSector_Read(void)  { return NVM_U32_ReadSlot(NVM_SLOT_SD_CARD_SECTOR);  }
U32 NVM_McuReset_Read(void)             { return NVM_U32_ReadSlot(NVM_SLOT_MCU_RESET);        }

/* ── Public U32 UPDATE wrappers ── */
NvmStatus_ten NVM_AccumChargeCap_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_ACCUM_CHG_CAP,  value_u32); }

NvmStatus_ten NVM_AccumChargeEnergy_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_ACCUM_CHG_ENRG, value_u32); }

NvmStatus_ten NVM_BatteryCycle_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_BAT_CYCLE,       value_u32); }

NvmStatus_ten NVM_AccumDischargeCap_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_ACCUM_DIS_CAP,  value_u32); }

NvmStatus_ten NVM_AccumDischargeEnergy_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_ACCUM_DIS_ENRG, value_u32); }

NvmStatus_ten NVM_PackCapacity_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_PACK_CAPACITY,   value_u32); }

NvmStatus_ten NVM_SdCardCurrentSector_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_SD_CARD_SECTOR,  value_u32); }

NvmStatus_ten NVM_McuReset_Update(U32 value_u32)
{ return NVM_U32_WriteSlot(NVM_SLOT_MCU_RESET,        value_u32); }

/* ==================== INITIALISATION ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : NVM_Init_gv
 *   Description   : Initialises the flash driver and restores all NVM-backed
 *                   variables into RAM on startup. Execution order:
 *                     1. Initialise flash driver.
 *                     2. Restore SoC + SoH from dual-block region.
 *                     3. Restore BootFlag + AppStatus from single-block region.
 *                     4. Restore all U32 slots into the shadow table.
 *                     5. Populate the public RAM mirror NVM_FlshStoredData_st.
 *                   If the flash driver fails to initialise, cached defaults
 *                   (0.0 for doubles, 0U for integers) remain in effect.
 *   Parameters    : None.
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
void NVM_Init_gv(void)
{
	DRV_flashStatus_ten flashinitStatus_en;

	flashinitStatus_en = (U32)DRV_FLASH_Init_gen();

    if ((flashinitStatus_en != (U32)DRV_FLASH_SUCCESS) &&
        (flashinitStatus_en != (U32)DRV_FLASH_ALREADY_INITIALIZED))
    {
        /* Flash driver failed – cached defaults remain valid */
    }
    else
    {
        NVM_SoCSoH_ReadInternal();
        NVM_FlagBlock_ReadInternal();
        NVM_U32SharedInit();

        NVM_FlshStoredData_st.APP_SOCpercent_d             = NVM_CachedSoC_d;
        NVM_FlshStoredData_st.APP_SOHpercent_d             = NVM_CachedSoH_d;
        NVM_FlshStoredData_st.APP_AccumChargeCap_u32       = NVM_AccumChargeCap_Read();
        NVM_FlshStoredData_st.APP_AccumChargeEnergy_u32    = NVM_AccumChargeEnergy_Read();
        NVM_FlshStoredData_st.APP_BatteryCycle_u32         = NVM_BatteryCycle_Read();
        NVM_FlshStoredData_st.APP_AccumDischargeCap_u32    = NVM_AccumDischargeCap_Read();
        NVM_FlshStoredData_st.APP_AccumDischargeEnergy_u32 = NVM_AccumDischargeEnergy_Read();
        NVM_FlshStoredData_st.APP_bootFlag_u16             = NVM_CachedBootFlag_u16;
        NVM_FlshStoredData_st.APP_appStatus_u16            = NVM_CachedAppStatus_u16;
        NVM_FlshStoredData_st.APP_PackCapacity_u32         = NVM_PackCapacity_Read();
        NVM_FlshStoredData_st.McuReset                     = (U8)NVM_McuReset_Read();
        NVM_FlshStoredData_st.CurrentSectorAddr_u32        = NVM_SdCardCurrentSector_Read();
    }
}
