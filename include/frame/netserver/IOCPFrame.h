// IOCPFrame.h: interface for the IOCPFrame class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_IOCPFRAME_H
#define MDK_IOCPFRAME_H

#include "NetEngine.h"

namespace mdk
{

class IOCPFrame : public NetEngine  
{
public:
	IOCPFrame();
	virtual ~IOCPFrame();

protected:
	//网络事件监听线程
	void* NetMonitor( void* );
	//接收数据，连接关闭时返回false，因具体响应器差别，需要派生类中实现
	connectState RecvData( NetConnect *pConnect, char *pData, unsigned short uSize );
	//发送数据
	connectState SendData(NetConnect *pConnect, unsigned short uSize);
	SOCKET ListenPort(int port);//监听一个端口,返回创建的套接字
	bool MonitorConnect(NetConnect *pConnect);//监听连接
};

}//namespace mdk

#endif // MDK_IOCPFRAME_H
