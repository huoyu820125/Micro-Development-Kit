#ifndef MDK_IOBUFFERBLOCK_H
#define MDK_IOBUFFERBLOCK_H

#define BUFBLOCK_SIZE 256
#include <stddef.h>

namespace mdk
{

class MemoryPool;
class Mutex;

/**
 * 数据写入块
 * 池对象类型，每次数据写入，引起1次数据写入块的创建
 */
class IOBufferBlock
{
	friend class IOBuffer;
//////////////////////////////////////////////////////////////////////////
//使用自己的内存管理
private:
	//内存池，在这里为该类型的对象分配内存
	static MemoryPool* ms_pMemoryPool;
	//内存池线程安全锁
	static Mutex* ms_pPoolMutex;
public:
	//new运算符重载
	void* operator new(size_t uObjectSize);
	//delete运算符重载
	void operator delete(void* pObject);
//////////////////////////////////////////////////////////////////////////
//使用自己的内存管理end

private:
	//私有构造函数，只有IOBuffer对象可对写入缓冲块进行访问
	IOBufferBlock();
	//私有虚析构造函数，派生类会自动调用适合的delete
	virtual ~IOBufferBlock();
private:
	/**
	 * IO缓冲
	 * 定义宏BUFBLOCK_SIZE
	 * 编译期静态确定类大小
	 * 不能使用指针在运行时动态指定大小，参考池对象使用限制二
	 */
	unsigned char m_buffer[BUFBLOCK_SIZE];
	//已收到数据的长度
	unsigned short m_uLength;
	//Recv()函数下次读取数据的开始位置
	unsigned short m_uRecvPos;
 
public:
//////////////////////////////////////////////////////////////////////////
//写入方法
	/*
		准备Buffer
		为写入uLength长度的数据准备缓冲，
		如果剩余缓冲长度不足，返回NULL
		否则返回缓冲首地址
		
		写入完成时必须调用WriteFinished()标记可读数据长度
	*/
	unsigned char* PrepareBuffer( unsigned short uLength );
	/**
	 * 写入完成
	 * 标记写入操作写入数据的长度
	 * 必须与PrepareBuffer()成对调用
	 */
	void WriteFinished(unsigned short uLength);

//////////////////////////////////////////////////////////////////////////
//读取方法
	
	//冲缓冲块读取uLength长度的数据
	//返回实际读取的数据长度
	unsigned short ReadData( unsigned char *data, unsigned short uLength, bool bDel = true );
};

}//namespace mdk

#endif //MDK_IOBUFFERBLOCK_H
