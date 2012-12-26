// stdafx.cpp : source file that includes just the standard includes
//	dmk_static.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "TestServer.h"

#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
using namespace std;

#ifdef WIN32
#ifdef _DEBUG
#pragma comment ( lib, "../lib/mdk_d.lib" )
#else
#pragma comment ( lib, "../lib/mdk.lib" )
#endif
#endif

int main()
{
	TestServer ser;
	
	ser.Start();
	ser.WaitStop();
	printf( "exit\n" );
	return 0;
	
	return 0;
}
