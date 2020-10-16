// (c): Klaus Darilion (IPCom GmbH, www.ipcom.at)
// License: GNU General Public License 2.0

//#include <WinDNS.h>
#include <windows.h>
#include <windns.h>

#include <eXosip2/eXosip.h>
#include ".\tapiline.h"
#include "WaveTsp.h"

//Function call for asyncrounous functions
ASYNC_COMPLETION g_pfnCompletionProc = 0;

TapiLine::TapiLine(void)
{
	this->initialized = false;
	this->htLine = 0;
	this->hdLine = 0;
	this->htCall = 0;
	this->hdCall = 0;
	this->status = TapiLine::IDLE;
	this->reg_status = TapiLine::SIP_UNREGISTERED;
	this->cid = 0;
	this->did = 0;
	this->tid = 0;
	this->rid = 0;
	this->line = NULL;
	this->from = "";
	this->to = "";
	this->referto = "";
	this->markedforshutdown = false;
	this->isIncoming = false;
	this->tapiCallback = NULL;
	this->deviceId = 0; // bad because 0 is a valid deviceId - but do not know how to solve this
	this->shutdownRequestId = 0;
	this->transferRequestId = 0;
	this->redirectRequestId = 0;
	this->incomingFromDisplayName = NULL;
	this->incomingFromUriUser = NULL;
	this->incomingToDisplayName = NULL;
	this->incomingToUriUser = NULL;
	this->enableRealmCheck = 0;
	this->transportProtocol = 0; //UDP
	this->send180Ringing = 0;
	this->lastReportedLineEvent = LINECALLSTATE_UNKNOWN;
}

TapiLine::~TapiLine(void)
{
	if (this->line) {
		TSPTRACE("TapiLine::~TapiLine: freeing old line parameter");
		free(this->line);
	}
}

// set the SIP proxy
void TapiLine::setProxy(std::string strdata)
{
	this->proxy=strdata;
}

// set the SIP outbound proxy
void TapiLine::setObProxy(std::string strdata)
{
	this->obproxy=strdata;
}

// set the SIP AoR username and authentication user name if no authusername is supplied
void TapiLine::setUsername(std::string strdata)
{
	this->username=strdata;
}

// get the SIP AoR username
const char * TapiLine::getUsername()
{
	return this->username.c_str();
}

// set the authentication user name
void TapiLine::setAuthusername(std::string strdata)
{
	this->authusername=strdata;
}

// set the phhone's (the party which gets called to get redirected) user name
void TapiLine::setPhoneusername(std::string strdata)
{
	this->phoneusername=strdata;
}

// set the SIP authentication password
void TapiLine::setPassword(std::string strdata)
{
	this->password=strdata;
}

// set auto-answer
void TapiLine::setAutoanswer(int temp)
{
	this->autoanswer=temp;
}

// set if realmcheck is disabled
void TapiLine::setEnableRealmCheck(int disable)
{
	this->enableRealmCheck = disable;
}

// transport protocol
void TapiLine::setTransportProtocol(int protocol)
{
	this->transportProtocol = protocol;
}

// transport protocol
int TapiLine::getTransportProtocol(void)
{
	return this->transportProtocol;
}

// send 180 ringing
void TapiLine::setSend180Ringing(int ringing)
{
	this->send180Ringing = ringing;
}

// send 180 ringing
int TapiLine::getSend180Ringing(void)
{
	return this->send180Ringing;
}

// get/set reverse-mode
void TapiLine::setReverseMode(int mode)
{
	this->reverseMode = mode;
}
int TapiLine::getReverseMode()
{
	return this->reverseMode;
}

// initialize the Line, sets the username, password, proxy... for the SIP call
int TapiLine::initialize(void)
{
	int i;

	// add authentication info for this line
	//i = eXosip_clear_authentication_info(); // do not clear as this clears alos authentication info of other lines
	if (this->authusername == "") {
		this->authusername = this->username;
		TSPTRACE("TapiLine::initialize(): authusername = %s\n",this->authusername.c_str());
	}

	if (this->enableRealmCheck ) {
		TSPTRACE("TapiLine::initialize(): adding credentials with realm\n");
		i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->authusername.c_str(), 
			this->password.c_str(), "", this->proxy.c_str());
	} else {
		TSPTRACE("TapiLine::initialize(): adding credentials without realm\n");
		i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->authusername.c_str(), 
			this->password.c_str(), "", 0);
	}

	this->initialized = true;
	return 0;
}

bool TapiLine::isInitialized(void)
{
	return this->initialized;
}

//REGISTER to the SIP proxy
int TapiLine::register_to_sipproxy()
{
	int rid, i;
	std::string from_uri, registrar_uri, route_set;
	osip_message_t *request;

	TSPTRACE("TapiLine::register_to_sipproxy(): ...\n");

	//i = eXosip_clear_authentication_info(); // ToDo: this may influence concurrent calls and REGISTERs
	//if (this->authusername == "") {
	//	// use SIP AoR username as authentication username
	//	i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->username.c_str(), 
	//		this->password.c_str(), "", "");
	//} else {
	//	i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->authusername.c_str(), 
	//		this->password.c_str(), "", "");
	//}

	from_uri =      ("sip:")  + this->username  + ("@") +  this->proxy;
	registrar_uri = ("sip:")  + this->proxy;

	TSPTRACE("TapiLine::register_to_sipproxy(): from_uri.c_str()=%s...\n",from_uri.c_str());
	TSPTRACE("TapiLine::register_to_sipproxy(): registrar_uri.c_str()=%s...\n",registrar_uri.c_str());
	
	rid = eXosip_register_build_initial_register(ctx, from_uri.c_str(), registrar_uri.c_str(), NULL, 300, &request)  	;

	if (rid < 0)
	{
		TSPTRACE("TapiLine::register_to_sipproxy: eXosip_register_build_initial_register failed: (bad arguments?)\n");
		this->reg_status = SIP_UNREGISTERED;
		return -1;
	}
	TSPTRACE("TapiLine::register_to_sipproxy: eXosip_register_build_initial_register succeeded\n");

	if (this->obproxy != "") {
		// add outboundproxy as route set

		// check if the obproxy contains the lr parameter
		if (this->obproxy.find(";lr") == std::string::npos) {
			// not found
			route_set = ("<sip:") + this->obproxy  + (";lr>");
		} else {
			// found
			route_set = ("<sip:") + this->obproxy  + (">");
		}
		char *header = osip_strdup ("route");
		char *route  = osip_strdup (route_set.c_str());
		osip_message_set_multiple_header (request, header, route);
		osip_free (route);
		osip_free (header);
	}

	TspSipTrace("TapiLine::register_to_sipproxy: sending message\n", request);
	eXosip_lock(ctx);
	this->reg_status = SIP_REGISTERING;
	i = eXosip_register_send_register(ctx, rid, request);

	if (i == -1) {
		TSPTRACE("TapiLine::register_to_sipproxy: eXosip_register_send_register failed: (bad arguments?)\n");
		this->reg_status = SIP_UNREGISTERED;
		eXosip_unlock(ctx);
		return -1;
	}
	TSPTRACE("TapiLine::register_to_sipproxy: eXosip_register_send_register returned: %d\n",i);
	this->rid = rid;
	eXosip_unlock(ctx);

	TSPTRACE("TapiLine::register_to_sipproxy: REGISTER sent ... further processing must be done in the thread\n");
	return rid;

}

//un-REGISTER from the SIP proxy
int TapiLine::unregister_from_sipproxy()
{
	int i;
	std::string from_uri, registrar_uri, route_set;
	osip_message_t *request;

	TSPTRACE("TapiLine::un-register_from_sipproxy(): unregister rid=%d...\n",this->rid);

	i = eXosip_register_build_register(ctx, this->rid, 0, &request);

	if (i < 0)
	{
		TSPTRACE("TapiLine::unregister_from_sipproxy: eXosip_register_build_register failed: (bad arguments?)\n");
		this->reg_status = SIP_UNREGISTERED;
		return -1;
	}
	TSPTRACE("TapiLine::unregister_from_sipproxy: eXosip_register_build_register succeeded\n");

	TspSipTrace("TapiLine::unregister_from_sipproxy: sending message\n", request);
	eXosip_lock(ctx);
	this->reg_status = SIP_DEREGISTERING;
	i = eXosip_register_send_register(ctx, this->rid, request);
	if (i == -1) {
		TSPTRACE("TapiLine::unregister_from_sipproxy: eXosip_register_send_register failed: (bad arguments?)\n");
		this->reg_status = SIP_UNREGISTERED;
		eXosip_unlock(ctx);
		return -1;
	}
	TSPTRACE("TapiLine::unregister_from_sipproxy: eXosip_register_send_register returned: %d\n",i);
	eXosip_unlock(ctx);

	TSPTRACE("TapiLine::unregister_from_sipproxy: REGISTER sent ... further processing must be done in the thread\n");
	return 0;
}

// store TAPI Line handle inside the TapiLine object
void TapiLine::setTapiLine(HTAPILINE lineHandle)
{
	this->htLine = lineHandle;
}

// retrieve TAPI Line handle from the TapiLine object
HTAPILINE TapiLine::getTapiLine()
{
	return (this->htLine);
}

// store the callback function used for reporting line status to TAPI
void TapiLine::setLineEventCallback(LINEEVENT callback)
{
	this->tapiCallback = callback;
}

// retrieve the callback function used for reporting line status to TAPI
LINEEVENT TapiLine::getLineEventCallback() {
	return this->tapiCallback;
}

// store the call handle (a handle for a certain call on this line)
// we only allow one call per line, thus this is a "singleton"
void TapiLine::setTapiCall(HTAPICALL call)
{
	this->htCall = call;
}

// make the call: initiate the click2dial feature
int TapiLine::makeCall(LPCWSTR pszDestAddress, DWORD dwCountryCode, DWORD dwRequestID)
{
	std::string destAddress;

	TSPTRACE("TapiLine::makeCall: ...\n");

	//if( mbrlen(pszDestAddress,100) <= 0 ) {
	//	//what type of length address is that!
	//	TSPTRACE("TapiLine::makeCall: number to long\n");
	//	return;
	//}
	//if( *pszDestAddress == L'T' || *pszDestAddress == L'P' ) {
	//	pszDestAddress++;
	//}

	// convert lpcwstr to string
	mbstate_t mbs;
	mbsinit(&mbs);
	char feld[202];
    wcsrtombs(feld, &pszDestAddress,100, &mbs);
	destAddress = std::string(feld);

//	i = eXosip_clear_authentication_info(); // ToDo: XXXX this may influence concurrent calls
//	i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->username.c_str(), this->password.c_str(), "", this->proxy.c_str());
//asterisk sends "asterisk" as realm, thus do not apply realm to allow interworking with asterisk
// update: in sip.conf this can bechanged with realm=..., thus we should be strict and set credentials for a realm
	//if (this->authusername == "") {
	//	// use SIP AoR username as authentication username
	//	i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->username.c_str(), 
	//		this->password.c_str(), "", "");
	//} else {
	//	i = eXosip_add_authentication_info(ctx, this->username.c_str(), this->authusername.c_str(), 
	//		this->password.c_str(), "", "");
	//}
	
	TSPTRACE("TapiLine::makeCall: removing spaces and special characters from phone number...\n");
	TSPTRACE("TapiLine::makeCall: original number: %s\n",destAddress.c_str());
	std::string::size_type pos;
	while (	(pos = destAddress.find_first_not_of("0123456789")) != std::string::npos) {
		destAddress.erase(pos,1);
		TSPTRACE("TapiLine::makeCall: bad digit found, new number is: %s\n",destAddress.c_str());
	}
	TSPTRACE("TapiLine::makeCall: trimmed number: %s\n",destAddress.c_str());

	from = ("sip:")  + this->username  + ("@") +  this->proxy;
	referto = ("sip:")  + destAddress     + ("@") +  this->proxy;
	// using openser it is possible to REGISTER multiple times and call yourself
	// this does not work with Asterisk. Thus, it is possible to
	// specify a "phone username" which gets called and gets refered
	if (this->phoneusername == "") {
		// use SIP AoR username as destination 
		to    = ("sip:")  + this->username     + ("@") +  this->proxy;
	} else {
		// use specified phone username as destination 
		if (this->phoneusername.find("@") == std::string::npos) {
			to = ("sip:") + this->phoneusername + ("@") +  this->proxy;
		} else {
			to = ("sip:") + this->phoneusername;
		}
	}

	if (this->reverseMode) {
		// reuse destAddress to change refertarget and calltarget
		destAddress = to;
		to = referto;
		referto = destAddress;
	}

	return this->sendInvite();
//	return this->sendOptions();
}

// retrieve the status of the state machine
TapiLine::StatusType TapiLine::getStatus(void)
{
	return this->status;
}

// set the status of the state machine
void TapiLine::setStatus(TapiLine::StatusType newStatus)
{
	this->status = newStatus;
}
	// retrieve the status of the state machine using the TAPI LINECALLSTATE_ Constants
DWORD TapiLine::getTapiStatus(void)
{
	TSPTRACE("TapiLine::getTapiStatus: ...\n");
	switch(this->status) {
		case TapiLine::IDLE:
			return LINECALLSTATE_IDLE;
		case TapiLine::RESERVED:
			return LINECALLSTATE_DIALTONE;
		case TapiLine::LEARNING:
			return LINECALLSTATE_DIALTONE;
		case TapiLine::CALLING:
			return LINECALLSTATE_DIALING;
		case TapiLine::CALLING_RINGING:
			return LINECALLSTATE_RINGBACK;
		case TapiLine::ESTABLISHED:
			LINECALLSTATE_CONNECTED;
		case TapiLine::ESTABLISHED_TRANSFERRING:
			return LINECALLSTATE_ONHOLDPENDTRANSFER;
		case TapiLine::ESTABLISHED_TRANSFER_ACCEPTED:
			return LINECALLSTATE_ONHOLDPENDTRANSFER;
		case TapiLine::ESTABLISHED_TRANSFERRED:
			return LINECALLSTATE_CONNECTED;
		case TapiLine::INCOMING:
			return LINECALLSTATE_OFFERING;
		case TapiLine::INCOMING_CANCELED:
			return LINECALLSTATE_IDLE;
	}
	TSPTRACE("TapiLine::getTapiStatus: ERROR: undefined call state\n");
	return LINECALLSTATE_UNKNOWN;
}

// mark the line to beeing shutdown by thread (async shutodwn tests for dialer.exe)
void TapiLine::markForShutdown(bool shut)
{
	this->markedforshutdown=shut;
}

// terminate all ongoing calls and set status to idle
int TapiLine::shutdown(DRV_REQUESTID dwRequestID)
{
	TSPTRACE("TapiLine::shutdown: ...\n");
	switch(this->status) {
		case TapiLine::IDLE:
		case TapiLine::RESERVED:
		case TapiLine::INCOMING_CANCELED:
			TSPTRACE("TapiLine::shutdown: line is idle, shutdown done\n");
			this->status = TapiLine::IDLE;
			// ASYNC_COMPLETE
			if (g_pfnCompletionProc) {
				TSPTRACE("TapiLine::shutdown: sending async_complete ...\n");
				g_pfnCompletionProc(dwRequestID,0);
			}
			this->reportStatus();
			return 0;
			break;
		default:
			if (!this->cid) {
				TSPTRACE("TapiLine::shutdown: ERROR: line not idle, but no cid...strange\n");
				this->status = TapiLine::IDLE;
				// ASYNC_COMPLETE
				if (g_pfnCompletionProc) {
					TSPTRACE("TapiLine::shutdown: sending async_complete ...\n");
					g_pfnCompletionProc(dwRequestID,0);
				}
				this->reportStatus();
				return 0;
			} else {
				if (this->shutdown_nowait() != 0) {
					if (g_pfnCompletionProc) {
						TSPTRACE("TapiLine::shutdown: sending async_complete after shutdown_nowait() failed ...\n");
						g_pfnCompletionProc(dwRequestID,0);
					}
					return 0;
				}
			}
	}

	return 1;

	//int i;
	//i=0; //counter
	//while (this->status != TapiLine::IDLE) {
	//	Sleep(50);
	//	i++;
	//	if (i > 20*40) {
	//		TSPTRACE("TapiLine::shutdown: line still not idle after 40 seconds, force shutdown...\n");
	//		this->status = TapiLine::IDLE;
	//		this->cid=0;
	//		this->did=0;
	//		this->reportStatus();
	//		break;
	//	}
	//}
	TSPTRACE("TapiLine::shutdown: ... done\n");
}

// return the eXosip internal call id for this TapiLine
int TapiLine::getCid(void)
{
	return this->cid;
}

// return the eXosip internal transaction id for this TapiLine
int TapiLine::getTid(void)
{
	return this->tid;
}

// return the eXosip internal registration id for this TapiLine
int TapiLine::getRid(void)
{
	return this->rid;
}
// return the eXosip internal registration line for this TapiLine
char *TapiLine::getLine(void)
{
	return this->line;
}

// return the eXosip internal dialog id for this TapiLine
int TapiLine::getDid(void)
{
	return this->did;
}

// handle the SIP events associated with this particular TapiLine
void TapiLine::handleEvent(eXosip_event_t *je)
{
	int i;

	TSPTRACE("TapiLine::handleEvent: ... \n");

	switch(this->status) {
		case IDLE:
			TSPTRACE("TapiLine::handleEvent: current status: IDLE\n");
			if (je->type == EXOSIP_CALL_INVITE) {
				// save ids
				this->cid = je->cid;
				this->did = je->did;
				this->tid = je->tid;
				// set new status
				this->status = INCOMING;
				this->isIncoming = true;
				// report call to TAPI and reuse hdLine as hdCall
				this->hdCall = (HDRVCALL) this->hdLine;

				// fetch Caller-ID: extract it from From header
				osip_from_t *from;
				char * temp;
				size_t l;
				from=osip_message_get_from (je->request);
				if (from) {
					temp = osip_from_get_displayname(from);
					if (temp) {
						l=strlen(temp); // excluding \0
						this->incomingFromDisplayName = (char *) malloc(l+1);
						if (this->incomingFromDisplayName) {
							strncpy(this->incomingFromDisplayName, osip_from_get_displayname(from), l+1);
							TSPTRACE("TapiLine::handleEvent: From display name = '%s'\n", this->incomingFromDisplayName);
						} else {
							TSPTRACE("TapiLine::handleEvent: Failed to allocate memory for From display name!\n");
						}
					}
					temp = osip_uri_get_username(osip_from_get_url(from));
					l=strlen(temp);
					this->incomingFromUriUser = (char *) malloc(l+1);
					if (this->incomingFromUriUser) {
						strncpy(this->incomingFromUriUser, osip_uri_get_username(osip_from_get_url(from)), l+1);
						TSPTRACE("TapiLine::handleEvent: From uri user = '%s'\n", this->incomingFromUriUser);
					} else {
						TSPTRACE("TapiLine::handleEvent: Failed to allocate memory for From uri user!\n");
					}
				}

				// fetch Called-ID: extract it from To header
				from=osip_message_get_to(je->request);
				if (from) {
					temp = osip_from_get_displayname(from);
					if (temp) {
						l=strlen(temp); // excluding \0
						this->incomingToDisplayName = (char *) malloc(l+1);
						if (this->incomingToDisplayName) {
							strncpy(this->incomingToDisplayName, osip_from_get_displayname(from), l+1);
							TSPTRACE("TapiLine::handleEvent: To display name = '%s'\n", this->incomingToDisplayName);
						} else {
							TSPTRACE("TapiLine::handleEvent: Failed to allocate memory for To display name!\n");
						}
					}
					temp = osip_uri_get_username(osip_from_get_url(from));
					l=strlen(temp);
					this->incomingToUriUser = (char *) malloc(l+1);
					if (this->incomingToUriUser) {
						strncpy(this->incomingToUriUser, osip_uri_get_username(osip_from_get_url(from)), l+1);
						TSPTRACE("TapiLine::handleEvent: To uri user = '%s'\n", this->incomingToUriUser);
					} else {
						TSPTRACE("TapiLine::handleEvent: Failed to allocate memory for To uri user!\n");
					}
				}

				// signal incoming call to TAPI
				TSPTRACE("TapiLine::handleEvent: new status: INCOMING\n");
				TSPTRACE("TapiLine::handleEvent: sending LINE_NEWCALL ...\n");
				TSPTRACE("TapiLine::handleEvent: htLine = %d\n",this->htLine);
				TSPTRACE("TapiLine::handleEvent: htCall = %d\n",this->htCall);
				TSPTRACE("TapiLine::handleEvent: hdCall = %d\n",this->hdCall);
				this->reportLineEvent( this->htLine, 
										0, 
										LINE_NEWCALL,  
										(DWORD_PTR)(HDRVCALL) this->hdCall,
										(DWORD_PTR)(LPHTAPICALL)&(this->htCall),
										0);
				TSPTRACE("TapiLine::handleEvent: sending LINE_NEWCALL ... done\n");
				TSPTRACE("TapiLine::handleEvent: htLine = %d\n",this->htLine);
				TSPTRACE("TapiLine::handleEvent: htCall = %d\n",this->htCall);
				TSPTRACE("TapiLine::handleEvent: hdCall = %d\n",this->hdCall);
				if (this->htCall == NULL) {
					TSPTRACE("TapiLine::handleEvent: ERROR on incoming call - did not get htCall, reject call...\n");
					int i;
					eXosip_lock(ctx);
					i=eXosip_call_send_answer(ctx, je->tid, 500, NULL);
					eXosip_unlock(ctx);
					if (i) {
						TSPTRACE("SipStack::processSipMessages: eXosip_call_send_answer(ctx, %d,500,NULL) failed\n", je->tid);
					} else {
						TSPTRACE("SipStack::processSipMessages: eXosip_call_send_answer(ctx, %d,500,NULL) succeeded\n", je->tid);
					}
				}			
			}
			break;
		case INCOMING:
			TSPTRACE("TapiLine::handleEvent: current status: INCOMING, je->type=%i\n", je->type);
			if (je->type == EXOSIP_CALL_CANCELLED) {
				TSPTRACE("TapiLine::handleEvent:INCOMING: EXOSIP_CALL_CANCELLED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=INCOMING_CANCELED;
				//support for Reason header:
				// if CANCEL request, check for Reason header
				if ( je->request ) {  
					if (strcmp(je->request->sip_method, "CANCEL")) {
						TSPTRACE("TapiLine::handleEvent: error, no CANCEL request, but %s received ...\n", je->request->sip_method);
					} else {
						// check for Reason header value
						int res;
						osip_header_t  *reason_header;
						TSPTRACE("TapiLine::handleEvent: CANCEL request ...\n");
						res = osip_message_header_get_byname(je->request, "reason", 0, &reason_header);
						if (reason_header) {
							if (strstr(reason_header->hvalue, "cause=200")) {
								TSPTRACE("TapiLine::handleEvent: incoming CANCEL request with Reason cause 200 ...\n");
								// call was answered elsewhere, signal answer and then go on
								// signaling IDLE as before
								TSPTRACE("TapiLine::handleEvent: sending LINECALLSTATE_CONNECTED ...\n");
								this->reportLineEvent( this->htLine, this->htCall, 
										LINE_CALLSTATE, LINECALLSTATE_CONNECTED, 0, 0);
							} else {
								TSPTRACE("TapiLine::handleEvent: CANCEL with Reason header, but cause!=200...\n");
							}
						} else {
							TSPTRACE("TapiLine::handleEvent: CANCEL without Reason header ...\n");
						}
					}
				}
				checkForShutdown("incoming call closed");
			}
			if (je->type == EXOSIP_CALL_CLOSED) {
				TSPTRACE("TapiLine::handleEvent:INCOMING: EXOSIP_CALL_CLOSED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=INCOMING_CANCELED;
				checkForShutdown("incoming call closed");
			}
			if (je->type == EXOSIP_CALL_RELEASED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("incoming call released");
				break;
			}
			break;
		case RESERVED:
			TSPTRACE("TapiLine::handleEvent: current status: RESERVED\n");
			break;
		case LEARNING:
			TSPTRACE("TapiLine::handleEvent: current status: LEARNING\n");
			break;
		case CALLING:
			TSPTRACE("TapiLine::handleEvent: current status: CALLING\n");
		case CALLING_RINGING:
			TSPTRACE("TapiLine::handleEvent: current status: CALLING_RINGING\n");
			if (je->type == EXOSIP_CALL_RELEASED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("ringing call released");
				break;
			}
			if (je->type == EXOSIP_CALL_RINGING) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RINGING received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->status=CALLING_RINGING;
				break;
			}
			if (je->type == EXOSIP_CALL_REQUESTFAILURE) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_REQUESTFAILURE received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				if ( (je->response != NULL) && (je->response->status_code == 487) ) {
					TSPTRACE("TapiLine::handleEvent:: Call cancelled 487 ...\n");
					this->cid=0;
					this->did=0;
					this->tid=0;
					this->status=IDLE;
				} else 	if ( (je->response != NULL) && (je->response->status_code == 403) ) {
					TSPTRACE("TapiLine::handleEvent:: Call forbidden 403 ...\n");
					this->cid=0;
					this->did=0;
					this->tid=0;
					this->status=IDLE;
				} else {
					TSPTRACE("TapiLine::handleEvent:: Call failed for unknown reason ...\n");
					this->cid=0;
					this->did=0;
					this->tid=0;
					this->status=IDLE;
				}
				checkForShutdown("ringing call requestfailure");
				break;
			}
			if (je->type == EXOSIP_CALL_SERVERFAILURE) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_SERVERFAILURE received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("ringing call serverfailure");
				break;
			}
			if (je->type == EXOSIP_CALL_GLOBALFAILURE) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_GLOBALFAILURE received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("ringing call globalfailure");
				break;
			}
			if (je->type == EXOSIP_CALL_ANSWERED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_ANSWERED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->did = je->did;
				osip_message_t *ack;
				eXosip_lock(ctx);
				i = eXosip_call_build_ack(ctx, je->did, &ack);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_build_ack failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_build_ack succeeded...\n");
				TspSipTrace("TapiLine::handleEvent: sending ACK\n",ack);
				i = eXosip_call_send_ack(ctx, je->did, ack);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_send_ack failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_send_ack succeeded...\n");
				eXosip_unlock(ctx);
				this->status=ESTABLISHED;
				this->reportStatus();

				// if we are in ACD mode we do not initiate the REFER automatically, but we
				// wait for the TAPI application to execute the blind transfer
				if (reverseMode == 1) {
					TSPTRACE("TapiLine::handleEvent: ACD mode is activated, wait for blind transfer ...\n");
					break;
				}

				// wait some time before sending the REFER reqeuest
				Sleep(50);
				osip_message_t *refer;
				eXosip_lock(ctx);
				TSPTRACE("TapiLine::handleEvent: building REFER ...\n");
				i = eXosip_call_build_refer(ctx, this->did, this->referto.c_str(), &refer);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_build_refer failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_build_refer succeeded...\n");
				i = osip_message_set_header(refer, "Referred-By", this->from.c_str());
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: osip_message_set_header failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: osip_message_set_header succeeded...\n");
				TspSipTrace("TapiLine::handleEvent: sending REFER\n",refer);
				i = eXosip_call_send_request(ctx, this->did, refer);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_send_request failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_send_request succeeded...\n");
				this->status = ESTABLISHED_TRANSFERRING;
				eXosip_unlock(ctx);
				TSPTRACE("TapiLine::handleEvent: sending REFER ... done\n");
			}
			break;
		case ESTABLISHED:
			TSPTRACE("TapiLine::handleEvent: current status: ESTABLISHED\n");
			if (je->type == EXOSIP_CALL_RELEASED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("established call released");
				break;
			}
			break;
		case ESTABLISHED_TRANSFERRING:
			TSPTRACE("TapiLine::handleEvent: current status: ESTABLISHED_TRANSFERRING\n");
			if (je->type == EXOSIP_CALL_RELEASED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForAsyncRefer("established_transferring call released", LINEERR_OPERATIONFAILED);
				checkForShutdown("established_transfering call released");
				break;
			}
			if (je->type == EXOSIP_CALL_MESSAGE_REQUESTFAILURE) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_REQUESTFAILURE received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				if ( (je->response != NULL) && (je->response->status_code == 407) ) {
					TSPTRACE("TapiLine::handleEvent:: 407 received, exosip should handle this...\n");
				} else 	if ( (je->response != NULL) && (je->response->status_code == 401) ) {
					TSPTRACE("TapiLine::handleEvent:: 401 received, exosip should handle this...\n");
				} else {
					TSPTRACE("TapiLine::handleEvent:: Call failed with status code %d, shuting down ...\n", je->response->status_code);
					checkForAsyncRefer("established_transferring call requestfailure", LINEERR_OPERATIONFAILED);
					this->shutdown_nowait();
				}
				checkForShutdown("established call requestfailure");
				break;
			}
			if (je->type == EXOSIP_CALL_MESSAGE_ANSWERED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_MESSAGE_ANSWERED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->status=ESTABLISHED_TRANSFER_ACCEPTED;
				checkForAsyncRefer("established_transferring call messageanswered", 0);
				break;
			}
			break;
		case ESTABLISHED_TRANSFER_ACCEPTED:
			TSPTRACE("TapiLine::handleEvent: current status: ESTABLISHED_TRANSFER_ACCEPTED\n");
			if (je->type == EXOSIP_CALL_RELEASED) {
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("established_transfer call released");
				break;
			}
			if (je->type == EXOSIP_CALL_MESSAGE_NEW) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_MESSAGE_NEW received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				// check if request is a NOTIFY
				if (0 != osip_strcasecmp (je->request->sip_method, "NOTIFY")) {
					TSPTRACE("TapiLine::handleEvent: non NOTIFY received, sending 403 ...\n");
					eXosip_lock(ctx);
					TSPTRACE("TapiLine::handleEvent: sending answer 403 ...\n");
					i = eXosip_call_send_answer(ctx, je->tid, 403, NULL);
					if (i != 0) {
						TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer failed...\n");
						eXosip_unlock(ctx);
						this->shutdown_nowait();
						break;
					}
					TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer succeeded...\n");
					eXosip_unlock(ctx);
					TSPTRACE("TapiLine::handleEvent: sending answer ... done\n");
					break;
				}
				// compare NOTIFY did with previous did
				if (je->did != this->did) {
					TSPTRACE("TapiLine::handleEvent: NOTIFY does not match the INVITE/REFER dialog, send 486\n");
					eXosip_lock(ctx);
					TSPTRACE("TapiLine::handleEvent: sending answer 481 ...\n");
					i = eXosip_call_send_answer(ctx, je->tid, 481, NULL);
					if (i != 0) {
						TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer failed...\n");
						eXosip_unlock(ctx);
						this->shutdown_nowait();
						break;
					}
					TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer succeeded...\n");
					eXosip_unlock(ctx);
					TSPTRACE("TapiLine::handleEvent: sending answer ... done\n");
					break;
				}
				//Respond to every NOTIFY with 200 OK
				osip_message_t *answer;
				eXosip_lock(ctx);
				TSPTRACE("TapiLine::handleEvent: building answer to NOTIFY ...\n");
				i = eXosip_call_build_answer(ctx, je->tid, 200, &answer);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_build_answer failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_build_answer succeeded...\n");
				i = eXosip_call_send_answer(ctx, je->tid, 200, answer);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: eXosip_call_send_answer succeeded...\n");
				TSPTRACE("TapiLine::handleEvent: sending answer ... done\n");
				// check sipfrag response code, >=200 is final response which will cause a BYE
				osip_body_t *body = NULL;
				i = osip_message_get_body(je->request, 0, &body);
				if (i != 0) {
					TSPTRACE("TapiLine::handleEvent: osip_message_get_body failed...\n");
					eXosip_unlock(ctx);
					this->shutdown_nowait();
					break;
				}
				TSPTRACE("TapiLine::handleEvent: osip_message_get_body succeeded...\n");
				if (body == NULL) {
					TSPTRACE("TapiLine::handleEvent: no body in NOTIFY ... ignore this NOTIFY and wait for next NOTIFY\n");
					eXosip_unlock(ctx);
					break;
				}
				TSPTRACE("TapiLine::handleEvent: bodylen=%d, body is: %.*s\n", body->length, body->length, body->body);
				if ( !strncmp(body->body, "SIP/2.0 1", min(body->length, 9)) ) {
					TSPTRACE("TapiLine::handleEvent: provisional response in NOTIFY body ... ignore this NOTIFY and wait for next NOTIFY\n");
					eXosip_unlock(ctx);
					break;
				}
				TSPTRACE("TapiLine::handleEvent: final response (or garbage) in NOTIFY body ... terminate call\n");

				this->status = TapiLine::ESTABLISHED_TRANSFERRED;
				eXosip_unlock(ctx);
				this->reportStatus();

				//sending BYE request
				TSPTRACE("TapiLine::handleEvent: sending BYE (shutting down)\n");
				this->shutdown_nowait();

				// we do not wait for CALL_RELEASED from eXosip as this takes 20 seconds. Thus we delete the cid/did 
				// and then the CALL_RELEASED event will be ignored in TapiLine. We immediately report idle.
				TSPTRACE("TapiLine::handleEvent: eXosip_call_terminate succeeded, set status to idle ...\n");
				this->status = TapiLine::IDLE;
				this->cid=0;
				this->did=0;
				this->tid=0;
				// XXXX: not sure if this is really needed.
				asyncCompletion("call transfered");
				this->reportStatus();

				//TSPTRACE("TapiLine::handleEvent: sending BYE\n");
				//eXosip_lock;
				//i = eXosip_call_terminate(ctx, je->cid, je->did);
				//if (i != 0) {
				//	TSPTRACE("TapiLine::handleEvent: eXosip_call_terminate failed...\n");
				//	eXosip_unlock(ctx);
				//	this->shutdown();
				//	break;
				//}
				//eXosip_unlock(ctx);
				//TSPTRACE("TapiLine::handleEvent: eXosip_call_terminate succeeded...\n");
				break;
			}
			break;
		case ESTABLISHED_TRANSFERRED:
			TSPTRACE("TapiLine::handleEvent: current status: ESTABLISHED_TRANSFERRED\n");
			if (je->type == EXOSIP_CALL_RELEASED) {
				TSPTRACE("TapiLine::handleEvent: EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'\n",je->cid,je->did,je->textinfo);
				this->cid=0;
				this->did=0;
				this->tid=0;
				this->status=IDLE;
				checkForShutdown("established_transferred call released");
				break;
			}
			break;
		default:
			TSPTRACE("TapiLine::handleEvent: current status: unknown --> ERROR\n");
			break;
	}

	this->reportStatus();
	TSPTRACE("TapiLine::handleEvent: ... done\n");
}

// report status of TapiLine object to TAPI (=Call State)
void TapiLine::reportStatus(void)
{
	//Todo: sync reported linecallste with local getTapiStatus function
	//	DWORD lineCallState;
	//	lineCallState = this->getTapiStatus();
	switch(this->status) {
		case IDLE:
			TSPTRACE("TapiLine::reportStatus: new status: IDLE\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_IDLE ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_IDLE, 0, 0);
			break;
		case INCOMING:
			TSPTRACE("TapiLine::reportStatus: new status: INCOMING\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_OFFERING ...\n");
			TSPTRACE("TapiLine::reportStatus: htLine = %d\n",this->htLine);
			TSPTRACE("TapiLine::reportStatus: htCall = %d\n",this->htCall);
			this->reportLineEvent( this->htLine, 
										this->htCall,
										LINE_CALLSTATE,
										LINECALLSTATE_OFFERING,
										0,
										LINEMEDIAMODE_INTERACTIVEVOICE);
			TSPTRACE("TapiLine::reportStatus: sending call-states... done\n");
			break;
		case INCOMING_CANCELED:
			TSPTRACE("TapiLine::reportStatus: new status: INCOMING_CANCELED\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_IDLE...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_IDLE, 0, 0);
			break;
		case RESERVED:
			TSPTRACE("TapiLine::reportStatus: new status: RESERVED\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_DIALTONE ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_DIALTONE, 0, 0);
			break;
		case LEARNING:
			TSPTRACE("TapiLine::reportStatus: new status: LEARNING\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_DIALTONE ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_DIALTONE, 0, 0);
			break;
		case CALLING:
			TSPTRACE("TapiLine::reportStatus: new status: CALLING\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_DIALING ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_DIALING, 0, 0);
			break;
		case CALLING_RINGING:
			TSPTRACE("TapiLine::reportStatus: new status: CALLING_RINGING\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_RINGBACK ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_RINGBACK, 0, 0);
			break;
		case ESTABLISHED:
			TSPTRACE("TapiLine::reportStatus: new status: ESTABLISHED\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_CONNECTED ...\n");
				this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_CONNECTED, 0, 0);
			break;
		case ESTABLISHED_TRANSFERRING:
			TSPTRACE("TapiLine::reportStatus: new status: ESTABLISHED_TRANSFERRING\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_ONHOLDPENDTRANSFER ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_ONHOLDPENDTRANSFER, 0, 0);
			break;
		case ESTABLISHED_TRANSFER_ACCEPTED:
			TSPTRACE("TapiLine::reportStatus: new status: ESTABLISHED_TRANSFER_ACCEPTED\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_ONHOLDPENDTRANSFER ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_ONHOLDPENDTRANSFER, 0, 0);
			break;
		case ESTABLISHED_TRANSFERRED:
			TSPTRACE("TapiLine::reportStatus: new status: ESTABLISHED_TRANSFERRED\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_CONNECTED ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_CONNECTED, 0, 0);
			break;
		default:
			TSPTRACE("TapiLine::reportStatus: new status: unknown --> ERROR\n");
			TSPTRACE("TapiLine::reportStatus: sending LINECALLSTATE_IDLE ...\n");
			this->reportLineEvent( this->htLine, this->htCall, 
					LINE_CALLSTATE, LINECALLSTATE_UNKNOWN, 0, 0);
			break;
	}
}
// handle the SIP events associated with this particular TapiLine
void TapiLine::handleRegEvent(eXosip_event_t *je)
{
	TSPTRACE("TapiLine::handleRegEvent: ...\n");

	switch(this->reg_status) {
		case SIP_UNREGISTERED:
			TSPTRACE("TapiLine::handleRegEvent: current status: SIP_UNREGISTERED\n");
			break;
		case SIP_REGISTERING:
			TSPTRACE("TapiLine::handleRegEvent: current status: SIP_REGISTERING\n");
			break;
		case SIP_REGISTERED:
			TSPTRACE("TapiLine::handleRegEvent: current status: SIP_REGISTERED\n");
			break;
		case SIP_DEREGISTERING:
			TSPTRACE("TapiLine::handleRegEvent: current status: SIP_DEREGISTERING\n");
			break;
		default:
			TSPTRACE("TapiLine::handleRegEvent: current status: unknown --> ERROR\n");
			break;
	}

	switch (je->type) {
		case EXOSIP_REGISTRATION_SUCCESS:
			if (this->reg_status == SIP_DEREGISTERING) {
				TSPTRACE("TapiLine::handleRegEvent: unREGISTRATION successful\n");
				this->reg_status = SIP_UNREGISTERED; 
				break;
			}
			this->reg_status = SIP_REGISTERED;

			if (osip_list_size(&(je->request->contacts)) > 0) {
				osip_contact_t *co;
				co = (osip_contact_t *) osip_list_get(&(je->request->contacts), 0);

				if (co != NULL && co->url != NULL) {
					/* check line parameter */
					osip_uri_param_t *u_param;
					int pos2;

					u_param = NULL;
					pos2 = 0;
					while (!osip_list_eol (&co->url->url_params, pos2)) {
						u_param = (osip_uri_param_t *) osip_list_get (&co->url->url_params, pos2);
						if (u_param == NULL || u_param->gname == NULL || u_param->gvalue == NULL) {
							u_param = NULL;
							/* skip */
						} else if (0 == osip_strcasecmp (u_param->gname, "line")) {
							TSPTRACE("TapiLine::handleRegEvent: line parameter found, value=%s\n",u_param->gvalue);
							// store new line parameter
							if (this->line) {
								TSPTRACE("TapiLine::handleRegEvent: freeing old line parameter");
								free(this->line);
							}
							this->line = (char *) malloc(strlen(u_param->gvalue)+1);
							if (!this->line) {
								TSPTRACE("TapiLine::handleRegEvent: ERROR - could no allocate memory for line parameter");
								free(this->line);
							} else {
								strncpy(this->line, u_param->gvalue, strlen(u_param->gvalue)+1);
							}
							break;
						}
						pos2++;
					}

					if (u_param == NULL || u_param->gname == NULL || u_param->gvalue == NULL) {
						TSPTRACE("TapiLine::handleRegEvent: ERROR - line parameter not found\n");
					}
				} else {
						TSPTRACE("TapiLine::handleRegEvent: ERROR - no contact header in REGISTER\n");
				}
			}
			break;
		case EXOSIP_REGISTRATION_FAILURE:    this->reg_status = SIP_UNREGISTERED; break;
		default: TSPTRACE("TapiLine::handleRegEvent: unknown event type - update TapiLine.cpp:handleRegEvent()");
	}
	this->reportRegStatus();
	TSPTRACE("TapiLine::handleEvent: ... done\n");
}


// report registration status of TapiLine object to TAPI (=Line State)
void TapiLine::reportRegStatus(void)
{ 
	//Todo: sync reported linecallste with local getTapiStatus function
	//	DWORD lineCallState;
	//	lineCallState = this->getTapiStatus();
	switch(this->reg_status) {
		case SIP_UNREGISTERED:
			TSPTRACE("TapiLine::reportRegStatus: new status: SIP_UNREGISTERED\n");
			if ( this->tapiCallback != 0 ) {
				TSPTRACE("TapiLine::reportRegStatus: sending LINEDEVSTATE_DISCONNECTED ...\n");
				this->tapiCallback( this->htLine, NULL, 
					LINE_LINEDEVSTATE, LINEDEVSTATE_DISCONNECTED, 0, 0);
			}
			break;
		case SIP_REGISTERING:
			TSPTRACE("TapiLine::reportStatus: new status: SIP_REGISTERING\n");
			if ( this->tapiCallback != 0 ) {
				TSPTRACE("TapiLine::reportRegStatus: sending LINEDEVSTATE_DISCONNECTED ...\n");
				this->tapiCallback( this->htLine, NULL, 
					LINE_LINEDEVSTATE, LINEDEVSTATE_DISCONNECTED, 0, 0);
			}
			break;
		case SIP_REGISTERED:
			TSPTRACE("TapiLine::reportStatus: new status: SIP_REGISTERED\n");
			if ( this->tapiCallback != 0 ) {
				TSPTRACE("TapiLine::reportRegStatus: sending LINEDEVSTATE_CONNECTED ...\n");
				this->tapiCallback( this->htLine, NULL, 
					LINE_LINEDEVSTATE, LINEDEVSTATE_CONNECTED, 0, 0);
			}
			break;
		case SIP_DEREGISTERING:
			TSPTRACE("TapiLine::reportStatus: new status: SIP_DEREGISTERING\n");
			if ( this->tapiCallback != 0 ) {
				TSPTRACE("TapiLine::reportRegStatus: sending LINEDEVSTATE_DISCONNECTED ...\n");
				this->tapiCallback( this->htLine, NULL, 
					LINE_LINEDEVSTATE, LINEDEVSTATE_DISCONNECTED, 0, 0);
			}
			break;
		default:
			TSPTRACE("TapiLine::reportStatus: new status: unknown --> ERROR\n");
			if ( this->tapiCallback != 0 ) {
				TSPTRACE("TapiLine::reportRegStatus: sending LINEDEVSTATE_DISCONNECTED ...\n");
				this->tapiCallback( this->htLine, NULL, 
					LINE_LINEDEVSTATE, LINEDEVSTATE_DISCONNECTED, 0, 0);
			}
			break;
	}
}
// shutdown TapiLine but do not wait for shutdown (send BYE, clear callid and done)
int TapiLine::shutdown_nowait(void)
{
	int i;
	eXosip_lock(ctx);
	TSPTRACE("TapiLine::shutdown_nowait: calling eXosip_call_terminate: cid=%d, did=%d\n",this->cid, this->did);
	i = eXosip_call_terminate(ctx, this->cid, this->did);
//	i = eXosip_call_terminate(ctx, this->cid, 0); this causes exosip to do not find established dialogs
	eXosip_unlock(ctx);
	if (i!=0) {
		TSPTRACE("TapiLine::shutdown_nowait: ERROR: eXosip_call_terminate failed\n");
		this->status = TapiLine::IDLE;
		this->cid=0;
		this->did=0;
		this->tid=0;
		this->reportStatus();
		return -1;
	}
	TSPTRACE("TapiLine::shutdown_nowait: eXosip_call_terminate succeeded\n");
	return 0;
}

// create and send the initial INVITE request
int TapiLine::sendInvite(void)
{
	int i;
	std::string route;
	osip_message_t *invite;

	if (this->obproxy == "") {
		route   = ("");
	} else {
		// check if the obproxy contains the lr parameter
		if (this->obproxy.find(";lr") == std::string::npos) {
			// not found
			route = ("<sip:") + this->obproxy  + (";lr>");
		} else {
			// found
			route = ("<sip:") + this->obproxy  + (">");
		}
	}

	TSPTRACE("TapiLine::sendInvite: From:        from.c_str()    ='%s'\n",this->from.c_str());
	TSPTRACE("TapiLine::sendInvite: To:            to.c_str()    ='%s'\n",this->to.c_str());
	TSPTRACE("TapiLine::sendInvite: Refer-To: referto.c_str()    ='%s'\n",this->referto.c_str());
	TSPTRACE("TapiLine::sendInvite: Route: route.c_str() ='%s'\n",route.c_str());

	//this->makeSrvLookup();

	i = eXosip_call_build_initial_invite(ctx, &invite,
		this->to.c_str(),	// to (may be equal to from)
		this->from.c_str(),	// from
		route.c_str(),		// route
		"click2dial call");	//subject
	if (i!=0)
	{
		TSPTRACE("TapiLine::sendInvite: eXosip_call_build_initial_invite failed: (bad arguments?)\n");
		this->status = IDLE;
		return -1;
	}
	TSPTRACE("TapiLine::sendInvite: eXosip_call_build_initial_invite succeeded\n");

	char tmp[4096];
	char localip[128];
	eXosip_guess_localip(ctx, AF_INET, localip, 128);
	TSPTRACE("TapiLine::sendInvite: localip = '%s'\n", localip);
	_snprintf(tmp, sizeof(tmp),
		"v=0\r\n"
		"o=click2dial 0 0 IN IP4 %s\r\n"
		"s=click2dial call\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n"
		"m=audio %s RTP/AVP 0 8 18 3 4 97 98\r\n"
		"a=rtpmap:0 PCMU/8000\r\n"
		"a=rtpmap:18 G729/8000\r\n"
		"a=rtpmap:97 ilbc/8000\r\n"
		"a=rtpmap:98 speex/8000\r\n",
		localip, localip, "8000");
	tmp[sizeof(tmp)-1] = '\0'; // ensure null termination
	TSPTRACE("TapiLine::sendInvite: SDP = '%s'\n", tmp);
	osip_message_set_body(invite, tmp, strlen(tmp));
	osip_message_set_content_type(invite, "application/sdp");

//	if (this->autoanswer) {
//		TSPTRACE("TapiLine::sendInvite: auto-asnwer is activated, adding headers ...\n");
//		//SNOM
//		i = osip_message_set_header(invite, "Call-Info", "<sip:127.0.0.1>;answer-after=0");
//		if (i != 0) {
//			TspTrace("osip_message_set_header SNOM failed ...");
//		}
//#ifdef AUTOANSWER_ASTRA
//		// Aastra: http://www.voip-info.org/wiki/view/Sayson+IP+Phone+Auto+Answer
//		i = osip_message_set_header(invite, "Alert-Info", "info=alert-autoanswer");
//		if (i != 0) {
//			TspTrace("osip_message_set_header Aastra failed ...");
//		}
//#else
//		// Polycom: http://www.voip-info.org/wiki/view/Polycom+auto-answer+config
//		i = osip_message_set_header(invite, "Alert-Info", "Ring Answer");
//		if (i != 0) {
//			TspTrace("osip_message_set_header POLYCOM failed ...");
//		}
//#endif		
	if (this->autoanswer) {
		//reverse engineered from FreePBX
		TSPTRACE("TapiLine::sendInvite: auto-answer is activated, adding headers ...\n");
		i = osip_message_set_header(invite, "Call-Info", ";answer-after=0");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer1 failed ...");
		}
		i = osip_message_set_header(invite, "Alert-Info", "<http://www.notused.com>;info=alert-autoanswer;delay=0");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer2 failed ...");
		}
		i = osip_message_set_header(invite, "Alert-Info", "ring-answer");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer3 failed ...");
		}
		i = osip_message_set_header(invite, "Alert-Info", "Ring Answer");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer4 failed ...");
		}

		i = osip_message_set_header(invite, "Allow", "INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, SUBSCRIBE, NOTIFY, INFO, PUBLISH, MESSAGE");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer5 failed ...");
		}
		i = osip_message_set_header(invite, "Supported", "replaces, timer");
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer6 failed ...");
		}
		i = osip_message_set_header(invite, "P-Asserted-Identity", this->from.c_str());
		if (i != 0) {
			TspTrace("osip_message_set_header for autoanswer7 failed ...");
		}
	}

	TspSipTrace("TapiLine::sendInvite: sending message\n", invite);
	eXosip_lock(ctx);
	i = eXosip_call_send_initial_invite(ctx, invite);
	if (i == -1) {
		TSPTRACE("TapiLine::sendInvite: eXosip_call_send_initial_invite(ctx,  failed: (bad arguments?)\n");
		this->status = IDLE;
		eXosip_unlock(ctx);
		return -1;
	}
	TSPTRACE("TapiLine::sendInvite: eXosip_call_send_initial_invite(ctx,  returned: %i\n",i);
	this->cid = i;
	this->status = CALLING;
	eXosip_unlock(ctx);

	TSPTRACE("TapiLine::sendInvite: INVITE sent ... further processing must be done in the thread\n");
	return 0;
}

// create and send an OPTIONS request, used to learn the NAT address
//int TapiLine::sendOptions(void)
//{
//	int i;
//	std::string route, optionsTo;
//	osip_message_t *options;
//
//	if (this->obproxy == "") {
//		route   = ("");
//	} else {
//		// check if the obproxy contains the lr parameter
//		if (this->obproxy.find(";lr") == std::string::npos) {
//			// not found
//			route = ("<sip:") + this->obproxy  + (";lr>");
//		} else {
//			// found
//			route = ("<sip:") + this->obproxy  + (">");
//		}
//	}
//
//	optionsTo  = ("sip:") + this->proxy;
//
//	TSPTRACE("TapiLine::sendOptions: To:    optionsTo.c_str() ='%s'\n",optionsTo.c_str());
//	TSPTRACE("TapiLine::sendOptions: From:  from.c_str()      ='%s'\n",this->from.c_str());
//	TSPTRACE("TapiLine::sendOptions: Route: route.c_str()     ='%s'\n",route.c_str());
//
//	i = eXosip_options_build_request(&options,
//		optionsTo.c_str(),	// to, OPTIONS request will be sent directly to proxy
//		this->from.c_str(),	// from
//		route.c_str());		// route
//	if (i!=0)
//	{
//		TSPTRACE("TapiLine::sendOptions: eXosip_options_build_request failed: (bad arguments?)");
//		this->status = IDLE;
//		return -1;
//	}
//	TSPTRACE("TapiLine::sendOptions: eXosip_options_build_request succeeded\n");
//
//	TspSipTrace("TapiLine::sendOptions: sending message\n", options);
//	eXosip_lock(ctx);
//	i = eXosip_options_send_request(options);
//	if (i == -1) {
//		TSPTRACE("TapiLine::sendOptions: eXosip_options_send_request failed: (bad arguments?)\n");
//		this->status = IDLE;
//		eXosip_unlock(ctx);
//		return -1;
//	}
//	TSPTRACE("TapiLine::sendOptions: eXosip_options_send_request returned: %i\n",i);
//	this->cid = i;
//	this->status = LEARNING;
//	eXosip_unlock(ctx);
//
//	TSPTRACE("TapiLine::sendOptions: OPTIONS sent ... further processing must be done in the thread\n");
//	return 0;
//}

//int TapiLine::makeSrvLookup(void)
//{
//	LONG status; 
//	DNS_RECORD * record;
//	record = NULL;
//
//	status = DnsQuery_A("www.enum.at", DNS_TYPE_A,
//		DNS_QUERY_BYPASS_CACHE, 0, 
//		&record, 0);
//	if (SUCCEEDED(status)) {
//		TSPTRACE("DnsQuery_A: SUCCEEDED(status) www.enum.at== TRUE");
//	}
//	DnsRecordListFree(record,DnsFreeRecordList);
//
//	status = DnsQuery_A("_sip._udp.enum.at", DNS_TYPE_SRV,
//		DNS_QUERY_BYPASS_CACHE, 0, 
//		&record, 0);
//	if (SUCCEEDED(status)) {
//		TSPTRACE("DnsQuery_A: SUCCEEDED(status) == TRUE");
//	}
//	switch (status) {
//		case DNS_ERROR_RCODE_NO_ERROR:
//			TSPTRACE("DnsQuery_A: SRV lookup succeeded");
//			break;
//		case DNS_ERROR_RCODE_FORMAT_ERROR:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_FORMAT_ERROR");
//			break;
//		case DNS_ERROR_RCODE_SERVER_FAILURE:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_SERVER_FAILURE");
//			break;
//		case DNS_ERROR_RCODE_NAME_ERROR:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_NAME_ERROR");
//			break;
//		case DNS_ERROR_RCODE_NOT_IMPLEMENTED:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_NOT_IMPLEMENTED");
//			break;
//		case DNS_ERROR_RCODE_REFUSED:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_REFUSED");
//			break;
//		case DNS_ERROR_RCODE_YXDOMAIN:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_YXDOMAIN");
//			break;
//		case DNS_ERROR_RCODE_YXRRSET:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_YXRRSET");
//			break;
//		case DNS_ERROR_RCODE_NXRRSET:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_NXRRSET");
//			break;
//		case DNS_ERROR_RCODE_NOTAUTH:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_NOTAUTH");
//			break;
//		case DNS_ERROR_RCODE_NOTZONE:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_NOTZONE");
//			break;
//		case DNS_ERROR_RCODE_BADSIG:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_BADSIG");
//			break;
//		case DNS_ERROR_RCODE_BADKEY:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_BADKEY");
//			break;
//		case DNS_ERROR_RCODE_BADTIME:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE_BADTIME");
//			break;
//		case DNS_INFO_NO_RECORDS:
//			TSPTRACE("DnsQuery_A: DNS_INFO_NO_RECORDS");
//			break;
//		case DNS_ERROR_BAD_PACKET:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_BAD_PACKET");
//			break;
//		case DNS_ERROR_NO_PACKET:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_NO_PACKET");
//			break;
//		case DNS_ERROR_RCODE:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_RCODE");
//			break;
//		case DNS_ERROR_UNSECURE_PACKET:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_UNSECURE_PACKET");
//			break;
//		case DNS_ERROR_NO_MEMORY:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_NO_MEMORY");
//			break;
//		case DNS_ERROR_INVALID_NAME:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_NAME");
//			break;
//		case DNS_ERROR_INVALID_DATA:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_DATA");
//			break;
//		case DNS_ERROR_INVALID_TYPE:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_TYPE");
//			break;
//		case DNS_ERROR_INVALID_IP_ADDRESS:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_IP_ADDRESS");
//			break;
//		case DNS_ERROR_INVALID_PROPERTY:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_PROPERTY");
//			break;
//		case DNS_ERROR_TRY_AGAIN_LATER:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_TRY_AGAIN_LATER");
//			break;
//		case DNS_ERROR_NOT_UNIQUE:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_NOT_UNIQUE");
//			break;
//		case DNS_ERROR_NON_RFC_NAME:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_NON_RFC_NAME");
//			break;
//		case DNS_STATUS_FQDN:
//			TSPTRACE("DnsQuery_A: DNS_STATUS_FQDN");
//			break;
//		case DNS_STATUS_DOTTED_NAME:
//			TSPTRACE("DnsQuery_A: DNS_STATUS_DOTTED_NAME");
//			break;
//		case DNS_STATUS_SINGLE_PART_NAME:
//			TSPTRACE("DnsQuery_A: DNS_STATUS_SINGLE_PART_NAME");
//			break;
//		case DNS_ERROR_INVALID_NAME_CHAR:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_INVALID_NAME_CHAR");
//			break;
//		case DNS_ERROR_NUMERIC_NAME:
//			TSPTRACE("DnsQuery_A: DNS_ERROR_NUMERIC_NAME");
//			break;
//		default:
//			TSPTRACE("DnsQuery_A: DNS error %i occurred", status);
//	}
//
//	if (status) {
//		return status;
//	}
//
//	if (!record) {
//		TSPTRACE("ERROR: record = NULL");
//		return 1;
//	}
//
//	//if (!*record) {
//	//	TSPTRACE("ERROR: *record = NULL");
//	//	return 1;
//	//}
//
//	if (! ((record)->wType == DNS_TYPE_SRV) ) {
//		TSPTRACE("ERROR: record != DNS_TYPE_SRV");
//		DnsRecordListFree(record,DnsFreeRecordList);
//		return 1;
//	}
//
//	PDNS_SRV_DATA srvData;
//	srvData = (PDNS_SRV_DATA) &((record)->Data);
//	TSPTRACE("INFO: pNameTarget = %s", srvData->pNameTarget);
//	TSPTRACE("INFO: wPriority   = %i", srvData->wPriority);
//	TSPTRACE("INFO: wWeight     = %i", srvData->wWeight);
//	TSPTRACE("INFO: wPort       = %i", srvData->wPort);
//
//	DnsRecordListFree(record,DnsFreeRecordList);
//
//	return 0;
//}

// handle the incoming-call event associated with this particular TapiLine
void TapiLine::handleIncomingEvent(eXosip_event_t *je)
{

}

void TapiLine::asyncCompletion(const char* text) {
	if (this->shutdownRequestId == 0) {
		TSPTRACE("TapiLine::asyncCompletion: triggered by '%s' but there is no queued dwRequestID ... skipping\n", text);
		return;
	}
	if (!text) {
		text = "";
	}
	if (g_pfnCompletionProc) {
		TSPTRACE("TapiLine::asyncCompletion: triggered by '%s' for dwRequestID=%d\n", text, this->shutdownRequestId);
		g_pfnCompletionProc(this->shutdownRequestId,0);
		this->shutdownRequestId = 0;
	}
}
void TapiLine::checkForShutdown(const char* text) {
	if (!text) {
		text = "";
	}
	if (this->markedforshutdown) {
		TSPTRACE("TapiLine::checkForShutdown: markedforshutdown==true for '%s'\n", text);
		asyncCompletion(text);
		this->markedforshutdown = false;
	}
}

void TapiLine::checkForAsyncRefer(const char* text, LONG status) {
	if (this->transferRequestId == 0) {
		TSPTRACE("TapiLine::checkForAsyncRefer: triggered by '%s' but there is no queued dwRequestID ... skipping\n", text);
		return;
	}
	if (!text) {
		text = "";
	}
	TSPTRACE("TapiLine::checkForAsyncRefer: transferRequestId is set, send asyncComplete for '%s' for requestID %d with status 0x%lx\n", text, this->transferRequestId, status);
	if (g_pfnCompletionProc) {
		g_pfnCompletionProc(this->transferRequestId,status);
		this->transferRequestId = 0;
	}
}

// make the call: initiate the click2dial feature
int TapiLine::referTo(LPCWSTR pszDestAddress)
{
	std::string destAddress;
	int i;

	TSPTRACE("TapiLine::referTo: ...\n");

	// convert lpcwstr to string
	mbstate_t mbs;
	mbsinit(&mbs);
	char feld[202];
    wcsrtombs(feld, &pszDestAddress,100, &mbs);
	destAddress = std::string(feld);
	
	TSPTRACE("TapiLine::referTo: removing spaces and special characters from phone number...\n");
	TSPTRACE("TapiLine::referTo: original number: %s\n",destAddress.c_str());
	std::string::size_type pos;
	while (	(pos = destAddress.find_first_not_of("0123456789")) != std::string::npos) {
		destAddress.erase(pos,1);
		TSPTRACE("TapiLine::referTo: bad digit found, new number is: %s\n",destAddress.c_str());
	}
	TSPTRACE("TapiLine::referTo: trimmed number: %s\n",destAddress.c_str());

	referto = ("sip:")  + destAddress     + ("@") +  this->proxy;

	osip_message_t *refer;
	eXosip_lock(ctx);
	TSPTRACE("TapiLine::referTo: building REFER ...\n");
	i = eXosip_call_build_refer(ctx, this->did, this->referto.c_str(), &refer);
	if (i != 0) {
		TSPTRACE("TapiLine::referTo: eXosip_call_build_refer failed...\n");
		eXosip_unlock(ctx);
		return 1;
	}
	TSPTRACE("TapiLine::referTo: eXosip_call_build_refer succeeded...\n");
	i = osip_message_set_header(refer, "Referred-By", this->referto.c_str());
	if (i != 0) {
		TSPTRACE("TapiLine::referTo: osip_message_set_header failed...\n");
		eXosip_unlock(ctx);
		return 2;
	}
	TSPTRACE("TapiLine::referTo: osip_message_set_header succeeded...\n");
	TspSipTrace("TapiLine::referTo: sending REFER\n",refer);
	i = eXosip_call_send_request(ctx, this->did, refer);
	if (i != 0) {
		TSPTRACE("TapiLine::referTo: eXosip_call_send_request failed...\n");
		eXosip_unlock(ctx);
		return 3;
	}
	TSPTRACE("TapiLine::referTo: eXosip_call_send_request succeeded...\n");
	this->status = ESTABLISHED_TRANSFERRING;
	eXosip_unlock(ctx);
	TSPTRACE("TapiLine::referTo: sending REFER for blind transfer done ... \n");
	return 0;
}

// make the call: initiate the click2dial feature
int TapiLine::redirectTo(LPCWSTR pszDestAddress)
{
	std::string destAddress;
	int i;

	TSPTRACE("TapiLine::redirectTo: ...\n");

	// convert lpcwstr to string
	mbstate_t mbs;
	mbsinit(&mbs);
	char feld[202];
    wcsrtombs(feld, &pszDestAddress,100, &mbs);
	destAddress = std::string(feld);
	
	TSPTRACE("TapiLine::redirectTo: removing spaces and special characters from phone number...\n");
	TSPTRACE("TapiLine::redirectTo: original number: %s\n",destAddress.c_str());
	std::string::size_type pos;
	while (	(pos = destAddress.find_first_not_of("0123456789")) != std::string::npos) {
		destAddress.erase(pos,1);
		TSPTRACE("TapiLine::redirectTo: bad digit found, new number is: %s\n",destAddress.c_str());
	}
	TSPTRACE("TapiLine::redirectTo: trimmed number: %s\n",destAddress.c_str());

	std::string redirectto = ("sip:")  + destAddress     + ("@") +  this->proxy;

	osip_message_t *answer;
	eXosip_lock(ctx);
	TSPTRACE("TapiLine::redirectTo: building 302 ...\n");
	i = eXosip_call_build_answer(ctx, this->tid, 302, &answer);
	if (i != 0) {
		TSPTRACE("TapiLine::redirectTo: eXosip_call_build_answer failed...\n");
		eXosip_unlock(ctx);
		return 1;
	}
	TSPTRACE("TapiLine::redirectTo: eXosip_call_build_answer succeeded...\n");
	i = osip_message_set_header(answer, "Contact", redirectto.c_str());
	if (i != 0) {
		TSPTRACE("TapiLine::redirectTo: osip_message_set_header failed...\n");
		eXosip_unlock(ctx);
		return 2;
	}
	TSPTRACE("TapiLine::redirectTo: osip_message_set_header succeeded...\n");
	TspSipTrace("TapiLine::redirectTo: sending 302\n",answer);
	i = eXosip_call_send_answer(ctx, this->tid, 302, answer);
	if (i != 0) {
		TSPTRACE("TapiLine::redirectTo: eXosip_call_send_answer failed...\n");
		eXosip_unlock(ctx);
		return 3;
	}
	TSPTRACE("TapiLine::redirectTo: eXosip_call_send_answer succeeded...\n");
	this->status = ESTABLISHED_TRANSFERRING;
	eXosip_unlock(ctx);
	TSPTRACE("TapiLine::redirectTo: sending 302 for redirect done ... \n");
	return 0;
}

// report TAPI-events to TAPI
void TapiLine::reportLineEvent(HTAPILINE htLine, HTAPICALL htCall, DWORD dwMsg,
							   DWORD_PTR dwParam1, DWORD_PTR dwParam2, DWORD_PTR dwParam3) {
	TSPTRACE("TapiLine::reportLineEvent: new event: %d, last event: %d\n", dwParam1, lastReportedLineEvent);
	if ( this->tapiCallback != 0 ) {
		// a call can never transit out of IDLE state, there must not be
		// another report after an IDLE report. Also other call states should be 
		// reported only once. TODO: also remember htCall to avoid
		// sending of any event after IDLE for the respective htCall
		if ( (dwMsg == LINE_CALLSTATE) && (dwParam1 == lastReportedLineEvent) ) {
			TSPTRACE("TapiLine::reportLineEvent: skip reporting this LINE_CALLSTATE as same state was already reported\n");
			return;
		}
		this->tapiCallback( htLine, htCall, dwMsg, 
			dwParam1, dwParam2, dwParam3);
		lastReportedLineEvent = dwParam1;
	} else {
		TSPTRACE("TapiLine::reportLineEvent: event not sent as callback function is not defined\n");
	}
}
