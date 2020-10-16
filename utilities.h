/*
  Name: utilities.h
  License: Under the GNU General Public License Version 2 or later (the "GPL")
  (c) Nick Knight

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

//Public functions from this file

#include <windows.h>
#include <string>

#ifndef UTILITIES
#define UTILITIES

//One off time call to ensure we have a key for our configs
bool initConfigStore(void);

bool storeConfigString(std::string item, std::string str);
bool readConfigString(std::string item, std::string &str);
bool storeConfigInt(std::string item, DWORD str);
bool readConfigInt(std::string item, DWORD &str);

//some mutex simplification 
//courtsey of a doc@ http://world.std.com/~jimf/papers/c++sync/c++sync.html

class Mutex {
private:
  CRITICAL_SECTION lock;

public:
  Mutex(void) {
    InitializeCriticalSection(&lock);
  }

  void Lock(void) {
    EnterCriticalSection(&lock);
  }

  void Unlock(void) {
    LeaveCriticalSection(&lock);
  }
};

#endif


