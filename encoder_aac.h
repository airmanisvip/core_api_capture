#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavcodec/avcodec.h"

#ifdef __cplusplus
};
#endif

#include <list>

class AACEncoder
{
public:
	AACEncoder();
	virtual ~AACEncoder();

public:
	virtual bool Init(unsigned int bitRateBps);
	virtual void UnInit();
	virtual unsigned char *GetHeader(unsigned int &size);

	virtual bool Encode(short *input, unsigned int numInputFrames, AVPacket &pkt, unsigned long long timestamp);

	AVCodecContext *GetCodecCtx()
	{
		return m_pCodecCtx;
	}

public:
	static int adts_header(char *p_adts_header, const int data_length, const int profile, const int samplerate, int channels);
	static void write_adts_frame(char *data, unsigned int size, FILE *file);

private:
	bool encode(AVFrame *inputFrame, AVPacket &pkt);

private:
	AVCodec	*m_pCodec;
	AVCodecContext *m_pCodecCtx;
	AVFrame *m_pInputFrame;

private:
	unsigned char *m_pInputBuffer;
	unsigned int  m_nBufSize;
	unsigned int  m_nBufDataSize;

private:
	bool m_isFirstFrame;
	std::list<unsigned long long> m_curEncodeTSList;
	unsigned long long m_bufStartTS;
};