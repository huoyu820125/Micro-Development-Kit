// Signal.cpp: implementation of the Signal class.
//
//////////////////////////////////////////////////////////////////////

#include "mdk/Signal.h"

namespace mdk
{
	
Signal::Signal()
{
#ifdef WIN32
	m_oneEvent = CreateEvent( NULL, false, false, NULL );
	m_allEvent = CreateEvent( NULL, true, false, NULL );
#else
	m_bNotifyAll = false;
	m_nSignal = 0;
	int nError = 0;
	pthread_mutexattr_t mutexattr;
	if ( 0 != (nError = pthread_mutexattr_init( &mutexattr )) ) return ;
	if ( 0 != (nError = pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_TIMED_NP )) ) return ;
	if ( 0 != (nError = pthread_mutex_init( &m_condMutex,  &mutexattr )) ) return;
	if ( 0 != (nError = pthread_mutexattr_destroy( &mutexattr )) ) return ;
	pthread_cond_init( &m_cond, NULL );
#endif	
}

Signal::~Signal()
{
#ifndef WIN32
	pthread_cond_destroy( &m_cond );
	pthread_mutex_destroy( &m_condMutex );
#endif
}

bool Signal::Wait( unsigned long lMillSecond )
{
	bool bHasSingle = true;
#ifdef WIN32
	ResetEvent( m_allEvent );
	HANDLE events[2];
	events[0] = m_oneEvent;
	events[1] = m_allEvent;
	int nObjIndex = WaitForMultipleObjects( 2, events, false, lMillSecond );
	if ( WAIT_TIMEOUT == nObjIndex ||
		(WAIT_ABANDONED_0 <= nObjIndex && nObjIndex <= WAIT_ABANDONED_0 + 1)
		) bHasSingle = false;
#else
	pthread_mutex_lock( &m_condMutex );
	if ( 0 != m_nSignal ) bHasSingle = true;
	else
	{
		unsigned long notimeout = -1;
		if ( notimeout == lMillSecond )
		{
			pthread_cond_wait(&m_cond, &m_condMutex);
			if ( 0 == m_nSignal && !m_bNotifyAll) 
			{
				pthread_mutex_unlock( &m_condMutex );
				Wait(lMillSecond);
			}
			bHasSingle = true;
		}
		else
		{
			int nSecond = lMillSecond / 1000;
			int nNSecond = (lMillSecond - nSecond * 1000) * 1000;
			timespec timeout;
			timeout.tv_sec=time(NULL) + nSecond;         
			timeout.tv_nsec=nNSecond;
			if ( 0 != pthread_cond_timedwait(&m_cond, &m_condMutex, &timeout) ) bHasSingle = false;
		}
	}
	m_nSignal = 0;
	pthread_mutex_unlock( &m_condMutex );
#endif
	
	return bHasSingle;
}

bool Signal::Notify()
{
#ifdef WIN32
	SetEvent( m_oneEvent );
#else
	pthread_mutex_lock( &m_condMutex );
	m_nSignal = 1;
	m_bNotifyAll = false;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock( &m_condMutex );
#endif
	
	return true;
}

bool Signal::NotifyAll()
{
#ifdef WIN32
	SetEvent( m_allEvent );
#else
	m_bNotifyAll = true;
	pthread_cond_broadcast(&m_cond);
#endif

	return true;
}

}//namespace mdk
