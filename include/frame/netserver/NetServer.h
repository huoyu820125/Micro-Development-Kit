#ifndef MDK_C_NET_SERVER_H
#define MDK_C_NET_SERVER_H

#include "mdk/Thread.h"

namespace mdk
{
	class NetEngine;
	class NetHost;
/**
 * 网络服务器类
 * 接收消息，执行业务处理
 * 
 */
class NetServer
{
	friend class NetEngine;
private:
	/*
	 * 网卡：负责网络层事务处理，执行特定的网络通信策略(IOCP、EPoll或传统的select)
	 * 要跟换网络策略，只要跟换网络策略类即可
	 * 
	 */
	NetEngine* m_pNetCard;
	//主线程
	Thread m_mainThread;
	bool m_bStop;
	
protected:
	void* TMain(void* pParam);
	/*
	 *	服务器回调主线程
	 *	Start()后
	 *	用户可以忽略此方法，自己另外创建主线程
	 */
	virtual void* Main(void* pParam){ return 0; }
	
	/**
	 * 新连接事件回调方法
	 * 
	 * 派生类实现具体连接业务处理
	 * 
	 */
	virtual void OnConnect(NetHost* pClient){}
	/**
	 * 连接关闭事件，回调方法
	 * 
	 * 派生类实现具体断开连接业务处理
	 * 
	 */
	virtual void OnCloseConnect(NetHost* pClient){}
	/**
	 * 数据到达，回调方法
	 * 
	 * 派生类实现具体断开连接业务处理
	 * 
	*/
	virtual void OnMsg(NetHost* pClient){}

	//服务器正常返回true，否则返回false
	bool IsOk();
 
public:
	NetServer();
	virtual ~NetServer();
	/**
	 * 运行服务器
	 * 成功返回NULL
	 * 失败返回失败原因
	 */
	const char* Start();
	/**
	 * 关闭服务器
	 */
	void Stop();
	/*
	 *	等待服务器停止
	 */
	void WaitStop();

	//设置单个服务器进程可能承载的平均连接数，默认5000
	void SetAverageConnectCount(int count);
	//设置心跳时间，不设置则，服务器不检查心跳
	void SetHeartTime( int nSecond );
	//设置网络IO线程数量
	void SetIOThreadCount(int nCount);
	//设置工作线程数
	void SetWorkThreadCount(int nCount);
	//监听某个端口，可多次调用监听多个端口
	bool Listen(int port);
	//连接外部服务器，可多次调用连接多个外部服务器
	//※不要连接自身，服务器未做此测试，可能出现bug
	bool Connect(const char *ip, int port);
	/*
		广播消息
			配合NetHost::InGroup(),NetHost::OutGroup()使用
			向属于recvGroupIDs中任意一组，同时过滤掉属于filterGroupIDs中任意一组的主机，发送消息
		例如：
			A B C3个主机
			A属于 1 2 4
			B属于 2 3 5
			C属于 1 3 5
			D属于 2 3
			E属于 3 5
			
			BroadcastMsg( {1,3}, 2, msg, len, {5}, 1 );
			向属于分组1或属于分组3,同时不属于分组5的主机发送消息，则AD都会收到消息，BCE被过滤

		也可以不理会这3个方法，自己创建管理连接分组
  
	 */
	void BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount );
	/*
		向某主机发送消息

		1.与NetHost::Send()的区别
			SendMsg()内部先在连接列表中，加锁查找对象，然后还是调用NetHost::Send()发送消息
		在已经得到NetHost对象的情况下，直接NetHost::Send()效率最高，且不存在锁竞争。
		
		2.什么时候用SendMsg()什么时候直接NetHost::Send()
			OnMsg() OnConnect()提供了当前NetHost对象，可以直接NetHost::Send()

			如果需要向其它连接发送消息，且没有事先保存NetHost对象，那就只能SendMsg
			如果事先保存了NetHost的对象，请参考NetHost::Hold()方法
		
	 */
	void SendMsg( int hostID, char *msg, int msgsize );
	/*
	 	关闭与主机的连接
	 */
	void CloseConnect( int hostID );
};

}  // namespace mdk
#endif //MDK_C_NET_SERVER_H
