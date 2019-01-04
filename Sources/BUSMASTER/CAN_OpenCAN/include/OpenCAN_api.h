#ifndef OPEN_CAN_
#define OPEN_CAN_

#include <Windows.h>
#include <stdint.h>
#include <stddef.h>

/* Defines */
// Fixed packet size for USB messages
#define USB_DEC_PACKET_SIZE (16U)
#define USB_ENC_PACKET_SIZE (USB_DEC_PACKET_SIZE + 2)

/* Typedefs and enums */
typedef struct
{
    uint32_t msgID;
    uint8_t isExtended;
    uint8_t DLC;
    uint8_t Data[8];
} CANMsg_Standard_t;

enum
{
    TX_CAN_SEND = 0,
    RX_CAN_RECV,
    TX_ACK_REQUEST,
    RX_ACK_SEND,
    TX_CAN_SET_BAUDRATE
};

#ifdef __cplusplus
extern "C"
{
#endif
    /*
 * Function prototypes 
 */

    uint8_t OpenCAN_Open(HANDLE *hSerialPort, uint8_t *pucCOMNumber);
    void OpenCAN_Close(HANDLE hSerialPort);
    uint8_t OpenCAN_WriteCAN(HANDLE hSerialPort, CANMsg_Standard_t *txMsg);
    uint8_t OpenCAN_ReadCAN(HANDLE hSerialPort, CANMsg_Standard_t *rxMsg);
#ifdef __cplusplus
}
#endif
#endif