// Logger.h: interface for the Logger class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_LOGGER_H
#define MDK_LOGGER_H

#include <stdio.h>
#include "Lock.h"

namespace mdk
{

class Logger  
{
public:
	Logger( const char *strErrLog, const char *strRunLog );
	virtual ~Logger();

public:
	void SetPrintLog( bool bPrint );
	void DelLog( int nDay );//删除nDay天以前的日志
	void Error( const char *format, ... );//输出错误日志
	void ErrorStream( unsigned char *stream, int nLen );//输出错误流日志
	void Run( const char *format, ... );//输出日志
	void RunStream( unsigned char *stream, int nLen );//输出流日志
private:
	bool OpenErrLog();
	bool OpenRunLog();
	void CreateLogDir();
	bool m_bPrint;
	Mutex m_writeMutex;
	bool m_bRLogOpened;
	bool m_bELogOpened;
	short m_nErrLogCurYear;
	unsigned char m_nErrLogCurMonth;
	unsigned char m_nErrLogCurDay;
	short m_nRunLogCurYear;
	unsigned char m_nRunLogCurMonth;
	unsigned char m_nRunLogCurDay;
	FILE *m_fpRunLog;
	FILE *m_fpErrLog;
};

}//namespace mdk

#endif // MDK_LOGGER_H
