#include "../../../include/frame/netserver/HostData.h"
#include "../../../include/mdk/atom.h"
#include "../../../include/mdk/mapi.h"

namespace mdk
{

HostData::HostData()
{
	m_autoFree = false;
	m_pHost = &m_hostRef;//指向空主机
	m_refCount = 0;
}

HostData::~HostData()
{
	if ( m_autoFree )
	{
		m_autoFree = false;
		m_pHost = &m_hostRef;
		return;
	}
	NetHost empty;
	m_hostRef = empty;//释放host拷贝
}

void HostData::Release()
{
	/*
		NetConnect::GetData()并发逻辑分析
		m_refCount > 0这里不会释放内存，多少GetData()并发都没关系

		m_refCount = 0时发生GetData()并发，一定是与SetData()发生并发，有可能是SetData(NULL)也有可能是SetData(新数据)
		因为SetData(NULL)之后，GetData就不可能通过NULL == m_pHostData检查

		if oldCount = AtomDec(&m_refCount, 1); 先完成
			oldCount = 0，下去执行真正的内存释放
			m_refCount = -1;
			GetData()中的if ( -1 == AtomAdd(&m_pHostData->m_refCount, 1) )后执行，会返回真，return NULL
			不会返回野指针
		else GetData()中的if ( -1 == AtomAdd(&m_pHostData->m_refCount, 1) )先完成
			m_refCount = 1;
			GetData()会返回内存地址
			然后这里的oldCount = AtomDec(&m_refCount, 1);执行
			m_refCount = 0;
			oldCount = 1 不会释放内存，直接return了
			GetData()返回的地址不是野指针，但是数据已经和Host没有关联了
	*/
	int oldCount = AtomDec(&m_refCount, 1);//计数-1，返回减之前的值，
	if ( 0 > oldCount ) mdk_assert(false);//重复释放
	if ( 0 != oldCount ) return;
	/*
		※1Release()没有Lock,会不会在释放之前又有发生GetData()调用，返回野指针呢？
		不可能，因为GetData()的前提是SetData()，而SetData()会将引用计数增加，
		所以在用户执行SetData(NULL)释放数据之前，引用计数是不可能归0的。
		根本不会进入到这里
		
		既然进入了这里说明，外部已经没有HostData的任何引用存在，所以GetData()只会返回NULL

		※2Release()没有Lock,会不会在释放之前又有发生SetData()调用，将野指针绑定给一个NetHost呢？
		不可能，因为要将HostData传递给SetData首先需要存在一个引用，也就是说引用计数不会为0
		也就不可能进入这里了
	*/
	//oldCount此时应该=-1，即对象已经被释放
	//之后不应该再有Release()调用，如果又有release调用就是重复释放，应该导致mdk_assert断言
	ReleaseInterface();
}

void HostData::ReleaseInterface()
{
	delete this;
}

NetHost HostData::GetHost()
{
	AutoLock lock(&m_lockHostRef);
	return *m_pHost;//要么指向有效主机，要么指向m_hostRef可能有效，可能无效，m_pHost不会是NULL
}

void HostData::SetHost( NetHost *pHost )
{
	AutoLock lock(&m_lockHostRef);
	m_hostRef = *pHost;
}

}