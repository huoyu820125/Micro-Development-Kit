// STNetConnect.h: interface for the STNetConnect class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_STNETCONNECT_H
#define MDK_STNETCONNECT_H

#include "STNetHost.h"
#include "../../../include/mdk/IOBuffer.h"
#include "../../../include/mdk/Socket.h"

#include <time.h>
#include <map>
#include <string>

namespace mdk
{

class NetEventMonitor;
class Socket;
class STNetEngine;
class MemoryPool;	

class STNetConnect  
{
	friend class STNetEngine;
	friend class STNetHost;
public:
	STNetConnect(int sock, int listenSock, bool bIsServer, NetEventMonitor *pNetMonitor, STNetEngine *pEngine, MemoryPool *pMemoryPool);
	virtual ~STNetConnect();

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
	//设置服务信息
	void SetSvrInfo(void *pData);
	//取服务信息
	void* GetSvrInfo();
	bool AddEpollSend();
	bool AddEpollRecv();
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
	
	Socket m_socket;//socket指针，用于调用网络操作
	NetEventMonitor *m_pNetMonitor;//底层投递操作接口
	STNetEngine *m_pEngine;//用于关闭连接
	int m_id;
	STNetHost m_host;
	time_t m_tLastHeart;//最后一次收到心跳时间
	bool m_bIsServer;//主机类型服务器
	std::map<int,int> m_groups;//所属分组
	MemoryPool *m_pMemoryPool;
	void *m_pSvrInfo;//服务信息，当NetConnect代表一个服务器时有效
	bool m_monitorSend;//监听发送
	bool m_monitorRecv;//监听接收
};

}
#endif // !defined MDK_STNETCONNECT_H
