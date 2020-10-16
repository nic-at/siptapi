/*
  Name: asttapi.cpp
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

// WaveTsp.h

#ifndef WAVETSP_H
#define WAVETSP_H

#include <eXosip2/eXosip.h>

#define DIM(rg) (sizeof(rg)/sizeof(*rg))

// since eXosip 4.0 the context must be specified
// defined in SipStack.cpp
extern eXosip_t *ctx;

//if not defined, Alert-Info will be set for Polycom
#define AUTOANSWER_ASTRA

//kd: constants for the number of lines and number of phones
#ifdef MULTILINE
	#define SIPTAPI_NUMLINES 40
#else
	#define SIPTAPI_NUMLINES 1
#endif

#define SIPTAPI_NUMPHONES 1 

#define SIPTAPI_VERSION_SHORT "v0.3.23"
#ifdef MULTILINE
#ifdef _DEBUG
#define SIPTAPI_VERSION_W L"v0.3.23 Multiline Debug"
#define SIPTAPI_VERSION    "v0.3.23 Multiline Debug"
#else
#define SIPTAPI_VERSION_W L"v0.3.23 Multiline Release"
#define SIPTAPI_VERSION    "v0.3.23 Multiline Release"
#endif
#else
#ifdef _DEBUG
#define SIPTAPI_VERSION_W L"v0.3.23 Singleline Debug"
#define SIPTAPI_VERSION    "v0.3.23 Singleline Debug"
#else
#define SIPTAPI_VERSION_W L"v0.3.23 Singleline Release"
#define SIPTAPI_VERSION    "v0.3.23 Singleline Release"
#endif
#endif


// Data sent back and forth between UI and TSP during a dial operation
class CtspCall;

struct TSPUIDATA
{
    CtspCall*   pCall;          // Call operation is happening on
    DWORD       dwRequestID;    // Asynch. operation ID
    LONG        tr;             // Asynch, operation result
    DWORD       nCallState;     // New call state
};

//#define DEBUGTSP
#if defined(_DEBUG) && defined(DEBUGTSP)

enum ParamType
{
    ptDword,
    ptString,
};

enum ParamDirection
{
    dirIn,
    dirOut,
};

struct FUNC_PARAM
{
    const char*     pszVal;
    DWORD           dwVal;
    ParamType       pt;
    ParamDirection  dir;
};

struct FUNC_INFO
{
    const char* pszFuncName;
    DWORD       dwNumParams;
    FUNC_PARAM* rgParams;
    LONG        lResult;
};

void Prolog(FUNC_INFO* pInfo);
LONG Epilog(FUNC_INFO* pInfo, LONG lResult);

#define BEGIN_PARAM_TABLE(fname) \
    const char* __pszFname = fname; \
    FUNC_PARAM __params[] = {

#define DWORD_IN_ENTRY(p) { #p, (DWORD)p, ptDword, dirIn },
#define DWORD_OUT_ENTRY(p) { #p, (DWORD)p, ptDword, dirOut },
#define STRING_IN_ENTRY(p) { #p, (DWORD)p, ptString, dirIn },
#define STRING_OUT_ENTRY(p) { #p, (DWORD)p, ptString, dirOut },

#define END_PARAM_TABLE() \
    }; FUNC_INFO __info = { __pszFname, DIM(__params), __params }; \
    Prolog(&__info);

#define EPILOG(r) Epilog(&__info, r)

#define TSPTRACE TspTrace
#define TSPSIPTRACE TspSipTrace
void TspTrace(LPCTSTR pszFormat, ...);
void TspSipTrace(LPCSTR pszFormat, osip_message_t *msg);

#else   // !_DEBUG

#define EPILOG(r) r

#define BEGIN_PARAM_TABLE(fname)
#define DWORD_IN_ENTRY(p)
#define DWORD_OUT_ENTRY(p)
#define STRING_IN_ENTRY(p)
#define STRING_OUT_ENTRY(p)
#define END_PARAM_TABLE()

#define TSPTRACE TspTrace
#define TSPSIPTRACE TspSipTrace
inline void TspTrace(LPCTSTR pszFormat, ...) {};
inline void TspSipTrace(LPCSTR pszFormat, osip_message_t *msg) {};

#endif  // _DEBUG

#endif  // WAVETSP_H