#pragma once

#include <Windows.h>
#include "encoder_h264.h"

class VideoEncode
{
public:
	VideoEncode();
	~VideoEncode();

private:
	static DWORD WINAPI VideoEncodeThread(LPVOID context);
	DWORD DoEncodeThread();
private:
	HANDLE			m_hEncodeThread;
	bool			m_isRunning;


private:
	H264Encoder	m_h264Encoder;
};