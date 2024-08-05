#include "video_encoder.h"

VideoEncode::VideoEncode()
	:m_hEncodeThread(NULL),
	m_isRunning(true)
{
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

	m_hEncodeThread = NULL;
}

DWORD VideoEncode::VideoEncodeThread(LPVOID context)
{
	return reinterpret_cast<VideoEncode *>(context)->DoEncodeThread();
}
DWORD VideoEncode::DoEncodeThread()
{
	while (m_isRunning)
	{

	}
	return 0;
}