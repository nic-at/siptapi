/*
  Name: Utilities.cpp
  License: Under the GNU General Public License Version 2 or later (the "GPL")
  (c) Nick Knight
  (c) Klaus Darilion (IPCom GmbH, www.ipcom.at)

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


#define STRICT

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <tapi.h>
#include <tspi.h>
#include <strsafe.h>
#include "wavetsp.h"
#include <string>

#include <eXosip2/eXosip.h>

#include "utilities.h"

//Utilities for the windows registry, so we can save out and read in the settings we require
//const char RegKey[] = "Software\\IPCom\\SIPTAPI";
//const char RegKey[] = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\SIPTAPI";
const char SubKeyName[]     = "SIPTAPI";
const char SubKeyRoot[]     = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony";
const char SubKeyComplete[] = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\SIPTAPI";
const HKEY WHICHKEY = HKEY_LOCAL_MACHINE;

Mutex outputMutex;

//Called once to make sure everything is in place.
bool initConfigStore(void)
{
	HKEY ourKey;
	DWORD result;

	TspTrace("initConfigStore: creating Registry Path HKEY_LOCAL_MACHINE\\%s  ...", SubKeyComplete);

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, SubKeyRoot, 0, KEY_ALL_ACCESS , &ourKey) )
	{
		HKEY newKey;
		if ( ERROR_SUCCESS != RegCreateKeyEx(
			ourKey,
			SubKeyName,
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&newKey,
			&result
			)) {
				TspTrace("initConfigStore: Failed to create key HKEY_LOCAL_MACHINE\\%s",SubKeyComplete);
		}

		RegCloseKey(newKey);
		RegCloseKey(ourKey);

		return true;
	}
	return false;
}

bool storeConfigString(std::string item, std::string str)
{
	HKEY ourKey;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, 
		SubKeyComplete, 
		0,
		KEY_ALL_ACCESS /* for just reading we would use KEY_READ*/, 
		&ourKey) )
	{
		//at this point we should have our key opened.
		if ( ERROR_SUCCESS != RegSetValueEx(ourKey,
			item.c_str(), 
			0, 
			REG_SZ,
			(const BYTE *)str.c_str(),
			str.size()) )
		{
			//MessageBox(0, "Failed set value", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Failed to set registry value");
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "Failed to open key: ";
		msg += SubKeyComplete;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TSPTRACE(msg.c_str());
		return false;
	}
	return true;
}

bool readConfigString(std::string item, std::string &str)
{
	HKEY ourKey;
	char tempStr[100];
	DWORD type;
	DWORD length = sizeof(tempStr);

	str = "";

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, SubKeyComplete, 0,KEY_READ, &ourKey) )
	{
		//at this point we should have our key opened.
		
		if ( ERROR_SUCCESS != RegQueryValueEx(ourKey,item.c_str(), 0, &type,(BYTE*)&tempStr[0],&length ) )
		{
			//MessageBox(0, "Failed to read value", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Failed to read value");
			return false;
		}
		else if ( REG_SZ != type )
		{
			//MessageBox(0, "unexpected data type", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Unexpected data type");
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "Failed to open key: ";
		msg += SubKeyComplete;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TSPTRACE(msg.c_str());
		return false;
	}
	//ensure it is terminated
	tempStr[length] = 0;
	str = tempStr;
	return true;
}


bool storeConfigInt(std::string item, DWORD value)
{
	HKEY ourKey;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, SubKeyComplete, 0,KEY_ALL_ACCESS /* for just reading we would use KEY_READ*/, &ourKey) )
	{
		//at this point we should have our key opened.
		if ( ERROR_SUCCESS != RegSetValueEx(ourKey,item.c_str(), 0, REG_DWORD,(const BYTE *)&value,sizeof(DWORD)) )
		{
			//MessageBox(0, "Failed set value", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Failed to set value");
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "Failed to open key: ";
		msg += SubKeyComplete;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TSPTRACE("Failed to open key");
		return false;
	}
	return true;
}

bool readConfigInt(std::string item, DWORD &value)
{
	HKEY ourKey;
	char tempStr[100];
	DWORD type;
	DWORD length = sizeof(tempStr);

	value = 0;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, SubKeyComplete, 0,KEY_READ, &ourKey) )
	{
		//at this point we should have our key opened.
		
		if ( ERROR_SUCCESS != RegQueryValueEx(ourKey,item.c_str(), 0, &type,(BYTE*)&value,&length ) )
		{
			//MessageBox(0, "Failed to read value", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Failed to read value");
			return false;
		}
		else if ( REG_DWORD != type )
		{
			MessageBox(0, "Unexpected data type", "Windows Registry error", MB_SETFOREGROUND);
			TSPTRACE("Unexpected data type");
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "Failed to open key: ";
		msg += SubKeyComplete;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TSPTRACE(msg.c_str());
		return false;
	}
	return true;

}

#if defined(_DEBUG) && defined(DEBUGTSP)

static
void DumpParams(FUNC_INFO* pInfo, ParamDirection dir)
{
	TSPTRACE("  %s parameters:\n", (dir == dirIn ? "in" : "out"));

	FUNC_PARAM* begin = &pInfo->rgParams[0];
	FUNC_PARAM* end = &pInfo->rgParams[pInfo->dwNumParams];

	for( FUNC_PARAM* pParam = begin; pParam != end; ++pParam ) {
		if( pParam->dir == dir ) {
			switch( pParam->pt ) {

			case ptString: {
				//char sz[MAX_PATH+1] = "<NULL>";
				//if( pParam->dwVal ) {
				//	/* NOTE TODO KLAUS This crashes on Unicode strings, thus we currently
				//	 * do not log strings
				//	 */
				//	wcstombs(sz, (const wchar_t*)pParam->dwVal, MAX_PATH);
				//	sz[MAX_PATH] = 0;
				//}
				//TSPTRACE("    %s= 0x%lx '%s'\n", pParam->pszVal, pParam->dwVal, sz);
				TSPTRACE("    %s= (logging disabled due to problems)\n", pParam->pszVal);
				break;
			}

			case ptDword:
			default:
				if( dir == dirIn ) {
					TSPTRACE("    %s= 0x%lx\n", pParam->pszVal, pParam->dwVal);
				} else {
					TSPTRACE("    %s= 0x%lx\n", pParam->pszVal, pParam->dwVal);
					//                    TSPTRACE("    %s= 0x%lx '0x%lx'\n", pParam->pszVal, pParam->dwVal, (pParam->dwVal ? *(DWORD*)pParam->dwVal : 0));
				}
				break;
			}
		}
	}
}

void Prolog(FUNC_INFO* pInfo)
{
    char    sz[MAX_PATH+1];
    GetModuleFileName(0, sz, MAX_PATH);

    TSPTRACE("%s() from %s\n", pInfo->pszFuncName, sz);
    DumpParams(pInfo, dirIn);
}

LONG Epilog(FUNC_INFO* pInfo, LONG tr)
{

    DumpParams(pInfo, dirOut);
    TSPTRACE("%s() returning 0x%x\n\n", pInfo->pszFuncName, tr);
    return tr;
}


void TspTrace(LPCSTR pszFormat, ...)
{
#define DEBUGBUFLEN 4096
	char    buf[DEBUGBUFLEN] = "SIPTAPI_0.3: ";

    va_list ap;
    
	outputMutex.Lock();

	va_start(ap, pszFormat);
//    wvsprintf(buf, pszFormat, ap);
	StringCbVPrintf(buf + strlen(buf), DEBUGBUFLEN-strlen(buf), pszFormat, ap);
    OutputDebugString(buf);

#ifdef DEBUGTSPTOFILE
    HANDLE  hFile = CreateFile("c:\\siptapi.log", GENERIC_WRITE,
                               0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
	    SetFilePointer(hFile, 0, 0, FILE_END);
	    //DWORD   err = GetLastError();
	    DWORD   nBytes;
	    WriteFile(hFile, buf, strlen(buf), &nBytes, 0);
	    CloseHandle(hFile);
	}
#endif

    va_end(ap);

	outputMutex.Unlock();

}

void TspSipTrace(LPCSTR pszFormat, osip_message_t *msg)
{
	char *dest=NULL;
	size_t length=0;
	int i;

	i = osip_message_to_str(msg, &dest, &length);
	if (i!=0) {
		TspTrace("TspSipTrace: cannot get printable message\n");
	} else {
		TspTrace("TspSipTrace: %s: \n", pszFormat);
		TspTrace("%s\n", dest);
		osip_free(dest);
	}
}
//void TspSipTrace(LPCSTR pszFormat, osip_message_t *msg)
//{
//	char *dest=NULL;
//	size_t length=0;
//	int i;
//
//	i = osip_message_to_str(msg, &dest, &length);
//	if (i!=0) {
//		TspTrace("TspSipTrace: cannot get printable message\n");
//	} else {
//		TspTrace("TspSipTrace: %s: %s\n", pszFormat, dest);
//		osip_free(dest);
//	}
//}
#endif  // _DEBUG
