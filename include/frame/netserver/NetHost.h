#ifndef MDK_NETHOST_H
#define MDK_NETHOST_H

#include "mdk/FixLengthInt.h"
#include "mdk/Lock.h"
#include <map>

namespace mdk
{
	
class NetConnect;
class Socket;
/**
	网络主机类
	表示一个连接上来的主机，私有构造函数，只能由引擎创建，用户使用
*/
class NetHost
{
	friend class NetConnect;
public:
	~NetHost();
	/*
		主机唯一标识
		※实际就是与该主机连接的SOCKET句柄，但不可直接使用socket相关api来操作该socket的io与close。
		因为close时，底层需要做清理工作，如果直接使用socketclose()，则底层可能没机会执行清理工作,造成连接不可用
		io操作底层已经使用io缓冲管理，直接使用api io将跳过io缓冲管理，且会与底层io并发，将导致数据错乱
	*/
		
	int ID();
	//取得已经到达数据长度
	uint32 GetLength();
	/*
		从接收缓冲中读数据
			参数：
				pMsg		消息保存接收的数据
				uLength		想要接收的长度
				bClearCache	是否将接收到的数据从缓存中清除，true删除，false保留
				比如报文格式：2byte内容长度+报文内容
				OnMsg()内接收逻辑如下
					1.Recv(msg, 2, true);
					2.解析msg得到内容长度，假设为256
					3.Recv(msg, 256, true)
					…
					如果3.这里返回false，表示实际到达数据<256，不够读取
					
					这时，用户有2种选择
					选择1：
					循环执行Recv 直到成功，如果不sleep，CPU直接100%，如果sleep，响应效率降低
					选择2：
					将256保存下来，直接return退出OnMsg，下次OnMsg触发时，再尝试Recv
					优点，没有sleep,不吃CPU
					缺点：用户代码难以组织，用户需要为所有连接维护一个int变量保存接收长度，
					也就是用户需要自己维护一个列表，在连接断开时，要从列表删除，工作繁杂
					
					传递false到bClearCache，解决上面的问题
					1.Recv(msg, 2, false);
					2.解析msg得到内容长度，假设为256
					3.Recv(msg, 256+2, true)//整个报文长度是256+2
						如果Recv成功，直接处理
						如果Recv失败，表示到达数据不够，因为1.哪里传递了false，
						报文长度信息不会从接收缓冲中删除，所以，用户可以直接return退出OnMsg,
						下次OnMsg触发时，还可以从连接上读到报文内容长度信息
			
			返回值：
			数据不够，直接返回false
			无需阻塞模式，引擎已经替用户处理好消息等待，消息到达时会有OnMsg被触发
	*/
	bool Recv(unsigned char* pMsg, unsigned short uLength, bool bClearCache = true );
	/**
		发送数据
		返回值：
			当连接无效时，返回false
	*/
	bool Send(const unsigned char* pMsg, unsigned short uLength);
	void Close();//关闭连接
	/**
		保持对象，使用完，必须Free()
		用户在OnConnect OnMsg OnClose时候有机会得到该主机的指针
		退出以上方法后，指针就可能被引擎释放。
		
		  但客户有可能需要保存该指针，在自己的业务使用，那么每次保存该指针，都必须Hold一次，让引擎知道有一个线程在访问该指针，每用完必须Free一次，相当于智能指针引用计数的概念
		  
			※使用案例
			案例1
			OnConnect(NetHost *pClient)
			{
			pClient->hold()
			pClient->send()
			pClient->free()
			接口说明说了，pClient在回调退出前，绝对安全，底层不会释放，hold free并不会引起错误，但不需要 
			}
			
			  案例2
			  OnConnect(NetHost *pClient)
			  {
			  连接建立，保存指针到map中，在业务处理中需要向该主机发送消息（转发、某玩家攻击该玩家等情况）时，使用
			  pClient->hold()
			  Lock map
			  Map.insert(pClient->ID(),pClient);//这里pClient被保存了1次，hold了1次
			  unlock map
			  }
			  OnCloseConnect (NetHost *pClient)
			  {
			  连接断开，指针没有用了，从map删除，并free
			  Lock map
			  pClient->free()
			  map.del(pClient->ID()) // pClient，从map删除， 1次hold了，free1次
			  unlock map
			  }
			  在以上前提下
			  错误方式1
			  OnMsg(NetHost *pClient)
			  {
			  攻击主机id 为1的玩家
			  Lock map
			  NetHost *otherHost = map.find(1)//这里第2次保存，没有hold， 还是1次
			  Unlock
			  使用otherHost，通知玩家受到攻击//如果这时玩家1正好退出游戏，并发生线程切换进入OnCloseConnect，指针被free
			  2种情况
			  1.	底层定时器检查指针状态的时间未到达，指针不会被释放，线程切换回OnMsg，发送通知，返回连接已断开，安全退出onMsg 
			  2.	底层定时器正好到达检查时间点，发现指针的访问计数归0，将删除指针。otherHost变为野指针，线程切换回OnMsg，发送受到攻击通知时，访问野指针，系统崩溃
			  }
			  错误方式2
			  OnMsg(NetHost *pClient)
			  {
			  攻击主机id 为1的玩家
			  Lock map
			  NetHost *otherHost = map.find(1)//这里第2次保存，没有hold， 还是1次
			  Unlock
			  otherHost->hold()错误，情况同上，可能在hold时，已经是野指针
			  使用otherHost，通知玩家受到攻击
			  otherHost->free()
			  
				}
				
				  正确方式1
				  OnMsg(NetHost *pClient)
				  {
				  攻击主机id 为1的玩家
				  Lock map
				  NetHost *otherHost = map.find(1)//这里第2次保存，没有hold， 还是1次
				  使用otherHost，通知玩家受到攻击，即使线程切换到free线程，因为lock，free线程会被挂起，等待这里unlock，底层不会释放，安全
				  Unlock
				  退出OnMsg
				  }
				  
					正确方式2
					OnMsg(NetHost *pClient)
					{
					攻击主机id 为1的玩家
					Lock map
					NetHost *otherHost = map.find(1)//这里第2次保存，没有hold， 还是1次
					otherHost->hold()，同上在unlock之前，free线程不可能执行，底层不可能释放，
					Unlock
					使用otherHost，通知玩家受到攻击，已经hold2次，即使线程切换到free线程，访问计数还是大于1，底层不会释放，安全
					otherHost->free()//使用完毕，释放访问，退出OnMsg
					
					  }
		*/
	void Hold();
	/**
		释放对象
		在完全不访问该指针后，Free()次数必须与Hold()相同，否则底层永远不释放该指针。
	*/
	void Free();
	/*
		主机类型是一个服务
		NetServer调用Connect连接到一个服务器时，产生的NetHost是指向一个服务主	
	*/
	bool IsServer();
	
	//////////////////////////////////////////////////////////////////////////
	//业务接口，与NetEngine::BroadcastMsg偶合
	void InGroup( int groupID );//放入某分组，同一个主机可多次调用该方法，放入多个分组
	void OutGroup( int groupID );//从某分组删除
	
private://私有构造，仅仅只在NetConnect内部被创建，客户只管使用
	NetHost( bool bIsServer );

private:
	NetConnect* m_pConnect;//连接对象指针,调用NetConnect的业务层接口，屏蔽NetConnect的通信层接口
	bool m_bIsServer;//主机类型服务器
	std::map<int,int> m_groups;//所属分组
	Mutex m_groupMutex;//m_groups线程安全
	
};

}  // namespace mdk
#endif//MDK_NETHOST_H
