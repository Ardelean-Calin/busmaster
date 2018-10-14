#ifndef OPEN_CAN_
#define OPEN_CAN_

#include <Windows.h>
#include <stdint.h>
#include <stddef.h>

/* Defines */
#define TX_MSG_DEC_SIZE (11)
#define TX_MSG_ENC_SIZE (TX_MSG_DEC_SIZE + 2)
#define RX_MSG_ENC_SIZE (12)
#define RX_MSG_DEC_SIZE (RX_MSG_ENC_SIZE - 2)

/* Typedefs and enums */
typedef struct
{
    uint16_t msgID;
    uint8_t DLC;
    uint8_t Data[8];
} CANMsg_Standard_t;

enum
{
    CAN_WRITE_MSG = 0,
    CAN_CHANGE_BAUDRATE
};

#ifdef __cplusplus
extern "C"
{
#endif
    /*
 * Function prototypes 
 */
    void *OpenCAN_Open(char *portName);
    void OpenCAN_Close(HANDLE handle);
    uint8_t OpenCAN_WriteCAN(HANDLE hSerialPort, CANMsg_Standard_t *txMsg);
    uint8_t OpenCAN_ReadCAN(HANDLE hSerialPort, CANMsg_Standard_t *rxMsg);
#ifdef __cplusplus
}
#endif
#endif