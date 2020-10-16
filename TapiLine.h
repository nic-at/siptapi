// (c): Klaus Darilion (IPCom GmbH, www.ipcom.at)
// License: GNU General Public License 2.0

#pragma once

#include <string>
#include <tspi.h>

//Function call for asyncrounous functions
extern ASYNC_COMPLETION g_pfnCompletionProc;

class TapiLine
{
public:
	TapiLine(void);
	~TapiLine(void);
	enum StatusType { IDLE, RESERVED, LEARNING, CALLING, CALLING_RINGING,
					  ESTABLISHED, ESTABLISHED_TRANSFERRING, 
					  ESTABLISHED_TRANSFER_ACCEPTED, ESTABLISHED_TRANSFERRED, INCOMING, INCOMING_CANCELED};
	enum RegStatusType { SIP_UNREGISTERED, SIP_REGISTERING, SIP_REGISTERED, SIP_DEREGISTERING};
	// status of the proxy registration
	RegStatusType reg_status;
private:
	// status of the TapiLine (state machine status)
	StatusType status;
	// SIP proxy
	std::string proxy;
	// SIP outbound proxy (eXosip uses Route: header to address the outbound proxy)
	std::string obproxy;
	// username (used in SIP AoR and as authentication user if no authuser is supplied)
	std::string username;
	// authentication username (used as authentication user)
	std::string authusername;
	// phone's (the party which gets called to get refered) username
	std::string phoneusername;
	// SIP authentication password
	std::string password;
	// SIP auto-answer
	int autoanswer;
	// if realm should be checked
	int enableRealmCheck;
	// transport protocol
	int transportProtocol;
	// if we should send 180 Ringing on incoming calls
	int send180Ringing;
	// if reverse-mode/ACD-mode should be used
	int reverseMode;
	// eXosip internal call id
	int cid;
	// eXosip internal transaction id
	int tid;
	// eXosip internal dialog id
	int did;
	// eXosip internal registration id
	int rid;
	// eXosip registration line parameter
	char *line;
public:
	// deviceId - thus we only have one device for each call for each line
	DWORD deviceId;
	// if the thread should shutdown this TapiLine, set by TSPI_lineDrop
	bool markedforshutdown;
	// dwRequestID for asynch lineDrop signaling
	DRV_REQUESTID shutdownRequestId;
	// dwRequestID for asynch blind transfer reporting
	DRV_REQUESTID transferRequestId;
	// dwRequestID for asynch redirect reporting
	DRV_REQUESTID redirectRequestId;
	// callerid of incoming calls
	char * incomingFromDisplayName;
	char * incomingFromUriUser;
	// calledid of incoming calls
	char * incomingToDisplayName;
	char * incomingToUriUser;
public:
	// set the SIP proxy
	void setProxy(std::string strdata);
	// set the SIP outbound proxy
	void TapiLine::setObProxy(std::string strdata);
	// set the SIP AoR username and authentication user name if no authusername is supplied
	void TapiLine::setUsername(std::string strdata);
	// get the SIP AoR username
	const char * TapiLine::getUsername();
	// set the authentication user name
	void TapiLine::setAuthusername(std::string strdata);
	// set the phone's user name
	void TapiLine::setPhoneusername(std::string strdata);
	// set the SIP authentication password
	void TapiLine::setPassword(std::string strdata);
	// set auto-answer
	void TapiLine::setAutoanswer(int temp);
	// set if realmcheck is enabled
	void TapiLine::setEnableRealmCheck(int);
	// set transport protocol
	void TapiLine::setTransportProtocol(int);
	int TapiLine::getTransportProtocol(void);
	// set 180ringing
	void TapiLine::setSend180Ringing(int);
	int TapiLine::getSend180Ringing(void);
	// set reverse-mode
	void TapiLine::setReverseMode(int);
	// get reverse-mode
	int TapiLine::getReverseMode();
private:
	// keeps track if this TapiLine is initalized (SIP parameters)
	bool initialized;
	// pointer to callback function
	LINEEVENT tapiCallback;	
public:
	// handle to the TAPI LINE, must be used as ID in callback function, will be provided by the TAPI
	HTAPILINE htLine;
	// handle to the TAPI LINE, will be provided to the TAPI
	HDRVLINE hdLine;
	// handle to the TAPI Call on this TAPI LINE, will be provided by the TAPI
	HTAPICALL htCall;
	// handle to the TAPI Call on this TAPI LINE, will be provided to the TAPI
	HDRVCALL hdCall;
	// is incoming
	bool isIncoming;
public:
	// initialize the Line, sets the username, password, proxy... for the SIP call
	int initialize(void);
	// check if the TapiLine is initialized (SIP paramters are set)
	bool isInitialized(void);
private:
//	// pointer to line handle
//	LPHDRVLINE lphdLine;
public:
	// store TAPI Line handle inside the TapiLine object
	void setTapiLine(HTAPILINE htLine);
	// retrieve TAPI Line handle from the TapiLine object
	HTAPILINE getTapiLine();
	// REGISTER to the SIP proxy
	int register_to_sipproxy();
	// un-REGISTER from the SIP proxy
	int unregister_from_sipproxy();
	// store the callback function used for reporting line status to TAPI
	void setLineEventCallback(LINEEVENT callback);
	// retrieve the callback function used for reporting line status to TAPI
	LINEEVENT getLineEventCallback();
	// store the call handle (a handle for a certain call on this line)
	// we only allow one call per line, thus this is a "singleton"
	void setTapiCall(HTAPICALL htCall);
	// make the call: initiate the click2dial feature
	int makeCall(LPCWSTR pszDestAddress, DWORD dwCountryCode, DWORD dwRequestID);
	// perform a blind transfer
	int referTo(LPCWSTR pszDestAddress);
	// perform a redirect
	int redirectTo(LPCWSTR pszDestAddress);
	// retrieve the status of the state machine
	StatusType getStatus(void);
	// set the status of the state machine
	void setStatus(StatusType newStatus);
	// retrieve the status of the state machine using the TAPI LINECALLSTATE_ Constants
	DWORD getTapiStatus(void);
	// mark the line to beeing shutdown by thread (async shutodwn tests for dialer.exe)
	void markForShutdown(bool);
	// return if the line is marked for shutdown
	bool isMarkForShutdown();
	// terminate all ongoing calls and set status to idle
	int shutdown(DRV_REQUESTID);
	// return the eXosip internal call id for this TapiLine
	int getCid(void);
	// return the eXosip internal transcation id for this TapiLine
	int getTid(void);
	// return the eXosip internal registration id (rid) for this TapiLine
	int getRid(void);
	// return the eXosip internal registration line for this TapiLine
	char *getLine(void);
	// return the eXosip internal dialog id for this TapiLine
	int getDid(void);
	// handle the SIP events associated with this particular TapiLine
	void handleEvent(eXosip_event_t *je);
	// handle the SIP registration events associated with this particular TapiLine
	void handleRegEvent(eXosip_event_t *je);
	// handle the incoming-call event associated with this particular TapiLine
	void handleIncomingEvent(eXosip_event_t *je);
	// report status of TapiLine object to TAPI
	void reportStatus(void);
	// report registration status of TapiLine object to TAPI
	void reportRegStatus(void);
	// send async completion request to TAPI
	void asyncCompletion(const char *);
	// check for shutdown handling
	void checkForShutdown(const char *);
	// send async completion request for blind transfer to TAPI
	void checkForAsyncRefer(const char *, LONG);
private:
	// SIP From/To URI
	std::string from;
	// if specified, this will be used in RURI and To instead of makeing it equal to from
	std::string to;
	// Refer-To URI in REFER request
	std::string referto;
	// report TAPI-events to TAPI
	void reportLineEvent(HTAPILINE htLine, HTAPICALL htCall, DWORD dwMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2, DWORD_PTR dwParam3);
	DWORD_PTR lastReportedLineEvent;
public:
	// shutdown TapiLine but do not wait for shutdown (send BYE, clear callid and done)
	int shutdown_nowait(void);
	// create and send the initial INVITE request
	int sendInvite(void);
	// create and send an OPTIONS request, used to learn the NAT address
	//int sendOptions(void);
	//int makeSrvLookup(void);
};
