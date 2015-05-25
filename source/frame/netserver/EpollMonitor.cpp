// EpollMonitor.cpp: implementation of the EpollMonitor class.
//
//////////////////////////////////////////////////////////////////////

#include "../../../include/frame/netserver/EpollMonitor.h"
#ifndef WIN32
#include <sys/epoll.h>
#include <cstdio>
#endif
#include "../../../include/mdk/atom.h"
#include "../../../include/mdk/mapi.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

EpollMonitor::EpollMonitor()
{
#ifndef WIN32
	m_bStop = true;
#endif
}

EpollMonitor::~EpollMonitor()
{
#ifndef WIN32
	Stop();
#endif
}

void EpollMonitor::SheildSigPipe()
{
#ifndef WIN32
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction( SIGPIPE, &sa, 0 );
#endif
}

bool EpollMonitor::Start( int nMaxMonitor )
{
#ifndef WIN32
	SheildSigPipe();
	m_nMaxMonitor = nMaxMonitor;
	m_epollExit = socket( PF_INET, SOCK_STREAM, 0 );
	/* 创建 epoll 句柄*/
    m_hEPollAccept = epoll_create(m_nMaxMonitor);
    m_hEPollIn = epoll_create(m_nMaxMonitor);
    m_hEPollOut = epoll_create(m_nMaxMonitor);
	if ( -1 == m_hEPollIn || -1 == m_hEPollOut || -1 == m_hEPollAccept ) 
	{
		if ( -1 == m_hEPollIn ) m_initError = "create epollin monitor faild";
		if ( -1 == m_hEPollOut ) m_initError = "create epollout monitor faild";
		if ( -1 == m_hEPollAccept ) m_initError = "create epollaccept monitor faild";
		return false;
	}
	m_bStop = false;
#endif

	return true;
}

bool EpollMonitor::Stop()
{
#ifndef WIN32
	if ( m_bStop ) return true;
	m_bStop = true;
	int64 connectId = -1;//预留id
	AddRecv(m_epollExit, (char*)&connectId, sizeof(int64));
	AddSend(m_epollExit, (char*)&connectId, sizeof(int64));
	AddAccept(m_epollExit);
	::closesocket(m_epollExit);
#endif
	return true;
}

//增加一个Accept操作，有新连接产生，WaitEvent会返回
bool EpollMonitor::AddAccept( int sock )
{
#ifndef WIN32
	epoll_event ev;
    ev.events = EPOLLIN|EPOLLONESHOT;
	if ( sock == m_epollExit ) ev.data.u64 = -1;
    else ev.data.u64 = (int64)sock;
	if ( 0 > epoll_ctl(m_hEPollAccept, EPOLL_CTL_MOD, sock, &ev) ) return false;
#endif	
	return true;
}

//增加一个接收数据的操作，有数据到达，WaitEvent会返回
bool EpollMonitor::AddRecv( int sock, char* pData, unsigned short dataSize )
{
#ifndef WIN32
	int64 connectId = 0;
	memcpy((char*)(&connectId), pData, dataSize);
	epoll_event ev;
	ev.events = EPOLLIN|EPOLLONESHOT;
	ev.data.u64 = connectId;
	if ( 0 > epoll_ctl(m_hEPollIn, EPOLL_CTL_MOD, sock, &ev) ) return false;
#endif	
	return true;
}

//增加一个发送数据的操作，发送完成，WaitEvent会返回
bool EpollMonitor::AddSend( int sock, char* pData, unsigned short dataSize )
{
#ifndef WIN32
	int64 connectId = 0;
	memcpy((char*)(&connectId), pData, dataSize);
 	epoll_event ev;
	ev.events = EPOLLOUT|EPOLLONESHOT;
	ev.data.u64 = connectId;
	if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_MOD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitor( int sock )
{
#ifndef WIN32
    if ( !DelMonitorIn(sock) || !DelMonitorOut(sock) ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitorIn( int sock )
{
#ifndef WIN32
    if ( epoll_ctl(m_hEPollIn, EPOLL_CTL_DEL, sock, NULL) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitorOut( int sock )
{
#ifndef WIN32
    if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_DEL, sock, NULL) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::AddConnectMonitor( int sock )
{
#ifndef WIN32
	epoll_event ev;
	ev.events = EPOLLONESHOT;
	ev.data.u64 = sock;
	if ( epoll_ctl(m_hEPollAccept, EPOLL_CTL_ADD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::AddDataMonitor( int sock, char* pData, unsigned short dataSize )
{
#ifndef WIN32
	int64 connectId = 0;
	memcpy((char*)(&connectId), pData, dataSize);
	epoll_event ev;
	ev.events = EPOLLONESHOT|EPOLLIN;
	ev.data.u64 = connectId;
	if ( epoll_ctl(m_hEPollIn, EPOLL_CTL_ADD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::AddSendableMonitor( int sock, char* pData, unsigned short dataSize )
{
#ifndef WIN32
	int64 connectId = 0;
	memcpy((char*)(&connectId), pData, dataSize);
	epoll_event ev;
	ev.events = EPOLLONESHOT|EPOLLOUT;
	ev.data.u64 = connectId;
	if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_ADD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::WaitConnect( void *eventArray, int &count, int timeout )
{
#ifndef WIN32
	uint64 tid = CurThreadId();
	epoll_event *events = (epoll_event*)eventArray;
	int nPollCount = count;
	while ( !m_bStop )
	{
		count = epoll_wait(m_hEPollAccept, events, nPollCount, timeout );
		if ( -1 == count ) 
		{
			if ( EINTR == errno ) continue;
			return false;
		}
		break;
	}
#endif
	return true;
}

bool EpollMonitor::WaitData( void *eventArray, int &count, int timeout )
{
#ifndef WIN32
	uint64 tid = CurThreadId();
	epoll_event *events = (epoll_event*)eventArray;
	int nPollCount = count;
	while ( !m_bStop )
	{
		count = epoll_wait(m_hEPollIn, events, nPollCount, timeout );
		if ( -1 == count ) 
		{
			if ( EINTR == errno ) continue;
			return false;
		}
		break;
	}
#endif
	return true;
}

bool EpollMonitor::WaitSendable( void *eventArray, int &count, int timeout )
{
#ifndef WIN32
	uint64 tid = CurThreadId();
	epoll_event *events = (epoll_event*)eventArray;
	int nPollCount = count;
	while ( !m_bStop )
	{
		count = epoll_wait(m_hEPollOut, events, nPollCount, timeout );
		if ( -1 == count ) 
		{
			if ( EINTR == errno ) continue;
			return false;
		}
		break;
	}
#endif
	return true;
}

bool EpollMonitor::IsStop( int64 connectId )
{
	if ( connectId == -1 ) return true;
	return false;
}

}//namespace mdk
