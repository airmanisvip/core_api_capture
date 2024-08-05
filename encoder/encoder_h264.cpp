#include "encoder_h264.h"

H264Encoder::H264Encoder()
	:m_pCodec(NULL),
	m_pCodecCtx(NULL),
	m_pInputFrame(NULL)
{
	m_pInputFrame = av_frame_alloc();
}
H264Encoder::~H264Encoder()
{
	if (m_pInputFrame)
	{
		av_frame_free(&m_pInputFrame);
	}
}

bool H264Encoder::Init(int width, int height, int fps, int bitRate)
{
	int nRet = 0;
	AVDictionary *options = NULL;

	m_pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!m_pCodec)
	{
		return false;
	}

	m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	if (!m_pCodecCtx)
	{
		return false;
	}

	m_pCodecCtx->codec_tag = 0;
	m_pCodecCtx->height = height;
	m_pCodecCtx->width = width;
	m_pCodecCtx->time_base.den = fps;//25;
	m_pCodecCtx->time_base.num = 1;
	m_pCodecCtx->framerate.den = 1;
	m_pCodecCtx->framerate.num = fps;//25;;

	m_pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

	m_pCodecCtx->bit_rate = bitRate;//900000;
	m_pCodecCtx->rc_max_rate = bitRate + 100000;//900000;
	m_pCodecCtx->rc_min_rate = bitRate;//900000;
	m_pCodecCtx->gop_size = 3 * fps;//25;
	m_pCodecCtx->qmin = 20;
	m_pCodecCtx->qmax = 32;

	if (m_pCodecCtx->rc_max_rate >= 1000 * 1000)
	{
		m_pCodecCtx->qmax = 38;
	}

	m_pCodecCtx->max_b_frames = 0;

	m_pCodecCtx->coder_type = FF_CODER_TYPE_AC;
	m_pCodecCtx->qcompress = 1.0;
	m_pCodecCtx->me_cmp |= FF_CMP_CHROMA;
	m_pCodecCtx->me_subpel_quality = 2;
	m_pCodecCtx->me_range = 64;
	m_pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	av_dict_set(&options, "preset", "superfast", 0);

	nRet = avcodec_open2(m_pCodecCtx, m_pCodecCtx->codec, &options);
	if (nRet < 0)
	{
		return false;
	}

	return true;
}
void H264Encoder::UnInit()
{
	if (m_pCodecCtx)
	{
		avcodec_close(m_pCodecCtx);
		avcodec_free_context(&m_pCodecCtx);
		m_pCodecCtx = NULL;
	}
}
bool H264Encoder::Encode(unsigned char *data, int width, int height, AVPacket &pkt, unsigned long long timestampNS, bool *receivePkt)
{
	int got_picture = 0;

	avpicture_fill((AVPicture *)m_pInputFrame, data, m_pCodecCtx->pix_fmt, m_pCodecCtx->width, m_pCodecCtx->height);

	m_pInputFrame->pts = timestampNS;
	m_pInputFrame->width = width;
	m_pInputFrame->height = height;
	m_pInputFrame->format = m_pCodecCtx->pix_fmt;

	int ret = avcodec_send_frame(m_pCodecCtx, m_pInputFrame);
	if (ret == 0)
	{
		ret = avcodec_receive_packet(m_pCodecCtx, &pkt);
	}

	if (ret == 0)
	{
		got_picture = 1;
	}

	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		ret = 0;
	}
	
	if (ret < 0)
	{
		return false;
	}

	if (got_picture && pkt.size)
	{
		*receivePkt = true;

		pkt.pts = timestampNS;
		pkt.dts = timestampNS;
	}
	else
	{
		receivePkt = false;
	}

	return true;
}