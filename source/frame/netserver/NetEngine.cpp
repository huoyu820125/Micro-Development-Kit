
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
		ReConnectAll();
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

void NetEngine::ReConnectAll()
{
	if ( 0 >= m_nReconnectTime ) return;//无重连机制
	static time_t lastConnect = time(NULL);
	time_t curTime = time(NULL);
	if ( m_nReconnectTime > curTime - lastConnect ) return;
	lastConnect = curTime;
	ConnectAll();
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
	if ( 0 == AtomAdd(&pConnect->m_nReadCount, 1) ) NotifyOnClose(pConnect);
	pConnect->Release();//连接断开释放共享对象
	return;
}

void NetEngine::NotifyOnClose(NetConnect *pConnect)
{
	if ( 0 == AtomAdd(&pConnect->m_nDoCloseWorkCount, 1) )//只有1个线程执行OnClose，且仅执行1次
	{
		AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
		m_workThreads.Accept( Executor::Bind(&NetEngine::CloseWorker), this, pConnect);
	}
}

bool NetEngine::OnConnect( SOCKET sock, bool isConnectServer )
{
	NetConnect *pConnect = new (m_pConnectPool->Alloc())NetConnect(sock, isConnectServer, m_pNetMonitor, this, m_pConnectPool);
	if ( NULL == pConnect ) 
	{
		closesocket(sock);
		return false;
	}
	pConnect->GetSocket()->SetSockMode();
	//加入管理列表
	AutoLock lock( &m_connectsMutex );
	pConnect->RefreshHeart();
	pair<ConnectList::iterator, bool> ret = m_connectList.insert( ConnectList::value_type(pConnect->GetSocket()->GetSocket(),pConnect) );
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();
	//执行业务
	m_workThreads.Accept( Executor::Bind(&NetEngine::ConnectWorker), this, pConnect );
	return true;
}

void* NetEngine::ConnectWorker( NetConnect *pConnect )
{
	if ( !m_pNetMonitor->AddMonitor(pConnect->GetSocket()->GetSocket()) ) 
	{
		AutoLock lock( &m_connectsMutex );
		ConnectList::iterator itNetConnect = m_connectList.find( pConnect->GetSocket()->GetSocket() );
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
		if ( !m_pNetMonitor->AddRecv( 
			pConnect->GetSocket()->GetSocket(), 
			(char*)(pConnect->PrepareBuffer(BUFBLOCK_SIZE)), 
			BUFBLOCK_SIZE ) )
		{
			AutoLock lock( &m_connectsMutex );
			ConnectList::iterator itNetConnect = m_connectList.find( pConnect->GetSocket()->GetSocket() );
			if ( itNetConnect == m_connectList.end() ) return 0;//底层已经主动断开
			CloseConnect( itNetConnect );
		}
#else
		if ( !m_pNetMonitor->AddRecv( 
			pConnect->GetSocket()->GetSocket(), 
			NULL, 
			0 ) )
		{
			AutoLock lock( &m_connectsMutex );
			ConnectList::iterator itNetConnect = m_connectList.find( pConnect->GetSocket()->GetSocket() );
			if ( itNetConnect == m_connectList.end() ) return 0;//底层已经主动断开
			CloseConnect( itNetConnect );
		}
#endif
	}
	pConnect->Release();
	return 0;
}

void NetEngine::OnClose( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
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

connectState NetEngine::OnData( SOCKET sock, char *pData, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);//client列表里查找
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
			OnClose( sock );
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
		pConnect->m_nReadCount = 1;
		m_pNetServer->OnMsg( pConnect->m_host );//无返回值，避免框架逻辑依赖于客户实现
		if ( !pConnect->m_bConnect ) break;
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
void NetEngine::CloseConnect( SOCKET sock )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find( sock );
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	CloseConnect( itNetConnect );
}

//响应发送完成事件
connectState NetEngine::OnSend( SOCKET sock, unsigned short uSize )
{
	connectState cs = unconnect;
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(sock);
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
	
	SOCKET sock = pConnect->GetID();
	AutoLock lock(&m_serListMutex);
	map<uint64,SOCKET>::iterator it = m_serIPList.begin();
	for ( ; it != m_serIPList.end(); it++ )
	{
		if ( sock != it->second ) continue;
		it->second = INVALID_SOCKET;
		break;
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
	for ( it = m_connectList.begin(); m_nHeartTime > 0 && it != m_connectList.end(); it++ )
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
void NetEngine::SendMsg( int hostID, char *msg, unsigned int msgsize )
{
	AutoLock lock( &m_connectsMutex );
	ConnectList::iterator itNetConnect = m_connectList.find(hostID);
	if ( itNetConnect == m_connectList.end() ) return;//底层已经主动断开
	NetConnect *pConnect = itNetConnect->second;
	AtomAdd(&pConnect->m_useCount, 1);//业务层先获取访问
	lock.Unlock();
	if ( pConnect->m_bConnect ) pConnect->SendData((const unsigned char*)msg,msgsize);
	pConnect->Release();//使用完毕释放共享对象

	return;
}

const char* NetEngine::GetInitError()//取得启动错误信息
{
	return m_startError.c_str();
}

}
// namespace mdk

