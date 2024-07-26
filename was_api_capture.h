#pragma once

#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <vector>

class AudioCaptureObserver
{
public:
	virtual void OnAudioCaptureRecordData(void *data, int sampleRate, int channels, int bitsPerSample, int sampleCnt, unsigned long long timestampNS = 0) = 0;
};

/**
 * 保证采集的数据的位数为16位
 *
 * 32位时，用float类型存储，sample值的范围[-1, 1],转为16位时，每个采样点要乘以32767后取整即可
 * 16位时，用short类型存储，sample值的范围[-32768, 32767]，转8位时，每个采样点要先转为[-128, 127],然后将[-128, 127]转为[0, 255]
 * 8位时，用unsigned char类型存储， sample值的范围[0, 255]
 * 
 * wasapi is always float
 */

class CWasApiCapture
{
public:
	CWasApiCapture(AudioCaptureObserver *observer);
	~CWasApiCapture();

	bool Initialize(bool isRender = false);
	void UnInitialize();

	bool StartCapture();
	void StopCapture();
private:
	static DWORD WINAPI WSAPICaptureThread(LPVOID context);
	DWORD DoCaptureThread();

	bool InitDevice();
	bool InitClient();
	bool InitCapture();

	BOOL AdjustFormatTo16Bits(WAVEFORMATEX *pwfx);

private:
	bool PushCaptureDataToObserver();

private:
	IMMDeviceEnumerator *m_pEnumerator;
	IMMDevice			*m_pMMDevice;
	IAudioClient		*m_pAudioClient;
	IAudioCaptureClient *m_pAudioCapture;

	HANDLE			m_hRecThread;
	HANDLE			m_hRecStarted;

	HANDLE			m_hInitEvent; //采集线程等待该事件后去初始化
	HANDLE			m_hReadSample;	//硬件有数据后出发该事件
	HANDLE			m_hExitEvent;	//停止采集

private:
	WAVEFORMATEX		m_waveFormat;
	std::vector<BYTE>	m_silenceData;

private:
	unsigned char		*m_10msBuffer = NULL;
	unsigned int		m_bufferSize = 0;
	unsigned long long  m_bufferStartTS = 0;

private:
	AudioCaptureObserver	*m_observer; //每够10ms数据回调一次

	bool			m_isLoopBack;
};