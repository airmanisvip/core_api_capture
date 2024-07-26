#include "was_api_capture.h"

#include <iostream>
#include "platform.h"
#include "audio_util.h"

#define SAFE_RELEASE(p) \
  do {                  \
    if ((p)) {          \
      (p)->Release();   \
      (p) = NULL;       \
    }                   \
  } while (0)

const unsigned int kBufferSize = (1024 * 1024);

static const unsigned int kBufferTime100NS = (5 * 10000000);

CWasApiCapture::CWasApiCapture(AudioCaptureObserver *observer):
	m_pEnumerator(NULL),
	m_pMMDevice(NULL),
	m_pAudioClient(NULL),
	m_pAudioCapture(NULL),
	m_hRecThread(NULL),
	m_hRecStarted(NULL),
	m_hInitEvent(NULL),
	m_hReadSample(NULL),
	m_hExitEvent(NULL),
	m_observer(observer),
	m_10msBuffer(NULL),
	m_isLoopBack(false)
{
	memset(&m_waveFormat, 0, sizeof(m_waveFormat));

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}
CWasApiCapture::~CWasApiCapture()
{
	UnInitialize();

	delete[] m_10msBuffer;
	m_10msBuffer = NULL;
}
bool CWasApiCapture::Initialize(bool isRender)
{
	HRESULT hr = S_OK;

	m_isLoopBack = isRender;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&m_pEnumerator));
	if (m_pEnumerator == NULL)
	{
		goto on_error;
	}
	
	m_hRecStarted = CreateEvent(NULL, false, false, NULL);
	if (NULL == m_hRecStarted)
	{
		goto on_error;
	}

	m_hInitEvent = CreateEvent(NULL, false, false, NULL);
	if (NULL == m_hInitEvent)
	{
		goto on_error;
	}

	m_hExitEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (m_hExitEvent == NULL)
	{
		goto on_error;
	}

	m_hReadSample = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (m_hReadSample == NULL)
	{
		goto on_error;
	}

	return true;

on_error:
	UnInitialize();
	return false;
}
void CWasApiCapture::UnInitialize()
{
	SAFE_RELEASE(m_pMMDevice);
	SAFE_RELEASE(m_pEnumerator);

	if (m_hInitEvent)
	{
		CloseHandle(m_hInitEvent);
	}
	if (m_hExitEvent)
	{
		CloseHandle(m_hExitEvent);
	}
	if (m_hReadSample)
	{
		CloseHandle(m_hReadSample);
	}
	if (m_hRecStarted)
	{
		CloseHandle(m_hRecStarted);
	}

	m_hInitEvent = NULL;
	m_hReadSample = NULL;
	m_hExitEvent = NULL;
	m_hRecStarted = NULL;
}

bool CWasApiCapture::StartCapture()
{
	m_hRecThread = CreateThread(NULL, 0, WSAPICaptureThread, this, 0, NULL);
	if (m_hRecThread == NULL)
	{
		return false;
	}
	SetThreadPriority(m_hRecThread, THREAD_PRIORITY_TIME_CRITICAL);

	WaitForSingleObject(m_hRecStarted, INFINITE);

	SetEvent(m_hInitEvent);

	return true;
}
void CWasApiCapture::StopCapture()
{
	if (m_hRecThread)
	{
		SetEvent(m_hExitEvent);

		WaitForSingleObject(m_hRecThread, INFINITE);
	}
}
DWORD WINAPI CWasApiCapture::WSAPICaptureThread(LPVOID context)
{
	return reinterpret_cast<CWasApiCapture *>(context)->DoCaptureThread();
}
DWORD CWasApiCapture::DoCaptureThread()
{
	bool keepRecording = true;
	HANDLE initArray[2] = { m_hExitEvent, m_hInitEvent };
	HANDLE eventArray[2] = { m_hExitEvent, m_hReadSample };

	//初始化com
	HRESULT hr_com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr_com))
	{
		return hr_com;
	}

	SetEvent(m_hRecStarted);

	DWORD initResult = WaitForMultipleObjects(2, initArray, FALSE, INFINITE);
	if (initResult == WAIT_OBJECT_0) //exit event
	{
		return 0;
	}
	else
	{
		bool res = InitDevice();
		if (!res)
		{
			goto on_error;
		}
		res = InitClient();
		if (!res)
		{
			goto on_error;
		}
		res = InitCapture();
		if (!res)
		{
			goto on_error;
		}
	}

	while (keepRecording)
	{
		DWORD waitResult = WaitForMultipleObjects(2, eventArray, FALSE, 10);
		if (waitResult == WAIT_OBJECT_0) //收到退出采集事件
		{
			keepRecording = false;
		}
		else if (waitResult == (WAIT_OBJECT_0 + 1) || (waitResult == WAIT_TIMEOUT)) //收到声卡采集到samples事件
		{
			PushCaptureDataToObserver();
		}
		else
		{
			goto on_error;
		}
	}

on_error:

	if (m_pAudioClient)
	{
		m_pAudioClient->Stop();
	}
	
	if (hr_com >= 0)
	{
		CoUninitialize();
	}

	SAFE_RELEASE(m_pAudioClient);
	SAFE_RELEASE(m_pAudioCapture);
	SAFE_RELEASE(m_pMMDevice);

	return 0;
}
bool CWasApiCapture::InitDevice()
{
	HRESULT hr;

	hr = m_pEnumerator->GetDefaultAudioEndpoint(m_isLoopBack ? eRender : eCapture, eConsole, &m_pMMDevice);
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}
bool CWasApiCapture::InitClient()
{
	DWORD flags;

	HRESULT hr(S_OK);
	WAVEFORMATEX *pWaveFmtIn = NULL;

	hr = m_pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pAudioClient);
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_pAudioClient->GetMixFormat(&pWaveFmtIn);
	if (FAILED(hr))
	{
		goto on_error;
	}

	//* 加上这个后 每次采集的sample个数会出现有波动 即会出现96个sample的时候？？？
	/*if (!AdjustFormatTo16Bits(pWaveFmtIn))
	{
		goto on_error;
	}*/

	memcpy(&m_waveFormat, pWaveFmtIn, sizeof(m_waveFormat));

	flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	if (m_isLoopBack)
	{
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
	}

	hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		flags,
		kBufferTime100NS,
		0,
		pWaveFmtIn,
		NULL);
	if (hr != S_OK)
	{
		goto on_error;
	}

	if (pWaveFmtIn)
	{
		CoTaskMemFree(pWaveFmtIn);
	}

	return true;

on_error:

	if (pWaveFmtIn)
	{
		CoTaskMemFree(pWaveFmtIn);
	}

	SAFE_RELEASE(m_pAudioClient);

	return false;
}
bool CWasApiCapture::InitCapture()
{
	HRESULT hr(S_OK);

	hr = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void **)&m_pAudioCapture);
	if (FAILED(hr))
	{
		return false;
	}
	hr = m_pAudioClient->SetEventHandle(m_hReadSample);
	if (FAILED(hr))
	{
		goto on_error;
	}

	hr = m_pAudioClient->Start();
	if (FAILED(hr))
	{
		goto on_error;
	}

	return true;

on_error:
	SAFE_RELEASE(m_pAudioCapture);

	return false;
}
BOOL CWasApiCapture::AdjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
	BOOL bRet(FALSE);

	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
		pwfx->wBitsPerSample = 16;
		pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
		pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;

		bRet = TRUE;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
		if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
		{
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
			pwfx->wBitsPerSample = 16;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;

			bRet = TRUE;
		}
	}

	return bRet;
}

static unsigned long long uint64_diff(unsigned long long ts1, unsigned long long ts2)
{
	return (ts1 < ts2) ? (ts2 - ts1) : (ts1 - ts2);
}

//bool CWasApiCapture::PushCaptureDataToObserver()
//{
//	BYTE *pData = NULL;
//	HRESULT hr;
//	UINT32 num_frames_in_next_packet = 0;
//
//	static unsigned long long totalsamples = 0;
//
//	int i = 0;
//	while (true)
//	{
//		DWORD flags = 0;
//		UINT32 num_frames_to_read = 0;
//		UINT64 recPos = 0;
//		UINT64 recTime = 0; //单位为 100纳秒，转毫秒 (recTime*100)/1000000
//
//		hr = m_pAudioCapture->GetNextPacketSize(&num_frames_in_next_packet);
//		if (FAILED(hr))
//		{
//			return false;
//		}
//
//		if (!num_frames_in_next_packet)
//			return false;
//
//		hr = m_pAudioCapture->GetBuffer(&pData, &num_frames_to_read, &flags, &recPos, &recTime);
//		if (FAILED(hr))
//		{
//			return false;
//		}
//
//		totalsamples = totalsamples + num_frames_to_read;
//
//
//		unsigned long long recTimeNS = recTime * 100;
//		if (!m_bufferStartTS)
//		{
//			m_bufferStartTS = recTimeNS;
//		}
//
//		unsigned int requiredSize = m_waveFormat.nChannels * (m_waveFormat.wBitsPerSample / 8) * num_frames_to_read;
//
//		if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
//		{	
//			if (m_silenceData.size() < requiredSize)
//			{
//				m_silenceData.resize(requiredSize);
//			}
//			pData = m_silenceData.data();
//		}
//
//		//buffer中数据是否够 10ms 
//		const unsigned int k10msSamples = (m_waveFormat.nSamplesPerSec * 10) / 1000;
//		const unsigned int k10msSampleSizeByte = m_waveFormat.nChannels * (m_waveFormat.wBitsPerSample / 8) * k10msSamples;
//
//		if (!m_10msBuffer)
//		{
//			m_10msBuffer = new unsigned char[kBufferSize];
//		}
//
//		while (m_bufferSize >= k10msSampleSizeByte)
//		{
//			if (m_observer)
//			{
//				m_observer->OnAudioCaptureRecordData(
//					m_10msBuffer,
//					m_waveFormat.nSamplesPerSec,
//					m_waveFormat.nChannels,
//					m_waveFormat.wBitsPerSample,
//					k10msSamples,
//					m_bufferStartTS);
//			}
//
//			unsigned int remain = m_bufferSize - k10msSampleSizeByte;
//
//			if (remain > 0)
//			{
//				MoveMemory(m_10msBuffer, m_10msBuffer + k10msSampleSizeByte, remain);
//			}
//
//			m_bufferSize = remain;
//
//			m_bufferStartTS = m_bufferStartTS + AudioUtil::AudioFramesToNS(m_waveFormat.nSamplesPerSec, k10msSamples);
//		}
//
//		if (m_bufferSize > 0)
//		{
//			unsigned int samples = m_bufferSize / (m_waveFormat.nChannels * m_waveFormat.nBlockAlign);
//			unsigned long long adjustNS = AudioUtil::AudioFramesToNS(m_waveFormat.nSamplesPerSec, samples);
//			recTimeNS = recTimeNS - adjustNS;
//		}
//
//		m_bufferStartTS = recTimeNS;
//
//		memcpy(m_10msBuffer + m_bufferSize, pData, requiredSize);
//		m_bufferSize = m_bufferSize + requiredSize;
//
//		if (m_bufferSize >= k10msSampleSizeByte)
//		{
//			if (m_observer)
//			{
//				m_observer->OnAudioCaptureRecordData(
//					m_10msBuffer,
//					m_waveFormat.nSamplesPerSec,
//					m_waveFormat.nChannels,
//					m_waveFormat.wBitsPerSample,
//					k10msSamples,
//					m_bufferStartTS);
//			}
//
//			unsigned int remain = m_bufferSize - k10msSampleSizeByte;
//
//			if (remain > 0)
//			{
//				MoveMemory(m_10msBuffer, m_10msBuffer + k10msSampleSizeByte, remain);
//			}
//
//			m_bufferSize = remain;
//
//			m_bufferStartTS = m_bufferStartTS + AudioUtil::AudioFramesToNS(m_waveFormat.nSamplesPerSec, k10msSamples);
//		}
//
//		/*std::cout << "total samples = " << totalsamples << std::endl;
//		std::cout << "m_bufferSize = " << m_bufferSize / (m_waveFormat.nChannels * (m_waveFormat.wBitsPerSample / 8)) << std::endl;*/
//		
//		m_pAudioCapture->ReleaseBuffer(num_frames_to_read);
//	}
//
//	return true;
//}
bool CWasApiCapture::PushCaptureDataToObserver()
{
	BYTE *pData = NULL;
	HRESULT hr;
	UINT32 num_frames_in_next_packet = 0;

	static unsigned long long totalsamples = 0;

	int i = 0;
	while (true)
	{
		DWORD flags = 0;
		UINT32 num_frames_to_read = 0;
		UINT64 recPos = 0;
		UINT64 recTime = 0; //单位为 100纳秒，转毫秒 (recTime*100)/1000000

		hr = m_pAudioCapture->GetNextPacketSize(&num_frames_in_next_packet);
		if (FAILED(hr))
		{
			return false;
		}

		if (!num_frames_in_next_packet)
			return false;

		hr = m_pAudioCapture->GetBuffer(&pData, &num_frames_to_read, &flags, &recPos, &recTime);
		if (FAILED(hr))
		{
			return false;
		}

		unsigned long long recTimeNS = recTime * 100;

		unsigned int requiredSize = m_waveFormat.nChannels * (m_waveFormat.wBitsPerSample / 8) * num_frames_to_read;

		if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
		{
			if (m_silenceData.size() < requiredSize)
			{
				m_silenceData.resize(requiredSize);
			}
			pData = m_silenceData.data();
		}

		//std::cout << " samples = " << num_frames_to_read << std::endl;
		//static unsigned long long last = 0;
		//std::cout << " diff = " << recTimeNS - last << " rate = " << num_frames_to_read << std::endl;
		//last = recTimeNS;

		if (m_observer)
		{
			m_observer->OnAudioCaptureRecordData(
				pData,
				m_waveFormat.nSamplesPerSec,
				m_waveFormat.nChannels,
				m_waveFormat.wBitsPerSample,
				num_frames_to_read,
				recTimeNS);
		}

		m_pAudioCapture->ReleaseBuffer(num_frames_to_read);
	}

	return true;
}