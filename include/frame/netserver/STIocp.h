// STIocp.h: interface for the STIocp class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_IOCPMONITOR_H
#define MDK_IOCPMONITOR_H

#ifdef WIN32
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

#include "NetEventMonitor.h"
#include "../../../include/mdk/MemoryPool.h"
#include "../../../include/mdk/Lock.h"

/*
 *	IOCP封装(单线程)
 *	主要是WaitEvent提供超时参数，不占用线程
*/
namespace mdk
{
	//套接字类型
	enum socketType
	{
		stListen = 0,			//监听套接字,只会发生opAccept
			stCommunication = 1,	//通信套接字,只发生opRecv,opSend
	};
	
		
class STIocp : public NetEventMonitor  
{
public:
	enum EventType
	{
		unknow = 0,
			connect = 1,
			close = 2,
			recv = 3,
			send = 4,
			timeout = 5,
			stop = 6,
	};
	typedef struct IO_EVENT
	{
		int sock;
		EventType type;
		int client;
		char *pData;
		unsigned short uDataSize;
	}IO_EVENT;
public:
	MemoryPool m_iocpDataPool;//iocp投递参数池
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
		int sock;
		EventType completiontype;//完成类型1recv 2send
	}IOCP_OVERLAPPED;
public:
	STIocp();
	virtual ~STIocp();
public:
	//开始监听
	bool Start( int nMaxMonitor );
	//停止监听
	bool Stop();
	//增加一个监听对象
	bool AddMonitor( int socket, char* pData, unsigned short dataSize );
	//等待事件，失败返回false,超时sockEvent.type = STIocp::timeout,外部调用了Stop()，sockEvent.type = STIocp::stop
	//timeout超时时间：INFINITE不超时，0不等待，>0超时时间单位(千分之一秒)
	bool WaitEvent( IO_EVENT &sockEvent, int timeout );
	//增加一个接受连接的操作，有连接进来，WaitEvent会返回
	bool AddAccept(int listenSocket);
	//增加一个接收数据的操作，有数据到达，WaitEvent会返回
	bool AddRecv( int socket, char* recvBuf, unsigned short bufSize );
	//增加一个发送数据的操作，发送完成，WaitEvent会返回
	bool AddSend( int socket, char* dataBuf, unsigned short dataSize );
protected:
	int GetError(int sock, WSAOVERLAPPED* pWSAOVERLAPPED)  ;
		
private:
	int m_listenSocket;
	HANDLE m_hCompletPort;//完成端口句柄
	int m_nCPUCount;
	IOCP_OVERLAPPED m_olExit;
};

}//namespace mdk


#endif // MDK_IOCPMONITOR_H
