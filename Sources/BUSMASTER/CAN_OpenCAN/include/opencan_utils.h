#ifndef OPENCAN_UTILS_H__
#define OPENCAN_UTILS_H__

#include <stdint.h>
#include <Windows.h>
// User-defined includes
#include "OpenCAN_api.h"

// Function prototypes
uint8_t ucScanPort(HANDLE *hSerialPort, uint8_t *pucCOMNumber);
uint8_t vPingPort(HANDLE hSerialPort);
uint8_t ucWriteBytesToSerial(HANDLE hSerialPort, uint8_t *Buf, uint8_t Len);
uint8_t ucReadFrameFromSerial(HANDLE hSerialPort, uint8_t *pucDecodedData, uint32_t ucTimeout);
uint8_t ucReadBytesFromSerial(HANDLE hSerialPort, uint8_t *pucDest, uint8_t ucBytesToRead, uint32_t ucTimeout, uint8_t *msgReadComplete);
void vExtractDataCircularBuffer(uint8_t *src, uint8_t *dst, uint8_t i, uint8_t len);
void vBuildCANFrameFromData(uint8_t *data, CANMsg_Standard_t *msg);
void vSerializeMessage(CANMsg_Standard_t *src, uint8_t *dest);

#endif