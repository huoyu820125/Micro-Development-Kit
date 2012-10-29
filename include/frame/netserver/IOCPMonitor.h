// IOCPMonitor.h: interface for the IOCPMonitor class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_IOCPMONITOR_H
#define MDK_IOCPMONITOR_H

#ifdef WIN32
//#define   _WIN32_WINNT   0x0400 
#include <winsock2.h>  
#include <mswsock.h>
#include <windows.h>
#else
typedef int SOCKADDR_IN;
typedef int WSAOVERLAPPED;
typedef int WSABUF;
typedef int HANDLE;
typedef int OVERLAPPED;
#endif

#include "frame/netserver/NetEventMonitor.h"
#include "mdk/MemoryPool.h"
#include "mdk/Lock.h"


namespace mdk
{
	//套接字类型
	enum socketType
	{
		stListen = 0,			//监听套接字,只会发生opAccept
			stCommunication = 1,	//通信套接字,只发生opRecv,opSend
	};
	
		
class IOCPMonitor : public NetEventMonitor  
{
public:
	enum EventType
	{
		unknow = 0,
			connect = 1,
			close = 2,
			recv = 3,
			send = 4,
	};
	typedef struct IO_EVENT
	{
		SOCKET sock;
		EventType type;
		SOCKET client;
		char *pData;
		unsigned short uDataSize;
	}IO_EVENT;
public:
	MemoryPool m_iocpDataPool;//iocp投递参数池
//	Mutex m_lockIocpDataPool;
	typedef struct IOCP_OVERLAPPED
	{
		/**
		 * OVERLAPPED类型指针
		 * 指向完成操作参数
		 * 传递给AcceptEx()的最后一个参数
		 * 传递给WSARecv()的第6个参数
		 * GetQueuedCompletionStatus()返回的第4个参数
		 */
		OVERLAPPED m_overlapped;
		/**
		 * 指向存有连接进来的客户端局域网和外网地址的内存
		 * 必须使用动态分配的内存块
		 * 传递给AcceptEx()的第3个参数
		 * 
		 */
		char m_outPutBuf[sizeof(SOCKADDR_IN)*2+32];
		/**
		 * 客户端局域网IP信息长度
		 * 传递给AcceptEx()的第5个参数
		 */
		unsigned long m_dwLocalAddressLength;
		/**
		 * 客户端外网IP信息长度
		 * 传递给AcceptEx()的第6个参数
		 */
		unsigned long m_dwRemoteAddressLength;
		WSABUF m_wsaBuffer;//WSARecv接收缓冲数据,传递给WSARecv()的第2个参数
		SOCKET sock;
		EventType completiontype;//完成类型1recv 2send
	}IOCP_OVERLAPPED;
public:
	IOCPMonitor();
	virtual ~IOCPMonitor();
public:
	//开始监听
	bool Start( int nMaxMonitor );
	//增加一个监听对象
	bool AddMonitor( SOCKET socket );
	//等待事件发生,block无作用
	bool WaitEvent( void *eventArray, int &count, bool block );
	//增加一个接受连接的操作，有连接进来，WaitEvent会返回
	bool AddAccept(SOCKET listenSocket);
	//增加一个接收数据的操作，有数据到达，WaitEvent会返回
	bool AddRecv( SOCKET socket, char* recvBuf, unsigned short bufSize );
	//增加一个发送数据的操作，发送完成，WaitEvent会返回
	bool AddSend( SOCKET socket, char* dataBuf, unsigned short dataSize );
protected:
	int GetError(SOCKET sock, WSAOVERLAPPED* pWSAOVERLAPPED)  ;
		
private:
	SOCKET m_listenSocket;
	HANDLE m_hCompletPort;//完成端口句柄
	int m_nCPUCount;
};

}//namespace mdk


#endif // MDK_IOCPMONITOR_H
