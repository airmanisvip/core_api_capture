#pragma once

#include <Windows.h>
#include <mutex>

#include "encoder_h264.h"
#include "../dshow/video_capture_dshow.h"

class VideoEncode : public VideoCaptureObserver
{
public:
	VideoEncode();
	~VideoEncode();

	bool Init(int width, int height, int fps, int bitRate);

	//
	// VideoCaptureObserver
	//
	virtual void OnVideoFrame420(const unsigned char *data, int strideY, int strideU, int strideV, int width, int height, unsigned long long timestampNS);

	AVCodecContext *GetCodecCtx()
	{
		return m_h264Encoder.GetCodecCtx();
	}
private:
	static DWORD WINAPI VideoEncodeThread(LPVOID context);
	DWORD DoEncodeThread();

private:
	int initSemaphore();
	void destroySemaphore();
	int  postSemaphore();
	int  waitSemaphore();

	void initFrameCache(int width, int height);
	void unInitFrameCache();
private:
	HANDLE			m_hEncodeThread;
	bool			m_isRunning;

private:
	HANDLE			m_encodeSemaphore;
	std::mutex		m_mutexCache;
	VideoFrameCache m_videoCache;

private:
	H264Encoder	m_h264Encoder;

private:
	int     m_nWidth;
	int		m_hHeight;
	int		m_nFps;
	int		m_nBitRate;
};