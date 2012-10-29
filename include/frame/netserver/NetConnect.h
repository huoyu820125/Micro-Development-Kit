// NetConnect.h: interface for the NetConnect class.
//
//////////////////////////////////////////////////////////////////////
/*
	网络连接类
	通信层对象，对业务层不可见
 */
#ifndef MDK_NETCONNECT_H
#define MDK_NETCONNECT_H

#include <time.h>
#include "NetHost.h"
#include "mdk/Lock.h"
#include "mdk/IOBuffer.h"
#include "mdk/Socket.h"

namespace mdk
{
class NetEventMonitor;
class Socket;
class NetEngine;
class NetConnect  
{
	friend class NetEngine;
	friend class IOCPFrame;
	friend class EpollFrame;
public:
	NetConnect(SOCKET sock, bool bIsConnectServer, NetEventMonitor *pNetMonitor, NetEngine *pEngine);
	virtual ~NetConnect();

public:
	/**
	 * 通信层接口
	 * 取得拥有者
	 * 释放线程访问确定可否释放
	 */
	bool IsFree();
	/**
	  业务层开始访问
	  m_uWorkAccessCount++
	 */
	void WorkAccess();
	/**
	  业务层完成访问
	  m_uWorkAccessCount--
	 */
	void WorkFinished();
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

	int GetID();//取得ID
	Socket* GetSocket();//取得套接字
	bool IsReadAble();//可读
	uint32 GetLength();//取得数据长度
	//从接收缓冲中读数据，数据不够，直接返回false，无阻塞模式
	//bClearCache为false，读出数据不会从接收缓冲删除，下次还是从相同位置读取
	bool ReadData(unsigned char* pMsg, unsigned short uLength, bool bClearCache = true );
	bool SendData( const unsigned char* pMsg, unsigned short uLength );
	bool SendStart();//开始发送流程
	void SendEnd();//结束发送流程
	void Close();//关闭连接
		
	//刷新心跳时间
	void RefreshHeart();
	//取得上次心跳时间
	time_t GetLastHeart();
	bool IsInGroups( int *groups, int count );//属于某些分组
	
private:
	/**
	 * 该对象当前拥有者
	 * enum Owner类型
	 * 0无拥有者可以释放
	 * 1通信层
	 * 2业务层
	 * 
	 * 对象从被创建开始，就应该设置为1
	 * 对象被业务层保存到内存时候，应该设置为2
	 * 对象被业务层从内存删除时，应该设置为1
	 * 
	 * 无访问且拥有者为网络层时，才能设置为0
	 * （即每次退出业务操作和关闭网络连接时，应该检查访问计数与拥有者，决定是否应该将拥有者设置为0）
	 * 关闭网络连接时候，如果2种访问计数都为0，而拥有者却是业务层，则一定是业务层逻辑忘记释放拥有属性，必须检查业务流程，清除
	 * 因为网络访问计数为0了，则一定不会再引发业务访问，而业务访问为0了，最后一个业务操作应该是断开连接业务，业务层必须释放拥有属性
	 * 
	 * 不必加访问锁
	 * 1.保证OnCloseConnect()一定在OnConnect()完成后才可能引发。
	 * 如此就保证了，不会在出现业务层释放占有权限后，又获取占有权限，这点客户代码不用关心，都是网络层的事情
	 * 
	 * 2.从业务上保证业务层释放对象后，就不会再占有对象
	 * 如此则保证了，访问计数为0时，可以安全的释放对象
	 */
	int m_owner;
	unsigned int m_uWorkAccessCount;//业务层访问计数
	Mutex m_lockWorkAccessCount;//业务层访问计数锁
	IOBuffer m_recvBuffer;//接收缓冲
	int m_nReadCount;//正在进行读接收缓冲的线程数
	bool m_bReadAble;//io缓冲中有数据可读
	bool m_bConnect;//连接正常

	IOBuffer m_sendBuffer;//发送缓冲
	int m_nSendCount;//正在进行发送的线程数
	bool m_bSendAble;//io缓冲中有数据需要发送
	Mutex m_sendMutex;//发送操作并发锁
	
	Socket m_socket;//socket指针，用于调用网络操作
	NetEventMonitor *m_pNetMonitor;//底层投递操作接口
	NetEngine *m_pEngine;//用于关闭连接
	int m_id;
	NetHost m_host;
	time_t m_tLastHeart;//最后一次收到心跳时间

};

}//namespace mdk

#endif // MDK_NETCONNECT_H
