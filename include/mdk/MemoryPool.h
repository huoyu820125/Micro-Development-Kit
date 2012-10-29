#ifndef MDK_MEMORY_POOL_H
#define MDK_MEMORY_POOL_H

#include "FixLengthInt.h"

#include <stdio.h>
#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif
/*
 *	无锁堆(n读n写并发,不需加锁)
 * 	内存池结构
 * 	n个池组成链表
 * 
 * 	池结构
 * 	每个池都是一段连续内存保存在char数组中
 * 	0~7byte为池地址，后面是n个用于固定长度分配的内存块（即内存块是定长的）
 * 
 * 	内存块结构
 * 	状态4byte+留空2byte+内存块块序号2byte
 * 	所以一个内存池最大可以保存的对象(内存块)数量为unsigned short的最大值65535
 *
 *	状态就2个(分配/未分配)，1个byte就可以表示，使用4byte是为了使用原子操作实现lock-free，而原子操作操作的是4byte地址
 *	2byte留空，是为了保证后面用于分配的内存守地址都是4byte对齐
 *  因为真实new出来的对象首地址是保证这点的，
 *  且IOCP投递缓冲struct，也要求首地址对齐，否则返回10014错误
 */
#define MEMERY_INFO 8

namespace mdk
{

/**
 * 内存池类
 * 
 */
class MemoryPool
{	
private:
	//下一个内存池
	MemoryPool* m_pNext;
	//指向预先申请的供分配的内存池
	unsigned char* m_pMemery;
	//Alloc()函数每次分配出去的地址指向内存空间的大小
	unsigned short m_uMemorySize;
	//管理的内存数
	unsigned short m_uMemoryCount;
	//未分配内存数
	int32 m_uFreeCount;
	void *m_resizeCtrl;
	
public:
	MemoryPool();
	MemoryPool( unsigned short uMemorySize, unsigned short uMemoryCount );
	//析构函数，递归的释放自身和其后所有内存池
	~MemoryPool();
	//初始化内存池
	bool Init(unsigned short uMemorySize, unsigned short uMemoryCount);

	//分配内存(链表方法)
	void* Alloc();
	
	//回收内存(链表方法)
	void Free(void* pObj);

private:
	//分配内存(结点方法)
	void* AllocMethod();

	//回收内存(结点方法)
	void FreeMethod(unsigned char* pObject);	

	//取得地址指向内存所属内存池的地址(链表方法)
	MemoryPool* GetMemoryBlock(unsigned char* pObj);

	//取得地址指向内存在块中序号(链表方法)
	unsigned short GetMemoryIndex(unsigned char* pObj);

	
};

}//namespace mdk

#endif //MDK_MEMORY_POOL_H
