// NetEventMonitor.h: interface for the NetEventMonitor class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_NETEVENTMONITOR_H
#define MDK_NETEVENTMONITOR_H

#include "mdk/Socket.h"
#include <string>
#define MAXPOLLSIZE 20000 //最大socket数

namespace mdk
{

class NetEventMonitor
{
public:
	NetEventMonitor();
	virtual ~NetEventMonitor();

public:
	//开始监听
	virtual bool Start( int nMaxMonitor ) = 0;
	//停止监听
	virtual bool Stop();
	//增加一个监听对象到监听列表
	virtual bool AddMonitor( SOCKET socket );
	//等待事件发生
	virtual bool WaitEvent( void *eventArray, int &count, bool block );

	//增加一个接受连接的操作，有连接进来，WaitEvent会返回
	virtual bool AddAccept(SOCKET socket);
	//增加一个接收数据的操作，有数据到达，WaitEvent会返回
	virtual bool AddRecv( SOCKET socket, char* recvBuf, unsigned short bufSize );
	//增加一个发送数据的操作，发送完成，WaitEvent会返回
	virtual bool AddSend( SOCKET socket, char* dataBuf, unsigned short dataSize );

	//删除一个监听对象从监听列表
	virtual bool DelMonitor( SOCKET socket );

	//取得最后的错误
	const char* GetInitError();

protected:
	std::string m_initError;//错误信息
};

}//namespace mdk

#endif // MDK_NETEVENTMONITOR_H
