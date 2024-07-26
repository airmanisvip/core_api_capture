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
 * ��֤�ɼ������ݵ�λ��Ϊ16λ
 *
 * 32λʱ����float���ʹ洢��sampleֵ�ķ�Χ[-1, 1],תΪ16λʱ��ÿ��������Ҫ����32767��ȡ������
 * 16λʱ����short���ʹ洢��sampleֵ�ķ�Χ[-32768, 32767]��ת8λʱ��ÿ��������Ҫ��תΪ[-128, 127],Ȼ��[-128, 127]תΪ[0, 255]
 * 8λʱ����unsigned char���ʹ洢�� sampleֵ�ķ�Χ[0, 255]
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

	HANDLE			m_hInitEvent; //�ɼ��̵߳ȴ����¼���ȥ��ʼ��
	HANDLE			m_hReadSample;	//Ӳ�������ݺ�������¼�
	HANDLE			m_hExitEvent;	//ֹͣ�ɼ�

private:
	WAVEFORMATEX		m_waveFormat;
	std::vector<BYTE>	m_silenceData;

private:
	unsigned char		*m_10msBuffer = NULL;
	unsigned int		m_bufferSize = 0;
	unsigned long long  m_bufferStartTS = 0;

private:
	AudioCaptureObserver	*m_observer; //ÿ��10ms���ݻص�һ��

	bool			m_isLoopBack;
};