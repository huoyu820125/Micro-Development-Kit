#ifndef MDK_NETHOST_H
#define MDK_NETHOST_H

#include "../../../include/mdk/FixLengthInt.h"
#include <string>

namespace mdk
{
class STNetConnect;
class Socket;
/**
	网络主机类
	表示一个连接上来的主机，私有构造函数，只能由引擎创建，用户使用

	该类是一个共享对象类，类似智能指针，所有通过复制产生的STNetHost对象共享一个STNetConnect指针。
	所以不要复制地址与引用，因为引用引用计数不会改变，造成可能访问一个已经被释放的STNetConnect指针
	错误范例：
	STNetHost *pHost = &host;//复制地址，引用计数不会改变
	vector<STNetHost*> hostList;
	hostList.push_back(&host);//复制地址，引用计数不会改变
	正确范例：
	STNetHost safeHost = host;//复制对象，引用计数会增加1，在safeHost析构之前，STNetConnect指针指向的内存绝对不会被释放
	vector<STNetHost> hostList;
	hostList.push_back(host);//复制对象，引用计数会增加1，在对象从hostList中删除之前，STNetConnect指针指向的内存绝对不会被释放
*/
class STNetHost
{
	friend class STNetConnect;
public:
	STNetHost();
	virtual ~STNetHost();
	STNetHost(const STNetHost& obj);	
	STNetHost& operator=(const STNetHost& obj);
	/*
		主机唯一标识
		※实际就是与该主机连接的int句柄，但不可直接使用socket相关api来操作该socket的io与close。
		因为close时，底层需要做清理工作，如果直接使用socketclose()，则底层可能没机会执行清理工作,造成连接不可用
		io操作底层已经使用io缓冲管理，直接使用api io将跳过io缓冲管理，且会与底层io并发，将导致数据错乱
	*/
		
	int ID();
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
	bool Recv(unsigned char* pMsg, unsigned int uLength, bool bClearCache = true );
	/**
		发送数据
		返回值：
			当连接无效时，返回false
	*/
	bool Send(const unsigned char* pMsg, unsigned int uLength);
	void Close();//关闭连接
	bool IsServer();//主机是一个服务
	void InGroup( int groupID );//放入某分组，同一个主机可多次调用该方法，放入多个分组
	void OutGroup( int groupID );//从某分组删除
	/*
		主机地址
		如果你在NetServer希望得到对方地址，应该调用本方法，而不是GetServerAddress
		因为STNetHost表示的就是对方主机，所以STNetHost的主机地址就是对方地址
	 */
	void GetAddress( std::string &ip, int &port );//主机地址
	/*
		服务器地址
		如果你希望知道对方主机连接到自己的哪个端口，应该调用本方法，而不是GetAddress]
		因为GetAddress表示的是对方
	 */
	void GetServerAddress( std::string &ip, int &port );
	/*
		取服务信息
		就是调用NetServer::Connect()时传入的第3个参数
	*/
	void* GetSvrInfo();
private:
	STNetConnect* m_pConnect;//连接对象指针,调用STNetConnect的业务层接口，屏蔽STNetConnect的通信层接口
	
};

}  // namespace mdk
#endif//MDK_NETHOST_H
