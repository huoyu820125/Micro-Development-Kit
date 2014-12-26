#include "../../include/mdk/FinishedTime.h"
#include "../../include/mdk/mapi.h"

namespace mdk
{

FinishedTime::FinishedTime( MethodPointer method, void *pObj )
{
	m_finished = false;
	m_task.Accept( method, pObj, this );
	m_start = MillTime();
}
FinishedTime::FinishedTime( FuntionPointer fun )
{
	m_finished = false;
	m_task.Accept( fun, this );
	m_start = MillTime();
}
FinishedTime::~FinishedTime()
{
	Finished();
}

mdk::uint32 FinishedTime::UseTime()
{
	return m_useTime;
}

void FinishedTime::Finished()
{
	if ( m_finished ) return;
	m_finished = true;
	m_end = MillTime();
	m_useTime = m_end - m_start;
	m_task.Execute();
}

}
