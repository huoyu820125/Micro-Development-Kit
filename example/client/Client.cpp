// SerMain.cpp : Defines the entry point for the console application.
//

#include "mdk/Socket.h"
using namespace mdk;

#include <map>
using namespace std;
typedef struct THREAD_CONTROL
{
	DWORD dwID;
	bool bCheck;
}THREAD_CONTROL;

map<DWORD,THREAD_CONTROL*> g_activeThread;
DWORD TMainThread( LPVOID pParam );//主线程入口
UINT uPort;
char strIP[256];

time_t g_lastdata = 0;
DWORD Client( LPVOID pParam  )
{
	DWORD dwThreadID;
	Socket sock;
	sock.Init( Socket::tcp );
	printf( "connect(%s:%d)\n", strIP, uPort );
	if ( !sock.Connect( strIP, uPort ) )
	{
		printf( "connect error\n" );
		return 0;
	}

	HANDLE hThread = CreateThread( 
		NULL, 0, 
		(LPTHREAD_START_ROUTINE)TMainThread,
		(LPVOID)&sock, 0, &dwThreadID );
	if ( NULL == hThread ) 
	{
		sock.Close();
		printf( "create thread error\n" );
		return 0;
	}
	
	short nSendSize;
	char buf[256];
	short len = 20;
	memcpy( buf, &len, 2 );
	memcpy( &buf[2], "01234567890123456789", len );
	len += 2;
	DWORD dwID = GetCurrentThreadId();
	while ( 1 )
	{
		nSendSize = sock.Send( buf, len );
		if ( -1 == nSendSize ) 
		{
			printf( "线程(%d)退出:发送失败\n", dwID );
			return 0;
		}
		Sleep( 100 );
		continue;
	}
	
	sock.Close();

	return 0;
}

DWORD TMainThread( LPVOID pParam )//主线程入口
{
	static int nID = 0;
	DWORD dwID = GetCurrentThreadId();
	THREAD_CONTROL *pThread = new THREAD_CONTROL;
	pThread->dwID = dwID;
	pThread->bCheck = false;
	g_activeThread.insert( map<DWORD,THREAD_CONTROL*>::value_type( dwID, pThread ) );

	char buf[256];
	short nLen;
	Socket *pSock = (Socket*)pParam;
	while ( 1 )
	{
		nLen = pSock->Receive( buf, 2 );
		if ( nLen <= 0 ) 
		{
			map<DWORD,THREAD_CONTROL*>::iterator it = g_activeThread.find( dwID );
			if ( it != g_activeThread.end() ) g_activeThread.erase( it );
			printf( "线程(%d)退出:服务器断开连接\n", dwID );
			return 0;
		}
		memcpy( &nLen, buf, 2 );
		nLen = pSock->Receive( buf, nLen );
		if ( nLen <= 0 ) 
		{
			map<DWORD,THREAD_CONTROL*>::iterator it = g_activeThread.find( dwID );
			if ( it != g_activeThread.end() ) g_activeThread.erase( it );
			printf( "线程(%d)退出:服务器断开连接\n", dwID );
			return 0;
		}
		g_lastdata = time(NULL);
		if ( pThread->bCheck )
		{
			buf[nLen] = 0;
			printf( buf );
		}
	}
}

int main(int argc, char* argv[])
{
	uPort = GetPrivateProfileInt( "Server", "Port", 8080, ".\\cli.ini" );
	DWORD dwSize = 255;
	GetPrivateProfileString( "Server", "IP", "127.0.0.1", strIP, dwSize, ".\\cli.ini" );
	setvbuf( stdout, (char*)NULL, _IONBF, 0 );     //added
	Socket::SocketInit();
	//	Client( NULL );
	DWORD dwThreadID;
	int i = 1;
	int nThreadCount = 0;
	char sCount[5];
	sCount[0]= getchar();
	sCount[1]= getchar();
	sCount[2]= getchar();
	getchar();
	sCount[3]= 0;
	nThreadCount = atoi( sCount );
	//	nThreadCount = 999;
	printf( "创建%d个client 按任意键开始\n", nThreadCount );
	for ( i = 1; i <= nThreadCount; i++ ){
		HANDLE hThread = CreateThread( 
			NULL, 0, 
			(LPTHREAD_START_ROUTINE)Client,
			(LPVOID)NULL, 0, &dwThreadID );
		if ( NULL == hThread )
		{
			printf( "create client(%d) error\n", i );
		}
		if ( 0 == i % 200 ) 
		{
			printf( "create%d\n", i );
			//			getchar();
		}
	}
	char c[100];
	int nPos;
	THREAD_CONTROL* pThead;
	map<DWORD, THREAD_CONTROL*>::iterator it;
	int nCount;
	while ( 1 ){
		c[0] = getchar();
		c[1] = getchar();
		c[2] = getchar();
		c[3] = getchar();
		c[3] = 0;
		nPos = atoi( c );
		nCount = g_activeThread.size();
		if ( nPos > nCount )
		{
			printf( "创建线程%d/%d\n", nPos, nCount );
			HANDLE hThread = CreateThread( 
				NULL, 0, 
				(LPTHREAD_START_ROUTINE)Client,
				(LPVOID)NULL, 0, &dwThreadID );
		}
		else printf( "检查线程%d/%d\n", nPos, nCount );
		tm *last = localtime(&g_lastdata);
		char stime[256];
		strftime( stime, 256, "%Y-%m-%d %H:%M:%S", last );
		printf( "last data:%s\n", stime );
		Sleep( 1000 );
		it = g_activeThread.begin();
		for ( i = 1; it != g_activeThread.end(); it++, i++ )
		{
			pThead = it->second;
			if ( i == nPos ) pThead->bCheck = true;
			else pThead->bCheck = false;
		}
		
		Sleep( 1000 );
	}
	
	return 0;
}

