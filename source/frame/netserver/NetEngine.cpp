
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#define strnicmp strncasecmp
#endif

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <assert.h>
#include <time.h>

#include "mdk/Socket.h"

#include "frame/netserver/NetEngine.h"
#include "frame/netserver/NetConnect.h"
#include "frame/netserver/NetEventMonitor.h"
#include "frame/netserver/NetServer.h"
#include "mdk/atom.h"
#include "mdk/MemoryPool.h"

using namespace std;
namespace mdk
{

void XXSleep( long lSecond )
{
#ifndef WIN32
	usleep( lSecond * 1000 );
#else
	Sleep( lSecond );
#endif
}
	
NetEngine::NetEngine()
{
	m_stop = true;//停止标志
	m_startError = "";
	m_nHeartTime = 0;//心跳间隔(S)
	m_nReconnectTime = 0;//默认不自动重连
	m_pNetMonitor = NULL;
	m_ioThreadCount = 16;//网络io线程数量
	m_workThreadCount = 16;//工作线程数量
	m_pNetServer = NULL;
	m_averageConnectCount = 5000;
}

NetEngine::~NetEngine()
{
	Stop();
}

//设置平均连接数
void NetEngine::SetAverageConnectCount(int count)
{
	m_averageConnectCount = count;
}

//设置心跳时间
void NetEngine::SetHeartTime( int nSecond )
{
	m_nHeartTime = nSecond;
}

//设置自动重连时间,小于等于0表示不自动重连
void NetEngine::SetReconnectTime( int nSecond )
{
	m_nReconnectTime = nSecond;
}

//设置网络IO线程数量
void NetEngine::SetIOThreadCount(int nCount)
{
	m_ioThreadCount = nCount;//网络io线程数量
}

//设置工作线程数
void NetEngine::SetWorkThreadCount(int nCount)
{
	m_workThreadCount = nCount;//工作线程数量
}

/**
 * 开始引擎
 * 成功返回true，失败返回false
 */
bool NetEngine::Start()
{
	if ( !m_stop ) return true;
	m_stop = false;	
	m_pConnectPool = new MemoryPool( sizeof(NetConnect), m_averageConnectCount * 2 );
	Socket::SocketInit();
	if ( !m_pNetMonitor->Start( MAXPOLLSIZE ) ) 
	{
		m_startError = m_pNetMonitor->GetInitError();
		Stop();
		return false;
	}
	m_workThreads.Start( m_workThreadCount );
	int i = 0;
	for ( ; i < m_ioThreadCount; i++ ) m_ioThreads.Accept( Executor::Bind(&NetEngine::NetMonitorTask), this, NULL );
	m_ioThreads.Start( m_ioThreadCount );
	if ( !ListenAll() )
	{
		Stop();
		return false;
	}
	ConnectAll();
	return m_mainThread.Run( Executor::Bind(&NetEngine::Main), this, 0 );
}

void* NetEngine::NetMonitorTask( void* pParam)
{
	return NetMonitor( pParam );
}

//等待停止
void NetEngine::WaitStop()
{
	m_mainThread.WaitStop();
}

//停止引擎
void NetEngine::Stop()
{
	if ( m_stop ) return;
	m_stop = true;
	m_pNetMonitor->Stop();
	m_mainThread.Stop( 3 );
	m_ioThreads.Stop();
	m_workThreads.Stop();
	Socket::SocketDestory();
	if ( NULL != m_pConnectPool )
	{
		delete m_pConnectPool;
		m_pConnectPool = NULL;
	}
}

//主线程
void* NetEngine::Main(void*)
{
	time_t lastConnect = time(NULL);
	time_t curTime = time(NULL);
	while ( !m_stop ) 
	{
		curTime = time(NULL);
		HeartMonitor();
		if ( 0 < m_nReconnectTime && m_nReconnectTime <= curTime - lastConnect )
		{
			lastConnect = curTime;
			ConnectAll();
		}
		XXSleep( 10000 );
	}
	return NULL;
}

//心跳线程
void NetEngine::HeartMonitor()
{
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	NetConnect *pConnect;
	time_t tCurTime = 0;
	/*	
		创建一个临时的释放列表保存要释放的对象，等遍历结束1次性导入等待释放对象列表
		避免在循环中因为重复的为等待释放列表的访问而加锁解所
	 */
	ReleaseList releaseList;
	tCurTime = time( NULL );
	time_t tLastHeart;
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin(); m_nHeartTime > 0 && it != m_connectList.end(); )//心跳时间<=0无心跳机制,或遍历完成
	{
		pConnect = it->second;
		if ( !pConnect->m_host.IsServer() ) //服务连接 ，不检查心跳
		{
			it++;
			continue;
		}
		//检查心跳
		tLastHeart = pConnect->GetLastHeart();
		if ( tCurTime < tLastHeart || tCurTime - tLastHeart < m_nHeartTime )//有心跳
		{
			it++;
			continue;
		}
		//无心跳/连接已断开，强制断开连接，加入释放列表
		CloseConnect( it );
		releaseList.push_back( pConnect );
		it = m_connectList.begin();
	}
	lock.Unlock();
	//////////////////////////////////////////////////////////////////////////
	//释放连接
	//将releaseList导入m_closedConnects
	ReleaseList::iterator itRelease;
	AutoLock lockRelease( &m_closedConnectsMutex );
	for ( itRelease = releaseList.begin(); itRelease != releaseList.end(); itRelease++ )
	{
		m_closedConnects.push_back( *itRelease );
	}
	//释放连接
	for ( itRelease = m_closedConnects.begin(); itRelease != m_closedConnects.end(); )
	{
		pConnect = *itRelease;
		if ( !pConnect->IsFree() )//不是自由状态，不可以释放
		{
			itRelease++;
			continue;
		}
		//释放一个对象
		m_closedConnects.erase( itRelease );
		itRelease = m_closedConnects.begin();
		m_pConnectPool->Free(pConnect);
		pConnect = NULL;
	}
}

//关闭一个连接
NetConnect* NetEngine::CloseConnect( ConnectList::iterator it )
{
	/*
	   必须先删除再关闭，顺序不能换，
	   避免关闭后，erase前，正好有client连接进来，
	   系统立刻就把该连接分配给新client使用，造成新client在插入m_connectList时失败
	*/
	NetConnect *pConnect = it->second;
	m_connectList.erase( it );
	pConnect->GetSocket()->Close();
	pConnect->m_bConnect = false;
	pConnect->WorkAccess();
	//执行业务NetServer::OnClose();
	m_workThreads.Accept( Executor::Bind(&NetEngine::CloseWorker), this, pConnect);
	return pConnect;
}

bool NetEngine::OnConnect( SOCKET sock, bool isConnectServer )
{
	NetConnect *pConnect = new (m_pConnectPool->Alloc())NetConnect(sock, isConnectServer, m_pNetMonitor, this);
	if ( NULL == pConnect ) 
	{
		closesocket(sock);
		return false;
	}
	AutoLock lock( &m_connectsMutex );//必须在监听前加锁，否则，可能m_connectList.insert之前，其它线程就监听到OnData，无法在m_connectList中找到要使用的NetConnect对象
	//监听连接
	if ( !MonitorConnect(pConnect) )
	{
		closesocket(sock);
		m_pConnectPool->Free(pConnect);
		pConnect = NULL;
		return false;
	}
	//加入管理列表
	pConnect->RefreshHeart();
	pConnect->WorkAccess();
	pair<ConnectList::iterator, bool> ret = m_connectList.insert( ConnectList::value_type(pConnect->GetSocket()->GetSocket(),pConnect) );
	lock.Unlock();
	//执行业务
	m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectWorker), this, pConnect );
	return true;
}

void* NetEngine::ConnectWorker( NetConnect *pConnect )
{
	m_pNetServer->OnConnect( &pConnect->m_host );
	pConnect->WorkFinished();
	return 0;
}

void NetEngine::OnClose( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return;//底层已经主动断开
	NetConnect *pConnect = CloseConnect( itNetConnect );
	lock.Unlock();

	AutoLock lockRelease( &m_closedConnectsMutex );
	m_closedConnects.push_back( pConnect );
}

void* NetEngine::CloseWorker( NetConnect *pConnect )
{
	SetServerClose(pConnect);//连接的服务断开
	m_pNetServer->OnCloseConnect( &pConnect->m_host );
	pConnect->WorkFinished();
	return 0;
}

connectState NetEngine::OnData( SOCKET sock, char *pData, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);//client列表里查找
	if ( itNetConnect == m_connectList.end() ) return cs;//底层已经断开

	NetConnect *pConnect = itNetConnect->second;
	pConnect->RefreshHeart();
	pConnect->WorkAccess();
	lock.Unlock();//确保业务层占有对象后，HeartMonitor()才有机会检查pConnect的状态
	try
	{
		cs = RecvData( pConnect, pData, uSize );//派生类实现
		if ( unconnect == cs )
		{
			pConnect->WorkFinished();
			OnClose( sock );
			return cs;
		}
		if ( 0 < AtomAdd(&pConnect->m_nReadCount, 1) ) //避免并发读
		{
			pConnect->WorkFinished();
			return cs;
		}
		//执行业务NetServer::OnMsg();
		m_workThreads.Accept( Executor::Bind(&NetEngine::MsgWorker), this, pConnect);
	}catch( ... ){}
	return cs;
}

void* NetEngine::MsgWorker( NetConnect *pConnect )
{
	for ( ; !m_stop; )
	{
		pConnect->m_nReadCount = 1;
		m_pNetServer->OnMsg( &pConnect->m_host );//无返回值，避免框架逻辑依赖于客户实现
		if ( !pConnect->m_bConnect ) break;
		if ( pConnect->IsReadAble() ) continue;
		if ( 1 == AtomDec(&pConnect->m_nReadCount,1) ) break;//避免漏接收
	}
	pConnect->WorkFinished();
	return 0;
}

connectState NetEngine::RecvData( NetConnect *pConnect, char *pData, unsigned short uSize )
{
	return unconnect;
}

//关闭一个连接
void NetEngine::CloseConnect( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find( sock );
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	NetConnect *pConnect = CloseConnect( itNetConnect );
				
	AutoLock lockRelease( &m_closedConnectsMutex );
	m_closedConnects.push_back( pConnect );
}

//响应发送完成事件
connectState NetEngine::OnSend( SOCKET sock, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return cs;//底层已经主动断开
	NetConnect *pConnect = itNetConnect->second;
	pConnect->WorkAccess();
	lock.Unlock();//确保业务层占有对象后，HeartMonitor()才有机会检查pConnect的状态
	try
	{
		if ( pConnect->m_bConnect ) cs = SendData(pConnect, uSize);
	}
	catch(...)
	{
	}
	pConnect->WorkFinished();
	return cs;
	
}

connectState NetEngine::SendData(NetConnect *pConnect, unsigned short uSize)
{
	return unconnect;
}

bool NetEngine::Listen(int port)
{
	AutoLock lock(&m_listenMutex);
	pair<map<int,SOCKET>::iterator,bool> ret 
		= m_serverPorts.insert(map<int,SOCKET>::value_type(port,INVALID_SOCKET));
	map<int,SOCKET>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ListenPort(port);
	if ( INVALID_SOCKET == it->second ) return false;
	return true;
}

SOCKET NetEngine::ListenPort(int port)
{
	return INVALID_SOCKET;
}

bool NetEngine::ListenAll()
{
	bool ret = true;
	AutoLock lock(&m_listenMutex);
	map<int,SOCKET>::iterator it = m_serverPorts.begin();
	char strPort[256];
	string strFaild;
	for ( ; it != m_serverPorts.end(); it++ )
	{
		if ( INVALID_SOCKET != it->second ) continue;
		it->second = ListenPort(it->first);
		if ( INVALID_SOCKET == it->second ) 
		{
			sprintf( strPort, "%d", it->first );
			strFaild += strPort;
			strFaild += " ";
			ret = false;
		}
	}
	if ( !ret ) m_startError += "listen port:" + strFaild + "faild";
	return ret;
}

bool addrToI64(uint64 &addr64, const char* ip, int port)
{
	unsigned char addr[8];
	int nIP[4];
	sscanf(ip, "%d.%d.%d.%d", &nIP[0], &nIP[1], &nIP[2], &nIP[3]);
	addr[0] = nIP[0];
	addr[1] = nIP[1];
	addr[2] = nIP[2];
	addr[3] = nIP[3];
	char checkIP[25];
	sprintf(checkIP, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	if ( 0 != strcmp(checkIP, ip) ) return false;
	memcpy( &addr[4], &port, 4);
	memcpy(&addr64, addr, 8);

	return true;
}

void i64ToAddr(char* ip, int &port, uint64 addr64)
{
	unsigned char addr[8];
	memcpy(addr, &addr64, 8);
	sprintf(ip, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	memcpy(&port, &addr[4], 4);
}

bool NetEngine::Connect(const char* ip, int port)
{
	uint64 addr64 = 0;
	if ( !addrToI64(addr64, ip, port) ) return false;

	AutoLock lock(&m_serListMutex);
	pair<map<uint64,SOCKET>::iterator,bool> ret 
		= m_serIPList.insert(map<uint64,SOCKET>::value_type(addr64,INVALID_SOCKET));
	map<uint64,SOCKET>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ConnectOtherServer(ip, port);
	if ( INVALID_SOCKET == it->second ) return false;
	lock.Unlock();

	if ( !OnConnect(it->second, true) )	it->second = INVALID_SOCKET;
	
	return true;
}

SOCKET NetEngine::ConnectOtherServer(const char* ip, int port)
{
	Socket sock;//监听socket
	if ( !sock.Init( Socket::tcp ) ) return INVALID_SOCKET;
	if ( !sock.Connect(ip, port) ) 
	{
		sock.Close();
		return INVALID_SOCKET;
	}
	Socket::InitForIOCP(sock.GetSocket());
	
	return sock.Detach();
}

bool NetEngine::ConnectAll()
{
	if ( m_stop ) return false;
	bool ret = true;
	AutoLock lock(&m_serListMutex);
	map<uint64,SOCKET>::iterator it = m_serIPList.begin();
	char ip[24];
	int port;
	for ( ; it != m_serIPList.end(); it++ )
	{
		if ( INVALID_SOCKET != it->second ) continue;
		i64ToAddr(ip, port, it->first);
		it->second = ConnectOtherServer(ip, port);
		if ( INVALID_SOCKET == it->second ) 
		{
			ret = false;
			continue;
		}
		if ( !OnConnect(it->second, true) )	it->second = INVALID_SOCKET;
	}

	return ret;
}

void NetEngine::SetServerClose(NetConnect *pConnect)
{
	if ( !pConnect->m_host.IsServer() ) return;
	
	SOCKET sock = pConnect->GetSocket()->GetSocket();
	AutoLock lock(&m_serListMutex);
	map<uint64,SOCKET>::iterator it = m_serIPList.begin();
	for ( ; it != m_serIPList.end(); it++ )
	{
		if ( sock != it->second ) continue;
		it->second = INVALID_SOCKET;
		break;
	}
}

//监听连接
bool NetEngine::MonitorConnect(NetConnect *pConnect)
{
	return false;
}

//向某组连接广播消息(业务层接口)
void NetEngine::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount )
{
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	NetConnect *pConnect;
	vector<NetConnect*> recverList;
	//加锁将所有广播接收连接复制到一个队列中
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin(); m_nHeartTime > 0 && it != m_connectList.end(); it++ )
	{
		pConnect = it->second;
		if ( !pConnect->IsInGroups(recvGroupIDs, recvCount) 
			|| pConnect->IsInGroups(filterGroupIDs, filterCount) ) continue;
		recverList.push_back(pConnect);
		pConnect->WorkAccess();//业务层先获取访问
	}
	lock.Unlock();
	
	//向队列中的连接开始广播
	vector<NetConnect*>::iterator itv = recverList.begin();
	for ( ; itv != recverList.end(); itv++ )
	{
		pConnect = *itv;
		if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
		pConnect->WorkFinished();//释放访问
	}
}

//向某主机发送消息(业务层接口)
void NetEngine::SendMsg( int hostID, char *msg, int msgsize )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(hostID);
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	NetConnect *pConnect = itNetConnect->second;
	pConnect->WorkAccess();//业务层先获取访问
	lock.Unlock();
	if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
	pConnect->WorkFinished();//释放访问

	return;
}

const char* NetEngine::GetInitError()//取得启动错误信息
{
	return m_startError.c_str();
}

}
// namespace mdk

