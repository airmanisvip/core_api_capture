#pragma once

#include "was_api_capture.h"
#include "ring_buffer.h"
#include "audio_fmt.h"
#include "libavutil/samplefmt.h"

#include <mutex>
#include <vector>
#include <list>

class AudioResampler;

struct AudioSegment
{
	std::vector<short> buffer;
	unsigned long long			timestamp; //纳秒

	AudioSegment(short *data, unsigned int numShort, unsigned long long timestamp)
		:timestamp(timestamp)
	{
		buffer.resize(numShort);
		memcpy(buffer.data(), data, numShort * 2);
	}
	void ClearData()
	{
		buffer.clear();
	}
};

class AudioBufferMgr : public AudioCaptureObserver
{
public:
	AudioBufferMgr();
	~AudioBufferMgr();


	void AddAudioSegment(short *data, unsigned int numShort, unsigned long long timestamp);
	AudioSegment *GetAudioSegment();
	unsigned int GetSegmentCnt()
	{
		return m_audioList.size();
	}

public:
	virtual void OnAudioCaptureRecordData(
		void *data,
		int sampleRate, int channels, int bitsPerSample,
		int sampleCnt, unsigned long long timestampNS);

private:
	bool is_need_resample(int srcSampleRate, int srcChannels, AudioFmt srcSampleFmt);
	enum AVSampleFormat audio_fmt_2_ffmpeg_fmt(AudioFmt srcFmt);


private:
	int			m_dstSampleRate;
	int			m_dstChannels;
	AudioFmt	m_dstSampleFmt;
	unsigned long long m_dstChLayout; //多通道布局信息 AV_CH_LAYOUT_STEREO
	bool		m_isNeedResample;

	unsigned char *m_resampleBuffer;

private:
	std::mutex m_lockBuffer;
	std::list<AudioSegment *> m_audioList;
	unsigned long long m_firstTimeStampNS;
	unsigned long long m_nextFrameTimeNS;

	//for test
	unsigned long long m_allSamples;

private:
	AudioResampler *m_resampler;
};