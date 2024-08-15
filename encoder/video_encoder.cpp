#include "video_encoder.h"
#include "../platform.h"

#include "../output/output_mgr.h"

//https://blog.csdn.net/qq_33844311/article/details/121687742

#define ALIGN_SIZE(size, align) size = (((size) + (align - 1)) & (~(align - 1)))

VideoEncode::VideoEncode()
	:m_hEncodeThread(NULL),
	m_isRunning(true),
	m_encodeSemaphore(NULL)
{
	initSemaphore();

	m_hEncodeThread = CreateThread(NULL, 0, VideoEncodeThread, this, 0, NULL);
	if (m_hEncodeThread == NULL)
	{
		return;
	}
}
VideoEncode::~VideoEncode()
{
	m_isRunning = false;

	WaitForSingleObject(m_hEncodeThread, INFINITE);
	CloseHandle(m_hEncodeThread);
	m_hEncodeThread = NULL;

	destroySemaphore();

	unInitFrameCache();
}
bool VideoEncode::Init(int width, int height, int fps, int bitRate)
{
	m_nWidth = width;
	m_hHeight = height;

	m_nFps = fps;
	m_nBitRate = bitRate;

	m_h264Encoder.Init(width, height, fps, bitRate);

	initFrameCache(width, height);

	return true;
}
void VideoEncode::OnVideoFrame420(const unsigned char *data, int strideY, int strideU, int strideV, int width, int height, unsigned long long timestampNS)
{
	//printf("--------------video---------------%llu, current = %llu\n", timestampNS / 1000000, os_gettime_ns() / 1000000);

	std::lock_guard<std::mutex> lock(m_mutexCache);

	if (m_videoCache.emptyFrames == 0)
	{
		m_videoCache.cache[m_videoCache.writeIndex].count += 1;
		m_videoCache.cache[m_videoCache.writeIndex].skipped += 1;
	}
	else
	{
		if (m_videoCache.emptyFrames != VideoFrameCache::kVideoCacheFrameCount)
		{
			if (++m_videoCache.writeIndex == VideoFrameCache::kVideoCacheFrameCount)
			{
				m_videoCache.writeIndex = 0;
			}
		}

		VideoFrame *currentFrame = &m_videoCache.cache[m_videoCache.writeIndex];

		currentFrame->timeStamp = timestampNS;
		currentFrame->lineSizeY = strideY;
		currentFrame->lineSizeU = strideU;
		currentFrame->lineSizeV = strideV;
		currentFrame->width = width;
		currentFrame->height = height;
		currentFrame->count = 1;
		currentFrame->skipped = 0;

		memcpy((void *)currentFrame->data, data, strideY * height * 3 / 2);

		m_videoCache.emptyFrames--;

		//通知编码线程去编码
		postSemaphore();
	}
}

DWORD VideoEncode::VideoEncodeThread(LPVOID context)
{
	return reinterpret_cast<VideoEncode *>(context)->DoEncodeThread();
}
DWORD VideoEncode::DoEncodeThread()
{
	AVPacket *pkt = av_packet_alloc();

	while (waitSemaphore() == 0)
	{
		if (!m_isRunning)
		{
			break;
		}

		while (m_isRunning)
		{
			bool complete = false;
			bool skipped = false;

			VideoFrame *currentFrame = NULL;

			std::lock_guard<std::mutex> lock(m_mutexCache);

			currentFrame = &m_videoCache.cache[m_videoCache.readIndex];

			//编码
			if (!m_h264Encoder.IsInited())
			{
				if (!m_h264Encoder.Init(currentFrame->width, currentFrame->height, m_nFps, m_nBitRate))
				{
					continue;
				}
			}
			
			bool isReceived = false;
			m_h264Encoder.Encode(currentFrame->data, currentFrame->width, currentFrame->height, *pkt, currentFrame->timeStamp, &isReceived);
			if (isReceived)
			{
				static unsigned long long last = 0;
				unsigned long long current = os_gettime_ns();
				//printf("-----------------------------%llu\n", (current - last) / 1000000);
				last = current;

				COutPutMgr::Instance()->WritePacket(pkt);
			}
			
			av_packet_unref(pkt);

			/*static unsigned long long last = 0;
			unsigned long long current = os_gettime_ns();
			printf("-----------------------------%llu\n", (current - last) / 1000000);
			last = current;*/

			complete = (--currentFrame->count == 0) ? true : false;
			skipped = (currentFrame->skipped > 0) ? true : false;
			if (complete)
			{
				if (++m_videoCache.readIndex == VideoFrameCache::kVideoCacheFrameCount)
				{
					m_videoCache.readIndex = 0;
				}

				if (++m_videoCache.emptyFrames == VideoFrameCache::kVideoCacheFrameCount)
				{
					m_videoCache.writeIndex = m_videoCache.readIndex;
				}

				break;
			}
			else if (skipped)
			{
				--currentFrame->skipped;
				continue;
			}
		}
	}

	av_packet_free(&pkt);

	m_h264Encoder.UnInit();

	return 0;
}
int VideoEncode::initSemaphore()
{
	m_encodeSemaphore = CreateSemaphore(NULL, (LONG)0, 0x7FFFFFFF, NULL);
	if (!m_encodeSemaphore)
	{
		return -1;
	}

	return 0;
}
void VideoEncode::destroySemaphore()
{
	if (m_encodeSemaphore)
	{
		CloseHandle(m_encodeSemaphore);
	}

	m_encodeSemaphore = NULL;
}
int VideoEncode::postSemaphore()
{
	if (!m_encodeSemaphore)
	{
		return -1;
	}
	BOOL nRet = ReleaseSemaphore(m_encodeSemaphore, 1, NULL);
	return nRet ? 0 : -1;
}
int VideoEncode::waitSemaphore()
{
	DWORD ret;
	if (!m_encodeSemaphore)
	{
		return -1;
	}

	ret = WaitForSingleObject(m_encodeSemaphore, INFINITE);
	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}
void VideoEncode::initFrameCache(int width, int height)
{
	for (int i = 0; i < VideoFrameCache::kVideoCacheFrameCount; i++)
	{
		struct VideoFrame *frame = (struct VideoFrame *)&m_videoCache.cache[i];

		int size = width * height;
		ALIGN_SIZE(size, 32);

		const unsigned int half_width = (width + 1) / 2;
		const unsigned int half_height = (height + 1) / 2;
		const unsigned int quarter_area = half_width * half_height;

		size += quarter_area;
		ALIGN_SIZE(size, 32);

		size += quarter_area;
		ALIGN_SIZE(size, 32);

		frame->lineSizeY = width;
		frame->lineSizeU = half_width;
		frame->lineSizeV = half_width;
		frame->data = new unsigned char[size];
	}
}
void VideoEncode::unInitFrameCache()
{
	for (int i = 0; i < VideoFrameCache::kVideoCacheFrameCount; i++)
	{
		struct VideoFrame *frame = (struct VideoFrame *)&m_videoCache.cache[i];

		if (frame->data)
		{
			delete[] frame->data;
		}

		frame->lineSizeY = 0;
		frame->lineSizeU = 0;
		frame->lineSizeV = 0;
		frame->timeStamp = 0;
	}
}