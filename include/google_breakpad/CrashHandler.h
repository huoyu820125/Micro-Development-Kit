#ifndef CRASH_HANDLE_H_
#define CRASH_HANDLE_H_

//#include "GlobalDefine.h"
#include "client/windows/crash_generation/client_info.h"
#include "client/windows/crash_generation/crash_generation_server.h"
#include "client/windows/handler/exception_handler.h"
#include "client/windows/common/ipc_protocol.h"
#include "client/windows/tests/crash_generation_app/abstract_class.h"
#include "../mdk/Logger.h"
#include <string>
using namespace std;

const wchar_t kPipeName[] = L"\\\\.\\pipe\\BreakpadCrashServices\\TestServer";

namespace mdk
{

class CrashClientHandler
{
public:
	CrashClientHandler();
	~CrashClientHandler();

	void Start(wchar_t * pAppName);
private:
	google_breakpad::ExceptionHandler* handler;
	google_breakpad::CustomInfoEntry *kCustomInfoEntries;
};

class CrashServerHandler
{
public:
	CrashServerHandler();
	virtual ~CrashServerHandler();

	virtual bool Start();
private:
	google_breakpad::CrashGenerationServer* crash_server;
};

bool WStringToString(const std::wstring &wstr,std::string &str);
bool StringToWString(const std::string &str,std::wstring &wstr);

};//---end--- mdk
#endif