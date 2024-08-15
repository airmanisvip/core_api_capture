#include "flv_file_writer.h"

FlvFileWriter::FlvFileWriter()
	:m_pFmtCtx(NULL),
	m_pVideoSt(NULL),
	m_pAudioSt(NULL)
{
}
FlvFileWriter::~FlvFileWriter()
{
	CloseFile();
}

bool FlvFileWriter::OpenFile(bool hasAudio, bool hasVideo, AVCodecContext *ctxVideo, AVCodecContext *ctxAudio, int fps, std::string filePath)
{
	avformat_alloc_output_context2(&m_pFmtCtx, NULL, "flv", filePath.c_str());
	if (!m_pFmtCtx)
	{
		return false;
	}

	if (hasVideo)
	{
		m_pVideoSt = avformat_new_stream(m_pFmtCtx, NULL);
		if (!m_pVideoSt)
		{
			return false;
		}
		m_pVideoSt->id = m_pFmtCtx->nb_streams - 1;
		m_pVideoSt->time_base.num = 1;
		m_pVideoSt->time_base.den = fps;

		if (avcodec_parameters_from_context(m_pVideoSt->codecpar, ctxVideo) < 0)
		{
			return false;
		}
	}

	if (hasAudio)
	{
		m_pAudioSt = avformat_new_stream(m_pFmtCtx, NULL);
		if (!m_pAudioSt)
		{
			return false;
		}
		m_pAudioSt->id = m_pFmtCtx->nb_streams - 1;
		m_pAudioSt->time_base.num = 1;
		m_pAudioSt->time_base.den = 44100;

		if (avcodec_parameters_from_context(m_pAudioSt->codecpar, ctxAudio) < 0)
		{
			return false;
		}
	}

	int nRet;
	if (!(m_pFmtCtx->flags & AVFMT_NOFILE))
	{
		nRet = avio_open(&m_pFmtCtx->pb, filePath.c_str(), AVIO_FLAG_WRITE);
		if (nRet < 0)
		{
			return false;
		}
	}

	AVDictionary *opt = NULL;
	nRet = avformat_write_header(m_pFmtCtx, &opt);
	if (nRet < 0)
	{
		char buf[256] = { 0 };
		av_make_error_string(buf, 256, nRet);
		
		return false;
	}

	return true;
}
void FlvFileWriter::CloseFile()
{	
	if (m_pFmtCtx)
	{
		av_write_trailer(m_pFmtCtx);

		if (!(m_pFmtCtx->flags & AVFMT_NOFILE))
		{
			avio_closep(&m_pFmtCtx->pb);
		}

		avformat_free_context(m_pFmtCtx);
	}
	m_pFmtCtx = NULL;
}
int FlvFileWriter::WritePacket(AVPacket *pkt)
{
	//输入时间戳单位是ms
	AVRational srcTimeBase;
	srcTimeBase.num = 1;
	srcTimeBase.den = 1000;

	AVRational *dstTimeBase = NULL;

	//0 - video
	if (pkt->stream_index == 0)
	{
		dstTimeBase = &m_pVideoSt->time_base;
		printf("------------video-----------------%llu\n", pkt->pts);
	}
	else
	{
		dstTimeBase = &m_pAudioSt->time_base;
		printf("------------audio-----------------%llu\n", pkt->pts);
	}

	av_packet_rescale_ts(pkt, srcTimeBase, *dstTimeBase);

	return av_interleaved_write_frame(m_pFmtCtx, pkt);
}