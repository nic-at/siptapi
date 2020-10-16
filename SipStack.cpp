// (c): Klaus Darilion (IPCom GmbH, www.ipcom.at)
// License: GNU General Public License 2.0

//ToDo: - use define for exosip_wait value
//      - move all defines in a single .h file
//      - remove unused parts from asttapi
//		- check all mutex


#include <eXosip2/eXosip.h>
#include ".\sipstack.h"

#include ".\WaveTsp.h"

// since eXosip 4.0 the context must be specified
// declared in WaveTsp.h
eXosip_t *ctx = 0;

Mutex lineMut;

#define DEFAULT_SIPPORT 5086
#define DEFAULT_SIPPORTTRIES 300

void dump_event(eXosip_event_t *je) {
	char *type;
	TSPTRACE("Event '%s' received...\n",je->textinfo);
	switch (je->type) {
		case EXOSIP_REGISTRATION_SUCCESS: type="EXOSIP_REGISTRATION_SUCCESS  < user is successfully registred."; break;
		case EXOSIP_REGISTRATION_FAILURE: type="EXOSIP_REGISTRATION_FAILURE  < user is not registred."; break;
		case EXOSIP_CALL_INVITE: type="EXOSIP_CALL_INVITE  < announce a new call"; break;
		case EXOSIP_CALL_REINVITE: type="EXOSIP_CALL_REINVITE  < announce a new INVITE within call"; break;
		case EXOSIP_CALL_NOANSWER: type="EXOSIP_CALL_NOANSWER  < announce no answer within the timeout"; break;
		case EXOSIP_CALL_PROCEEDING: type="EXOSIP_CALL_PROCEEDING  < announce processing by a remote app"; break;
		case EXOSIP_CALL_RINGING: type="EXOSIP_CALL_RINGING  < announce ringback"; break;
		case EXOSIP_CALL_ANSWERED: type="EXOSIP_CALL_ANSWERED  < announce start of call"; break;
		case EXOSIP_CALL_REDIRECTED: type="EXOSIP_CALL_REDIRECTED  < announce a redirection"; break;
		case EXOSIP_CALL_REQUESTFAILURE: type="EXOSIP_CALL_REQUESTFAILURE  < announce a request failure"; break;
		case EXOSIP_CALL_SERVERFAILURE: type="EXOSIP_CALL_SERVERFAILURE  < announce a server failure"; break;
		case EXOSIP_CALL_GLOBALFAILURE: type="EXOSIP_CALL_GLOBALFAILURE  < announce a global failure"; break;
		case EXOSIP_CALL_ACK: type="EXOSIP_CALL_ACK  < ACK received for 200ok to INVITE"; break;
		case EXOSIP_CALL_CANCELLED: type="EXOSIP_CALL_CANCELLED  < announce that call has been cancelled"; break;
		case EXOSIP_CALL_MESSAGE_NEW: type="EXOSIP_CALL_MESSAGE_NEW  < announce new incoming request."; break;
		case EXOSIP_CALL_MESSAGE_PROCEEDING: type="EXOSIP_CALL_MESSAGE_PROCEEDING  < announce a 1xx for request."; break;
		case EXOSIP_CALL_MESSAGE_ANSWERED: type="EXOSIP_CALL_MESSAGE_ANSWERED  < announce a 200ok"; break;
		case EXOSIP_CALL_MESSAGE_REDIRECTED: type="EXOSIP_CALL_MESSAGE_REDIRECTED  < announce a failure."; break;
		case EXOSIP_CALL_MESSAGE_REQUESTFAILURE: type="EXOSIP_CALL_MESSAGE_REQUESTFAILURE  < announce a failure."; break;
		case EXOSIP_CALL_MESSAGE_SERVERFAILURE: type="EXOSIP_CALL_MESSAGE_SERVERFAILURE  < announce a failure."; break;
		case EXOSIP_CALL_MESSAGE_GLOBALFAILURE: type="EXOSIP_CALL_MESSAGE_GLOBALFAILURE  < announce a failure."; break;
		case EXOSIP_CALL_CLOSED: type="EXOSIP_CALL_CLOSED  < a BYE was received for this call"; break;
		case EXOSIP_CALL_RELEASED: type="EXOSIP_CALL_RELEASED  < call context is cleared."; break;
		case EXOSIP_MESSAGE_NEW: type="EXOSIP_MESSAGE_NEW  < announce new incoming request."; break;
		case EXOSIP_MESSAGE_PROCEEDING: type="EXOSIP_MESSAGE_PROCEEDING  < announce a 1xx for request."; break;
		case EXOSIP_MESSAGE_ANSWERED: type="EXOSIP_MESSAGE_ANSWERED  < announce a 200ok"; break;
		case EXOSIP_MESSAGE_REDIRECTED: type="EXOSIP_MESSAGE_REDIRECTED  < announce a failure."; break;
		case EXOSIP_MESSAGE_REQUESTFAILURE: type="EXOSIP_MESSAGE_REQUESTFAILURE  < announce a failure."; break;
		case EXOSIP_MESSAGE_SERVERFAILURE: type="EXOSIP_MESSAGE_SERVERFAILURE  < announce a failure."; break;
		case EXOSIP_MESSAGE_GLOBALFAILURE: type="EXOSIP_MESSAGE_GLOBALFAILURE  < announce a failure."; break;
		case EXOSIP_SUBSCRIPTION_NOANSWER: type="EXOSIP_SUBSCRIPTION_NOANSWER  < announce no answer"; break;
		case EXOSIP_SUBSCRIPTION_PROCEEDING: type="EXOSIP_SUBSCRIPTION_PROCEEDING  < announce a 1xx"; break;
		case EXOSIP_SUBSCRIPTION_ANSWERED: type="EXOSIP_SUBSCRIPTION_ANSWERED  < announce a 200ok"; break;
		case EXOSIP_SUBSCRIPTION_REDIRECTED: type="EXOSIP_SUBSCRIPTION_REDIRECTED  < announce a redirection"; break;
		case EXOSIP_SUBSCRIPTION_REQUESTFAILURE: type="EXOSIP_SUBSCRIPTION_REQUESTFAILURE  < announce a request failure"; break;
		case EXOSIP_SUBSCRIPTION_SERVERFAILURE: type="EXOSIP_SUBSCRIPTION_SERVERFAILURE  < announce a server failure"; break;
		case EXOSIP_SUBSCRIPTION_GLOBALFAILURE: type="EXOSIP_SUBSCRIPTION_GLOBALFAILURE  < announce a global failure"; break;
		case EXOSIP_SUBSCRIPTION_NOTIFY: type="EXOSIP_SUBSCRIPTION_NOTIFY  < announce new NOTIFY request"; break;
		case EXOSIP_IN_SUBSCRIPTION_NEW: type="EXOSIP_IN_SUBSCRIPTION_NEW  < announce new incoming SUBSCRIBE."; break;
		case EXOSIP_NOTIFICATION_NOANSWER: type="EXOSIP_NOTIFICATION_NOANSWER  < announce no answer"; break;
		case EXOSIP_NOTIFICATION_PROCEEDING: type="EXOSIP_NOTIFICATION_PROCEEDING  < announce a 1xx"; break;
		case EXOSIP_NOTIFICATION_ANSWERED: type="EXOSIP_NOTIFICATION_ANSWERED  < announce a 200ok"; break;
		case EXOSIP_NOTIFICATION_REDIRECTED: type="EXOSIP_NOTIFICATION_REDIRECTED  < announce a redirection"; break;
		case EXOSIP_NOTIFICATION_REQUESTFAILURE: type="EXOSIP_NOTIFICATION_REQUESTFAILURE  < announce a request failure"; break;
		case EXOSIP_NOTIFICATION_SERVERFAILURE: type="EXOSIP_NOTIFICATION_SERVERFAILURE  < announce a server failure"; break;
		case EXOSIP_NOTIFICATION_GLOBALFAILURE: type="EXOSIP_NOTIFICATION_GLOBALFAILURE  < announce a global failure"; break;
		case EXOSIP_EVENT_COUNT: type="EXOSIP_EVENT_COUNT  < MAX number of events"; break;
		default: type="unknown type - update SipStack.cpp:dump_event()";
	}
	TSPTRACE("dump_event: ================== new event =======================\n",type);
	TSPTRACE("dump_event: Event type = '%s'...\n",type);
	TSPTRACE("dump_event: event cid = %d\n",je->cid);
	TSPTRACE("dump_event: event did = %d\n",je->did);
	TSPTRACE("dump_event: event tid = %d\n",je->tid);
	TSPTRACE("dump_event: event rid = %d\n",je->rid);

	if (je->response != NULL) {
		TSPTRACE("dump_event: response received:\n");
		TSPTRACE("dump_event: event->response: status code = '%d'\n",je->response->status_code);
		TSPSIPTRACE("dump_event: full response message:",je->response);
	} else {
		TSPTRACE("dump_event: request received:\n");
	}
	if (je->request != NULL) {
		TSPTRACE("dump_event: event->request: method = '%s'\n",je->request->sip_method);
		TSPSIPTRACE("dump_event: full request message:",je->request);
	}

	TSPTRACE("dump_event: ... done\n");

}

SipStack::SipStack(void)
: shutdownThread(false)
{
	this->initialized = false;
	this->shutdownThread = false;
}

SipStack::~SipStack(void)
{
}

// initializes the eXosip stack; returns 0 on sucess
int SipStack::initialize(int transport)
{
	int i;
	char version_string[100];

	TSPTRACE("SipStack::initialize: initializing ...\n");

	if (this->initialized == true) {
		TSPTRACE("SipStack::initialize: WARNING: already initializing ... return 0\n");
		return 0;
	}

#if defined(_DEBUG)
	this->logfile = fopen("c:\\siptapi_osip.log", "a+");
	if (this->logfile) { 
		TSPTRACE("SipStack::initialize: creating eXosip logfile ... done\n");
		TRACE_INITIALIZE (TRACE_LEVEL7, logfile);
	} else {
		TSPTRACE("SipStack::initialize: creating eXosip logfile ... failed, logging to stdout...\n");
		TRACE_INITIALIZE (TRACE_LEVEL7, NULL);
	}
#endif  // _DEBUG

	// There is no eXosip_free() function. Thus we alloc memory only
	// once, to avoid mem leaking
	if (!ctx) {
		ctx = eXosip_malloc();
		if (ctx==NULL) {
			TSPTRACE("SipStack::initialize: eXosip_malloc failed\n");
			return -1;
		}
		TSPTRACE("SipStack::initialize: eXosip_malloc succeeded\n");
	} else {
		TSPTRACE("SipStack::initialize: ctx already allocated\n");
	}

	i=eXosip_init(ctx);
	if (i!=0) {
		TSPTRACE("SipStack::initialize: eXosip_init failed\n");
        return -1;
	}
	TSPTRACE("SipStack::initialize: eXosip_init succeeded\n");

	int sipport = DEFAULT_SIPPORT;
	int counter = 0;
	while (1) {
		if (transport == 1) { //TCP
			i = eXosip_listen_addr(ctx, IPPROTO_TCP, INADDR_ANY, sipport, AF_INET, 0);
		} else {
			i = eXosip_listen_addr(ctx, IPPROTO_UDP, INADDR_ANY, sipport, AF_INET, 0);
		}
		if (i!=0) {
			TSPTRACE("SipStack::initialize: could not initialize port %i\n",sipport);
			sipport += 2;
			counter++;
			if (counter < DEFAULT_SIPPORTTRIES) {
                continue;
			} else {
				TSPTRACE("SipStack::initialize: failed to bind a SIP port after %i tries, stopping...\n",counter);
				return -1;
			}
		}
		TSPTRACE("SipStack::initialize: eXosip uses UDP port %i for SIP signaling\n",sipport);
		break;
	}

	_snprintf(version_string, sizeof(version_string), "www.ipcom.at SIPTAPI " SIPTAPI_VERSION " / eXosip %s", eXosip_get_version());
	version_string[sizeof(version_string)-1] = '\0';
	eXosip_set_user_agent(ctx, version_string);

	i=1;
	eXosip_set_option(ctx, EXOSIP_OPT_UDP_LEARN_PORT, &i);
	this->initialized = true;
	TSPTRACE("SipStack::initialize: initializing ... suceeded\n");

	return 0;
}

bool SipStack::isInitialized(void)
{
	return this->initialized;
}

// shut down eXosip, returns 0 on success
int SipStack::shutdown(void)
{
	TSPTRACE("SipStack::shutdown: shutting down ...\n");

	this->shutdownThread = TRUE;
	int i = 0;
	while ( this->shutdownThread == TRUE) {
		TSPTRACE("SipStack::shutdown: Waiting for event loop to exit ...\n");
		Sleep(100);
		i++;
		if (i > 600) {
			TSPTRACE("SipStack::shutdown: ERROR: shutdownThread failed, quitting anyway ...\n");
			//TerminateThread(this->thrHandle,-1);
			break;
		}
	}
	TSPTRACE("SipStack::shutdown: calling eXosip_quit...\n");
	eXosip_quit(ctx);
	TSPTRACE("SipStack::shutdown: calling eXosip_quit...done\n");
	this->initialized = false;
#if defined(_DEBUG)
	if (this->logfile) {
		TSPTRACE("SipStack::shutdown: closing eXosip logfile ...\n");
		fclose(this->logfile);
		TSPTRACE("SipStack::shutdown: closing eXosip logfile ... done\n");
	} else { 
		TSPTRACE("SipStack::shutdown: no eXosip logfile ... no need to close\n");
	}
#endif  // _DEBUG
	TSPTRACE("SipStack::shutdown: shutting down ... succeeded\n");
	return 0;
}

// add a new TapiLine to SipStack; needs pointer to TapiLine
void SipStack::addTapiLine(TapiLine *tapiLine)
{
	TSPTRACE("SipStack::addTapiLine: adding TapiLine ->dwDeviceID=0x%X to SipStack\n", tapiLine->deviceId);
	listTapiLines.push_back(tapiLine);
}

// delete a TapiLine identified by the deviceID.
void SipStack::deleteTapiLine(TapiLine * tapiLine)
{
	t_listTapiLines::iterator listIt = listTapiLines.begin();
	TSPTRACE("SipStack::deleteTapiLine: ... \n");
	while ( listIt != listTapiLines.end() ) {
		if ((*listIt) == tapiLine) {
			TSPTRACE("SipStack::deleteTapiLine: found TapiLine with ->dwDeviceID=0x%X, deleting...\n", tapiLine->deviceId);
			listTapiLines.erase(listIt);
			// looks like this makes the iterator invalid, thus start
			// again from the beginning
			listIt = listTapiLines.begin();
			continue;
		}
		listIt++;
	}
	TSPTRACE("SipStack::deleteTapiLine: ... done\n");
}

// search if deviceID is already used for a TapiLine, and returns it if found
TapiLine * SipStack::findTapiLine(DWORD dwDeviceID)
{
	t_listTapiLines::iterator listIt = listTapiLines.begin();
	TSPTRACE("SipStack::findTapiLine: ... \n");
	while ( listIt != listTapiLines.end() ) {
		if ((*listIt)->deviceId == dwDeviceID) {
			TSPTRACE("SipStack::findTapiLine: found TapiLine with ->dwDeviceID=0x%X ...\n", (*listIt)->deviceId);
			return (*listIt);
		}
		listIt++;
	}
	TSPTRACE("SipStack::findTapiLine: no TapiLine for dwDeviceID=0x%X found\n", dwDeviceID);
	return 0;
}

//// search if a certain TapiLine is a registered TapiLine, and returns 1 
//// and its dwDeviceID if found
//int SipStack::findTapiLine(TapiLine * tapiLine, DWORD *deviceID)
//{
//	mapTapiLines::iterator it = trackTapiLines.begin();
//	while ( it != trackTapiLines.end() ) {
//		if (tapiLine == it->second) {
//			TSPTRACE("SipStack::findTapiLine2: found TapiLine. corresponding dwDeviceID=0x%X\n", it->first);
//			*deviceID = it->first;
//			return 1;
//		}
//		it++;
//	}
//	TSPTRACE("SipStack::findTapiLine2: no TapiLine found\n");
//	return 0;
//}
//// search if a certain TapiLine is a registered TapiLine, and returns 1 
//// and its dwDeviceID if found
//int SipStack::findTapiLineFromHdline(HDRVLINE hdLine, DWORD *deviceID)
//{
//	mapTapiLines::iterator it = trackTapiLines.begin();
//
//	it = trackTapiLines.begin();
//	TSPTRACE("SipStack::findTapiLineFromHdline: iterator initialized ...\n");
//	int k;
//	k=0;
//	while ( it != trackTapiLines.end() ) {
//		TSPTRACE("SipStack::findTapiLineFromHdline: iterate, k=%d\n", k);
//		//todo: should we also check the did?
//		if (hdLine == it->second->hdLine) {
//			TSPTRACE("SipStack::findTapiLineFromHdline: found TapiLine for hdLine=%d\n", hdLine);
//			break;
//		}
//		it++;
//		k++;
//	}
//	TSPTRACE("SipStack::findTapiLineFromHdline: iteration done: k=%d\n", k);
//	if ( it == trackTapiLines.end() ) {
//		TSPTRACE("SipStack::findTapiLineFromHdline: no TapiLine found for hdLine=%d\n", hdLine);
//		return 0;
//	} else {
//		*deviceID = it->first;
//		return 1;
//	}
//}
// search if a certain TapiLine from its hdLine and return pointer
// to the line
TapiLine * SipStack::getTapiLineFromHdline(HDRVLINE hdLine)
{
	t_listTapiLines::iterator listIt = listTapiLines.begin();
	TSPTRACE("SipStack::getTapiLineFromHdline: ... \n");
	TSPTRACE("SipStack::getTapiLineFromHdline: listTapiLines.size() = %d\n", listTapiLines.size());
	int i=0;
	while ( listIt != listTapiLines.end() ) {
		TSPTRACE("SipStack::getTapiLineFromHdline: check line %d\n", i);
		if ((*listIt)->hdLine == hdLine) {
			TSPTRACE("SipStack::getTapiLineFromHdline: found TapiLine with ->hdLine=0x%X ...\n", (*listIt)->hdLine);
			return (*listIt);
		}
		listIt++;
		i++;
	}
	TSPTRACE("SipStack::getTapiLineFromHdline: no TapiLine for hdLine=0x%X found\n", hdLine);
	return 0;
}

// search if a certain TapiLine from its hdCall and return pointer
// to the TapiLine
TapiLine * SipStack::getTapiLineFromHdcall(HDRVCALL hdCall)
{
	t_listTapiLines::iterator listIt = listTapiLines.begin();
	TSPTRACE("SipStack::getTapiLineFromHdcall: ... \n");
	TSPTRACE("SipStack::getTapiLineFromHdcall: listTapiLines.size() = %d\n", listTapiLines.size());
	int i=0;
	while ( listIt != listTapiLines.end() ) {
		TSPTRACE("SipStack::getTapiLineFromHdcall: check line %d\n", i);
		if ((*listIt)->hdCall == hdCall) {
			TSPTRACE("SipStack::getTapiLineFromHdcall: found TapiLine with ->hdCall=0x%X ...\n", (*listIt)->hdCall);
			return (*listIt);
		}
		listIt++;
		i++;
	}
	TSPTRACE("SipStack::getTapiLineFromHdcall: no TapiLine for hdCall=0x%X found\n", hdCall);
	return 0;
}

// returns the number of active TapiLines
size_t SipStack::getActiveTapiLines(void)
{
	size_t num;
	num = listTapiLines.size();
	TSPTRACE("SipStack::getActiveTapiLines: holding '%zu' active TapiLines\n", num);
	return num;
}

// called by thread to process SIP stuff
void SipStack::processSipMessages(void)
{
	eXosip_event_t *je;
	t_listTapiLines::iterator listIt;

	TSPTRACE("SipStack::processSipMessages: ... \n");
	while (this->shutdownThread == FALSE ) {

		// do SIPmessage processing
		// try the _wait function without lock, as this locked up the Release build
		je = eXosip_event_wait(ctx, 0, 100);

		eXosip_lock(ctx);
		eXosip_automatic_action(ctx);
		eXosip_unlock(ctx);

		TSPTRACE("SipStack::processSipMessages: eXosip_event_wait finished ... \n");

		if (je == NULL) {
			TSPTRACE("SipStack::processSipMessages: no event ... continue\n");
			continue;
		}
		// log event data
		dump_event(je);

		if (je->response != NULL) {
			if ( (je->response->status_code == 401) || (je->response->status_code == 407) ) {
				TSPTRACE("SipStack::processSipMessages: discard event, authentication will be handled by eXosip\n");
				eXosip_event_free(je);
				continue;
			}
		}

		// iterate over all TapiLines and find the corresponding TapiLine
		// if no TapiLine is found: reject request, ignore response
		//
		// for REGISTRATION events use the rid
		// for outgoing calls use the cid use the rid

		TSPTRACE("SipStack::processSipMessages: lock sipStackMutex2 ...\n");
		this->sipStackMut.Lock();
		listIt = listTapiLines.begin();
		TSPTRACE("SipStack::processSipMessages: iterator initialized ...\n");
		int k;

		if (je->type == EXOSIP_CALL_INVITE) {
			int incoming_rid=0;
			// new incoming call
			// find associated registration and use the respective
			// Tapiline to indicate the incoming call.

			// fetch the line parameter
			if (je->request->req_uri) {
				osip_uri_t *ruri;
				osip_uri_param_t *u_param;
				int pos2;

				TSPTRACE("SipStack::processSipMessages: new incoming call, searching for line parameter in RURI ...\n");
				ruri = je->request->req_uri;
				u_param = NULL;
				pos2 = 0;
				while (!osip_list_eol (&ruri->url_params, pos2)) {
					u_param = (osip_uri_param_t *) osip_list_get (&ruri->url_params, pos2);
					if (u_param == NULL || u_param->gname == NULL || u_param->gvalue == NULL) {
						u_param = NULL;
						/* skip */
					} else if (0 == osip_strcasecmp (u_param->gname, "line")) {
						TSPTRACE("SipStack::processSipMessages: line parameter found, value=%s\n",u_param->gvalue);
						// find corresponding TAPI line
						// check if corresponding TAPI line is busy and reject call
						TSPTRACE("SipStack::processSipMessages: looking for registration with line=%s\n", u_param->gvalue);
						k=0;
						while ( listIt != listTapiLines.end() ) {
							TSPTRACE("SipStack::processSipMessages: iterate, k=%d\n", k);
							if ((*listIt)->getLine()) {
								TSPTRACE("SipStack::processSipMessages: tapiline has line %s\n", (*listIt)->getLine());
							}
							if (0 == strcmp((*listIt)->getLine(), u_param->gvalue)) {
								TSPTRACE("SipStack::processSipMessages: found TapiLine for line=%s: rid=%d\n", (*listIt)->getLine(), (*listIt)->getRid());
								incoming_rid=(*listIt)->getRid();
								break;
							}
							listIt++;
							k++;
						}
						TSPTRACE("SipStack::processSipMessages: iteration done: k=%d\n", k);
						if ( listIt == listTapiLines.end() ) {
							TSPTRACE("SipStack::processSipMessages: no TapiLine found for line=%s\n", u_param->gvalue);
						}
						break;
					}
					pos2++;
				}

				if (incoming_rid == 0) {
					TSPTRACE("SipStack::processSipMessages: WARNING - failed to search for line=... parameter in R-URI. Probably the SIP server uses broken NAT traversal. Trying to find the TAPI line with user-name comparison.\n");
					// find corresponding TAPI line by comparing the username
					if (ruri->username) { 
						TSPTRACE("SipStack::processSipMessages: looking for registration with username=%s\n", ruri->username);
						k=0;
						while ( listIt != listTapiLines.end() ) {
							TSPTRACE("SipStack::processSipMessages: iterate, k=%d\n", k);
							if ((*listIt)->getUsername()) {
								TSPTRACE("SipStack::processSipMessages: tapiline has username %s\n", (*listIt)->getUsername());
							}
							if (0 == strcmp((*listIt)->getUsername(), ruri->username)) {
								TSPTRACE("SipStack::processSipMessages: found TapiLine for username=%s: rid=%d\n", (*listIt)->getUsername(), (*listIt)->getRid());
								incoming_rid=(*listIt)->getRid();
								break;
							}
							listIt++;
							k++;
						}
						TSPTRACE("SipStack::processSipMessages: iteration done: k=%d\n", k);
						if ( listIt == listTapiLines.end() ) {
							TSPTRACE("SipStack::processSipMessages: no TapiLine found for username=%s\n", ruri->username);
						}
					} else {
							TSPTRACE("SipStack::processSipMessages: no username in request URI, aborting username comparison ...");
					}
				}


				//if (u_param == NULL || u_param->gname == NULL 
				//	|| u_param->gvalue == NULL || incoming_rid == 0) {
				if (incoming_rid == 0) {
					TSPTRACE("SipStack::processSipMessages: ERROR - no corresponding tapiline found ...reject call\n");
					int i;
					eXosip_lock(ctx);
					i=eXosip_call_send_answer(ctx, je->tid, 404, NULL);
					eXosip_unlock(ctx);
					if (i) {
						TSPTRACE("SipStack::processSipMessages: eXosip_call_send_answer(ctx, %d,404,NULL) failed\n", je->tid);
					} else {
						TSPTRACE("SipStack::processSipMessages: eXosip_call_send_answer(ctx, %d,404,NULL) succeeded\n", je->tid);
					}

					eXosip_event_free(je);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex12 ...\n");
					this->sipStackMut.Unlock();
					continue;
				}

				TSPTRACE("SipStack::processSipMessages: incoming call on tapiline...\n");
				// handle incoming call
				// todo: check if already busy and reject
				if ((*listIt)->getStatus() == TapiLine::IDLE) {
					if ((*listIt)->getSend180Ringing()) {
						TSPTRACE("SipStack::processSipMessages: send180Ringing=yes, send 180 ...\n");
						int i;
						eXosip_lock(ctx);
						i=eXosip_call_send_answer(ctx, je->tid, 180, NULL);
						eXosip_unlock(ctx);
					} else {
						TSPTRACE("SipStack::processSipMessages: send180Ringing=no\n");
					}
					TapiLine *incomingCall = new TapiLine;
					incomingCall->deviceId = (*listIt)->deviceId;
					incomingCall->htLine = (*listIt)->htLine;
					incomingCall->hdLine = (HDRVLINE) rand();
					incomingCall->setLineEventCallback((*listIt)->getLineEventCallback());
					this->addTapiLine(incomingCall);
					incomingCall->handleEvent(je);
				} else if (((*listIt)->getStatus()== TapiLine::CALLING) || ((*listIt)->getStatus()== TapiLine::CALLING_RINGING)){
					// this is probably the incoming call due to the outgoing call caused by the registration
					// just ignore this incoming call
					TSPTRACE("SipStack::processSipMessages: ignore incoming self-call due to outgoing call ...\n");
				} else {
					TSPTRACE("SipStack::processSipMessages: ERROR - tapiline not idle, reject incoming call ...\n");
					int i;
					eXosip_lock(ctx);
					i=eXosip_call_send_answer(ctx, je->tid, 403, NULL);
					eXosip_unlock(ctx);
					if (i) {
						TSPTRACE("SipStack::processSipMessages3: eXosip_call_send_answer(ctx, %d,403,NULL) failed\n", je->tid);
					} else {
						TSPTRACE("SipStack::processSipMessages3: eXosip_call_send_answer(ctx, %d,403,NULL) succeeded\n", je->tid);
					}
				}
				// clean up
				eXosip_event_free(je);
				TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex13 ...\n");
				this->sipStackMut.Unlock();
				continue;
			} else {
					TSPTRACE("SipStack::processSipMessages: ERROR - no RURI in incoming call, reject\n");
					int i;
					eXosip_lock(ctx);
					i=eXosip_call_send_answer(ctx, je->tid, 403, NULL);
					eXosip_unlock(ctx);
					if (i) {
						TSPTRACE("SipStack::processSipMessages2: eXosip_call_send_answer(ctx, %d,403,NULL) failed\n", je->tid);
					} else {
						TSPTRACE("SipStack::processSipMessages2: eXosip_call_send_answer(ctx, %d,403,NULL) succeeded\n", je->tid);
					}
					eXosip_event_free(je);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex133 ...\n");
					this->sipStackMut.Unlock();
					continue;
			}
		}


		switch (je->type) {
			case EXOSIP_REGISTRATION_SUCCESS: 
			case EXOSIP_REGISTRATION_FAILURE: 
				// registration events
				TSPTRACE("SipStack::processSipMessages: looking for registration with rid=%d\n", je->rid);
				while ( listIt != listTapiLines.end() ) {
					TSPTRACE("SipStack::processSipMessages: iterate, k=%d\n", k);
					if (je->rid == (*listIt)->getRid()) {
						TSPTRACE("SipStack::processSipMessages: found TapiLine for rid=%d\n", je->rid);
						break;
					}
					listIt++;
					k++;
				}
				TSPTRACE("SipStack::processSipMessages: iteration done: k=%d\n", k);
				if ( listIt == listTapiLines.end() ) {
					TSPTRACE("SipStack::processSipMessages: no TapiLine found for rid=%d\n", je->rid);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex14 ...\n");
					this->sipStackMut.Unlock();
				} else {
					(*listIt)->handleRegEvent(je);
					eXosip_event_free(je);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex15 ...\n");
					this->sipStackMut.Unlock();
					continue;
				}
				break;

			default:
				while ( listIt != listTapiLines.end() ) {
					TSPTRACE("SipStack::processSipMessages: TapiLines dump:\n");
					TSPTRACE("SipStack::processSipMessages: TapiLine k=%d\n", k);
					TSPTRACE("SipStack::processSipMessages:          cid=%d\n",(*listIt)->getCid());
					TSPTRACE("SipStack::processSipMessages:          did=%d\n",(*listIt)->getDid());
					TSPTRACE("SipStack::processSipMessages:          tid=%d\n",(*listIt)->getTid());
					TSPTRACE("SipStack::processSipMessages:          rid=%d\n",(*listIt)->getRid());
					listIt++;
					k++;
				}

				listIt = listTapiLines.begin();
				TSPTRACE("SipStack::processSipMessages: iterator initialized ...\n");
				k=0;

				while ( listIt != listTapiLines.end() ) {
					int i;
					TSPTRACE("SipStack::processSipMessages: iterate, k=%d\n", k);
					//todo: should we also check the did? do not check against registration
					// on OPTIONS reqeusts the call-id=0. ON outgoing calls und incomign IVNITE the call-id
					// starts with 1, thus, reject all call ids 0.
					if ( je->cid==0 ) {
						TSPTRACE("SipStack::processSipMessages: skip event as event->cid==0\n");
//					} else if ( (i=(*listIt)->getRid()) ) {
//						TSPTRACE("SipStack::processSipMessages: k=%d: skip registration, rid=%d\n", k, i);
					} else if (je->cid == (*listIt)->getCid()) {
						TSPTRACE("SipStack::processSipMessages: found TapiLine for cid=%d\n", je->cid);
						TSPTRACE("SipStack::processSipMessages: incoming did=%d, TapiLine did=%d\n", je->did, (*listIt)->getDid());
						TSPTRACE("SipStack::processSipMessages: incoming tid=%d, TapiLine tid=%d\n", je->tid, (*listIt)->getTid());
						break;
					}
					listIt++;
					k++;
				}
				TSPTRACE("SipStack::processSipMessages: iteration done: k=%d\n", k);
				if ( listIt == listTapiLines.end() ) {
					TSPTRACE("SipStack::processSipMessages: no TapiLine found for cid=%d\n", je->cid);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex16 ...\n");
					this->sipStackMut.Unlock();
				} else {
					(*listIt)->handleEvent(je);
					eXosip_event_free(je);
					TSPTRACE("SipStack::processSipMessages: unlock sipStackMutex17 ...\n");
					this->sipStackMut.Unlock();
					continue;
				}
		}

		if ( je->response == NULL && je->request ) { // reject incoming request
			int i;
			//if (!strcmp(je->request->sip_method, "OPTIONS")) {
			//	eXosip_lock(ctx);
			//	i=eXosip_message_send_answer(ctx, je->tid, 200, NULL);
			//	eXosip_unlock(ctx);
			//	if (i) {
			//		TSPTRACE("SipStack::processSipMessages: eXosip_message_send_answer(%d,200,NULL) failed\n", je->tid);
			//	} else {
			//		TSPTRACE("SipStack::processSipMessages: eXosip_message_send_answer(%d,200,NULL) succeeded\n", je->tid);
			//	}
			if (!strcmp(je->request->sip_method, "OPTIONS")) {
				eXosip_lock(ctx);
				i=eXosip_options_send_answer(ctx, je->tid, 200, NULL);
				eXosip_unlock(ctx);
				if (i) {
					TSPTRACE("SipStack::processSipMessages: eXosip_options_send_answer(%d,200,NULL) failed\n", je->tid);
				} else {
					TSPTRACE("SipStack::processSipMessages: eXosip_options_send_answer(%d,200,NULL) succeeded\n", je->tid);
				}
			} else if (!strcmp(je->request->sip_method, "SUBSCRIBE")) {
				eXosip_lock(ctx);
				i=eXosip_insubscription_send_answer(ctx, je->tid, 405, NULL);
				eXosip_unlock(ctx);
				if (i) {
					TSPTRACE("SipStack::processSipMessages: eXosip_insubscription_send_answer(%d,405,NULL) failed\n", je->tid);
				} else {
					TSPTRACE("SipStack::processSipMessages: eXosip_insubscription_send_answer(%d,405,NULL) succeeded\n", je->tid);
				}
			} else {
				eXosip_lock(ctx);
				i=eXosip_message_send_answer(ctx, je->tid, 405, NULL);
				eXosip_unlock(ctx);
				if (i) {
					TSPTRACE("SipStack::processSipMessages: eXosip_message_send_answer(%d,405,NULL) failed\n", je->tid);
				} else {
					TSPTRACE("SipStack::processSipMessages: eXosip_message_send_answer(%d,405,NULL) succeeded\n", je->tid);
				}
			}
		} else {
			TSPTRACE("SipStack::processSipMessages: ignoring response with cid=%d,did=%d,tid=%d\n", je->cid, je->did, je->tid);
		}

		eXosip_event_free(je);
	} //while (this->shutdownThread == FALSE )

	this->shutdownThread = FALSE;
}
