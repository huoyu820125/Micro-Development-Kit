#ifndef MDK_C_NET_SERVER_H
#define MDK_C_NET_SERVER_H

#include "STNetHost.h"

namespace mdk
{
	class STNetEngine;
	class STNetHost;
/**
 * 网络服务器基类(单线程版)
 * 接收消息，执行业务处理
 * 
 */
class STNetServer
{
	friend class STNetEngine;
private:
	/*
	 * 网卡：负责网络层事务处理，执行特定的网络通信策略(IOCP、EPoll或传统的select)
	 * 要跟换网络策略，只要跟换网络策略类即可
	 * 
	 */
	STNetEngine* m_pNetCard;
	bool m_bStop;
	
protected:
public:
	/*
		主函数，回调方法
			此方法将在主线程中循环被调用，直到此方法return 0,则主线程不再调用此方法
			主线程伪码如下
			while ( 没有停止 )
			{
				执行Main()
				等待网络IO发生(最多10秒)，如果有io发生，则执行OnConncect() OnMsg() OnCloseConnect()
				如果有未成功的链接，尝试链接
				检查所有链接的心跳
			}
			
	 		※不建议用户自己创建主线程，单线程版没有Lock控制，如果在自己的主线程中调用本类的方法将不保证有效，甚至崩溃
			如果觉得单线程排队时间太长，请使用多线程版
	*/
	virtual int Main(){ return 0; }
	
	/**
		有新连接进来，业务处理回调方法
		参数：
			host 连接进来的主机
			用于数据io和一些其它主机操作，具体参考STNetHost类
	 */
	virtual void OnConnect(STNetHost &host){}
	/*
	响应链接到地址失败的情况。
	reConnectSecond是调用Connect()时候的传入最后一个参数，表示底层在多长时间后会自动尝试再次链接这个地址，小于0表示不会尝试
	*/	
	virtual void OnConnectFailed( char *ip, int port, int reConnectTime ){}
	/**
		有连接断开，业务处理回调方法
		参数：
			host 连接断开的主机
			用于调用ID()方法，标识断开对象，不必Close()，引擎已经Close()过了，
			其它主机操作，具体参考STNetHost类
	*/
	virtual void OnCloseConnect(STNetHost &host){}
	/**
		有数据可读，业务处理回调方法
		参数：
			host 有数据可读的主机
			用于调用ID()方法，标识断开对象，不必Close()，引擎已经Close()过了，
			其它主机操作，具体参考STNetHost类
	*/
	virtual void OnMsg(STNetHost &host){}

	/*
		服务器状态检查，仅仅为main()方法中作为循环退出条件使用
		服务器Start()后返回true，Stop()后返回false
	*/
	bool IsOk();
 
public:
	STNetServer();
	virtual ~STNetServer();
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
	//设置自动重连时间,最小10s，不设置则，或设置小于等于0，服务器不重连
	//设置心跳时间,最小10s，不设置则，或设置小于等于0，服务器不检查心跳
	void SetHeartTime( int nSecond );
	//打开TCP_NODELAY模式（高吞吐）
	void OpenNoDelay();
	//监听某个端口，可多次调用监听多个端口
	bool Listen(int port);
	//异步连接外部服务器，可多次调用连接多个外部服务器
	//可对同一个ip端口，调用多次，产生多个链接
	//reConnectTime 此链接断开后，自动重连的等待时间，最小10s，不传递或传递小于等于0，则断开后不重连
	//※不要连接自身，服务器未做此测试，可能出现bug
	/*
		v1.93新增pSvrInfo参数
		用于绑定一个信息，在连接完成时可以通过NetHost.GetSvrInfo取得
		以确定host代表是哪个服务器
	*/
	bool Connect(const char *ip, int port, void *pSvrInfo = NULL, int reConnectTime = -1);
	/*
		广播消息
		向属于recvGroupIDs中任意一组，同时过滤掉属于filterGroupIDs中任意一组的主机，发送消息
		配合STNetHost::InGroup(),STNetHost::OutGroup()使用

		参数：
			recvGroupIDs		要接收该消息的分组列表
			recvCount		recvGroupIDs中分组数
			msg				消息
			msgsize			消息长度
			filterGroupIDs		不能接收该消息的分组列表
			filterCount		filterGroupIDs中分组数

		应用场景：
			游戏有很多地图，地图有唯一ID，作为分组ID
			NetHost1->InGroup(地图ID1) 玩家进入地图1
			NetHost2->InGroup(地图ID2) 玩家进入地图2
			BroadcastMsg({地图ID1, 地图ID2,}, 2,…)向地图1与地图2中的所有玩家发送消息


		例如：
			A B C3个主机
			A属于 1 2 4
			B属于 2 3 5
				C属于 1 3 5
				D属于 2 3
				E属于 3 5
				BroadcastMsg( {1,3}, 2, msg, len, {5}, 1 );
				向属于分组1或属于分组3,同时不属于分组5的主机发送消息，则AD都会收到消息，BCE被过滤

		用户也可以不理会该方法，自己创建管理分组
  
	 */
	void BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, unsigned int msgsize, int *filterGroupIDs, int filterCount );
	/*
		向某主机发送消息

					参数：
						hosteID	接收方id
						msg		消息
						msgsize	消息长度

		与STNetHost::Send()的区别
		SendMsg()内部先在连接列表中，加锁查找对象，然后还是调用STNetHost::Send()发送消息
		在已经得到STNetHost对象的情况下，直接STNetHost::Send()效率最高，且不存在锁竞争。
	 */
	void SendMsg( int hostID, char *msg, unsigned int msgsize );
	/*
	 	关闭与主机的连接
	 */
	void CloseConnect( int hostID );
};

}  // namespace mdk
#endif //MDK_C_NET_SERVER_H
