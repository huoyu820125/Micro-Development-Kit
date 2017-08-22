// Socket.cpp: implementation of the Socket class.
//
//////////////////////////////////////////////////////////////////////

#include "../../include/mdk/Socket.h"

#ifdef WIN32
#include <windows.h>
#pragma comment ( lib, "ws2_32.lib" )
#else
#include <netdb.h>
#include <netinet/tcp.h>
#endif
#include <stdio.h>

namespace mdk
{

Socket::Socket()
{
	m_hSocket = INVALID_SOCKET;
	m_bBlock = true;
	m_bOpened = false;//未打开状态
}

Socket::Socket( int hSocket, protocol nProtocolType )
{
	m_hSocket = hSocket;
	m_bBlock = true;
	m_bOpened = false;//未打开状态
	Init(nProtocolType);
	InitPeerAddress();
	InitLocalAddress();
}

Socket::~Socket()
{

}

/*
	功能：Socket初始化(windows专用)
	参数：
		lpwsaData	WSADATA*	[In]	A pointer to a WSADATA structure. If lpwsaData is not equal to NULL, then the address of the WSADATA structure is filled by the call to ::WSAStartup. This function also ensures that ::WSACleanup is called for you before the application terminates.

	返回值：超时返回TRUE，否则返回FALSE
*/
bool Socket::SocketInit(void *lpVoid)
{
#ifdef WIN32
	// initialize Winsock library
	WSADATA *lpwsaData = (WSADATA *)lpVoid;
	WSADATA wsaData;
	if (lpwsaData == NULL)
		lpwsaData = &wsaData;
	
	WORD wVersionRequested = MAKEWORD(1, 1);
	__int32 nResult = WSAStartup(wVersionRequested, lpwsaData);
	if (nResult != 0)
		return false;
	
	if (LOBYTE(lpwsaData->wVersion) != 1 || HIBYTE(lpwsaData->wVersion) != 1)
	{
		WSACleanup();
		return false;
	}
#endif
	return true;
}

void Socket::SocketDestory()
{
#ifdef WIN32
	WSACleanup();
#endif
}

void Socket::GetLastErrorMsg( std::string &strError )
{
	char strErr[1024];
#ifdef WIN32
	LPSTR lpBuffer;
	DWORD dwErrorCode = WSAGetLastError();
	
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwErrorCode,
		LANG_NEUTRAL,
		(LPTSTR)&lpBuffer,
		0,
		NULL );
	sprintf( strErr, "Socket Error(%ld):%s", dwErrorCode, lpBuffer );
	strError = strErr;
	LocalFree(lpBuffer);

#else
	sprintf( strErr, "socket errno(%d):%s\n", errno, strerror(errno) );
	strError = strErr;
#endif
}

bool Socket::InitForIOCP( int hSocket, int listenSock )
{
#ifdef WIN32
	return 0 == setsockopt( hSocket,
		SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&(listenSock), sizeof(listenSock) );
#else
	return true;
#endif
}


//取得套接字句柄
int Socket::GetSocket()
{
	return m_hSocket;
}

void Socket::GetPeerAddress( std::string& strWanIP, int& nWanPort )
{ 
	nWanPort = m_nWanPort;
	strWanIP = m_strWanIP;
	return;
}

bool Socket::InitPeerAddress()
{
	if ( INVALID_SOCKET == m_hSocket ) return false;

	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	socklen_t nSockAddrLen = sizeof(sockAddr);
	if ( SOCKET_ERROR == getpeername( m_hSocket, 
		(sockaddr*)&sockAddr, &nSockAddrLen ) ) return false;
	m_nWanPort = ntohs(sockAddr.sin_port);
	m_strWanIP = inet_ntoa(sockAddr.sin_addr);
	
	return true;
}

void Socket::GetLocalAddress( std::string& strWanIP, int& nWanPort )
{ 
	nWanPort = m_nLocalPort;
	strWanIP = m_strLocalIP;
	return;
}

bool Socket::InitLocalAddress()
{
	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	socklen_t nSockAddrLen = sizeof(sockAddr);
	if ( SOCKET_ERROR == getsockname( m_hSocket, 
		(sockaddr*)&sockAddr, &nSockAddrLen )) return false;
	m_nLocalPort = ntohs(sockAddr.sin_port);
	m_strLocalIP = inet_ntoa(sockAddr.sin_addr);

	return true;
}

/*
	功能：一致性初始化，m_hSocket来至于外部创建，还是内部创建
	参数：
		nProtocolType	__int32		[In]	A particular protocol to be used with the socket that is specific to the indicated address family.
	返回值：成功返回TRUE,失败返回FALSE
*/
bool Socket::Init(protocol nProtocolType)
{
	if ( m_bOpened ) return true;
	if ( m_hSocket == INVALID_SOCKET )
	{
		m_hSocket = socket( PF_INET, nProtocolType, 0 );
		if ( m_hSocket == INVALID_SOCKET ) return false;
	}
	m_bOpened = true;
	
	return m_bOpened;
}

/*
	功能：客户端函数，发起TCP连接
	参数：
		lpszHostAddress	LPCTSTR		[In]	对方Ip地址或域名
		nHostPort		UINT		[In]	对方监听的端口
	返回值：成功返回TRUE,失败返回FALSE
*/
bool Socket::Connect( const char *lpszHostAddress, unsigned short nHostPort, long lSecond )
{
	if ( NULL == lpszHostAddress ) return false;
	//将域名转换为真实IP，如果lpszHostAddress本来就是ip，不影响转换结果
 	char ip[64]; //真实IP
#ifdef WIN32
 	PHOSTENT hostinfo;   
#else
	struct hostent * hostinfo;   
#endif
	strcpy( ip, lpszHostAddress ); 
	if((hostinfo = gethostbyname(lpszHostAddress)) != NULL)   
	{
		strcpy( ip, inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list) ); 
	}

	//使用真实ip进行连接
	sockaddr_in sockAddr;
	memset(&sockAddr,0,sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(ip);
	sockAddr.sin_port = htons( nHostPort );
	SetSendTimeout(lSecond, 0);
	if ( SOCKET_ERROR != connect(m_hSocket, (sockaddr*)&sockAddr, sizeof(sockAddr)) )
	{
		SetSendTimeout(0, 0);
		InitPeerAddress();
		InitLocalAddress();
		return true;
	}
	SetSendTimeout(0, 0);

	return false;
}

//开始网络Service
bool Socket::StartServer( int nPort )
{
	if ( !this->Bind( nPort ) ) return false;
	return this->Listen();
}

//是关闭状态返回true,否则返回false
bool Socket::IsClosed()
{
	return !m_bOpened;
}
/*
	功能：断开socket连接
	逻辑：支持傻瓜式调用，任何时候都可以调用此函数断开链接，变为断开状态
	返回值：永远返回TRUE
*/
void Socket::Close()
{
	if ( INVALID_SOCKET == m_hSocket ) return;
	if ( m_bOpened ) 
	{
		closesocket(m_hSocket);
		m_bOpened = false;
	}
	m_hSocket = INVALID_SOCKET;
	return;
}

/*
	功能：绑定一个socket句柄，让该类对象在这个句柄上进行操作
	逻辑：
		傻瓜式绑定，不管类对象之前是否已经绑定了其它套接字，首先会调用傻瓜式函数close关闭连接，然后在绑定新的套接字，
		如果没有实现调用Detach解除旧绑定，那么旧的绑定sock将丢失
	参数：
		hSocket	int	[In]	要绑定sock句柄
*/
void Socket::Attach(int hSocket)
{
	m_hSocket = hSocket;
	m_bBlock = true;
	m_bOpened = true;//未打开状态
	InitPeerAddress();
	InitLocalAddress();
}

/*
	功能：解除绑定，返回绑定的socket句柄
	返回值：已绑定的socket句柄，可能是一个INVALID_SOCKET，说明之前没有任何绑定
*/
int Socket::Detach()
{
	int hSocket = m_hSocket;
	m_hSocket = INVALID_SOCKET;
	m_bBlock = true;
	m_bOpened = false;//未打开状态
	return hSocket;
}

/*
	功能：接收数据
	参数：
		lpBuf		void*		[Out]	保存接收的数据
		nBufLen		__int32		[Out]	收到数据的长度
		lSecond		long		[In]	超时时间秒
		lMinSecond	long		[In]	超时时间毫秒
	返回值：实际接收到的字节数，超时返回-2，断开连接返回-1，其它错误返回-3
*/
int Socket::Receive( void* lpBuf, int nBufLen, bool bCheckDataLength, long lSecond, long lMinSecond )
{
	if ( TimeOut( lSecond, lMinSecond ) ) return seTimeOut;//超时
	int nResult;
	int nFlag = 0;
	if ( bCheckDataLength ) nFlag = MSG_PEEK;
	nResult = recv(m_hSocket, (char*)lpBuf, nBufLen, nFlag);
	if ( 0 == nResult ) return seSocketClose;//断开连接
	if ( SOCKET_ERROR != nResult ) return nResult;//无异常发生
	
	//socket发生异常
#ifdef WIN32
		int nError = GetLastError();
		if ( WSAEWOULDBLOCK == nError ) return 0;//非阻塞recv返回，无数据可接收
		return seError;
#else
		if ( EAGAIN == errno ) return 0;//非阻塞recv返回，无数据可接收
		return seError;
#endif
}

/*
	功能：调用库函数send发送数据
	参数：
		lpBuf	const void*	[In]	发送的数据
		nBufLen	int		[In]	数据长度
		nFlags	int		[In]	An indicator specifying the way in which the call is made
	返回值：成功实际发送的字节数，失败返回小于0
*/
int Socket::Send( const void* lpBuf, int nBufLen, int nFlags )
{
	int nSendSize = send(m_hSocket, (char*)lpBuf, nBufLen, nFlags);
	if ( 0 > nSendSize ) 
	{
#ifdef WIN32
		int nError = GetLastError();
		if ( WSAEWOULDBLOCK == nError ) return 0;//非阻塞，缓冲已满,返回WSAEWOULDBLOCK本次发送缓冲会被系统清空，即1byte也不会被发送，所以返回实际发送0byte
		return seError;
#else
		if ( EAGAIN == errno ) return 0;//非阻塞，缓冲已满，同上
		return seError;
#endif
	}
	if ( nSendSize <= nBufLen ) 
	{
		return nSendSize;
	}
	return seError;
}

/*
	功能：服务端函数，绑定监听的端口与IP
	参数：
		nSocketPort			UINT		[In]	监听的端口
		lpszSocketAddress	LPCTSTR		[In]	IP
	返回值：成功返回TRUE，否则返回FALSE
*/
bool Socket::Bind( unsigned short nPort, char *strIP )
{
	sockaddr_in sockAddr;
	memset(m_sockAddr,0,sizeof(sockAddr));
	sockaddr_in *pAddr = (sockaddr_in*)m_sockAddr;
	pAddr->sin_family = AF_INET;
	if ( NULL == strIP ) pAddr->sin_addr.s_addr = htonl(INADDR_ANY);
	else
	{
		unsigned long lResult = inet_addr( strIP );
		if ( lResult == INADDR_NONE ) return false;
		pAddr->sin_addr.s_addr = lResult;
	}
	pAddr->sin_port = htons((unsigned short)nPort);

	return (SOCKET_ERROR != bind(m_hSocket, (sockaddr*)pAddr, sizeof(sockAddr)));
}

/*
	功能：服务端函数，开始监听
	参数：
		nConnectionBacklog	__int32	[In]	最大连接数
	返回值：成功返回TRUE，否则返回FALSE
*/
bool Socket::Listen( int nConnectionBacklog )
{ 
	return (SOCKET_ERROR != listen(m_hSocket, nConnectionBacklog)); 
}

/*
	功能：服务端函数，接收客户端连接
	参数：
	rConnectedSocket	Socket	[In]	client socket对象
	返回值：成功返回TRUE，否则返回FALSE
	※注：返回TRUE不表示rConnectedSocket等返回参数值有效，因为如果是非阻塞模式，
	无连接到达，该函数一样返回TRUE，而此时rConnectedSocket对象句柄将指向INVALID_SOCKET
*/
bool Socket::Accept(Socket& rConnectedSocket)
{
	if ( INVALID_SOCKET != rConnectedSocket.m_hSocket ) return false;
	rConnectedSocket.m_hSocket = accept(m_hSocket, NULL, NULL);
	if ( INVALID_SOCKET == rConnectedSocket.m_hSocket )
	{
#ifdef WIN32
		if ( WSAEWOULDBLOCK == GetLastError() ) return true;//非阻塞返回，无连接请求到达
#else
		if ( EAGAIN == errno ) return true;//非阻塞返回，无连接请求到达
#endif
		return false;//socket异常
	}
	rConnectedSocket.m_bOpened = true;
	rConnectedSocket.InitPeerAddress();
	rConnectedSocket.InitLocalAddress();
	return true;
}

/*
	功能：套接字设置，端口重用等
	参数：
		nOptionName		__int32		[In]	The socket option for which the value is to be set
		lpOptionValue	const void*	[In]	A pointer to the buffer in which the value for the requested option is supplied
		nOptionLen		__int32		[In]	lpOptionValue的大小
		nLevel			__int32		[In]	The level at which the option is defined; the supported levels include SOL_SOCKET and IPPROTO_TCP. See the Windows Sockets 2 Protocol-Specific Annex (a separate document included with the Platform SDK) for more information on protocol-specific levels
	返回值：成功返回TRUE，否则返回FALSE
*/
bool Socket::SetSockOpt(
						   int nOptionName, 
						   const void* lpOptionValue, 
						   int nOptionLen, 
						   int nLevel)
{ 
	return ( SOCKET_ERROR != setsockopt( 
		m_hSocket, 
		nLevel, 
		nOptionName, 
		(char *)lpOptionValue, 
		nOptionLen)); 
}

/*
	功能：超时测试
	参数：
		lSecond		long	[In]	超时设置秒
		lMinSecond	long	[In]	超时设置毫秒
	返回值：超时返回TRUE，否则返回FALSE
*/
bool Socket::TimeOut( long lSecond, long lMinSecond )
{
	if ( lSecond <= 0 && lMinSecond <= 0 ) return false;
	//接收超时设置
	timeval outtime;//超时结构
	outtime.tv_sec = lSecond;
	outtime.tv_usec =lMinSecond;
	int nSelectRet;
#ifdef WIN32
	FD_SET readfds = { 1, m_hSocket };
	nSelectRet=::select( 0, &readfds, NULL, NULL, &outtime ); //检查可读状态
#else
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(m_hSocket, &readfds);
	nSelectRet=::select(m_hSocket+1, &readfds, NULL, NULL, &outtime); //检查可读状态
#endif

	if ( SOCKET_ERROR == nSelectRet ) 
	{
		return true;
	}
	if ( 0 == nSelectRet ) //超时发生，无可读数据 
	{
		return true;
	}

	return false;
}

/*
	功能：阻塞方式设置
	参数：
		bWait		bool	[In]	TRUE阻塞，FALSE非阻塞
	返回值：成功返回TRUE，否则返回FALSE
*/
bool Socket::SetSockMode( bool bWait )
{
#ifdef WIN32
	m_bBlock = bWait;
	unsigned long ul = 1;
	if ( m_bBlock ) ul = 0;
	else ul = 1;
	int ret = ioctlsocket( m_hSocket, FIONBIO, (unsigned long*)&ul );
	if ( ret == SOCKET_ERROR )
	{
		return false;
	} 
#else
	m_bBlock = bWait;
	int flags = fcntl( m_hSocket, F_GETFL, 0 ); //取得当前状态设置
	if ( !m_bBlock ) 
		fcntl( m_hSocket, F_SETFL, flags|O_NONBLOCK );//追加非阻塞标志
	else 
		fcntl( m_hSocket, F_SETFL, flags&(~O_NONBLOCK&0xffffffff) );//去掉非阻塞标志，阻塞状态
#endif
	return true;
}

/*
	功能：调用库函数sendto发送UDP数据
	参数：
	    strIP	const char*	[In]	接收方IP
		nPort	int		[In]	接收方端口
		lpBuf	const void*	[In]	发送的数据
		nBufLen	int		[In]	数据长度
		nFlags	int		[In]	An indicator specifying the way in which the call is made
	返回值：成功返回实际发送字节数，可能小于请求发送的长度，失败返回常量SOCKET_ERROR,调用WSAGetLastError函数可获取错误信息
*/
int Socket::SendTo( const char *strIP, int nPort, const void* lpBuf, int nBufLen, int nFlags )
{
	sockaddr_in sockAddr;
	memset(&sockAddr,0,sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(nPort);
	sockAddr.sin_addr.s_addr = inet_addr(strIP);

	int ret = sendto( m_hSocket, (const char*)lpBuf, nBufLen, nFlags, 
		(sockaddr*)&sockAddr, sizeof(sockaddr));
	if (ret < 0) ret = -1;
	return ret;
}

/*
	功能：调用库函数recvFrom接收UDP数据
	参数：
		lpBuf		void*		[Out]	保存接收的数据
		nBufLen		int			[Out]	收到数据的长度
		strFromIP	string&		[Out]	发送方IP
		nFromPort	int&		[Out]	发送方端口
		lSecond		long		[In]	超时时间秒
		lMinSecond	long		[In]	超时时间毫秒
		返回值：实际接收到的字节数，超时返回0
*/
int Socket::ReceiveFrom( char* lpBuf, int nBufLen, std::string &strFromIP, int &nFromPort, bool bCheckDataLength, long lSecond, long lMinSecond )
{
	strFromIP = "";
	nFromPort = -1;
	if ( 0 >= nBufLen ) return 0;
	sockaddr_in sockAddr;
	socklen_t nAddrLen = sizeof(sockaddr);
	/* waiting for receive data */
	int nResult;
	int nFlag = 0;
	while ( true )
	{
		if ( TimeOut( lSecond, lMinSecond ) ) return seTimeOut;
		if ( bCheckDataLength )nFlag = MSG_PEEK;
		nResult = recvfrom(m_hSocket, lpBuf, nBufLen, nFlag, (sockaddr*)&sockAddr, &nAddrLen);
		if ( nAddrLen > 0 ) GetAddress(&sockAddr, strFromIP, nFromPort);
		if ( SOCKET_ERROR == nResult ) //socket发生异常
		{
#ifndef WIN32
			if ( EAGAIN == errno ) return 0;//非阻塞recv返回，无数据可接收
			return seError;
#else
			int nError = GetLastError();
			if ( 0 == nError )//之前存在失败的udp发送
			{
				if ( MSG_PEEK == nFlag )//没有删除接收缓冲，从接收缓冲将消息删除
				{
					recvfrom(m_hSocket, lpBuf, nBufLen, 0, (sockaddr*)&sockAddr, &nAddrLen);
				}
				continue;
			}
			if ( WSAEWOULDBLOCK == nError ) return 0;//非阻塞recv返回，无数据可接收
			return seError;
#endif
		}		
		break;
	}
	return nResult;
}

void Socket::GetAddress( const sockaddr_in *sockAddr, std::string &strIP, int &nPort )
{
	nPort = ntohs(sockAddr->sin_port);
	strIP = inet_ntoa(sockAddr->sin_addr);
}

char* Socket::HostName2IP( char *hostname )
{
	if ( NULL == hostname ) return "";

#ifdef WIN32
	PHOSTENT hostinfo;   
#else
	struct hostent * hostinfo;   
#endif
	if ( NULL != (hostinfo = gethostbyname(hostname)) )
	{
		return inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list); 
	}

	return "";
}

bool Socket::SetNoDelay( bool yes )
{
	int opt = yes?1:0;
	if ( -1 == setsockopt(m_hSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetNoDelay( int sock, bool yes )
{
	int opt = yes?1:0;
	if ( -1 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetSendBufSize( int buffsize )
{
	if ( -1 == setsockopt(m_hSocket, SOL_SOCKET, SO_SNDBUF, (char*)&buffsize, sizeof(buffsize)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetSendBufSize( int sock, int buffsize )
{
	if ( -1 == setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&buffsize, sizeof(buffsize)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetRecvBufSize( int buffsize )
{
	if ( -1 == setsockopt(m_hSocket, SOL_SOCKET, SO_RCVBUF, (char*)&buffsize, sizeof(buffsize)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetRecvBufSize( int sock, int buffsize )
{
	if ( -1 == setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&buffsize, sizeof(buffsize)) )
	{
		return false;
	}

	return true;
}

bool Socket::SetSendTimeout( long sec, long usec )
{
	struct timeval timeo = {sec, usec};
	socklen_t len = sizeof(timeo);
	if ( -1 == setsockopt(m_hSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeo, len) )
	{
		return false;
	}

	return true;
}

bool Socket::SetSendTimeout( int sock, long sec, long usec )
{
	struct timeval timeo = {sec, usec};
	socklen_t len = sizeof(timeo);
	if ( -1 == setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeo, len) )
	{
		return false;
	}

	return true;
}

bool Socket::SetRecvTimeout( long sec, long usec )
{
	struct timeval timeo = {sec, usec};
	socklen_t len = sizeof(timeo);
	if ( -1 == setsockopt(m_hSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeo, len) )
	{
		return false;
	}

	return true;
}

bool Socket::SetRecvTimeout( int sock, long sec, long usec )
{
	struct timeval timeo = {sec, usec};
	socklen_t len = sizeof(timeo);
	if ( -1 == setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeo, len) )
	{
		return false;
	}

	return true;
}


}//namespace mdk
