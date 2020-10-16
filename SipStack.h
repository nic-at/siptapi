// (c): Klaus Darilion (IPCom GmbH, www.ipcom.at)
// License: GNU General Public License 2.0

#pragma once

#include <map>
#include <list>
#include "utilities.h" //Mutex
#include "TapiLine.h"

extern Mutex lineMut;
//kd
typedef std::map<DWORD, TapiLine*> mapTapiLines;
typedef std::list<TapiLine*> t_listTapiLines;

class SipStack
{
public:
	SipStack(void);
	~SipStack(void);
	Mutex sipStackMut;

private:
	bool initialized;
	mapTapiLines trackTapiLines;
	t_listTapiLines listTapiLines;

#if defined(_DEBUG)
	FILE *logfile;
#endif  // _DEBUG

public:
	// initializes the eXosip stack; returns 0 on sucess
	int initialize(int transport);
	bool isInitialized(void);
	// shut down eXosip, returns 0 on success
	int shutdown(void);
	// add a new TapiLine to SipStack; needs pointer to TapiLine
	void addTapiLine(TapiLine *);
	// delete a TapiLine identified by the deviceID.
	void deleteTapiLine(TapiLine *);
	// search if deviceID is already used for a TapiLine, and returns it if found
	TapiLine * findTapiLine(DWORD);
	//// search if a certain TapiLine is a regsitered TapiLine, and returns 1
	//// and the deviceID if found
	//int findTapiLine(TapiLine *, DWORD *deviceID);
	//// search if a certain TapiLine is a regsitered TapiLine, and returns 1
	//// and the deviceID if found
	//int findTapiLineFromHdline(HDRVLINE hdline, DWORD *deviceID);
	// search if a certain TapiLine from its hdLine and return pointer
	// to the line
	TapiLine * getTapiLineFromHdline(HDRVLINE hdLine);
	// search if a certain TapiLine from its hdCall and return pointer
	// to the TapiLine
	TapiLine * getTapiLineFromHdcall(HDRVCALL hdCall);
	// returns the number of active TapiLines
	size_t getActiveTapiLines(void);
	// called by thread to process SIP stuff
	void processSipMessages(void);
	// this variable is used to signal the SipStack to exit from the SIP processing event loop
	bool shutdownThread;
	// handle to the worker thread
	HANDLE thrHandle;
};
