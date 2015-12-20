#ifndef MSWINSOCKUTIL_H_
#define MSWINSOCKUTIL_H_

//////////////////////////////////////////////////////////////////////////
//主要api函数AcceptEx和GetSocketAddr
//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
#include <winsock2.h>
#include <mswsock.h>

namespace mdk
{



	class MsWinsockUtil
	{
	public:
		//加载函数
		static void	LoadExtensionFunction( SOCKET ActiveSocket );	

		//connectex
		static void LoadClientFunction(SOCKET ActiveSocket);

		//AcceptEx
		static LPFN_ACCEPTEX				m_lpfnAccepteEx;			
		//GetAcceptExSockAddrs函数
		static LPFN_GETACCEPTEXSOCKADDRS	m_lpfnGetAcceptExSockAddrs;

		//connect
		static LPFN_CONNECTEX				m_lpnConnectEx;			//连接
	private:
		static bool LoadExtensionFunction( SOCKET ActiveSocket,	GUID FunctionID, void **ppFunc);
	};

}

#endif

#endif