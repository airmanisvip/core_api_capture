#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavutil/samplefmt.h"


#ifdef __cplusplus
};
#endif

struct SwrContext;

class AudioResampler
{
public:
	AudioResampler();
	~AudioResampler();

	bool Init(int srcSampleRate, int srcChannels, enum AVSampleFormat srcFmt, unsigned long long srcChLayout,
		int dstSampleRate = 44100, int dstChannels = 2, enum AVSampleFormat dstFmt = AV_SAMPLE_FMT_S16);
	void UnInit();

	//outSampleCnt - 输入为 dstData 空间大小，输出为 dstData中sample个数
	bool DoResample(const unsigned char *srcData, int inSampleCnt, unsigned long long &delayNS);

	unsigned long long GetDefaultLayout(int channels);
	inline const unsigned char *GetResampleBuffer(int &sampleCnt) const
	{
		sampleCnt = m_nResampleCnt;

		return m_resampleBuffer;
	}

private:
	int		m_srcSampleRate;
	int     m_srcChannels;
	enum AVSampleFormat		m_srcSampleFmt;

private:
	int		m_dstSampleRate;
	int     m_dstChannels;
	unsigned long long		m_dstChLayout;
	enum AVSampleFormat		m_dstSampleFmt;

private:
	SwrContext *m_swrCtx;

private:
	unsigned char *m_resampleBuffer;
	int m_nResampleBufferSize;
	int m_nResampleCnt;
};