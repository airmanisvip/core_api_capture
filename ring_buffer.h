#pragma once

class RingBuffer
{
public:
	RingBuffer();
	~RingBuffer();

	void Init(size_t size);
	void Free();
	void Reset();

	/*
	  #define MAX_BUF_SIZE (1000 * 1024 * sizeof(float))
	  外部需要保证size小于buffer的capacity，并且不大于剩余空间 即 m_dataSize + size <= MAX_BUF_SIZE
	*/
	void Push(const unsigned char *data, size_t size);

	/*
	  Peek前需要判断m_dataSize大小,如果小于size，则说明写线程Pending中
	*/
	void Peek(unsigned char *data, size_t size);
	void Pop(unsigned char *data, size_t size);
	size_t DataSize() { return m_dataSize; }

private:
	unsigned char *m_pData;

	size_t m_capacity;

	size_t m_writePos;
	size_t m_readPos;

	size_t m_dataSize;
};