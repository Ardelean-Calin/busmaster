/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
* \file      CAN_OpenCAN/CAN_OpenCAN.cpp
* \author    Ardelean Calin
* \copyright Copyright (c) 2018, Robert Bosch Engineering and Business Solutions. All rights reserved.
*
* Implementation of OPENCAN
*/

/* C++ includes */
#include <string>
#include <vector>
#include <stdio.h>

/* Project includes */
#include "CAN_OpenCAN_stdafx.h"
#include "CAN_OpenCAN.h"
//#include "include/Error.h"
//#include "Include/Struct_CAN.h"
//#include "Include/BaseDefs.h"
//#include "Include/CanUSBDefs.h"
//#include "Include/DIL_CommonDefs.h"

// #include "DataTypes/MsgBufAll_DataTypes.h"
#include "MsgBufFSE.h"
#include "BusEmulation/BusEmulation.h"
#include "BusEmulation/BusEmulation_i.c"
//#include "DataTypes/DIL_DataTypes.h"
#include "Utility/Utility_Thread.h"
#include "Utility/Utility.h"
#include "BaseDIL_CAN_Controller.h"
//#include "../BusmasterKernel/BusmasterDriverInterface/Include/CANDriverDefines.h"

//#include "DIL_Interface/BaseDIL_CAN_Controller.h"
//#include "../Application/GettextBusmaster.h"
#include "OpenCAN_api.h"
#include "Utility\MultiLanguageSupport.h"

#define USAGE_EXPORT
#include "CAN_OpenCAN_Extern.h"

typedef struct {
	DWORD dwClientID;
	uint8_t* ucBuffer;
} xClient_t;


/* Global parameters */
static HANDLE			hcan;
static xClient_t xClient;
static int lastIndex = 0;
CBaseCANBufFSE* xMsgBuf[16];
static DWORD startTimestamp;
static CPARAM_THREADPROC sg_sParmRThread;
static CRITICAL_SECTION sg_DIL_CriticalSection;

HANDLE hActionEvent = nullptr;

DWORD WINAPI CanWaitForRx(LPVOID pVoid);

#define CALLBACK_TYPE __stdcall

// Handles and ID related to RX
static HANDLE sg_hReadThread = nullptr;
static DWORD sg_dwReadThreadId = 0;

/* CDIL_CAN_OPENCAN class definition */
class CDIL_CAN_OPENCAN : public CBaseDIL_CAN_Controller
{
public:
	/* STARTS IMPLEMENTATION OF THE INTERFACE FUNCTIONS... */
	HRESULT CAN_PerformInitOperations(void);
	HRESULT CAN_PerformClosureOperations(void);
	HRESULT CAN_GetTimeModeMapping(SYSTEMTIME& CurrSysTime, UINT64& TimeStamp, LARGE_INTEGER& QueryTickCount);
	HRESULT CAN_ListHwInterfaces(INTERFACE_HW_LIST& sSelHwInterface, INT& nCount, PSCONTROLLER_DETAILS InitData);
	HRESULT CAN_SelectHwInterface(const INTERFACE_HW_LIST& sSelHwInterface, INT nCount);
	HRESULT CAN_DeselectHwInterface(void);
	HRESULT CAN_SetConfigData(PSCONTROLLER_DETAILS InitData, int Length);
	HRESULT CAN_StartHardware(void);
	HRESULT CAN_StopHardware(void);
	HRESULT CAN_GetCurrStatus(STATUSMSG& StatusData);
	HRESULT CAN_GetTxMsgBuffer(BYTE*& pouFlxTxMsgBuffer);
	HRESULT CAN_SendMsg(DWORD dwClientID, const STCAN_MSG& sCanTxMsg);
	HRESULT CAN_GetBusConfigInfo(BYTE* BusInfo);
	HRESULT CAN_GetLastErrorString(std::string& acErrorStr);
	HRESULT CAN_GetControllerParams(LONG& lParam, UINT nChannel, ECONTR_PARAM eContrParam);
	HRESULT CAN_SetControllerParams(int nValue, ECONTR_PARAM eContrparam);
	HRESULT CAN_GetErrorCount(SERROR_CNT& sErrorCnt, UINT nChannel, ECONTR_PARAM eContrParam);

	// Specific function set
	HRESULT CAN_SetAppParams(HWND hWndOwner);
	HRESULT CAN_ManageMsgBuf(BYTE bAction, DWORD ClientID, CBaseCANBufFSE* pBufObj);
	HRESULT CAN_RegisterClient(BOOL bRegister, DWORD& ClientID, char* pacClientName);
	HRESULT CAN_GetCntrlStatus(const HANDLE& hEvent, UINT& unCntrlStatus);
	HRESULT CAN_LoadDriverLibrary(void);
	HRESULT CAN_UnloadDriverLibrary(void);
	HRESULT CAN_SetHardwareChannel(PSCONTROLLER_DETAILS, DWORD dwDriverId, bool bIsHardwareListed, unsigned int unChannelCount);
};

CDIL_CAN_OPENCAN* g_pouDIL_CAN_OPENCAN = nullptr;

USAGEMODE HRESULT GetIDIL_CAN_Controller(void** ppvInterface)
{
    HRESULT hResult;

    hResult = S_OK;
    if (!g_pouDIL_CAN_OPENCAN)
    {
        g_pouDIL_CAN_OPENCAN = new CDIL_CAN_OPENCAN;
        if (!(g_pouDIL_CAN_OPENCAN))
        {
            hResult = S_FALSE;
        }
    }
    *ppvInterface = (void*)g_pouDIL_CAN_OPENCAN;

    return(hResult);
}
static STCANDATA sg_asCANMsg;

DWORD WINAPI CanWaitForRx(LPVOID pVoid)
{
	CANMsg_Standard_t rxMsg;
    STCAN_MSG sCanRxMsg;
	uint8_t ucBytesRead;

	for (;;)
	{
		ucBytesRead = OpenCAN_ReadCAN(hcan, &rxMsg);

		if (ucBytesRead)
		{
			// Create RX message structure
			sCanRxMsg.m_bCANFD = false;
			sCanRxMsg.m_ucChannel = 1;
			memcpy(sCanRxMsg.m_ucData, rxMsg.Data, rxMsg.DLC);
			sCanRxMsg.m_ucDataLen = rxMsg.DLC;
			sCanRxMsg.m_ucEXTENDED = rxMsg.isExtended;
			sCanRxMsg.m_unMsgID = rxMsg.msgID;

			sg_asCANMsg.m_lTickCount.QuadPart = (GetTickCount() - startTimestamp) * 10;
			sg_asCANMsg.m_uDataInfo.m_sCANMsg = sCanRxMsg;
			sg_asCANMsg.m_ucDataType = RX_FLAG;
			EnterCriticalSection(&sg_DIL_CriticalSection);
			
			for(int i = 0; i < 7; i++)
			{
				/* code */
				xMsgBuf[i]->WriteIntoBuffer(&sg_asCANMsg);
			}
			
			LeaveCriticalSection(&sg_DIL_CriticalSection);
			// SetEvent(hActionEvent);
		}
		else
		{
			// Show error frame?
			continue;
		}		
	}

	return 0;
}


HRESULT CDIL_CAN_OPENCAN::CAN_StartHardware(void)
{
	uint8_t comport;
	uint8_t error = OpenCAN_Open(&hcan, &comport);
	if (error)
	{
		return ERR_LOAD_HW_INTERFACE;
	}
	else
	{
		// sg_hEventRecv = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		// Create receive thread
		sg_hReadThread = CreateThread(nullptr, 1000, CanWaitForRx, nullptr, 0, &sg_dwReadThreadId);
		// xMsgBuf->vClearMessageBuffer();
		startTimestamp = GetTickCount();
		// sg_sParmRThread.bStartThread(CanWaitForRx);
		return S_OK;
	}
}


HRESULT CDIL_CAN_OPENCAN::CAN_StopHardware(void)
{
	if (sg_hReadThread != nullptr)
    {
        TerminateThread(sg_hReadThread, 0);
        sg_hReadThread = nullptr;
    }
	// Terminate read thread
	// sg_sParmRThread.bTerminateThread();
	OpenCAN_Close(hcan);
	return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SendMsg(DWORD dwClientID, const STCAN_MSG& sCanTxMsg)
{

	CANMsg_Standard_t txMsg;
	memset((void*)txMsg.Data, 0U, 8);

	txMsg.msgID = (uint16_t)sCanTxMsg.m_unMsgID;
	txMsg.DLC = (uint8_t)sCanTxMsg.m_ucDataLen;
	txMsg.isExtended = (uint8_t)sCanTxMsg.m_ucEXTENDED;
	memcpy((void*)txMsg.Data, (void*)sCanTxMsg.m_ucData, txMsg.DLC);

	OpenCAN_WriteCAN(hcan, &txMsg);

    // Write into the client message buffer, to be displayed in the message window
    STCANDATA data;
	data.m_lTickCount.QuadPart = (GetTickCount() - startTimestamp) * 10;
	data.m_uDataInfo.m_sCANMsg = sCanTxMsg;
	data.m_ucDataType = TX_FLAG;
	EnterCriticalSection(&sg_DIL_CriticalSection);
	for(int i = 0; i < 7; i++)
	{
		/* code */
		xMsgBuf[i]->WriteIntoBuffer(&data);
	}
	// xMsgBuf->WriteIntoBuffer(&data);	
	LeaveCriticalSection(&sg_DIL_CriticalSection);

	return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_PerformInitOperations(void)
{
	DWORD dwClientID = 0;
	
	// sg_sParmRThread.m_hActionEvent = CreateEvent(nullptr, false,
    //             false, nullptr);
	// ResetEvent(sg_sParmRThread.m_hActionEvent);
	InitializeCriticalSection(&sg_DIL_CriticalSection);
    CAN_RegisterClient(TRUE, dwClientID, CAN_MONITOR_NODE);
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_PerformClosureOperations(void)
{
	DeleteCriticalSection(&sg_DIL_CriticalSection);
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetTimeModeMapping(SYSTEMTIME &CurrSysTime, UINT64 &TimeStamp, LARGE_INTEGER &QueryTickCount)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_ListHwInterfaces(INTERFACE_HW_LIST &sSelHwInterface, INT &nCount, PSCONTROLLER_DETAILS InitData)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SelectHwInterface(const INTERFACE_HW_LIST &sSelHwInterface, INT nCount)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_DeselectHwInterface(void)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SetConfigData(PSCONTROLLER_DETAILS InitData, int Length)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetCurrStatus(STATUSMSG &StatusData)
{
	StatusData.wControllerStatus = NORMAL_ACTIVE;
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetTxMsgBuffer(BYTE *&pouFlxTxMsgBuffer)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetBusConfigInfo(BYTE *BusInfo)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetLastErrorString(std::string &acErrorStr)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetControllerParams(LONG &lParam, UINT nChannel, ECONTR_PARAM eContrParam)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SetControllerParams(int nValue, ECONTR_PARAM eContrparam)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetErrorCount(SERROR_CNT &sErrorCnt, UINT nChannel, ECONTR_PARAM eContrParam)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SetAppParams(HWND hWndOwner)
{
	// hWndOwner contains the window => Useful for UI development
	CAN_ManageMsgBuf(MSGBUF_CLEAR, 0, nullptr);
    return S_OK;
}

/**
* \brief         Registers the buffer pBufObj to the client ClientID
* \param[in]     bAction, contains one of the values MSGBUF_ADD or MSGBUF_CLEAR
* \param[in]     ClientID, is the client ID
* \param[in]     pBufObj, is pointer to CBaseCANBufFSE object
* \return        S_OK for success, S_FALSE for failure
* \authors       Arunkumar Karri
* \date          07.10.2011 Created
*/
HRESULT CDIL_CAN_OPENCAN::CAN_ManageMsgBuf(BYTE bAction, DWORD ClientID, CBaseCANBufFSE* pBufObj)
{
	if (bAction == MSGBUF_ADD)
	{
		xMsgBuf[lastIndex] = pBufObj;
		lastIndex++;
	} else if (bAction == MSGBUF_CLEAR)
	{
		// xMsgBuf = nullptr;
	}
    return S_OK;
}


HRESULT CDIL_CAN_OPENCAN::CAN_RegisterClient(BOOL bRegister, DWORD& ClientID, char* pacClientName)
{
    if (bRegister)
    {
        // Only one client allowed
        ClientID = xClient.dwClientID = 1;
    }
    else
    {
        // TODO: unregister
    }
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_GetCntrlStatus(const HANDLE& hEvent, UINT& unCntrlStatus)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_LoadDriverLibrary(void)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_UnloadDriverLibrary(void)
{
    return S_OK;
}

HRESULT CDIL_CAN_OPENCAN::CAN_SetHardwareChannel(PSCONTROLLER_DETAILS, DWORD dwDriverId, bool bIsHardwareListed, unsigned int unChannelCount)
{
    return S_OK;
}
