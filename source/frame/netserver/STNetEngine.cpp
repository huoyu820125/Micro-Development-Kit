
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
	m_noDelay = false;
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
		OnConnect( e.client, e.sock );
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
	int sock;
	map<int,int>::iterator it;
	pair<map<int,int>::iterator,bool> ret;
	
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
				OnConnect(sockClient.Detach(), 0);
			}
			continue;
		}
		//不是监听socket一定是io事件
		//加入到io列表，统一调度
		if ( m_pNetMonitor->IsWriteAble(i) ) eventType = 1|2;//recv+send事件
		else eventType = 1;//recv事件
		ret = m_ioList.insert(map<int,int>::value_type(sock,eventType) );//增加可io的对象
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
	
	bool mainFinished = false;
#ifndef WIN32
	m_hEPoll = epoll_create(20000);
	mdk_assert( -1 != m_hEPoll );
	m_events = new epoll_event[20000];	//epoll事件
	mdk_assert( NULL != m_events );
#endif
	m_clientList= (SVR_CONNECT**)new mdk::uint64[20000];
	while ( !m_stop ) 
	{
		if ( !mainFinished )
		{
			if ( 0== m_pNetServer->Main() ) mainFinished = true;
		}
		Select();
#ifdef WIN32
		if ( !WINIO( 10000 ) ) break;
#else
		if ( !LinuxIO( 10000 ) ) break;
#endif
		curTime = time(NULL);
		if ( 10000 <= curTime - lastConnect ) continue;
		lastConnect = curTime;
		HeartMonitor();
		ConnectAll();
	}

	uint64 *p = (uint64*)m_clientList;
	delete[]p;
#ifndef WIN32
	delete[]m_events;
#endif
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

bool STNetEngine::OnConnect( int sock, int listenSock, SVR_CONNECT *pSvr )
{
	if ( m_noDelay ) Socket::SetNoDelay(sock, true);
	STNetConnect *pConnect = new (m_pConnectPool->Alloc())STNetConnect(sock, listenSock, 
		NULL == pSvr?false:true, m_pNetMonitor, this, m_pConnectPool);
	if ( NULL == pConnect ) 
	{
		closesocket(sock);
		return false;
	}
	if ( NULL != pSvr && pSvr->pSvrInfo ) 
	{
		pConnect->SetSvrInfo(pSvr->pSvrInfo);
	}

	//加入管理列表
	pConnect->RefreshHeart();
	AtomAdd(&pConnect->m_useCount, 1);//被m_connectList访问
	pair<ConnectList::iterator, bool> ret = m_connectList.insert( ConnectList::value_type(pConnect->GetSocket()->GetSocket(),pConnect) );
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
	if ( !m_pNetMonitor->AddMonitor(sock, NULL, 0) ) return false;
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

void STNetEngine::OnClose( int sock )
{
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

connectState STNetEngine::OnData( int sock, char *pData, unsigned short uSize )
{
	connectState cs = unconnect;
	ConnectList::iterator itNetConnect = m_connectList.find(sock);//client列表里查找
	if ( itNetConnect == m_connectList.end() ) return cs;//底层已经断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被引擎访问，局部变量离开时，析构函数自动释放访问

	pConnect->RefreshHeart();
	cs = RecvData( pConnect, pData, uSize );
	if ( unconnect == cs )
	{
		OnClose( sock );
		return cs;
	}
	if ( 0 != AtomAdd(&pConnect->m_nReadCount, 1) ) return cs;
	//执行业务STNetServer::OnMsg();
	MsgWorker(pConnect);
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
void STNetEngine::CloseConnect( int sock )
{
	ConnectList::iterator itNetConnect = m_connectList.find( sock );
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

//响应发送完成事件
connectState STNetEngine::OnSend( int sock, unsigned short uSize )
{
	connectState cs = unconnect;
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
	if ( itNetConnect == m_connectList.end() )return cs;//底层已经主动断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被引擎访问，局部变量离开时，析构函数自动释放访问
	if ( pConnect->m_bConnect ) cs = SendData(pConnect, uSize);

	return cs;
}

connectState STNetEngine::SendData(STNetConnect *pConnect, unsigned short uSize)
{
#ifdef WIN32
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
	return ok;
#else
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
		if ( Socket::seError == nFinishedSize ) cs = unconnect;
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
#endif
	return ok;
}

bool STNetEngine::Listen(int port)
{
	pair<map<int,int>::iterator,bool> ret 
		= m_serverPorts.insert(map<int,int>::value_type(port,INVALID_SOCKET));
	map<int,int>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ListenPort(port);
	if ( INVALID_SOCKET == it->second ) return false;
	return true;
}

int STNetEngine::ListenPort(int port)
{
	Socket listenSock;//监听socket
	if ( !listenSock.Init( Socket::tcp ) ) return INVALID_SOCKET;
	listenSock.SetSockMode();
	if ( !listenSock.StartServer( port ) ) 
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}
	if ( !m_pNetMonitor->AddMonitor( listenSock.GetSocket(), NULL, 0 ) ) 
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
	map<int,int>::iterator it = m_serverPorts.begin();
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


bool STNetEngine::Connect(const char* ip, int port, void *pSvrInfo, int reConnectTime)
{
	uint64 addr64 = 0;
	if ( !addrToI64(addr64, ip, port) ) return false;
	
	vector<SVR_CONNECT*> sockArray;
	map<uint64,vector<SVR_CONNECT*> >::iterator it = m_keepIPList.find(addr64);
	if ( it == m_keepIPList.end() ) m_keepIPList.insert( map<uint64,vector<SVR_CONNECT*> >::value_type(addr64,sockArray) );
	SVR_CONNECT *pSvr = new SVR_CONNECT;
	pSvr->reConnectSecond = reConnectTime;
	pSvr->lastConnect = 0;
	pSvr->sock = INVALID_SOCKET;
	pSvr->addr = addr64;
	pSvr->state = SVR_CONNECT::unconnected;
	pSvr->pSvrInfo = pSvrInfo;
	m_keepIPList[addr64].push_back(pSvr);
	if ( m_stop ) return false;
	
	//保存链接结果
	pSvr->lastConnect = time(NULL);
	ConnectResult ret = ConnectOtherServer(ip, port, pSvr->sock);
	if ( STNetEngine::success == ret )
	{
		OnConnect(pSvr->sock, pSvr->sock, pSvr);
		pSvr->state = SVR_CONNECT::connected;
	}
	else if ( STNetEngine::waitReulst == ret )
	{
		pSvr->state = SVR_CONNECT::connectting;
	}
	else //报告失败
	{
		pSvr->state = SVR_CONNECT::unconnectting;
		ConnectFailed( pSvr );
	}
	
	return true;
}

STNetEngine::ConnectResult STNetEngine::ConnectOtherServer(const char* ip, int port, int &svrSock)
{
	svrSock = INVALID_SOCKET;
	Socket sock;//监听socket
	if ( !sock.Init( Socket::tcp ) ) return STNetEngine::cannotCreateSocket;
	sock.SetSockMode();
// 	bool successed = sock.Connect(ip, port);
	svrSock = sock.Detach();
	return AsycConnect(svrSock, ip, port);
}

bool STNetEngine::ConnectAll()
{
	if ( m_stop ) return false;
	time_t curTime = time(NULL);
	char ip[24];
	int port;
	int i = 0;
	int count = 0;
	int sock = INVALID_SOCKET;
	
	//重链尝试
	SVR_CONNECT *pSvr = NULL;
	map<uint64,vector<SVR_CONNECT*> >::iterator it = m_keepIPList.begin();
	vector<SVR_CONNECT*>::iterator itSvr;
	for ( ; it != m_keepIPList.end(); it++ )
	{
		i64ToAddr(ip, port, it->first);
		itSvr = it->second.begin();
		for ( ; itSvr != it->second.end();  )
		{
			pSvr = *itSvr;
			if ( SVR_CONNECT::connectting == pSvr->state 
				|| SVR_CONNECT::connected == pSvr->state 
				|| SVR_CONNECT::unconnectting == pSvr->state 
				) 
			{
				if ( 0 > pSvr->reConnectSecond && SVR_CONNECT::connected == pSvr->state ) //已连接成功的，释放不需要重连的
				{
					itSvr = it->second.erase(itSvr);
					delete pSvr;
					continue;
				}
				itSvr++;
				continue;
			}
			if ( 0 > pSvr->reConnectSecond && 0 != pSvr->lastConnect ) 
			{
				itSvr = it->second.erase(itSvr);
				delete pSvr;
				continue;
			}
			if ( curTime - pSvr->lastConnect < pSvr->reConnectSecond ) 
			{
				itSvr++;
				continue;
			}
			
			pSvr->lastConnect = curTime;
			ConnectResult ret = ConnectOtherServer(ip, port, pSvr->sock);
			if ( STNetEngine::success == ret )
			{
				OnConnect(pSvr->sock, pSvr->sock, pSvr);
				pSvr->state = SVR_CONNECT::connected;
			}
			else if ( STNetEngine::waitReulst == ret )
			{
				pSvr->state = SVR_CONNECT::connectting;
			}
			else //报告失败
			{
				pSvr->state = SVR_CONNECT::unconnectting;
				ConnectFailed( pSvr );
			}
			itSvr++;
		}
	}
	
	return true;
}

void STNetEngine::SetServerClose(STNetConnect *pConnect)
{
	if ( !pConnect->m_host.IsServer() ) return;
	int sock = pConnect->GetID();
	map<uint64,vector<SVR_CONNECT*> >::iterator it = m_keepIPList.begin();
	int i = 0;
	int count = 0;
	SVR_CONNECT *pSvr = NULL;
	for ( ; it != m_keepIPList.end(); it++ )
	{
		count = it->second.size();
		for ( i = 0; i < count; i++ )
		{
			pSvr = it->second[i];
			if ( sock != pSvr->sock ) continue;
			pSvr->sock = INVALID_SOCKET;
			pSvr->state = SVR_CONNECT::unconnected;
			return;
		}
	}
}

//向某组连接广播消息(业务层接口)
void STNetEngine::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, unsigned int msgsize, int *filterGroupIDs, int filterCount )
{
	ConnectList::iterator it;
	STNetConnect *pConnect;
	vector<STNetConnect*> recverList;
	STNetHost accessHost;
	//加锁将所有广播接收连接复制到一个队列中
	for ( it = m_connectList.begin(); it != m_connectList.end(); it++ )
	{
		pConnect = it->second;
		if ( !pConnect->IsInGroups(recvGroupIDs, recvCount) 
			|| pConnect->IsInGroups(filterGroupIDs, filterCount) ) continue;
		recverList.push_back(pConnect);
		accessHost = pConnect->m_host;//被用户访问，局部变量离开时，析构函数自动释放访问
	}
	
	//向队列中的连接开始广播
	vector<STNetConnect*>::iterator itv = recverList.begin();
	for ( ; itv != recverList.end(); itv++ )
	{
		pConnect = *itv;
		if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
	}
}

//向某主机发送消息(业务层接口)
void STNetEngine::SendMsg( int hostID, char *msg, unsigned int msgsize )
{
	ConnectList::iterator itNetConnect = m_connectList.find(hostID);
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	STNetConnect *pConnect = itNetConnect->second;
	STNetHost accessHost = pConnect->m_host;//被用户访问，局部变量离开时，析构函数自动释放访问

	if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);

	return;
}

const char* STNetEngine::GetInitError()//取得启动错误信息
{
	return m_startError.c_str();
}

void STNetEngine::Select()
{
	int clientCount = 0;
	int i = 0;
	SVR_CONNECT *pSvr = NULL;
	//复制所有connectting状态的sock到监听列表
	clientCount = 0;
	map<uint64,vector<SVR_CONNECT*> >::iterator it = m_keepIPList.begin();
	for ( ; it != m_keepIPList.end() && clientCount < 20000; it++ )
	{
		for ( i = 0; i < it->second.size() && clientCount < 20000; i++ )
		{
			pSvr = it->second[i];
			if ( SVR_CONNECT::connectting != pSvr->state ) continue;
			mdk_assert( INVALID_SOCKET != pSvr->sock );
			m_clientList[clientCount++] = pSvr;
		}
	}
#ifndef WIN32
	EpollConnect( m_clientList, clientCount );
#else
	SelectConnect( m_clientList, clientCount );
#endif
	
	return;
}

#ifndef WIN32
#include <netdb.h>
#endif

STNetEngine::ConnectResult STNetEngine::AsycConnect( int svrSock, const char *lpszHostAddress, unsigned short nHostPort )
{
	if ( NULL == lpszHostAddress ) return STNetEngine::invalidParam;
	//将域名转换为真实IP，如果lpszHostAddress本来就是ip，不影响转换结果
	char ip[64]; //真实IP
#ifdef WIN32
	PHOSTENT hostinfo;   
#else
	struct hostent * hostinfo;   
#endif
	strcpy( ip, lpszHostAddress ); 
	if((hostinfo = gethostbyname(lpszHostAddress)) != NULL)   
	{
		strcpy( ip, inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list) ); 
	}

	//使用真实ip进行连接
	sockaddr_in sockAddr;
	memset(&sockAddr,0,sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(ip);
	sockAddr.sin_port = htons( nHostPort );

	if ( SOCKET_ERROR != connect(svrSock, (sockaddr*)&sockAddr, sizeof(sockAddr)) ) 
	{
		return STNetEngine::success;
	}
#ifndef WIN32
	if ( EINPROGRESS == errno ) 
	{
		return STNetEngine::waitReulst;
	}
#else
	int nError = GetLastError();
	if ( WSAEWOULDBLOCK == nError ) 
	{
		return STNetEngine::waitReulst;
	}
#endif

	return STNetEngine::faild;
}

void* STNetEngine::ConnectFailed( STNetEngine::SVR_CONNECT *pSvr )
{
	if ( NULL == pSvr ) return NULL;
	char ip[32];
	int port;
	int reConnectSecond;
	i64ToAddr(ip, port, pSvr->addr);
	reConnectSecond = pSvr->reConnectSecond;
	int svrSock = pSvr->sock;
	pSvr->sock = INVALID_SOCKET;
	pSvr->state = SVR_CONNECT::unconnected;
	closesocket(svrSock);

	m_pNetServer->OnConnectFailed( ip, port, reConnectSecond );

	return NULL;
}

bool STNetEngine::EpollConnect( SVR_CONNECT **clientList, int clientCount )
{
	bool finished = true;
	static time_t start = -1;

#ifndef WIN32
	int i = 0;
	SVR_CONNECT *pSvr = NULL;
	epoll_event ev;
	int monitorCount = 0;


	//////////////////////////////////////////////////////////////////////////
	//全部加入监听队列
	for ( i = 0; i < clientCount; i++ )
	{
		pSvr = clientList[i];
		ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
		ev.data.ptr = pSvr;
		if ( 0 > epoll_ctl(m_hEPoll, EPOLL_CTL_ADD, pSvr->sock, &ev) ) pSvr->inEpoll = false;
		else 
		{
			pSvr->inEpoll = true;
			monitorCount++;
		}
	}

	if ( -1 == start ) start = time(NULL);
	//////////////////////////////////////////////////////////////////////////
	//循环epoll_wait()直到真实失败or超时or返回事件
	int count = 0;
	while ( true )
	{
		count = epoll_wait(m_hEPoll, m_events, 20000, 0 );
		if ( -1 == count ) 
		{
			if ( EINTR == errno ) continue;//假失败
		}
		break;
	}
	if ( 0 == count )
	{
		time_t curtime = time(NULL);
		if ( curtime - start < 20 ) return true;
		start = -1;
	}
	if ( -1 == count ) printf( "epoll_wait return %d\n", count );
	volatile int errCode = errno;//epoll_wait失败时状态

	/*
		过程A:必须先于过程B执行
		因为过程B如果发现链接失败，会将clientList[i]->sock设置为INVALID_SOCKET
		过程A再想清理epoll监听列队时，在clientList中就已经找不到这个句柄了
	*/
	//////////////////////////////////////////////////////////////////////////
	//过程A:清空epoll监听队列，并补全遗漏的通知
	/*
		遍历监听区间
		将所有监听中的句柄从epoll监听队列删除
		将所有放入epoll监听队列失败的句柄，认为是可读可写的，去尝试一下链接是否成功
	*/
	errCode = errno;//epoll_wait失败时状态
	volatile int delSock = 0;
	volatile int delError = 0;
	for ( i = 0; i < clientCount; i++ )
	{
		if ( clientList[i]->inEpoll )
		{
			delSock = clientList[i]->sock;
			clientList[i]->inEpoll = false;
			delError = epoll_ctl(m_hEPoll, EPOLL_CTL_DEL, clientList[i]->sock, NULL); 
			mdk_assert( 0 == delError );//不应该删除失败，如果失败强制崩溃
			if ( 0 >= count ) ConnectIsFinished(clientList[i], true, true, count, errCode );//不能早于epoll_ctl删除，原因同过程A过程B执行顺序
		}
		else//添加到Epoll失败的句柄，认为可读可写，进入尝试取IP等方法
		{
			ConnectIsFinished(clientList[i], true, true, 1, errCode );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//过程B:处理事件
	if ( 0 < count ) //处理epoll通知结果
	{
		if ( count < monitorCount ) finished = false;//存在尚未返回结果的sock
		for ( i = 0; i < count; i++ )
		{
			pSvr = (SVR_CONNECT*)(m_events[i].data.ptr);
			if ( !ConnectIsFinished(pSvr, m_events[i].events&EPOLLIN, m_events[i].events&EPOLLOUT, count, errCode ) )//sock尚未返回结果
			{
				finished = false;
			}
		}
	}

#endif
	return finished;
}

bool STNetEngine::SelectConnect( SVR_CONNECT **clientList, int clientCount )
{
	bool finished = true;
	static time_t start = -1;
#ifdef WIN32
	int startPos = 0;
	int endPos = 0;
	int i = 0;
	int maxSocket = 0;
	SVR_CONNECT *pSvr = NULL;
	fd_set readfds; 
	fd_set sendfds; 


	//开始监听，每次监听1000个sock,select1次最大监听1024个
	int svrSock;
	for ( endPos = 0; endPos < clientCount; )
	{
		maxSocket = 0;
		FD_ZERO(&readfds);     
		FD_ZERO(&sendfds);  
		startPos = endPos;//记录本次监听sock开始位置
		for ( i = 0; i < FD_SETSIZE - 1 && endPos < clientCount; i++ )
		{
			pSvr = clientList[endPos];
			if ( maxSocket < pSvr->sock ) maxSocket = pSvr->sock;
			FD_SET(pSvr->sock, &readfds); 
			FD_SET(pSvr->sock, &sendfds); 
			endPos++;
		}

		//超时设置
		timeval outtime;
		outtime.tv_sec = 0;
		outtime.tv_usec = 0;
		if ( -1 == start ) start = time(NULL);
		int nSelectRet =::select( maxSocket + 1, &readfds, &sendfds, NULL, &outtime ); //检查读写状态
		if ( 0 == nSelectRet )
		{
			time_t curtime = time(NULL);
			if ( curtime - start < 20 ) return true;
			start = -1;
		}
		int errCode = GetLastError();
		if ( SOCKET_ERROR == nSelectRet ) //出错时错误码和打印所有句柄状态
		{
			printf( "select return %d\n", nSelectRet );
			for ( i = startPos; i < endPos; i++ )
			{
				pSvr = clientList[i];
				svrSock = pSvr->sock;
				printf( "select = %d errno(%d) sock(%d) read(%d) send(%d)\n", nSelectRet, errCode, svrSock, FD_ISSET(svrSock, &readfds), FD_ISSET(svrSock, &sendfds) );
			}
		}

		for ( i = startPos; i < endPos; i++ )
		{
			if ( !ConnectIsFinished(clientList[i], 
				0 != FD_ISSET(clientList[i]->sock, &readfds), 
				0 != FD_ISSET(clientList[i]->sock, &sendfds), 
				nSelectRet, errCode ) )//链接尚未返回结果,遍历结束后不等待，继续监听未返回的sock
			{
				finished = false;
			}
		}
	}
#endif
	return finished;
}

bool STNetEngine::ConnectIsFinished( SVR_CONNECT *pSvr, bool readable, bool sendable, int api, int errorCode )
{
	int reason = 0;
	bool successed = true;
	int nSendSize0 = 0;
	int sendError0 = 0;
	int nSendSize1 = 0;
	int sendError1 = 0;
	char buf[256];
	int nRecvSize0 = 0;
	int recvError0 = 0;
	int nRecvSize1 = 0;
	int recvError1 = 0;
	char clientIP[256];
	char serverIP[256];
	int svrSock = pSvr->sock;

	if ( sendable )
	{
		nSendSize0 = send(svrSock, buf, 0, 0);
		sendError0 = 0;
		if ( 0 > nSendSize0 ) 
		{
#ifdef WIN32
			sendError0 = GetLastError();
#else
			sendError0 = errno;
#endif
			successed = false;
		}
	}
	if ( readable )
	{
		nRecvSize0 = recv(svrSock, buf, 0, MSG_PEEK);
		recvError0 = 0;
		if ( SOCKET_ERROR == nRecvSize0 )
		{
#ifdef WIN32
			recvError0 = GetLastError();
#else
			recvError0 = errno;
#endif
			successed = false;
		}

		nRecvSize1 = recv(svrSock, buf, 1, MSG_PEEK);
		recvError1 = 0;
		if ( SOCKET_ERROR == nRecvSize1 )
		{
#ifdef WIN32
			recvError1 = GetLastError();
#else
			recvError1 = errno;
#endif
			successed = false;
		}
	}

	if ( 0 >= api ) 
	{
		successed = false;
		reason = 1;
	}
	else if ( successed )
	{
		if ( !readable && !sendable ) //sock尚未返回结果
		{
			return false;
		}
		sockaddr_in sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));
		socklen_t nSockAddrLen = sizeof(sockAddr);
		if ( SOCKET_ERROR == getsockname( svrSock, (sockaddr*)&sockAddr, &nSockAddrLen ) ) 
		{
			reason = 2;
			successed = false;
		}
		else
		{
			strcpy(clientIP, inet_ntoa(sockAddr.sin_addr));

			if ( 0 == strcmp("0.0.0.0", clientIP) ) 
			{
				reason = 3;
				successed = false;
			}
			else
			{
				memset(&sockAddr, 0, sizeof(sockAddr));
				nSockAddrLen = sizeof(sockAddr);
				if ( SOCKET_ERROR == getpeername( svrSock, (sockaddr*)&sockAddr, &nSockAddrLen ) ) 
				{
					reason = 4;
					successed = false;
				}
				else strcpy(serverIP, inet_ntoa(sockAddr.sin_addr));
			}
		}
	}
	if ( !successed )
	{
		pSvr->state = SVR_CONNECT::unconnectting;
		ConnectFailed( pSvr );
		return true;
	}

	OnConnect(svrSock, svrSock, pSvr);
	pSvr->state = SVR_CONNECT::connected;
	return true;
}

void STNetEngine::OpenNoDelay()
{
	m_noDelay = true;
}

}
// namespace mdk

