// NetConnect.cpp: implementation of the NetConnect class.
//
//////////////////////////////////////////////////////////////////////

#include "../../../include/frame/netserver/NetConnect.h"
#include "../../../include/frame/netserver/NetEventMonitor.h"
#include "../../../include/frame/netserver/NetEngine.h"
#include "../../../include/frame/netserver/HostData.h"
#include "../../../include/mdk/atom.h"
#include "../../../include/mdk/MemoryPool.h"
#include "../../../include/mdk/mapi.h"
using namespace std;

namespace mdk
{

NetConnect::NetConnect(int sock, int listenSock, bool bIsServer, NetEventMonitor *pNetMonitor, NetEngine *pEngine, MemoryPool *pMemoryPool)
:m_socket(sock,Socket::tcp)
{
	m_pMemoryPool = pMemoryPool;
	m_useCount = 1;
	m_pEngine = pEngine;
	m_pNetMonitor = pNetMonitor;
	m_id = -1;
	m_host.m_pConnect = this;
	m_nReadCount = 0;
	m_bReadAble = false;

	m_nSendCount = 0;//正在进行发送的线程数
	m_bSendAble = false;//io缓冲中有数据需要发送
	m_bConnect = true;//只有发现连接才创建对象，所以对象创建，就一定是连接状态
	m_nDoCloseWorkCount = 0;//没有执行过NetServer::OnClose()
	m_bIsServer = bIsServer;
#ifdef WIN32
	Socket::InitForIOCP(sock, listenSock);
#endif
	m_socket.InitPeerAddress();
	m_socket.InitLocalAddress();
 	m_pHostData = NULL;
	m_autoFreeData = true;
	m_pSvrInfo = NULL;
}

NetConnect::~NetConnect()
{
	/*
		不用检查m_autoFree
		if m_autoFree = true,应该释放
		if m_autoFree = false
			if data与host解除了关联,则m_pHostData=NULL
			else if data被析构m_pHostData=NULL
			else data中存在host的引用,host不可能被析构
		所以NULL != m_pHostData就一定是代理模式，执行Release()
	*/
	if ( NULL != m_pHostData ) m_pHostData->Release();
	m_pHostData = NULL;
}

void NetConnect::Release()
{
	if ( 1 == AtomDec(&m_useCount, 1) )
	{
		m_host.m_pConnect = NULL;
		if ( NULL == m_pMemoryPool ) 
		{
			delete this;
			return;
		}
		this->~NetConnect();
		m_pMemoryPool->Free(this);
	}
}

void NetConnect::SetID( int64 connectId )
{
	m_id = connectId;
}

void NetConnect::RefreshHeart()
{
	m_tLastHeart = time( NULL );
}

time_t NetConnect::GetLastHeart()
{
	return m_tLastHeart;
}

unsigned char* NetConnect::PrepareBuffer(unsigned short uRecvSize)
{
	return m_recvBuffer.PrepareBuffer( uRecvSize );
}

void NetConnect::WriteFinished(unsigned short uLength)
{
	m_recvBuffer.WriteFinished( uLength );
}

bool NetConnect::IsReadAble()
{
	return m_bReadAble && 0 < m_recvBuffer.GetLength();
}

uint32 NetConnect::GetLength()
{
	return m_recvBuffer.GetLength();
}

bool NetConnect::ReadData( unsigned char* pMsg, unsigned int uLength, bool bClearCache )
{
	m_bReadAble = m_recvBuffer.ReadData( pMsg, uLength, bClearCache );
	if ( !m_bReadAble ) uLength = 0;
	
	return m_bReadAble;
}

bool NetConnect::SendData( const unsigned char* pMsg, unsigned int uLength )
{
	unsigned char *ioBuf = NULL;
	int nSendSize = 0;
	AutoLock lock(&m_sendMutex);//回复与主动通知存在并发send
	if ( 0 >= m_sendBuffer.GetLength() )//没有等待发送的数据，可直接发送
	{
		nSendSize = m_socket.Send( pMsg, uLength );
	}
	if ( Socket::seError == nSendSize ) return false;//发生错误，连接可能已断开
	if ( uLength == nSendSize ) return true;//所有数据已发送，返回成功
	
	//数据加入发送缓冲，交给底层去发送
	uLength -= nSendSize;
	while ( true )
	{
		if ( uLength > BUFBLOCK_SIZE )
		{
			ioBuf = m_sendBuffer.PrepareBuffer( BUFBLOCK_SIZE );
			memcpy( ioBuf, &pMsg[nSendSize], BUFBLOCK_SIZE );
			m_sendBuffer.WriteFinished( BUFBLOCK_SIZE );
			nSendSize += BUFBLOCK_SIZE;
			uLength -= BUFBLOCK_SIZE;
		}
		else
		{
			ioBuf = m_sendBuffer.PrepareBuffer( uLength );
			memcpy( ioBuf, &pMsg[nSendSize], uLength );
			m_sendBuffer.WriteFinished( uLength );
			break;
		}
	}
	if ( !SendStart() ) return true;//已经在发送
#ifdef WIN32
	IOCP_DATA iocpData;
	iocpData.connectId = m_id;
	iocpData.buf = NULL; 
	iocpData.bufSize = 0;
	return m_pNetMonitor->AddSend( m_socket.GetSocket(), (char*)&iocpData, sizeof(IOCP_DATA) );
#else
	return m_pNetMonitor->AddSend( m_socket.GetSocket(), (char*)&m_id, sizeof(int) );
#endif
	return true;
}

Socket* NetConnect::GetSocket()
{
	return &m_socket;
}

int64 NetConnect::GetID()
{
	return m_id;
}

//开始发送流程
bool NetConnect::SendStart()
{
	if ( 0 != AtomAdd(&m_nSendCount,1) ) return false;//只允许存在一个发送流程
	return true;
}

//结束发送流程
void NetConnect::SendEnd()
{
	m_nSendCount = 0;
}

void NetConnect::Close()
{
	m_pEngine->CloseConnect(m_id);
}

bool NetConnect::IsServer()
{
	return m_bIsServer;
}

void NetConnect::InGroup( int groupID )
{
	m_groups.insert(map<int,int>::value_type(groupID,groupID));
}

void NetConnect::OutGroup( int groupID )
{
	map<int,int>::iterator it;
	it = m_groups.find(groupID);
	if ( it == m_groups.end() ) return;
	m_groups.erase(it);
}

bool NetConnect::IsInGroups( int *groups, int count )
{
	int i = 0;
	for ( i = 0; i < count; i++ )
	{
		if ( m_groups.end() != m_groups.find(groups[i]) ) return true;
	}
	
	return false;
}

void NetConnect::GetServerAddress( string &ip, int &port )
{
	if ( this->m_bIsServer ) m_socket.GetPeerAddress( ip, port );
	else m_socket.GetLocalAddress( ip, port );
	return;
}

void NetConnect::GetAddress( string &ip, int &port )
{
	if ( !this->m_bIsServer ) m_socket.GetPeerAddress( ip, port );
	else m_socket.GetLocalAddress( ip, port );
	return;
}

void NetConnect::SetData( HostData *pData, bool autoFree )
{
	m_autoFreeData = autoFree;
	//释放已设置的数据或引用计数
	if ( NULL != m_pHostData ) 
	{
		if ( m_pHostData->m_autoFree ) return;//自由模式，不能解除关联
		NetHost unconnectHost;
		m_pHostData->SetHost(&unconnectHost);//关联到未连接的host
		mdk::AutoLock lock(&m_mutexData);
		m_pHostData->Release();//释放数据,并发GetData()分析，参考HostData::Release()内部注释
		/*
			与GetData并发
			可能返回NULL，也可能返回新HostData，也可能返回无效的HostData
			就算提前到罪愆执行，结果也一样
			就算对整个SetData加lock控制，也不会有多大改善
		*/
		m_pHostData = NULL;//解除host与data的绑定
	}

	if ( NULL == pData ) return;

	if ( -1 == AtomAdd(&pData->m_refCount, 1) ) return; //有线程完成了Release()的释放检查，对象即将被释放，放弃关联
	pData->m_autoFree = autoFree;
	/*
		autoFree = true
		HostData的生命周期由框架管理
		框架保证了，地址的有效性，
		直接复制NetHost指针,到HostData中,不增加引用计数.
		

		autoFree = false
		HostData的生命周期由用户自行管理，框架不管
		则复制NetHost对象到HostData中,引用计数增加，表示有用户在访问NetHost对象，
		确保在用户释放HostData之前,NetHost永远不会被框架释放。
	*/
	if ( autoFree ) 
	{
		/*
			代理模式，不需要增加引用计数，减回来
			前面+1检查能通过，旧值最小应该=1，否则说明+1之后，外部又发生了重复Release()，是不合理的，触发断言
		*/
		if ( 1 > AtomDec(&pData->m_refCount, 1) ) 
		{
			mdk::mdk_assert(false);
			return; 
		}
		pData->m_pHost = &m_host;//代理模式，copy规则限制了，pData不会超出host生命周期，只要pData存在，m_pHost必然存在，不必copy
	}
	else //自主模式
	{
		pData->m_hostRef = m_host;//确保m_pHost指向安全内存，将host做1份copy
	}
	m_pHostData = pData;//必须最后，确保GetData()并发时，返回的时是完整数据

	return;
}

HostData* NetConnect::GetData()
{
	if ( m_autoFreeData ) return m_pHostData;

	mdk::AutoLock lock(&m_mutexData);
	if ( NULL == m_pHostData ) return NULL;
	if ( !m_pHostData->m_autoFree ) 
	{
		//自主模式，需要记录引用计数
		if ( -1 == AtomAdd(&m_pHostData->m_refCount, 1) ) //取数据时，有线程执行了SetData(NULL)解除关联，绝对不会是Release()释放数据 
		{
			return NULL;
		}
	}
	return m_pHostData;
}

void NetConnect::SetSvrInfo(void *pData)
{
	m_pSvrInfo = pData;
}

void* NetConnect::GetSvrInfo()
{
	return m_pSvrInfo;
}

}//namespace mdk

