
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

const char* STNetServer::Start()
{
	if ( NULL == this->m_pNetCard ) return "no class STNetServer object";
	if ( !m_pNetCard->Start() ) return m_pNetCard->GetInitError();
	m_bStop = false;
	return NULL;
}

void STNetServer::WaitStop()
{
	if ( NULL == this->m_pNetCard ) return;
	m_pNetCard->WaitStop();
}

void STNetServer::Stop()
{
	m_bStop = true;
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

void STNetServer::SetHeartTime( int nSecond )
{
	m_pNetCard->SetHeartTime(nSecond);
}

bool STNetServer::Listen(int port)
{
	m_pNetCard->Listen(port);
	return true;
}

bool STNetServer::Connect(const char *ip, int port, void *pSvrInfo, int reConnectTime)
{
	m_pNetCard->Connect(ip, port, pSvrInfo, reConnectTime);
	return true;
}

void STNetServer::BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, unsigned int msgsize, int *filterGroupIDs, int filterCount )
{
	m_pNetCard->BroadcastMsg( recvGroupIDs, recvCount, msg, msgsize, filterGroupIDs, filterCount );
}

void STNetServer::SendMsg( int hostID, char *msg, unsigned int msgsize )
{
	m_pNetCard->SendMsg(hostID, msg, msgsize);
}

void STNetServer::CloseConnect( int hostID )
{
	m_pNetCard->CloseConnect( hostID );
}

//´ò¿ªTCP_NODELAY
void STNetServer::OpenNoDelay()
{
	m_pNetCard->OpenNoDelay();
}

}  // namespace mdk
