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
	  �ⲿ��Ҫ��֤sizeС��buffer��capacity�����Ҳ�����ʣ��ռ� �� m_dataSize + size <= MAX_BUF_SIZE
	*/
	void Push(const unsigned char *data, size_t size);

	/*
	  Peekǰ��Ҫ�ж�m_dataSize��С,���С��size����˵��д�߳�Pending��
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