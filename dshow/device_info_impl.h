#pragma once

#include "video_capture_defines.h"
#include "rw_lock.h"

#include <vector>
#include <dshow.h>

class DeviceInfoImpl
{
public:
	static DeviceInfoImpl *Create();

	DeviceInfoImpl();
	~DeviceInfoImpl();

	int Init();

	unsigned int NumberOfDevices();

	int GetDeviceName(unsigned int deviceNumber, char* deviceNameUTF8, unsigned int deviceNameLength,
		char* deviceUniqueIdUTF8, unsigned int deviceUniqueIdUTF8Length,
		char* productUniqueIdUTF8, unsigned int productUniqueIdUTF8Length);

	int GetWindowsCapability(const int capabilityIndex, VideoCaptureCapabilityWindows &windowsCapability);

	int NumberOfCapabilities(const char* deviceUniqueIdUTF8);

	int GetCapability(const char* deviceUniqueIdUTF8, const unsigned int deviceCapabilityNumber, VideoCaptureCapability& capability);

	int GetBestMatchedCapability(const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested, VideoCaptureCapability& resulting);
	
	int GetOrientation(const char* deviceUniqueIdUTF8, VideoRotation& orientation);

	IBaseFilter *GetDeviceFilter(const char *devUniqueIdUTF8, char *productUniqueIdUTF8 = NULL, unsigned int productUniqueIdUTF8Len = 0);

private:
	void getProductId(const char* devicePath, char* productUniqueIdUTF8, unsigned int productUniqueIdUTF8Length);

	int createCapabilityMap(const char *deviceUniqueIdUTF8);

	int getDeviceInfo(unsigned int deviceNumber,
		char* deviceNameUTF8,
		unsigned int deviceNameLength,
		char* deviceUniqueIdUTF8,
		unsigned int deviceUniqueIdUTF8Length,
		char* productUniqueIdUTF8,
		unsigned int productUniqueIdUTF8Length);

private:
	typedef std::vector<VideoCaptureCapability> VideoCaptureCapabiltyVector;
	typedef std::vector<VideoCaptureCapabilityWindows> VideoCaptureCapabiltyWinVector;

private:
	VideoCaptureCapabiltyVector m_captureCapabilityVec;
	VideoCaptureCapabiltyWinVector m_captureCapabilityWinVec;
	RWLockWrapper &m_apiLock;

private:
	char			*m_lastUsedDeviceName;
	unsigned int	m_lastUsedDeviceNameLength;

private:
	///////////////////////////////////////////
	//    Ã¶¾ÙÏà¹Ø
	///////////////////////////////////////////
	ICreateDevEnum	*m_pDsDevEnum;
	IEnumMoniker	*m_pDsMonikerDevEnum;

private:
	bool m_CoUninitializeIsRequired;
};