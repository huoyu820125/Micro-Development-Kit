#ifndef MDK_C_NET_SERVER_H
#define MDK_C_NET_SERVER_H

#include "../../../include/mdk/Thread.h"
#include "NetHost.h"

namespace mdk
{
	class NetEngine;
	class NetHost;
/**
 * 网络服务器基类
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
public:
	void* RemoteCall TMain(void* pParam);
	/*
		后台业务线程，回调方法
		
			服务器业务线程不做任何事情，直接调用此方法，此方法退出，则服务器业务线程退出
		※此线程退出，不表示服务器停止，这只是业务逻辑线程，
			服务器完全可以没有长期运行于后台的业务逻辑，只处理网络消息

	 	触发时机：服务器启动

		退出时机：
			Stop()被调用后，3s内不自己退出则被强制杀死
				IF业务中存在循环，可以使用IsOK()检查是否有Stop()被调用
				IF业务中存在线程挂起函数，需要在Stop()调用前自行发送信号唤醒线程正常结束
			
	 	用户也可以忽略此方法，自己创建主线程
	 */
	virtual void* Main(void* pParam){ return 0; }
	
	/**
		有新连接进来，业务处理回调方法
		参数：
			host 连接进来的主机
			用于数据io和一些其它主机操作，具体参考NetHost类
	 */
	virtual void OnConnect(NetHost &host){}
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
			其它主机操作，具体参考NetHost类
	*/
	virtual void OnCloseConnect(NetHost &host){}
	/**
		有数据可读，业务处理回调方法
		参数：
			host 有数据可读的主机
			用于调用ID()方法，标识断开对象，不必Close()，引擎已经Close()过了，
			其它主机操作，具体参考NetHost类
	*/
	virtual void OnMsg(NetHost &host){}

	/*
		服务器状态检查，仅仅为main()方法中作为循环退出条件使用
		服务器Start()后返回true，Stop()后返回false
	*/
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
	//设置心跳时间,最小10s，不设置则，或设置小于等于0，服务器不检查心跳
	void SetHeartTime( int nSecond );
	//设置网络IO线程数量，建设设置为CPU数量的1~2倍
	void SetIOThreadCount(int nCount);
	//设置工作线程数（即OnConnect OnMsg OnClose的并发数）
	void SetWorkThreadCount(int nCount);
	//设置工作线程启动回调函数
	void SetOnWorkStart( MethodPointer method, void *pObj, void *pParam );
	void SetOnWorkStart( FuntionPointer fun, void *pParam );
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
		配合NetHost::InGroup(),NetHost::OutGroup()使用

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

		与NetHost::Send()的区别
		SendMsg()内部先在连接列表中，加锁查找对象，然后还是调用NetHost::Send()发送消息
		在已经得到NetHost对象的情况下，直接NetHost::Send()效率最高，且不存在锁竞争。
	 */
	bool SendMsg( int64 hostID, char *msg, unsigned int msgsize );
	/*
	 	关闭与主机的连接
	 */
	void CloseConnect( int64 hostID );
};

}  // namespace mdk
#endif //MDK_C_NET_SERVER_H
