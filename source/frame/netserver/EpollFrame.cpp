// EpollFrame.cpp: implementation of the EpollFrame class.
//
//////////////////////////////////////////////////////////////////////

#include "frame/netserver/EpollMonitor.h"
#include "frame/netserver/EpollFrame.h"
#include "frame/netserver/NetConnect.h"
#include "mdk/atom.h"
#include "mdk/Lock.h"
#include "mdk/Socket.h"
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

EpollFrame::EpollFrame()
{
#ifndef WIN32
	m_pNetMonitor = new EpollMonitor;
#endif
}

EpollFrame::~EpollFrame()
{
#ifndef WIN32
	if ( NULL != m_pNetMonitor ) 
	{
		delete m_pNetMonitor;
		m_pNetMonitor = NULL;
	}
#endif
}

void* EpollFrame::NetMonitor( void* )
{
#ifndef WIN32
	int nCount = MAXPOLLSIZE;
	EpollMonitor::IO_EVENT e[MAXPOLLSIZE];
	int i = 0;
	Socket sock;
	Socket sockClient;
	map<SOCKET,int> ioList;
	map<SOCKET,int>::iterator it;
	int eventType = 0;
	while ( !m_stop )
	{
		//没有可io的socket则等待新可io的socket
		//否则检查是否有新的可io的socket，有则取出加入到ioList中，没有也不等待
		//继续进行ioList中的socket进行io操作
		if ( 0 >= ioList.size() ) m_pNetMonitor->WaitEvent( e, nCount, true );
		else m_pNetMonitor->WaitEvent( e, nCount, false );

		//加入到ioList中
		for ( i = 0; i < nCount; i++ )
		{
			if ( EpollMonitor::epoll_accept == e[i].type )//连接类型直接执行业务 
			{
				OnConnect(e[i].sock, false);
				continue;
			}
			
			//对于recv send则加入到io列表，统一调度
			if ( EpollMonitor::epoll_in == e[i].type ) eventType = 1;//recv事件
			else eventType = 2;//send事件
			it = ioList.find(e[i].sock);
			if ( it == ioList.end() ) ioList.insert(map<SOCKET,int>::value_type(e[i].sock,eventType) );//增加可io的对象
			else it->second = it->second|eventType;//设置新事件
		}
		
		//遍历ioList，执行1次io
		
		for ( it = ioList.begin(); it != ioList.end(); it++ )
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
		it = ioList.begin();
		while (  it != ioList.end() ) 
		{
			if ( 0 == it->second ) 
			{
				ioList.erase(it);
				it = ioList.begin();
			}
			else it++;
		}
	}
#endif
				
	return NULL;
}

connectState EpollFrame::RecvData( NetConnect *pConnect, char *pData, unsigned short uSize )
{
#ifndef WIN32
	unsigned char* pWriteBuf = NULL;	
	int nRecvLen = 0;
	unsigned int nMaxRecvSize = 0;
	while ( nMaxRecvSize < 4096 )
	{
		pWriteBuf = pConnect->PrepareBuffer(BUFBLOCK_SIZE);
		nRecvLen = pConnect->GetSocket()->Receive(pWriteBuf, BUFBLOCK_SIZE);
		if ( nRecvLen < 0 ) return unconnect;
		if ( 0 == nRecvLen ) 
		{
			if ( !m_pNetMonitor->AddRecv(pConnect->GetSocket()->GetSocket(), NULL, 0) ) return unconnect;
			return wait_recv;
		}
		nMaxRecvSize += nRecvLen;
		pConnect->WriteFinished( nRecvLen );
	}
#endif
	return ok;
}

SOCKET EpollFrame::ListenPort(int port)
{
#ifndef WIN32
	Socket listenSock;//监听socket
	if ( !listenSock.Init( Socket::tcp ) ) return INVALID_SOCKET;
	listenSock.SetSockMode();
	if ( !listenSock.StartServer( port ) ) 
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}
	m_pNetMonitor->AddMonitor( listenSock.GetSocket() );
	if ( !m_pNetMonitor->AddAccept( listenSock.GetSocket() ) ) 
	{
		listenSock.Close();
		return INVALID_SOCKET;
	}

	return listenSock.Detach();
#endif
	return INVALID_SOCKET;
}

bool EpollFrame::MonitorConnect(NetConnect *pConnect)
{
#ifndef WIN32
	m_pNetMonitor->AddMonitor( pConnect->GetSocket()->GetSocket() );
	return m_pNetMonitor->AddRecv( pConnect->GetSocket()->GetSocket(), NULL, 0 );
#endif
	return false;
}

connectState EpollFrame::SendData(NetConnect *pConnect, unsigned short uSize)
{
#ifndef WIN32
	try
	{
		connectState cs = wait_send;//默认为等待状态
		//////////////////////////////////////////////////////////////////////////
		//执行发送
		unsigned char buf[4096];
		int nSize = 0;
		int nSendSize = 0;
		int nFinishedSize = 0;
		nSendSize = pConnect->m_sendBuffer.GetLength();
		if ( 0 < nSendSize )
		{
			nSize = 0;
			//一次发送4096byte
			if ( 4096 < nSendSize )//1次发不完，设置为就绪状态
			{
				pConnect->m_sendBuffer.ReadData(buf, 4096, false);
				nSize += 4096;
				nSendSize -= 4096;
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
		//发送流程开始
		m_pNetMonitor->AddSend( pConnect->GetSocket()->GetSocket(), NULL, 0 );
		return cs;
	}
	catch(...)
	{
	}
#endif
	return ok;
}

}//namespace mdk
