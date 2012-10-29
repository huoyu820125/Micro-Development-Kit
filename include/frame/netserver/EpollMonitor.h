// EpollMonitor.h: interface for the EpollMonitor class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_EPOLLMONITOR_H
#define MDK_EPOLLMONITOR_H

#include "mdk/Thread.h"
#include "mdk/Task.h"
#include "mdk/Queue.h"
#include "mdk/Signal.h"
#include "NetEventMonitor.h"
#include <map>
#include <vector>

namespace mdk
{

class EpollMonitor : public NetEventMonitor  
{
public:
	enum EventType
	{
		epoll_accept = 0,
		epoll_in = 1,
		epoll_out = 2,
	};
	typedef struct IO_EVENT
	{
		SOCKET sock;
		EventType type;
	}IO_EVENT;
public:
	EpollMonitor();
	virtual ~EpollMonitor();

public:
	//开始监听
	bool Start( int nMaxMonitor );
	//停止监听
	bool Stop();
	//增加一个Accept操作，有新连接产生，WaitEvent会返回
	bool AddAccept( SOCKET sock );
	//增加一个接收数据的操作，有数据到达，WaitEvent会返回
	bool AddRecv( SOCKET sock, char* recvBuf, unsigned short bufSize );
	//增加一个发送数据的操作，发送完成，WaitEvent会返回
	bool AddSend( SOCKET sock, char* dataBuf, unsigned short dataSize );
	//等待事件发生
	bool WaitEvent( void *eventArray, int &count, bool block );
	//删除一个监听对象从监听列表
	bool DelMonitor( SOCKET sock );
	bool AddMonitor( SOCKET sock );
	
protected:
	void SheildSigPipe();//屏蔽SIGPIPE信号，避免进程被该信号关闭
	void* WaitAcceptEvent( void *pData );
	void* WaitInEvent( void *pData );
	bool DelMonitorIn( SOCKET sock );
	void* WaitOutEvent( void *pData );
	bool DelMonitorOut( SOCKET sock );
		
	
private:
	bool m_bStop;
	int m_nMaxMonitor;//监听的socket数量
	SOCKET m_epollExit;//epoll退出控制sock
	Signal m_ioSignal;//有事件信号
	
	int m_hEPollAccept;//新连接事件生产者epoll句柄
	Thread m_acceptThread;//单线程accept，accept recv分离
	Queue* m_acceptEvents;//生产者-消费者，accept完成事件队列
	Signal m_sigWaitAcceptSpace;//accept队列有空间信号
	
	int m_hEPollIn;//EPOLLIN生产者 epoll句柄
	Thread m_pollinThread;//EPOLLIN生产线程，因为epoll_wait是惊群的
	Queue* m_inEvents;//生产者-消费者，EPOLLIN事件队列
	Signal m_sigWaitInSpace;//EPOLLIN队列有空间信号
	
	int m_hEPollOut;//EPOLLOUT生产者 epoll句柄
	Thread m_polloutThread;//EPOLLOUT生产线程，因为epoll_wait是惊群的
	Queue* m_outEvents;//生产者-消费者，EPOLLOUT事件队列
	Signal m_sigWaitOutSpace;//EPOLLOUT队列有空间信号
	
};

}//namespace mdk

#endif // MDK_EPOLLMONITOR_H
