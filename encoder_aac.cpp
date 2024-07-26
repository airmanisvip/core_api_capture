#include "encoder_aac.h"
#include "audio_util.h"

static const unsigned int kInputBufferSize = 10 * 1024;
static const unsigned int kFrameSizeInSamples = 1024;
static const unsigned int kFrameSizeInByte = kFrameSizeInSamples * 2 * 2;

AACEncoder::AACEncoder()
	:m_pCodec(NULL),
	m_pCodecCtx(NULL),
	m_pInputFrame(NULL),
	m_pInputBuffer(NULL),
	m_nBufSize(0),
	m_nBufDataSize(0),
	m_isFirstFrame(true),
	m_bufStartTS(0)
{
}
AACEncoder::~AACEncoder()
{
	UnInit();
}
bool AACEncoder::Init(unsigned int bitRateBps)
{
	m_pCodec = avcodec_find_encoder_by_name("libfdk_aac");
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
	m_pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_pCodecCtx->bit_rate = bitRateBps;

	m_pCodecCtx->sample_rate = 44100;
	m_pCodecCtx->channels = 2;
	m_pCodecCtx->frame_size = 1024;
	m_pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	m_pCodecCtx->sample_fmt = m_pCodecCtx->codec->sample_fmts[0];//AV_SAMPLE_FMT_S16;

	m_pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	
	int nRet = avcodec_open2(m_pCodecCtx, m_pCodecCtx->codec, 0);
	if (nRet < 0)
	{
		goto on_error;
	}

	m_pInputFrame = av_frame_alloc();
	if (!m_pInputFrame)
	{
		goto on_error;
	}

	m_pInputFrame->sample_rate = m_pCodecCtx->sample_rate;
	m_pInputFrame->nb_samples = m_pCodecCtx->frame_size;
	m_pInputFrame->channel_layout = m_pCodecCtx->channel_layout;
	m_pInputFrame->format = m_pCodecCtx->sample_fmt;

	nRet = av_frame_get_buffer(m_pInputFrame, 0);
	if (nRet != 0)
	{
		goto on_error;
	}

	return true;

on_error:

	UnInit();

	return false;
}
void AACEncoder::UnInit()
{
	if (m_pCodecCtx)
	{
		avcodec_close(m_pCodecCtx);
		avcodec_free_context(&m_pCodecCtx);
		m_pCodecCtx = NULL;
	}

	av_frame_free(&m_pInputFrame);

	delete[] m_pInputBuffer;
	m_pInputBuffer = NULL;
}
unsigned char *AACEncoder::GetHeader(unsigned int &size)
{
	size = m_pCodecCtx->extradata_size;

	return m_pCodecCtx->extradata;
}

int AACEncoder::adts_header(char *p_adts_header, const int data_length, const int profile, const int samplerate, int channels)
{
	/*
	#define ADTS_HEADER_LEN  7;

const int sampling_frequencies[] = {
	96000,  // 0x0
	88200,  // 0x1
	64000,  // 0x2
	48000,  // 0x3
	44100,  // 0x4
	32000,  // 0x5
	24000,  // 0x6
	22050,  // 0x7
	16000,  // 0x8
	12000,  // 0x9
	11025,  // 0xa
	8000   // 0xb
	// 0xc d e f是保留的
};
	*/
	int adtsLen = data_length + 7;

	p_adts_header[0] = 0xff;					//syncword:0xfff                          高8bits
	p_adts_header[1] = 0xf0;					//syncword:0xfff                          低4bits
	p_adts_header[1] |= (0 << 3);				//MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
	p_adts_header[1] |= (0 << 1);				//Layer:0                                 2bits
	p_adts_header[1] |= 1;						//protection absent:1                     1bit

	p_adts_header[2] = (profile) << 6;			//profile:profile               2bits
	p_adts_header[2] |= (4 & 0x0f) << 2;		//sampling frequency index:sampling_frequency_index  4bits
	p_adts_header[2] |= (0 << 1);				//private bit:0                   1bit
	p_adts_header[2] |= (channels & 0x04) >> 2;	//channel configuration:channels  高1bit
	p_adts_header[3] = (channels & 0x03) << 6; //channel configuration:channels 低2bits
	p_adts_header[3] |= (0 << 5);               //original：0                1bit
	p_adts_header[3] |= (0 << 4);               //home：0                    1bit
	p_adts_header[3] |= (0 << 3);               //copyright id bit：0        1bit
	p_adts_header[3] |= (0 << 2);               //copyright id start：0      1bit
	p_adts_header[3] |= ((adtsLen & 0x1800) >> 11);

	p_adts_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
	p_adts_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
	p_adts_header[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
	p_adts_header[6] = 0xfc;      //11111100       //buffer fullness:0x7ff 低6bits

	return 0;
}
void AACEncoder::write_adts_frame(char *data, unsigned int size, FILE *file)
{
	char adts_header_buf[7] = { 0 };

	adts_header(adts_header_buf, size, 1, 44100, 2);

	fwrite(adts_header_buf, 1, 7, file);
	fwrite(data, 1, size, file);
}

#include <iostream>
bool AACEncoder::Encode(short *input, unsigned int numInputFrames, AVPacket &pkt, unsigned long long timestamp)
{
	////FILE *file = fopen("F:\\1.pcm", "ab+");
	////fwrite(input, 2, numInputFrames, file);
	////fclose(file);

	//return true;

	static unsigned long long last = 0;

	if (m_isFirstFrame)
	{
		m_bufStartTS = timestamp;
		m_curEncodeTSList.push_back(timestamp);

		m_isFirstFrame = false;
	}

	if (!m_pInputBuffer)
	{
		m_nBufSize = kInputBufferSize;
		m_nBufDataSize = 0;
		m_pInputBuffer = new unsigned char[kInputBufferSize];
	}

	bool nRet = false;

	unsigned int oldDataSize = m_nBufDataSize;

	memcpy(m_pInputBuffer + m_nBufDataSize, input, numInputFrames * 2);
	m_nBufDataSize = m_nBufDataSize + numInputFrames * 2;

	if (m_nBufDataSize >= kFrameSizeInByte)
	{
		avcodec_fill_audio_frame(m_pInputFrame, 2, m_pCodecCtx->sample_fmt, m_pInputBuffer, kFrameSizeInByte, 0);

		m_pInputFrame->pts = m_bufStartTS;

		nRet = encode(m_pInputFrame, pkt);
		if (nRet)
		{
			pkt.pts = *m_curEncodeTSList.begin();
			m_curEncodeTSList.pop_front();

			pkt.dts = pkt.pts;

			/*char adts_header_buf[7] = { 0 };
			adts_header(adts_header_buf, pkt.size, 1, 44100, 2);

			FILE *file = fopen("F:\\1.aac", "ab+");
			fwrite(adts_header_buf, 1, 7, file);
			fwrite(pkt.data, 1, pkt.size, file);
			fclose(file);*/
		}

		m_bufStartTS = timestamp + AudioUtil::AudioFramesToNS(m_pCodecCtx->sample_rate, (kFrameSizeInByte - oldDataSize) / (2 * 2));
		m_curEncodeTSList.push_back(m_bufStartTS);
		
		unsigned int remain = m_nBufDataSize - kFrameSizeInByte;
		if (remain > 0)
		{
			memmove(m_pInputBuffer, m_pInputBuffer + kFrameSizeInByte, remain);
		}
		m_nBufDataSize = remain;
	}

	return nRet;
}
bool AACEncoder::encode(AVFrame *inputFrame, AVPacket &pkt)
{
	bool got_packet = false;

	int nRet = avcodec_send_frame(m_pCodecCtx, inputFrame);
	if (nRet == 0)
	{
		nRet = avcodec_receive_packet(m_pCodecCtx, &pkt);

		got_packet = true;
	}

	if (nRet == AVERROR_EOF || nRet == AVERROR(EAGAIN))
	{
		nRet = 0;
	}

	if (nRet < 0)
	{
		return false;
	}

	return got_packet;
}