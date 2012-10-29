#ifndef MDK_NETHOST_H
#define MDK_NETHOST_H

#include "mdk/FixLengthInt.h"
#include <map>

namespace mdk
{
	
class NetConnect;
class Socket;
/**
	网络主机类
	表示网络中的一个连接过来的主机
	业务层通信接口对象
	提供发送接收关闭操作
	提供访问安全操作Hold与Free

 	※hostID实际就是与该主机连接的SOCKET句柄，但不可直接使用socket相关api来操作该socket的io与close。
 	因为close时，底层需要做清理工作，如果直接使用socketclose()，则底层可能没机会执行清理工作,造成连接不可用
 	io操作底层已经使用io缓冲管理，直接使用api io将跳过io缓冲管理，且会与底层io并发，将导致数据错乱
*/
class NetHost
{
	friend class NetConnect;
private:
	NetConnect* m_pConnect;//连接对象指针,调用NetConnect的业务层接口，屏蔽NetConnect的通信层接口
	bool m_bIsServer;//主机类型服务器
	std::map<int,int> m_groups;//所属分组
private://私有构造，仅仅只在NetConnect内部被创建，客户只管使用
	NetHost( bool bIsServer );

public:
	~NetHost();
	int ID();//连接ID
	//取得已经到达数据长度
	uint32 GetLength();
	//从接收缓冲中读数据，数据不够，直接返回false，无阻塞模式
	//bClearCache为false，读出数据不会从接收缓冲删除，下次还是从相同位置读取
	bool Recv(unsigned char* pMsg, unsigned short uLength, bool bClearCache = true );
	/**
	 * 发送数据
	 * 当连接无效时，返回false
	 */
	bool Send(const unsigned char* pMsg, unsigned short uLength);
	void Close();//关闭连接
	/**
	 * 保持对象
	 * 必须与Free()成对调用
	 * 客户有可能需要保存网络主机对象，然后在需要的是使用
	 * 需要保持对象，避免框架删除该主机对象
	 */
	void Hold();
	/**
	 * 释放对象
	 * 必须与Hold()成对调用
	 * 让框架得到可以对该主机对象进行回收控制
	 */
	void Free();
	//主机是一个服务
	bool IsServer();
	//////////////////////////////////////////////////////////////////////////
	//业务接口，与NetEngine::BroadcastMsg偶合
	void InGroup( int groupID );//放入某分组，同一个主机可多次调用该方法，放入多个分组，非线程安全
	void OutGroup( int groupID );//从某分组删除，非线程安全
private:
};

}  // namespace mdk
#endif//MDK_NETHOST_H
