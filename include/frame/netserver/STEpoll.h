// STEpoll.h: interface for the STEpoll class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_EPOLLMONITOR_H
#define MDK_EPOLLMONITOR_H

#include "NetEventMonitor.h"
#include <map>
#ifndef WIN32
#include <sys/epoll.h>
#define INFINITE -1
#endif
/*
 *	Epoll封装(单线程)
 *	主要是WaitEvent提供超时参数，不占用线程
 */
namespace mdk
{

	class STEpoll : public NetEventMonitor
{
public:
	STEpoll();
	virtual ~STEpoll();

public:
	//开始监听
	bool Start( int nMaxMonitor );
	//停止监听
	bool Stop();
	//等待事件，返回有事件的socket数量，-1表示失败
	//timeout超时时间：INFINITE不超时，0不等待，>0超时时间单位(千分之一秒)
	//※不可多线程调用，m_events内容会被覆盖
	int WaitEvent( int timeout );
	//得到第i个事件的句柄,发现epoll退出信号，返回INVALID_SOCKET
	int GetSocket( int i );
	//第i个socket有接收可连接
	bool IsAcceptAble( int i );
	//第i个socket可读
	bool IsReadAble( int i );
	//第i个socket可写
	bool IsWriteAble( int i );

	//增加一个Accept操作，有新连接产生，WaitEvent会返回
	bool AddAccept( int sock );
	bool AddMonitor( int socket, char* pData, unsigned short dataSize );
	//增加一个IO操作，可IO时WaitEvent会返回
	bool AddIO( int sock, bool read, bool write );
	//等待事件发生
	//删除一个监听对象从监听列表
	bool DelMonitor( int sock );
protected:
	void SheildSigPipe();//屏蔽SIGPIPE信号，避免进程被该信号关闭
		
	
private:
	bool m_bStop;
	int m_nMaxMonitor;//监听的socket数量
	int m_epollExit;//epoll退出控制sock
	
	int m_hEpoll;//EPOLLIN生产者 epoll句柄
	std::map<int,int> m_listenSockets;
#ifndef WIN32
	epoll_event *m_events;	//epoll事件
#endif
};

}//namespace mdk

#endif // MDK_EPOLLMONITOR_H
