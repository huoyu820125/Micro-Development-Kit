// IOBuffer.h: interface for the IOBuffer class.
//
//////////////////////////////////////////////////////////////////////
/*
 	无锁io缓冲（无锁列队，保证顺序性，完整性，一致性）
	无锁列队概念：同时存在且最多存在2个线程，1个写1个读，不需要加锁的列队

	读或者写线程>2时，对于顺序缓冲来说，本来就是不允许的，
	因为无法保证顺序写/读，加不加锁都无法得到正确结果，
	所以无锁列队就是指1读1写不需要加锁的列队，不需要支持n写n读
 */
#ifndef MDK_IOBUFFER_H
#define MDK_IOBUFFER_H

#include "FixLengthInt.h"
#include "IOBufferBlock.h"
#include "Lock.h"
#include <vector>

namespace mdk
{

class IOBuffer  
{
public:
	IOBuffer();
	virtual ~IOBuffer();

private:
	/**
	 * 缓冲
	 * std::vector<IOBufferBlock*>类型
	 */
	std::vector<IOBufferBlock*> m_recvBufferList;
	/**
	 * 缓冲中数据长度
	 * 使用原子操作修改
	 */
	uint32 m_uDataSize;
	/**
	 * 当前等待写入数据的空闲缓冲块
	 */
	IOBufferBlock* m_pRecvBufferBlock;
	Mutex m_mutex;

public:
	//向缓冲区写入一段数据
	bool WriteData( char *data, int nSize );
	/*
	 *	从缓冲区接收一定长度的数据
	 *	数据长度足够则成功，返回true
	 *	否则失败，返回false
	 */
	bool ReadData( unsigned char *data, int uLength, bool bDel = true );
	//清空缓冲
	void Clear();

	/*
		准备Buffer
		为写入uLength长度的数据准备缓冲，
		如果当前缓冲块(m_pRecvBufferBlock)空闲空间不足，则创建一块新缓冲
		
		写入完成时必须调用WriteFinished()标记可读数据长度
	*/
	unsigned char* PrepareBuffer(unsigned short uRecvSize);
	/**
	 * 写入完成
	 * 标记写入操作写入数据的长度
	 * 必须与PrepareBuffer()成对调用
	 */
	void WriteFinished(unsigned short uLength);

	uint32 GetLength();

protected:
	//增加一块缓冲块
	void AddBuffer();	
};

}//namespace mdk

#endif // MDK_IOBUFFER_H
