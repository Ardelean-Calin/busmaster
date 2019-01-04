#include "cobs.h"
#include "OpenCAN_api.h"
#include "opencan_utils.h"
#include <stdint.h>
#include <stddef.h>

// Connect to an OpenCAN defice if any is found on the Serial Port
// @returns 0 if no error, error code else
uint8_t OpenCAN_Open(HANDLE *hSerialPort, uint8_t *pucCOMNumber)
{
    uint8_t error = ucScanPort(hSerialPort, pucCOMNumber);

    // No OpenCAN found
    if (error)
        return 1U;

    return 0U;
}

void OpenCAN_Close(HANDLE hSerialPort)
{
    CloseHandle(hSerialPort);
}

/**
 * Blocks until we send a CAN Message.
 */
uint8_t OpenCAN_WriteCAN(HANDLE hSerialPort, CANMsg_Standard_t *txMsg)
{
    uint8_t rawMsg[USB_DEC_PACKET_SIZE];
    uint8_t encodedMsg[USB_ENC_PACKET_SIZE];
    uint8_t bytesWritten;

    // Transform our message structure into a byte array
    vSerializeMessage(txMsg, &rawMsg[0]);
    // Encode data using COBS
    StuffData(rawMsg, USB_DEC_PACKET_SIZE, encodedMsg);
    // Put the EOF as the last byte
    encodedMsg[USB_ENC_PACKET_SIZE - 1] = 0x00;

    // Send the data off to be written when time allows it. ucWriteBytesToSerial blocks
    // until data was sent or until timeout.
    bytesWritten = ucWriteBytesToSerial(hSerialPort, (uint8_t *)encodedMsg, USB_ENC_PACKET_SIZE);
    if (!bytesWritten)
        return 1U;
    else
        return 0U;
}

/**
 * Blocks until we receive a correctly formatted RX Message.
 */
uint8_t OpenCAN_ReadCAN(HANDLE hSerialPort, CANMsg_Standard_t *rxMsg)
{
    uint8_t ucBytesReadTotal = 0U;
    uint8_t pucDecodedData[USB_DEC_PACKET_SIZE];

    ucBytesReadTotal = ucReadFrameFromSerial(hSerialPort, &pucDecodedData[0], INFINITE);
    vBuildCANFrameFromData(pucDecodedData, rxMsg);

    // Return the number of bytes read in total
    return ucBytesReadTotal;
}