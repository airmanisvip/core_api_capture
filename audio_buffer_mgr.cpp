#include "audio_buffer_mgr.h"

#include "audio_util.h"
#include "platform.h"
#include "audio_resampler.h"

const int kAUDIO_ENCODE_FRAMES = 1024;
const int kMAX_RING_BUFFER_SIZE = (1000 * kAUDIO_ENCODE_FRAMES * sizeof(float));

const int kTS_SMOOTHING_THRESHOLD = 70000000ULL;

AudioBufferMgr::AudioBufferMgr()
	:m_firstTimeStampNS(0),
	m_nextFrameTimeNS(0),
	m_allSamples(0),
	m_resampler(NULL),
	m_isNeedResample(false),
	m_resampleBuffer(NULL),
	m_dstChannels(2),
	m_dstSampleFmt(kAudio_Format_S16),
	m_dstSampleRate(44100)
{
}
AudioBufferMgr::~AudioBufferMgr()
{
}
bool AudioBufferMgr::is_need_resample(int srcSampleRate, int srcChannels, AudioFmt srcSampleFmt)
{
	if (srcSampleRate != m_dstSampleRate || srcChannels != m_dstChannels || srcSampleFmt != m_dstSampleFmt)
	{
		return true;
	}
	return false;
}
enum AVSampleFormat AudioBufferMgr::audio_fmt_2_ffmpeg_fmt(AudioFmt srcFmt)
{
	switch (srcFmt)
	{
	case kAudio_Format_U8:
	{
		return AV_SAMPLE_FMT_U8;
	}
	break;
	case kAudio_Format_S16:
	{
		return AV_SAMPLE_FMT_S16;
	}
	break;
	case kAudio_Format_S32:
	{
		return AV_SAMPLE_FMT_S32;
	}
	break;
	case kAudio_Format_FLT:
	{
		return AV_SAMPLE_FMT_FLT;
	}
	break;
	}

	return AV_SAMPLE_FMT_NB;
}

static unsigned long long uint64_diff(unsigned long long ts1, unsigned long long ts2)
{
	return (ts1 < ts2) ? (ts2 - ts1) : (ts1 - ts2);
}

#include <iostream>
//wasapi always AV_SAMPLE_FMT_FLT
void AudioBufferMgr::OnAudioCaptureRecordData(
	void *data,
	int sampleRate, int channels, int bitsPerSample,
	int sampleCnt, unsigned long long timestampNS)
{
	//printf("------------audio-----------------%llu\n", (os_gettime_ns() - timestampNS) / 1000000);

#if 0
	FILE *file = fopen("f:\\1.pcm", "ab+");
	fwrite(data, 1, channels * (bitsPerSample / 8) * sampleCnt, file);
	fclose(file);
#else
	unsigned long long timeStamp = timestampNS;

	if (m_nextFrameTimeNS != 0)
	{
		unsigned long long diff = uint64_diff(m_nextFrameTimeNS, timeStamp);
		if (diff <= kTS_SMOOTHING_THRESHOLD)
		{
			timeStamp = m_nextFrameTimeNS;
		}
	}

	m_nextFrameTimeNS = timeStamp + AudioUtil::AudioFramesToNS(sampleRate, sampleCnt);

	if (is_need_resample(sampleRate, channels, kAudio_Format_FLT))
	{
		if (!m_resampler)
		{
			m_resampler = new AudioResampler();
			bool ret = m_resampler->Init(sampleRate, channels, AV_SAMPLE_FMT_FLT, m_resampler->GetDefaultLayout(channels), m_dstSampleRate, m_dstChannels);
			if (!ret)
			{
				delete m_resampler;
				m_resampler = NULL;
			}
		}
	}

	if (m_resampler)
	{
		unsigned long long delay = 0;
		
		if (m_resampler->DoResample((const unsigned char *)data, sampleCnt, delay))
		{
			timeStamp = timeStamp - delay;

			int reSampleCnt = 0;
			const unsigned char *pBuffer = m_resampler->GetResampleBuffer(reSampleCnt);

			AddAudioSegment((short *)pBuffer, reSampleCnt * 2, timeStamp);
		}
	}
	else
	{
		AddAudioSegment((short *)data, sampleCnt * 2, timeStamp);
	}
#endif
}
void AudioBufferMgr::AddAudioSegment(short *data, unsigned int numShort, unsigned long long timestamp)
{
	//static unsigned long long last = 0;
	//std::cout << "diff = " << (timestamp - last) / (1000) << " samples = " << numShort / 2 << std::endl;
	//last = timestamp;

	AudioSegment *seg = new AudioSegment(data, numShort, timestamp);

	m_lockBuffer.lock();

	m_audioList.push_back(seg);

	m_lockBuffer.unlock();

	//std::cout << "list size = " << m_audioList.size() << std::endl;
}
AudioSegment *AudioBufferMgr::GetAudioSegment()
{
	AudioSegment *seg = NULL;

	if (m_audioList.size() > 0)
	{
		seg = *m_audioList.begin();

		m_lockBuffer.lock();

		m_audioList.pop_front();

		m_lockBuffer.unlock();
	}
	else
	{
		//std::cout << "list is empty ..." << std::endl;
	}

	return seg;
}