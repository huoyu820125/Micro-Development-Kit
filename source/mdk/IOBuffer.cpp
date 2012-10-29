// IOBuffer.cpp: implementation of the IOBuffer class.
//
//////////////////////////////////////////////////////////////////////

#include "mdk/IOBuffer.h"
#include "mdk/atom.h"
#include <new>
#include <string.h>
using namespace std;

namespace mdk
{

IOBuffer::IOBuffer()
{
	m_pRecvBufferBlock = NULL;
	m_uDataSize = 0;
	m_recvBufferList.clear();
}

IOBuffer::~IOBuffer()
{
	Clear();
}

//增加一块缓冲块
void IOBuffer::AddBuffer()
{
	m_pRecvBufferBlock = new IOBufferBlock();//申请缓冲块
	if ( NULL == m_pRecvBufferBlock ) return;
	AutoLock lock( &m_mutex );
	m_recvBufferList.push_back( m_pRecvBufferBlock ); //加入缓冲列表
}

/**
 * 取得当前接收缓冲地址,传入将要接收的数据长度
 * 用于保存接收数据
 * 如果传入长度>当前缓冲块剩余长度，则创建新缓冲块
 */
unsigned char* IOBuffer::PrepareBuffer(unsigned short uRecvSize)
{
	if ( uRecvSize > BUFBLOCK_SIZE ) return NULL;
//	AutoLock lock( &m_mutex );
	if ( NULL == m_pRecvBufferBlock ) AddBuffer();//创建第一块缓冲块
	unsigned char* pWriteBuf = m_pRecvBufferBlock->PrepareBuffer( uRecvSize );
	//剩余缓冲不足以保存希望写入的数据长度，增加缓冲块
	if ( NULL == pWriteBuf ) 
	{
		AddBuffer();
		pWriteBuf = m_pRecvBufferBlock->PrepareBuffer( uRecvSize );
	}
	
	return pWriteBuf;//返回第一个空闲字节地址
}

/**
 * 写入完成
 * 标记写入操作写入数据的长度
 * 必须与PrepareBuffer()成对调用
 */
void IOBuffer::WriteFinished(unsigned short uLength)
{
	m_pRecvBufferBlock->WriteFinished( uLength );
	AtomAdd(&m_uDataSize, uLength);
}

//数据写入缓冲
bool IOBuffer::WriteData( char *data, int nSize )
{
	unsigned char *ioBuf = PrepareBuffer( nSize );
	if ( NULL == ioBuf ) return false;
	memcpy( ioBuf, data, nSize );
	WriteFinished( nSize );
	
	return true;
}

/*
 *	从缓冲区读取一定长度的数据
 *	数据长度足够则成功，返回true
 *	否则失败，返回false
 */
bool IOBuffer::ReadData( unsigned char *data, int uLength, bool bDel )
{
	//这里检查m_uDataSize不需要原子操作
	//cpu与内存1次交换最小长度就是4byte,对于小于等于4byte的类型的取值/赋值操作，
	//本身就是原子操作
	if ( 0 > uLength || m_uDataSize < uLength ) return false;//读取长度小于0，或数据不够，不执行读取

	//读取数据
	IOBufferBlock *pRecvBlock = NULL;
	AutoLock lock( &m_mutex );
	vector<IOBufferBlock*>::iterator it = m_recvBufferList.begin();
	unsigned short uRecvSize = 0;
	unsigned short uStartPos = 0;
	
	for ( ; it != m_recvBufferList.end();  )
	{
		pRecvBlock = *it;
		uRecvSize = pRecvBlock->ReadData( &data[uStartPos], uLength, bDel );
		if ( bDel ) AtomDec(&m_uDataSize, uRecvSize);
		if ( uLength == uRecvSize ) return true;//读取完成
		
		//缓冲内容不足够读取，从下一块缓冲读取
		uStartPos += uRecvSize;
		uLength -= uRecvSize;
		if ( bDel )//删除缓冲块
		{
			//前面已经保证数据足够，不可能从正在等待的缓冲块中还接收不完整数据
			//不用做以下检查
			//if ( m_pRecvBufferBlock == pRecvBlock ) m_pRecvBufferBlock = NULL;
			//写操作只会在m_pRecvBufferBlock当前指向的内存上进行
			//删除操作绝对不在m_pRecvBufferBlock当前指向的内存上进行
			//读操作要么在m_pRecvBufferBlock之前的内存上进行
			//要么在m_pRecvBufferBlock当前指向的内存块前端，已经写入完成的byte上进行
			//结论，并发读写永远不会发生冲突，无锁
			delete pRecvBlock;//释放缓冲块
			m_recvBufferList.erase( it );
			it = m_recvBufferList.begin();//准备从下一个缓冲块中读取
		}
		else it++;
	}

	return true;
}

void IOBuffer::Clear()
{
	
	IOBufferBlock *pRecvBlock = NULL;
	vector<IOBufferBlock*>::iterator it = m_recvBufferList.begin();
	for ( ; it != m_recvBufferList.end(); it++ )
	{
		pRecvBlock = *it;
		delete pRecvBlock;
	}
	m_recvBufferList.clear();
	m_pRecvBufferBlock = NULL;
	m_uDataSize = 0;
}

uint32 IOBuffer::GetLength()
{
	return m_uDataSize;
}

}//namespace mdk
