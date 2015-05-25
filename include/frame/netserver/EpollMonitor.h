// EpollMonitor.h: interface for the EpollMonitor class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_EPOLLMONITOR_H
#define MDK_EPOLLMONITOR_H

#include "../../../include/mdk/Thread.h"
#include "../../../include/mdk/Task.h"
#include "../../../include/mdk/Queue.h"
#include "../../../include/mdk/Signal.h"
#include "../../../include/mdk/Lock.h"
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
		int sock;
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
	//增加一个Accept操作，有新连接产生
	bool AddAccept( int sock );
	//增加一个接收数据的操作，有数据到达
	bool AddRecv( int sock, char* pData, unsigned short dataSize );
	//增加一个发送数据的操作，发送完成
	bool AddSend( int sock, char* pData, unsigned short dataSize );
	//等待事件发生
	//删除一个监听对象从监听列表
	bool DelMonitor( int sock );
	
	bool AddConnectMonitor( int sock );
	bool AddDataMonitor( int sock, char* pData, unsigned short dataSize );
	bool AddSendableMonitor( int sock, char* pData, unsigned short dataSize );
	bool WaitConnect( void *eventArray, int &count, int timeout );
	bool WaitData( void *eventArray, int &count, int timeout );
	bool WaitSendable( void *eventArray, int &count, int timeout );
	bool IsStop( int64 connectId );

protected:
	void SheildSigPipe();//屏蔽SIGPIPE信号，避免进程被该信号关闭
	bool DelMonitorIn( int sock );
	bool DelMonitorOut( int sock );
	
private:
	bool m_bStop;
	int m_nMaxMonitor;//监听的socket数量
	int m_epollExit;//epoll退出控制sock
	int m_hEPollAccept;//新连接事件生产者epoll句柄
	int m_hEPollIn;//EPOLLIN生产者 epoll句柄
	int m_hEPollOut;//EPOLLOUT生产者 epoll句柄
};

}//namespace mdk

#endif // MDK_EPOLLMONITOR_H
