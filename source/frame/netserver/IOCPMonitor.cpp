// IOCPMonitor.cpp: implementation of the IOCPMonitor class.
//
//////////////////////////////////////////////////////////////////////

#include "../../../include/frame/netserver/IOCPMonitor.h"


#ifdef WIN32
#include "../../../include/frame/netserver/MsWinsockUtil.h"
#pragma comment ( lib, "mswsock.lib" )
#pragma comment ( lib, "ws2_32.lib" )
#endif
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

IOCPMonitor::IOCPMonitor()
:m_iocpDataPool( sizeof(IOCP_OVERLAPPED), 200 )
{
	m_nCPUCount = 0;
#ifdef WIN32
	SYSTEM_INFO sysInfo;
	::GetSystemInfo(&sysInfo);
	m_nCPUCount = sysInfo.dwNumberOfProcessors;
#endif
}

IOCPMonitor::~IOCPMonitor()
{

}

int IOCPMonitor::GetError(int sock, WSAOVERLAPPED* pWSAOVERLAPPED)  
{  
#ifdef WIN32
	DWORD dwTrans;  
	DWORD dwFlags;  
	if(FALSE == WSAGetOverlappedResult(sock, pWSAOVERLAPPED, &dwTrans, FALSE, &dwFlags))  
		return WSAGetLastError();  
	else  
		return ERROR_SUCCESS;  
#endif
	return 0;
}  

bool IOCPMonitor::Start( int nMaxMonitor )
{
#ifdef WIN32
	//启动IOCP监听
	//创建完成端口
	int NumberOfConcurrentThreads = 0;
	if ( 0 < m_nCPUCount ) NumberOfConcurrentThreads = m_nCPUCount * 2 + 2;
	m_hCompletPort = CreateIoCompletionPort( INVALID_HANDLE_VALUE, 
		0, 0, NumberOfConcurrentThreads );
	if ( NULL == m_hCompletPort ) 
	{
		m_initError = "create iocp monitor faild";
		return false;
	}
	return true;

#endif
	return true;
}

bool IOCPMonitor::AddMonitor( int sock, char* pData, unsigned short dataSize )
{
#ifdef WIN32
	//将监听套接字加入IOCP列队
	if ( NULL == CreateIoCompletionPort( (HANDLE)sock, m_hCompletPort, 
		(DWORD)sock, 0 ) ) return false;
	return true;

#endif
	return true;
}

bool IOCPMonitor::WaitEvent( void *eventArray, int &count, bool block )
{
#ifdef WIN32
	IO_EVENT *events = (IO_EVENT*)eventArray;
	count = 0;
	/*
		完成操作上的传输长度
		发现accept操作完成时为0
		发现recv/send操作完成时，记录接收/发送字节数
		如果recv/send操作上的传输长度小于等于0表示套接字已关闭
	*/
	DWORD dwIOSize;
	IOCP_OVERLAPPED *pOverlapped = NULL;//接收完成数据
	int sock;
	if ( !::GetQueuedCompletionStatus( m_hCompletPort, &dwIOSize, 
						(LPDWORD)&sock, (OVERLAPPED**)&pOverlapped,
						INFINITE ) )
	{
		DWORD dwErrorCode = GetLastError();
		if ( ERROR_INVALID_HANDLE == dwErrorCode ) //启动时，未知原因非法句柄，可能是捕获到前一次程序关闭后未处理的完成事件
		{
			try
			{
				if ( NULL != pOverlapped )
				{
					m_iocpDataPool.Free(pOverlapped);
					pOverlapped = NULL;
					return true;
				}
			}catch(...)
			{
			}
			return true;
		}
		
		if ( ERROR_OPERATION_ABORTED == dwErrorCode ) 
		{
			if ( IOCPMonitor::connect == pOverlapped->completiontype ) 
			{
				m_iocpDataPool.Free(pOverlapped);
				pOverlapped = NULL;
				return false;
			}
		}
		if ( IOCPMonitor::connect == pOverlapped->completiontype ) //Accept上的socket关闭，重新投递监听
		{
			AddAccept(sock);
		}
		else//客户端异常断开，拔网线，断电，终止进程
		{
			events[count].connectId = pOverlapped->connectId;
			events[count].type = IOCPMonitor::close;
			count++;
		}
	}
	else if ( IOCPMonitor::close == pOverlapped->completiontype )
	{
		Stop();
		return false;
	}
	else if ( 0 == dwIOSize && IOCPMonitor::recv == pOverlapped->completiontype )
	{
		events[count].connectId = pOverlapped->connectId;
		events[count].type = IOCPMonitor::close;
		count++;
	}
	else//io事件
	{
		if ( IOCPMonitor::connect == pOverlapped->completiontype )
		{
			AddAccept( sock );
		}
		//io事件
		events[count].connectId = pOverlapped->connectId;
		events[count].type = pOverlapped->completiontype;
		events[count].client = pOverlapped->sock;
		events[count].pData = pOverlapped->m_wsaBuffer.buf;
		events[count].uDataSize = (unsigned short)dwIOSize;
		count++;
	}
	m_iocpDataPool.Free(pOverlapped);
	pOverlapped = NULL;
				
	return true;
	
#endif
	return true;
}

bool IOCPMonitor::AddAccept( int listenSocket )
{
#ifdef WIN32
	if ( SOCKET_ERROR == listenSocket ) return false;
	//创建参数
	IOCP_OVERLAPPED *pOverlapped = new (m_iocpDataPool.Alloc())IOCP_OVERLAPPED;
	if ( NULL == pOverlapped ) return false;
	memset( &pOverlapped->m_overlapped, 0, sizeof(OVERLAPPED) );
	pOverlapped->m_dwLocalAddressLength = sizeof(SOCKADDR_IN) + 16;//客户端局域网IP
	pOverlapped->m_dwRemoteAddressLength = sizeof(SOCKADDR_IN) + 16;//客户端外网IP
	memset( pOverlapped->m_outPutBuf, 0, sizeof(SOCKADDR_IN)*2+32 );
	Socket client;
	client.Init( Socket::tcp );
	pOverlapped->sock = client.GetSocket();
	pOverlapped->completiontype = IOCPMonitor::connect;
	//投递接受连接操作
	//AcceptEx 当recvdatalength = 0时，直接返回
	//
	if ( !MsWinsockUtil::m_lpfnAccepteEx( listenSocket,
		client.GetSocket(), 
		pOverlapped->m_outPutBuf, 0,
		pOverlapped->m_dwLocalAddressLength, 
		pOverlapped->m_dwRemoteAddressLength, 
		NULL, &pOverlapped->m_overlapped ) )
	{
		int nErrCode = WSAGetLastError();
		if ( ERROR_IO_PENDING != nErrCode ) 
		{
			m_iocpDataPool.Free(pOverlapped);
			pOverlapped = NULL;
			return false;
		}
	}
	
	return true;
	
#endif
	return true;
}

//增加一个接收数据的操作，有数据到达，WaitEvent会返回
bool IOCPMonitor::AddRecv( int socket, char* pData, unsigned short dataSize )
{
#ifdef WIN32
	IOCP_DATA *pIocpData = (IOCP_DATA *)pData;
	IOCP_OVERLAPPED *pOverlapped = new (m_iocpDataPool.Alloc())IOCP_OVERLAPPED;
	if ( NULL == pOverlapped )return false;
	memset( &pOverlapped->m_overlapped, 0, sizeof(OVERLAPPED) );
	pOverlapped->m_wsaBuffer.buf = pIocpData->buf;
	pOverlapped->m_wsaBuffer.len = pIocpData->bufSize;
	pOverlapped->m_overlapped.Internal = 0;
	pOverlapped->connectId = pIocpData->connectId;
	pOverlapped->sock = socket;
	pOverlapped->completiontype = IOCPMonitor::recv;
	
	DWORD dwRecv = 0;
	DWORD dwFlags = 0;
	//投递数据接收操作
	if ( ::WSARecv( socket, 
		&pOverlapped->m_wsaBuffer, 
		1, &dwRecv, &dwFlags, 
		&pOverlapped->m_overlapped, NULL ) )
	{
		int nErrCode = WSAGetLastError();
		if ( ERROR_IO_PENDING != nErrCode ) 
		{
			m_iocpDataPool.Free(pOverlapped);
			pOverlapped = NULL;
			return false;
		}
	}
	return true;
	
#endif
	return true;
}

//增加一个发送数据的操作，发送完成，WaitEvent会返回
bool IOCPMonitor::AddSend( int socket, char* pData, unsigned short dataSize )
{
#ifdef WIN32
	IOCP_DATA *pIocpData = (IOCP_DATA *)pData;
	IOCP_OVERLAPPED *pOverlapped = new (m_iocpDataPool.Alloc())IOCP_OVERLAPPED;
	if ( NULL == pOverlapped ) return false;
	memset( &pOverlapped->m_overlapped, 0, sizeof(OVERLAPPED) );
	pOverlapped->m_wsaBuffer.buf = pIocpData->buf;
	pOverlapped->m_wsaBuffer.len = pIocpData->bufSize;
	pOverlapped->m_overlapped.Internal = 0;
	pOverlapped->connectId = pIocpData->connectId;
	pOverlapped->sock = socket;
	pOverlapped->completiontype = IOCPMonitor::send;
	
	DWORD dwSend = 0;
	DWORD dwFlags = 0;
	//投递数据接收操作
	if ( ::WSASend( socket, 
		&pOverlapped->m_wsaBuffer, 
		1, &dwSend, dwFlags, 
		&pOverlapped->m_overlapped, NULL ) )
	{
		int nErrCode = WSAGetLastError();
		if ( ERROR_IO_PENDING != nErrCode ) 
		{
			m_iocpDataPool.Free(pOverlapped);
			pOverlapped = NULL;
			return false;
		}
	}
	return true;
	
#endif
	return true;
}

bool IOCPMonitor::Stop()
{
#ifdef WIN32
	memset( &m_stopOverlapped.m_overlapped, 0, sizeof(OVERLAPPED) );
	m_stopOverlapped.m_wsaBuffer.buf = NULL;
	m_stopOverlapped.m_wsaBuffer.len = 0;
	m_stopOverlapped.m_overlapped.Internal = 0;
	m_stopOverlapped.sock = 0;
	m_stopOverlapped.completiontype = IOCPMonitor::close;
	
	::PostQueuedCompletionStatus(m_hCompletPort, 0, 0, (OVERLAPPED*)&m_stopOverlapped );
#endif
	return true;

}

}//namespace mdk
