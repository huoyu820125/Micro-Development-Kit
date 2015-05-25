// Signal.cpp: implementation of the Signal class.
//
//////////////////////////////////////////////////////////////////////

#include "../../include/mdk/Signal.h"
#include "../../include/mdk/atom.h"
#include <ctime>

#ifdef WIN32
#include <windows.h>
#endif

namespace mdk
{
	
Signal::Signal()
{
#ifdef WIN32
	m_signal = CreateEvent( NULL, false, false, NULL );
#else
	m_waitCount = 0;
	sem_init( &m_signal, 0, 0 );
#endif	
}

Signal::~Signal()
{
#ifdef WIN32
	if (NULL != m_signal) CloseHandle(m_signal);
#else
	sem_destroy(&m_signal);
#endif
}

bool Signal::Wait( unsigned long lMillSecond )
{
	bool bHasSingle = true;
#ifdef WIN32
	int nObjIndex = WaitForSingleObject( m_signal, lMillSecond );
	if ( WAIT_TIMEOUT == nObjIndex ||
		(WAIT_ABANDONED_0 <= nObjIndex && nObjIndex <= WAIT_ABANDONED_0 + 1)
		) bHasSingle = false;
#else
	AtomAdd(&m_waitCount, 1);
	unsigned long notimeout = -1;
	if ( notimeout == lMillSecond )
	{
		sem_wait( &m_signal );//等待任务
	}
	else
	{
		int nSecond = lMillSecond / 1000;
		int nNSecond = (lMillSecond % 1000) * 1000000;
		
		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += nSecond;
		timeout.tv_nsec += nNSecond;
		
		timeout.tv_sec += timeout.tv_nsec / (1000 * 1000 * 1000);
		timeout.tv_nsec = timeout.tv_nsec % (1000 * 1000 * 1000);
		if ( 0 != sem_timedwait(&m_signal, &timeout) ) bHasSingle = false;
	}
	/*
		windows行为，在没有wait时，notify n次 之后再有多个wait，只能通过一个
		linux行为，在没有wait时，notify n次 之后再有多个wait，会通过n个，第n+1个开始阻塞
		简单说就是windows上第2~n个notify的信号丢失了

		为了和windows行为一致，当等待线程为0时，将多余的信号丢弃
	 */
	if ( 1 == AtomDec(&m_waitCount, 1) )
	{
		sem_init( &m_signal, 0, 0 );
	}
#endif
	
	return bHasSingle;
}

bool Signal::Notify()
{
#ifdef WIN32
	SetEvent( m_signal );
#else
	sem_post(&m_signal);	
#endif
	
	return true;
}

}//namespace mdk
