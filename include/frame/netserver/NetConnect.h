// NetConnect.h: interface for the NetConnect class.
//
//////////////////////////////////////////////////////////////////////
/*
	网络连接类
	通信层对象，对业务层不可见
 */
#ifndef MDK_NETCONNECT_H
#define MDK_NETCONNECT_H

#include "NetHost.h"
#include "../../../include/mdk/Lock.h"
#include "../../../include/mdk/IOBuffer.h"
#include "../../../include/mdk/Socket.h"

#include <time.h>
#include <map>
#include <string>

namespace mdk
{
class HostData;
class NetEventMonitor;
class Socket;
class NetEngine;
class MemoryPool;
class NetConnect  
{
public:

	friend class NetEngine;
	friend class NetHost;
	friend class IOCPFrame;
	friend class EpollFrame;
public:
	NetConnect(int sock, int listenSock, bool bIsServer, NetEventMonitor *pNetMonitor, NetEngine *pEngine, MemoryPool *pMemoryPool);
	virtual ~NetConnect();

public:
	/*
	* 准备Buffer
	* 为写入uLength长度的数据准备缓冲，
	* 写入完成时必须调用WriteFinished()标记可读数据长度
	*/
	unsigned char* PrepareBuffer(unsigned short uRecvSize);
	/**
	 * 写入完成
	 * 标记写入操作写入数据的长度
	 * 必须与PrepareBuffer()成对调用
	 */
	void WriteFinished(unsigned short uLength);
	/*
	 *	不提供bool WriteData( char *data, int nSize );接口
	 *	来写数据，是为了避免COPY操作，提高效率
	 *	另外也统一了IOCP与EPOLL的数据写入方式
	 */

	void SetID( int64 connectId );
	int64 GetID();//取得ID
	Socket* GetSocket();//取得套接字
	bool IsReadAble();//可读
	uint32 GetLength();//取得数据长度
	//从接收缓冲中读数据，数据不够，直接返回false，无阻塞模式
	//bClearCache为false，读出数据不会从接收缓冲删除，下次还是从相同位置读取
	bool ReadData(unsigned char* pMsg, unsigned int uLength, bool bClearCache = true );
	bool SendData( const unsigned char* pMsg, unsigned int uLength );
	bool SendStart();//开始发送流程
	void SendEnd();//结束发送流程
	void Close();//关闭连接
		
	//刷新心跳时间
	void RefreshHeart();
	//取得上次心跳时间
	time_t GetLastHeart();
	bool IsInGroups( int *groups, int count );//属于某些分组
	bool IsServer();//主机是一个服务
	void InGroup( int groupID );//放入某分组，同一个主机可多次调用该方法，放入多个分组，非线程安全
	void OutGroup( int groupID );//从某分组删除，非线程安全
	void Release();
	/*
		主机地址
		NetConnect表示的是对方，所以自身地址就是对方地址
	 */
	void GetAddress( std::string &ip, int &port );//主机地址
	/*
		服务器地址
	 */
	void GetServerAddress( std::string &ip, int &port );
	
	//设置客户数据,SetData(NULL)释放数据控制权
	void SetData( HostData *pData, bool autoFree );
	//取得客户数据
	HostData* GetData();
	//设置服务信息
	void SetSvrInfo(void *pData);
	//取服务信息
	void* GetSvrInfo();
private:
	int m_useCount;//访问计数
	IOBuffer m_recvBuffer;//接收缓冲
	int m_nReadCount;//正在进行读接收缓冲的线程数
	bool m_bReadAble;//io缓冲中有数据可读
	bool m_bConnect;//连接正常
	int m_nDoCloseWorkCount;//NetServer::OnClose执行次数

	IOBuffer m_sendBuffer;//发送缓冲
	int m_nSendCount;//正在进行发送的线程数
	bool m_bSendAble;//io缓冲中有数据需要发送
	Mutex m_sendMutex;//发送操作并发锁
	
	Socket m_socket;//socket指针，用于调用网络操作
	NetEventMonitor *m_pNetMonitor;//底层投递操作接口
	NetEngine *m_pEngine;//用于关闭连接
	int64 m_id;
	NetHost m_host;
	time_t m_tLastHeart;//最后一次收到心跳时间
	bool m_bIsServer;//主机类型服务器
	std::map<int,int> m_groups;//所属分组
	MemoryPool *m_pMemoryPool;
	HostData *m_pHostData;//主机数据
	mdk::Mutex m_mutexData;//主机数据锁
	bool m_autoFreeData;
	void *m_pSvrInfo;//服务信息，当NetConnect代表一个服务器时有效

};


}//namespace mdk

#endif // MDK_NETCONNECT_H
