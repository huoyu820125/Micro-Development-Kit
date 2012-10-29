// Logger.cpp: implementation of the Logger class.
//
//////////////////////////////////////////////////////////////////////

#include "mdk/Logger.h"

#include <time.h>
#include <stdarg.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#else
#include   <unistd.h>                     //chdir() 
#include   <sys/stat.h>                 //mkdir() 
#include   <sys/types.h>               //mkdir() 

#endif

namespace mdk
{

Logger::Logger( const char *strErrLog, const char *strRunLog )
{
	m_bRLogOpened = false;
	m_bELogOpened = false;
	m_fpRunLog = m_fpErrLog = NULL;

	m_bPrint = false;
	CreateLogDir();
}

Logger::~Logger()
{
	if ( NULL != m_fpErrLog ) 
	{
		fclose(m_fpErrLog);
		m_fpErrLog = NULL;
	}
	if ( NULL != m_fpRunLog ) 
	{
		fclose(m_fpRunLog);
		m_fpRunLog = NULL;
	}
}

void Logger::CreateLogDir()
{
	if (-1 == access( "./log", 0 ))
	{
#ifdef WIN32
		mkdir( "./log" );
#else
		umask(0);
		if( 0 > mkdir("./log", 0777) )
		{
			printf( "create log faild\n" );
		}
#endif
	}
	if (-1 == access( "./log/err", 0 ))
	{
#ifdef WIN32
		mkdir( "./log/err" );
#else
		umask(0);
		if( 0 > mkdir("./log/err", 0777) )
		{
			printf( "create ./log/err faild\n" );
		}
#endif
	}
	if (-1 == access( "./log/access", 0 ))
	{
#ifdef WIN32
		mkdir( "./log/access" );
#else
		umask(0);
		if( 0 > mkdir("./log/access", 0777) )
		{
			printf( "create ./log/access faild\n" );
		}
#endif
	}

#ifndef WIN32
	umask(0);
	chmod("./log",S_IRWXU|S_IRWXG|S_IRWXO);
	umask(0);
	chmod("./log/access",S_IRWXU|S_IRWXG|S_IRWXO);
	umask(0);
	chmod("./log/err",S_IRWXU|S_IRWXG|S_IRWXO);
#endif
}

bool Logger::OpenErrLog()
{
	time_t cutTime = time(NULL);
	tm *pCurTM = localtime(&cutTime);
	if ( m_bELogOpened ) 
	{
		char strTime[32];
		strftime( strTime, 30, "%Y-%m-%d", pCurTM );
		int nY, nM, nD;
		sscanf( strTime, "%d-%d-%d", &nY, &nM, &nD );
		if ( m_nErrLogCurYear == nY && 
			m_nErrLogCurMonth == nM &&
			m_nErrLogCurDay == nD ) return true;
		fclose(m_fpErrLog) ;
		m_fpErrLog = NULL;
		m_nErrLogCurYear = nY;
		m_nErrLogCurMonth = nM;
		m_nErrLogCurDay = nD;
		m_bELogOpened = false;
	}
	
	char strErrLog[32];
	strftime( strErrLog, 30, "./log/err/%Y-%m-%d.log", pCurTM );
	m_fpErrLog = fopen( strErrLog, "a" );
	m_bELogOpened = NULL != m_fpErrLog;

	return m_bELogOpened;
}

bool Logger::OpenRunLog()
{
	time_t cutTime = time(NULL);
	tm *pCurTM = localtime(&cutTime);
	if ( m_bRLogOpened ) 
	{
		char strTime[32];
		strftime( strTime, 30, "%Y-%m-%d", pCurTM );
		int nY, nM, nD;
		sscanf( strTime, "%d-%d-%d", &nY, &nM, &nD );
		if ( m_nRunLogCurYear == nY && 
			m_nRunLogCurMonth == nM &&
			m_nRunLogCurDay == nD ) return true;
		fclose(m_fpRunLog) ;
		m_fpRunLog = NULL;
		m_nRunLogCurYear = nY;
		m_nRunLogCurMonth = nM;
		m_nRunLogCurDay = nD;
		m_bRLogOpened = false;
	}
	
	char strRunLog[32];
	strftime( strRunLog, 30, "./log/access/%Y-%m-%d.log", pCurTM );
	m_fpRunLog = fopen( strRunLog, "a" );
	m_bRLogOpened = NULL != m_fpRunLog;

	return m_bRLogOpened;
}

void Logger::DelLog( int nDay )
{
	time_t cutTime = time(NULL);
	cutTime -= (86400 * nDay);
	tm *pCurTM = localtime(&cutTime);
	char strErrLog[32];
	strftime( strErrLog, 30, "./log/err/%Y-%m-%d.log", pCurTM );
	char strRunLog[32];
	strftime( strRunLog, 30, "./log/access/%Y-%m-%d.log", pCurTM );
	remove( strErrLog );
	remove( strRunLog );
}

void Logger::SetPrintLog( bool bPrint )
{
	m_bPrint = bPrint;
}

void Logger::Error( const char *format, ... )
{
	AutoLock lock( &m_writeMutex );
	if ( !OpenErrLog() || !OpenRunLog() ) return;
	//取得时间
	time_t cutTime = time(NULL);
	tm *pCurTM = localtime(&cutTime);
	char strTime[32];
	strftime( strTime, 30, "%Y-%m-%d %H:%M:%S", pCurTM );
	
	//写入日志内容
	fprintf( m_fpErrLog, "%s:(Error) ", strTime );
	fprintf( m_fpRunLog, "%s:(Error) ", strTime );
	{
		va_list ap;
		va_start( ap, format );
		vfprintf( m_fpErrLog, format, ap );
		va_end( ap );
	}
	{
		va_list ap;
		va_start( ap, format );
		vfprintf( m_fpRunLog, format, ap );
		va_end( ap );
	}
	fprintf( m_fpErrLog, "\r\n" );
	fprintf( m_fpRunLog, "\r\n" );
	fflush(m_fpErrLog);
	fflush(m_fpRunLog);

	//打印日志内容
	if ( m_bPrint ) 
	{
		printf( "%s:(Error) ", strTime );
		va_list ap;
		va_start( ap, format );
		vprintf( format, ap );
		va_end( ap );
		printf( "\n" );
	}

	return;
}

void Logger::Run( const char *format, ... )
{
	AutoLock lock( &m_writeMutex );
	if ( !OpenRunLog() ) return;
	//取得时间
	time_t cutTime = time(NULL);
	tm *pCurTM = localtime(&cutTime);
	char strTime[32];
	strftime( strTime, 30, "%Y-%m-%d %H:%M:%S", pCurTM );
	//写入日志内容
	fprintf( m_fpRunLog, "%s:", strTime );
	va_list ap;
	va_start( ap, format );
	vfprintf( m_fpRunLog, format, ap );
	va_end( ap );
	fprintf( m_fpRunLog, "\r\n" );
	fflush(m_fpRunLog);

	//打印日志内容
	if ( m_bPrint ) 
	{
		printf( "%s:", strTime );
		va_list ap;
		va_start( ap, format );
		vprintf( format, ap );
		va_end( ap );
		printf( "\n" );
	}
	
	return;
}

void Logger::ErrorStream( unsigned char *stream, int nLen )
{
	AutoLock lock( &m_writeMutex );
	if ( !OpenErrLog() || !OpenRunLog() ) return;
	fprintf( m_fpErrLog, "Error stream:" );
	fprintf( m_fpRunLog, "Error stream:" );
	int i = 0;
	for ( i = 0; i < nLen - 1; i++ ) 
	{
		fprintf( m_fpErrLog, "%x,", stream[i] );
		fprintf( m_fpRunLog, "%x,", stream[i] );
	}
	fprintf( m_fpErrLog, "%x\r\n", stream[i] );
	fprintf( m_fpRunLog, "%x\r\n", stream[i] );
	fflush(m_fpRunLog);
	fflush(m_fpErrLog);
	
	return;
}

void Logger::RunStream( unsigned char *stream, int nLen )
{
	AutoLock lock( &m_writeMutex );
	if ( !OpenRunLog() ) return;
	fprintf( m_fpRunLog, "stream:" );
	int i = 0;
	for ( i = 0; i < nLen - 1; i++ ) 
	{
		fprintf( m_fpRunLog, "%x,", stream[i] );
	}
	fprintf( m_fpRunLog, "%x\r\n", stream[i] );
	fflush(m_fpRunLog);
	
	return;
}

}//namespace mdk
