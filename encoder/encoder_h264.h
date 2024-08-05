#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavcodec/avcodec.h"

#ifdef __cplusplus
};
#endif

class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();

	bool Init(int width, int height, int fps, int bitRate);
	void UnInit();
	bool Encode(unsigned char *data, int width, int height, AVPacket &pkt, unsigned long long timestampNS, bool *receivePkt);

private:
	AVCodec			*m_pCodec;
	AVCodecContext	*m_pCodecCtx;
	AVFrame			*m_pInputFrame;
};