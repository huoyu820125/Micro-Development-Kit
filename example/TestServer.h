// TestServer.h: interface for the TestServer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TESTSERVER_H__CD091694_1C7D_42D0_A893_DB0B6482ADC8__INCLUDED_)
#define AFX_TESTSERVER_H__CD091694_1C7D_42D0_A893_DB0B6482ADC8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "frame/netserver/NetServer.h"
#include "frame/netserver/NetHost.h"

class TestServer : public mdk::NetServer  
{
public:
	TestServer();
	virtual ~TestServer();

protected:
	/*
	 *	服务器主线程
	 */
	void* Main(void* pParam);
	
	/**
	 * 新连接事件回调方法
	 * 
	 * 派生类实现具体连接业务处理
	 * 
	 */
	void OnConnect(mdk::NetHost* pClient);
	/**
	 * 连接关闭事件，回调方法
	 * 
	 * 派生类实现具体断开连接业务处理
	 * 
	 */
	void OnCloseConnect(mdk::NetHost* pClient);
	/**
	 * 数据到达，回调方法
	 * 
	 * 派生类实现具体断开连接业务处理
	 * 
	*/
	void OnMsg(mdk::NetHost* pClient);
	
};

#endif // !defined(AFX_TESTSERVER_H__CD091694_1C7D_42D0_A893_DB0B6482ADC8__INCLUDED_)
