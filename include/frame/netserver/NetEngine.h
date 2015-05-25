#ifndef MDK_NET_ENGINE_H
#define MDK_NET_ENGINE_H

#include "../../../include/mdk/Socket.h"
#include "../../../include/mdk/Thread.h"
#include "../../../include/mdk/Lock.h"
#include "../../../include/mdk/ThreadPool.h"
#include "../../../include/mdk/FixLengthInt.h"
#include "../../../include/mdk/MemoryPool.h"
#include "../../../include/mdk/Signal.h"

#include <map>
#include <vector>
#include <string>
#ifndef WIN32
#include <sys/epoll.h>
#endif

namespace mdk
{
class Mutex;
class NetConnect;
class NetHost;
class NetEventMonitor;
class NetServer;
class MemoryPool;
typedef std::map<int64,NetConnect*> ConnectList;
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
	Signal m_sigStop;//停止信号
	/**
		连接表
		map<unsigned long,NetConnect*>
		定时检查该列表中连接是有发送心跳，
		将没有心跳的连接断开
	*/
	ConnectList m_connectList;
	Mutex m_connectsMutex;//连接列表访问控制
	int m_nHeartTime;//心跳间隔(S)
	Thread m_mainThread;
	NetEventMonitor *m_pNetMonitor;
	ThreadPool m_ioThreads;//io线程池
	int m_ioThreadCount;//io线程数量
	ThreadPool m_workThreads;//业务线程池
	int m_workThreadCount;//业务线程数量
	NetServer *m_pNetServer;
	std::map<int,int> m_serverPorts;//提供服务的端口,key端口，value状态监听这个端口的套接字
	Mutex m_listenMutex;//监听操作互斥

	typedef struct SVR_CONNECT
	{
		enum ConnectState
		{
			unconnected = 0,
				connectting = 1,
				unconnectting = 2,
				connected = 3,
		};
		int sock;				//句柄
		uint64 addr;				//地址
		int reConnectSecond;		//重链时间，小于0表示不重链
		time_t lastConnect;			//上次尝试链接时间
		ConnectState state;			//链接状态
		void *pSvrInfo;				//服务信息
#ifndef WIN32
		bool inEpoll;				//在epoll中
#endif
	}SVR_CONNECT;
	std::map<uint64,std::vector<SVR_CONNECT*> > m_keepIPList;//要保持连接的外部服务地址列表，断开会重连
	Mutex m_serListMutex;//连接的服务地址列表互斥
	Thread m_connectThread;
	Signal m_wakeConnectThread;
protected:
	//网络事件监听线程
	virtual void* NetMonitor( void* ) = 0;
	void* RemoteCall NetMonitorTask( void* );
	//响应连接事件,sock为新连接的套接字
	bool OnConnect( int sock, SVR_CONNECT *pSvr = NULL );
	void* RemoteCall ConnectWorker( NetConnect *pConnect );//业务层处理连接
	//响应关闭事件，sock为关闭的套接字
	void OnClose( int64 connectId );
	void NotifyOnClose(NetConnect *pConnect);//发出OnClose通知
	void* RemoteCall CloseWorker( NetConnect *pConnect );//业务层处理关闭
	void* RemoteCall ConnectFailed( NetEngine::SVR_CONNECT *pSvr );//业务层处理主动向外链接失败
	//响应数据到达事件，sock为有数据到达的套接字
	connectState OnData( int64 connectId, char *pData, unsigned short uSize );
	/*
		接收数据
		返回连接状态
		因具体响应器差别，需要派生类中实现
	*/
	virtual connectState RecvData( NetConnect *pConnect, char *pData, unsigned short uSize );
	void* RemoteCall MsgWorker( NetConnect *pConnect );//业务层处理消息
	connectState OnSend( int64 connectId, unsigned short uSize );//响应发送事件
	virtual connectState SendData(NetConnect *pConnect, unsigned short uSize);//发送数据
	virtual int ListenPort(int port);//监听一个端口,返回创建的套接字
	//向某组连接广播消息(业务层接口)
	void BroadcastMsg( int *recvGroupIDs, int recvCount, char *msg, unsigned int msgsize, int *filterGroupIDs, int filterCount );
	bool SendMsg( int64 hostID, char *msg, unsigned int msgsize );//向某主机发送消息(业务层接口)
private:
	//主线程
	void* RemoteCall Main(void*);
	//心跳线程
	void HeartMonitor();
	//关闭一个连接，将socket从监听器中删除
	void CloseConnect( ConnectList::iterator it );

	//////////////////////////////////////////////////////////////////////////
	//服务端口
	bool ListenAll();//监听所有注册的端口
	//////////////////////////////////////////////////////////////////////////
	//与其它服务器交互
	enum ConnectResult
	{
		success = 0,
		waitReulst = 1,
		cannotCreateSocket = 2,
		invalidParam = 3,
		faild = 4,
	};
	NetEngine::ConnectResult ConnectOtherServer(const char* ip, int port, int &svrSock);//异步连接一个服务,立刻成功返回true，否则返回false，等待select结果
	bool ConnectAll();//连接所有注册的服务，已连接的会自动跳过
	void SetServerClose(NetConnect *pConnect);//设置已连接的服务为关闭状态
	const char* GetInitError();//取得启动错误信息
	void* RemoteCall ConnectThread(void*);//异步链接线程
	NetEngine::ConnectResult AsycConnect( int svrSock, const char *lpszHostAddress, unsigned short nHostPort );
	bool EpollConnect( SVR_CONNECT **clientList, int clientCount );//clientCount个连接全部收到结果，返回true,否则返回false
	bool SelectConnect( SVR_CONNECT **clientList, int clientCount );//clientCount个连接全部收到结果，返回true,否则返回false
	bool ConnectIsFinished( SVR_CONNECT *pSvr, bool readable, bool sendable, int api, int errorCode );//链接已完成返回true（不表示成功，失败也是完成，外部不需要处理，成功失败在内部已处理），否则返回false
	
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
	//设置网络IO线程数量
	void SetIOThreadCount(int nCount);
	//设置工作线程数
	void SetWorkThreadCount(int nCount);
	//设置工作线程启动回调函数
	void SetOnWorkStart( MethodPointer method, void *pObj, void *pParam );
	void SetOnWorkStart( FuntionPointer fun, void *pParam );
	//打开TCP_NODELAY
	void OpenNoDelay();
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
	void CloseConnect( int64 connectId );
	//监听一个端口
	bool Listen( int port );
	/*
	连接一个服务
	reConnectTime < 0表示断开后不重新自动链接
	*/
	bool Connect(const char* ip, int port, void *pSvrInfo, int reConnectTime);
#ifndef WIN32
	int m_hEPoll;
	epoll_event *m_events;
#endif
	int64 m_nextConnectId;
	bool m_noDelay;//开启TCP_NODELAY
};

}  // namespace mdk
#endif //MDK_NET_ENGINE_H
