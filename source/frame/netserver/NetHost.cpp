#include "frame/netserver/NetHost.h"
#include "frame/netserver/NetConnect.h"
#include "mdk/Socket.h"
using namespace std;

namespace mdk
{

NetHost::NetHost( bool bIsServer )
:m_pConnect(NULL)
{
	m_bIsServer = bIsServer;
}

NetHost::~NetHost()
{
}
 
bool NetHost::Send(const unsigned char* pMsg, unsigned short uLength)
{
	return m_pConnect->SendData(pMsg, uLength);
	return true;
}

uint32 NetHost::GetLength()
{
	return m_pConnect->GetLength();
}

bool NetHost::Recv( unsigned char* pMsg, unsigned short uLength, bool bClearCache )
{
	return m_pConnect->ReadData( pMsg, uLength, bClearCache );
}

void NetHost::Close()
{
	m_pConnect->Close();
}

void NetHost::Hold()
{
	m_pConnect->WorkAccess();
	return;
}

void NetHost::Free()
{
	m_pConnect->WorkFinished();
	return;
}

bool NetHost::IsServer()
{
	return m_bIsServer;
}

int NetHost::ID()
{
	return m_pConnect->GetID();
}

//放入某分组
void NetHost::InGroup( int groupID )
{
	m_groups.insert(map<int,int>::value_type(groupID,groupID));
}

//从某分组删除
void NetHost::OutGroup( int groupID )
{
	map<int,int>::iterator it;
	it = m_groups.find(groupID);
	if ( it == m_groups.end() ) return;
	m_groups.erase(it);
}

}  // namespace mdk
