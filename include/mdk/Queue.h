// Queue.h: interface for the Queue class.
//
//	1对n lock free队列
//	并发模式：1读n写、n读1写
//	push与pop复杂度：O(1)
//////////////////////////////////////////////////////////////////////

#ifndef MDK_QUEUE_H
#define MDK_QUEUE_H

#include "FixLengthInt.h"

namespace mdk
{

class Queue  
{
	typedef	struct QUEUE_NODE
	{
		void *pObject;
		bool IsEmpty;
	}QUEUE_NODE;
	
public:
	Queue( int nSize );
	virtual ~Queue();

public:
	bool Push( void *pObject );
	void* Pop();
	void Clear();//清除数据
protected:
	
private:
	QUEUE_NODE *m_queue;
	uint32 m_push;
	uint32 m_pop;
	uint32 m_nSize;
	int32 m_nWriteAbleCount;
	int32 m_nReadAbleCount;
};

}

#endif // MDK_QUEUE_H
