#pragma once

#include <dshow.h>
#include <mutex>

#include "video_capture_defines.h"
#include "device_info_impl.h"

const int kDefaultVideoW = 1270;
const int kDefaultVideoH = 720;
const int kDefaultVideoFPS = 15;

class VideoCaptureObserver
{
public:
	virtual void OnVideoFrame420(const unsigned char *data, int strideY, int strideU, int strideV, int width, int height, unsigned long long timestampNS) = 0;
};

class CaptureSinkFilter;

class VideoCapture : public VideoCaptureExternal
{
public:
	VideoCapture();
	virtual ~VideoCapture();

	static unsigned int EnumAllDevices();

public:
	int Init(const char *deviceUniqueIdUtf8);

	void RegisterCaptureObserver(VideoCaptureObserver *observer);
	void UnRegisterCaptureObserver();

	int StartCapture(const VideoCaptureCapability &capability);
	int StopCapture();

	const char *CurrentDeviceName();

	bool IsCaptureStarted();

	int SetCameraOutput(const VideoCaptureCapability& requestedCapability);

	//
	// VideoCaptureExternal
	//
	virtual int IncomingFrame(unsigned char *videoFrame, size_t videoFrameLength, const VideoCaptureCapability& frameInfo, long long captureTime = 0);

	int DisConnectGraph();

private:
	HRESULT connectDVCamera();

private:
	DeviceInfoImpl m_dsInfo;

private:
	IBaseFilter     *m_pCaptureFilter;

	IGraphBuilder	*m_pGraphBuilder;

	IMediaControl	*m_pMediaControl;

	IPin			*m_pOutPutPin;
	IPin			*m_pInPutPin;

	CaptureSinkFilter	*m_pRecvSinkFilter;

private:
	IBaseFilter		*m_pDvFilter;
	IPin			*m_pInputDvPin;
	IPin			*m_pOutputDvPin;

private:
	VideoCaptureCapability	m_requestedCapability;

private:
	char *m_pDeviceUniqueID;

private:
	std::mutex m_mutex;

private:
	VideoCaptureObserver	*m_observer;
};