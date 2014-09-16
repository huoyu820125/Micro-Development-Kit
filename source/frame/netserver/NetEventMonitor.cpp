// NetEventMonitor.cpp: implementation of the NetEventMonitor class.
//
//////////////////////////////////////////////////////////////////////

#include "../../../include/frame/netserver/NetEventMonitor.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace mdk
{

NetEventMonitor::NetEventMonitor()
{
	m_initError = "";
}

NetEventMonitor::~NetEventMonitor()
{

}

bool NetEventMonitor::Stop()
{
	return true;
}

bool NetEventMonitor::AddMonitor( SOCKET socket, char* pData, unsigned short dataSize )
{
	return true;
}

bool NetEventMonitor::AddConnectMonitor( SOCKET sock )
{
	return false;
}

bool NetEventMonitor::AddDataMonitor( SOCKET sock, char* pData, unsigned short dataSize )
{
	return false;
}

bool NetEventMonitor::AddSendableMonitor( SOCKET sock, char* pData, unsigned short dataSize )
{
	return false;
}

bool NetEventMonitor::DelMonitor( SOCKET socket )
{
	return true;
}

bool NetEventMonitor::WaitEvent( void *eventArray, int &count, bool block )
{
	return true;
}

bool NetEventMonitor::AddAccept(SOCKET socket)
{
	return true;
}

bool NetEventMonitor::AddRecv( SOCKET socket, char* pData, unsigned short dataSize )
{
	return true;
}

bool NetEventMonitor::AddSend( SOCKET socket, char* pData, unsigned short dataSize )
{
	return true;
}

const char* NetEventMonitor::GetInitError()
{	
	return m_initError.c_str();
}

}//namespace mdk
