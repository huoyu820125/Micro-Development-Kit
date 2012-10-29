// EpollMonitor.cpp: implementation of the EpollMonitor class.
//
//////////////////////////////////////////////////////////////////////

#include "frame/netserver/EpollMonitor.h"
#ifndef WIN32
#include <sys/epoll.h>
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

EpollMonitor::EpollMonitor()
{
#ifndef WIN32
	m_bStop = true;
	m_acceptEvents = new Queue(MAXPOLLSIZE);
	m_inEvents = new Queue(MAXPOLLSIZE);
	m_outEvents = new Queue(MAXPOLLSIZE);
#endif
}

EpollMonitor::~EpollMonitor()
{
#ifndef WIN32
	Stop();
	if ( NULL != m_acceptEvents )
	{
		delete m_acceptEvents;
		m_acceptEvents = NULL;
	}
	if ( NULL != m_inEvents )
	{
		delete m_inEvents;
		m_inEvents = NULL;
	}
	if ( NULL != m_outEvents )
	{
		delete m_outEvents;
		m_outEvents = NULL;
	}
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
	m_acceptThread.Run( Executor::Bind(&EpollMonitor::WaitAcceptEvent), this, NULL);
	m_pollinThread.Run( Executor::Bind(&EpollMonitor::WaitInEvent), this, NULL);
	m_polloutThread.Run( Executor::Bind(&EpollMonitor::WaitOutEvent), this, NULL);
#endif

	return true;
}

bool EpollMonitor::Stop()
{
#ifndef WIN32
	if ( m_bStop ) return true;
	m_bStop = true;
	AddRecv(m_epollExit, NULL, 0);
	AddSend(m_epollExit, NULL, 0);
	AddAccept(m_epollExit);
	m_acceptThread.Stop();
	m_pollinThread.Stop();
	m_polloutThread.Stop();
#endif
	return true;
}

void* EpollMonitor::WaitAcceptEvent( void *pData )
{
#ifndef WIN32
	epoll_event *events = new epoll_event[m_nMaxMonitor];	//epoll事件
	Socket sock;
	Socket sockClient;
	SOCKET hSock;
	/* 等待有事件发生 */
	while ( !m_bStop )
	{
		int nPollCount = epoll_wait(m_hEPollAccept, events, m_nMaxMonitor, -1 );
		if ( -1 == nPollCount ) break;
		/* 处理所有事件，nfds 为返回发生事件数 */
		int i = 0;
		for ( i = 0; i < nPollCount; i++ ) 
		{
			if ( m_epollExit == events[i].data.fd ) 
			{
				::closesocket(m_epollExit);
				break;
			}
			
			sock.Detach();
			sock.Attach(events[i].data.fd);
			while ( true )
			{
				sock.Accept( sockClient );
				if ( INVALID_SOCKET == sockClient.GetSocket() ) break;
				sockClient.SetSockMode();
				hSock = sockClient.Detach();
				AddMonitor(hSock);
				while ( !m_acceptEvents->Push((void*)hSock) ) m_sigWaitAcceptSpace.Wait();
				m_ioSignal.Notify();
			}
		}
	}
	delete[]events;
#endif
	return NULL;
}

void* EpollMonitor::WaitInEvent( void *pData )
{
#ifndef WIN32
	epoll_event *events = new epoll_event[m_nMaxMonitor];	//epoll事件
	/* 等待有事件发生 */
	while ( !m_bStop )
	{
		int nPollCount = epoll_wait(m_hEPollIn, events, m_nMaxMonitor, -1 );
		if ( -1 == nPollCount ) break;
		/* 处理所有事件，nfds 为返回发生事件数 */
		int i = 0;
		for ( i = 0; i < nPollCount; i++ ) 
		{
			if ( m_epollExit == events[i].data.fd ) 
			{
				::closesocket(m_epollExit);
				break;
			}
			while ( !m_inEvents->Push((void*)events[i].data.fd) ) m_sigWaitInSpace.Wait();
			m_ioSignal.Notify();
		}
	}
	delete[]events;
#endif
	return NULL;
}

void* EpollMonitor::WaitOutEvent( void *pData )
{
#ifndef WIN32
	epoll_event *events = new epoll_event[m_nMaxMonitor];	//epoll事件
	/* 等待有事件发生 */
	while ( !m_bStop )
	{
		int nPollCount = epoll_wait(m_hEPollOut, events, m_nMaxMonitor, -1 );
		if ( -1 == nPollCount ) break;
		/* 处理所有事件，nfds 为返回发生事件数 */
		int i = 0;
		for ( i = 0; i < nPollCount; i++ ) 
		{
			if ( m_epollExit == events[i].data.fd ) 
			{
				::closesocket(m_epollExit);
				break;
			}
			while ( !m_outEvents->Push((void*)events[i].data.fd) ) m_sigWaitOutSpace.Wait();
			m_ioSignal.Notify();
		}
	}
	delete[]events;
#endif
	return NULL;
}

bool EpollMonitor::WaitEvent( void *eventArray, int &count, bool block )
{
#ifndef WIN32
	count = 0;
	IO_EVENT *events = (IO_EVENT*)eventArray;
	void *pElement;
	while ( !m_bStop )
	{
		//先处理新连接
		while ( !m_bStop )
		{
			pElement = m_acceptEvents->Pop();
			if ( NULL == pElement ) 
			{
				m_sigWaitAcceptSpace.Notify();
				break;
			}
			events[count].sock = (SOCKET)pElement;
			events[count++].type = epoll_accept;//发送事件
		}
		//先处理发送事件
		while ( !m_bStop )
		{
			pElement = m_outEvents->Pop();
			if ( NULL == pElement ) 
			{
				m_sigWaitOutSpace.Notify();
				break;
			}
			events[count].sock = (SOCKET)pElement;
			events[count++].type = epoll_out;//发送事件
		}
		//先处理接收事件
		while ( !m_bStop )
		{
			pElement = m_inEvents->Pop();
			if ( NULL == pElement ) 
			{
				m_sigWaitInSpace.Notify();
				break;
			}
			events[count].sock = (SOCKET)pElement;
			events[count++].type = epoll_in;//接收事件
		}
		if ( 0 < count ) break;
		if ( !block ) return false;
		m_ioSignal.Wait();
	}
#endif
	return true;
}

//增加一个Accept操作，有新连接产生，WaitEvent会返回
bool EpollMonitor::AddAccept( SOCKET sock )
{
#ifndef WIN32
	epoll_event ev;
    ev.events = EPOLLIN|EPOLLET;
    ev.data.fd = sock;
	if ( 0 > epoll_ctl(m_hEPollAccept, EPOLL_CTL_ADD, sock, &ev) ) return false;
#endif	
	return true;
}

//增加一个接收数据的操作，有数据到达，WaitEvent会返回
bool EpollMonitor::AddRecv( SOCKET sock, char* recvBuf, unsigned short bufSize )
{
#ifndef WIN32
	epoll_event ev;
    ev.events = EPOLLIN|EPOLLONESHOT;
    ev.data.fd = sock;
	if ( 0 > epoll_ctl(m_hEPollIn, EPOLL_CTL_MOD, sock, &ev) ) return false;
#endif	
	return true;
}

//增加一个发送数据的操作，发送完成，WaitEvent会返回
bool EpollMonitor::AddSend( SOCKET sock, char* dataBuf, unsigned short dataSize )
{
#ifndef WIN32
	epoll_event ev;
    ev.events = EPOLLOUT|EPOLLONESHOT;
    ev.data.fd = sock;
    if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_MOD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitor( SOCKET sock )
{
#ifndef WIN32
    if ( !DelMonitorIn(sock) || !DelMonitorOut(sock) ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitorIn( SOCKET sock )
{
#ifndef WIN32
    if ( epoll_ctl(m_hEPollIn, EPOLL_CTL_DEL, sock, NULL) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::DelMonitorOut( SOCKET sock )
{
#ifndef WIN32
    if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_DEL, sock, NULL) < 0 ) return false;
#endif	
	return true;
}

bool EpollMonitor::AddMonitor( SOCKET sock )
{
#ifndef WIN32
	epoll_event ev;
    ev.events = EPOLLONESHOT;
    ev.data.fd = sock;
    if ( epoll_ctl(m_hEPollIn, EPOLL_CTL_ADD, sock, &ev) < 0 ) return false;
    if ( epoll_ctl(m_hEPollOut, EPOLL_CTL_ADD, sock, &ev) < 0 ) return false;
#endif	
	return true;
}

}//namespace mdk
