#include "ring_buffer.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

RingBuffer::RingBuffer()
	:m_pData(nullptr),
	m_capacity(0),
	m_writePos(0),
	m_readPos(0),
	m_dataSize(0)
{
}
RingBuffer::~RingBuffer()
{
	Free();
}
void RingBuffer::Init(size_t size)
{
	m_pData = new unsigned char[size];
	m_capacity = size;
	m_writePos = 0;
	m_readPos = 0;
	m_dataSize = 0;
}
void RingBuffer::Free()
{
	delete[] m_pData;

	m_capacity = 0;
	m_writePos = 0;
	m_readPos = 0;
	m_dataSize = 0;
}
void RingBuffer::Reset()
{
	m_writePos = 0;
	m_readPos = 0;
	m_dataSize = 0;

	memset(m_pData, 0, m_capacity);
}
void RingBuffer::Push(const unsigned char *data, size_t size)
{
	//if (size > m_capacity)
	//{
	//	return;
	//}

	if (m_dataSize + size > m_capacity)
	{
		Reset();
	}

	size_t new_pos = m_writePos + size;

	if (new_pos < m_capacity)
	{
		memcpy(m_pData + m_writePos, data, size);
	}
	else
	{
		size_t back_size = m_capacity - m_writePos;
		size_t loop_size = size - back_size;

		if (back_size)
		{
			memcpy(m_pData + m_writePos, data, back_size);
		}
		memcpy(m_pData, data + back_size, loop_size);

		new_pos -= m_capacity;
	}

	m_dataSize += size;
	m_writePos = new_pos;
}
void RingBuffer::Peek(unsigned char *data, size_t size)
{
	assert(size <= m_dataSize);

	if (data && size <= m_dataSize)
	{
		/*
		 ------------------------------
		      |         |  start_size
		 ------------------------------
		   writePos   readPos
		*/
		size_t start_size = m_capacity - m_readPos;
		if (start_size < size)
		{
			memcpy(data, m_pData + m_readPos, start_size);
			memcpy(data + start_size, m_pData, size - start_size);
		}
		else
		{
			memcpy(data, m_pData + m_readPos, size);
		}
	}
}
void RingBuffer::Pop(unsigned char *data, size_t size)
{
	if (!m_dataSize)
	{
		printf("...empty\n");
		m_readPos = m_writePos = 0;
		return;
	}

	Peek(data, size);

	m_dataSize -= size;

	m_readPos += size;
	if (m_readPos >= m_capacity)
	{
		m_readPos -= m_capacity;
	}

	//printf("--left size = %u\n", m_dataSize);
}