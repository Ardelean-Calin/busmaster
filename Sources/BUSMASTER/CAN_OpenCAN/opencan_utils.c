#include <stdio.h>

#include "cobs.h"
#include "opencan_utils.h"

uint8_t ucScanPort(HANDLE *hSerialPort, uint8_t *pucCOMNumber)
{
  // Will hold the port name
  char cPortName[7];
  uint8_t pingSuccess = 0U;

  // Try each of the 256 possible COM ports
  for (uint16_t i = 0; i < 256; i++)
  {
    // Iterate through all possible 255 ports
    sprintf(cPortName, "COM%d", i);
    *hSerialPort = CreateFile(cPortName,                    //port name
                              GENERIC_READ | GENERIC_WRITE, //Read/Write
                              0,                            // No Sharing
                              0,                            // No Security
                              OPEN_EXISTING,                // Open existing port only
                              FILE_FLAG_OVERLAPPED,         // Overlapped I/O
                              NULL);
    // Set baudrate. VERY important for Read, even though we don't
    // need to specify it on the uC
    DCB dcb;
    GetCommState(*hSerialPort, &dcb);
    dcb.BaudRate = CBR_256000;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(*hSerialPort, &dcb);

    if (*hSerialPort == INVALID_HANDLE_VALUE)
      continue;

    // Receive events on byte receive
    if (!SetCommMask(*hSerialPort, EV_RXCHAR))
      continue;

    // Try and PING the COM port.
    pingSuccess = vPingPort(*hSerialPort);
    if (pingSuccess)
    {
      *pucCOMNumber = i;
      return 0U;
    }
  }

  // No OpenCAN found
  return 1U;
}

/*
* Waits for (with timeout) and reads a COBS encoded Frame from the given Serial Port
*/
uint8_t ucReadFrameFromSerial(HANDLE hSerialPort, uint8_t *pucDecodedData, uint32_t ucTimeout)
{
  uint8_t ucBufferIndex = 0U;
  uint8_t ucBytesRead = 0U;
  uint8_t ucBytesReadTotal = 0U;
  uint8_t pucCircularBuffer[USB_ENC_PACKET_SIZE];
  uint8_t pucEncodedData[USB_ENC_PACKET_SIZE];

  // Specifies wether the read was complete or not
  uint8_t msgReadComplete = FALSE;
  do
  {
    ucBytesRead = ucReadBytesFromSerial(hSerialPort, &pucCircularBuffer[ucBufferIndex], 1U, ucTimeout, &msgReadComplete);
    ucBytesReadTotal += ucBytesRead;
    if (ucBytesRead)
    {
      // The EOF byte is 0x00 - Read until it comes
      if (pucCircularBuffer[ucBufferIndex] == 0x00U)
      {
        vExtractDataCircularBuffer(pucCircularBuffer, pucEncodedData, ucBufferIndex, USB_ENC_PACKET_SIZE);
        UnStuffData(pucEncodedData, USB_ENC_PACKET_SIZE, pucDecodedData);
        msgReadComplete = TRUE;
      }

      // Increment the ringbuffer index
      ucBufferIndex = (ucBufferIndex + 1) % USB_ENC_PACKET_SIZE;
    }

  } while (!msgReadComplete);

  return ucBytesReadTotal;
}

uint8_t vPingPort(HANDLE hSerialPort)
{
  uint8_t rawTxMsg[USB_DEC_PACKET_SIZE];
  uint8_t encodedTxMsg[USB_ENC_PACKET_SIZE];
  uint8_t rawRxMsg[USB_DEC_PACKET_SIZE];
  uint8_t bytesWritten;

  // Transform our message structure into a byte array
  rawTxMsg[0] = TX_ACK_REQUEST;
  // Rest of the bytes are unimportant. Encode data using COBS
  StuffData(rawTxMsg, USB_DEC_PACKET_SIZE, encodedTxMsg);
  // Put the EOF as the last byte
  encodedTxMsg[USB_ENC_PACKET_SIZE - 1] = 0x00;

  // Send the data off to be written when time allows it. ucWriteBytesToSerial blocks
  // until data was sent or until timeout.
  bytesWritten = ucWriteBytesToSerial(hSerialPort, (uint8_t *)encodedTxMsg, USB_ENC_PACKET_SIZE);
  if (!bytesWritten)
    return 0U;
  else
  {
    // Blocks until we read a valid frame from the COM port or until timeout (100ms)
    uint8_t noBytesRead = ucReadFrameFromSerial(hSerialPort, (uint8_t *)&rawRxMsg, 100U);
    // Check if the received bytes contain a valid acknowledge message. In the future
    // could be the version
    if (noBytesRead && memcmp(&rawRxMsg + 1, "Ardelean Calin", 14))
      return 1U;
    return 0U;
  }
}

void vSerializeMessage(CANMsg_Standard_t *src, uint8_t *dest)
{
  // Command to execute on the OpenCAN
  dest[0] = TX_CAN_SEND;
  dest[1] = src->isExtended;
  dest[2] = (src->msgID & 0xFF000000) >> 24;
  dest[3] = (src->msgID & 0x00FF0000) >> 16;
  dest[4] = (src->msgID & 0x0000FF00) >> 8;
  dest[5] = (src->msgID & 0x000000FF);
  dest[6] = src->DLC;
  // Copy data buffer in preparation for send
  memcpy((void *)&dest[7], src->Data, 8);
}

// Returns the number of bytes written
uint8_t ucWriteBytesToSerial(HANDLE hSerialPort, uint8_t *Buf, uint8_t Len)
{
  DWORD bytesWritten;
  DWORD dwRes;
  OVERLAPPED osWrite = {0};
  osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  if (osWrite.hEvent == NULL)
    // error creating osWrite event handle
    return 0U;

  if (!WriteFile(hSerialPort, (void *)Buf, (DWORD)Len, &bytesWritten, &osWrite))
  {
    // Error while writing file.
    if (GetLastError() != ERROR_IO_PENDING)
    {
      bytesWritten = 0U;
    }
    else
    {
      // Write is pending, so wait for it indefinitely (?)
      dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
      switch (dwRes)
      {
      case WAIT_OBJECT_0:
        if (!GetOverlappedResult(hSerialPort, &osWrite, (LPDWORD)&bytesWritten, FALSE))
        {
          // Error transfering
          bytesWritten = 0U;
        }
        break;
      default:
        bytesWritten = 0U;
        break;
      }
    }
  }

  CloseHandle(osWrite.hEvent);
  return (uint8_t)bytesWritten;
}

uint8_t ucReadBytesFromSerial(HANDLE hSerialPort, uint8_t *pucDest, uint8_t ucBytesToRead, uint32_t ucTimeout, uint8_t *msgReadComplete)
{
  // TODO: Can we simplify this?
  DWORD bytesRead = 0U;
  BOOL fWaitingOnRead = FALSE;

  OVERLAPPED osReader = {0};
  osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (osReader.hEvent == NULL)
    // Error creating overlapped event; abort.
    return 0U;

  // Start a read operation
  if (!fWaitingOnRead)
  {
    // Issue read operation.
    if (!ReadFile(hSerialPort, pucDest, ucBytesToRead, &bytesRead, &osReader))
    {
      if (GetLastError() != ERROR_IO_PENDING) // read not delayed?
        // Error in communications; report it.
        return 0U;
      else
        fWaitingOnRead = TRUE;
    }
    else
    {
      // read completed immediately
      CloseHandle(osReader.hEvent);
      return (uint8_t)bytesRead;
    }
  }

  // Stores result of the wait for event
  DWORD dwRes;

  if (fWaitingOnRead)
  {
    dwRes = WaitForSingleObject(osReader.hEvent, ucTimeout);
    switch (dwRes)
    {
    // Read completed.
    case WAIT_OBJECT_0:
      if (!GetOverlappedResult(hSerialPort, &osReader, &bytesRead, FALSE))
        // Error in communications; report it.
        return 0U;
      else
      {
        // Read completed successfully.
        CloseHandle(osReader.hEvent);
        return (uint8_t)bytesRead;
      }

      //  Reset flag so that another opertion can be issued.
      fWaitingOnRead = FALSE;
      break;
    case WAIT_TIMEOUT:
      *msgReadComplete = TRUE;
      break;
    default:
      break;
    }
  }

  CloseHandle(osReader.hEvent);
  return (uint8_t)bytesRead;
}

/** 
 * Orders the data in the circular buffer.
 */
void vExtractDataCircularBuffer(uint8_t *src, uint8_t *dst, uint8_t i, uint8_t len)
{
  uint8_t upperBytesNo = len - 1 - i;
  memcpy(dst, &src[i + 1], upperBytesNo);
  memcpy(&dst[upperBytesNo], src, i + 1);
}

void vBuildCANFrameFromData(uint8_t *data, CANMsg_Standard_t *msg)
{
  // Create the CAN message from the received data
  msg->isExtended = data[1];
  msg->msgID = (data[2] << 24) + (data[3] << 16) + (data[4] << 8) + data[5];
  msg->DLC = data[6];
  memcpy(msg->Data, &data[7], 8U);
}