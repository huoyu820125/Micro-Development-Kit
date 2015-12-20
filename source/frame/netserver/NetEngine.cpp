
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

#include "../../../include/mdk/mapi.h"
#include "../../../include/mdk/Socket.h"

#include "../../../include/frame/netserver/NetEngine.h"
#include "../../../include/frame/netserver/NetConnect.h"
#include "../../../include/frame/netserver/NetEventMonitor.h"
#include "../../../include/frame/netserver/NetServer.h"
#include "../../../include/mdk/atom.h"
#include "../../../include/mdk/MemoryPool.h"

using namespace std;
namespace mdk
{

NetEngine::NetEngine()
{
	Socket::SocketInit();
	m_pConnectPool = NULL;
	m_stop = true;//停止标志
	m_startError = "";
	m_nHeartTime = 0;//心跳间隔(S)，默认不检查
	m_pNetMonitor = NULL;
	m_ioThreadCount = 16;//网络io线程数量
	m_workThreadCount = 16;//工作线程数量
	m_pNetServer = NULL;
	m_averageConnectCount = 5000;
	m_nextConnectId = 0;
	m_noDelay = false;
}

NetEngine::~NetEngine()
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
void NetEngine::SetAverageConnectCount(int count)
{
	m_averageConnectCount = count;
}

//设置心跳时间
void NetEngine::SetHeartTime( int nSecond )
{
	m_nHeartTime = nSecond;
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

void NetEngine::SetOnWorkStart( MethodPointer method, void *pObj, void *pParam )
{
	m_workThreads.SetOnStart(method, pObj, pParam);
}

void NetEngine::SetOnWorkStart( FuntionPointer fun, void *pParam )
{
	m_workThreads.SetOnStart(fun, pParam);
}

/**
 * 开始引擎
 * 成功返回true，失败返回false
 */
bool NetEngine::Start()
{
	if ( !m_stop ) return true;
	m_stop = false;	
	int memoryCount = 2;
	for ( memoryCount = 2; memoryCount * memoryCount < m_averageConnectCount * 2; memoryCount++ );
	if ( memoryCount < 200 ) memoryCount = 200;
	if ( NULL != m_pConnectPool )//之前Stop了又重新Start
	{
		delete m_pConnectPool;
		m_pConnectPool = NULL;
	}
	m_pConnectPool = new MemoryPool( sizeof(NetConnect), memoryCount );
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
	m_workThreads.Start( m_workThreadCount );
	int i = 0;
	for ( i = 0; i < m_ioThreadCount; i++ ) m_ioThreads.Accept( Executor::Bind(&NetEngine::NetMonitorTask), this, NULL);
#ifndef WIN32
	for ( i = 0; i < m_ioThreadCount; i++ ) m_ioThreads.Accept( Executor::Bind(&NetEngine::NetMonitorTask), this, (void*)1 );
	for ( i = 0; i < m_ioThreadCount; i++ ) m_ioThreads.Accept( Executor::Bind(&NetEngine::NetMonitorTask), this, (void*)2 );
	m_ioThreads.Start( m_ioThreadCount * 3 );
#else
	m_ioThreads.Start( m_ioThreadCount );
#endif
	
	if ( !ListenAll() )
	{
		Stop();
		return false;
	}
	ConnectAll();
	m_connectThread.Run( Executor::Bind(&NetEngine::ConnectThread), this, 0 );
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
	m_sigStop.Notify();
	m_mainThread.Stop( 3000 );
	m_ioThreads.Stop();
	m_workThreads.Stop();
}

//主线程
void* NetEngine::Main(void*)
{
	while ( !m_stop ) 
	{
		if ( m_sigStop.Wait( 10000 ) ) break;
		HeartMonitor();
		ConnectAll();
	}
	return NULL;
}

//心跳线程
void NetEngine::HeartMonitor()
{
	if ( 0 >= m_nHeartTime ) return;//无心跳机制
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	NetConnect *pConnect;
	time_t tCurTime = 0;
	tCurTime = time( NULL );
	time_t tLastHeart;
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin();  it != m_connectList.end(); )
	{
		pConnect = it->second;
		if ( pConnect->m_host.IsServer() ) //服务连接，不检查心跳
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
		//无心跳/连接已断开，强制断开连接
		CloseConnect( it );
		it = m_connectList.begin();
	}
	lock.Unlock();
}

//关闭一个连接
void NetEngine::CloseConnect( ConnectList::iterator it )
{
	/*
	   必须先删除再关闭，顺序不能换，
	   避免关闭后，erase前，正好有client连接进来，
	   系统立刻就把该连接分配给新client使用，造成新client在插入m_connectList时失败
	*/
	NetConnect *pConnect = it->second;
	m_connectList.erase( it );//之后不可能有MsgWorker()发生，因为OnData里面已经找不到连接了
	/*
		pConnect->GetSocket()->Close();
		以上操作在V1.51版中，被从此处移动到CloseWorker()中
		在m_pNetServer->OnCloseConnect()之后执行

		A.首先推迟Close的目的
			在OnCloseConnect()完成前，也就是业务层完成连接断开业务前
			不让系统回收socket的句柄，再利用
			
			以避免发生如下情况。
				比如用户在业务层(NetServer派生类)中创建map<int,NetHost>类型host列表
				在NetServer::OnConnect()时加入
				在NetServer::OnClose())时删除

				如果在这里就执行关闭socket（假设句柄为100）

				业务层NetServer::OnClose将在之后得到通知，
				如果这时又有新连接进来，则系统会从新使用100作为句柄分配给新连接。
				由于是多线程并发，所以可能在NetServer::OnClose之前，先执行NetServer::OnConnect()
				由于NetServer::OnClose还没有完成，100这个key依旧存在于用户创建的map中，
				导致NetServer::OnConnect()中的插入操作失败
				
		  因此，用户需要准备一个wait_insert列队，在OnConnect()中insert失败时，
		  需要将对象保存到wait_insert列队，并终止OnConnect()业务逻辑

		  在OnClose中删除对象后，用对象的key到wait_insert列队中检查，
		  找到匹配的对象再insert，然后继续执行OnConnect的后续业务，
		  OnConnect业务逻辑才算完成
		  
		  1.代码上非常麻烦
		  2.破坏了功能内聚，OnConnect()与OnClose()逻辑被迫耦合在一起

		B.再分析推迟Close有没有其它副作用
		问题1：由于连接没有关闭，在server端主动close时，连接状态实际还是正常的，
		如果client不停地发送数据，会不会导致OnMsg线程一直接收不完数据，
		让OnClose没机会执行？
		
		  答：不会，因为m_bConnect标志被设置为false了，而OnMsg是在MsgWorker()中被循环调用，
		每次循环都会检查m_bConnect标志，所以即使还有数据可接收，OnMsg也会被终止
	 */
//	pConnect->GetSocket()->Close();

	pConnect->m_bConnect = false;
	/*
		执行业务NetServer::OnClose();
		避免与未完成MsgWorker并发，(MsgWorker内部循环调用OnMsg())，也就是避免与OnMsg并发

		与MsgWorker的并发情况分析
		情况1：MsgWorker已经return
			那么AtomAdd返回0，执行NotifyOnClose()，不可能发生在OnMsg之前，
			之后也不可能OnMsg，前面已经说明MsgWorker()不可能再发生
		情况2：MsgWorker未返回，分2种情况
			情况1：这里先AtomAdd
				必然返回非0，因为没有发生过AtomDec
				不执行OnClose
				遗漏OnClose？
				不会！那么看MsgWorker()，AtomAdd返回非0，所以AtomDec必然返回>1，
				MsgWorker()会再循环一次OnMsg（这次OnMsg是没有数据的，对用户没有影响
				OnMsg读不到足够数据很正常），
				然后退出循环，发现m_bConnect=false，于是NotifyOnClose()发出OnClose通知
				OnClose通知没有被遗漏
			情况2：MsgWorker先AtomDec
				必然返回1，因为MsgWorker循环中首先置了1，而中间又没有AtomAdd发生
				MsgWorker退出循环
				发现m_bConnect=false，于是NotifyOnClose()发出OnClose通知
				然后这里AtomAdd必然返回0，也NotifyOnClose()发出OnClose通知
				重复通知？
				不会，NotifyOnClose()保证了多线程并发调用下，只会通知1次

		与OnData的并发情况分析
			情况1：OnData先AtomAdd
				保证有MsgWorker会执行
				AtomAdd返回非0，放弃NotifyOnClose
				MsgWorker一定会NotifyOnClose
			情况2：这里先AtomAdd
				OnData再AtomAdd时必然返回>0，OnData放弃MsgWorker
				遗漏OnMsg？应该算做放弃数据，而不是遗漏
				分3种断开情况
				1.server发现心跳没有了，主动close，那就是网络原因，强制断开，无所谓数据丢失
				2.client与server完成了所有业务，希望正常断开
					那就应该按照通信行业连接安全断开的原则，让接收方主动Close
					而不能发送方主动Close,所以不可能遗漏数据
					如果发送放主动close，服务器无论如何设计，都没办法保证收到最后的这次数据
	 */
	NotifyOnClose(pConnect);
	pConnect->Release();//连接断开释放共享对象
	return;
}

void NetEngine::NotifyOnClose(NetConnect *pConnect)
{
	if ( 0 == AtomAdd(&pConnect->m_nReadCount, 1) )  
	{
		if ( 0 == AtomAdd(&pConnect->m_nDoCloseWorkCount, 1) )//只有1个线程执行OnClose，且仅执行1次
		{
			AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
			m_workThreads.Accept( Executor::Bind(&NetEngine::CloseWorker), this, pConnect);
		}
	}
}

bool NetEngine::OnConnect( int sock, SVR_CONNECT *pSvr )
{
	if ( m_noDelay ) Socket::SetNoDelay(sock, true);
	NetConnect *pConnect = new (m_pConnectPool->Alloc())NetConnect(sock, 
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
	pConnect->GetSocket()->SetSockMode();
	//加入管理列表
	AutoLock lock( &m_connectsMutex );
	int64 connectId = -1;
	while ( -1 == connectId )//跳过预留ID
	{
		connectId = m_nextConnectId;
		m_nextConnectId++;
	}
	pConnect->SetID(connectId);
	pConnect->RefreshHeart();
	pair<ConnectList::iterator, bool> ret = m_connectList.insert( ConnectList::value_type(connectId,pConnect) );
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();
	//执行业务
	m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectWorker), this, pConnect );
	return true;
}

void* NetEngine::ConnectWorker( NetConnect *pConnect )
{
	int64 connectId = pConnect->GetID();
	bool successed;
#ifdef WIN32
	successed = m_pNetMonitor->AddMonitor(pConnect->GetSocket()->GetSocket(), (char*)&connectId, sizeof(int64) );
#else
	pConnect->SendStart();
	successed = m_pNetMonitor->AddSendableMonitor(pConnect->GetSocket()->GetSocket(), (char*)&connectId, sizeof(int64) );
#endif
	if ( !successed )
	{
		AutoLock lock( &m_connectsMutex );
		ConnectList::iterator itNetConnect = m_connectList.find( connectId );
		if ( itNetConnect == m_connectList.end() ) return 0;//底层已经主动断开
		CloseConnect( itNetConnect );
		pConnect->Release();
		return 0;
	}

	m_pNetServer->OnConnect( pConnect->m_host );
	/*
		监听连接
		※必须等OnConnect业务完成，才可以开始监听连接上的IO事件
		否则，可能业务层尚未完成连接初始化工作，就收到OnMsg通知，
		导致业务层不知道该如何处理消息
		
		※尚未加入监听，pConnect对象不存在并发线程访问
		如果OnConnect业务中，没有关闭连接，才能加入监听

		※如果不检查m_bConnect，则AddRecv有可能成功，导致OnData有机会触发。
		因为CloseConnect方法只是设置了关闭连接的标志，并将NetConnect从连接列表删除，
		并没有真的关闭socket。
		这是为了保证socket句柄在NetServer::OnClose业务完成前，不被系统重复使用，

		真正关闭是在NetEngine::CloseWorker()里是另外一个线程了。
		所以如果OnConnect业务中调用了关闭，但在CloseWorker线程执行前，
		在这里仍然有可能先被执行，监听成功，而这个监听是不希望发生的
	*/
	if ( pConnect->m_bConnect )
	{
#ifdef WIN32
		IOCP_DATA iocpData;
		iocpData.connectId = connectId;
		iocpData.buf = (char*)(pConnect->PrepareBuffer(BUFBLOCK_SIZE)); 
		iocpData.bufSize = BUFBLOCK_SIZE; 
		successed = m_pNetMonitor->AddRecv( pConnect->GetSocket()->GetSocket(), (char*)&iocpData, sizeof(IOCP_DATA) );
#else
		successed = m_pNetMonitor->AddDataMonitor( pConnect->GetSocket()->GetSocket(), (char*)&connectId, sizeof(int64) );
#endif
		if ( !successed )
		{
			AutoLock lock( &m_connectsMutex );
			ConnectList::iterator itNetConnect = m_connectList.find( connectId );
			if ( itNetConnect == m_connectList.end() ) return 0;//底层已经主动断开
			CloseConnect( itNetConnect );
		}
	}
	pConnect->Release();
	return 0;
}

void NetEngine::OnClose( int64 connectId )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(connectId);
	if ( itNetConnect == m_connectList.end() )return;//底层已经主动断开
	CloseConnect( itNetConnect );
	lock.Unlock();
}

void* NetEngine::CloseWorker( NetConnect *pConnect )
{
	SetServerClose(pConnect);//连接的服务断开
	m_pNetServer->OnCloseConnect( pConnect->m_host );
	/*
		以下pConnect->GetSocket()->Close();操作
		是V1.51版中，从CloseConnect( ConnectList::iterator it )中移动过来
		推迟执行close

		确保业务层完成close业务后，系统才可以再利用socket句柄
		详细原因，参考CloseConnect( ConnectList::iterator it )中注释
	*/
	pConnect->GetSocket()->Close();
	pConnect->Release();//使用完毕释放共享对象
	return 0;
}

connectState NetEngine::OnData( int64 connectId, char *pData, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(connectId);//client列表里查找
	if ( itNetConnect == m_connectList.end() ) return cs;//底层已经断开

	NetConnect *pConnect = itNetConnect->second;
	pConnect->RefreshHeart();
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();//确保业务层占有对象后，HeartMonitor()才有机会检查pConnect的状态
	try
	{
		cs = RecvData( pConnect, pData, uSize );//派生类实现
		if ( unconnect == cs )
		{
			pConnect->Release();//使用完毕释放共享对象
			OnClose( connectId );
			return cs;
		}
		/*
			避免并发MsgWorker，也就是避免并发读

			与MsgWorker的并发情况分析
			情况1：MsgWorker已经return
				那么AtomAdd返回0，触发新的MsgWorker，未并发

			情况2：MsgWorker未完成，分2种情况
				情况1：这里先AtomAdd
				必然返回非0，因为没有发生过AtomDec
				放弃触发MsgWorker
				遗漏OnMsg？
				不会！那么看MsgWorker()，AtomAdd返回非0，所以AtomDec必然返回>1，
				MsgWorker()会再循环一次OnMsg
				没有遗漏OnMsg，无并发
			情况2：MsgWorker先AtomDec
				必然返回1，因为MsgWorker循环中首先置了1，而中间又没有AtomAdd发生
				MsgWorker退出循环
				然后这里AtomAdd，必然返回0，触发新的MsgWorker，未并发
		 */
		if ( 0 < AtomAdd(&pConnect->m_nReadCount, 1) ) 
		{
			pConnect->Release();//使用完毕释放共享对象
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
		if ( !pConnect->m_bConnect ) 
		{
			pConnect->m_nReadCount = 0;
			break;
		}
		pConnect->m_nReadCount = 1;
		m_pNetServer->OnMsg( pConnect->m_host );//无返回值，避免框架逻辑依赖于客户实现
		if ( pConnect->IsReadAble() ) continue;
		if ( 1 == AtomDec(&pConnect->m_nReadCount,1) ) break;//避免漏接收
	}
	//触发OnClose(),确保NetServer::OnClose()一定在所有NetServer::OnMsg()完成之后
	if ( !pConnect->m_bConnect ) NotifyOnClose(pConnect);
	pConnect->Release();//使用完毕释放共享对象
	return 0;
}

connectState NetEngine::RecvData( NetConnect *pConnect, char *pData, unsigned short uSize )
{
	return unconnect;
}

//关闭一个连接
void NetEngine::CloseConnect( int64 connectId )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find( connectId );
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

//响应发送完成事件
connectState NetEngine::OnSend( int64 connectId, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(connectId);
	if ( itNetConnect == m_connectList.end() )return cs;//底层已经主动断开
	NetConnect *pConnect = itNetConnect->second;
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();//确保业务层占有对象后，HeartMonitor()才有机会检查pConnect的状态
	try
	{
		if ( pConnect->m_bConnect ) cs = SendData(pConnect, uSize);
	}
	catch(...)
	{
	}
	pConnect->Release();//使用完毕释放共享对象
	return cs;
	
}

connectState NetEngine::SendData(NetConnect *pConnect, unsigned short uSize)
{
	return unconnect;
}

bool NetEngine::Listen(int port)
{
	AutoLock lock(&m_listenMutex);
	pair<map<int,int>::iterator,bool> ret 
		= m_serverPorts.insert(map<int,int>::value_type(port,INVALID_SOCKET));
	map<int,int>::iterator it = ret.first;
	if ( !ret.second && INVALID_SOCKET != it->second ) return true;
	if ( m_stop ) return true;

	it->second = ListenPort(port);
	if ( INVALID_SOCKET == it->second ) return false;
	return true;
}

int NetEngine::ListenPort(int port)
{
	return INVALID_SOCKET;
}

bool NetEngine::ListenAll()
{
	bool ret = true;
	AutoLock lock(&m_listenMutex);
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

bool NetEngine::Connect(const char* ip, int port, void *pSvrInfo, int reConnectTime)
{
	uint64 addr64 = 0;
	if ( !addrToI64(addr64, ip, port) ) return false;
	
	AutoLock lock(&m_serListMutex);
	
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
	if ( NetEngine::success == ret )
	{
		pSvr->state = SVR_CONNECT::connected;
		OnConnect(pSvr->sock, pSvr);
	}
	else if ( NetEngine::waitReulst == ret )
	{
		pSvr->state = SVR_CONNECT::connectting;
		m_wakeConnectThread.Notify();
	}
	else //报告失败
	{
		pSvr->state = SVR_CONNECT::unconnectting;
		m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectFailed), this, pSvr );
	}
	
	return true;
}

NetEngine::ConnectResult NetEngine::ConnectOtherServer(const char* ip, int port, int &svrSock)
{
	svrSock = INVALID_SOCKET;
	Socket sock;//监听socket
	if ( !sock.Init( Socket::tcp ) ) return NetEngine::cannotCreateSocket;
	sock.SetSockMode();
// 	bool successed = sock.Connect(ip, port);
	svrSock = sock.Detach();					//清除sock相关数据
	return AsycConnect(svrSock, ip, port);		//
}

bool NetEngine::ConnectAll()
{
	if ( m_stop ) return false;
	AutoLock lock(&m_serListMutex);
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
			if ( NetEngine::success == ret )
			{
				pSvr->state = SVR_CONNECT::connected;
				OnConnect(pSvr->sock, pSvr);
			}
			else if ( NetEngine::waitReulst == ret )
			{
				pSvr->state = SVR_CONNECT::connectting;
				m_wakeConnectThread.Notify();
			}
			else //报告失败
			{
				pSvr->state = SVR_CONNECT::unconnectting;
				m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectFailed), this, pSvr );
			}
			itSvr++;
		}
	}
	
	return true;
}

void NetEngine::SetServerClose(NetConnect *pConnect)
{
	if ( !pConnect->m_host.IsServer() ) return;
	int sock = pConnect->GetSocket()->GetSocket();
	AutoLock lock(&m_serListMutex);
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
void NetEngine::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, unsigned int msgsize, int *filterGroupIDs, int filterCount )
{
	//////////////////////////////////////////////////////////////////////////
	//关闭无心跳的连接
	ConnectList::iterator it;
	NetConnect *pConnect;
	vector<NetConnect*> recverList;
	//加锁将所有广播接收连接复制到一个队列中
	AutoLock lock( &m_connectsMutex );
	for ( it = m_connectList.begin(); it != m_connectList.end(); it++ )
	{
		pConnect = it->second;
		if ( !pConnect->IsInGroups(recvGroupIDs, recvCount) 
			|| pConnect->IsInGroups(filterGroupIDs, filterCount) ) continue;
		recverList.push_back(pConnect);
		AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	}
	lock.Unlock();
	
	//向队列中的连接开始广播
	vector<NetConnect*>::iterator itv = recverList.begin();
	for ( ; itv != recverList.end(); itv++ )
	{
		pConnect = *itv;
		if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
		pConnect->Release();//使用完毕释放共享对象
	}
}

//向某主机发送消息(业务层接口)
bool NetEngine::SendMsg( int64 hostID, char *msg, unsigned int msgsize )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(hostID);
	if ( itNetConnect == m_connectList.end() ) return false;//底层已经主动断开
	NetConnect *pConnect = itNetConnect->second;
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();
	bool ret = false;
	if ( pConnect->m_bConnect ) ret = pConnect->SendData((const unsigned char*)msg,msgsize);
	pConnect->Release();//使用完毕释放共享对象

	return ret;
}

const char* NetEngine::GetInitError()//取得启动错误信息
{
	return m_startError.c_str();
}

void* NetEngine::ConnectThread(void*)
{
	SVR_CONNECT** clientList = (SVR_CONNECT**)new mdk::uint64[20000];
	int clientCount = 0;
	int i = 0;
	SVR_CONNECT *pSvr = NULL;
#ifndef WIN32
	m_hEPoll = epoll_create(20000);
	mdk_assert( -1 != m_hEPoll );
	m_events = new epoll_event[20000];	//epoll事件
	mdk_assert( NULL != m_events );
#endif

	bool isEnd = true;//已经到达m_keepIPList末尾
	while ( !m_stop )
	{
		//复制所有connectting状态的sock到监听列表
		clientCount = 0;
		isEnd = true;
		{
			AutoLock lock(&m_serListMutex);
			map<uint64,vector<SVR_CONNECT*> >::iterator it = m_keepIPList.begin();
			for ( ; it != m_keepIPList.end() && clientCount < 20000; it++ )
			{
				for ( i = 0; i < it->second.size() && clientCount < 20000; i++ )
				{
					pSvr = it->second[i];
					if ( SVR_CONNECT::connectting != pSvr->state ) continue;
					mdk_assert( INVALID_SOCKET != pSvr->sock );
					clientList[clientCount++] = pSvr;
				}
			}
			if ( it != m_keepIPList.end() ) isEnd = false;
		}
#ifndef WIN32
		bool finished = EpollConnect( clientList, clientCount );
#else
		bool finished = SelectConnect( clientList, clientCount );
#endif
		/*
			从m_keepIPList中拿出来的所有链接都已经返回结果，且m_keepIPList中没有链接在等待处理，
			则等待新的链接任务被放入m_keepIPList
		*/
		if ( finished && isEnd ) m_wakeConnectThread.Wait();
	}
	uint64 *p = (uint64*)clientList;
	delete[]p;
#ifndef WIN32
	delete[]m_events;
#endif
	return NULL;
}

#ifndef WIN32
#include <netdb.h>
#endif

NetEngine::ConnectResult NetEngine::AsycConnect( int svrSock, const char *lpszHostAddress, unsigned short nHostPort )
{
	if ( NULL == lpszHostAddress ) return NetEngine::invalidParam;
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
		return NetEngine::success;
	}
#ifndef WIN32
	if ( EINPROGRESS == errno ) 
	{
		return NetEngine::waitReulst;
	}
#else
	int nError = GetLastError();
	if ( WSAEWOULDBLOCK == nError ) 
	{
		return NetEngine::waitReulst;
	}
#endif

	return NetEngine::faild;
}

void* NetEngine::ConnectFailed( NetEngine::SVR_CONNECT *pSvr )
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

bool NetEngine::EpollConnect( SVR_CONNECT **clientList, int clientCount )
{
	bool finished = true;
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

	//////////////////////////////////////////////////////////////////////////
	//循环epoll_wait()直到真实失败or超时or返回事件
	int count = 0;
	while ( true )
	{
		count = epoll_wait(m_hEPoll, m_events, 20000, 20000 );
		if ( -1 == count ) 
		{
			if ( EINTR == errno ) continue;//假失败
		}
		break;
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

bool NetEngine::SelectConnect( SVR_CONNECT **clientList, int clientCount )
{
	bool finished = true;
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
		outtime.tv_sec = 20;
		outtime.tv_usec = 0;
		int nSelectRet =::select( maxSocket + 1, &readfds, &sendfds, NULL, &outtime ); //检查读写状态
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

bool NetEngine::ConnectIsFinished( SVR_CONNECT *pSvr, bool readable, bool sendable, int api, int errorCode )
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
		m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectFailed), this, pSvr );
		return true;
	}

	pSvr->state = SVR_CONNECT::connected;
	OnConnect(svrSock, pSvr);
	return true;
}

//打开TCP_NODELAY
void NetEngine::OpenNoDelay()
{
	m_noDelay = true;
}

}
// namespace mdk

