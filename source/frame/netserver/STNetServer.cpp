#include <assert.h>

#include "../../../include/frame/netserver/STNetServer.h"
#include "../../../include/frame/netserver/STNetEngine.h"


namespace mdk
{

STNetServer::STNetServer()
{
	m_pNetCard = new STNetEngine;
	m_pNetCard->m_pNetServer = this;
	m_bStop = true;
}
 
STNetServer::~STNetServer()
{
	delete m_pNetCard;
}

void* STNetServer::TMain(void* pParam)
{
	Main(pParam);
	return 0;
}

const char* STNetServer::Start()
{
	if ( NULL == this->m_pNetCard ) return "no class STNetServer object";
	if ( !m_pNetCard->Start() ) return m_pNetCard->GetInitError();
	m_bStop = false;
	m_mainThread.Run(Executor::Bind(&STNetServer::TMain), this, NULL);
	return NULL;
}

void STNetServer::WaitStop()
{
	if ( NULL == this->m_pNetCard ) return;
	m_pNetCard->WaitStop();
	m_mainThread.WaitStop();
}

void STNetServer::Stop()
{
	m_bStop = true;
	m_mainThread.Stop( 3000 );
	if ( NULL != this->m_pNetCard ) m_pNetCard->Stop();
}


bool STNetServer::IsOk()
{
	return !m_bStop;
}


void STNetServer::SetAverageConnectCount(int count)
{
	m_pNetCard->SetAverageConnectCount(count);
}

void STNetServer::SetReconnectTime( int nSecond )
{
	m_pNetCard->SetReconnectTime(nSecond);
}

void STNetServer::SetHeartTime( int nSecond )
{
	m_pNetCard->SetHeartTime(nSecond);
}

bool STNetServer::Listen(int port)
{
	m_pNetCard->Listen(port);
	return true;
}

bool STNetServer::Connect(const char *ip, int port)
{
	m_pNetCard->Connect(ip, port);
	return true;
}

void STNetServer::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount )
{
	m_pNetCard->BroadcastMsg( recvGroupIDs, recvCount, msg, msgsize, filterGroupIDs, filterCount );
}

void STNetServer::SendMsg( int hostID, char *msg, int msgsize )
{
	m_pNetCard->SendMsg(hostID, msg, msgsize);
}

void STNetServer::CloseConnect( int hostID )
{
	m_pNetCard->CloseConnect( hostID );
}

}  // namespace mdk
