#include "output_mgr.h"
#include <algorithm>

COutPutMgr *COutPutMgr::m_pInstance = NULL;

COutPutMgr *COutPutMgr::Instance()
{
	if (m_pInstance) return m_pInstance;

	m_pInstance = new COutPutMgr();
	return m_pInstance;
}
COutPutMgr::COutPutMgr()
	:m_isReceivedVideo(false),
	m_hThread(NULL),
	m_isRunning(true),
	m_firstVideoTimestamp(0)
{
}
COutPutMgr::~COutPutMgr()
{
}

bool COutPutMgr::OpenFile(bool hasAudio, bool hasVideo, AVCodecContext *ctxVideo, AVCodecContext *ctxAudio, int fps, std::string filePath)
{
	m_hThread = CreateThread(NULL, 0, WriteThread, this, 0, NULL);
	if (m_hThread == NULL)
	{
		return false;
	}

	return m_flvWriter.OpenFile(hasAudio, hasVideo, ctxVideo, ctxAudio, fps, filePath);
}
int COutPutMgr::WritePacket(AVPacket *pkt)
{
	std::lock_guard<std::mutex> lock(m_mtxList);

	if (pkt->stream_index == 1 && (!m_firstVideoTimestamp || pkt->pts < m_firstVideoTimestamp))
	{
		return 0;
	}

	if (pkt->stream_index == 0)
	{
		m_isReceivedVideo = true;
	}

	AVPacket *dstPkt = av_packet_alloc();

	av_packet_ref(dstPkt, pkt);

	dstPkt->pts = dstPkt->pts / 1000000;
	dstPkt->dts = dstPkt->pts;

	m_listPkt.push_back(dstPkt);

	std::stable_sort(m_listPkt.begin(), m_listPkt.end(), [](AVPacket *a, AVPacket *b) { return a->pts < b->pts; });

	return 0;
}
void COutPutMgr::CloseFile()
{
	m_isRunning = false;

	WaitForSingleObject(m_hThread, INFINITE);
	
	CloseHandle(m_hThread);

	m_hThread = NULL;

	m_flvWriter.CloseFile();
}
DWORD COutPutMgr::WriteThread(LPVOID context)
{
	return reinterpret_cast<COutPutMgr *>(context)->DoWriteThread();
}
DWORD COutPutMgr::DoWriteThread()
{
	while (true)
	{
		if (!m_isRunning)
		{
			break;
		}

		if (m_firstVideoTimestamp > 0 && m_listPkt.size() < 7)
		{
			Sleep(1);
			continue;
		}
		else if (m_listPkt.size() <= 0)
		{
			Sleep(1);
			continue;
		}

		AVPacket *pkt = NULL;
		{
			std::lock_guard<std::mutex> lock(m_mtxList);
			pkt = *m_listPkt.begin();
			m_listPkt.pop_front();
		}

		if (pkt)
		{
			if (m_firstVideoTimestamp <= 0 && pkt->stream_index == 0)
			{
				m_firstVideoTimestamp = pkt->pts;
			}

			int ret = m_flvWriter.WritePacket(pkt);

			av_packet_unref(pkt);
		}
	}

	return 0;
}
