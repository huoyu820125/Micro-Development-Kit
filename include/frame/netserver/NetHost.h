#ifndef MDK_NETHOST_H
#define MDK_NETHOST_H

#include "../../../include/mdk/FixLengthInt.h"
#include <string>

namespace mdk
{
class HostData;
class NetConnect;
class Socket;
/**
	网络主机类
	表示一个连接上来的主机，私有构造函数，只能由引擎创建，用户使用

	※NetHost对象复制规范
	该类是一个共享对象类，类似智能指针，所有通过复制产生的NetHost对象共享一个NetConnect指针。
	所以不要复制地址与引用，因为引用引用计数不会改变，造成可能访问一个已经被释放的NetConnect指针
	错误范例：
	NetHost *pHost = &host;//复制地址，引用计数不会改变
	vector<NetHost*> hostList;
	hostList.push_back(&host);//复制地址，引用计数不会改变
	正确范例：
	NetHost safeHost = host;//复制对象，引用计数会增加1，在safeHost析构之前，NetConnect指针指向的内存绝对不会被释放
	vector<NetHost> hostList;
	hostList.push_back(host);//复制对象，引用计数会增加1，在对象从hostList中删除之前，NetConnect指针指向的内存绝对不会被释放
*/
class NetHost
{
	friend class NetConnect;
public:
	NetHost();
	virtual ~NetHost();
	NetHost(const NetHost& obj);	
	NetHost& operator=(const NetHost& obj);
	/*
		主机唯一标识
		※V1.79以前，实际就是与该主机连接的int句柄，但不可直接使用socket相关api来操作该socket的io与close。
		因为close时，底层需要做清理工作，如果直接使用socketclose()，则底层可能没机会执行清理工作,造成连接不可用
		io操作底层已经使用io缓冲管理，直接使用api io将跳过io缓冲管理，且会与底层io并发，将导致数据错乱

		※V1.79开始，变成一个自增的int64值与int句柄无关
			id范围??兆，链接??多兆亿之后，id将转一圈回来，需要时间大约几十年
			所以当某个链接持续几十年在线的极端情况下，才可能导致2个链接ID相同
	*/	
	int64 ID();

	/*
		设置客户数据
		※多个线程同时SetData()，不是线程安全，结果未可知
		※其它任何方式并发，都是线程安全的

		autoFree = true 代理模式
			HostData的生命周期由框架管理，用户不需要显式调用HostData.Release()，也不需要调用SetData(NULL)
			使用说明：
				1.不能将HostData指针赋给其它线程访问
				2.存在copy限制，HostData只能在其来复制对象作用域之内使用,否则是不安全的
				3.调用SetData()与主机关联后，HostData的释放工作已经移交给框架了，不要再执行HostData的释放操作
				4.一旦调用SetData()与主机关联，之后的任何SetData()都将被放弃，包括SetData(NULL)。
					因为该模式下，所有的copy都是不操作引用计数的，一旦解除关联，
					用户无法知道什么时候HostData才是可以安全删除的

			注解：
			框架会在NetConnect被释放时，帮用户释放数据

			※使用条款2说明
			这种模式下，存在copy限制，HostData只能在其来复制对象作用域之内使用,否则是不安全的
			比如HostData *pData = host.GetData();
			pData是从host这个变量中得到的，那么pData只能在host被释放前使用，
			否则离开host变量作用域，NetConnect引用计数有可能为0，底层有可能会释放NetConnect，
			导致pData指向野指针
			比如以下代码将可能访问野指针
			OnMessage( NetHost &host )
			{
				HostData *pData;
				if (...)//将消息转发给其它host
				{
					NetHost otherHost = hostmap.find( 其它hostid );//在map中找到其它host
					//危险操作，将HostData复制给一个作用域更大的指针，pData的作用域>otherHost的作用域
					pData = otherHost.GetData();
					使用pData
					离开otherHost作用域
				}
				pData可能变成野指针
				因为otherHost已经消失，otherHost对应的连接有可能被另外一个OnClose()线城所释放，
				连带pData一起被释放
			}

		autoFree = false 自主模式
			HostData的生命周期由用户自行管理，框架不管，需要用户显式调用HostData.Release()
			使用说明：
				※不建议对同一个host多次SetData关联不同数据，结果将未可知
				1.每次GetData()之后，用完了记得Release()
				2.为每个需要访问HostData的线程，调用1次GetData()获各自专属的HostData指针，
					所有线程访问完毕时，调用Release()释放访问，最后个访问的线程将真正的释放数据
				3.连接不用了后，调用SetData(NULL)解除数据于主机的关联，否则框架无法释放主机对应的连接

			注解：
				GetData()就是增加1个引用，对象要真正释放，需要GetData()调用次数+1的release()
				比如：
					HostData *pData = new HostData();
					host.SetData(pData);
					另外一个地方
					HostData *pData = host.GetData();
					实际上就存在了2个引用HostData的地方
					GetData()1个，new出来本身1个，所以需要Release()2次
				-------------------------------------------------------------------------------------------
				由于HostData内部创建了NetHost的一份复制
				在使用SetData(NULL)解除数据于NetHost的关联,或HostData被真正释放之前，
				NetConnect永远不会被框架释放.
				所以连接无效以后，需要SetData(NULL)或HostData.Release()进行释放
				SetData(NULL)会导致NetConnect被框架释放
				HostData.Release()只有在最后一次HostData.Release()的时，才会释放NetConnect

				由于GetData()保证了HostData的访问安全，所以
				用户可以在任何地方使用HostData指针，只要这个指针是出至GetData()
	*/
	void SetData( HostData *pData, bool autoFree = true );
	//取得客户数据，数据作用域，参考SetData()的2种模式说明
	HostData* GetData();
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
		因为NetHost表示的就是对方主机，所以NetHost的主机地址就是对方地址
		if 连接是使用127.0.0.1建立，则取到地址127.0.0.1
		if 连接是使用内网地址建立，则取到内网地址
		if 连接是使用外网地址建立，则取到外网地址
	 */
	void GetAddress( std::string &ip, int &port );//主机地址
	/*
		服务器地址
		如果你希望知道对方主机连接到自己的哪个端口，应该调用本方法，而不是GetAddress]
		因为GetAddress表示的是对方
		if 连接是使用127.0.0.1建立，则取到地址127.0.0.1
		if 连接是使用内网地址建立，则取到内网地址
		if 连接是使用外网地址建立，则取到外网地址
	 */
	void GetServerAddress( std::string &ip, int &port );
	/*
		取服务信息
		就是调用NetServer::Connect()时传入的第3个参数
	*/
	void* GetSvrInfo();
private:
	NetConnect* m_pConnect;//连接对象指针,调用NetConnect的业务层接口，屏蔽NetConnect的通信层接口
	
};

}  // namespace mdk
#endif//MDK_NETHOST_H
