#include <assert.h>

#include "frame/netserver/NetServer.h"
#include "frame/netserver/NetEngine.h"
#include "frame/netserver/NetHost.h"
#include "frame/netserver/IOCPFrame.h"
#include "frame/netserver/EpollFrame.h"


namespace mdk
{

NetServer::NetServer()
{
#ifdef WIN32
	m_pNetCard = new mdk::IOCPFrame;
#else
	m_pNetCard = new mdk::EpollFrame;
#endif
	m_pNetCard->m_pNetServer = this;
	m_bStop = true;
}
 
NetServer::~NetServer()
{
}

void* NetServer::TMain(void* pParam)
{
	Main(pParam);
	return 0;
}

/**
 * 客户接口，对所有可见
 * 运行服务器
 * 默认逻辑按一下顺序启动模块
 * 硬盘、显卡、CPU、网卡
 * 
 */
const char* NetServer::Start()
{
	if ( NULL == this->m_pNetCard ) return "no class NetEngine object";
	if ( !m_pNetCard->Start() ) return m_pNetCard->GetInitError();
	m_bStop = false;
	m_mainThread.Run(Executor::Bind(&NetServer::TMain), this, NULL);
	return NULL;
}

void NetServer::WaitStop()
{
	if ( NULL == this->m_pNetCard ) return;
	m_pNetCard->WaitStop();
	m_mainThread.WaitStop();
}

/**
 * 客户接口，对所有可见
 * 关闭服务器
 */
void NetServer::Stop()
{
	if ( NULL != this->m_pNetCard ) m_pNetCard->Stop();
	m_bStop = true;
	m_mainThread.Stop( 3000 );
}

//服务器正常返回true，否则返回false
bool NetServer::IsOk()
{
	return !m_bStop;
}

//设置平均连接数
void NetServer::SetAverageConnectCount(int count)
{
	m_pNetCard->SetAverageConnectCount(count);
}

//设置心跳时间
void NetServer::SetHeartTime( int nSecond )
{
	m_pNetCard->SetHeartTime(nSecond);
}

//设置网络IO线程数量
void NetServer::SetIOThreadCount(int nCount)
{
	m_pNetCard->SetIOThreadCount(nCount);
}

//设置工作线程数
void NetServer::SetWorkThreadCount(int nCount)
{
	m_pNetCard->SetWorkThreadCount(nCount);
}

//监听端口
bool NetServer::Listen(int port)
{
	m_pNetCard->Listen(port);
	return true;
}

//连接其它IP
bool NetServer::Connect(const char *ip, int port)
{
	m_pNetCard->Connect(ip, port);
	return true;
}

//向某组连接广播消息
void NetServer::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount )
{
	m_pNetCard->BroadcastMsg( recvGroupIDs, recvCount, msg, msgsize, filterGroupIDs, filterCount );
}

//向某主机发送消息
void NetServer::SendMsg( int hostID, char *msg, int msgsize )
{
	m_pNetCard->SendMsg(hostID, msg, msgsize);
}

/*
	关闭与主机的连接
 */
void NetServer::CloseConnect( int hostID )
{
	m_pNetCard->CloseConnect( hostID );
}

}  // namespace mdk
