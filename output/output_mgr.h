#pragma once

#include <Windows.h>
#include <mutex>
#include <list>
#include <string>
#include "flv_file_writer.h"

struct AVPacket;

class COutPutMgr
{
public:
	static COutPutMgr *Instance();
	~COutPutMgr();

	bool OpenFile(bool hasAudio, bool hasVideo, AVCodecContext *ctxVideo, AVCodecContext *ctxAudio, int fps, std::string filePath);
	int WritePacket(AVPacket *pkt);
	void CloseFile();

private:
	COutPutMgr();

private:
	static DWORD WINAPI WriteThread(LPVOID context);
	DWORD DoWriteThread();

private:
	HANDLE			m_hThread;
	bool			m_isRunning;

private:
	std::mutex  m_mtxList;
	std::list<AVPacket *> m_listPkt;

private:
	FlvFileWriter	m_flvWriter;

private:
	bool m_isReceivedVideo;
	unsigned long long m_firstVideoTimestamp;

private:
	static COutPutMgr *m_pInstance;
};