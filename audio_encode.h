#pragma once

#include "was_api_capture.h"
#include "ring_buffer.h"
#include "encoder_aac.h"

#include <thread>

class AudioBufferMgr;

class AudioEncode
{
public:
	AudioEncode(AudioBufferMgr *mgr);
	~AudioEncode();

private:
	static DWORD WINAPI AudioEncodeThread(LPVOID context);
	DWORD DoEncodeThread();
private:
	HANDLE			m_hEncodeThread;
	bool			m_isRunning;

private:
	AudioBufferMgr *m_audioMgr = NULL;

private:
	AACEncoder	m_aacEncoder;
};