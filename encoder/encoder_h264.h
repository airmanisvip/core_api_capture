#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"

#ifdef __cplusplus
};
#endif

#include <list>

class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();

	bool Init(int width, int height, int fps, int bitRate);
	void UnInit();
	bool Encode(const unsigned char *data, int width, int height, AVPacket &pkt, unsigned long long timestampNS, bool *receivePkt);
	bool IsInited()
	{
		return m_isInit;
	}
	void GetHeaderData(char *data, int &size);

	AVCodecContext *GetCodecCtx()
	{
		return m_pCodecCtx;
	}

private:
	AVCodec			*m_pCodec;
	AVCodecContext	*m_pCodecCtx;
	AVFrame			*m_pInputFrame;

private:
	std::list<unsigned long long> m_cacheTimestamp;

private:
	int		m_nWidth;
	int		m_nHeight;
	int		m_nFps;

private:
	bool			m_isInit;
};