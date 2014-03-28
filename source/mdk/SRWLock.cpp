// SRWLock.cpp: implementation of the SRWLock class.
//
//////////////////////////////////////////////////////////////////////

#include "../../include/mdk/SRWLock.h"
#include "../../include/mdk/atom.h"
#include "../../include/mdk/mapi.h"
#include <cstdio>
#include <cstring>

SRWLock::SRWLock()
{
	int i = 0;
	for ( ; i < 2000; i++ ) memset(&m_threadData[i], 0, sizeof(THREAD_HIS));
	m_readCount = 0;
	m_writeCount = 0;
	m_ownerThreadId = 0;
	m_bWaitRead = false;
	m_waitWriteCount = 0;
}

SRWLock::~SRWLock()
{
}

SRWLock::THREAD_HIS* SRWLock::GetThreadData( bool lock )
{
	mdk::uint64 curTID = mdk::CurThreadId();
	int i = 0;
	for ( ; i < MAX_THREAD_COUNT; i++ ) 
	{
		if ( 0 == m_threadData[i].threadID ) 
		{
			if ( !lock ) return NULL;
			if ( 0 != mdk::AtomAdd(&m_threadData[i].threadID, 1) ) continue;
			m_threadData[i].threadID = curTID;
			return &m_threadData[i];
		}
		
		if ( curTID != m_threadData[i].threadID ) continue;
		return &m_threadData[i];
	}

	return NULL;
}

void SRWLock::Lock()
{
	THREAD_HIS *threadOp = GetThreadData(true);
	char filename[256];
	sprintf( filename, "./log/%I64d.txt", threadOp->threadID );
	FILE *fp = fopen( filename, "a" );
	if ( NULL == fp )
	{
		fp = fopen( filename, "w" );
	}
	fclose(fp);
	fp = fopen( filename, "a" );
	fprintf( fp, "Lock()£º%d¶Á %dÐ´\n", threadOp->readCount, threadOp->writeCount );
	fclose(fp);
	if ( 0 < threadOp->writeCount )
	{
		mdk::AtomAdd(&m_writeCount, 1);
		threadOp->writeCount++;
		fp = fopen( filename, "a" );
		fprintf( fp, "Ð´Ç¶Ì×Lock()£¬Ö±½Óreturn\n" );
		fclose(fp);
		return;
	}

	m_checkLock.Lock();
	mdk::AtomAdd(&m_writeCount, 1);
	if ( m_ownerThreadId == threadOp->threadID )
	{
		if ( m_readCount == threadOp->readCount )
		{
			threadOp->writeCount++;
			m_checkLock.Unlock();
			fp = fopen( filename, "a" );
			fprintf( fp, "¶ÁÇ¶Ì×Lock()£¬Ö±½Óreturn\n" );
			fclose(fp);
			return;
		}

		mdk::AtomDec(&m_readCount, threadOp->readCount);
		m_bWaitRead = true;
		m_checkLock.Unlock();
		fp = fopen( filename, "a" );
		fprintf( fp, "Lock():wait¶ÁÍê³É\n" );
		fclose(fp);
		m_waitRead.Wait(-1);
		m_checkLock.Lock();
	}
	else 
	{
		if ( threadOp->readCount == mdk::AtomDec(&m_readCount, threadOp->readCount) )
		{
			if ( 0 < threadOp->readCount )
			{
				mdk::AtomAdd(&m_readCount, threadOp->readCount);
				threadOp->writeCount++;
				m_checkLock.Unlock();
				return;
			}
		}
	}
// 	else 
// 	{
// 		if ( threadOp->readCount == mdk::AtomDec(&m_readCount, threadOp->readCount) )
// 		{
// 			if ( m_bWaitRead ) 
// 			{
// 				m_bWaitRead = false;
// 				m_waitRead.Notify();
// 				fp = fopen( filename, "a" );
// 				fprintf( fp, "Lock().»½ÐÑÐ´\n" );
// 				fclose(fp);
// 			}
// 			else if ( 0 < threadOp->readCount )
// 			{
// 				mdk::AtomAdd(&m_readCount, threadOp->readCount);
// 				threadOp->writeCount++;
// 				m_checkLock.Unlock();
// 				return;
// 			}
// 		}
// 	}
 	m_checkLock.Unlock();

	fp = fopen( filename, "a" );
	fprintf( fp, "Lock():waitËø\n" );
	fclose(fp);
	m_lock.Lock();
	fp = fopen( filename, "a" );
	fprintf( fp, "Lock():ÈëËø\n" );
	fclose(fp);
	mdk::AtomAdd(&m_readCount, threadOp->readCount);
	m_ownerThreadId = threadOp->threadID;
	threadOp->writeCount++;
	return;
}

void SRWLock::Unlock()
{
	THREAD_HIS *threadOp = GetThreadData(true);
	mdk::mdk_assert(NULL != threadOp);
	threadOp->writeCount--;
	char filename[256];
	sprintf( filename, "./log/%I64d.txt", threadOp->threadID );
	FILE *fp = fopen( filename, "a" );
	if ( NULL == fp )
	{
		fp = fopen( filename, "w" );
	}
	fclose(fp);
	fp = fopen( filename, "a" );
	fprintf( fp, "Unlock()£º%d¶Á %dÐ´\n", threadOp->readCount, threadOp->writeCount );
	fclose(fp);

	m_checkLock.Lock();
	mdk::AtomDec(&m_writeCount, 1);
	if ( 0 == threadOp->writeCount ) 
	{
		fp = fopen( filename, "a" );
		fprintf( fp, "»½ÐÑ%d¶Á\n", m_waitWriteCount );
		fclose(fp);
		for ( ; m_waitWriteCount > 0; m_waitWriteCount-- ) m_waitWrite.Notify();
	}
	if ( 0 < threadOp->writeCount || 0 < threadOp->readCount ) 
	{
		fp = fopen( filename, "a" );
		fprintf( fp, "Ç¶Ì×ºöÂÔ\n" );
		fclose(fp);
		m_checkLock.Unlock();
		return;
	}
	m_ownerThreadId = 0;
	m_checkLock.Unlock();
	m_lock.Unlock();
	fp = fopen( filename, "a" );
	fprintf( fp, "Unlock()£º³öËø\n" );
	fclose(fp);
/*
	if ( m_bWaitRead ) 
	{
		m_bWaitRead = false;
		m_waitRead.Notify();
		fp = fopen( filename, "a" );
		fprintf( fp, "»½ÐÑÐ´\n" );
		fclose(fp);
	}
*/
		
	return;
}

void SRWLock::ShareLock()
{
	THREAD_HIS *threadOp = GetThreadData(true);
	char filename[256];
	sprintf( filename, "./log/%I64d.txt", threadOp->threadID );
	FILE *fp = fopen( filename, "a" );
	if ( NULL == fp )
	{
		fp = fopen( filename, "w" );
	}
	fclose(fp);
	fp = fopen( filename, "a" );
	fprintf( fp, "ShareLock()£º%d¶Á %dÐ´\n", threadOp->readCount, threadOp->writeCount );
	fclose(fp);

	if ( 0 < threadOp->writeCount || 0 < threadOp->readCount )
	{
		fp = fopen( filename, "a" );
		fprintf( fp, "Ç¶Ì×£¬Ö±½Óreturn\n" );
		fclose(fp);
		threadOp->readCount++;
		mdk::AtomAdd(&m_readCount, 1);
		return;
	}

	m_checkLock.Lock();
	while ( 0 < m_writeCount )
	{
		m_waitWriteCount++;
		m_checkLock.Unlock();
		fp = fopen( filename, "a" );
		fprintf( fp, "ShareLock():WaitÐ´Íê³É\n" );
		fclose(fp);
		m_waitWrite.Wait(-1);
		m_checkLock.Lock();
	}

	if ( 0 < mdk::AtomAdd(&m_readCount, 1) )
	{
		threadOp->readCount++;
		m_checkLock.Unlock();
		fp = fopen( filename, "a" );
		fprintf( fp, "¶ÁÇ¶Ì×,Ö±½Óreturn\n" );
		fclose(fp);
		return;
	}
	fp = fopen( filename, "a" );
	fprintf( fp, "ShareLock():WaitËø\n" );
	fclose(fp);
	m_lock.Lock();
	fp = fopen( filename, "a" );
	fprintf( fp, "ShareLock():ÈëËø\n" );
	fclose(fp);
	m_ownerThreadId = threadOp->threadID;
	threadOp->readCount++;
	m_checkLock.Unlock();

	return;
}

void SRWLock::ShareUnlock()
{
	THREAD_HIS *threadOp = GetThreadData(true);
	mdk::mdk_assert(NULL != threadOp);
	threadOp->readCount--;
	char filename[256];
	sprintf( filename, "./log/%I64d.txt", threadOp->threadID );
	FILE *fp = fopen( filename, "a" );
	if ( NULL == fp )
	{
		fp = fopen( filename, "w" );
	}
	fclose(fp);
	fp = fopen( filename, "a" );
	fprintf( fp, "ShareUnlock()£º%d¶Á %dÐ´\n", threadOp->readCount, threadOp->writeCount );
	fclose(fp);

	m_checkLock.Lock();
	if ( 1 != mdk::AtomDec(&m_readCount, 1) ) 
	{
		m_checkLock.Unlock();
		fp = fopen( filename, "a" );
		fprintf( fp, "¶ÁÎ´Íê³É£¬ºöÂÔShareUnlock\n" );
		fclose(fp);
		return;
	}

	if ( 0 < threadOp->writeCount ) 
	{
		m_checkLock.Unlock();
		fp = fopen( filename, "a" );
		fprintf( fp, "Ð´Î´Íê³É£¬ºöÂÔShareUnlock\n" );
		fclose(fp);
		return;
	}
	if ( m_bWaitRead ) 
	{
		m_bWaitRead = false;
		m_waitRead.Notify();
		fp = fopen( filename, "a" );
		fprintf( fp, "»½ÐÑÐ´\n" );
		fclose(fp);
	}
	fp = fopen( filename, "a" );
	fprintf( fp, "ShareUnlock():³öËø\n" );
	fclose(fp);
	m_ownerThreadId = 0;
	m_lock.Unlock();
	m_checkLock.Unlock();
}
