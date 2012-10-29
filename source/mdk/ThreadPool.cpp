#include <assert.h>

#include "mdk/ThreadPool.h"
#include "mdk/MemoryPool.h"
#include "mdk/Task.h"

using namespace std;

namespace mdk
{

ThreadPool::ThreadPool()
:m_nMinThreadNum(0), m_nThreadNum(0)
{
	m_pTaskPool = new MemoryPool( sizeof(Task), 10000 );
	m_pContextPool = NULL;
}
 
ThreadPool::~ThreadPool()
{
	Stop();
	if ( NULL != m_pTaskPool ) 
	{
		delete m_pTaskPool;
		m_pTaskPool = NULL;
	}
	if ( NULL != m_pContextPool ) 
	{
		delete m_pContextPool;
		m_pContextPool = NULL;
	}
}
 
bool ThreadPool::Start( int nMinThreadNum )
{
	m_nMinThreadNum = nMinThreadNum;
	m_pContextPool = new MemoryPool( sizeof(THREAD_CONTEXT), m_nMinThreadNum * 2 );
	return CreateThread( m_nMinThreadNum );
}

bool ThreadPool::CreateThread(unsigned short nNum)
{
	AutoLock lock(&m_threadsMutex);
	if ( 0 >= nNum ) nNum = m_nMinThreadNum;
	if ( m_nThreadNum + nNum < m_nMinThreadNum ) nNum = m_nMinThreadNum - m_nThreadNum;//保证最小线程数
	THREAD_CONTEXT *pContext;
	for (int i = 0; i < nNum; i++)
	{
		pContext = CreateContext();
		pContext->bIdle = true;
		pContext->bRun = true;
		pContext->thread.Run( Executor::Bind(&ThreadPool::ThreadFunc), this, pContext );		
		m_threads.insert( threadMaps::value_type(pContext->thread.GetID(), pContext) );
	}
	m_nThreadNum += nNum;
	return true;
}

THREAD_CONTEXT* ThreadPool::CreateContext()
{
	AutoLock lock(&m_contextPoolMutex);
	THREAD_CONTEXT *pContext = new (m_pContextPool->Alloc())THREAD_CONTEXT;
	
	return pContext;
}

void ThreadPool::ReleaseContext(THREAD_CONTEXT *pContext)
{
	AutoLock lock(&m_contextPoolMutex);
	pContext->thread.~Thread();
	m_pContextPool->Free(pContext);
}

void ThreadPool::Stop()
{
	AutoLock lock(&m_threadsMutex);

	threadMaps::iterator it = m_threads.begin();
	//全部设为停止
	for ( it = m_threads.begin(); it != m_threads.end(); it++ )
	{
		it->second->bRun = false;
		m_sigNewTask.Notify();
	}
	//通知所有线程，停止等待任务
	for ( it = m_threads.begin(); it != m_threads.end(); it++ )
	{
		m_sigNewTask.Notify();
	}
	//通知所有线程，停止
	for ( it = m_threads.begin(); it != m_threads.end(); it++ )
	{
		it->second->thread.Stop( 3000 );
		ReleaseContext(it->second);
	}
	m_threads.clear();
	m_nThreadNum = 0;

	return;
}

void ThreadPool::Accept( MethodPointer method, void *pObj, void *pParam )
{
	Task* pTask = CreateTask();
	pTask->Accept(method, pObj, pParam);
	PushTask(pTask);
}

void ThreadPool::Accept( FuntionPointer fun, void *pParam )
{
	Task* pTask = CreateTask();
	pTask->Accept(fun, pParam);
	PushTask(pTask);
}

Task* ThreadPool::CreateTask()
{
	AutoLock lock(&m_taskPoolMutex);
	Task *pTask = new (m_pTaskPool->Alloc())Task;
	
	return pTask;
}

void ThreadPool::ReleaseTask(Task *pTask)
{
	AutoLock lock(&m_taskPoolMutex);
	pTask->~Task();
	m_pTaskPool->Free(pTask);
}

void ThreadPool::PushTask(Task* pTask)
{
	AutoLock lockThread(&m_threadsMutex);//如果有线程正在对线程列队进行操作必须互斥
	AutoLock lock( &m_tasksMutex );
	m_tasks.push_back(pTask);
	m_sigNewTask.Notify();
}

Task* ThreadPool::PullTask()
{
	Task* pTask = NULL;
	vector<Task*>::iterator itTask;
	AutoLock lock( &m_tasksMutex );
	if ( m_tasks.empty() ) return pTask;
	itTask = m_tasks.begin();
	pTask = *itTask;
	m_tasks.erase(itTask);
	
	return pTask;
}

void* ThreadPool::ThreadFunc(void* pParam)
{
	THREAD_CONTEXT *pContext = (THREAD_CONTEXT*)pParam;
	Task *pTask = NULL;
	while ( pContext->bRun )
	{
		if ( !pContext->bRun ) break;//外部停止
		pContext->bIdle = false;
		while ( pContext->bRun )
		{
			pTask = PullTask();//取得任务
			if ( NULL == pTask ) break;
			pTask->Execute();//执行任务
			ReleaseTask(pTask);
		}
		pContext->bIdle = true;
		if ( !m_sigNewTask.Wait() ) continue;//等待任务
	}
	return (void*)0;
}

void ThreadPool::StopIdle()
{
	int nThreadNum = m_nMinThreadNum * 2;
	AutoLock lock(&m_threadsMutex);
	threadMaps::iterator it = m_threads.begin();
	for ( ; m_nThreadNum > nThreadNum && it != m_threads.end(); it++)
	{
		if ( !it->second->bIdle ) continue;
		it->second->bRun = false;
		it->second->thread.Stop( 3000 );
		ReleaseContext(it->second);
		m_threads.erase( it );
		it = m_threads.begin();
		m_nThreadNum--;
	}
	
	return;
}

}//namespace mdk
