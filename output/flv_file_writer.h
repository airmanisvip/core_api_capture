#pragma once

#include <string>

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavformat/avformat.h>
#include <libavutil/error.h>

#ifdef __cplusplus
};
#endif

class FlvFileWriter
{
public:
	FlvFileWriter();
	~FlvFileWriter();

public:
	bool OpenFile(bool hasAudio, bool hasVideo, AVCodecContext *ctxVideo, AVCodecContext *ctxAudio, int fps, std::string filePath);
	void CloseFile();
	int WritePacket(AVPacket *pkt);

private:
	AVFormatContext *m_pFmtCtx;
	AVStream		*m_pVideoSt;
	AVStream		*m_pAudioSt;
};