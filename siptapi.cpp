/*
Name: asttapi.cpp
License: Under the GNU General Public License Version 2 or later (the "GPL")
(c): Nick Knight
(c): Klaus Darilion (IPCom GmbH, www.ipcom.at)

Description:
*/
/* ***** BEGIN LICENSE BLOCK *****
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Asttapi.
*
* The Initial Developer of the Original Code is
* Nick Knight.
* Portions created by the Initial Developer are Copyright (C) 2005
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): Klaus Darilion (IPCom GmbH, www.ipcom.at)
*
* ***** END LICENSE BLOCK ***** */

// The debugger can't handle symbols more than 255 characters long.
// STL often creates symbols longer than that.
// When symbols are longer than 255 characters, the warning is issued.
#pragma warning(disable:4786)

// Helge
//#define TRIALPROXY "85.200.227.170"
//#define TRIALOUTBOUNDPROXY "85.200.227.170"
//#define TRIALUSERNAME "2607"

#if defined(TRIALPROXY) || defined(TRIALOUTBOUNDPROXY) || defined(TRIALUSERNAME)
#undef MULTILINE
#endif

//Validity date
//#define EXPIRATIONDATE "2017-12-31"

//Domain prefix, must contain a leading .
//#define DOMAINSUFFIX ".allocloud.com"

// setting the version to 3.0 causes problem with dialer.exe
//#define TAPI_CURRENT_VERSION 0x00030000

#include "dll.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <atlconv.h>

//For debug and definitions
#include "WaveTsp.h"

//our resources
#include "resource.h"

//misc functions
#include "utilities.h"

//kd
#include "TapiLine.h"
#include "SipStack.h"

#include <map>

////////////////////////////////////////////////////////////////////////////////
//
// Globals
//
// Keep track of all the globals we require
//
////////////////////////////////////////////////////////////////////////////////

DWORD g_dwPermanentProviderID = 0;
DWORD g_dwLineDeviceIDBase = 0;

HPROVIDER g_hProvider = 0;

//For our windows...
HINSTANCE g_hinst = 0;

//kd
SipStack *g_sipStack = NULL;

// { dwDialPause, dwDialSpeed, dwDigitDuration, dwWaitForDialtone }
LINEDIALPARAMS      g_dpMin = { 100,  50, 100,  100 };
LINEDIALPARAMS      g_dpDef = { 250,  50, 250,  500 };
LINEDIALPARAMS      g_dpMax = { 1000, 50, 1000, 1000 };


////////////////////////////////////////////////////////////////////////////////
// Function DllMain
//
// Dll entry
//
////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HINSTANCE   hinst,
					DWORD       dwReason,
					void*      /*pReserved*/)
{
	if( dwReason == DLL_PROCESS_ATTACH )
	{
		g_hinst = hinst;
	}

	return TRUE;
}



LONG TSPIAPI TSPI_providerInit(
							   DWORD               dwTSPIVersion,
							   DWORD               dwPermanentProviderID,
							   DWORD               dwLineDeviceIDBase,
							   DWORD               dwPhoneDeviceIDBase,
							   DWORD_PTR           dwNumLines,
							   DWORD_PTR           dwNumPhones,
							   ASYNC_COMPLETION    lpfnCompletionProc,
							   LPDWORD             lpdwTSPIOptions                         // TSPI v2.0
							   )

{
	BEGIN_PARAM_TABLE("TSPI_providerInit")
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(dwPermanentProviderID)
		DWORD_IN_ENTRY(dwLineDeviceIDBase)
		DWORD_IN_ENTRY(dwPhoneDeviceIDBase)
		DWORD_OUT_ENTRY(dwNumLines)
		DWORD_OUT_ENTRY(dwNumPhones)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_providerInit: ...\n");

	//Record all of the globals we need
	g_pfnCompletionProc = lpfnCompletionProc;

	//other params we need to track
	g_dwPermanentProviderID = dwPermanentProviderID;
	g_dwLineDeviceIDBase = dwLineDeviceIDBase;

	TSPTRACE("TSPI_providerInit: dwTSPIVersion=0x%X, dwPermanentProviderID=0x%X,"\
		"dwLineDeviceIDBase=0x%X, dwPhoneDeviceIDBase=0x%X,"\
		"dwNumLines=0x%X, dwNumPhones=0x%X,"\
		"lpfnCompletionProc=0x%X, lpdwTSPIOptions=0x%X\n",
		dwTSPIVersion, dwPermanentProviderID, dwLineDeviceIDBase,
		dwPhoneDeviceIDBase, dwNumLines, dwNumPhones,
		lpfnCompletionProc, lpdwTSPIOptions);

	if (g_sipStack) {
		TSPTRACE("TSPI_providerInit: WARNING: SIPTAPI already initialized, doing nothing ...\n");
		return EPILOG(0);
	}

	g_sipStack = new SipStack;
	if (!g_sipStack) {
		TSPTRACE("TSPI_providerInit: ERROR: failed to create sipStack\n");
		return EPILOG(LINEERR_NOMEM);
	}

	TSPTRACE("TSPI_providerInit: ... done\n");

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerShutdown
//
// Shutdown and clean up.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerShutdown(
								   DWORD               dwTSPIVersion,
								   DWORD               dwPermanentProviderID                   // TSPI v2.0
								   )
{
	BEGIN_PARAM_TABLE("TSPI_providerShutdown")
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_providerShutdown: dwTSPIVersion=0x%X, dwPermanentProviderID=0x%X\n",
		dwTSPIVersion, dwPermanentProviderID);

	if (!g_sipStack) {
		TSPTRACE("SIPTAPI: WARNING: SIPTAPI shutdown, but no SipStack! shutdown anyway ...\n");
		return EPILOG(0);
	}

	if (!g_sipStack->isInitialized()) {
		TSPTRACE("SIPTAPI: SIPTAPI shutdown, deleting sipStack ...\n");
		return EPILOG(0);
	}

	// shut down SipStack
	if (g_sipStack->shutdown()) {
		TSPTRACE("SIPTAPI: shutdown of SipStack failed, shutdown anyway ...\n");
		delete g_sipStack;
		return EPILOG(0);
	}
	TSPTRACE("SIPTAPI: shutdown of SipStack succeeded, shuting down ...\n");
	delete g_sipStack;

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Capabilities
//
// TAPI will ask us what our capabilities are
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineNegotiateTSPIVersion
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineNegotiateTSPIVersion(
	DWORD dwDeviceID,
	DWORD dwLowVersion,
	DWORD dwHighVersion,
	LPDWORD lpdwTSPIVersion)
{
	BEGIN_PARAM_TABLE("TSPI_lineNegotiateTSPIVersion")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(dwLowVersion)
		DWORD_IN_ENTRY(dwHighVersion)
		DWORD_OUT_ENTRY(lpdwTSPIVersion)
		END_PARAM_TABLE()

		LONG tr = 0;

	if ( dwLowVersion <= TAPI_CURRENT_VERSION )
	{
#define MIN(a, b) (a < b ? a : b)
		*lpdwTSPIVersion = MIN(TAPI_CURRENT_VERSION,dwHighVersion);
	}
	else
	{
		tr = LINEERR_INCOMPATIBLEAPIVERSION;
	}

	return EPILOG(tr);

}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerEnumDevices
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerEnumDevices(
									  DWORD dwPermanentProviderID,
									  LPDWORD lpdwNumLines,
									  LPDWORD lpdwNumPhones,
									  HPROVIDER hProvider,
									  LINEEVENT lpfnLineCreateProc,
									  PHONEEVENT lpfnPhoneCreateProc)
{
	BEGIN_PARAM_TABLE("TSPI_providerEnumDevices")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		DWORD_OUT_ENTRY(lpdwNumLines)
		DWORD_OUT_ENTRY(lpdwNumPhones)
		DWORD_IN_ENTRY(hProvider)
		DWORD_IN_ENTRY(lpfnLineCreateProc)
		DWORD_IN_ENTRY(lpfnPhoneCreateProc)
		END_PARAM_TABLE()

	std::string strData;
	DWORD tempInt;
/*
	// this can be used to report only the real activated number of lines
	// but then the permanent device id of a line may change when other lines are
	// activated/deactivated
	int i, activelines;
	DWORD tempInt;
	//kd: (...up to 33 bytes...)
	char lineNrString[34];
	std::string strData;

	activelines = 0;
	for (i=0; i < SIPTAPI_NUMLINES; i++) {
		// build string for fetching value from registry
		_ultoa(0,lineNrString,10);
		strData = std::string("lineactive") + lineNrString;
		// get configuration for this specific line/device from registry 
		readConfigInt(strData, tempInt);
		if (tempInt) {
			activelines++;
		}
	}
*/

	g_hProvider = hProvider;
	*lpdwNumLines = SIPTAPI_NUMLINES;
	*lpdwNumPhones = SIPTAPI_NUMPHONES;

#ifdef MULTILINE
	// check if manual line config (with variable number of lines) is used
	strData = std::string("lineconfigviaregistry");
	readConfigInt(strData, tempInt);
	if (tempInt) {
		strData = std::string("numLines");
		readConfigInt(strData, tempInt);
		TSPTRACE("TSPI_providerEnumDevices: Using line config via Registry with %d lines\n", tempInt);
		*lpdwNumLines = tempInt;
	}
#endif
	TSPTRACE("TSPI_providerEnumDevices: Reporting %d lines\n", *lpdwNumLines);

#ifdef EXPIRATIONDATE
	// check if license is still valid
	SYSTEMTIME currentSystemTime, licenseSystemTime;
	TSPTRACE("TSPI_providerEnumDevices: verify if license is still valid ...");
	GetLocalTime(&currentSystemTime);
	GetLocalTime(&licenseSystemTime); // workaround to init the structure :-)
	TSPTRACE("TSPI_providerEnumDevices: parsing validity date ...");
	if (sscanf_s(EXPIRATIONDATE, 
		"%04u-%02u-%02u", &licenseSystemTime.wYear, 
		&licenseSystemTime.wMonth, &licenseSystemTime.wDay) != 3) {
			// error parsing date
			TSPTRACE("TSPI_providerEnumDevices: invalid expiration date ...");
			*lpdwNumLines = 0;
			*lpdwNumPhones = 0;
	} else {
		// compare timestamps
		FILETIME currentFileTime, licenseFileTime;
		TSPTRACE("TSPI_providerEnumDevices: compare expiration date ...");
		SystemTimeToFileTime(&currentSystemTime, &currentFileTime);
		SystemTimeToFileTime(&licenseSystemTime, &licenseFileTime);
		if (CompareFileTime(&currentFileTime,&licenseFileTime) == 1) {
			// license expired
			TSPTRACE("TSPI_providerEnumDevices: licensed expired at %s", EXPIRATIONDATE);
			*lpdwNumLines = 0;
			*lpdwNumPhones = 0;
		} else {
			// license valid
			TSPTRACE("TSPI_providerEnumDevices: licensed still valid (till %s (%04u-%02u-%02u))", EXPIRATIONDATE,
				licenseSystemTime.wYear, licenseSystemTime.wMonth, licenseSystemTime.wDay);
		}
	}
#endif

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetDevCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetDevCaps(
								 DWORD           dwDeviceID,
								 DWORD           dwTSPIVersion,
								 DWORD           dwExtVersion,
								 LPLINEDEVCAPS   pldc
								 )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetDevCaps")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(dwExtVersion)
		DWORD_IN_ENTRY(pldc)
		END_PARAM_TABLE()

	LONG tr = 0;
	DWORD lineNr, tempInt;
	std::string strData, strLineAlias;
	char lineNrString[100]; //kd: (...int2ascii conversion needs up to 33 bytes...) Is this 64bit safe?

	TSPTRACE("TSPI_lineGetDevCaps: dwDeviceID=0x%X, dwTSPIVersion=0x%X\n",
		dwDeviceID, dwTSPIVersion);

	lineNr = dwDeviceID - g_dwLineDeviceIDBase + 1;
	TSPTRACE("TSPI_lineGetDevCaps: called for line number 0x%X\n",
		lineNr);

	// The ProviderInfo will be shown when installing the SIPTAPI
	#define WIDEN2(x) L ## x
	#define WIDEN(x) WIDEN2(x)
	#define __WTIMESTAMP__ WIDEN(__TIMESTAMP__)
	const wchar_t   szProviderInfo[] = L"SIPTAPI for click2dial " SIPTAPI_VERSION_W L" (Build " __WTIMESTAMP__ L")"; // (29+1)*2 = 60
	// convert lineNr into char field
	_ultoa(lineNr,lineNrString,10);

	//check if this line is active
	strData = std::string("lineactive") + lineNrString;
		readConfigInt(strData, tempInt);
	if (!tempInt) {
		strLineAlias = std::string(": deactivated");	// (13+1)*2 = 28
	} else {
		// get configuration for this specific line/device from registry
		// line alias = sip:user@domain
		std::string proxy, username;
		proxy = std::string("proxy") + lineNrString;
		username = std::string("username") + lineNrString;

		// SIP Proxy
		if ( false == readConfigString(proxy, strData) ) {
			TSPTRACE("TSPI_lineGetDevCaps: Error: no SIP proxy configured\n");
			proxy = std::string("ERROR");
		} else {
#ifdef DOMAINSUFFIX
			std::basic_string <char>::size_type index1;
			index1 = strData.find(DOMAINSUFFIX,0);
			if (index1 == std::string::npos ) {
				TSPTRACE("TSPI_lineGetDevCaps: INFO: SIP domain does not contain suffix '" DOMAINSUFFIX "', appending ...\n");
				// remove possible trailing .
				strData.erase(strData.find_last_not_of(".")+1);
				strData = strData + DOMAINSUFFIX;
			}
#endif
			proxy = strData;
		}

		// SIP username
		if ( false == readConfigString(username, strData) ) {
			TSPTRACE("TSPI_lineGetDevCaps: WARNING: no SIP username configured\n");
			username = std::string("ERROR");
		} else {
			username = strData;
		}
		strLineAlias = std::string(": sip:") + username + std::string("@") + proxy;
	}

	// The LineName will be shown in TAPI applications like Outlook
	std::string strLineName = std::string("SIPTAPI ");
	// make line number at least 3 digits
	if (lineNr < 10) {
		strLineName = strLineName + std::string("00") + lineNrString + strLineAlias;
	} else if (lineNr < 100) {
		strLineName = strLineName + std::string("0") + lineNrString + strLineAlias;
	} else {
		strLineName = strLineName + lineNrString + strLineAlias;
	}
	TSPTRACE("TSPI_lineGetDevCaps: complete lineString = '%s'\n",strLineName.c_str());
	
	pldc->dwNeededSize = sizeof(LINEDEVCAPS) +
		sizeof(szProviderInfo) +
		sizeof(wchar_t) * (strLineName.length() + 1); // +1 as length is excluding \0
	
	if( pldc->dwNeededSize <= pldc->dwTotalSize )
	{
		TSPTRACE("TSPI_lineGetDevCaps: pldc big enough to store capabillities\n");
		pldc->dwUsedSize = pldc->dwNeededSize;

		// ProviderInfo
		pldc->dwProviderInfoSize    = sizeof(szProviderInfo);
		pldc->dwProviderInfoOffset  = sizeof(LINEDEVCAPS) + 0;
		wchar_t* pszProviderInfo = (wchar_t*)((BYTE*)pldc + pldc->dwProviderInfoOffset);
		wcscpy(pszProviderInfo, szProviderInfo);

		// LineName
		pldc->dwLineNameOffset      = sizeof(LINEDEVCAPS) + sizeof(szProviderInfo);
		pldc->dwLineNameSize        = sizeof(wchar_t) * (strLineName.length() + 1);

		// convert line name string and copy it into the wchar buffer for the TAPI
		MultiByteToWideChar(
            CP_ACP,
			MB_PRECOMPOSED,
			strLineName.c_str(),
			-1,
            (wchar_t*)((BYTE*)pldc + pldc->dwLineNameOffset),
            (pldc->dwLineNameSize)/sizeof(wchar_t) );
	}
	else
	{
		TSPTRACE("TSPI_lineGetDevCaps: pldc too small to store capabillities\n");
		pldc->dwUsedSize = sizeof(LINEDEVCAPS);
	}
	
	pldc->dwStringFormat      = STRINGFORMAT_ASCII;

	// Microsoft recommended algorithm for
	// calculating the permanent line ID
#define MAKEPERMLINEID(dwPermProviderID, dwDeviceID) \
	((LOWORD(dwPermProviderID) << 16) | dwDeviceID)

	pldc->dwPermanentLineID   = MAKEPERMLINEID(g_dwPermanentProviderID, dwDeviceID - g_dwLineDeviceIDBase);
	pldc->dwAddressModes      = LINEADDRESSMODE_ADDRESSID;
	pldc->dwNumAddresses      = 1;
	pldc->dwBearerModes       = LINEBEARERMODE_VOICE;
	pldc->dwMediaModes        = LINEMEDIAMODE_INTERACTIVEVOICE;
	pldc->dwGenerateDigitModes= LINEDIGITMODE_DTMF;
	pldc->dwDevCapFlags       = LINEDEVCAPFLAGS_CLOSEDROP;
	pldc->dwMaxNumActiveCalls = 1;
	pldc->dwLineFeatures      = LINEFEATURE_MAKECALL;

	// DialParams
	pldc->MinDialParams = g_dpMin;
	pldc->MaxDialParams = g_dpMax;
	pldc->DefaultDialParams = g_dpDef;

	return EPILOG(tr);
}


void TackOnDataLineAddressCaps(void* pData, const char* pStr, DWORD* pSize)
{
	USES_CONVERSION;

	// Convert the string to Unicode
	LPCWSTR pWStr = A2CW(pStr);

	size_t cbStr = (strlen(pStr) + 1) * 2;
	LPLINEADDRESSCAPS pDH = (LPLINEADDRESSCAPS)pData;

	// If this isn't an empty string then tack it on
	if (cbStr > 2)
	{
		// Increase the needed size to reflect this string whether we are
		// successful or not.
		pDH->dwNeededSize += cbStr;

		// Do we have space to tack on the string?
		if (pDH->dwTotalSize >= pDH->dwUsedSize + cbStr)
		{
			// YES, tack it on
			memcpy((char *)pDH + pDH->dwUsedSize, pWStr, cbStr);

			// Now adjust size and offset in message and used 
			// size in the header
			DWORD* pOffset = pSize + 1;

			*pSize   = cbStr;
			*pOffset = pDH->dwUsedSize;
			pDH->dwUsedSize += cbStr;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAddressCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetAddressCaps(
									 DWORD   dwDeviceID,
									 DWORD   dwAddressID,
									 DWORD   dwTSPIVersion,
									 DWORD   dwExtVersion,
									 LPLINEADDRESSCAPS  pac)
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressCaps")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(dwAddressID)
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_OUT_ENTRY(dwExtVersion)
		END_PARAM_TABLE()

	DWORD lineNr;
	char lineNrString[100]; //kd: (...int2ascii conversion needs up to 33 bytes...) Is this 64bit safe?

	lineNr = dwDeviceID - g_dwLineDeviceIDBase + 1;
	TSPTRACE("TSPI_lineGetAddressCaps: dwDeviceID=0x%X, dwTSPIVersion=0x%X\n",
		dwDeviceID, dwTSPIVersion);
	TSPTRACE("TSPI_lineGetAddressCaps: called for line number 0x%X\n",
		lineNr);


	/* TODO (Nick#1#): Most of this function has been taken from an example
	and will need to be modified in more detail */

	//pac->dwNeededSize = sizeof(LPLINEADDRESSCAPS);
	//pac->dwUsedSize = sizeof(LPLINEADDRESSCAPS);
	pac->dwNeededSize = sizeof(LINEADDRESSCAPS);
	pac->dwUsedSize = sizeof(LINEADDRESSCAPS);


	pac->dwLineDeviceID = dwDeviceID;
	pac->dwAddressSharing = LINEADDRESSSHARING_PRIVATE;
	pac->dwCallInfoStates = LINECALLINFOSTATE_MEDIAMODE | LINECALLINFOSTATE_APPSPECIFIC;
	pac->dwCallerIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
	pac->dwCalledIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
	pac->dwRedirectionIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN ;
	pac->dwCallStates = LINECALLSTATE_IDLE | LINECALLSTATE_OFFERING | LINECALLSTATE_ACCEPTED | LINECALLSTATE_DIALING | LINECALLSTATE_CONNECTED;

	pac->dwDialToneModes = LINEDIALTONEMODE_UNAVAIL;
	pac->dwBusyModes = LINEDIALTONEMODE_UNAVAIL;
	pac->dwSpecialInfo = LINESPECIALINFO_UNAVAIL;

	pac->dwDisconnectModes = LINEDISCONNECTMODE_UNAVAIL;

	/* TODO (Nick#1#): This needs to be taken from the UI */
	pac->dwMaxNumActiveCalls = 1;

	pac->dwAddrCapFlags = LINEADDRCAPFLAGS_DIALED;

	pac->dwCallFeatures = LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP | LINECALLFEATURE_BLINDTRANSFER | LINECALLFEATURE_REDIRECT;
	pac->dwAddressFeatures = LINEADDRFEATURE_MAKECALL | LINEADDRFEATURE_PICKUP;

	pac->dwTransferModes = LINETRANSFERMODE_TRANSFER;

	// Specify Address
	TSPTRACE("TSPI_lineGetAddressCaps: specify Address ...");
	pac->dwAddressOffset = pac->dwUsedSize; //This is where we place the Address
	//std::string strLineName = std::string("SIPTAPI ");
    std::string strLineName = std::string("");
    // convert lineNr into char field
	_ultoa(lineNr,lineNrString,10);

	// make line number at least 3 digits
	if (lineNr < 10) {
		strLineName = strLineName + std::string("00") + lineNrString;
	} else if (lineNr < 100) {
		strLineName = strLineName + std::string("0") + lineNrString;
	} else {
		strLineName = strLineName + lineNrString;
	}
	TSPTRACE("TSPI_lineGetAddressCaps: complete Address = '%s'\n",strLineName.c_str());
	TackOnDataLineAddressCaps(pac, strLineName.c_str(), &pac->dwAddressSize);
	//lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_ADDRESS;

	return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Lines
//
// After a suitable line has been found it will be opened with lineOpen
// which TAPI will forward onto TSPI_lineOpen
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Function ThreadProc
//
// this is the thread for a line which listens for messages and processes them
//
////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	BEGIN_PARAM_TABLE("ThreadProc")
		DWORD_IN_ENTRY(lpParameter)
		END_PARAM_TABLE()

	TSPTRACE("ThreadProc: thread started, entering SIP message loop ...\n");
	// this function returns when SipStack::shutdown is called
	g_sipStack->processSipMessages();
	TSPTRACE("ThreadProc: SIP message loop terminated ... thread is terminating\n");

	ExitThread(0);

	//	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineOpen
//
// This function is typically called where the software needs to reserve some 
// hardware, you can assign any 32bit value to the *phdLine, and it will be sent
// back to any future calls to functions about that line.
//
// Becuase this is sockets not hardware we can set-up the sockets required in this 
// functions, and perhaps get the thread going to read the output of the manager.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineOpen(
						   DWORD               dwDeviceID,
						   HTAPILINE           htLine,
						   LPHDRVLINE          phdLine,
						   DWORD               dwTSPIVersion,
						   LINEEVENT           pfnEventProc
						   )
{
	BEGIN_PARAM_TABLE("TSPI_lineOpen")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(htLine)
		DWORD_OUT_ENTRY(*phdLine)
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(pfnEventProc)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_lineOpen: ...\n");

	DWORD lineNr, tempInt;
	std::string strData;
	lineNr = dwDeviceID - g_dwLineDeviceIDBase + 1;
	TSPTRACE("TSPI_lineOpen: called for line number 0x%X\n", lineNr);

TSPTRACE("TSPI_lineOpen: htLine = %d\n", htLine);
TSPTRACE("TSPI_lineOpen: *phdLine = %d\n", *phdLine);

	//kd: (...up to 33 bytes...), is that 64bit safe? use 100 for safety
	char lineNrString[100];
	_ultoa(lineNr,lineNrString,10);


	// check if this line is deactivated
	strData = std::string("lineactive") + lineNrString;
	// get configuration for this specific line/device from registry 
	readConfigInt(strData, tempInt);
	if (!tempInt) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X is deactivated\n", dwDeviceID);
		return EPILOG(LINEERR_RESOURCEUNAVAIL);
	}

	TSPTRACE("SipStack::TSPI_lineOpen: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	// check if there is already a call with this DeviceID
	if (g_sipStack->findTapiLine(dwDeviceID)) {
		TSPTRACE("TSPI_lineOpen: ERROR: dwDeviceID 0x%X is already in active TapiLines map\n", dwDeviceID);
		TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_RESOURCEUNAVAIL);
	}
	TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Unlock();

	// create new TapiLine
	TapiLine *newLine;
	newLine = new TapiLine();

	if (!newLine) {
		TSPTRACE("TSPI_lineOpen: ERROR: failed to create TapiLine object!\n");
		return EPILOG(LINEERR_RESOURCEUNAVAIL);
	}

	newLine->setTapiLine(htLine);
	newLine->setLineEventCallback(pfnEventProc);

	// get configuration for this specific line/device from registry 
	// and create the new TapiLine object with the corresponding
	// configuration parameters .....
	// config parameters: proxy, obproxy, username, password
	std::string proxy, obproxy, username, password, authusername, phoneusername, autoanswer;
	proxy = std::string("proxy") + lineNrString;
	obproxy = std::string("obproxy") + lineNrString;
	username = std::string("username") + lineNrString;
	authusername = std::string("authusername") + lineNrString;
	phoneusername = std::string("phoneusername") + lineNrString;
	password = std::string("password") + lineNrString;
	autoanswer = std::string("autoanswer") + lineNrString;

	// SIP Proxy
	if ( false == readConfigString(proxy, strData) ) {
		TSPTRACE("TSPI_lineOpen: Error: no SIP proxy configured\n");
		return EPILOG(LINEERR_CALLUNAVAIL);
	}
#ifdef TRIALPROXY
	strData = TRIALPROXY;
#endif

#ifdef DOMAINSUFFIX
	std::basic_string <char>::size_type index1;
	index1 = strData.find(DOMAINSUFFIX,0);
	if (index1 == std::string::npos ) {
		TSPTRACE("TSPI_lineOpen: INFO: SIP domain does not contain suffix '" DOMAINSUFFIX "', appending ...\n");
		// remove possible trailing .
		strData.erase(strData.find_last_not_of(".")+1);
		strData = strData + DOMAINSUFFIX;
	}
#endif

	TSPTRACE("TSPI_lineOpen: SIP proxy is '%s'\n",strData.c_str());
	newLine->setProxy(strData);

	// SIP Outbound Proxy
	if ( false == readConfigString(obproxy, strData) ) {
		TSPTRACE("TSPI_lineOpen: WARNING: no SIP outbound-proxy configured\n");
		strData = "";
	}
#ifdef TRIALOUTBOUNDPROXY
	strData = TRIALOUTBOUNDPROXY;
#endif

#ifdef DOMAINSUFFIX
	if (!strData.empty()) {
		std::basic_string <char>::size_type index1;
		index1 = strData.find(DOMAINSUFFIX,0);
		if (index1 == std::string::npos ) {
			TSPTRACE("TSPI_lineOpen: INFO: SIP outbound proxy does not contain suffix '" DOMAINSUFFIX "', appending ...\n");
			// remove possible trailing .
			strData.erase(strData.find_last_not_of(".")+1);
			strData = strData + DOMAINSUFFIX;
		}
	}
#endif

	TSPTRACE("TSPI_lineOpen: SIP outbound proxy is '%s'\n",strData.c_str());
	newLine->setObProxy(strData);

	// SIP username
	if ( false == readConfigString(username, strData) ) {
		TSPTRACE("TSPI_lineOpen: WARNING: no SIP username configured\n");
		strData = "";
	}
#ifdef TRIALUSERNAME
	strData = TRIALUSERNAME;
#endif
	TSPTRACE("TSPI_lineOpen: SIP username is '%s'\n",strData.c_str());
	newLine->setUsername(strData);

	// SIP authusername
	if ( false == readConfigString(authusername, strData) ) {
		TSPTRACE("TSPI_lineOpen: INFO: no SIP authusername configured\n");
		strData = "";
	}
	TSPTRACE("TSPI_lineOpen: SIP authusername is '%s'\n",strData.c_str());
	newLine->setAuthusername(strData);

	// SIP phoneusername
	if ( false == readConfigString(phoneusername, strData) ) {
		TSPTRACE("TSPI_lineOpen: INFO: no SIP phoneusername configured\n");
		strData = "";
	}
	TSPTRACE("TSPI_lineOpen: SIP phoneusername is '%s'\n",strData.c_str());
	newLine->setPhoneusername(strData);

	// SIP password
	// ToDo: crypt/decrypt password
	if ( false == readConfigString(password, strData) ) {
		TSPTRACE("TSPI_lineOpen: WARNING: no SIP password configured\n");
		strData = "";
	}
	//TSPTRACE("TSPI_lineOpen: SIP password is '%s'\n",strData.c_str());
	newLine->setPassword(strData);

	// auto-answer mode
	if ( false == readConfigInt(autoanswer, tempInt) ) {
		TSPTRACE("TSPI_lineOpen: WARNING: no autoanswer, assuming 'no'\n");
		tempInt = 0;
	}
	TSPTRACE("TSPI_lineOpen: autoanswer is '%s'\n",tempInt?"on":"off");
	newLine->setAutoanswer(tempInt);

	// check the realm?
	strData = std::string("enablerealmcheck");
	// this is a global setting for all lines
	readConfigInt(strData, tempInt);
	newLine->setEnableRealmCheck(tempInt);
	if (tempInt) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X realm check activated\n", dwDeviceID);
	} else {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X realm check deactivated\n", dwDeviceID);
	}

	// check the transport protocol
	strData = std::string("transportprotocol");
	// this is a global setting for all lines
	readConfigInt(strData, tempInt);
	newLine->setTransportProtocol(tempInt);
	if (tempInt==1) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X:transport protocol: TCP\n", dwDeviceID);
	} else {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X:transport protocol: UDP\n", dwDeviceID);
	}


	// if we should send 180 Ringing
	strData = std::string("send180ringing");
	// this is a global setting for all lines
	readConfigInt(strData, tempInt);
	newLine->setSend180Ringing(tempInt);
	if (tempInt==1) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X:send 180 ringing: yes\n", dwDeviceID);
	} else {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X:send 180 ringing: no\n", dwDeviceID);
	}

	// check the mode, reverse mode means, that the target is called first, then transfered to the local user
	strData = std::string("reversemode");
	// this is a global setting for all lines
	readConfigInt(strData, tempInt);
	newLine->setReverseMode(tempInt);
	if (tempInt==2) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X reverse-mode activated\n", dwDeviceID);
	} else if (tempInt==1) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X ACD-mode activated\n", dwDeviceID);
	} else {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X reverse-mode/ACD-mode deactivated\n", dwDeviceID);
	}

	// initialize sipStack when needed
	// the SIP stack will be initialized on the first opened line, und will
	// be shutdown on the last call closed
	TSPTRACE("SipStack::TSPI_lineOpen: lock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Lock();
	if (!g_sipStack->isInitialized()) {
		TSPTRACE("TSPI_lineOpen: sipStack is not yet initialized, initializing ...\n");
		if (g_sipStack->initialize(newLine->getTransportProtocol())) {
			TSPTRACE("TSPI_lineOpen: sipStack initialization failed, exiting ...\n");
			TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 3 ...\n");
			g_sipStack->sipStackMut.Unlock();
			return EPILOG(LINEERR_RESOURCEUNAVAIL);
		}
		TSPTRACE("TSPI_lineOpen: sipStack initialization succeeded, starting thread ...\n");
		// start worker thread
		g_sipStack->thrHandle = CreateThread(NULL,0,ThreadProc,0,0,NULL);
		if (g_sipStack->thrHandle == NULL) {
			DWORD dw = GetLastError(); 
			TSPTRACE("TSPI_lineOpen: ERROR: thread creation failed: GetLastError returned %u\n", dw); 
			// todo error handling if thread creating fails
		} else {
			TSPTRACE("TSPI_lineOpen: thread successful created ...\n");
		}
	}
	TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 4 ...\n");
	g_sipStack->sipStackMut.Unlock();

	newLine->deviceId = dwDeviceID;
	// we do not generate a dedicated handle but just reuse the provided handle
	newLine->hdLine = (HDRVLINE) newLine->htLine;
	// send back the handle
	*phdLine = newLine->hdLine;

	TSPTRACE("SipStack::TSPI_lineOpen: lock sipStackMutex 3 ...\n");
	g_sipStack->sipStackMut.Lock();
	g_sipStack->addTapiLine(newLine);

	// initialize line (set authentication ...)
	if(!newLine->isInitialized()) {
		if (newLine->initialize()) {
			//Todo error handling if eXosip initialization fails
		}
	}
	TSPTRACE("TSPI_lineOpen: newLine initialization succeeded\n");

	// REGISTER line/account to SIP proxy
	strData = std::string("register") + lineNrString;
	// get configuration for this specific line/device from registry 
	readConfigInt(strData, tempInt);
	if (!tempInt) {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X registration is deactivated\n", dwDeviceID);
		TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 5 ...\n");
		g_sipStack->sipStackMut.Unlock();
	} else {
		TSPTRACE("TSPI_lineOpen: dwDeviceID 0x%X registration is activated\n", dwDeviceID);
		if (newLine->register_to_sipproxy() < 0) {
			TSPTRACE("TSPI_lineOpen: Proxy REGISTER failed, exiting ...\n");
			TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 6...\n");
			g_sipStack->sipStackMut.Unlock();
			return EPILOG(LINEERR_RESOURCEUNAVAIL);
		}
		TSPTRACE("SipStack::TSPI_lineOpen: unlock sipStackMutex 7 ...\n");
		g_sipStack->sipStackMut.Unlock();
		// wait for registration to be finished
		int i;
		for (i=0;i<30;i++) { // 30x100ms
			if (newLine->reg_status == TapiLine::SIP_REGISTERED) {
				TSPTRACE("TSPI_lineOpen: REGISTER suceeded\n");
				break;
			}
			Sleep(100);
		}
		if (i == 30) {
			TSPTRACE("TSPI_lineOpen: WARNING: Registration after 3 seconds still not sucessful, ignoring ...\n");
		}
	}

	TSPTRACE("TSPI_lineOpen: ... succeeded\n");
	TSPTRACE("TSPI_lineOpen: htLine = %d\n", htLine);
	TSPTRACE("TSPI_lineOpen: *phdLine = %d\n", *phdLine);
	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineClose
//
// Called by TAPI when a line is no longer required.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineClose(HDRVLINE hdLine)
{
	BEGIN_PARAM_TABLE("TSPI_lineClose")
		DWORD_IN_ENTRY(hdLine)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_lineClose: ...\n");

	// hdline is a pointer to the corresponding TapiLine object
	// when this function returns, all actions of the TapiLine must have finished

	TapiLine *activeLine;
	TSPTRACE("SipStack::TSPI_lineClose: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdline(hdLine);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineClose: ERROR: line/call not found ...\n");
		TSPTRACE("SipStack::TSPI_lineClose: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(0);
	}

	// releasing the lock should be fine as the TapiLine object is only deleted in this function
	TSPTRACE("SipStack::TSPI_lineClose: unlock sipStackMutex 22...\n");
	g_sipStack->sipStackMut.Unlock();

	// un-register if necessary
	if (activeLine->reg_status != TapiLine::SIP_UNREGISTERED) {
		int i;
		activeLine->unregister_from_sipproxy();
		for (i=0;i<30;i++) { // 30x100ms
			if (activeLine->reg_status == TapiLine::SIP_UNREGISTERED) {
				TSPTRACE("TSPI_lineClose: unREGISTER suceeded\n");
				break;
			}
			Sleep(100);
		}
		if (i == 30) {
			TSPTRACE("TSPI_lineClose: WARNING: unRegistration after 3 seconds still not sucessful, ignoring ...\n");
		}
	}

	TSPTRACE("SipStack::TSPI_lineClose: lock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Lock();
	// the sipStack has to make sure to release all SIP
	// stuff before returning
	g_sipStack->deleteTapiLine(activeLine);
	delete activeLine;
	TSPTRACE("SipStack::TSPI_lineClose: unlock sipStackMutex 5 ...\n");
	g_sipStack->sipStackMut.Unlock();

	if ( ! (g_sipStack->getActiveTapiLines()) ) {
		TSPTRACE("TSPI_lineClose: no more active TapiLines, shutting down SipStack...\n");
		g_sipStack->shutdown();
		TSPTRACE("TSPI_lineClose: no more active TapiLines, shutting down SipStack ... done\n");
	}
	

	TSPTRACE("TSPI_lineClose: ... done\n");
	return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineMakeCall
//
// This function is called by TAPI to initialize a new outgoing call. This will
// initiate the call and return, when the call is made (but not necasarily connected)
// we should then signal TAPI via the asyncrounous completion function.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineMakeCall(
							   DRV_REQUESTID       dwRequestID,
							   HDRVLINE            hdLine,
							   HTAPICALL           htCall,
							   LPHDRVCALL          phdCall,
							   LPCWSTR             pszDestAddress,
							   DWORD               dwCountryCode,
							   LPLINECALLPARAMS    const pCallParams
							   )
{
	BEGIN_PARAM_TABLE("TSPI_lineMakeCall")
		DWORD_IN_ENTRY(dwRequestID)
		DWORD_IN_ENTRY(hdLine)
		DWORD_IN_ENTRY(htCall)
		DWORD_IN_ENTRY(phdCall)
		STRING_IN_ENTRY(pszDestAddress) //Pointer to a null-terminated Unicode string
		DWORD_IN_ENTRY(dwCountryCode)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_lineMakeCall: ...\n");

	TSPTRACE("TSPI_lineMakeCall: hdLine = %d\n", hdLine);
	TSPTRACE("TSPI_lineMakeCall: htCall = %d\n", htCall);
	TSPTRACE("TSPI_lineMakeCall: *phdCall = %d\n", *phdCall);

	TapiLine *activeLine = g_sipStack->getTapiLineFromHdline( hdLine);
	TSPTRACE("TSPI_lineMakeCall: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineMakeCall: ERROR: line not found, rejecting new call...\n");
		TSPTRACE("TSPI_lineMakeCall: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALLINEHANDLE);
	}

	// check if TapiLine is busy
	if (activeLine->getStatus() != TapiLine::IDLE) {
		// line is busy, return proper error value
		TSPTRACE("TSPI_lineMakeCall: ERROR: line is busy ... rejecting new call\n");
		TSPTRACE("TSPI_lineMakeCall: unlock sipStackMutex 2...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INUSE);
	}
	// reserve TapiLine for use (avoid concurrent access)
	activeLine->setStatus(TapiLine::RESERVED);

	activeLine->setTapiCall(htCall);
	if (activeLine->makeCall(pszDestAddress, dwCountryCode, dwRequestID)) {
		TSPTRACE("TSPI_lineMakeCall: ERROR: makeCall failed ... rejecting new call\n");
		TSPTRACE("TSPI_lineMakeCall: unlock sipStackMutex 3...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INUSE);
	}

	// ASYNC_COMPLETE
	TSPTRACE("TSPI_lineMakeCall: sending async_complete ...\n");
	g_pfnCompletionProc(dwRequestID,0);

	//// we only allow one call per line, thus as call handle we
	//// can reuse the line handle
	// reuse incoming call handle as outgoing call handle
	activeLine->hdCall = (HDRVCALL) activeLine->htCall;
	// send back the handle
	*phdCall = activeLine->hdCall;

	TSPTRACE("SipStack::TSPI_lineMakeCall: unlock sipStackMutex 4...\n");
	g_sipStack->sipStackMut.Unlock();
	

	TSPTRACE("TSPI_lineMakeCall: hdLine = %d\n", hdLine);
	TSPTRACE("TSPI_lineMakeCall: htCall = %d\n", htCall);
	TSPTRACE("TSPI_lineMakeCall: *phdCall = %d\n", *phdCall);

	TSPTRACE("TSPI_lineMakeCall: ... done\n");
	return EPILOG(dwRequestID);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineDrop
//
// This function is called by TAPI to signal the end of a call. The status 
// information for the call should be retained until the function TSPI_lineCloseCall
// is called.
//
// 2013-12-09: 
// "The TSPI_lineDrop function drops or disconnects the specified call. User-user 
// information can optionally be transmitted as part of the call disconnect. 
// This function can be called by the application at any time. When TSPI_lineDrop 
// returns, the call should be idle."  --> NOT IMPLEMENTED YET
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineDrop(
						   DRV_REQUESTID       dwRequestID,
						   HDRVCALL            hdCall,
						   LPCSTR              lpsUserInfo,
						   DWORD               dwSize
						   )
{
	BEGIN_PARAM_TABLE("TSPI_lineDrop")
		DWORD_IN_ENTRY(dwRequestID)
		DWORD_IN_ENTRY(hdCall)
		DWORD_OUT_ENTRY(lpsUserInfo)
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_lineDrop: ...\n");

	TapiLine *activeLine;
	TSPTRACE("SipStack::TSPI_lineDrop: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineDrop: ERROR: line/call not found ...\n");
		TSPTRACE("SipStack::TSPI_lineDrop: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}

	if (activeLine->shutdown(dwRequestID) == 0) { //shutdown done
		TSPTRACE("TSPI_lineDrop: shutting down TapiLine ... done\n");
	} else { // need to wait for sipstack to shut Tapiline down
		TSPTRACE("TSPI_lineDrop: need to wait for sipstack to shut Tapiline down ...\n");
		activeLine->shutdownRequestId = dwRequestID;
		activeLine->markForShutdown(true);
	}

	TSPTRACE("SipStack::TSPI_lineDrop: unlock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Unlock();

	TSPTRACE("TSPI_lineDrop: ... done\n");
	return EPILOG(dwRequestID);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineCloseCall
//
// This function should deallocate all of the calls resources, TSPI_lineDrop 
// may not be called before this one - so we also have to check the call 
// is dropped as well. Note: Outlook send first TSPI_lineDrop, then TSPI_lineCloseCall()
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineCloseCall(
								HDRVCALL            hdCall
								)
{
	BEGIN_PARAM_TABLE("TSPI_lineCloseCall")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		TSPTRACE("TSPI_lineCloseCall: ...\n");

	TapiLine *activeLine;
	TSPTRACE("TSPI_lineCloseCall: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineCloseCall: ERROR: line/call not found ...\n");
		TSPTRACE("TSPI_lineCloseCall: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}

	// shutdown should be done in TSPI_lineDrop
	// activeLine->shutdown();

	// for incoming calls, 
	// if the call is over, delete this TapiLine
	if (activeLine->isIncoming) {
		TSPTRACE("TSPI_lineCloseCall: deleting incoming call ...\n");
		g_sipStack->deleteTapiLine(activeLine);
		delete activeLine;
	} else {
		// If TSPI_lineCloseCall is called for a call on which there are outstanding 
		// asynchronous operations, the async operations must be reported before 
		// this procedure returns. After this procedure returns, the service provider 
		// must not report further events on the call.
		// 2013-12-19: Not sure if this is still correct, as TSPI_lineDrop should return 
		// when line is IDLE
		while (1) {
			if (!activeLine->shutdownRequestId) {
				TSPTRACE("TSPI_lineCloseCall: all async operations are completed, ready to return from TSPI_lineCloseCall ...\n");
				break;
			}
			TSPTRACE("TSPI_lineCloseCall: waiting for async operations to complete ...\n");
			TSPTRACE("TSPI_lineCloseCall: unlock sipStackMutex ...\n");
			g_sipStack->sipStackMut.Unlock();
			TSPTRACE("TSPI_lineCloseCall: sleeping 100ms ...\n");
			Sleep(100);
			TSPTRACE("TSPI_lineCloseCall: lock sipStackMutex ...\n");
			g_sipStack->sipStackMut.Lock();
			activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
			if (activeLine == NULL) {
				TSPTRACE("TSPI_lineCloseCall: ERROR: line/call not found ...\n");
				TSPTRACE("TSPI_lineCloseCall: unlock sipStackMutex ...\n");
				g_sipStack->sipStackMut.Unlock();
				return EPILOG(LINEERR_INVALCALLHANDLE);
			}
		}
	}

	/* set handle to 0 as TAPI will reuse handles and thus we may find
	   the wrong TapiLine as it might reference to an old line */
	activeLine->hdCall = 0;

	TSPTRACE("SipStack::TSPI_lineCloseCall: unlock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Unlock();

	TSPTRACE("TSPI_lineCloseCall: ... done\n");
	return EPILOG(0);
}



////////////////////////////////////////////////////////////////////////////////
//
// Status
//
// if TAPI requires to find out the status of our lines then it can call
// the following functions.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetLineDevStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetLineDevStatus(
									   HDRVLINE            hdLine,
									   LPLINEDEVSTATUS     plds
									   )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetLineDevStatus")
		DWORD_IN_ENTRY(hdLine)
		END_PARAM_TABLE()

		return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAdressStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPI_lineGetAdressStatus(
							  HDRVLINE hdLine,
							  DWORD dwAddressID,
							  LPLINEADDRESSSTATUS pas)
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAdressStatus")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallStatus(
									HDRVCALL            hdCall,
									LPLINECALLSTATUS    pls
									)
{
	//TODO finish this offf....
	BEGIN_PARAM_TABLE("TSPI_lineGetCallStatus")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

	TSPTRACE("TSPI_lineGetCallStatus: pls->dwTotalSize      = %d\r\n",pls->dwTotalSize);
	TSPTRACE("TSPI_lineGetCallStatus: sizeof(LINECALLSTATUS)= %d\r\n",sizeof(LINECALLSTATUS));

	if (sizeof(LINECALLSTATUS) > pls->dwTotalSize) {
		TSPTRACE("TSPI_lineGetCallStatus: ERROR: sizeof(LINECALLSTATUS) > dwTotalSize\r\n");
		return EPILOG(LINEERR_NOMEM);
	}

	// TAPI Service PRovider MUST NOT write this member!
	// http://msdn2.microsoft.com/en-us/library/ms725567.aspx
	//	ls->dwTotalSize = sizeof(LINECALLSTATUS);
	// we use all the fixed size members, thus we need at least the size of the fixed size members
	pls->dwNeededSize = sizeof(LINECALLSTATUS);
	pls->dwUsedSize = sizeof(LINECALLSTATUS);
	pls->dwCallStateMode = 0;

	TSPTRACE("TSPI_lineGetCallStatus: pls->dwTotalSize  = %d\r\n",pls->dwTotalSize);
	TSPTRACE("TSPI_lineGetCallStatus: pls->dwNeededSize = %d\r\n",pls->dwNeededSize);

	// TAPI Service PRovider MUST NOT write this member!
	// http://msdn2.microsoft.com/en-us/library/ms725567.aspx
	// pls->dwCallPrivilege = LINECALLPRIVILEGE_MONITOR | LINECALLFEATURE_ACCEPT | LINECALLFEATURE_ANSWER | LINECALLFEATURE_COMPLETECALL | LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP;
	//LINECALLPRIVILEGE_NONE 
	//LINECALLPRIVILEGE_OWNER 

	//and more...
	TapiLine *activeLine;
	TSPTRACE("SipStack::TSPI_lineGetCallStatus: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineGetCallStatus: ERROR: line/call not found ...\n");
		TSPTRACE("SipStack::TSPI_lineGetCallStatus: unlock sipStackMutex 22...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}

	pls->dwCallState = activeLine->getTapiStatus();

	// com(economy) zeigt den hangup button nicht, wir müssen signalisieren, dass das geht!
	//	pls->dwCallFeatures = LINECALLFEATURE_ACCEPT;
	pls->dwCallFeatures = LINECALLFEATURE_DROP;

	if (activeLine->getStatus() == TapiLine::INCOMING) {
		TSPTRACE("SipStack::TSPI_lineGetCallStatus: incoming call ... signal redirect capabilities ...\n");
		pls->dwCallFeatures = pls->dwCallFeatures | LINECALLFEATURE_REDIRECT;
	}

	if (activeLine->getReverseMode() == 1) {
		TSPTRACE("SipStack::TSPI_lineGetCallStatus: incoming call ... signal blind transfer capabilities ...\n");
		pls->dwCallFeatures = pls->dwCallFeatures | LINECALLFEATURE_BLINDTRANSFER;
	}

	TSPTRACE("SipStack::TSPI_lineGetCallStatus: unlock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Unlock();

	return EPILOG(0);
}

//Required (maybe) by lineGetCallInfo
//Thanks to the poster!
//http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&threadm=114501c32a84%24a337f930%24a101280a%40phx.gbl&rnum=3&prev=/groups%3Fq%3DTSPI_lineGetCallInfo%26ie%3DUTF-8%26oe%3DUTF-8%26hl%3Den%26btnG%3DGoogle%2BSearch
void TackOnData(void* pData, const char* pStr, DWORD* pSize)
{
	USES_CONVERSION;

	// Convert the string to Unicode
	LPCWSTR pWStr = A2CW(pStr);

	size_t cbStr = (strlen(pStr) + 1) * 2;
	LPLINECALLINFO pDH = (LPLINECALLINFO)pData;

	// If this isn't an empty string then tack it on
	if (cbStr > 2)
	{
		// Increase the needed size to reflect this string whether we are
		// successful or not.
		pDH->dwNeededSize += cbStr;

		// Do we have space to tack on the string?
		if (pDH->dwTotalSize >= pDH->dwUsedSize + cbStr)
		{
			// YES, tack it on
			memcpy((char *)pDH + pDH->dwUsedSize, pWStr, cbStr);

			// Now adjust size and offset in message and used 
			// size in the header
			DWORD* pOffset = pSize + 1;

			*pSize   = cbStr;
			*pOffset = pDH->dwUsedSize;
			pDH->dwUsedSize += cbStr;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallInfo
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallInfo(
								  HDRVCALL            hdCall,
								  LPLINECALLINFO      lpCallInfo
								  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetCallInfo")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		TapiLine *activeLine;
		TSPTRACE("SipStack::TSPI_lineGetCallInfo: lock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Lock();
		activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
		if (activeLine == NULL) {
			TSPTRACE("TSPI_lineGetCallInfo: ERROR - TapiLine with hdCall=%d not found",hdCall);
			TSPTRACE("SipStack::TSPI_lineGetCallInfo: unlock sipStackMutex ...\n");
            g_sipStack->sipStackMut.Unlock();
			return(LINEERR_INVALCALLHANDLE);
		}

		//Some useful information taken from a newsgroup cutting I found - it's ok for
		//the SP to fill in as much info as it can, set dwNeededSize apprpriately, &
		//return success.  (The app might not care about the var-length fields;
		//otherwise it'll see dwNeededSized, realloc a bigger buf, and retry)

		//These are the items that a TSP is required to fill out - other items we must preserve
		// - that is they are used by TAPI.
		lpCallInfo->dwNeededSize = sizeof (LINECALLINFO);
		lpCallInfo->dwUsedSize = sizeof (LINECALLINFO);
		lpCallInfo->dwLineDeviceID = activeLine->deviceId;  //at the mo we don't have any concept of more than one line - perhaps should to though!
		lpCallInfo->dwAddressID = 0;
		lpCallInfo->dwBearerMode = LINEBEARERMODE_SPEECH;
		//lpCallInfo->dwRate;
		//lpCallInfo->dwMediaMode;
		//lpCallInfo->dwAppSpecific;
		//lpCallInfo->dwCallID;
		//lpCallInfo->dwRelatedCallID;
		//lpCallInfo->dwCallParamFlags;
		//lpCallInfo->DialParams;
		lpCallInfo->dwOrigin = LINECALLORIGIN_INBOUND;
		lpCallInfo->dwReason = LINECALLREASON_DIRECT;  //one of LINECALLORIGIN_ Constants
		lpCallInfo->dwCompletionID = 0;
		lpCallInfo->dwCountryCode = 0;   // 0 = unknown
		lpCallInfo->dwTrunk = 0xFFFFFFFF; // 0xFFFFFFFF = unknown
		lpCallInfo->dwCallerIDFlags = 0;   //or LINECALLPARTYID_NAME | LINECALLPARTYID_ADDRESS; once we know we have the caller ID
		
		lpCallInfo->dwConnectedIDFlags = 0;
		lpCallInfo->dwConnectedIDSize = 0;
		lpCallInfo->dwConnectedIDOffset = 0;
		lpCallInfo->dwConnectedIDNameSize = 0;
		lpCallInfo->dwConnectedIDNameOffset = 0;


		// For the next section we need to find our call relating to this.
		lpCallInfo->dwCallerIDOffset = 0;
		lpCallInfo->dwCallerIDSize = 0;

		// CallerID
		TSPTRACE("TSPI_lineGetCallInfo: line found ...");
		if ( activeLine->incomingFromUriUser ) {
			TSPTRACE("TSPI_lineGetCallInfo: inserting callerID");
			lpCallInfo->dwCallerIDOffset = lpCallInfo->dwUsedSize; //This is where we place the caller ID info
			//we can also call the following function on caller ID name etc
			TackOnData(lpCallInfo, activeLine->incomingFromUriUser, &lpCallInfo->dwCallerIDSize);
			lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_ADDRESS;
		}

		if ( activeLine->incomingFromDisplayName ) {
			TSPTRACE("TSPI_lineGetCallInfo: inserting callerName");
			lpCallInfo->dwCallerIDNameOffset = lpCallInfo->dwUsedSize;
			TackOnData(lpCallInfo, activeLine->incomingFromDisplayName, &lpCallInfo->dwCallerIDNameSize);
			lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_NAME;
		}

		// CalledID (extracted from To header)
		if ( activeLine->incomingToUriUser ) {
			TSPTRACE("TSPI_lineGetCallInfo: inserting calledID");
			lpCallInfo->dwCalledIDOffset = lpCallInfo->dwUsedSize; //This is where we place the caller ID info
			//we can also call the following function on caller ID name etc
			TackOnData(lpCallInfo, activeLine->incomingToUriUser, &lpCallInfo->dwCalledIDSize);
			lpCallInfo->dwCalledIDFlags |= LINECALLPARTYID_ADDRESS;
		}

		if ( activeLine->incomingToDisplayName ) {
			TSPTRACE("TSPI_lineGetCallInfo: inserting calledName");
			lpCallInfo->dwCalledIDNameOffset = lpCallInfo->dwUsedSize;
			TackOnData(lpCallInfo, activeLine->incomingToDisplayName, &lpCallInfo->dwCalledIDNameSize);
			lpCallInfo->dwCalledIDFlags |= LINECALLPARTYID_NAME;
		}
		
		// TODO: connectedID

		//std::string connectedID;
		//if ( ( calledIDName = ourCall->getCalledID() ) != "" )
		//{
		//	TSPTRACE("Inserting connectedID: %s ", calledIDName.c_str());
		//	lpCallInfo->dwCalledIDOffset = lpCallInfo->dwUsedSize;
		//	TackOnData(lpCallInfo, calledIDName.c_str() ,&lpCallInfo->dwCalledIDSize);
		//	lpCallInfo->dwConnectedIDFlags |= LINECALLPARTYID_ADDRESS;
		//}

		//std::string connectedIDName;
		//if ( ( connectedIDName = ourCall->getCalledID() ) != "" )
		//{
		//	TSPTRACE("Inserting calledIDName: %s ", calledIDName.c_str());
		//	lpCallInfo->dwConnectedIDNameOffset = lpCallInfo->dwUsedSize;
		//	TackOnData(lpCallInfo, connectedIDName.c_str() ,&lpCallInfo->dwConnectedIDNameSize);
		//	lpCallInfo->dwConnectedIDFlags |= LINECALLPARTYID_NAME;
		//}

		TSPTRACE("SipStack::TSPI_lineGetCallInfo: unlock sipStackMutex 2 ...\n");
		g_sipStack->sipStackMut.Unlock();

		return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallAddressID
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallAddressID(
									   HDRVCALL            hdCall,
									   LPDWORD             pdwAddressID
									   )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetCallAddressID")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Installation, removal and configuration
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// Helper Function to enable/disable GUI elements
//
////////////////////////////////////////////////////////////////////////////////

#ifdef MULTILINE
void EnableGuiElements(HWND hwnd, BOOL bEnable) {
    int i;
    static int elements[] =
    {
		IDC_PROXY01, IDC_OBPROXY01, IDC_USERNAME01, IDC_PASSWORD01, IDC_AUTHUSERNAME01, IDC_PHONEUSERNAME01, IDC_LINEACTIVE01, IDC_REGISTER01, IDC_AA01,
		IDC_PROXY02, IDC_OBPROXY02, IDC_USERNAME02, IDC_PASSWORD02, IDC_AUTHUSERNAME02, IDC_PHONEUSERNAME02, IDC_LINEACTIVE02, IDC_REGISTER02, IDC_AA02,
		IDC_PROXY03, IDC_OBPROXY03, IDC_USERNAME03, IDC_PASSWORD03, IDC_AUTHUSERNAME03, IDC_PHONEUSERNAME03, IDC_LINEACTIVE03, IDC_REGISTER03, IDC_AA03,
		IDC_PROXY04, IDC_OBPROXY04, IDC_USERNAME04, IDC_PASSWORD04, IDC_AUTHUSERNAME04, IDC_PHONEUSERNAME04, IDC_LINEACTIVE04, IDC_REGISTER04, IDC_AA04,
		IDC_PROXY05, IDC_OBPROXY05, IDC_USERNAME05, IDC_PASSWORD05, IDC_AUTHUSERNAME05, IDC_PHONEUSERNAME05, IDC_LINEACTIVE05, IDC_REGISTER05, IDC_AA05,
		IDC_PROXY06, IDC_OBPROXY06, IDC_USERNAME06, IDC_PASSWORD06, IDC_AUTHUSERNAME06, IDC_PHONEUSERNAME06, IDC_LINEACTIVE06, IDC_REGISTER06, IDC_AA06,
		IDC_PROXY07, IDC_OBPROXY07, IDC_USERNAME07, IDC_PASSWORD07, IDC_AUTHUSERNAME07, IDC_PHONEUSERNAME07, IDC_LINEACTIVE07, IDC_REGISTER07, IDC_AA07,
		IDC_PROXY08, IDC_OBPROXY08, IDC_USERNAME08, IDC_PASSWORD08, IDC_AUTHUSERNAME08, IDC_PHONEUSERNAME08, IDC_LINEACTIVE08, IDC_REGISTER08, IDC_AA08,
		IDC_PROXY09, IDC_OBPROXY09, IDC_USERNAME09, IDC_PASSWORD09, IDC_AUTHUSERNAME09, IDC_PHONEUSERNAME09, IDC_LINEACTIVE09, IDC_REGISTER09, IDC_AA09,
		IDC_PROXY10, IDC_OBPROXY10, IDC_USERNAME10, IDC_PASSWORD10, IDC_AUTHUSERNAME10, IDC_PHONEUSERNAME10, IDC_LINEACTIVE10, IDC_REGISTER10, IDC_AA10,
		IDC_PROXY11, IDC_OBPROXY11, IDC_USERNAME11, IDC_PASSWORD11, IDC_AUTHUSERNAME11, IDC_PHONEUSERNAME11, IDC_LINEACTIVE11, IDC_REGISTER11, IDC_AA11,
		IDC_PROXY12, IDC_OBPROXY12, IDC_USERNAME12, IDC_PASSWORD12, IDC_AUTHUSERNAME12, IDC_PHONEUSERNAME12, IDC_LINEACTIVE12, IDC_REGISTER12, IDC_AA12,
		IDC_PROXY13, IDC_OBPROXY13, IDC_USERNAME13, IDC_PASSWORD13, IDC_AUTHUSERNAME13, IDC_PHONEUSERNAME13, IDC_LINEACTIVE13, IDC_REGISTER13, IDC_AA13,
		IDC_PROXY14, IDC_OBPROXY14, IDC_USERNAME14, IDC_PASSWORD14, IDC_AUTHUSERNAME14, IDC_PHONEUSERNAME14, IDC_LINEACTIVE14, IDC_REGISTER14, IDC_AA14,
		IDC_PROXY15, IDC_OBPROXY15, IDC_USERNAME15, IDC_PASSWORD15, IDC_AUTHUSERNAME15, IDC_PHONEUSERNAME15, IDC_LINEACTIVE15, IDC_REGISTER15, IDC_AA15,
		IDC_PROXY16, IDC_OBPROXY16, IDC_USERNAME16, IDC_PASSWORD16, IDC_AUTHUSERNAME16, IDC_PHONEUSERNAME16, IDC_LINEACTIVE16, IDC_REGISTER16, IDC_AA16,
		IDC_PROXY17, IDC_OBPROXY17, IDC_USERNAME17, IDC_PASSWORD17, IDC_AUTHUSERNAME17, IDC_PHONEUSERNAME17, IDC_LINEACTIVE17, IDC_REGISTER17, IDC_AA17,
		IDC_PROXY18, IDC_OBPROXY18, IDC_USERNAME18, IDC_PASSWORD18, IDC_AUTHUSERNAME18, IDC_PHONEUSERNAME18, IDC_LINEACTIVE18, IDC_REGISTER18, IDC_AA18,
		IDC_PROXY19, IDC_OBPROXY19, IDC_USERNAME19, IDC_PASSWORD19, IDC_AUTHUSERNAME19, IDC_PHONEUSERNAME19, IDC_LINEACTIVE19, IDC_REGISTER19, IDC_AA19,
		IDC_PROXY20, IDC_OBPROXY20, IDC_USERNAME20, IDC_PASSWORD20, IDC_AUTHUSERNAME20, IDC_PHONEUSERNAME20, IDC_LINEACTIVE20, IDC_REGISTER20, IDC_AA20,
		IDC_PROXY21, IDC_OBPROXY21, IDC_USERNAME21, IDC_PASSWORD21, IDC_AUTHUSERNAME21, IDC_PHONEUSERNAME21, IDC_LINEACTIVE21, IDC_REGISTER21, IDC_AA21,
		IDC_PROXY22, IDC_OBPROXY22, IDC_USERNAME22, IDC_PASSWORD22, IDC_AUTHUSERNAME22, IDC_PHONEUSERNAME22, IDC_LINEACTIVE22, IDC_REGISTER22, IDC_AA22,
		IDC_PROXY23, IDC_OBPROXY23, IDC_USERNAME23, IDC_PASSWORD23, IDC_AUTHUSERNAME23, IDC_PHONEUSERNAME23, IDC_LINEACTIVE23, IDC_REGISTER23, IDC_AA23,
		IDC_PROXY24, IDC_OBPROXY24, IDC_USERNAME24, IDC_PASSWORD24, IDC_AUTHUSERNAME24, IDC_PHONEUSERNAME24, IDC_LINEACTIVE24, IDC_REGISTER24, IDC_AA24,
		IDC_PROXY25, IDC_OBPROXY25, IDC_USERNAME25, IDC_PASSWORD25, IDC_AUTHUSERNAME25, IDC_PHONEUSERNAME25, IDC_LINEACTIVE25, IDC_REGISTER25, IDC_AA25,
		IDC_PROXY26, IDC_OBPROXY26, IDC_USERNAME26, IDC_PASSWORD26, IDC_AUTHUSERNAME26, IDC_PHONEUSERNAME26, IDC_LINEACTIVE26, IDC_REGISTER26, IDC_AA26,
		IDC_PROXY27, IDC_OBPROXY27, IDC_USERNAME27, IDC_PASSWORD27, IDC_AUTHUSERNAME27, IDC_PHONEUSERNAME27, IDC_LINEACTIVE27, IDC_REGISTER27, IDC_AA27,
		IDC_PROXY28, IDC_OBPROXY28, IDC_USERNAME28, IDC_PASSWORD28, IDC_AUTHUSERNAME28, IDC_PHONEUSERNAME28, IDC_LINEACTIVE28, IDC_REGISTER28, IDC_AA28,
		IDC_PROXY29, IDC_OBPROXY29, IDC_USERNAME29, IDC_PASSWORD29, IDC_AUTHUSERNAME29, IDC_PHONEUSERNAME29, IDC_LINEACTIVE29, IDC_REGISTER29, IDC_AA29,
		IDC_PROXY30, IDC_OBPROXY30, IDC_USERNAME30, IDC_PASSWORD30, IDC_AUTHUSERNAME30, IDC_PHONEUSERNAME30, IDC_LINEACTIVE30, IDC_REGISTER30, IDC_AA30,
		IDC_PROXY31, IDC_OBPROXY31, IDC_USERNAME31, IDC_PASSWORD31, IDC_AUTHUSERNAME31, IDC_PHONEUSERNAME31, IDC_LINEACTIVE31, IDC_REGISTER31, IDC_AA31,
		IDC_PROXY32, IDC_OBPROXY32, IDC_USERNAME32, IDC_PASSWORD32, IDC_AUTHUSERNAME32, IDC_PHONEUSERNAME32, IDC_LINEACTIVE32, IDC_REGISTER32, IDC_AA32,
		IDC_PROXY33, IDC_OBPROXY33, IDC_USERNAME33, IDC_PASSWORD33, IDC_AUTHUSERNAME33, IDC_PHONEUSERNAME33, IDC_LINEACTIVE33, IDC_REGISTER33, IDC_AA33,
		IDC_PROXY34, IDC_OBPROXY34, IDC_USERNAME34, IDC_PASSWORD34, IDC_AUTHUSERNAME34, IDC_PHONEUSERNAME34, IDC_LINEACTIVE34, IDC_REGISTER34, IDC_AA34,
		IDC_PROXY35, IDC_OBPROXY35, IDC_USERNAME35, IDC_PASSWORD35, IDC_AUTHUSERNAME35, IDC_PHONEUSERNAME35, IDC_LINEACTIVE35, IDC_REGISTER35, IDC_AA35,
		IDC_PROXY36, IDC_OBPROXY36, IDC_USERNAME36, IDC_PASSWORD36, IDC_AUTHUSERNAME36, IDC_PHONEUSERNAME36, IDC_LINEACTIVE36, IDC_REGISTER36, IDC_AA36,
		IDC_PROXY37, IDC_OBPROXY37, IDC_USERNAME37, IDC_PASSWORD37, IDC_AUTHUSERNAME37, IDC_PHONEUSERNAME37, IDC_LINEACTIVE37, IDC_REGISTER37, IDC_AA37,
		IDC_PROXY38, IDC_OBPROXY38, IDC_USERNAME38, IDC_PASSWORD38, IDC_AUTHUSERNAME38, IDC_PHONEUSERNAME38, IDC_LINEACTIVE38, IDC_REGISTER38, IDC_AA38,
		IDC_PROXY39, IDC_OBPROXY39, IDC_USERNAME39, IDC_PASSWORD39, IDC_AUTHUSERNAME39, IDC_PHONEUSERNAME39, IDC_LINEACTIVE39, IDC_REGISTER39, IDC_AA39,
		IDC_PROXY40, IDC_OBPROXY40, IDC_USERNAME40, IDC_PASSWORD40, IDC_AUTHUSERNAME40, IDC_PHONEUSERNAME40, IDC_LINEACTIVE40, IDC_REGISTER40, IDC_AA40,
        0
    };
	TSPTRACE("EnableGuiElements: %s", bEnable?"enabling":"disabling");

    for (i = 0; elements[i]; i++) {
        EnableWindow (GetDlgItem (hwnd, elements[i]), bEnable);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Function ConfigDlgProc
//
// The callback function for our dialog box.
//
////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK ConfigDlgProc(
							HWND    hwnd,
							UINT    nMsg,
							WPARAM  wparam,
							LPARAM  lparam)
{
	BOOL    b = FALSE;
	char szTemp[256];
	std::string temp;
	DWORD tempInt;

	switch( nMsg )
	{
	case WM_INITDIALOG:
		//CenterWindow(hwnd);

		// dynamically set the version
		SetDlgItemText(hwnd, IDC_STATIC_VERSION, "SIPTAPI " SIPTAPI_VERSION_SHORT " (this software is GPL licensed) www.ipcom.at");

		// if a domain-suffix is defined, write it to the config dialog
#ifdef DOMAINSUFFIX
		SetDlgItemText(hwnd, IDC_STATIC_DSUFFIX, "Following domain suffix is appended: " DOMAINSUFFIX);
#endif

		readConfigInt("enablerealmcheck", tempInt);
		CheckDlgButton(hwnd, IDC_CHECK_REALM, tempInt);
		readConfigInt("reversemode", tempInt);
		if (tempInt == 2) {
			CheckDlgButton(hwnd, IDC_CHECK_REVERSEMODE, 1);
		} else if (tempInt == 1) {
			CheckDlgButton(hwnd, IDC_CHECK_ACDMODE, 1);
		}
		readConfigInt("transportprotocol", tempInt);
		CheckDlgButton(hwnd, IDC_CHECK_TCP, tempInt);
		readConfigInt("send180ringing", tempInt);
		CheckDlgButton(hwnd, IDC_CHECK_SEND180, tempInt);

		readConfigString("proxy1", temp);
#ifdef TRIALPROXY
	temp = TRIALPROXY;
#endif
		SetDlgItemText(hwnd, IDC_PROXY01, temp.c_str());
		readConfigString("obproxy1", temp);
#ifdef TRIALOUTBOUNDPROXY
	temp = TRIALOUTBOUNDPROXY;
#endif
		SetDlgItemText(hwnd, IDC_OBPROXY01, temp.c_str());
		readConfigString("username1", temp);
#ifdef TRIALUSERNAME
	temp = TRIALUSERNAME;
#endif
		SetDlgItemText(hwnd, IDC_USERNAME01, temp.c_str());
		readConfigString("password1", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD01, temp.c_str());
		readConfigString("authusername1", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME01, temp.c_str());
		readConfigString("phoneusername1", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME01, temp.c_str());
		readConfigInt("lineactive1", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE01, tempInt);
		readConfigInt("register1", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER01, tempInt);
		readConfigInt("autoanswer1", tempInt);
		CheckDlgButton(hwnd, IDC_AA01, tempInt);

#ifdef MULTILINE
		readConfigString("proxy2", temp);
		SetDlgItemText(hwnd, IDC_PROXY02, temp.c_str());
		readConfigString("obproxy2", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY02, temp.c_str());
		readConfigString("username2", temp);
		SetDlgItemText(hwnd, IDC_USERNAME02, temp.c_str());
		readConfigString("password2", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD02, temp.c_str());
		readConfigString("authusername2", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME02, temp.c_str());
		readConfigString("phoneusername2", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME02, temp.c_str());
		readConfigInt("lineactive2", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE02, tempInt);
		readConfigInt("register2", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER02, tempInt);
		readConfigInt("autoanswer2", tempInt);
		CheckDlgButton(hwnd, IDC_AA02, tempInt);

		readConfigString("proxy3", temp);
		SetDlgItemText(hwnd, IDC_PROXY03, temp.c_str());
		readConfigString("obproxy3", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY03, temp.c_str());
		readConfigString("username3", temp);
		SetDlgItemText(hwnd, IDC_USERNAME03, temp.c_str());
		readConfigString("password3", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD03, temp.c_str());
		readConfigString("authusername3", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME03, temp.c_str());
		readConfigString("phoneusername3", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME03, temp.c_str());
		readConfigInt("lineactive3", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE03, tempInt);
		readConfigInt("register3", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER03, tempInt);
		readConfigInt("autoanswer3", tempInt);
		CheckDlgButton(hwnd, IDC_AA03, tempInt);

		readConfigString("proxy4", temp);
		SetDlgItemText(hwnd, IDC_PROXY04, temp.c_str());
		readConfigString("obproxy4", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY04, temp.c_str());
		readConfigString("username4", temp);
		SetDlgItemText(hwnd, IDC_USERNAME04, temp.c_str());
		readConfigString("password4", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD04, temp.c_str());
		readConfigString("authusername4", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME04, temp.c_str());
		readConfigString("phoneusername4", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME04, temp.c_str());
		readConfigInt("lineactive4", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE04, tempInt);
		readConfigInt("register4", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER04, tempInt);
		readConfigInt("autoanswer4", tempInt);
		CheckDlgButton(hwnd, IDC_AA04, tempInt);

		readConfigString("proxy5", temp);
		SetDlgItemText(hwnd, IDC_PROXY05, temp.c_str());
		readConfigString("obproxy5", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY05, temp.c_str());
		readConfigString("username5", temp);
		SetDlgItemText(hwnd, IDC_USERNAME05, temp.c_str());
		readConfigString("password5", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD05, temp.c_str());
		readConfigString("authusername5", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME05, temp.c_str());
		readConfigString("phoneusername5", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME05, temp.c_str());
		readConfigInt("lineactive5", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE05, tempInt);
		readConfigInt("register5", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER05, tempInt);
		readConfigInt("autoanswer5", tempInt);
		CheckDlgButton(hwnd, IDC_AA05, tempInt);

		readConfigString("proxy6", temp);
		SetDlgItemText(hwnd, IDC_PROXY06, temp.c_str());
		readConfigString("obproxy6", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY06, temp.c_str());
		readConfigString("username6", temp);
		SetDlgItemText(hwnd, IDC_USERNAME06, temp.c_str());
		readConfigString("password6", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD06, temp.c_str());
		readConfigString("authusername6", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME06, temp.c_str());
		readConfigString("phoneusername6", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME06, temp.c_str());
		readConfigInt("lineactive6", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE06, tempInt);
		readConfigInt("register6", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER06, tempInt);
		readConfigInt("autoanswer6", tempInt);
		CheckDlgButton(hwnd, IDC_AA06, tempInt);

		readConfigString("proxy7", temp);
		SetDlgItemText(hwnd, IDC_PROXY07, temp.c_str());
		readConfigString("obproxy7", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY07, temp.c_str());
		readConfigString("username7", temp);
		SetDlgItemText(hwnd, IDC_USERNAME07, temp.c_str());
		readConfigString("password7", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD07, temp.c_str());
		readConfigString("authusername7", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME07, temp.c_str());
		readConfigString("phoneusername7", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME07, temp.c_str());
		readConfigInt("lineactive7", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE07, tempInt);
		readConfigInt("register7", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER07, tempInt);
		readConfigInt("autoanswer7", tempInt);
		CheckDlgButton(hwnd, IDC_AA07, tempInt);

		readConfigString("proxy8", temp);
		SetDlgItemText(hwnd, IDC_PROXY08, temp.c_str());
		readConfigString("obproxy8", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY08, temp.c_str());
		readConfigString("username8", temp);
		SetDlgItemText(hwnd, IDC_USERNAME08, temp.c_str());
		readConfigString("password8", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD08, temp.c_str());
		readConfigString("authusername8", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME08, temp.c_str());
		readConfigString("phoneusername8", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME08, temp.c_str());
		readConfigInt("lineactive8", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE08, tempInt);
		readConfigInt("register8", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER08, tempInt);
		readConfigInt("autoanswer8", tempInt);
		CheckDlgButton(hwnd, IDC_AA08, tempInt);

		readConfigString("proxy9", temp);
		SetDlgItemText(hwnd, IDC_PROXY09, temp.c_str());
		readConfigString("obproxy9", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY09, temp.c_str());
		readConfigString("username9", temp);
		SetDlgItemText(hwnd, IDC_USERNAME09, temp.c_str());
		readConfigString("password9", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD09, temp.c_str());
		readConfigString("authusername9", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME09, temp.c_str());
		readConfigString("phoneusername9", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME09, temp.c_str());
		readConfigInt("lineactive9", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE09, tempInt);
		readConfigInt("register9", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER09, tempInt);
		readConfigInt("autoanswer9", tempInt);
		CheckDlgButton(hwnd, IDC_AA09, tempInt);

		readConfigString("proxy10", temp);
		SetDlgItemText(hwnd, IDC_PROXY10, temp.c_str());
		readConfigString("obproxy10", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY10, temp.c_str());
		readConfigString("username10", temp);
		SetDlgItemText(hwnd, IDC_USERNAME10, temp.c_str());
		readConfigString("password10", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD10, temp.c_str());
		readConfigString("authusername10", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME10, temp.c_str());
		readConfigString("phoneusername10", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME10, temp.c_str());
		readConfigInt("lineactive10", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE10, tempInt);
		readConfigInt("register10", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER10, tempInt);
		readConfigInt("autoanswer10", tempInt);
		CheckDlgButton(hwnd, IDC_AA10, tempInt);

		readConfigString("proxy11", temp);
		SetDlgItemText(hwnd, IDC_PROXY11, temp.c_str());
		readConfigString("obproxy11", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY11, temp.c_str());
		readConfigString("username11", temp);
		SetDlgItemText(hwnd, IDC_USERNAME11, temp.c_str());
		readConfigString("password11", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD11, temp.c_str());
		readConfigString("authusername11", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME11, temp.c_str());
		readConfigString("phoneusername11", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME11, temp.c_str());
		readConfigInt("lineactive11", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE11, tempInt);
		readConfigInt("register11", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER11, tempInt);
		readConfigInt("autoanswer11", tempInt);
		CheckDlgButton(hwnd, IDC_AA11, tempInt);

		readConfigString("proxy12", temp);
		SetDlgItemText(hwnd, IDC_PROXY12, temp.c_str());
		readConfigString("obproxy12", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY12, temp.c_str());
		readConfigString("username12", temp);
		SetDlgItemText(hwnd, IDC_USERNAME12, temp.c_str());
		readConfigString("password12", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD12, temp.c_str());
		readConfigString("authusername12", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME12, temp.c_str());
		readConfigString("phoneusername12", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME12, temp.c_str());
		readConfigInt("lineactive12", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE12, tempInt);
		readConfigInt("register12", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER12, tempInt);
		readConfigInt("autoanswer12", tempInt);
		CheckDlgButton(hwnd, IDC_AA12, tempInt);

		readConfigString("proxy13", temp);
		SetDlgItemText(hwnd, IDC_PROXY13, temp.c_str());
		readConfigString("obproxy13", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY13, temp.c_str());
		readConfigString("username13", temp);
		SetDlgItemText(hwnd, IDC_USERNAME13, temp.c_str());
		readConfigString("password13", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD13, temp.c_str());
		readConfigString("authusername13", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME13, temp.c_str());
		readConfigString("phoneusername13", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME13, temp.c_str());
		readConfigInt("lineactive13", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE13, tempInt);
		readConfigInt("register13", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER13, tempInt);
		readConfigInt("autoanswer13", tempInt);
		CheckDlgButton(hwnd, IDC_AA13, tempInt);

		readConfigString("proxy14", temp);
		SetDlgItemText(hwnd, IDC_PROXY14, temp.c_str());
		readConfigString("obproxy14", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY14, temp.c_str());
		readConfigString("username14", temp);
		SetDlgItemText(hwnd, IDC_USERNAME14, temp.c_str());
		readConfigString("password14", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD14, temp.c_str());
		readConfigString("authusername14", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME14, temp.c_str());
		readConfigString("phoneusername14", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME14, temp.c_str());
		readConfigInt("lineactive14", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE14, tempInt);
		readConfigInt("register14", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER14, tempInt);
		readConfigInt("autoanswer14", tempInt);
		CheckDlgButton(hwnd, IDC_AA14, tempInt);

		readConfigString("proxy15", temp);
		SetDlgItemText(hwnd, IDC_PROXY15, temp.c_str());
		readConfigString("obproxy15", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY15, temp.c_str());
		readConfigString("username15", temp);
		SetDlgItemText(hwnd, IDC_USERNAME15, temp.c_str());
		readConfigString("password15", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD15, temp.c_str());
		readConfigString("authusername15", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME15, temp.c_str());
		readConfigString("phoneusername15", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME15, temp.c_str());
		readConfigInt("lineactive15", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE15, tempInt);
		readConfigInt("register15", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER15, tempInt);
		readConfigInt("autoanswer15", tempInt);
		CheckDlgButton(hwnd, IDC_AA15, tempInt);

		readConfigString("proxy16", temp);
		SetDlgItemText(hwnd, IDC_PROXY16, temp.c_str());
		readConfigString("obproxy16", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY16, temp.c_str());
		readConfigString("username16", temp);
		SetDlgItemText(hwnd, IDC_USERNAME16, temp.c_str());
		readConfigString("password16", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD16, temp.c_str());
		readConfigString("authusername16", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME16, temp.c_str());
		readConfigString("phoneusername16", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME16, temp.c_str());
		readConfigInt("lineactive16", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE16, tempInt);
		readConfigInt("register16", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER16, tempInt);
		readConfigInt("autoanswer16", tempInt);
		CheckDlgButton(hwnd, IDC_AA16, tempInt);

		readConfigString("proxy17", temp);
		SetDlgItemText(hwnd, IDC_PROXY17, temp.c_str());
		readConfigString("obproxy17", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY17, temp.c_str());
		readConfigString("username17", temp);
		SetDlgItemText(hwnd, IDC_USERNAME17, temp.c_str());
		readConfigString("password17", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD17, temp.c_str());
		readConfigString("authusername17", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME17, temp.c_str());
		readConfigString("phoneusername17", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME17, temp.c_str());
		readConfigInt("lineactive17", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE17, tempInt);
		readConfigInt("register17", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER17, tempInt);
		readConfigInt("autoanswer17", tempInt);
		CheckDlgButton(hwnd, IDC_AA17, tempInt);

		readConfigString("proxy18", temp);
		SetDlgItemText(hwnd, IDC_PROXY18, temp.c_str());
		readConfigString("obproxy18", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY18, temp.c_str());
		readConfigString("username18", temp);
		SetDlgItemText(hwnd, IDC_USERNAME18, temp.c_str());
		readConfigString("password18", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD18, temp.c_str());
		readConfigString("authusername18", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME18, temp.c_str());
		readConfigString("phoneusername18", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME18, temp.c_str());
		readConfigInt("lineactive18", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE18, tempInt);
		readConfigInt("register18", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER18, tempInt);
		readConfigInt("autoanswer18", tempInt);
		CheckDlgButton(hwnd, IDC_AA18, tempInt);

		readConfigString("proxy19", temp);
		SetDlgItemText(hwnd, IDC_PROXY19, temp.c_str());
		readConfigString("obproxy19", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY19, temp.c_str());
		readConfigString("username19", temp);
		SetDlgItemText(hwnd, IDC_USERNAME19, temp.c_str());
		readConfigString("password19", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD19, temp.c_str());
		readConfigString("authusername19", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME19, temp.c_str());
		readConfigString("phoneusername19", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME19, temp.c_str());
		readConfigInt("lineactive19", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE19, tempInt);
		readConfigInt("register19", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER19, tempInt);
		readConfigInt("autoanswer19", tempInt);
		CheckDlgButton(hwnd, IDC_AA19, tempInt);

		readConfigString("proxy20", temp);
		SetDlgItemText(hwnd, IDC_PROXY20, temp.c_str());
		readConfigString("obproxy20", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY20, temp.c_str());
		readConfigString("username20", temp);
		SetDlgItemText(hwnd, IDC_USERNAME20, temp.c_str());
		readConfigString("password20", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD20, temp.c_str());
		readConfigString("authusername20", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME20, temp.c_str());
		readConfigString("phoneusername20", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME20, temp.c_str());
		readConfigInt("lineactive20", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE20, tempInt);
		readConfigInt("register20", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER20, tempInt);
		readConfigInt("autoanswer20", tempInt);
		CheckDlgButton(hwnd, IDC_AA20, tempInt);

		readConfigString("proxy21", temp);
		SetDlgItemText(hwnd, IDC_PROXY21, temp.c_str());
		readConfigString("obproxy21", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY21, temp.c_str());
		readConfigString("username21", temp);
		SetDlgItemText(hwnd, IDC_USERNAME21, temp.c_str());
		readConfigString("password21", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD21, temp.c_str());
		readConfigString("authusername21", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME21, temp.c_str());
		readConfigString("phoneusername21", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME21, temp.c_str());
		readConfigInt("lineactive21", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE21, tempInt);
		readConfigInt("register21", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER21, tempInt);
		readConfigInt("autoanswer21", tempInt);
		CheckDlgButton(hwnd, IDC_AA21, tempInt);

		readConfigString("proxy22", temp);
		SetDlgItemText(hwnd, IDC_PROXY22, temp.c_str());
		readConfigString("obproxy22", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY22, temp.c_str());
		readConfigString("username22", temp);
		SetDlgItemText(hwnd, IDC_USERNAME22, temp.c_str());
		readConfigString("password22", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD22, temp.c_str());
		readConfigString("authusername22", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME22, temp.c_str());
		readConfigString("phoneusername22", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME22, temp.c_str());
		readConfigInt("lineactive22", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE22, tempInt);
		readConfigInt("register22", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER22, tempInt);
		readConfigInt("autoanswer22", tempInt);
		CheckDlgButton(hwnd, IDC_AA22, tempInt);

		readConfigString("proxy23", temp);
		SetDlgItemText(hwnd, IDC_PROXY23, temp.c_str());
		readConfigString("obproxy23", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY23, temp.c_str());
		readConfigString("username23", temp);
		SetDlgItemText(hwnd, IDC_USERNAME23, temp.c_str());
		readConfigString("password23", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD23, temp.c_str());
		readConfigString("authusername23", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME23, temp.c_str());
		readConfigString("phoneusername23", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME23, temp.c_str());
		readConfigInt("lineactive23", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE23, tempInt);
		readConfigInt("register23", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER23, tempInt);
		readConfigInt("autoanswer23", tempInt);
		CheckDlgButton(hwnd, IDC_AA23, tempInt);

		readConfigString("proxy24", temp);
		SetDlgItemText(hwnd, IDC_PROXY24, temp.c_str());
		readConfigString("obproxy24", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY24, temp.c_str());
		readConfigString("username24", temp);
		SetDlgItemText(hwnd, IDC_USERNAME24, temp.c_str());
		readConfigString("password24", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD24, temp.c_str());
		readConfigString("authusername24", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME24, temp.c_str());
		readConfigString("phoneusername24", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME24, temp.c_str());
		readConfigInt("lineactive24", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE24, tempInt);
		readConfigInt("register24", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER24, tempInt);
		readConfigInt("autoanswer24", tempInt);
		CheckDlgButton(hwnd, IDC_AA24, tempInt);

		readConfigString("proxy25", temp);
		SetDlgItemText(hwnd, IDC_PROXY25, temp.c_str());
		readConfigString("obproxy25", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY25, temp.c_str());
		readConfigString("username25", temp);
		SetDlgItemText(hwnd, IDC_USERNAME25, temp.c_str());
		readConfigString("password25", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD25, temp.c_str());
		readConfigString("authusername25", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME25, temp.c_str());
		readConfigString("phoneusername25", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME25, temp.c_str());
		readConfigInt("lineactive25", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE25, tempInt);
		readConfigInt("register25", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER25, tempInt);
		readConfigInt("autoanswer25", tempInt);
		CheckDlgButton(hwnd, IDC_AA25, tempInt);

		readConfigString("proxy26", temp);
		SetDlgItemText(hwnd, IDC_PROXY26, temp.c_str());
		readConfigString("obproxy26", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY26, temp.c_str());
		readConfigString("username26", temp);
		SetDlgItemText(hwnd, IDC_USERNAME26, temp.c_str());
		readConfigString("password26", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD26, temp.c_str());
		readConfigString("authusername26", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME26, temp.c_str());
		readConfigString("phoneusername26", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME26, temp.c_str());
		readConfigInt("lineactive26", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE26, tempInt);
		readConfigInt("register26", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER26, tempInt);
		readConfigInt("autoanswer26", tempInt);
		CheckDlgButton(hwnd, IDC_AA26, tempInt);

		readConfigString("proxy27", temp);
		SetDlgItemText(hwnd, IDC_PROXY27, temp.c_str());
		readConfigString("obproxy27", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY27, temp.c_str());
		readConfigString("username27", temp);
		SetDlgItemText(hwnd, IDC_USERNAME27, temp.c_str());
		readConfigString("password27", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD27, temp.c_str());
		readConfigString("authusername27", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME27, temp.c_str());
		readConfigString("phoneusername27", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME27, temp.c_str());
		readConfigInt("lineactive27", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE27, tempInt);
		readConfigInt("register27", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER27, tempInt);
		readConfigInt("autoanswer27", tempInt);
		CheckDlgButton(hwnd, IDC_AA27, tempInt);

		readConfigString("proxy28", temp);
		SetDlgItemText(hwnd, IDC_PROXY28, temp.c_str());
		readConfigString("obproxy28", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY28, temp.c_str());
		readConfigString("username28", temp);
		SetDlgItemText(hwnd, IDC_USERNAME28, temp.c_str());
		readConfigString("password28", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD28, temp.c_str());
		readConfigString("authusername28", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME28, temp.c_str());
		readConfigString("phoneusername28", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME28, temp.c_str());
		readConfigInt("lineactive28", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE28, tempInt);
		readConfigInt("register28", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER28, tempInt);
		readConfigInt("autoanswer28", tempInt);
		CheckDlgButton(hwnd, IDC_AA28, tempInt);

		readConfigString("proxy29", temp);
		SetDlgItemText(hwnd, IDC_PROXY29, temp.c_str());
		readConfigString("obproxy29", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY29, temp.c_str());
		readConfigString("username29", temp);
		SetDlgItemText(hwnd, IDC_USERNAME29, temp.c_str());
		readConfigString("password29", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD29, temp.c_str());
		readConfigString("authusername29", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME29, temp.c_str());
		readConfigString("phoneusername29", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME29, temp.c_str());
		readConfigInt("lineactive29", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE29, tempInt);
		readConfigInt("register29", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER29, tempInt);
		readConfigInt("autoanswer29", tempInt);
		CheckDlgButton(hwnd, IDC_AA29, tempInt);

		readConfigString("proxy30", temp);
		SetDlgItemText(hwnd, IDC_PROXY30, temp.c_str());
		readConfigString("obproxy30", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY30, temp.c_str());
		readConfigString("username30", temp);
		SetDlgItemText(hwnd, IDC_USERNAME30, temp.c_str());
		readConfigString("password30", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD30, temp.c_str());
		readConfigString("authusername30", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME30, temp.c_str());
		readConfigString("phoneusername30", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME30, temp.c_str());
		readConfigInt("lineactive30", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE30, tempInt);
		readConfigInt("register30", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER30, tempInt);
		readConfigInt("autoanswer30", tempInt);
		CheckDlgButton(hwnd, IDC_AA30, tempInt);

		readConfigString("proxy31", temp);
		SetDlgItemText(hwnd, IDC_PROXY31, temp.c_str());
		readConfigString("obproxy31", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY31, temp.c_str());
		readConfigString("username31", temp);
		SetDlgItemText(hwnd, IDC_USERNAME31, temp.c_str());
		readConfigString("password31", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD31, temp.c_str());
		readConfigString("authusername31", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME31, temp.c_str());
		readConfigString("phoneusername31", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME31, temp.c_str());
		readConfigInt("lineactive31", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE31, tempInt);
		readConfigInt("register31", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER31, tempInt);
		readConfigInt("autoanswer31", tempInt);
		CheckDlgButton(hwnd, IDC_AA31, tempInt);

		readConfigString("proxy32", temp);
		SetDlgItemText(hwnd, IDC_PROXY32, temp.c_str());
		readConfigString("obproxy32", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY32, temp.c_str());
		readConfigString("username32", temp);
		SetDlgItemText(hwnd, IDC_USERNAME32, temp.c_str());
		readConfigString("password32", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD32, temp.c_str());
		readConfigString("authusername32", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME32, temp.c_str());
		readConfigString("phoneusername32", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME32, temp.c_str());
		readConfigInt("lineactive32", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE32, tempInt);
		readConfigInt("register32", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER32, tempInt);
		readConfigInt("autoanswer32", tempInt);
		CheckDlgButton(hwnd, IDC_AA32, tempInt);

		readConfigString("proxy33", temp);
		SetDlgItemText(hwnd, IDC_PROXY33, temp.c_str());
		readConfigString("obproxy33", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY33, temp.c_str());
		readConfigString("username33", temp);
		SetDlgItemText(hwnd, IDC_USERNAME33, temp.c_str());
		readConfigString("password33", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD33, temp.c_str());
		readConfigString("authusername33", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME33, temp.c_str());
		readConfigString("phoneusername33", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME33, temp.c_str());
		readConfigInt("lineactive33", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE33, tempInt);
		readConfigInt("register33", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER33, tempInt);
		readConfigInt("autoanswer33", tempInt);
		CheckDlgButton(hwnd, IDC_AA33, tempInt);

		readConfigString("proxy34", temp);
		SetDlgItemText(hwnd, IDC_PROXY34, temp.c_str());
		readConfigString("obproxy34", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY34, temp.c_str());
		readConfigString("username34", temp);
		SetDlgItemText(hwnd, IDC_USERNAME34, temp.c_str());
		readConfigString("password34", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD34, temp.c_str());
		readConfigString("authusername34", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME34, temp.c_str());
		readConfigString("phoneusername34", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME34, temp.c_str());
		readConfigInt("lineactive34", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE34, tempInt);
		readConfigInt("register34", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER34, tempInt);
		readConfigInt("autoanswer34", tempInt);
		CheckDlgButton(hwnd, IDC_AA34, tempInt);

		readConfigString("proxy35", temp);
		SetDlgItemText(hwnd, IDC_PROXY35, temp.c_str());
		readConfigString("obproxy35", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY35, temp.c_str());
		readConfigString("username35", temp);
		SetDlgItemText(hwnd, IDC_USERNAME35, temp.c_str());
		readConfigString("password35", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD35, temp.c_str());
		readConfigString("authusername35", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME35, temp.c_str());
		readConfigString("phoneusername35", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME35, temp.c_str());
		readConfigInt("lineactive35", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE35, tempInt);
		readConfigInt("register35", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER35, tempInt);
		readConfigInt("autoanswer35", tempInt);
		CheckDlgButton(hwnd, IDC_AA35, tempInt);

		readConfigString("proxy36", temp);
		SetDlgItemText(hwnd, IDC_PROXY36, temp.c_str());
		readConfigString("obproxy36", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY36, temp.c_str());
		readConfigString("username36", temp);
		SetDlgItemText(hwnd, IDC_USERNAME36, temp.c_str());
		readConfigString("password36", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD36, temp.c_str());
		readConfigString("authusername36", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME36, temp.c_str());
		readConfigString("phoneusername36", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME36, temp.c_str());
		readConfigInt("lineactive36", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE36, tempInt);
		readConfigInt("register36", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER36, tempInt);
		readConfigInt("autoanswer36", tempInt);
		CheckDlgButton(hwnd, IDC_AA36, tempInt);

		readConfigString("proxy37", temp);
		SetDlgItemText(hwnd, IDC_PROXY37, temp.c_str());
		readConfigString("obproxy37", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY37, temp.c_str());
		readConfigString("username37", temp);
		SetDlgItemText(hwnd, IDC_USERNAME37, temp.c_str());
		readConfigString("password37", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD37, temp.c_str());
		readConfigString("authusername37", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME37, temp.c_str());
		readConfigString("phoneusername37", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME37, temp.c_str());
		readConfigInt("lineactive37", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE37, tempInt);
		readConfigInt("register37", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER37, tempInt);
		readConfigInt("autoanswer37", tempInt);
		CheckDlgButton(hwnd, IDC_AA37, tempInt);

		readConfigString("proxy38", temp);
		SetDlgItemText(hwnd, IDC_PROXY38, temp.c_str());
		readConfigString("obproxy38", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY38, temp.c_str());
		readConfigString("username38", temp);
		SetDlgItemText(hwnd, IDC_USERNAME38, temp.c_str());
		readConfigString("password38", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD38, temp.c_str());
		readConfigString("authusername38", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME38, temp.c_str());
		readConfigString("phoneusername38", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME38, temp.c_str());
		readConfigInt("lineactive38", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE38, tempInt);
		readConfigInt("register38", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER38, tempInt);
		readConfigInt("autoanswer38", tempInt);
		CheckDlgButton(hwnd, IDC_AA38, tempInt);

		readConfigString("proxy39", temp);
		SetDlgItemText(hwnd, IDC_PROXY39, temp.c_str());
		readConfigString("obproxy39", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY39, temp.c_str());
		readConfigString("username39", temp);
		SetDlgItemText(hwnd, IDC_USERNAME39, temp.c_str());
		readConfigString("password39", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD39, temp.c_str());
		readConfigString("authusername39", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME39, temp.c_str());
		readConfigString("phoneusername39", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME39, temp.c_str());
		readConfigInt("lineactive39", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE39, tempInt);
		readConfigInt("register39", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER39, tempInt);
		readConfigInt("autoanswer39", tempInt);
		CheckDlgButton(hwnd, IDC_AA39, tempInt);

		readConfigString("proxy40", temp);
		SetDlgItemText(hwnd, IDC_PROXY40, temp.c_str());
		readConfigString("obproxy40", temp);
		SetDlgItemText(hwnd, IDC_OBPROXY40, temp.c_str());
		readConfigString("username40", temp);
		SetDlgItemText(hwnd, IDC_USERNAME40, temp.c_str());
		readConfigString("password40", temp);
		SetDlgItemText(hwnd, IDC_PASSWORD40, temp.c_str());
		readConfigString("authusername40", temp);
		SetDlgItemText(hwnd, IDC_AUTHUSERNAME40, temp.c_str());
		readConfigString("phoneusername40", temp);
		SetDlgItemText(hwnd, IDC_PHONEUSERNAME40, temp.c_str());
		readConfigInt("lineactive40", tempInt);
		CheckDlgButton(hwnd, IDC_LINEACTIVE40, tempInt);
		readConfigInt("register40", tempInt);
		CheckDlgButton(hwnd, IDC_REGISTER40, tempInt);
		readConfigInt("autoanswer40", tempInt);
		CheckDlgButton(hwnd, IDC_AA40, tempInt);

		readConfigInt("lineconfigviaregistry", tempInt);
		CheckDlgButton(hwnd, IDC_CHECK_REGISTRY, tempInt);
		if (tempInt) {
			EnableGuiElements(hwnd, 0);
		}
#endif

#ifdef EXPIRATIONDATE
		// check if license is still valid
		SYSTEMTIME currentSystemTime, licenseSystemTime;
		TSPTRACE("ConfigDlgProc: verify if license is still valid ...");
		GetLocalTime(&currentSystemTime);
		GetLocalTime(&licenseSystemTime); // workaround to init the structure :-)
		TSPTRACE("ConfigDlgProc: parsing validity date ...");
		if (sscanf_s(EXPIRATIONDATE, 
			"%04u-%02u-%02u", &licenseSystemTime.wYear, 
			&licenseSystemTime.wMonth, &licenseSystemTime.wDay) != 3) {
				// error parsing date
				TSPTRACE("ConfigDlgProc: invalid expiration date ...");
				MessageBox(0, "Trial phase expired! SIPTAPI won't work anymore!", 
					"SIPTAPI", MB_OK | MB_ICONERROR | MB_SERVICE_NOTIFICATION);
		} else {
			// compare timestamps
			FILETIME currentFileTime, licenseFileTime;
			TSPTRACE("ConfigDlgProc: compare expiration date ...");
			SystemTimeToFileTime(&currentSystemTime, &currentFileTime);
			SystemTimeToFileTime(&licenseSystemTime, &licenseFileTime);
			if (CompareFileTime(&currentFileTime,&licenseFileTime) == 1) {
				// license expired
				TSPTRACE("ConfigDlgProc: licensed expired at %s",EXPIRATIONDATE);
				MessageBox(0, "Trial phase expired! SIPTAPI won't work anymore!", 
					"SIPTAPI", MB_OK | MB_ICONERROR | MB_SERVICE_NOTIFICATION);
			} else {
				// license valid
				TSPTRACE("ConfigDlgProc: licensed still valid (till %s (%04u-%02u-%02u))",EXPIRATIONDATE,
					licenseSystemTime.wYear, licenseSystemTime.wMonth, licenseSystemTime.wDay);
			}
		}
#endif

		b = TRUE;
		break;

	case WM_COMMAND:
		switch( wparam )
		{
#ifdef MULTILINE
		case IDC_CHECK_REGISTRY:
			if (IsDlgButtonChecked(hwnd, IDC_CHECK_REGISTRY)) {
				EnableGuiElements(hwnd,0);
			} else {
				EnableGuiElements(hwnd,1);
			}
			break;
#endif
		case IDC_CHECK_REVERSEMODE:
			if (IsDlgButtonChecked(hwnd, IDC_CHECK_REVERSEMODE)) {
//				EnableWindow (GetDlgItem (hwnd, IDC_CHECK_ACDMODE), 0);
				CheckDlgButton(hwnd, IDC_CHECK_ACDMODE, BST_UNCHECKED);
			}
			break;
		case IDC_CHECK_ACDMODE:
			if (IsDlgButtonChecked(hwnd, IDC_CHECK_ACDMODE)) {
//				EnableWindow (GetDlgItem (hwnd, IDC_CHECK_REVERSEMODE), 0);
				CheckDlgButton(hwnd, IDC_CHECK_REVERSEMODE, BST_UNCHECKED);
			}
			break;
		case IDOK:
			EndDialog(hwnd, IDOK);
			//plus it will now go onto apply...

		case IDC_APPLY:
			tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_REALM);
			storeConfigInt("enablerealmcheck", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_REVERSEMODE);
			if (tempInt) {
				storeConfigInt("reversemode", 2);
			} else {
				tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_ACDMODE);
				if (tempInt) {
					storeConfigInt("reversemode", 1);
				} else {
					storeConfigInt("reversemode", 0);
				}
			}
			tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_TCP);
			storeConfigInt("transportprotocol", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_SEND180);
			storeConfigInt("send180ringing", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY01, szTemp, sizeof(szTemp));
			storeConfigString("proxy1", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY01, szTemp, sizeof(szTemp));
			storeConfigString("obproxy1", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME01, szTemp, sizeof(szTemp));
			storeConfigString("username1", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD01, szTemp, sizeof(szTemp));
			storeConfigString("password1", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME01, szTemp, sizeof(szTemp));
			storeConfigString("authusername1", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME01, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername1", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE01);
			storeConfigInt("lineactive1", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER01);
			storeConfigInt("register1", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA01);
			storeConfigInt("autoanswer1", tempInt);

#ifdef MULTILINE
			GetDlgItemText(hwnd, IDC_PROXY02, szTemp, sizeof(szTemp));
			storeConfigString("proxy2", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY02, szTemp, sizeof(szTemp));
			storeConfigString("obproxy2", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME02, szTemp, sizeof(szTemp));
			storeConfigString("username2", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD02, szTemp, sizeof(szTemp));
			storeConfigString("password2", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME02, szTemp, sizeof(szTemp));
			storeConfigString("authusername2", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME02, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername2", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE02);
			storeConfigInt("lineactive2", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER02);
			storeConfigInt("register2", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA02);
			storeConfigInt("autoanswer2", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY03, szTemp, sizeof(szTemp));
			storeConfigString("proxy3", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY03, szTemp, sizeof(szTemp));
			storeConfigString("obproxy3", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME03, szTemp, sizeof(szTemp));
			storeConfigString("username3", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD03, szTemp, sizeof(szTemp));
			storeConfigString("password3", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME03, szTemp, sizeof(szTemp));
			storeConfigString("authusername3", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME03, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername3", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE03);
			storeConfigInt("lineactive3", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER03);
			storeConfigInt("register3", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA03);
			storeConfigInt("autoanswer3", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY04, szTemp, sizeof(szTemp));
			storeConfigString("proxy4", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY04, szTemp, sizeof(szTemp));
			storeConfigString("obproxy4", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME04, szTemp, sizeof(szTemp));
			storeConfigString("username4", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD04, szTemp, sizeof(szTemp));
			storeConfigString("password4", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME04, szTemp, sizeof(szTemp));
			storeConfigString("authusername4", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME04, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername4", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE04);
			storeConfigInt("lineactive4", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER04);
			storeConfigInt("register4", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA04);
			storeConfigInt("autoanswer4", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY05, szTemp, sizeof(szTemp));
			storeConfigString("proxy5", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY05, szTemp, sizeof(szTemp));
			storeConfigString("obproxy5", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME05, szTemp, sizeof(szTemp));
			storeConfigString("username5", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD05, szTemp, sizeof(szTemp));
			storeConfigString("password5", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME05, szTemp, sizeof(szTemp));
			storeConfigString("authusername5", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME05, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername5", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE05);
			storeConfigInt("lineactive5", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER05);
			storeConfigInt("register5", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA05);
			storeConfigInt("autoanswer5", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY06, szTemp, sizeof(szTemp));
			storeConfigString("proxy6", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY06, szTemp, sizeof(szTemp));
			storeConfigString("obproxy6", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME06, szTemp, sizeof(szTemp));
			storeConfigString("username6", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD06, szTemp, sizeof(szTemp));
			storeConfigString("password6", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME06, szTemp, sizeof(szTemp));
			storeConfigString("authusername6", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME06, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername6", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE06);
			storeConfigInt("lineactive6", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER06);
			storeConfigInt("register6", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA06);
			storeConfigInt("autoanswer6", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY07, szTemp, sizeof(szTemp));
			storeConfigString("proxy7", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY07, szTemp, sizeof(szTemp));
			storeConfigString("obproxy7", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME07, szTemp, sizeof(szTemp));
			storeConfigString("username7", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD07, szTemp, sizeof(szTemp));
			storeConfigString("password7", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME07, szTemp, sizeof(szTemp));
			storeConfigString("authusername7", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME07, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername7", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE07);
			storeConfigInt("lineactive7", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER07);
			storeConfigInt("register7", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA07);
			storeConfigInt("autoanswer7", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY08, szTemp, sizeof(szTemp));
			storeConfigString("proxy8", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY08, szTemp, sizeof(szTemp));
			storeConfigString("obproxy8", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME08, szTemp, sizeof(szTemp));
			storeConfigString("username8", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD08, szTemp, sizeof(szTemp));
			storeConfigString("password8", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME08, szTemp, sizeof(szTemp));
			storeConfigString("authusername8", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME08, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername8", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE08);
			storeConfigInt("lineactive8", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER08);
			storeConfigInt("register8", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA08);
			storeConfigInt("autoanswer8", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY09, szTemp, sizeof(szTemp));
			storeConfigString("proxy9", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY09, szTemp, sizeof(szTemp));
			storeConfigString("obproxy9", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME09, szTemp, sizeof(szTemp));
			storeConfigString("username9", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD09, szTemp, sizeof(szTemp));
			storeConfigString("password9", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME09, szTemp, sizeof(szTemp));
			storeConfigString("authusername9", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME09, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername9", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE09);
			storeConfigInt("lineactive9", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER09);
			storeConfigInt("register9", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA09);
			storeConfigInt("autoanswer9", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY10, szTemp, sizeof(szTemp));
			storeConfigString("proxy10", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY10, szTemp, sizeof(szTemp));
			storeConfigString("obproxy10", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME10, szTemp, sizeof(szTemp));
			storeConfigString("username10", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD10, szTemp, sizeof(szTemp));
			storeConfigString("password10", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME10, szTemp, sizeof(szTemp));
			storeConfigString("authusername10", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME10, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername10", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE10);
			storeConfigInt("lineactive10", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER10);
			storeConfigInt("register10", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA10);
			storeConfigInt("autoanswer10", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY11, szTemp, sizeof(szTemp));
			storeConfigString("proxy11", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY11, szTemp, sizeof(szTemp));
			storeConfigString("obproxy11", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME11, szTemp, sizeof(szTemp));
			storeConfigString("username11", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD11, szTemp, sizeof(szTemp));
			storeConfigString("password11", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME11, szTemp, sizeof(szTemp));
			storeConfigString("authusername11", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME11, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername11", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE11);
			storeConfigInt("lineactive11", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER11);
			storeConfigInt("register11", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA11);
			storeConfigInt("autoanswer11", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY12, szTemp, sizeof(szTemp));
			storeConfigString("proxy12", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY12, szTemp, sizeof(szTemp));
			storeConfigString("obproxy12", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME12, szTemp, sizeof(szTemp));
			storeConfigString("username12", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD12, szTemp, sizeof(szTemp));
			storeConfigString("password12", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME12, szTemp, sizeof(szTemp));
			storeConfigString("authusername12", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME12, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername12", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE12);
			storeConfigInt("lineactive12", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER12);
			storeConfigInt("register12", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA12);
			storeConfigInt("autoanswer12", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY13, szTemp, sizeof(szTemp));
			storeConfigString("proxy13", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY13, szTemp, sizeof(szTemp));
			storeConfigString("obproxy13", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME13, szTemp, sizeof(szTemp));
			storeConfigString("username13", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD13, szTemp, sizeof(szTemp));
			storeConfigString("password13", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME13, szTemp, sizeof(szTemp));
			storeConfigString("authusername13", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME13, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername13", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE13);
			storeConfigInt("lineactive13", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER13);
			storeConfigInt("register13", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA13);
			storeConfigInt("autoanswer13", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY14, szTemp, sizeof(szTemp));
			storeConfigString("proxy14", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY14, szTemp, sizeof(szTemp));
			storeConfigString("obproxy14", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME14, szTemp, sizeof(szTemp));
			storeConfigString("username14", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD14, szTemp, sizeof(szTemp));
			storeConfigString("password14", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME14, szTemp, sizeof(szTemp));
			storeConfigString("authusername14", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME14, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername14", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE14);
			storeConfigInt("lineactive14", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER14);
			storeConfigInt("register14", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA14);
			storeConfigInt("autoanswer14", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY15, szTemp, sizeof(szTemp));
			storeConfigString("proxy15", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY15, szTemp, sizeof(szTemp));
			storeConfigString("obproxy15", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME15, szTemp, sizeof(szTemp));
			storeConfigString("username15", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD15, szTemp, sizeof(szTemp));
			storeConfigString("password15", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME15, szTemp, sizeof(szTemp));
			storeConfigString("authusername15", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME15, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername15", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE15);
			storeConfigInt("lineactive15", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER15);
			storeConfigInt("register15", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA15);
			storeConfigInt("autoanswer15", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY16, szTemp, sizeof(szTemp));
			storeConfigString("proxy16", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY16, szTemp, sizeof(szTemp));
			storeConfigString("obproxy16", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME16, szTemp, sizeof(szTemp));
			storeConfigString("username16", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD16, szTemp, sizeof(szTemp));
			storeConfigString("password16", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME16, szTemp, sizeof(szTemp));
			storeConfigString("authusername16", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME16, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername16", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE16);
			storeConfigInt("lineactive16", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER16);
			storeConfigInt("register16", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA16);
			storeConfigInt("autoanswer16", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY17, szTemp, sizeof(szTemp));
			storeConfigString("proxy17", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY17, szTemp, sizeof(szTemp));
			storeConfigString("obproxy17", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME17, szTemp, sizeof(szTemp));
			storeConfigString("username17", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD17, szTemp, sizeof(szTemp));
			storeConfigString("password17", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME17, szTemp, sizeof(szTemp));
			storeConfigString("authusername17", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME17, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername17", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE17);
			storeConfigInt("lineactive17", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER17);
			storeConfigInt("register17", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA17);
			storeConfigInt("autoanswer17", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY18, szTemp, sizeof(szTemp));
			storeConfigString("proxy18", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY18, szTemp, sizeof(szTemp));
			storeConfigString("obproxy18", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME18, szTemp, sizeof(szTemp));
			storeConfigString("username18", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD18, szTemp, sizeof(szTemp));
			storeConfigString("password18", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME18, szTemp, sizeof(szTemp));
			storeConfigString("authusername18", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME18, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername18", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE18);
			storeConfigInt("lineactive18", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER18);
			storeConfigInt("register18", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA18);
			storeConfigInt("autoanswer18", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY19, szTemp, sizeof(szTemp));
			storeConfigString("proxy19", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY19, szTemp, sizeof(szTemp));
			storeConfigString("obproxy19", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME19, szTemp, sizeof(szTemp));
			storeConfigString("username19", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD19, szTemp, sizeof(szTemp));
			storeConfigString("password19", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME19, szTemp, sizeof(szTemp));
			storeConfigString("authusername19", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME19, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername19", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE19);
			storeConfigInt("lineactive19", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER19);
			storeConfigInt("register19", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA19);
			storeConfigInt("autoanswer19", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY20, szTemp, sizeof(szTemp));
			storeConfigString("proxy20", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY20, szTemp, sizeof(szTemp));
			storeConfigString("obproxy20", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME20, szTemp, sizeof(szTemp));
			storeConfigString("username20", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD20, szTemp, sizeof(szTemp));
			storeConfigString("password20", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME20, szTemp, sizeof(szTemp));
			storeConfigString("authusername20", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME20, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername20", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE20);
			storeConfigInt("lineactive20", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER20);
			storeConfigInt("register20", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA20);
			storeConfigInt("autoanswer20", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY21, szTemp, sizeof(szTemp));
			storeConfigString("proxy21", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY21, szTemp, sizeof(szTemp));
			storeConfigString("obproxy21", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME21, szTemp, sizeof(szTemp));
			storeConfigString("username21", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD21, szTemp, sizeof(szTemp));
			storeConfigString("password21", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME21, szTemp, sizeof(szTemp));
			storeConfigString("authusername21", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME21, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername21", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE21);
			storeConfigInt("lineactive21", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER21);
			storeConfigInt("register21", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA21);
			storeConfigInt("autoanswer21", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY22, szTemp, sizeof(szTemp));
			storeConfigString("proxy22", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY22, szTemp, sizeof(szTemp));
			storeConfigString("obproxy22", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME22, szTemp, sizeof(szTemp));
			storeConfigString("username22", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD22, szTemp, sizeof(szTemp));
			storeConfigString("password22", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME22, szTemp, sizeof(szTemp));
			storeConfigString("authusername22", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME22, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername22", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE22);
			storeConfigInt("lineactive22", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER22);
			storeConfigInt("register22", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA22);
			storeConfigInt("autoanswer22", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY23, szTemp, sizeof(szTemp));
			storeConfigString("proxy23", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY23, szTemp, sizeof(szTemp));
			storeConfigString("obproxy23", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME23, szTemp, sizeof(szTemp));
			storeConfigString("username23", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD23, szTemp, sizeof(szTemp));
			storeConfigString("password23", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME23, szTemp, sizeof(szTemp));
			storeConfigString("authusername23", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME23, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername23", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE23);
			storeConfigInt("lineactive23", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER23);
			storeConfigInt("register23", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA23);
			storeConfigInt("autoanswer23", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY24, szTemp, sizeof(szTemp));
			storeConfigString("proxy24", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY24, szTemp, sizeof(szTemp));
			storeConfigString("obproxy24", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME24, szTemp, sizeof(szTemp));
			storeConfigString("username24", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD24, szTemp, sizeof(szTemp));
			storeConfigString("password24", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME24, szTemp, sizeof(szTemp));
			storeConfigString("authusername24", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME24, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername24", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE24);
			storeConfigInt("lineactive24", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER24);
			storeConfigInt("register24", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA24);
			storeConfigInt("autoanswer24", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY25, szTemp, sizeof(szTemp));
			storeConfigString("proxy25", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY25, szTemp, sizeof(szTemp));
			storeConfigString("obproxy25", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME25, szTemp, sizeof(szTemp));
			storeConfigString("username25", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD25, szTemp, sizeof(szTemp));
			storeConfigString("password25", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME25, szTemp, sizeof(szTemp));
			storeConfigString("authusername25", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME25, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername25", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE25);
			storeConfigInt("lineactive25", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER25);
			storeConfigInt("register25", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA25);
			storeConfigInt("autoanswer25", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY26, szTemp, sizeof(szTemp));
			storeConfigString("proxy26", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY26, szTemp, sizeof(szTemp));
			storeConfigString("obproxy26", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME26, szTemp, sizeof(szTemp));
			storeConfigString("username26", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD26, szTemp, sizeof(szTemp));
			storeConfigString("password26", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME26, szTemp, sizeof(szTemp));
			storeConfigString("authusername26", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME26, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername26", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE26);
			storeConfigInt("lineactive26", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER26);
			storeConfigInt("register26", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA26);
			storeConfigInt("autoanswer26", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY27, szTemp, sizeof(szTemp));
			storeConfigString("proxy27", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY27, szTemp, sizeof(szTemp));
			storeConfigString("obproxy27", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME27, szTemp, sizeof(szTemp));
			storeConfigString("username27", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD27, szTemp, sizeof(szTemp));
			storeConfigString("password27", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME27, szTemp, sizeof(szTemp));
			storeConfigString("authusername27", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME27, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername27", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE27);
			storeConfigInt("lineactive27", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER27);
			storeConfigInt("register27", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA27);
			storeConfigInt("autoanswer27", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY28, szTemp, sizeof(szTemp));
			storeConfigString("proxy28", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY28, szTemp, sizeof(szTemp));
			storeConfigString("obproxy28", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME28, szTemp, sizeof(szTemp));
			storeConfigString("username28", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD28, szTemp, sizeof(szTemp));
			storeConfigString("password28", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME28, szTemp, sizeof(szTemp));
			storeConfigString("authusername28", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME28, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername28", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE28);
			storeConfigInt("lineactive28", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER28);
			storeConfigInt("register28", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA28);
			storeConfigInt("autoanswer28", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY29, szTemp, sizeof(szTemp));
			storeConfigString("proxy29", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY29, szTemp, sizeof(szTemp));
			storeConfigString("obproxy29", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME29, szTemp, sizeof(szTemp));
			storeConfigString("username29", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD29, szTemp, sizeof(szTemp));
			storeConfigString("password29", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME29, szTemp, sizeof(szTemp));
			storeConfigString("authusername29", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME29, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername29", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE29);
			storeConfigInt("lineactive29", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER29);
			storeConfigInt("register29", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA29);
			storeConfigInt("autoanswer29", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY30, szTemp, sizeof(szTemp));
			storeConfigString("proxy30", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY30, szTemp, sizeof(szTemp));
			storeConfigString("obproxy30", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME30, szTemp, sizeof(szTemp));
			storeConfigString("username30", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD30, szTemp, sizeof(szTemp));
			storeConfigString("password30", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME30, szTemp, sizeof(szTemp));
			storeConfigString("authusername30", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME30, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername30", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE30);
			storeConfigInt("lineactive30", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER30);
			storeConfigInt("register30", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA30);
			storeConfigInt("autoanswer30", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY31, szTemp, sizeof(szTemp));
			storeConfigString("proxy31", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY31, szTemp, sizeof(szTemp));
			storeConfigString("obproxy31", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME31, szTemp, sizeof(szTemp));
			storeConfigString("username31", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD31, szTemp, sizeof(szTemp));
			storeConfigString("password31", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME31, szTemp, sizeof(szTemp));
			storeConfigString("authusername31", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME31, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername31", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE31);
			storeConfigInt("lineactive31", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER31);
			storeConfigInt("register31", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA31);
			storeConfigInt("autoanswer31", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY32, szTemp, sizeof(szTemp));
			storeConfigString("proxy32", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY32, szTemp, sizeof(szTemp));
			storeConfigString("obproxy32", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME32, szTemp, sizeof(szTemp));
			storeConfigString("username32", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD32, szTemp, sizeof(szTemp));
			storeConfigString("password32", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME32, szTemp, sizeof(szTemp));
			storeConfigString("authusername32", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME32, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername32", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE32);
			storeConfigInt("lineactive32", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER32);
			storeConfigInt("register32", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA32);
			storeConfigInt("autoanswer32", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY33, szTemp, sizeof(szTemp));
			storeConfigString("proxy33", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY33, szTemp, sizeof(szTemp));
			storeConfigString("obproxy33", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME33, szTemp, sizeof(szTemp));
			storeConfigString("username33", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD33, szTemp, sizeof(szTemp));
			storeConfigString("password33", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME33, szTemp, sizeof(szTemp));
			storeConfigString("authusername33", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME33, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername33", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE33);
			storeConfigInt("lineactive33", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER33);
			storeConfigInt("register33", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA33);
			storeConfigInt("autoanswer33", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY34, szTemp, sizeof(szTemp));
			storeConfigString("proxy34", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY34, szTemp, sizeof(szTemp));
			storeConfigString("obproxy34", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME34, szTemp, sizeof(szTemp));
			storeConfigString("username34", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD34, szTemp, sizeof(szTemp));
			storeConfigString("password34", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME34, szTemp, sizeof(szTemp));
			storeConfigString("authusername34", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME34, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername34", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE34);
			storeConfigInt("lineactive34", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER34);
			storeConfigInt("register34", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA34);
			storeConfigInt("autoanswer34", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY35, szTemp, sizeof(szTemp));
			storeConfigString("proxy35", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY35, szTemp, sizeof(szTemp));
			storeConfigString("obproxy35", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME35, szTemp, sizeof(szTemp));
			storeConfigString("username35", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD35, szTemp, sizeof(szTemp));
			storeConfigString("password35", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME35, szTemp, sizeof(szTemp));
			storeConfigString("authusername35", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME35, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername35", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE35);
			storeConfigInt("lineactive35", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER35);
			storeConfigInt("register35", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA35);
			storeConfigInt("autoanswer35", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY36, szTemp, sizeof(szTemp));
			storeConfigString("proxy36", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY36, szTemp, sizeof(szTemp));
			storeConfigString("obproxy36", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME36, szTemp, sizeof(szTemp));
			storeConfigString("username36", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD36, szTemp, sizeof(szTemp));
			storeConfigString("password36", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME36, szTemp, sizeof(szTemp));
			storeConfigString("authusername36", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME36, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername36", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE36);
			storeConfigInt("lineactive36", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER36);
			storeConfigInt("register36", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA36);
			storeConfigInt("autoanswer36", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY37, szTemp, sizeof(szTemp));
			storeConfigString("proxy37", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY37, szTemp, sizeof(szTemp));
			storeConfigString("obproxy37", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME37, szTemp, sizeof(szTemp));
			storeConfigString("username37", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD37, szTemp, sizeof(szTemp));
			storeConfigString("password37", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME37, szTemp, sizeof(szTemp));
			storeConfigString("authusername37", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME37, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername37", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE37);
			storeConfigInt("lineactive37", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER37);
			storeConfigInt("register37", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA37);
			storeConfigInt("autoanswer37", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY38, szTemp, sizeof(szTemp));
			storeConfigString("proxy38", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY38, szTemp, sizeof(szTemp));
			storeConfigString("obproxy38", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME38, szTemp, sizeof(szTemp));
			storeConfigString("username38", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD38, szTemp, sizeof(szTemp));
			storeConfigString("password38", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME38, szTemp, sizeof(szTemp));
			storeConfigString("authusername38", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME38, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername38", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE38);
			storeConfigInt("lineactive38", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER38);
			storeConfigInt("register38", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA38);
			storeConfigInt("autoanswer38", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY39, szTemp, sizeof(szTemp));
			storeConfigString("proxy39", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY39, szTemp, sizeof(szTemp));
			storeConfigString("obproxy39", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME39, szTemp, sizeof(szTemp));
			storeConfigString("username39", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD39, szTemp, sizeof(szTemp));
			storeConfigString("password39", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME39, szTemp, sizeof(szTemp));
			storeConfigString("authusername39", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME39, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername39", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE39);
			storeConfigInt("lineactive39", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER39);
			storeConfigInt("register39", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA39);
			storeConfigInt("autoanswer39", tempInt);

			GetDlgItemText(hwnd, IDC_PROXY40, szTemp, sizeof(szTemp));
			storeConfigString("proxy40", szTemp);
			GetDlgItemText(hwnd, IDC_OBPROXY40, szTemp, sizeof(szTemp));
			storeConfigString("obproxy40", szTemp);
			GetDlgItemText(hwnd, IDC_USERNAME40, szTemp, sizeof(szTemp));
			storeConfigString("username40", szTemp);
			GetDlgItemText(hwnd, IDC_PASSWORD40, szTemp, sizeof(szTemp));
			storeConfigString("password40", szTemp);
			GetDlgItemText(hwnd, IDC_AUTHUSERNAME40, szTemp, sizeof(szTemp));
			storeConfigString("authusername40", szTemp);
			GetDlgItemText(hwnd, IDC_PHONEUSERNAME40, szTemp, sizeof(szTemp));
			storeConfigString("phoneusername40", szTemp);
			tempInt = IsDlgButtonChecked(hwnd, IDC_LINEACTIVE40);
			storeConfigInt("lineactive40", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_REGISTER40);
			storeConfigInt("register40", tempInt);
			tempInt = IsDlgButtonChecked(hwnd, IDC_AA40);
			storeConfigInt("autoanswer40", tempInt);

			tempInt = IsDlgButtonChecked(hwnd, IDC_CHECK_REGISTRY);
			storeConfigInt("lineconfigviaregistry", tempInt);
#endif

			b= TRUE;
			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		break;
	}

	return b;
}


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerInstall
//
// Called by TAPI on installation, ideal oppertunity to gather information
// via. some form of user interface.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerInstall(
									TUISPIDLLCALLBACK pfnUIDLLCallback,
									HWND hwndOwner,
									DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerInstall")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		initConfigStore();

	std::string strData;

#ifdef MULTILINE
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_MULTI),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#else
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_SINGLE),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#endif


	MessageBox(hwndOwner, "SIPTAPI Service Provider installed",
		"TAPI Installation", MB_OK);

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerInstall(
					 HWND                hwndOwner,
					 DWORD               dwPermanentProviderID
					 )
{
	// Although this func is never called by TAPI v2.0, we export
	// it so that the Telephony Control Panel Applet knows that it
	// can add this provider via lineAddProvider(), otherwise
	// Telephon.cpl will not consider it installable

	BEGIN_PARAM_TABLE("TSPI_providerInstall")
		DWORD_IN_ENTRY(hwndOwner)
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerRemove(
								   TUISPIDLLCALLBACK pfnUIDLLCallback,
								   HWND hwndOwner,
								   DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerRemove")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		return EPILOG(0);
}
LONG TSPIAPI TSPI_providerRemove(
								 HWND    hwndOwner,
								 DWORD   dwPermanentProviderID)
{
	// Although this func is never called by TAPI v2.0, we export
	// it so that the Telephony Control Panel Applet knows that it
	// can remove this provider via lineRemoveProvider(), otherwise
	// Telephon.cpl will not consider it removable
	BEGIN_PARAM_TABLE("TSPI_providerRemove")
		DWORD_IN_ENTRY(hwndOwner)
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		return EPILOG(0);
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerUIIdentify(
									   TUISPIDLLCALLBACK pfnUIDLLCallback,
									   LPWSTR pszUIDLLName)
{
	BEGIN_PARAM_TABLE("TUISPI_providerUIIdentify")
		STRING_IN_ENTRY(pszUIDLLName)
		END_PARAM_TABLE()

		return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Dialog config area
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_lineConfigDialog
//
// Called when a request for config is made upon us.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_lineConfigDialog(                                        // TSPI v2.0
									 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
									 DWORD               dwDeviceID,
									 HWND                hwndOwner,
									 LPCWSTR             lpszDeviceClass
									 )
{
	BEGIN_PARAM_TABLE("TUISPI_lineConfigDialog")
		DWORD_IN_ENTRY(lpfnUIDLLCallback)
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(hwndOwner)
		STRING_IN_ENTRY(lpszDeviceClass)	//A pointer to a null-terminated string
		END_PARAM_TABLE()

#ifdef MULTILINE
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_MULTI),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#else
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_SINGLE),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#endif

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerUIIdentify
//
// Becuase the UI part of a TSP can exsist in another DLL we need to tell TAPI
// that this is the DLL which provides the UI functionality as well.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerUIIdentify(                                        // TSPI v2.0
									 LPWSTR              lpszUIDLLName
									 )
{
	BEGIN_PARAM_TABLE("TSPI_providerUIIdentify")
		STRING_IN_ENTRY(lpszUIDLLName)	//Pointer to a block of memory at least MAX_PATH in length, into which the service provider must copy a null-terminated string specifying the fully qualified path for the DLL containing the service provider functions which must execute in the process of the calling application.
		END_PARAM_TABLE()

		char szPath[MAX_PATH+1];
	GetModuleFileNameA(g_hinst,szPath,MAX_PATH);
	mbstowcs(lpszUIDLLName,szPath,strlen(szPath)+1);

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerGenericDialogData(                                 // TSPI v2.0
							   DWORD_PTR           dwObjectID,
							   DWORD               dwObjectType,
							   LPVOID              lpParams,
							   DWORD               dwSize
							   )
{
	BEGIN_PARAM_TABLE("TSPI_providerGenericDialogData")
		DWORD_IN_ENTRY(dwObjectID)
		DWORD_IN_ENTRY(dwObjectType)
		DWORD_IN_ENTRY(lpParams)
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerConfig
//
// Obsolete - only in TAPI version 1.4 and below
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerConfig(
								   TUISPIDLLCALLBACK pfnUIDLLCallback,
								   HWND hwndOwner,
								   DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerConfig")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

#ifdef MULTILINE
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_MULTI),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#else
	DialogBox(g_hinst,
		MAKEINTRESOURCE(IDD_DIALOG_SINGLE),
		hwndOwner,
		(DLGPROC) ConfigDlgProc);
#endif

	return EPILOG(0);
}

///////////////////Start////////////////////
LONG
TSPIAPI
TSPI_lineAccept(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall,
				LPCSTR              lpsUserUserInfo,
				DWORD               dwSize
				)
{
	BEGIN_PARAM_TABLE("TSPI_lineAccept")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()
//	return EPILOG(0);
	return EPILOG(LINEERR_INUSE);
}

LONG
TSPIAPI
TSPI_lineAddToConference(
						 DRV_REQUESTID       dwRequestID,
						 HDRVCALL            hdConfCall,
						 HDRVCALL            hdConsultCall
						 )
{
	BEGIN_PARAM_TABLE("TSPI_lineAddToConference")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()
		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineAnswer(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall,
				LPCSTR              lpsUserUserInfo,
				DWORD               dwSize
				)
{
	BEGIN_PARAM_TABLE("TSPI_lineAnswer")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()
//	return EPILOG(0);
	return EPILOG(LINEERR_INUSE);
}


LONG
TSPIAPI
TSPI_lineBlindTransfer(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   LPCWSTR             lpszDestAddress,
					   DWORD               dwCountryCode)
{
	BEGIN_PARAM_TABLE("TSPI_lineBlindTransfer")
		DWORD_IN_ENTRY(dwRequestID)
		DWORD_IN_ENTRY(hdCall)
		STRING_IN_ENTRY(lpszDestAddress)	//A pointer to a null-terminated Unicode string
		DWORD_IN_ENTRY(dwCountryCode)
		END_PARAM_TABLE()

	TSPTRACE("TSPI_lineBlindTransfer: ...\n");

	TapiLine *activeLine;
	TSPTRACE("TSPI_lineDrop: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineBlindTransfer: ERROR: line/call not found ...\n");
		TSPTRACE(":TSPI_lineBlindTransfer: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}

	if (activeLine->referTo(lpszDestAddress) != 0) {
		TSPTRACE("TSPI_lineBlindTransfer: error initiating blind transfer ...\n");
		TSPTRACE("SipStack::TSPI_lineBlindTransfer: unlock sipStackMutex 3...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_OPERATIONFAILED);
	}

	TSPTRACE("TSPI_lineBlindTransfer: blind transfer result will be reported async ...\n");
	activeLine->transferRequestId = dwRequestID;

	TSPTRACE("SipStack::TSPI_lineBlindTransfer: unlock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Unlock();

	TSPTRACE("TSPI_lineBlindTransfer: ... done\n");
	return EPILOG(dwRequestID);
}





LONG
TSPIAPI
TSPI_lineCompleteCall(
					  DRV_REQUESTID       dwRequestID,
					  HDRVCALL            hdCall,
					  LPDWORD             lpdwCompletionID,
					  DWORD               dwCompletionMode,
					  DWORD               dwMessageID
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineCompleteCall")
		DWORD_IN_ENTRY(dwCompletionMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineCompleteTransfer(
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  HDRVCALL            hdConsultCall,
						  HTAPICALL           htConfCall,
						  LPHDRVCALL          lphdConfCall,
						  DWORD               dwTransferMode
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineCompleteTransfer")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineConditionalMediaDetection(
								   HDRVLINE            hdLine,
								   DWORD               dwMediaModes,
								   LPLINECALLPARAMS    const lpCallParams
								   )
{
	BEGIN_PARAM_TABLE("TSPI_lineConditionalMediaDetection")
		DWORD_IN_ENTRY(dwMediaModes)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDevSpecific(
					 DRV_REQUESTID       dwRequestID,
					 HDRVLINE            hdLine,
					 DWORD               dwAddressID,
					 HDRVCALL            hdCall,
					 LPVOID              lpParams,
					 DWORD               dwSize
					 )
{
	BEGIN_PARAM_TABLE("TSPI_lineDevSpecific")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDevSpecificFeature(
							DRV_REQUESTID       dwRequestID,
							HDRVLINE            hdLine,
							DWORD               dwFeature,
							LPVOID              lpParams,
							DWORD               dwSize
							)
{
	BEGIN_PARAM_TABLE("TSPI_lineDevSpecificFeature")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineDial(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall,
			  LPCWSTR             lpszDestAddress,
			  DWORD               dwCountryCode
			  )
{
	BEGIN_PARAM_TABLE("TSPI_lineDial")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineDropOnClose(                                           // TSPI v1.4
					 HDRVCALL            hdCall
					 )
{
	BEGIN_PARAM_TABLE("TSPI_lineDropOnClose")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDropNoOwner(                                           // TSPI v1.4
					 HDRVCALL            hdCall
					 )
{
	BEGIN_PARAM_TABLE("TSPI_lineDropNoOwner")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineForward(
				 DRV_REQUESTID       dwRequestID,
				 HDRVLINE            hdLine,
				 DWORD               bAllAddresses,
				 DWORD               dwAddressID,
				 LPLINEFORWARDLIST   const lpForwardList,
				 DWORD               dwNumRingsNoAnswer,
				 HTAPICALL           htConsultCall,
				 LPHDRVCALL          lphdConsultCall,
				 LPLINECALLPARAMS    const lpCallParams
				 )
{
	BEGIN_PARAM_TABLE("TSPI_lineForward")
		DWORD_IN_ENTRY(bAllAddresses)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGatherDigits(
					  HDRVCALL            hdCall,
					  DWORD               dwEndToEndID,
					  DWORD               dwDigitModes,
					  LPWSTR              lpsDigits,
					  DWORD               dwNumDigits,
					  LPCWSTR             lpszTerminationDigits,
					  DWORD               dwFirstDigitTimeout,
					  DWORD               dwInterDigitTimeout
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGatherDigits")
		DWORD_IN_ENTRY(dwEndToEndID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGenerateDigits(
						HDRVCALL            hdCall,
						DWORD               dwEndToEndID,
						DWORD               dwDigitMode,
						LPCWSTR             lpszDigits,
						DWORD               dwDuration
						)
{
	BEGIN_PARAM_TABLE("TSPI_lineGenerateDigits")
		DWORD_IN_ENTRY(dwEndToEndID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGenerateTone(
					  HDRVCALL            hdCall,
					  DWORD               dwEndToEndID,
					  DWORD               dwToneMode,
					  DWORD               dwDuration,
					  DWORD               dwNumTones,
					  LPLINEGENERATETONE  const lpTones
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGenerateTone")
		DWORD_IN_ENTRY(dwEndToEndID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetAddressID(
					  HDRVLINE            hdLine,
					  LPDWORD             lpdwAddressID,
					  DWORD               dwAddressMode,
					  LPCWSTR             lpsAddress,
					  DWORD               dwSize
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressID")
		DWORD_IN_ENTRY(dwAddressMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGetAddressStatus(
						  HDRVLINE            hdLine,
						  DWORD               dwAddressID,
						  LPLINEADDRESSSTATUS lpAddressStatus
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressStatus")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()


		////This function can be expanded when we impliment
		////parking, transfer etc

		lpAddressStatus->dwUsedSize = sizeof(LINEADDRESSSTATUS);
		lpAddressStatus->dwNeededSize = sizeof(LINEADDRESSSTATUS);
		
		TspTrace("TSPI_lineGetAddressStatus: received: dwNumInUse = %ld", lpAddressStatus->dwNumInUse);
		TspTrace("TSPI_lineGetAddressStatus: received: dwNumActiveCalls = %ld", lpAddressStatus->dwNumActiveCalls);
		TspTrace("TSPI_lineGetAddressStatus: received: dwAddressFeatures = %ld", lpAddressStatus->dwAddressFeatures);

		// Actually we should count the active calls for an address, but just send back something
		// to make Lotus Organizer happy.
		//DWORD dwUsedSize;
		lpAddressStatus->dwNumInUse = 1;
		lpAddressStatus->dwNumActiveCalls = 0;
		lpAddressStatus->dwNumOnHoldCalls = 0;
		lpAddressStatus->dwNumOnHoldPendCalls = 0;
		lpAddressStatus->dwAddressFeatures = LINEADDRFEATURE_MAKECALL;
		//DWORD dwNumRingsNoAnswer;
		//DWORD dwForwardNumEntries;
		//DWORD dwForwardSize;
		//DWORD dwForwardOffset;
		//DWORD dwTerminalModesSize;
		//DWORD dwTerminalModesOffset;
		//DWORD dwDevSpecificSize;
		//DWORD dwDevSpecificOffset;

		TspTrace("TSPI_lineGetAddressStatus: send: dwNumInUse = %ld", lpAddressStatus->dwNumInUse);
		TspTrace("TSPI_lineGetAddressStatus: send: dwNumActiveCalls = %ld", lpAddressStatus->dwNumActiveCalls);
		TspTrace("TSPI_lineGetAddressStatus: send: dwAddressFeatures = %ld", lpAddressStatus->dwAddressFeatures);

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGetDevConfig(
					  DWORD               dwDeviceID,
					  LPVARSTRING         lpDeviceConfig,
					  LPCWSTR             lpszDeviceClass
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetDevConfig")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetExtensionID(
						DWORD               dwDeviceID,
						DWORD               dwTSPIVersion,
						LPLINEEXTENSIONID   lpExtensionID
						)
{
	BEGIN_PARAM_TABLE("TSPI_lineGetExtensionID")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		lpExtensionID->dwExtensionID0 = 0;
	lpExtensionID->dwExtensionID1 = 0;
	lpExtensionID->dwExtensionID2 = 0;
	lpExtensionID->dwExtensionID3 = 0;

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetIcon(
				 DWORD               dwDeviceID,
				 LPCWSTR             lpszDeviceClass,
				 LPHICON             lphIcon
				 )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetIcon")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetID(
			   HDRVLINE            hdLine,
			   DWORD               dwAddressID,
			   HDRVCALL            hdCall,
			   DWORD               dwSelect,
			   LPVARSTRING         lpDeviceID,
			   LPCWSTR             lpszDeviceClass,
			   HANDLE              hTargetProcess                          // TSPI v2.0
			   )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetID")
		DWORD_IN_ENTRY(dwSelect)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineGetNumAddressIDs(
						  HDRVLINE            hdLine,
						  LPDWORD             lpdwNumAddressIDs
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetNumAddressIDs")
		DWORD_IN_ENTRY(hdLine)
		END_PARAM_TABLE()

		*lpdwNumAddressIDs = 1;

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineHold(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall
			  )
{
	BEGIN_PARAM_TABLE("TSPI_lineHold")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineMonitorDigits(
					   HDRVCALL            hdCall,
					   DWORD               dwDigitModes
					   )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorDigits")
		DWORD_IN_ENTRY(dwDigitModes)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineMonitorMedia(
					  HDRVCALL            hdCall,
					  DWORD               dwMediaModes
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorMedia")
		DWORD_IN_ENTRY(dwMediaModes)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineMonitorTones(
					  HDRVCALL            hdCall,
					  DWORD               dwToneListID,
					  LPLINEMONITORTONE   const lpToneList,
					  DWORD               dwNumEntries
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorTones")
		DWORD_IN_ENTRY(dwToneListID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineNegotiateExtVersion(
							 DWORD               dwDeviceID,
							 DWORD               dwTSPIVersion,
							 DWORD               dwLowVersion,
							 DWORD               dwHighVersion,
							 LPDWORD             lpdwExtVersion
							 )
{
	BEGIN_PARAM_TABLE("TSPI_lineNegotiateExtVersion")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_linePark(
			  DRV_REQUESTID       dwRequestID,
			  HDRVCALL            hdCall,
			  DWORD               dwParkMode,
			  LPCWSTR             lpszDirAddress,
			  LPVARSTRING         lpNonDirAddress
			  )
{
	BEGIN_PARAM_TABLE("TSPI_linePark")
		DWORD_IN_ENTRY(dwParkMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_linePickup(
				DRV_REQUESTID       dwRequestID,
				HDRVLINE            hdLine,
				DWORD               dwAddressID,
				HTAPICALL           htCall,
				LPHDRVCALL          lphdCall,
				LPCWSTR             lpszDestAddress,
				LPCWSTR             lpszGroupID
				)
{
	BEGIN_PARAM_TABLE("TSPI_linePickup")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_linePrepareAddToConference(
								DRV_REQUESTID       dwRequestID,
								HDRVCALL            hdConfCall,
								HTAPICALL           htConsultCall,
								LPHDRVCALL          lphdConsultCall,
								LPLINECALLPARAMS    const lpCallParams
								)
{
	BEGIN_PARAM_TABLE("TSPI_linePrepareAddToConference")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineRedirect(
				  DRV_REQUESTID       dwRequestID,
				  HDRVCALL            hdCall,
				  LPCWSTR             lpszDestAddress,
				  DWORD               dwCountryCode
				  )
{
	BEGIN_PARAM_TABLE("TSPI_lineRedirect")
		DWORD_IN_ENTRY(dwCountryCode)
		END_PARAM_TABLE()

	TSPTRACE("TSPI_lineRedirect: ...\n");

	TapiLine *activeLine;
	TSPTRACE("TSPI_lineRedirect: lock sipStackMutex ...\n");
	g_sipStack->sipStackMut.Lock();
	activeLine = g_sipStack->getTapiLineFromHdcall(hdCall);
	if (activeLine == NULL) {
		TSPTRACE("TSPI_lineRedirect: ERROR: line/call not found ...\n");
		TSPTRACE("TSPI_lineRedirect: unlock sipStackMutex ...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}

	if (activeLine->redirectTo(lpszDestAddress) == 0) {
		TSPTRACE("TSPI_lineRedirect: error initiating redirect ...\n");
		TSPTRACE("TSPI_lineRedirect: unlock sipStackMutex 3...\n");
		g_sipStack->sipStackMut.Unlock();
		return EPILOG(LINEERR_OPERATIONFAILED);
	}

	TSPTRACE("TSPI_lineRedirect: redirect result will be reported async ...\n");
	activeLine->redirectRequestId = dwRequestID;

	TSPTRACE("TSPI_lineRedirect: unlock sipStackMutex 2...\n");
	g_sipStack->sipStackMut.Unlock();

	TSPTRACE("TSPI_lineRedirect: ... done\n");
	return EPILOG(dwRequestID);
}

LONG
TSPIAPI
TSPI_lineReleaseUserUserInfo(                                   // TSPI v1.4
							 DRV_REQUESTID       dwRequestID,
							 HDRVCALL            hdCall
							 )
{
	BEGIN_PARAM_TABLE("TSPI_lineReleaseUserUserInfo")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineRemoveFromConference(
							  DRV_REQUESTID       dwRequestID,
							  HDRVCALL            hdCall
							  )
{
	BEGIN_PARAM_TABLE("TSPI_lineRemoveFromConference")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSecureCall(
					DRV_REQUESTID       dwRequestID,
					HDRVCALL            hdCall
					)
{
	BEGIN_PARAM_TABLE("TSPI_lineSecureCall")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSelectExtVersion(
						  HDRVLINE            hdLine,
						  DWORD               dwExtVersion
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSelectExtVersion")
		DWORD_IN_ENTRY(dwExtVersion)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSendUserUserInfo(
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  LPCSTR              lpsUserUserInfo,
						  DWORD               dwSize
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSendUserUserInfo")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetAppSpecific(
						HDRVCALL            hdCall,
						DWORD               dwAppSpecific
						)
{
	BEGIN_PARAM_TABLE("TSPI_lineSetAppSpecific")
		DWORD_IN_ENTRY(dwAppSpecific)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallData(                                           // TSPI v2.0
					 DRV_REQUESTID       dwRequestID,
					 HDRVCALL            hdCall,
					 LPVOID              lpCallData,
					 DWORD               dwSize
					 )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallData")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()
		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallParams(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   DWORD               dwBearerMode,
					   DWORD               dwMinRate,
					   DWORD               dwMaxRate,
					   LPLINEDIALPARAMS    const lpDialParams
					   )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallParams")
		DWORD_IN_ENTRY(dwBearerMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallQualityOfService(                               // TSPI v2.0
								 DRV_REQUESTID       dwRequestID,
								 HDRVCALL            hdCall,
								 LPVOID              lpSendingFlowspec,
								 DWORD               dwSendingFlowspecSize,
								 LPVOID              lpReceivingFlowspec,
								 DWORD               dwReceivingFlowspecSize
								 )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallQualityOfService")
		DWORD_IN_ENTRY(dwSendingFlowspecSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetCallTreatment(                                      // TSPI v2.0
						  DRV_REQUESTID       dwRequestID,
						  HDRVCALL            hdCall,
						  DWORD               dwTreatment
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallTreatment")
		DWORD_IN_ENTRY(dwTreatment)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCurrentLocation(                                    // TSPI v1.4
							DWORD               dwLocation
							)
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCurrentLocation")
		DWORD_IN_ENTRY(dwLocation)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetDefaultMediaDetection(
								  HDRVLINE            hdLine,
								  DWORD               dwMediaModes
								  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetDefaultMediaDetection")
		DWORD_IN_ENTRY(dwMediaModes)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetDevConfig(
					  DWORD               dwDeviceID,
					  LPVOID              const lpDeviceConfig,
					  DWORD               dwSize,
					  LPCWSTR             lpszDeviceClass
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetDevConfig")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineSetLineDevStatus(                                      // TSPI v2.0
						  DRV_REQUESTID       dwRequestID,
						  HDRVLINE            hdLine,
						  DWORD               dwStatusToChange,
						  DWORD               fStatus
						  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetLineDevStatus")
		DWORD_IN_ENTRY(dwStatusToChange)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetMediaControl(
						 HDRVLINE                    hdLine,
						 DWORD                       dwAddressID,
						 HDRVCALL                    hdCall,
						 DWORD                       dwSelect,
						 LPLINEMEDIACONTROLDIGIT     const lpDigitList,
						 DWORD                       dwDigitNumEntries,
						 LPLINEMEDIACONTROLMEDIA     const lpMediaList,
						 DWORD                       dwMediaNumEntries,
						 LPLINEMEDIACONTROLTONE      const lpToneList,
						 DWORD                       dwToneNumEntries,
						 LPLINEMEDIACONTROLCALLSTATE const lpCallStateList,
						 DWORD                       dwCallStateNumEntries
						 )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetMediaControl")
		DWORD_IN_ENTRY(dwSelect)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetMediaMode(
					  HDRVCALL            hdCall,
					  DWORD               dwMediaMode
					  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetMediaMode")
		DWORD_IN_ENTRY(dwMediaMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetStatusMessages(
						   HDRVLINE            hdLine,
						   DWORD               dwLineStates,
						   DWORD               dwAddressStates
						   )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetStatusMessages")
		DWORD_IN_ENTRY(dwLineStates)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetTerminal(
					 DRV_REQUESTID       dwRequestID,
					 HDRVLINE            hdLine,
					 DWORD               dwAddressID,
					 HDRVCALL            hdCall,
					 DWORD               dwSelect,
					 DWORD               dwTerminalModes,
					 DWORD               dwTerminalID,
					 DWORD               bEnable
					 )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetTerminal")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetupConference(
						 DRV_REQUESTID       dwRequestID,
						 HDRVCALL            hdCall,
						 HDRVLINE            hdLine,
						 HTAPICALL           htConfCall,
						 LPHDRVCALL          lphdConfCall,
						 HTAPICALL           htConsultCall,
						 LPHDRVCALL          lphdConsultCall,
						 DWORD               dwNumParties,
						 LPLINECALLPARAMS    const lpCallParams
						 )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetupConference")
		DWORD_IN_ENTRY(dwNumParties)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetupTransfer(
					   DRV_REQUESTID       dwRequestID,
					   HDRVCALL            hdCall,
					   HTAPICALL           htConsultCall,
					   LPHDRVCALL          lphdConsultCall,
					   LPLINECALLPARAMS    const lpCallParams
					   )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetupTransfer")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSwapHold(
				  DRV_REQUESTID       dwRequestID,
				  HDRVCALL            hdActiveCall,
				  HDRVCALL            hdHeldCall
				  )
{
	BEGIN_PARAM_TABLE("TSPI_lineSwapHold")
		DWORD_IN_ENTRY(dwRequestID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineUncompleteCall(
						DRV_REQUESTID       dwRequestID,
						HDRVLINE            hdLine,
						DWORD               dwCompletionID
						)
{
	BEGIN_PARAM_TABLE("TSPI_lineUncompleteCall")
		DWORD_IN_ENTRY(dwCompletionID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineUnhold(
				DRV_REQUESTID       dwRequestID,
				HDRVCALL            hdCall
				)
{
	BEGIN_PARAM_TABLE("TSPI_lineUnhold")
		DWORD_IN_ENTRY(hdCall)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineUnpark(
				DRV_REQUESTID       dwRequestID,
				HDRVLINE            hdLine,
				DWORD               dwAddressID,
				HTAPICALL           htCall,
				LPHDRVCALL          lphdCall,
				LPCWSTR             lpszDestAddress
				)
{
	BEGIN_PARAM_TABLE("TSPI_lineUnpark")
		DWORD_IN_ENTRY(dwAddressID)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_phoneClose(
				HDRVPHONE           hdPhone
				)
{
	BEGIN_PARAM_TABLE("TSPI_phoneClose")
		DWORD_IN_ENTRY(hdPhone)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneDevSpecific(
					  DRV_REQUESTID       dwRequestID,
					  HDRVPHONE           hdPhone,
					  LPVOID              lpParams,
					  DWORD               dwSize
					  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneDevSpecific")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetButtonInfo(
						HDRVPHONE           hdPhone,
						DWORD               dwButtonLampID,
						LPPHONEBUTTONINFO   lpButtonInfo
						)
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetButtonInfo")
		DWORD_IN_ENTRY(dwButtonLampID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetData(
				  HDRVPHONE           hdPhone,
				  DWORD               dwDataID,
				  LPVOID              lpData,
				  DWORD               dwSize
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetData")
		DWORD_IN_ENTRY(dwDataID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetDevCaps(
					 DWORD               dwDeviceID,
					 DWORD               dwTSPIVersion,
					 DWORD               dwExtVersion,
					 LPPHONECAPS         lpPhoneCaps
					 )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetDevCaps")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetDisplay(
					 HDRVPHONE           hdPhone,
					 LPVARSTRING         lpDisplay
					 )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetDisplay")
		DWORD_IN_ENTRY(hdPhone)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetExtensionID(
						 DWORD               dwDeviceID,
						 DWORD               dwTSPIVersion,
						 LPPHONEEXTENSIONID  lpExtensionID
						 )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetExtensionID")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetGain(
				  HDRVPHONE           hdPhone,
				  DWORD               dwHookSwitchDev,
				  LPDWORD             lpdwGain
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetGain")
		DWORD_IN_ENTRY(hdPhone)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetHookSwitch(
						HDRVPHONE           hdPhone,
						LPDWORD             lpdwHookSwitchDevs
						)
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetHookSwitch")
		DWORD_IN_ENTRY(lpdwHookSwitchDevs)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_phoneGetIcon(
				  DWORD               dwDeviceID,
				  LPCWSTR             lpszDeviceClass,
				  LPHICON             lphIcon
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetIcon")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_phoneGetID(
				HDRVPHONE           hdPhone,
				LPVARSTRING         lpDeviceID,
				LPCWSTR             lpszDeviceClass,
				HANDLE              hTargetProcess                          // TSPI v2.0
				)
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetID")
		DWORD_IN_ENTRY(hdPhone)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetLamp(
				  HDRVPHONE           hdPhone,
				  DWORD               dwButtonLampID,
				  LPDWORD             lpdwLampMode
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetLamp")
		DWORD_IN_ENTRY(dwButtonLampID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetRing(
				  HDRVPHONE           hdPhone,
				  LPDWORD             lpdwRingMode,
				  LPDWORD             lpdwVolume
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetRing")
		DWORD_IN_ENTRY(lpdwRingMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetStatus(
					HDRVPHONE           hdPhone,
					LPPHONESTATUS       lpPhoneStatus
					)
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetStatus")
		DWORD_IN_ENTRY(hdPhone)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetVolume(
					HDRVPHONE           hdPhone,
					DWORD               dwHookSwitchDev,
					LPDWORD             lpdwVolume
					)
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetVolume")
		DWORD_IN_ENTRY(dwHookSwitchDev)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneNegotiateExtVersion(
							  DWORD               dwDeviceID,
							  DWORD               dwTSPIVersion,
							  DWORD               dwLowVersion,
							  DWORD               dwHighVersion,
							  LPDWORD             lpdwExtVersion
							  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneNegotiateExtVersion")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneNegotiateTSPIVersion(
							   DWORD               dwDeviceID,
							   DWORD               dwLowVersion,
							   DWORD               dwHighVersion,
							   LPDWORD             lpdwTSPIVersion
							   )
{
	BEGIN_PARAM_TABLE("TSPI_phoneNegotiateTSPIVersion")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneOpen(
			   DWORD               dwDeviceID,
			   HTAPIPHONE          htPhone,
			   LPHDRVPHONE         lphdPhone,
			   DWORD               dwTSPIVersion,
			   PHONEEVENT          lpfnEventProc
			   )
{
	BEGIN_PARAM_TABLE("TSPI_phoneOpen")
		DWORD_IN_ENTRY(dwTSPIVersion)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSelectExtVersion(
						   HDRVPHONE           hdPhone,
						   DWORD               dwExtVersion
						   )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSelectExtVersion")
		DWORD_IN_ENTRY(dwExtVersion)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetButtonInfo(
						DRV_REQUESTID       dwRequestID,
						HDRVPHONE           hdPhone,
						DWORD               dwButtonLampID,
						LPPHONEBUTTONINFO   const lpButtonInfo
						)
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetButtonInfo")
		DWORD_IN_ENTRY(dwButtonLampID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetData(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwDataID,
				  LPVOID              const lpData,
				  DWORD               dwSize
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetData")
		DWORD_IN_ENTRY(dwDataID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TSPI_phoneSetDisplay(
					 DRV_REQUESTID       dwRequestID,
					 HDRVPHONE           hdPhone,
					 DWORD               dwRow,
					 DWORD               dwColumn,
					 LPCWSTR             lpsDisplay,
					 DWORD               dwSize
					 )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetDisplay")
		DWORD_IN_ENTRY(dwRow)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetGain(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwHookSwitchDev,
				  DWORD               dwGain
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetGain")
		DWORD_IN_ENTRY(dwHookSwitchDev)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetHookSwitch(
						DRV_REQUESTID       dwRequestID,
						HDRVPHONE           hdPhone,
						DWORD               dwHookSwitchDevs,
						DWORD               dwHookSwitchMode
						)
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetHookSwitch")
		DWORD_IN_ENTRY(dwHookSwitchDevs)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetLamp(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwButtonLampID,
				  DWORD               dwLampMode
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetLamp")
		DWORD_IN_ENTRY(dwButtonLampID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetRing(
				  DRV_REQUESTID       dwRequestID,
				  HDRVPHONE           hdPhone,
				  DWORD               dwRingMode,
				  DWORD               dwVolume
				  )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetRing")
		DWORD_IN_ENTRY(dwRingMode)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetStatusMessages(
							HDRVPHONE           hdPhone,
							DWORD               dwPhoneStates,
							DWORD               dwButtonModes,
							DWORD               dwButtonStates
							)
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetStatusMessages")
		DWORD_IN_ENTRY(dwPhoneStates)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetVolume(
					DRV_REQUESTID       dwRequestID,
					HDRVPHONE           hdPhone,
					DWORD               dwHookSwitchDev,
					DWORD               dwVolume
					)
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetVolume")
		DWORD_IN_ENTRY(dwHookSwitchDev)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_providerConfig(
					HWND                hwndOwner,
					DWORD               dwPermanentProviderID
					)
{   //Tapi version 1.4 and earlier - now can be ignored.
	BEGIN_PARAM_TABLE("TSPI_providerConfig")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerCreateLineDevice(                                  // TSPI v1.4
							  DWORD_PTR           dwTempID,
							  DWORD               dwDeviceID
							  )
{
	BEGIN_PARAM_TABLE("TSPI_providerCreateLineDevice")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerCreatePhoneDevice(                                 // TSPI v1.4
							   DWORD_PTR           dwTempID,
							   DWORD               dwDeviceID
							   )
{
	BEGIN_PARAM_TABLE("TSPI_providerCreatePhoneDevice")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}



LONG
TSPIAPI
TSPI_providerFreeDialogInstance(                                // TSPI v2.0
								HDRVDIALOGINSTANCE  hdDlgInst
								)
{
	BEGIN_PARAM_TABLE("TSPI_providerFreeDialogInstance")
		DWORD_IN_ENTRY(hdDlgInst)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TUISPI_lineConfigDialogEdit(                                    // TSPI v2.0
							TUISPIDLLCALLBACK   lpfnUIDLLCallback,
							DWORD               dwDeviceID,
							HWND                hwndOwner,
							LPCWSTR             lpszDeviceClass,
							LPVOID              const lpDeviceConfigIn,
							DWORD               dwSize,
							LPVARSTRING         lpDeviceConfigOut
							)
{
	BEGIN_PARAM_TABLE("TUISPI_lineConfigDialogEdit")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TUISPI_phoneConfigDialog(                                       // TSPI v2.0
						 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
						 DWORD               dwDeviceID,
						 HWND                hwndOwner,
						 LPCWSTR             lpszDeviceClass
						 )
{
	BEGIN_PARAM_TABLE("TUISPI_phoneConfigDialog")
		DWORD_IN_ENTRY(dwDeviceID)
		END_PARAM_TABLE()

		return EPILOG(0);
}


LONG
TSPIAPI
TUISPI_providerGenericDialog(                                   // TSPI v2.0
							 TUISPIDLLCALLBACK   lpfnUIDLLCallback,
							 HTAPIDIALOGINSTANCE htDlgInst,
							 LPVOID              lpParams,
							 DWORD               dwSize,
							 HANDLE              hEvent
							 )
{
	BEGIN_PARAM_TABLE("TUISPI_providerGenericDialog")
		DWORD_IN_ENTRY(dwSize)
		END_PARAM_TABLE()

		return EPILOG(0);
}

LONG
TSPIAPI
TUISPI_providerGenericDialogData(                               // TSPI v2.0
								 HTAPIDIALOGINSTANCE htDlgInst,
								 LPVOID              lpParams,
								 DWORD               dwSize
								 )
{
	BEGIN_PARAM_TABLE("TUISPI_providerGenericDialogData")
		DWORD_IN_ENTRY(lpParams)
		END_PARAM_TABLE()

		return EPILOG(0);
}




