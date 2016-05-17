// IOCPFrame.cpp: implementation of the IOCPFrame class.
//
//////////////////////////////////////////////////////////////////////

#include "../../../include/frame/netserver/IOCPMonitor.h"
#include "../../../include/frame/netserver/IOCPFrame.h"
#include "../../../include/frame/netserver/NetConnect.h"
#include "../../../include/mdk/atom.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

IOCPFrame::IOCPFrame()
{
#ifdef WIN32
	m_pNetMonitor = new IOCPMonitor;
#endif
}

IOCPFrame::~IOCPFrame()
{
	if ( NULL != m_pNetMonitor ) 
	{
		delete m_pNetMonitor;
		m_pNetMonitor = NULL;
	}
}

void* IOCPFrame::NetMonitor( void* )
{
#ifdef WIN32
	IOCPMonitor::IO_EVENT e[1];
	int nCount = 1;
	while ( !m_stop )
	{
		if ( !m_pNetMonitor->WaitEvent( e, nCount, true ) ) return 0;
		if ( nCount <= 0 ) continue;
		switch( e->type )
		{
		case IOCPMonitor::connect :
			OnConnect( e->client, e->listenSock );
			break;
		case IOCPMonitor::recv :
			OnData( e->connectId, e->pData, e->uDataSize );
			break;
		case IOCPMonitor::close :
			OnClose( e->connectId );
			break;
		case IOCPMonitor::send :
			OnSend( e->connectId, e->uDataSize );
			break;
		default:
			break;
		}
	}
#endif
	
	return NULL;
}

//接收数据，投递失败表示连接已关闭，返回false, 因具体响应器差别，需要派生类中实现
connectState IOCPFrame::RecvData( NetConnect *pConnect, char *pData, unsigned short uSize )
{
	pConnect->WriteFinished( uSize );
	IOCP_DATA iocpData;
	iocpData.connectId = pConnect->GetID();
	iocpData.buf = (char*)(pConnect->PrepareBuffer(BUFBLOCK_SIZE)); 
	iocpData.bufSize = BUFBLOCK_SIZE; 
	if ( !m_pNetMonitor->AddRecv(  pConnect->GetSocket()->GetSocket(), 
		(char*)&iocpData, sizeof(IOCP_DATA) ) )
	{
		return unconnect;
	}

	return ok;
}

connectState IOCPFrame::SendData(NetConnect *pConnect, unsigned short uSize)
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
	
	IOCP_DATA iocpData;
	iocpData.connectId = pConnect->GetID();
	iocpData.buf = (char*)buf; 
	if ( nLength > BUFBLOCK_SIZE )
	{
		pConnect->m_sendBuffer.ReadData(buf, BUFBLOCK_SIZE, false);
		iocpData.bufSize = BUFBLOCK_SIZE; 
		m_pNetMonitor->AddSend( pConnect->GetSocket()->GetSocket(), (char*)&iocpData, sizeof(IOCP_DATA) );
	}
	else
	{
		pConnect->m_sendBuffer.ReadData(buf, nLength, false);
		iocpData.bufSize = nLength; 
		m_pNetMonitor->AddSend( pConnect->GetSocket()->GetSocket(), (char*)&iocpData, sizeof(IOCP_DATA) );
	}
	return ok;
}

int IOCPFrame::ListenPort(int port)
{
	Socket listenSock;//监听socket
	if ( !listenSock.Init( Socket::tcp ) ) return INVALID_SOCKET;
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
	int i = 0;
	for ( ; i < 500; i++ ) 
	{
		if ( !m_pNetMonitor->AddAccept( listenSock.GetSocket() ) )
		{
			listenSock.Close();
			return INVALID_SOCKET;
		}
	}
	return listenSock.Detach();
}

}//namespace mdk
