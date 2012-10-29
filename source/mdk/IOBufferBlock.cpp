#include "mdk/IOBufferBlock.h"
#include "mdk/MemoryPool.h"
#include "mdk/Lock.h"

#include <string.h>

namespace mdk
{

MemoryPool* IOBufferBlock::ms_pMemoryPool = new MemoryPool( sizeof(IOBufferBlock), 10000 );
Mutex* IOBufferBlock::ms_pPoolMutex = new Mutex;

void* IOBufferBlock::operator new(size_t uObjectSize)
{
	void *pObject = ms_pMemoryPool->Alloc();
	return pObject;
}

void IOBufferBlock::operator delete(void* pObject)
{
	ms_pMemoryPool->Free( pObject );
}

IOBufferBlock::IOBufferBlock()
:m_uLength(0), m_uRecvPos(0)
{
}
 
IOBufferBlock::~IOBufferBlock()
{
}

unsigned char* IOBufferBlock::PrepareBuffer(unsigned short uLength)
{
	if ( 0 >= uLength || m_uLength + uLength > BUFBLOCK_SIZE ) return NULL;
	return &m_buffer[m_uLength];
}
 
void IOBufferBlock::WriteFinished(unsigned short uLength)
{
	m_uLength += uLength;
}
 
//冲缓冲块读取uLength长度的数据
unsigned short IOBufferBlock::ReadData( unsigned char *data, unsigned short uLength, bool bDel )
{
	unsigned short uReadSize;
	if ( uLength > m_uLength - m_uRecvPos ) uReadSize = m_uLength - m_uRecvPos;
	else uReadSize = uLength;
	memcpy( data, &m_buffer[m_uRecvPos], uReadSize );
	if ( bDel ) m_uRecvPos += uReadSize;

	return uReadSize;
}

}//namespace mdk
