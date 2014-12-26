// FinishedTime.h: interface for the FinishedTime class.
//
//////////////////////////////////////////////////////////////////////
/*
	时间统计类
	用于性能统计
	例：
		int g_t = 0;
		void* CountTime( FinishedTime *pTime )
		{
			mdk::uint32 ut = pTime->UseTime();
			if ( ut > 1000 ) g_t++;
			printf( "XXX操作执行时间超过1000ms的次数为%d", gt );
		}
		void XXX()
		{
			FinishedTime count( CountTime );//统计XXX()执行时间，而不用在所有return的地方去写统计代码
			if ( 条件1 ) 
			{
			执行...
			return;
			}
			if ( 条件... ) 
			{
				执行...
				return;
			}
			if ( 条件n ) 
			{
				执行...
				return;
			}
			执行...
		}
 */
#ifndef MDK_FINISHEDTIME_H
#define MDK_FINISHEDTIME_H

#include "Task.h"
#include<ctime>

namespace mdk
{

class FinishedTime
{
public:
	/*
		帮听统计函数
	*/
	FinishedTime( MethodPointer method, void *pObj );
	FinishedTime( FuntionPointer fun );
	~FinishedTime();

	//经过的时间ms
	mdk::uint32 UseTime();
	//执行绑定的统计，用于提前统计
	void Finished();
protected:
private:
	mdk::int64 m_start;
	mdk::int64 m_end;
	mdk::uint32 m_useTime;
	mdk::Task m_task;
	bool m_finished;
};

}

#endif //MDK_FINISHEDTIME_H
