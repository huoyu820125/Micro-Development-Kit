// NetEventMonitor.h: interface for the NetEventMonitor class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_NETEVENTMONITOR_H
#define MDK_NETEVENTMONITOR_H

#include "../../../include/mdk/Socket.h"
#include "../../../include/mdk/FixLengthInt.h"
#include <string>
#define MAXPOLLSIZE 20000 //最大socket数

namespace mdk
{

typedef struct IOCP_DATA
{
	int64 connectId;
	char *buf;
	unsigned short bufSize;
}IOCP_DATA;

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
	virtual bool AddMonitor( int socket, char* pData, unsigned short dataSize );
	virtual bool AddConnectMonitor( int sock );
	virtual bool AddDataMonitor( int sock, char* pData, unsigned short dataSize );
	virtual bool AddSendableMonitor( int sock, char* pData, unsigned short dataSize );
	//等待事件发生
	virtual bool WaitEvent( void *eventArray, int &count, bool block );

	//增加一个接受连接的操作，有连接进来，WaitEvent会返回
	virtual bool AddAccept(int socket);
	//增加一个接收数据的操作，有数据到达，WaitEvent会返回
	virtual bool AddRecv( int socket, char* pData, unsigned short dataSize );
	//增加一个发送数据的操作，发送完成，WaitEvent会返回
	virtual bool AddSend( int socket, char* pData, unsigned short dataSize );

	//删除一个监听对象从监听列表
	virtual bool DelMonitor( int socket );

	//取得最后的错误
	const char* GetInitError();

protected:
	std::string m_initError;//错误信息
};

}//namespace mdk

#endif // MDK_NETEVENTMONITOR_H
