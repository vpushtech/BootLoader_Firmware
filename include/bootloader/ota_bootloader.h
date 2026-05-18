/*
 * ota_bootloader.h
 * Author: PandurangaKarapothul
 * Description: BLE OTA Bootloader Header - Frame Format, States, and Function Declarations
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 PandurangaKarapothul Initial OTA bootloader header
PandurangaKarapothul Added sub-command definitions
PandurangaKarapothul Added state machine enumerations
 ******************************************************************************/

#ifndef OTA_BOOTLOADER_H_
#define OTA_BOOTLOADER_H_

/* ==================== INCLUDES ==================== */
#include "drv_uart.h"
#include "common_types.h"

/* ==================== OTA FRAME FORMAT DEFINITIONS ==================== */
#define OTA_SOF                     0x5650U
#define OTA_EOF                     0xAAAAU
#define OTA_HOST_TO_DEVICE          0x00U
#define OTA_DEVICE_TO_HOST          0x01U

/* Frame structure offsets */
#define OTA_PAYLOAD_LEN_MSB_INDEX   4U
#define OTA_PAYLOAD_LEN_LSB_INDEX   5U
#define OTA_FRAME_OVERHEAD          9U
#define OTA_PAYLOAD_LEN_FIELD       2U

/* ==================== OTA COMMAND DEFINITIONS ==================== */
#define OTA_CMD_UPDATE              0x05U

/* OTA sub-commands (within CMD=0x05 payload) */
#define OTA_SUBCMD_TRIGGER          0x36U
#define OTA_SUBCMD_FLASH_CMD        0x33U
#define OTA_SUBCMD_FILE_SIZE        0x01U
#define OTA_SUBCMD_CHUNK            0x02U
#define OTA_SUBCMD_CRC              0x03U

/* ==================== OTA ACKNOWLEDGMENT DEFINITIONS ==================== */
#define OTA_ACK_OK                  0x01U
#define OTA_ACK_FAIL                0x00U

/* ==================== OTA CONFIGURATION ==================== */
#define OTA_MAX_RX_ERRORS           3U
#define OTA_MAX_FAIL_COUNT          5U
#define OTA_MAX_PAYLOAD_BYTES       265U
#define OTA_UART_INSTANCE           DRV_UART_INSTANCE_1

/* ==================== ENUMERATIONS ==================== */
typedef enum
{
    OTA_CMD_INVALID_E    = 0,
    OTA_CMD_OTA_UPDATE_E = 5,
    OTA_CMD_TOTAL_E      = 6
} OTA_Command_ten;

typedef enum
{
    OTA_ACK_IDLE_E                  = 0,
    OTA_ACK_OTA_UPDATE_E            = 1,
    OTA_ACK_RECEIVED_DATA_INVALID_E = 2
} OTA_Ack_ten;

typedef enum
{
    OTA_RX_STATE_START_OF_FRAME_E,
    OTA_RX_STATE_DIRECTION_E,
    OTA_RX_STATE_COMMAND_ID_E,
    OTA_RX_STATE_PAYLOAD_LENGTH_E,
    OTA_RX_STATE_PAYLOAD_DATA_E,
    OTA_RX_STATE_CHECKSUM_E,
    OTA_RX_STATE_END_OF_FRAME_E,
    OTA_RX_STATE_DATA_INVALID_E
} OTA_RxState_ten;

/* ==================== STRUCTURES ==================== */
typedef struct
{
    BIN     receiveFlag_b;
    BIN     transmitFlag_b;

    U8  txBuffer_au8[OTA_MAX_PAYLOAD_BYTES + OTA_FRAME_OVERHEAD];
    U8  txPayloadLength_u8;

    U8  rxCommand_u8;
    U16 rxPayloadLength_u16;
    U8  rxPayloadData_au8[OTA_MAX_PAYLOAD_BYTES];
    U8  rxBuffer_au8[OTA_MAX_PAYLOAD_BYTES + OTA_FRAME_OVERHEAD];

} OTA_Packet_St;

/* ==================== EXTERN VARIABLES ==================== */
extern U8         OTA_UartTxErrorCount_u8;
extern U8         OTA_UartRxErrorCount_u8;
extern BIN            OTA_UartTxInit_b;
extern BIN            OTA_UartRxInit_b;

extern OTA_Packet_St   OTA_Packet_st;
extern OTA_Ack_ten     OTA_Ack_en;
extern OTA_RxState_ten OTA_RxState_en;
extern OTA_Command_ten OTA_Command_en;

extern U8         OTA_StatusIndex_u8;
extern U8         OTA_UartRxByte_u8;
extern BIN        uart_flag_b;
/* ==================== FUNCTION DECLARATIONS ==================== */

void OTA_Init_gv(void);
void OTA_ProcessRxData_gv(U8 rxData_argu8);
void OTA_TxScheduler_gv(void);
void OTA_SendData_gv(OTA_Command_ten command_argen,
                     U16 payloadLen_argu16,
                     U8 *payloadData_argptru8);
U8 OTA_CalculateChecksum_gv(U8 *data_argptru8, U8 length_argu8);
void OTA_ResetBuffer_gv(U8 *buffer_argptru8, U8 length_argu8);
void OTA_ProcessOtaCommand_gv(U8 *payload, U16 payloadLen);
void uart_callback1_gv(void *driverState_argp, uart_event_t event_argin,
                        void *userData_argp);
void uart_callback0_gv(void *driverState_argp, uart_event_t event_argin,
                        void *userData_argp);

#endif /* OTA_BOOTLOADER_H_ */
