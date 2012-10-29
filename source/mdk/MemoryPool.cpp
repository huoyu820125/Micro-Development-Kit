#include "mdk/MemoryPool.h"
#include "mdk/atom.h"
#include "mdk/Lock.h"
#include <new>

namespace mdk
{
	
MemoryPool::MemoryPool()
{
	m_resizeCtrl = new Mutex;
}

MemoryPool::MemoryPool( unsigned short uMemorySize, unsigned short uMemoryCount )
{
	m_resizeCtrl = new Mutex;
	if ( !Init( uMemorySize, uMemoryCount ) ) throw;
}
 
MemoryPool::~MemoryPool()
{
	if ( NULL != m_pNext ) delete m_pNext;
	delete[]m_pMemery;
	m_pMemery = NULL;
	m_uFreeCount = 0;
	m_pNext = NULL;
	if ( NULL != m_resizeCtrl )
	{
		Mutex* pMutex = (Mutex*)m_resizeCtrl;
		m_resizeCtrl = NULL;
		delete pMutex;
	}
}

//初始化内存池
bool MemoryPool::Init(unsigned short uMemorySize, unsigned short uMemoryCount)
{
	if ( 0 >= uMemoryCount || 0 >= uMemorySize ) return false;//内存数与内存大小必须大0
	unsigned char uAddrSize = sizeof(this);
	m_uMemoryCount = uMemoryCount;
	m_uMemorySize = uMemorySize;
	//预分配内存=MemoryPool对象自身地址，8byte，支持64位机寻址
	//			+uMemoryCount个内存块长度
	//			(内存块长度=4byte标记是否分配+2byte留空+2byte序号
	//						+实际分配给外部使用的内存的长度)
	m_pMemery = new unsigned char[8+(MEMERY_INFO + uMemorySize) * m_uMemoryCount];
	if ( NULL == m_pMemery ) return false;
	unsigned long nPos = 0;
	uint64 uThis = (uint64)this;
	//记录对象地址
	//头8个字节保存对象自身地址，用于从内存地址找到内存池对象地址
	if ( 8 == uAddrSize )
	{
		m_pMemery[nPos++] = (unsigned char)(uThis >> 56);
		m_pMemery[nPos++] = (unsigned char)(uThis >> 48);
		m_pMemery[nPos++] = (unsigned char)(uThis >> 40);
		m_pMemery[nPos++] = (unsigned char)(uThis >> 32);
	}
	else
	{
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
	}
	m_pMemery[nPos++] = (unsigned char)(uThis >> 24);
	m_pMemery[nPos++] = (unsigned char)(uThis >> 16);
	m_pMemery[nPos++] = (unsigned char)(uThis >> 8);
	m_pMemery[nPos++] = (unsigned char)uThis;
	//初始化内存
	unsigned short i;
	for ( i = 0; i < m_uMemoryCount; i++ )
	{
		//状态未分配
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		//留空字节
		m_pMemery[nPos++] = 0;
		m_pMemery[nPos++] = 0;
		//保存内存序号
		m_pMemery[nPos++] = (unsigned char) (i >> 8);
		m_pMemery[nPos++] = (unsigned char) i;
		nPos += uMemorySize;
	}
	m_pNext = NULL;
	//标记未分配内存数
	m_uFreeCount = m_uMemoryCount;

	return true;
}

//分配内存
void* MemoryPool::AllocMethod()
{
	void *pObject = NULL;
	int i = 0;
	int nBlockStartPos = 8 + i;
	for ( i = 0; i < m_uMemoryCount; i++ )
	{
		if ( 0 == m_pMemery[nBlockStartPos] )//自由内存，进行分配
		{
			if ( 0 == AtomAdd(&m_pMemery[nBlockStartPos], 1) )//成功标记为已分配
			{
				nBlockStartPos += MEMERY_INFO;
				pObject = &(m_pMemery[nBlockStartPos]);
				break;
			}
			//else 已被别的线程抢先标记
		}
		//定位到下一块内存
		nBlockStartPos += MEMERY_INFO + m_uMemorySize;
	}
	return pObject;
}

void* MemoryPool::Alloc()
{
	//找到可分配的内存池
	MemoryPool *pBlock = this;
	for ( ; NULL != pBlock; pBlock = pBlock->m_pNext )
	{
		if ( 0 < (int32)AtomDec(&pBlock->m_uFreeCount, 1) ) break;//可分配
		AtomAdd(&pBlock->m_uFreeCount, 1);
		if ( NULL == pBlock->m_pNext ) //所有内存池都没有空闲内存可分配，新增一个内存池
		{
			AutoLock lock((Mutex*)m_resizeCtrl);
			if ( NULL == pBlock->m_pNext ) 
			{
				pBlock->m_pNext = new MemoryPool( m_uMemorySize, m_uMemoryCount );
			}
		}
	}
	return pBlock->AllocMethod();
}

void MemoryPool::FreeMethod(unsigned char* pObject)
{
	//内存标记为未分配
	pObject = pObject - MEMERY_INFO;
	AtomSet(pObject,0);
	AtomSelfAdd(&m_uFreeCount);//可分配计数+1
	
	return;
}

void MemoryPool::Free(void* pObj)
{
	unsigned char *pObject = (unsigned char*)pObj;
	MemoryPool *pBlock = GetMemoryBlock( pObject );	
	pBlock->FreeMethod(pObject);

	return;
}

MemoryPool* MemoryPool::GetMemoryBlock(unsigned char* pObj)
{
	unsigned short uIndex = GetMemoryIndex( pObj );
	unsigned char *pMemery = pObj - (uIndex * (MEMERY_INFO+m_uMemorySize)) - MEMERY_INFO - 8;
	uint64 pBlock = 0;
	pBlock = pMemery[0];
	pBlock = (pBlock << 8) + pMemery[1];
	pBlock = (pBlock << 8) + pMemery[2];
	pBlock = (pBlock << 8) + pMemery[3];
	pBlock = (pBlock << 8) + pMemery[4];
	pBlock = (pBlock << 8) + pMemery[5];
	pBlock = (pBlock << 8) + pMemery[6];
	pBlock = (pBlock << 8) + pMemery[7];
	return (MemoryPool*)pBlock;
}

unsigned short MemoryPool::GetMemoryIndex(unsigned char* pObj)
{
	unsigned char *pMemory = pObj - MEMERY_INFO;
	unsigned short uIndex = 0;
	uIndex = pMemory[6];
	uIndex = (uIndex << 8) + pMemory[7];

	return uIndex;
}

}//namespace mdk
