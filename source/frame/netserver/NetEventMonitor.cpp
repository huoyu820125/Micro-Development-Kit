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

bool NetEventMonitor::AddMonitor( int socket, char* pData, unsigned short dataSize )
{
	return true;
}

bool NetEventMonitor::AddConnectMonitor( int sock )
{
	return false;
}

bool NetEventMonitor::AddDataMonitor( int sock, char* pData, unsigned short dataSize )
{
	return false;
}

bool NetEventMonitor::AddSendableMonitor( int sock, char* pData, unsigned short dataSize )
{
	return false;
}

bool NetEventMonitor::DelMonitor( int socket )
{
	return true;
}

bool NetEventMonitor::WaitEvent( void *eventArray, int &count, bool block )
{
	return true;
}

bool NetEventMonitor::AddAccept(int socket)
{
	return true;
}

bool NetEventMonitor::AddRecv( int socket, char* pData, unsigned short dataSize )
{
	return true;
}

bool NetEventMonitor::AddSend( int socket, char* pData, unsigned short dataSize )
{
	return true;
}

const char* NetEventMonitor::GetInitError()
{	
	return m_initError.c_str();
}

}//namespace mdk
