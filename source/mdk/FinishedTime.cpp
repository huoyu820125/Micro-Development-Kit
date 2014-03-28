#include "../../include/mdk/FinishedTime.h"

namespace mdk
{

FinishedTime::FinishedTime( MethodPointer method, void *pObj )
{
	m_finished = false;
	m_task.Accept( method, pObj, this );
	m_start = clock();
}
FinishedTime::FinishedTime( FuntionPointer fun )
{
	m_finished = false;
	m_task.Accept( fun, this );
	m_start = clock();
}
FinishedTime::~FinishedTime()
{
	Finished();
}

mdk::uint32 FinishedTime::UseTime()
{
	return (mdk::uint32)(((double)m_useTime) / CLOCKS_PER_SEC * 1000);
}

void FinishedTime::Finished()
{
	if ( m_finished ) return;
	m_finished = true;
	m_end = clock();
	m_useTime = m_end - m_start;
	m_task.Execute();
}

}
