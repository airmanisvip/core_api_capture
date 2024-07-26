#include "audio_resampler.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/mathematics.h"
#include "libswresample/swresample.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavdevice/avdevice.h"
#include "inttypes.h"
#include "libavutil/channel_layout.h"

#ifdef __cplusplus
};
#endif

#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "avutil.lib")

static const int kDefaultSampleRate = 44100;
static const int kDefaultChannels = 2;
static const enum AVSampleFormat kDefaultFmt = AV_SAMPLE_FMT_S16;
static const int kDefaultFmtByte = 2;

AudioResampler::AudioResampler()
	:m_swrCtx(NULL),
	m_dstSampleRate(kDefaultSampleRate),
	m_dstChannels(kDefaultChannels),
	m_dstSampleFmt(kDefaultFmt),
	m_resampleBuffer(NULL),
	m_nResampleBufferSize(0),
	m_nResampleCnt(0)
{
}
AudioResampler::~AudioResampler()
{
	UnInit();
}

bool AudioResampler::Init(int srcSampleRate, int srcChannels, enum AVSampleFormat srcFmt, unsigned long long srcChLayout,
	int dstSampleRate, int dstChannels, enum AVSampleFormat dstFmt)
{
	m_dstSampleRate = kDefaultSampleRate;
	m_dstChannels = kDefaultChannels;
	m_dstSampleFmt = kDefaultFmt;
	m_dstChLayout = av_get_default_channel_layout(kDefaultChannels);

	m_srcSampleRate = srcSampleRate;
	m_srcChannels = srcChannels;
	m_srcSampleFmt = srcFmt;

	m_swrCtx = swr_alloc_set_opts(NULL, m_dstChLayout, m_dstSampleFmt, m_dstSampleRate, srcChLayout, srcFmt, srcSampleRate, 0, NULL);
	if (!m_swrCtx)
	{
		return false;
	}

	if (swr_init(m_swrCtx) < 0)
	{
		swr_free(&m_swrCtx);
		m_swrCtx = NULL;
		return false;
	}

	return true;
}

#include <iostream>
void AudioResampler::UnInit()
{
	if (m_swrCtx)
	{
		swr_free(&m_swrCtx);
	}
	m_swrCtx = NULL;

	delete[] m_resampleBuffer;
	m_resampleBuffer = NULL;
}
bool AudioResampler::DoResample(const unsigned char *srcData, int inSampleCnt, unsigned long long &delayNS)
{
	//delayµ¥Î»ÊÇ 1/m_srcSampleRate
	long long delay = swr_get_delay(m_swrCtx, m_srcSampleRate);

	int estimatedSampleCnt = (int)av_rescale_rnd(delay + inSampleCnt, m_dstSampleRate, m_srcSampleRate, AV_ROUND_UP);

	int sizeInByte = estimatedSampleCnt * m_dstChannels * kDefaultFmtByte;

	if (!m_resampleBuffer || sizeInByte > m_nResampleBufferSize)
	{
		delete[] m_resampleBuffer;
		m_resampleBuffer = NULL;

		m_resampleBuffer = new unsigned char[sizeInByte];

		m_nResampleBufferSize = sizeInByte;
	}
	
	delayNS = (unsigned long long)swr_get_delay(m_swrCtx, (1000*1000*1000));

	int ret = swr_convert(m_swrCtx, &m_resampleBuffer, estimatedSampleCnt, &srcData, inSampleCnt);
	if (ret < 0)
	{
		return false;
	}

	m_nResampleCnt = ret;

	return true;
}

unsigned long long AudioResampler::GetDefaultLayout(int channels)
{
	return av_get_default_channel_layout(channels);
}