// stdafx.cpp : source file that includes just the standard includes
//	dmk_static.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "mdk/Thread.h"
#include "mdk/Lock.h"
#include "mdk/Logger.h"
#include "mdk/Signal.h"
#include "mdk/MemoryPool.h"
#include "mdk/IOBuffer.h"
#include "mdk/ConfigFile.h"
#include "mdk/FixLengthInt.h"
#include "mdk/Socket.h"
#include "mdk/Executor.h"
#include "mdk/ThreadPool.h"
#include "mdk/Task.h"
#include "mdk/atom.h"
#include "frame/netserver/IOCPFrame.h"
#include "TestServer.h"

#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
using namespace std;

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
void XXSleep( long lSecond )
{
#ifndef WIN32
	usleep( lSecond * 1000 );
#else
	Sleep( lSecond );
#endif
}


class TT
{
public:
	TT(){
		d = 0;
	}
	~TT(){
	}
	int d;

	void *f(void*)
	{
		printf( "%d\n", d );
		return 0;
	}
};
int main()
{
	mdk::Socket::SocketInit();
	TestServer ser;
	
	ser.Start();
	ser.WaitStop();
	printf( "exit\n" );
	return 0;
	
	return 0;
}
