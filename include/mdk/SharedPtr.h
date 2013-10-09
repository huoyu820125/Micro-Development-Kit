// SharedPtr.h: interface for the SharedPtr class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_SHAREDPTR_H
#define MDK_SHAREDPTR_H
#include "atom.h"

namespace mdk
{

template<class Obj>
class SharedPtr
{
public:
	SharedPtr()
	{
		m_pUseCount = NULL;
		m_pObject = NULL;
		m_released = true;
	}

	SharedPtr(Obj* pObject)
	{
		m_pUseCount = new int(1);
		m_pObject = pObject;
		m_released = false;
	}

	SharedPtr(const SharedPtr& ap)
	{
		AtomAdd(ap.m_pUseCount, 1);
		m_pObject = ap.m_pObject;
		m_pUseCount = ap.m_pUseCount;
		m_released = false;
	}

	SharedPtr& operator=(const SharedPtr& ap)
	{
		if ( *this == ap ) return *this;
		Release();
		AtomAdd(ap.m_pUseCount, 1);
		m_pObject = ap.m_pObject;
		m_pUseCount = ap.m_pUseCount;
		m_released = false;
		return *this;
	}
		
	SharedPtr& operator=(void *pObject)
	{
		if ( *this == pObject ) return *this;
		Release();
		if ( NULL == pObject ) return *this;
		m_pUseCount = new int(1);
		m_pObject = (Obj*)pObject;
		m_released = false;
		return *this;
	}
	
	virtual ~SharedPtr()
	{
		Release();
	}

public :
	void Release()
	{
		if ( m_released ) return;
		m_released = true;
		if ( 1 == AtomDec(m_pUseCount, 1) ) 
		{
			delete m_pObject;
			delete m_pUseCount;
		}
		m_pObject = NULL;
		return;
	}

	Obj* operator->()
	{
		return m_pObject;
	}

	const Obj* operator->() const
	{
		return m_pObject;
	}
	
	Obj &operator*() 
	{
		if ( NULL == m_pObject )//访问null指针，主动触发崩溃
		{
			char *p = NULL;
			*p = 1;
			exit(0);
		}
		return *m_pObject;
	}

	const Obj &operator*() const 
	{
		if ( NULL == m_pObject )//访问null指针，主动触发崩溃
		{
			char *p = NULL;
			*p = 1;
			exit(0);
		}
		return *m_pObject;
	}
	
	bool operator==(const SharedPtr &poniter) const
	{
		if ( NULL == m_pObject || NULL == poniter.m_pObject ) return false;
		return m_pObject == poniter.m_pObject?true:false;
	}
	
	bool operator!=(const SharedPtr &poniter) const
	{
		if ( NULL == m_pObject || NULL == poniter.m_pObject ) return true;
		return m_pObject != poniter.m_pObject?true:false;
	}
	
	bool operator==(const void *poniter) const
	{
		if ( NULL == m_pObject || NULL == poniter ) return false;
		return m_pObject == poniter?true:false;
	}
	
	bool operator!=(const void *poniter) const
	{
		if ( NULL == m_pObject || NULL == poniter ) return true;
		return m_pObject != poniter?true:false;
	}
	
private:
	int *m_pUseCount;
	Obj *m_pObject;
	bool m_released;
};

}//end namespace mdk

#endif // !defined MDK_SHAREDPTR_H