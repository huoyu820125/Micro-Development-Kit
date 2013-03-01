
#include "../../../include/frame/netserver/STIocp.h"
#include "../../../include/frame/netserver/STEpoll.h"
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

#include "../../../include/mdk/Socket.h"
#include "../../../include/mdk/atom.h"
#include "../../../include/mdk/MemoryPool.h"
#include "../../../include/mdk/mapi.h"

#include "../../../include/frame/netserver/STNetEngine.h"
#include "../../../include/frame/netserver/STNetConnect.h"
#include "../../../include/frame/netserver/STNetServer.h"

using namespace std;
namespace mdk
{
	
STNetEngine::STNetEngine()
{
	Socket::SocketInit();
	m_pConnectPool = NULL;
	m_stop = true;//停止标志
	m_startError = "";
	m_nHeartTime = 0;//心跳间隔(S)，默认不检查
	m_nReconnectTime = 0;//默认不自动重连
#ifdef WIN32
	m_pNetMonitor = new STIocp;
#else
	m_pNetMonitor = new STEpoll;
#endif
	m_pNetServer = NULL;
	m_averageConnectCount = 5000;
}

STNetEngine::~STNetEngine()
{
	Stop();
	if ( NULL != m_pConnectPool )
	{
		delete m_pConnectPool;
		m_pConnectPool = NULL;
	}
	Socket::SocketDestory();
}

//设置平均连接数
void STNetEngine::SetAverageConnectCount(int count)
{
	m_averageConnectCount = count;
}

//设置心跳时间
void STNetEngine::SetHeartTime( int nSecond )
{
	m_nHeartTime = nSecond;
}

//设置自动重连时间,小于等于0表示不自动重连
void STNetEngine::SetReconnectTime( int nSecond )
{
	m_nReconnectTime = nSecond;
}

/**
 * 开始引擎
 * 成功返回true，失败返回false
 */
bool STNetEngine::Start()
{
	if ( !m_stop ) return true;
	m_stop = false;	
	
	int memoryCount = 2;
	for ( memoryCount = 2; memoryCount * memoryCount < m_averageConnectCount * 2; memoryCount++ );
	if ( memoryCount < 200 ) memoryCount = 200;
	if ( NULL != m_pConnectPool )//之前Stop()过,又重新启动
	{
		delete m_pConnectPool;
		m_pConnectPool = NULL;
	}
	m_pConnectPool = new MemoryPool( sizeof(STNetConnect), memoryCount );
	if ( NULL == m_pConnectPool )
	{
		m_startError = "内存不足，无法创建NetConnect内存池";
		Stop();
		return false;
	}
	if ( !m_pNetMonitor->Start( MAXPOLLSIZE ) ) 
	{
		m_startError = m_pNetMonitor->GetInitError();
		Stop();
		return false;
	}
	if ( !ListenAll() )
	{
		Stop();
		return false;
	}
	ConnectAll();
	return m_mainThread.Run( Executor::Bind(&STNetEngine::Main), this, 0 );
}

bool STNetEngine::WINIO(int timeout)
{
#ifdef WIN32
	STIocp::IO_EVENT e;
	if ( !m_pNetMonitor->WaitEvent( e, timeout ) ) return false;
	switch( e.type )
	{
	case STIocp::timeout :
		break;
	case STIocp::stop :
		return false;
		break;
	case STIocp::connect :
		OnConnect( e.client, false );
		m_pNetMonitor->AddAccept( e.sock );
		break;
	case STIocp::recv :
		OnData( e.sock, e.pData, e.uDataSize );
		break;
	case STIocp::close :
		OnClose( e.sock );
		break;
	case STIocp::send :
		OnSend( e.sock, e.uDataSize );
		break;
	default:
		break;
	}
	return true;
#endif
	return false;
}

bool STNetEngine::LinuxIO( int timeout )
{
#ifndef WIN32
	int nCount = 0;
	int eventType = 0;
	int i = 0;
	Socket sockListen;
	Socket sockClient;
	SOCKET sock;
	map<SOCKET,int>::iterator it;
	pair<map<SOCKET,int>::iterator,bool> ret;
	
	//没有可io的socket则等待新可io的socket
	//否则检查是否有新的可io的socket，有则取出加入到m_ioList中，没有也不等待
	//继续进行m_ioList中的socket进行io操作
	if ( 0 >= m_ioList.size() ) nCount = m_pNetMonitor->WaitEvent( timeout );
	else nCount = m_pNetMonitor->WaitEvent( 0 );
	if ( 0 > nCount ) return false;
	//加入到m_ioList中
	for ( i = 0; i < nCount; i++ )
	{
		sock = m_pNetMonitor->GetSocket(i);
		if ( INVALID_SOCKET == sock ) return false;//STEpoll已关闭
		if ( m_pNetMonitor->IsAcceptAble(i) )//连接类型直接执行业务 
		{
			while ( true )
			{
				sockListen.Detach();
				sockListen.Attach(sock);
				sockListen.Accept( sockClient );
				if ( INVALID_SOCKET == sockClient.GetSocket() ) break;
				sockClient.SetSockMode();
				OnConnect(sockClient.Detach(), false);
			}
			continue;
		}
		//不是监听socket一定是io事件
		//加入到io列表，统一调度
		if ( m_pNetMonitor->IsWriteAble(i) ) eventType = 1|2;//recv+send事件
		else eventType = 1;//recv事件
		ret = m_ioList.insert(map<SOCKET,int>::value_type(sock,eventType) );//增加可io的对象
		if ( !ret.second ) ret.first->second = ret.first->second|eventType;//设置新事件
	}
	//遍历m_ioList，执行1次io
	for ( it = m_ioList.begin(); it != m_ioList.end(); it++ )
	{
		if ( 1&it->second ) //可读
		{
			if ( ok != OnData( it->first, 0, 0 ) ) //数据已读完或连接已断开
			{
				it->second = it->second&~1;//清除事件
			}
		}
		if ( 2&it->second ) //可写
		{
			if ( ok != OnSend( it->first, 0 ) )//数据已经发送完，或socket已经断开，或socket不可写
			{
				it->second = it->second&~2;//清除事件
			}
		}
	}
	
	//将不可io的socket清除
	it = m_ioList.begin();
	while (  it != m_ioList.end() ) 
	{
		if ( 0 == it->second ) 
		{
			m_ioList.erase(it);
			it = m_ioList.begin();
			continue;
		}
		it++;
	}
	return true;
#endif
	return false;
}

//等待停止
void STNetEngine::WaitStop()
{
	m_mainThread.WaitStop();
}

//停止引擎
void STNetEngine::Stop()
{
	if ( m_stop ) return;
	m_stop = true;
	m_pNetMonitor->Stop();
	m_mainThread.Stop(3000);
#ifndef WIN32
	m_ioList.clear();
#endif
}

//主线程
void* STNetEngine::Main(void*)
{
	time_t lastConnect = time(NULL);
	time_t curTime = time(NULL);
	
	while ( !m_stop ) 
	{
#ifdef WIN32
		if ( !WINIO( 10000 ) ) break;
#else
		if ( !LinuxIO( 10000 ) ) break;
#endif
		curTime = time(NULL);
		if ( 10000 <= curTime - lastConnect ) continue;
		lastConnect = curTime;
		HeartMonitor();
		ReConnectAll();
	}
	return NULL;
}

//心跳线程
void STNetEngine::HeartMonitor()
{
	if ( 0 >= m_nHeartTime ) return;
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	STNetConnect *pConnect;
	time_t tCurTime = 0;
	/*	
		创建一个临时的释放列表保存要释放的对象，等遍历结束1次性导入等待释放对象列表
		避免在循环中因为重复的为等待释放列表的访问而加锁解所
	 */
	tCurTime = time( NULL );
	time_t tLastHeart;
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin(); it != m_connectList.end(); )//心跳时间<=0无心跳机制,或遍历完成
	{
		pConnect = it->second;
		if ( pConnect->m_host.IsServer() ) //服务连接 ，不检查心跳
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
		it = m_connectList.begin();
	}
	lock.Unlock();
}

void STNetEngine::ReConnectAll()
{
	if ( 0 >= m_nReconnectTime ) return;//无重连机制
	static time_t lastConnect = time(NULL);
	time_t curTime = time(NULL);
	if ( m_nReconnectTime > curTime - lastConnect ) return;
	lastConnect = curTime;
	ConnectAll();
}

//关闭一个连接
void STNetEngine::CloseConnect( ConnectList::iterator it )
{
	/*
	   必须先删除再关闭，顺序不能换，
	   避免关闭后，erase前，正好有client连接进来，
	   系统立刻就把该连接分配给新client使用，造成新client在插入m_connectList时失败
	*/
	STNetConnect *pConnect = it->second;
	m_connectList.erase( it );//之后不可能有MsgWorker()发生，因为OnData里面已经找不到连接了
	AtomDec(&pConnect->m_useCount, 1);//m_connectList访问完成
	pConnect->GetSocket()->Close();
	pConnect->m_bConnect = false;
	if ( 0 == AtomAdd(&pConnect->m_nReadCount, 1) ) NotifyOnClose(pConnect);
	pConnect->Release();//连接断开释放共享对象
	return;
}

void STNetEngine::NotifyOnClose(STNetConnect *pConnect)
{
	if ( 0 == AtomAdd(&pConnect->m_nDoCloseWorkCount, 1) )//只有1个线程执行OnClose，且仅执行1次
	{
		SetServerClose(pConnect);//连接的服务断开
		m_pNetServer->OnCloseConnect( pConnect->m_host );
	}
}

int g_c = 0;
bool STNetEngine::OnConnect( SOCKET sock, bool isConnectServer )
{
	AtomAdd(&g_c, 1);
	STNetConnect *pConnect = new (m_pConnectPool->Alloc())STNetConnect(sock, isConnectServer, m_pNetMonitor, this, m_pConnectPool);
	if ( NULL == pConnect ) 
	{
		closesocket(sock);
		return false;
	}
	//加入管理列表
	pConnect->RefreshHeart();
	AtomAdd(&pConnect->m_useCount, 1);//被m_connectList访问
	AutoLock lock( &m_connectsMutex );
	pair<ConnectList::iterator, bool> ret = m_connectList.insert( ConnectList::value_type(pConnect->GetSocket()->GetSocket(),pConnect) );
	lock.Unlock();
	//执行业务
	STNetHost accessHost = pConnect->m_host;//被引擎访问，局部变量离开时，析构函数自动释放访问
	m_pNetServer->OnConnect( pConnect->m_host );
	/*
		监听连接
		必须等OnConnect完成，才可以开始监听连接上的IO事件
		否则，可能业务层尚未完成连接初始化工作，就收到OnMsg通知，
		导致业务层不知道该如何处理消息
	 */
	bool bMonitor = true;
	if ( !m_pNetMonitor->AddMonitor(sock) ) return false;
#ifdef WIN32
	bMonitor = m_pNetMonitor->AddRecv( 
		sock, 
		(char*)(pConnect->PrepareBuffer(BUFBLOCK_SIZE)), 
		BUFBLOCK_SIZE );
#else
	bMonitor = m_pNetMonitor->AddIO( sock, true, false );
#endif
	if ( !bMonitor ) CloseConnect(pConnect->GetSocket()->GetSocket());
	return true;
}

void STNetEngine::OnClose( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

connectState STNetEngine::OnData( SOCKET sock, char *pData, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);//client列表里查找
	if ( itNetConnect == m_connectList.end() ) return cs;//底层已经断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被引擎访问，局部变量离开时，析构函数自动释放访问
	lock.Unlock();

	pConnect->RefreshHeart();
	try
	{
		cs = RecvData( pConnect, pData, uSize );
		if ( unconnect == cs )
		{
			OnClose( sock );
			return cs;
		}
		if ( 0 != AtomAdd(&pConnect->m_nReadCount, 1) ) return cs;
		//执行业务STNetServer::OnMsg();
		MsgWorker(pConnect);
	}catch( ... ){}
	return cs;
}

void* STNetEngine::MsgWorker( STNetConnect *pConnect )
{
	for ( ; !m_stop; )
	{
		m_pNetServer->OnMsg( pConnect->m_host );//无返回值，避免框架逻辑依赖于客户实现
		if ( !pConnect->m_bConnect ) break;
		if ( !pConnect->IsReadAble() ) break;
	}
	AtomDec(&pConnect->m_nReadCount,1);
	//确保NetServer::OnClose()一定在所有NetServer::OnMsg()完成之后
	if ( !pConnect->m_bConnect ) NotifyOnClose(pConnect);
	return 0;
}

connectState STNetEngine::RecvData( STNetConnect *pConnect, char *pData, unsigned short uSize )
{
#ifdef WIN32
	pConnect->WriteFinished( uSize );
	if ( !m_pNetMonitor->AddRecv(  pConnect->GetSocket()->GetSocket(), 
		(char*)(pConnect->PrepareBuffer(BUFBLOCK_SIZE)), BUFBLOCK_SIZE ) )
	{
		return unconnect;
	}
#else
	unsigned char* pWriteBuf = NULL;	
	int nRecvLen = 0;
	unsigned int nMaxRecvSize = 0;
	//最多接收1M数据，让给其它连接进行io
	while ( nMaxRecvSize < 1048576 )
	{
		pWriteBuf = pConnect->PrepareBuffer(BUFBLOCK_SIZE);
		nRecvLen = pConnect->GetSocket()->Receive(pWriteBuf, BUFBLOCK_SIZE);
		if ( nRecvLen < 0 ) return unconnect;
		if ( 0 == nRecvLen ) 
		{
			if ( !m_pNetMonitor->AddIO(pConnect->GetSocket()->GetSocket(), true, false) ) return unconnect;
			return wait_recv;
		}
		nMaxRecvSize += nRecvLen;
		pConnect->WriteFinished( nRecvLen );
	}
#endif
	return ok;
}

//关闭一个连接
void STNetEngine::CloseConnect( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find( sock );
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

//响应发送完成事件
connectState STNetEngine::OnSend( SOCKET sock, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return cs;//底层已经主动断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被引擎访问，局部变量离开时，析构函数自动释放访问
	lock.Unlock();
	try
	{
		if ( pConnect->m_bConnect ) cs = SendData(pConnect, uSize);
	}
	catch(...)
	{
	}
	return cs;
	
}

connectState STNetEngine::SendData(STNetConnect *pConnect, unsigned short uSize)
{
#ifdef WIN32
	try
	{
		unsigned char buf[BUFBLOCK_SIZE];
		if ( uSize > 0 ) pConnect->m_sendBuffer.ReadData(buf, uSize);
		int nLength = pConnect->m_sendBuffer.GetLength();
		if ( 0 >= nLength ) 
		{
			pConnect->SendEnd();//发送结束
			nLength = pConnect->m_sendBuffer.GetLength();//第二次检查发送缓冲
			if ( 0 >= nLength ) 
			{
				/*
					情况1：外部发送线程未完成发送缓冲写入
						外部线程完成写入时，不存在发送流程，单线程SendStart()必定成功
						结论：不会漏发送
					其它情况：不存在其它情况
				*/
				return ok;//没有待发送数据，退出发送线程
			}
			/*
				外部发送线程已完成发送缓冲写入
				多线程并发SendStart()，只有一个成功
				结论：不会出现并发发送，也不会漏数据
			*/
			if ( !pConnect->SendStart() ) return ok;//已经在发送
			//发送流程开始
		}

		if ( nLength > BUFBLOCK_SIZE )
		{
			pConnect->m_sendBuffer.ReadData(buf, BUFBLOCK_SIZE, false);
			m_pNetMonitor->AddSend( pConnect->GetSocket()->GetSocket(), (char*)buf, BUFBLOCK_SIZE );
		}
		else
		{
			pConnect->m_sendBuffer.ReadData(buf, nLength, false);
			m_pNetMonitor->AddSend( pConnect->GetSocket()->GetSocket(), (char*)buf, nLength );
		}
	}
	catch(...)
	{
	}
	return ok;
#else
	try
	{
		connectState cs = wait_send;//默认为等待状态
		//////////////////////////////////////////////////////////////////////////
		//执行发送
		unsigned char buf[BUFBLOCK_SIZE];
		int nSize = 0;
		int nSendSize = 0;
		int nFinishedSize = 0;
		nSendSize = pConnect->m_sendBuffer.GetLength();
		if ( 0 < nSendSize )
		{
			nSize = 0;
			//一次发送4096byte
			if ( BUFBLOCK_SIZE < nSendSize )//1次发不完，设置为就绪状态
			{
				pConnect->m_sendBuffer.ReadData(buf, BUFBLOCK_SIZE, false);
				nSize += BUFBLOCK_SIZE;
				nSendSize -= BUFBLOCK_SIZE;
				cs = ok;
			}
			else//1次可发完，设置为等待状态
			{
				pConnect->m_sendBuffer.ReadData(buf, nSendSize, false);
				nSize += nSendSize;
				nSendSize = 0;
				cs = wait_send;
			}
			nFinishedSize = pConnect->GetSocket()->Send((char*)buf, nSize);//发送
			if ( -1 == nFinishedSize ) cs = unconnect;
			else
			{
				pConnect->m_sendBuffer.ReadData(buf, nFinishedSize);//将发送成功的数据从缓冲清除
				if ( nFinishedSize < nSize ) //sock已写满，设置为等待状态
				{
					cs = wait_send;
				}
			}

		}
		if ( ok == cs || unconnect == cs ) return cs;//就绪状态或连接关闭直接返回，连接关闭不必结束发送流程，pNetConnect对象会被释放，发送流程自动结束

		//等待状态，结束本次发送，并启动新发送流程
		pConnect->SendEnd();//发送结束
		//////////////////////////////////////////////////////////////////////////
		//检查是否需要开始新的发送流程
		if ( 0 >= pConnect->m_sendBuffer.GetLength() ) return cs;
		/*
			外部发送线程已完成发送缓冲写入
			多线程并发SendStart()，只有一个成功
			结论：不会出现并发发送，也不会漏数据
		*/
		if ( !pConnect->SendStart() ) return cs;//已经在发送
		return cs;
	}
	catch(...)
	{
	}
#endif
	return ok;
}

bool STNetEngine::Listen(int port)
{
	pair<map<int,SOCKET>::iterator,bool> ret 
		= m_serverPorts.insert(map<int,SOCKET>::value_type(port,INVALID_SOCKET));
	map<int,SOCKET>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ListenPort(port);
	if ( INVALID_SOCKET == it->second ) return false;
	return true;
}

SOCKET STNetEngine::ListenPort(int port)
{
	Socket listenSock;//监听socket
	if ( !listenSock.Init( Socket::tcp ) ) return INVALID_SOCKET;
	listenSock.SetSockMode();
	if ( !listenSock.StartServer( port ) ) 
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}
	if ( !m_pNetMonitor->AddMonitor( listenSock.GetSocket() ) ) 
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}
	if ( !m_pNetMonitor->AddAccept( listenSock.GetSocket() ) )
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}

	return listenSock.Detach();
}


bool STNetEngine::ListenAll()
{
	bool ret = true;
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


bool STNetEngine::Connect(const char* ip, int port)
{
	uint64 addr64 = 0;
	if ( !addrToI64(addr64, ip, port) ) return false;

	pair<map<uint64,SOCKET>::iterator,bool> ret 
		= m_serIPList.insert(map<uint64,SOCKET>::value_type(addr64,INVALID_SOCKET));
	map<uint64,SOCKET>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ConnectOtherServer(ip, port);
	if ( INVALID_SOCKET == it->second ) return false;

	if ( !OnConnect(it->second, true) )	it->second = INVALID_SOCKET;
	
	return true;
}

SOCKET STNetEngine::ConnectOtherServer(const char* ip, int port)
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

bool STNetEngine::ConnectAll()
{
	if ( m_stop ) return false;
	bool ret = true;
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

void STNetEngine::SetServerClose(STNetConnect *pConnect)
{
	if ( !pConnect->m_host.IsServer() ) return;
	
	SOCKET sock = pConnect->GetID();
	map<uint64,SOCKET>::iterator it = m_serIPList.begin();
	for ( ; it != m_serIPList.end(); it++ )
	{
		if ( sock != it->second ) continue;
		it->second = INVALID_SOCKET;
		break;
	}
}

//向某组连接广播消息(业务层接口)
void STNetEngine::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount )
{
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	STNetConnect *pConnect;
	vector<STNetConnect*> recverList;
	STNetHost accessHost;
	//加锁将所有广播接收连接复制到一个队列中
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin(); m_nHeartTime > 0 && it != m_connectList.end(); it++ )
	{
		pConnect = it->second;
		if ( !pConnect->IsInGroups(recvGroupIDs, recvCount) 
			|| pConnect->IsInGroups(filterGroupIDs, filterCount) ) continue;
		recverList.push_back(pConnect);
		accessHost = pConnect->m_host;//被用户访问，局部变量离开时，析构函数自动释放访问
	}
	lock.Unlock();
	
	//向队列中的连接开始广播
	vector<STNetConnect*>::iterator itv = recverList.begin();
	for ( ; itv != recverList.end(); itv++ )
	{
		pConnect = *itv;
		if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
	}
}

//向某主机发送消息(业务层接口)
void STNetEngine::SendMsg( int hostID, char *msg, int msgsize )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(hostID);
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被用户访问，局部变量离开时，析构函数自动释放访问
	lock.Unlock();
	if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);

	return;
}

const char* STNetEngine::GetInitError()//取得启动错误信息
{
	return m_startError.c_str();
}

}
// namespace mdk

