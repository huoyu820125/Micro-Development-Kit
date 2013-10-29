#ifndef HOST_DATA_H
#define HOST_DATA_H
#include "../../../include/mdk/Lock.h"
#include "../../../include/frame/netserver/NetHost.h"
#include "../../../include/mdk/Lock.h"

namespace mdk
{

class NetHost;
/*
	主机数据
	配给NetHost的数据基类
	表示连接相关的业务数据，由用户在派生类中定义具体业务数据。
	业务数据建议定义成public，不必遵循oop那套类成员必须通过方法访问的规则
	因为本来目的就是替代关联到NetHost上的数据结构，数据结构都是直接访问数据的，
	所以应该把该类型当作数据结构看待。
*/
class HostData
{
friend class NetConnect;
public:
	HostData();
	virtual ~HostData();
	/*
		安全的释放对象
		会检查引用计数
		然后调用ReleaseInterface进行释放
	*/
	void Release();
	/*
		返回NetHost对象，而不返回指针，
		符合NetHost的copy限制规则
	*/
	NetHost GetHost();

protected:
	/*
		由于HostData对象的创建方式，可能是new、malloc()、内存池、对象池，回收方式各步相同
		所以需要用户实现释放方法，告诉框架如何释放HostData对象
		
		※如果HostData对象是从new产生，则可以不用重写release()，默认使用delete方式回收对象HostData
	*/
	virtual void ReleaseInterface();
	void SetHost(NetHost *pHost);//线程安全的修改关联的host

private:
	bool m_autoFree;//true代理模式，让框架自动释放HostData对象，false自主模式，用户管理生命周期
	NetHost m_hostRef;//host的一份复制，在代理模式时，确保host是有效的
	mdk::Mutex m_lockHostRef;//hostbian变化锁
	NetHost *m_pHost;
	/*
		引用计数
		当对象由引擎管理时，此成员被忽略
		当用户自己管理对象声明周期时，可以确保release在正确的时候释放对象
	*/
	int m_refCount;
};

}
#endif //HOST_DATA_H