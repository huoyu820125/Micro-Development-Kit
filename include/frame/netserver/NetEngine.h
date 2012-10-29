#ifndef MDK_NET_ENGINE_H
#define MDK_NET_ENGINE_H

#include "mdk/Socket.h"
#include "mdk/Thread.h"
#include "mdk/Lock.h"
#include "mdk/ThreadPool.h"
#include "mdk/FixLengthInt.h"
#include "mdk/MemoryPool.h"

#include <map>
#include <vector>
#include <string>

namespace mdk
{
void XXSleep( long lSecond );
class Mutex;
class NetConnect;
class NetHost;
class NetEventMonitor;
class NetServer;
class MemoryPool;
typedef std::map<SOCKET,NetConnect*> ConnectList;
typedef std::vector<NetConnect*> ReleaseList;
	
/**
 * 服务器通信引擎类
 * 通信层对象类型
 * 使用一种通信策略（IOCP、EPoll或传统的select等）进行通信控制
 * 
 * 成员
 * 服务器网络对象
 * 客户端网络对象映射表
 * 接口
 * 启动
 * 停止
 * 
 * 方法
 * 建立新连接
 * 断开连接
 * 消息到达
 * 
 */
enum connectState
{
	ok = 0,
	unconnect = 1,
	wait_recv = 2,
	wait_send = 3,
};
class NetEngine
{
	friend class NetServer;
protected:
	std::string m_startError;//启动失败原因
	MemoryPool *m_pConnectPool;//NetConnect对象池
	int m_averageConnectCount;//平均连接数
	bool m_stop;//停止标志
	/**
		连接表
		map<unsigned long,NetConnect*>
		定时检查该列表中连接是有发送心跳，
		将没有心跳的，断开连接，丢入m_closedConnects中等待释放
	*/
	ConnectList m_connectList;
	/**
		已断开的连接列表
		vector<NetConnect*>
		使用NetConnect::GetOwner()方法,确定对象是否存在访问。
		定时检查该列表中对象是否已退出所有线程，执行释放
	*/
	ReleaseList m_closedConnects;
	Mutex m_connectsMutex;//连接列表访问控制
	Mutex m_closedConnectsMutex;//已关闭连接列表访问控制
	int m_nHeartTime;//心跳间隔(S)
	int m_nReconnectTime;//自动重连时间(S)
	Thread m_mainThread;
	NetEventMonitor *m_pNetMonitor;
	ThreadPool m_ioThreads;//io线程池
	int m_ioThreadCount;//io线程数量
	ThreadPool m_workThreads;//业务线程池
	int m_workThreadCount;//业务线程数量
	NetServer *m_pNetServer;
	std::map<int,SOCKET> m_serverPorts;//提供服务的端口,key端口，value状态监听这个端口的套接字
	Mutex m_listenMutex;//监听操作互斥
	std::map<uint64,SOCKET> m_serIPList;//连接的外部服务地址列表
	Mutex m_serListMutex;//连接的服务地址列表互斥
protected:
	//网络事件监听线程
	virtual void* NetMonitor( void* ) = 0;
	void* NetMonitorTask( void* );
	//响应连接事件,sock为新连接的套接字
	bool OnConnect( SOCKET sock, bool isConnectServer );
	void* ConnectWorker( NetConnect *pConnect );//业务层处理连接
	//响应关闭事件，sock为关闭的套接字
	void OnClose( SOCKET sock );
	void* CloseWorker( NetConnect *pConnect );//业务层处理关闭
	//响应数据到达事件，sock为有数据到达的套接字
	connectState OnData( SOCKET sock, char *pData, unsigned short uSize );
	/*
		接收数据
		返回连接状态
		因具体响应器差别，需要派生类中实现
	*/
	virtual connectState RecvData( NetConnect *pConnect, char *pData, unsigned short uSize );
	void* MsgWorker( NetConnect *pConnect );//业务层处理消息
	connectState OnSend( SOCKET sock, unsigned short uSize );//响应发送事件
	virtual connectState SendData(NetConnect *pConnect, unsigned short uSize);//发送数据
	virtual bool MonitorConnect(NetConnect *pConnect);//监听连接
	virtual SOCKET ListenPort(int port);//监听一个端口,返回创建的套接字
	//向某组连接广播消息(业务层接口)
	void BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, int msgsize, int *filterGroupIDs, int filterCount );
	void SendMsg( int hostID, char *msg, int msgsize );//向某主机发送消息(业务层接口)
private:
	//主线程
	void* Main(void*);
	//心跳线程
	void HeartMonitor();
	//关闭一个连接，将socket从监听器中删除
	NetConnect* CloseConnect( ConnectList::iterator it );

	//////////////////////////////////////////////////////////////////////////
	//服务端口
	bool ListenAll();//监听所有注册的端口
	//////////////////////////////////////////////////////////////////////////
	//与其它服务器交互
	SOCKET ConnectOtherServer(const char* ip, int port);//连接一个服务,返回相关套接字
	bool ConnectAll();//连接所有注册的服务，已连接的会自动跳过
	void SetServerClose(NetConnect *pConnect);//设置已连接的服务为关闭状态
	const char* GetInitError();//取得启动错误信息
public:
	/**
	 * 构造函数,绑定服务器与通信策略
	 * 
	 */
	NetEngine();
	virtual ~NetEngine();

	//设置平均连接数
	void SetAverageConnectCount(int count);
	//设置心跳时间
	void SetHeartTime( int nSecond );
	//设置自动重连时间
	void SetReconnectTime( int nSecond );
	//设置网络IO线程数量
	void SetIOThreadCount(int nCount);
	//设置工作线程数
	void SetWorkThreadCount(int nCount);
	/**
	 * 开始
	 * 成功返回true，失败返回false
	 */
	bool Start();
	//停止
	void Stop();
	//等待停止
	void WaitStop();
	//关闭一个网络对象,通信层发现网络对象关闭连接时，派生类调用接口
	void CloseConnect( SOCKET sock );
	//监听一个端口
	bool Listen( int port );
	/*
		连接一个服务
		heartMsg是该服务器要求的心跳包报文
		如果对方没有要求心跳，将len设置为0，
	*/
	bool Connect(const char* ip, int port);
};

}  // namespace mdk
#endif //MDK_NET_ENGINE_H
