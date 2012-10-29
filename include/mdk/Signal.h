// Signal.h: interface for the Signal class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_SINGLE_H
#define MDK_SINGLE_H

/*
	信号类
	效果
	如果没有线程在等待
	NotifyAll会丢失
	Notify不会丢失

	windows linux差别
	如果NotifyAll时候，另外一个线程正在调Wait，
	这个NotifyAll在windows下可能会丢失
	(因为windows下的Wait会将信号重置为无，然后再等待，
	如果NotifyAll之后，别的线程激活之前，
	正在调用Wait的线程抢先得到cpu时间片，将信号重置了，则Notifyall丢失)

	在linux不会丢失
	

*/
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


namespace mdk
{
	
class Signal  
{
public:
	Signal();
	virtual ~Signal();

	bool Wait( unsigned long nMillSecond = (unsigned long)-1 );
	bool Notify();
	bool NotifyAll();
	
private:
#ifdef WIN32
	HANDLE m_oneEvent;
	HANDLE m_allEvent;
#else
	int m_nSignal;
	bool m_bNotifyAll;
	pthread_cond_t m_cond;
	pthread_mutex_t m_condMutex;
#endif
};

}//namespace mdk

#endif // MDK_SINGLE_H
