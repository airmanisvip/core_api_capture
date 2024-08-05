#include "audio_encode.h"

#include "audio_util.h"
#include "platform.h"

#include "audio_buffer_mgr.h"

#include <iostream>

AudioEncode::AudioEncode(AudioBufferMgr *mgr)
	:m_hEncodeThread(NULL),
	m_isRunning(true),
	m_audioMgr(mgr)
{
	m_hEncodeThread = CreateThread(NULL, 0, AudioEncodeThread, this, 0, NULL);
	if (m_hEncodeThread == NULL)
	{
		return;
	}
}
AudioEncode::~AudioEncode()
{
	m_isRunning = false;

	WaitForSingleObject(m_hEncodeThread, INFINITE);

	m_hEncodeThread = NULL;

	m_audioMgr = NULL;
}

DWORD AudioEncode::AudioEncodeThread(LPVOID context)
{
	return reinterpret_cast<AudioEncode *>(context)->DoEncodeThread();
}

DWORD AudioEncode::DoEncodeThread()
{
	AVPacket *pkt = av_packet_alloc();

	unsigned long long last = 0;
	unsigned long long preTime = os_gettime_ns();

	unsigned char *encodeBuffer = new unsigned char[2 * 2 * 1024];
	unsigned int bufDataSize = 0;
	unsigned long long bufferStartTime = 0;

	m_aacEncoder.Init(64000);

	unsigned int nSize = 0;
	unsigned char *pHeader = m_aacEncoder.GetHeader(nSize);

	while (m_isRunning)
	{
		//10msÒ»´Î
		unsigned long long target = preTime + 5 * 1000000;

		os_sleep_fastto_ns(target);

		AudioSegment *seg = NULL;

		while(m_audioMgr->GetSegmentCnt() > 0)
		{
			seg = m_audioMgr->GetAudioSegment();
			if (!seg)
			{
				break;
			}

			if (m_aacEncoder.Encode(seg->buffer.data(), seg->buffer.size(), *pkt, seg->timestamp))
			{
				/*static unsigned long long last = 0;
				std::cout << "timestamp = " << pkt->pts / 1000 << std::endl;
				last = pkt->pts;*/
			}

			av_packet_unref(pkt);

			delete seg;
		}

		preTime = target;
	}

	delete[] encodeBuffer;
	encodeBuffer = NULL;

	m_aacEncoder.UnInit();

	av_packet_free(&pkt);

	return 0;
}