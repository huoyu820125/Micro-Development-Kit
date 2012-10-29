// TestServer.cpp: implementation of the TestServer class.
//
//////////////////////////////////////////////////////////////////////

#include "TestServer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TestServer::TestServer()
{
	SetIOThreadCount(4);//设置网络IO线程数量
	SetWorkThreadCount(4);//设置工作线程数
	Listen(8888);
	Listen(6666);
	Listen(9999);
//	Connect("127.0.0.1",8888);//连接自身，未测试，不建议这么做
}

TestServer::~TestServer()
{

}

/*
 *	服务器主线程
 */
void* TestServer::Main(void* pParam)
{
	while ( IsOk() )
	{
		//执行业务
#ifndef WIN32
		usleep( 1000 * 1000 );
#else
		Sleep( 1000 );
#endif
	}
	
	return 0;
}

/**
 * 新连接事件回调方法
 * 
 * 派生类实现具体连接业务处理
 * 
 */
void TestServer::OnConnect(mdk::NetHost* pClient)
{
	printf( "new client(%d) connect in\n", pClient->ID() );
	//保存连接的2种方式
	//方式1，保存NetHost对象指针
	//pClient->Hold();//保持对象，告诉服务器引擎，业务层在将来某些时候可能会访问pClient，
	//进行send close操作，不能释放pClient对象，直到pClient->Free()被调用
	//将pClient加入到从自己的列表中，以待将来需要访问时使用

	//方式2，保存NetHost ID
	//将pClient->ID()加入到从自己的列表中，以待将来需要send，close操作时，
	//直接将ID传递给SendMsg() CloseConnect()接口即可，不需要pClient->Hold()保持对象
	//在send close操作时，如果pClient已经被底层释放，接口会返回失败
}

/**
 * 连接关闭事件，回调方法
 * 
 * 派生类实现具体断开连接业务处理
 * 
 */
void TestServer::OnCloseConnect(mdk::NetHost* pClient)
{
	printf( "client(%d) close connect\n", pClient->ID() );
	//将pClient从自己的列表中删除
	//pClient->Free();//告诉服务器引擎，业务层将不再需要使用pClient对象，需要释放时即可释放
}

void TestServer::OnMsg(mdk::NetHost* pClient)
{
	//假设报文结构为：2byte表示数据长度+报文数据
	unsigned char c[256];
	/*
		读取数据长度，长度不足2byte直接返回，等待下次数据到达时再读取
		只读取2byte，false表示，不将读取到的数据从缓冲中删除，下次还是可以读到
	*/
	if ( !pClient->Recv( c, 2, false ) ) return;
	unsigned short len = 0;
	memcpy( &len, c, 2 );//得到数据长度
	len += 2;//报文长度 = 报文头长度+数据长度
	if ( len > 256 ) 
	{
		printf( "close client:invaild fromat msg\n" );
		pClient->Close();
		return;
	}
	if ( pClient->GetLength() < len + 2 ) return;//数据长度不够，等待下次再读取
	pClient->Recv(c, len);//将报文读出，并从接收缓冲中删除，绝对不可能长度不够，即使连接已经断开，也可以读到数据
	pClient->Send( c, len );//收到消息原样回复
}
