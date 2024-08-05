#include "video_capture_dshow.h"

#include "dshow_helps.h"
#include "sink_filter_ds.h"
#include "libyuv.h"
#include "../platform.h"

#include <new>
#include <string.h>
#include <dvdmedia.h>  // VIDEOINFOHEADER2

#define kSinkFilterName L"SinkFilter"

VideoCapture::VideoCapture()
	:m_pDeviceUniqueID(NULL),
	m_pCaptureFilter(NULL),
	m_pGraphBuilder(NULL),
	m_pMediaControl(NULL),
	m_pOutPutPin(NULL),
	m_pInPutPin(NULL),
	m_pRecvSinkFilter(NULL),
	m_pDvFilter(NULL),
	m_pInputDvPin(NULL),
	m_pOutputDvPin(NULL),
	m_observer(NULL)
{
	m_requestedCapability.width = 1280;
	m_requestedCapability.height = 720;
	m_requestedCapability.maxFPS = 15;
	//m_requestedCapability.videoType = VideoType::kI420;
}
VideoCapture::~VideoCapture()
{
	delete[] m_pDeviceUniqueID;
	m_pDeviceUniqueID = nullptr;

	if (m_pMediaControl)
	{
		m_pMediaControl->Stop();
	}
	if (m_pGraphBuilder)
	{
		if (m_pRecvSinkFilter)
			m_pGraphBuilder->RemoveFilter(m_pRecvSinkFilter);

		if (m_pCaptureFilter)
			m_pGraphBuilder->RemoveFilter(m_pCaptureFilter);

		if (m_pDvFilter)
			m_pGraphBuilder->RemoveFilter(m_pDvFilter);
	}
	RELEASE_AND_CLEAR(m_pInPutPin);
	RELEASE_AND_CLEAR(m_pOutPutPin);

	RELEASE_AND_CLEAR(m_pCaptureFilter);  // release the capture device
	RELEASE_AND_CLEAR(m_pRecvSinkFilter);
	RELEASE_AND_CLEAR(m_pDvFilter);

	RELEASE_AND_CLEAR(m_pMediaControl);

	RELEASE_AND_CLEAR(m_pInputDvPin);
	RELEASE_AND_CLEAR(m_pOutputDvPin);

	RELEASE_AND_CLEAR(m_pGraphBuilder);
}
unsigned int VideoCapture::EnumAllDevices()
{
	DeviceInfoImpl dsInfo;
	dsInfo.Init();

	unsigned int num = dsInfo.NumberOfDevices();

	for (unsigned int i = 0; i < num; i++)
	{
		char szName[1024] = { 0 };
		char szUniqueId[1024] = { 0 };

		dsInfo.GetDeviceName(i, szName, 1024, szUniqueId, 1024, 0, 0);
		int len = strlen(szUniqueId);

		printf("name = %s, unique id = %s\n", szName, szUniqueId);
	}

	return num;
}
int VideoCapture::Init(const char *deviceUniqueIdUtf8)
{
	int len = strlen(deviceUniqueIdUtf8);
	m_pDeviceUniqueID = new (std::nothrow)char[len + 1];

	memcpy(m_pDeviceUniqueID, deviceUniqueIdUtf8, len + 1);

	if (m_dsInfo.Init() != 0)
	{
		return -1;
	}

	m_pCaptureFilter = m_dsInfo.GetDeviceFilter(deviceUniqueIdUtf8);
	if (!m_pCaptureFilter)
	{
		return -1;
	}

	// Get the interface for DShow's GraphBuilder
	HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&m_pGraphBuilder);
	if (FAILED(hr))
	{
		return -1;
	}

	hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void **)&m_pMediaControl);
	if (FAILED(hr))
	{
		return -1;
	}

	hr = m_pGraphBuilder->AddFilter(m_pCaptureFilter, L"VideoCaptureFilter");
	if (FAILED(hr))
	{
		return -1;
	}

	m_pOutPutPin = GetOutputPin(m_pCaptureFilter, PIN_CATEGORY_CAPTURE);

	m_pRecvSinkFilter = new CaptureSinkFilter(kSinkFilterName, NULL, &hr, *this);
	m_pRecvSinkFilter->AddRef();

	hr = m_pGraphBuilder->AddFilter(m_pRecvSinkFilter, kSinkFilterName);

	m_pInPutPin = GetInputPin(m_pRecvSinkFilter);

	// Temporary connect here.
	// This is done so that no one else can use the capture device.
	if (SetCameraOutput(m_requestedCapability) != 0)
	{
		return -1;
	}

	hr = m_pMediaControl->Pause();
	if (FAILED(hr))
	{
		return -1;
	}

	return 0;
}
void VideoCapture::RegisterCaptureObserver(VideoCaptureObserver *observer)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_observer = observer;
}
void VideoCapture::UnRegisterCaptureObserver()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_observer = NULL;
}

int VideoCapture::StartCapture(const VideoCaptureCapability &capability)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (capability != m_requestedCapability)
	{
		DisConnectGraph();

		if (SetCameraOutput(capability) != 0)
		{
			return -1;
		}
	}
	HRESULT hr = m_pMediaControl->Run();
	if (FAILED(hr))
	{
		return -1;
	}

	return 0;
}
int VideoCapture::StopCapture()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HRESULT hr = m_pMediaControl->Pause();
	if (FAILED(hr))
	{
		return -1;
	}
	return 0;
}
const char *VideoCapture::CurrentDeviceName()
{
	return m_pDeviceUniqueID;
}

int ConvertVideoType(VideoType video_type) {
	switch (video_type) {
	case VideoType::kUnknown:
		return libyuv::FOURCC_ANY;
	case VideoType::kI420:
		return libyuv::FOURCC_I420;
	case VideoType::kIYUV:  // same as VideoType::kYV12
	case VideoType::kYV12:
		return libyuv::FOURCC_YV12;
	case VideoType::kRGB24:
		return libyuv::FOURCC_24BG;
	case VideoType::kABGR:
		return libyuv::FOURCC_ABGR;
	case VideoType::kRGB565:
		return libyuv::FOURCC_RGBP;
	case VideoType::kYUY2:
		return libyuv::FOURCC_YUY2;
	case VideoType::kUYVY:
		return libyuv::FOURCC_UYVY;
	case VideoType::kMJPEG:
		return libyuv::FOURCC_MJPG;
	case VideoType::kNV21:
		return libyuv::FOURCC_NV21;
	case VideoType::kNV12:
		return libyuv::FOURCC_NV12;
	case VideoType::kARGB:
		return libyuv::FOURCC_ARGB;
	case VideoType::kBGRA:
		return libyuv::FOURCC_BGRA;
	case VideoType::kARGB4444:
		return libyuv::FOURCC_R444;
	case VideoType::kARGB1555:
		return libyuv::FOURCC_RGBO;
	}

	return libyuv::FOURCC_ANY;
}

int VideoCapture::IncomingFrame(unsigned char *videoFrame, size_t videoFrameLength, const VideoCaptureCapability& frameInfo, long long captureTime/* = 0*/)
{
	//原始数据位MJPEG格式
	std::lock_guard<std::mutex> lock(m_mutex);

	int width = frameInfo.width;
	int height = frameInfo.height;

	int stride_y = frameInfo.width;
	int stride_uv = (frameInfo.width + 1) / 2;
	int target_width = frameInfo.width;
	int target_height = frameInfo.height;

	if (frameInfo.videoType != VideoType::kMJPEG)
	{
		return -1;
	}

	unsigned char *yuvBuffer = new unsigned char[stride_y * height + (stride_uv + stride_uv) * ((height + 1) / 2)];
	unsigned char *uBuffer = yuvBuffer + stride_y * height;
	unsigned char *vBuffer = uBuffer + stride_uv * (height + 1) / 2;

	const int nRet = libyuv::ConvertToI420(videoFrame, videoFrameLength, yuvBuffer, stride_y, 
		uBuffer, stride_uv, 
		vBuffer, stride_uv, 
		0, 0, width, height, 
		target_width, target_height, 
		libyuv::kRotate0, ConvertVideoType(frameInfo.videoType));

	if (m_observer)
	{
		m_observer->OnVideoFrame420(yuvBuffer, stride_y, stride_uv, stride_uv, width, height, os_gettime_ns());
	}

	delete[] yuvBuffer;


	return 0;
}

int VideoCapture::DisConnectGraph()
{
	HRESULT hr = m_pMediaControl->Stop();
	hr += m_pGraphBuilder->Disconnect(m_pOutPutPin);
	hr += m_pGraphBuilder->Disconnect(m_pInPutPin);

	// if the DV camera filter exist
	if (m_pDvFilter)
	{
		m_pGraphBuilder->Disconnect(m_pInputDvPin);
		m_pGraphBuilder->Disconnect(m_pOutputDvPin);
	}
	if (hr != S_OK)
	{
		return -1;
	}
	return 0;
}

bool VideoCapture::IsCaptureStarted()
{
	OAFilterState state = 0;
	HRESULT hr = m_pMediaControl->GetState(1000, &state);
	if (hr != S_OK && hr != VFW_S_CANT_CUE)
	{
	}

	return state == State_Running;
}

int VideoCapture::SetCameraOutput(const VideoCaptureCapability& requestedCapability)
{
	// Get the best matching capability
	VideoCaptureCapability capability;
	int capabilityIndex;

	// Store the new requested size
	m_requestedCapability = requestedCapability;

	// Match the requested capability with the supported.
	if ((capabilityIndex = m_dsInfo.GetBestMatchedCapability(m_pDeviceUniqueID, m_requestedCapability, capability)) < 0)
	{
		return -1;
	}

	// Reduce the frame rate if possible.
	if (capability.maxFPS > requestedCapability.maxFPS)
	{
		capability.maxFPS = requestedCapability.maxFPS;
	}
	else if (capability.maxFPS <= 0)
	{
		capability.maxFPS = 30;
	}

	// Convert it to the windows capability index since they are not nexessary
	// the same
	VideoCaptureCapabilityWindows windowsCapability;
	if (m_dsInfo.GetWindowsCapability(capabilityIndex, windowsCapability) != 0)
	{
		return -1;
	}

	IAMStreamConfig* streamConfig = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	VIDEO_STREAM_CONFIG_CAPS caps;

	HRESULT hr = m_pOutPutPin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig);
	if (hr)
	{
		return -1;
	}

	// Get the windows capability from the capture device
	bool isDVCamera = false;
	hr = streamConfig->GetStreamCaps(windowsCapability.directShowCapabilityIndex, &pmt, reinterpret_cast<BYTE*>(&caps));
	if (!FAILED(hr))
	{
		if (pmt->formattype == FORMAT_VideoInfo2)
		{
			VIDEOINFOHEADER2* h = reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat);
			if (capability.maxFPS > 0 && windowsCapability.supportFrameRateControl)
			{
				h->AvgTimePerFrame = REFERENCE_TIME(10000000.0 / capability.maxFPS);
			}
		}
		else
		{
			VIDEOINFOHEADER* h = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
			if (capability.maxFPS > 0 && windowsCapability.supportFrameRateControl) {
				h->AvgTimePerFrame = REFERENCE_TIME(10000000.0 / capability.maxFPS);
			}
		}

		// Set the sink filter to request this capability
		m_pRecvSinkFilter->SetMatchingMediaType(capability);

		// Order the capture device to use this capability
		hr += streamConfig->SetFormat(pmt);

		// Check if this is a DV camera and we need to add MS DV Filter
		if (pmt->subtype == MEDIASUBTYPE_dvsl ||
			pmt->subtype == MEDIASUBTYPE_dvsd || pmt->subtype == MEDIASUBTYPE_dvhd)
			isDVCamera = true;  // This is a DV camera. Use MS DV filter
	}
	RELEASE_AND_CLEAR(streamConfig);

	if (FAILED(hr))
	{
		return -1;
	}

	if (isDVCamera)
	{
		hr = connectDVCamera();
	}
	else
	{
		hr = m_pGraphBuilder->ConnectDirect(m_pOutPutPin, m_pInPutPin, NULL);
	}
	if (hr != S_OK)
	{
		return -1;
	}
	return 0;
}
HRESULT VideoCapture::connectDVCamera()
{
	HRESULT hr = S_OK;

	if (!m_pDvFilter)
	{
		hr = CoCreateInstance(CLSID_DVVideoCodec, NULL, CLSCTX_INPROC, IID_IBaseFilter, (void**)&m_pDvFilter);
		if (hr != S_OK)
		{
			return hr;
		}
		hr = m_pGraphBuilder->AddFilter(m_pDvFilter, L"VideoDecoderDV");
		if (hr != S_OK)
		{
			return hr;
		}
		m_pInputDvPin = GetInputPin(m_pDvFilter);
		if (m_pInputDvPin == NULL)
		{
			return -1;
		}
		m_pOutputDvPin = GetOutputPin(m_pDvFilter, GUID_NULL);
		if (m_pOutputDvPin == NULL)
		{
			return -1;
		}
	}
	hr = m_pGraphBuilder->ConnectDirect(m_pOutPutPin, m_pInputDvPin, NULL);
	if (hr != S_OK)
	{
		return hr;
	}

	hr = m_pGraphBuilder->ConnectDirect(m_pOutputDvPin, m_pInPutPin, NULL);
	if (hr != S_OK)
	{
		if (hr == HRESULT_FROM_WIN32(ERROR_TOO_MANY_OPEN_FILES))
		{
		}
		else
		{
		}
		return hr;
	}
	return hr;
}