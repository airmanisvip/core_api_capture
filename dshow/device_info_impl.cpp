#include "device_info_impl.h"

#include "video_capture_defines.h"
#include "dshow_helps.h"

#include <assert.h>

#include <dvdmedia.h>
#include <streams.h>

static const int kMaxDevNameLength = 1024;


DeviceInfoImpl *DeviceInfoImpl::Create()
{
	DeviceInfoImpl *dsInfo = new DeviceInfoImpl();
	if (!dsInfo || dsInfo->Init() != 0)
	{
		delete dsInfo;
		dsInfo = NULL;
	}

	return dsInfo;
}

DeviceInfoImpl::DeviceInfoImpl()
	: m_apiLock(*RWLockWrapper::Create()),
	m_lastUsedDeviceName(NULL),
	m_lastUsedDeviceNameLength(0),
	m_pDsDevEnum(NULL),
	m_pDsMonikerDevEnum(NULL),
	m_CoUninitializeIsRequired(true)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);  // Use COINIT_MULTITHREADED since Voice
									  // Engine uses COINIT_MULTITHREADED
	if (FAILED(hr))
	{
		// Avoid calling CoUninitialize() since CoInitializeEx() failed.
		m_CoUninitializeIsRequired = FALSE;

		if (hr == RPC_E_CHANGED_MODE)
		{
			// Calling thread has already initialized COM to be used in a
			// single-threaded apartment (STA). We are then prevented from using STA.
			// Details: hr = 0x80010106 <=> "Cannot change thread mode after it is
			// set".
			//
		}
	}
}

DeviceInfoImpl::~DeviceInfoImpl(void)
{
	m_apiLock.AcquireLockExclusive();

	free(m_lastUsedDeviceName);

	m_apiLock.ReleaseLockExclusive();

	delete &m_apiLock;

	RELEASE_AND_CLEAR(m_pDsMonikerDevEnum);
	RELEASE_AND_CLEAR(m_pDsDevEnum);
	if (m_CoUninitializeIsRequired)
	{
		CoUninitialize();
	}
}

int DeviceInfoImpl::Init()
{
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void**)&m_pDsDevEnum);
	if (hr != NOERROR)
	{
		return -1;
	}
	return 0;
}
unsigned int DeviceInfoImpl::NumberOfDevices()
{
	ReadLockScoped cs(m_apiLock);
	return getDeviceInfo(0, 0, 0, 0, 0, 0, 0);
}
int DeviceInfoImpl::GetDeviceName(unsigned int deviceNumber,
	char* deviceNameUTF8,
	unsigned int deviceNameLength,
	char* deviceUniqueIdUTF8,
	unsigned int deviceUniqueIdUTF8Length,
	char* productUniqueIdUTF8,
	unsigned int productUniqueIdUTF8Length)
{
	ReadLockScoped cs(m_apiLock);
	const int32_t result = getDeviceInfo(
		deviceNumber, deviceNameUTF8, deviceNameLength, deviceUniqueIdUTF8,
		deviceUniqueIdUTF8Length, productUniqueIdUTF8, productUniqueIdUTF8Length);
	return result > (int32_t)deviceNumber ? 0 : -1;
}
int DeviceInfoImpl::GetWindowsCapability(const int capabilityIndex, VideoCaptureCapabilityWindows &windowsCapability)
{
	ReadLockScoped cs(m_apiLock);

	if (capabilityIndex < 0 || static_cast<size_t>(capabilityIndex) >= m_captureCapabilityWinVec.size())
	{
		return -1;
	}

	windowsCapability = m_captureCapabilityWinVec[capabilityIndex];
	return 0;
}

int DeviceInfoImpl::NumberOfCapabilities(const char* deviceUniqueIdUTF8)
{
	if (!deviceUniqueIdUTF8)
		return -1;

	m_apiLock.AcquireLockShared();

	if (m_lastUsedDeviceNameLength == strlen((char*)deviceUniqueIdUTF8))
	{
		// Is it the same device that is asked for again.
		if (_strnicmp((char*)m_lastUsedDeviceName, (char*)deviceUniqueIdUTF8, m_lastUsedDeviceNameLength) == 0)
		{
			// yes
			m_apiLock.ReleaseLockShared();
			return static_cast<int>(m_captureCapabilityVec.size());
		}
	}

	// Need to get exclusive rights to create the new capability map.
	m_apiLock.ReleaseLockShared();

	WriteLockScoped cs2(m_apiLock);

	int ret = createCapabilityMap(deviceUniqueIdUTF8);

	return ret;
}

int DeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8, const unsigned int deviceCapabilityNumber, VideoCaptureCapability& capability)
{
	assert(deviceUniqueIdUTF8 != NULL);

	ReadLockScoped cs(m_apiLock);

	if ((m_lastUsedDeviceNameLength != strlen((char*)deviceUniqueIdUTF8)) || 
		(_strnicmp((char*)m_lastUsedDeviceName, (char*)deviceUniqueIdUTF8,
			m_lastUsedDeviceNameLength) != 0))
	{
		m_apiLock.ReleaseLockShared();
		m_apiLock.AcquireLockExclusive();

		if (-1 == createCapabilityMap(deviceUniqueIdUTF8))
		{
			m_apiLock.ReleaseLockExclusive();
			m_apiLock.AcquireLockShared();
			return -1;
		}
		m_apiLock.ReleaseLockExclusive();
		m_apiLock.AcquireLockShared();
	}

	// Make sure the number is valid
	if (deviceCapabilityNumber >= (unsigned int)m_captureCapabilityVec.size())
	{
		return -1;
	}

	capability = m_captureCapabilityVec[deviceCapabilityNumber];

	return 0;
}

int DeviceInfoImpl::GetBestMatchedCapability(const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested, VideoCaptureCapability& resulting)
{
	if (!deviceUniqueIdUTF8)
		return -1;

	ReadLockScoped cs(m_apiLock);
	if ((m_lastUsedDeviceNameLength != strlen((char*)deviceUniqueIdUTF8)) ||
		(_strnicmp((char*)m_lastUsedDeviceName, (char*)deviceUniqueIdUTF8,
			m_lastUsedDeviceNameLength) != 0))
	{
		m_apiLock.ReleaseLockShared();
		m_apiLock.AcquireLockExclusive();

		if (-1 == createCapabilityMap(deviceUniqueIdUTF8))
		{
			return -1;
		}
		m_apiLock.ReleaseLockExclusive();
		m_apiLock.AcquireLockShared();
	}

	int bestformatIndex = -1;
	int bestWidth = 0;
	int bestHeight = 0;
	int bestFrameRate = 0;
	VideoType bestVideoType = VideoType::kUnknown;

	const int numberOfCapabilies = static_cast<int>(m_captureCapabilityVec.size());

	for (int tmp = 0; tmp < numberOfCapabilies; ++tmp)  // Loop through all capabilities
	{
		VideoCaptureCapability& capability = m_captureCapabilityVec[tmp];

		const int diffWidth = capability.width - requested.width;
		const int diffHeight = capability.height - requested.height;
		const int diffFrameRate = capability.maxFPS - requested.maxFPS;

		const int currentbestDiffWith = bestWidth - requested.width;
		const int currentbestDiffHeight = bestHeight - requested.height;
		const int currentbestDiffFrameRate = bestFrameRate - requested.maxFPS;

		if ((diffHeight >= 0 && diffHeight <= abs(currentbestDiffHeight))  // Height better or equalt that previouse.
			|| (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight))
		{
			if (diffHeight == currentbestDiffHeight)  // Found best height. Care about the width)
			{
				// Width better or equal
				if ((diffWidth >= 0 && diffWidth <= abs(currentbestDiffWith)) || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith))
				{
					if (diffWidth == currentbestDiffWith && diffHeight == currentbestDiffHeight)  // Same size as previously
					{
						// Also check the best frame rate if the diff is the same as
						// previouse
						// Frame rate to high but Current frame rate is
						if (((diffFrameRate >= 0 && diffFrameRate <= currentbestDiffFrameRate) || 
							(currentbestDiffFrameRate < 0 && diffFrameRate >= currentbestDiffFrameRate)))
						{
							// Same frame rate as previous  or frame rate allready good enough
							if ((currentbestDiffFrameRate == diffFrameRate) || (currentbestDiffFrameRate >= 0))
							{
								if (bestVideoType != requested.videoType &&
									requested.videoType != VideoType::kUnknown &&
									(capability.videoType == requested.videoType ||
										capability.videoType == VideoType::kI420 ||
										capability.videoType == VideoType::kYUY2 ||
										capability.videoType == VideoType::kYV12))
								{
									bestVideoType = capability.videoType;
									bestformatIndex = tmp;
								}
								// If width height and frame rate is full filled we can use the
								// camera for encoding if it is supported.
								if (capability.height == requested.height &&
									capability.width == requested.width &&
									capability.maxFPS >= requested.maxFPS)
								{
									bestformatIndex = tmp;
								}
							}
							else  // Better frame rate
							{
								bestWidth = capability.width;
								bestHeight = capability.height;
								bestFrameRate = capability.maxFPS;
								bestVideoType = capability.videoType;
								bestformatIndex = tmp;
							}
						}
					}
					else  // Better width than previously
					{
						bestWidth = capability.width;
						bestHeight = capability.height;
						bestFrameRate = capability.maxFPS;
						bestVideoType = capability.videoType;
						bestformatIndex = tmp;
					}
				}     // else width no good
			}
			else  // Better height
			{
				bestWidth = capability.width;
				bestHeight = capability.height;
				bestFrameRate = capability.maxFPS;
				bestVideoType = capability.videoType;
				bestformatIndex = tmp;
			}
		}  // else height not good
	}    // end for

	// Copy the capability
	if (bestformatIndex < 0)
		return -1;

	resulting = m_captureCapabilityVec[bestformatIndex];

	return bestformatIndex;
}

// Default implementation. This should be overridden by Mobile implementations.
int DeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8, VideoRotation& orientation)
{
	orientation = kVideoRotation_0;
	return -1;
}

IBaseFilter *DeviceInfoImpl::GetDeviceFilter(const char *devUniqueIdUTF8, char *productUniqueIdUTF8, unsigned int productUniqueIdUTF8Len)
{
	const int uniqueIdLen = strlen(devUniqueIdUTF8);

	RELEASE_AND_CLEAR(m_pDsMonikerDevEnum);

	HRESULT hr = m_pDsDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &m_pDsMonikerDevEnum, 0);
	if (hr != 0)
	{
		return NULL;
	}

	m_pDsMonikerDevEnum->Reset();

	ULONG cFetched = 0;
	IMoniker *pM = NULL;

	IBaseFilter *captureFilter = NULL;
	bool deviceFound = false;
	while (S_OK == m_pDsMonikerDevEnum->Next(1, &pM, &cFetched) && !deviceFound)
	{
		IPropertyBag *pBag = NULL;
		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
		if (S_OK == hr)
		{
			//Find the decription or friendly name
			VARIANT varName;
			VariantInit(&varName);
			if (uniqueIdLen > 0)
			{
				hr = pBag->Read(L"DevicePath", &varName, 0);
				if (FAILED(hr))
				{
					hr = pBag->Read(L"Description", &varName, 0);
					if (FAILED(hr))
					{
						hr = pBag->Read(L"FriendlyName", &varName, 0);
					}
				}

				if (SUCCEEDED(hr))
				{
					char tempDevicePathUTF8[256] = { 0 };
					WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, tempDevicePathUTF8, sizeof(tempDevicePathUTF8), NULL, NULL);
					int len = strlen(tempDevicePathUTF8);
					if (strncmp(tempDevicePathUTF8, (const char *)devUniqueIdUTF8, uniqueIdLen) == 0)
					{
						deviceFound = true;
						hr = pM->BindToObject(0, 0, IID_IBaseFilter, (void **)&captureFilter);
						if (FAILED(hr))
						{
							printf("Failed to bind to the selected capture device");
						}

						if (productUniqueIdUTF8 && productUniqueIdUTF8Len > 0)
						{
							getProductId(devUniqueIdUTF8, productUniqueIdUTF8, productUniqueIdUTF8Len);
						}
					}
				}
			}

			VariantClear(&varName);
			pBag->Release();
			pM->Release();
		}
	}

	return captureFilter;
}

// Constructs a product ID from the Windows DevicePath. on a USB device the
// devicePath contains product id and vendor id. This seems to work for firewire
// as well.
// Example of device path:
// "\\?\usb#vid_0408&pid_2010&mi_00#7&258e7aaf&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\global"
// "\\?\avc#sony&dv-vcr&camcorder&dv#65b2d50301460008#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\global"
void DeviceInfoImpl::getProductId(const char* devicePath, char* productUniqueIdUTF8, unsigned int productUniqueIdUTF8Length)
{
	*productUniqueIdUTF8 = '\0';
	char* startPos = strstr((char*)devicePath, "\\\\?\\");
	if (!startPos)
	{
		strncpy_s((char*)productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
		printf("Failed to get the product Id");
		return;
	}
	startPos += 4;

	char* pos = strchr(startPos, '&');
	if (!pos || pos >= (char*)devicePath + strlen((char*)devicePath))
	{
		strncpy_s((char*)productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
		printf("Failed to get the product Id");
		return;
	}
	// Find the second occurrence.
	pos = strchr(pos + 1, '&');
	unsigned int bytesToCopy = (unsigned int)(pos - startPos);
	if (pos && (bytesToCopy <= productUniqueIdUTF8Length) && bytesToCopy <= kMaxDevNameLength)
	{
		strncpy_s((char*)productUniqueIdUTF8, productUniqueIdUTF8Length, (char*)startPos, bytesToCopy);
	}
	else
	{
		strncpy_s((char*)productUniqueIdUTF8, productUniqueIdUTF8Length, "", 1);
		printf("Failed to get the product Id");
	}
}

int DeviceInfoImpl::createCapabilityMap(const char *deviceUniqueIdUTF8)
{
	// Reset old capability list
	m_captureCapabilityVec.clear();
	m_captureCapabilityWinVec.clear();

	const int32_t deviceUniqueIdUTF8Length = (int32_t)strlen((char*)deviceUniqueIdUTF8);

	if (deviceUniqueIdUTF8Length > kMaxVideoDeviceUniqueNameLength)
	{
		return -1;
	}

	char productId[kMaxVideoDeviceProductLength];
	IBaseFilter* captureDevice = GetDeviceFilter(deviceUniqueIdUTF8, productId, kMaxVideoDeviceProductLength);
	if (!captureDevice)
		return -1;

	IPin* outputCapturePin = GetOutputPin(captureDevice, GUID_NULL);
	if (!outputCapturePin)
	{
		RELEASE_AND_CLEAR(captureDevice);
		return -1;
	}
	IAMExtDevice* extDevice = NULL;
	HRESULT hr = captureDevice->QueryInterface(IID_IAMExtDevice, (void**)&extDevice);
	if (SUCCEEDED(hr) && extDevice)
	{
		extDevice->Release();
	}

	IAMStreamConfig* streamConfig = NULL;
	hr = outputCapturePin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig);
	if (FAILED(hr))
	{
		return -1;
	}

	// this  gets the FPS
	IAMVideoControl* videoControlConfig = NULL;
	HRESULT hrVC = captureDevice->QueryInterface(IID_IAMVideoControl,
		(void**)&videoControlConfig);
	if (FAILED(hrVC))
	{
	}

	AM_MEDIA_TYPE* pmt = NULL;
	VIDEO_STREAM_CONFIG_CAPS caps;
	int count, size;

	hr = streamConfig->GetNumberOfCapabilities(&count, &size);
	if (FAILED(hr))
	{
		RELEASE_AND_CLEAR(videoControlConfig);
		RELEASE_AND_CLEAR(streamConfig);
		RELEASE_AND_CLEAR(outputCapturePin);
		RELEASE_AND_CLEAR(captureDevice);
		return -1;
	}

	// Check if the device support formattype == FORMAT_VideoInfo2 and
	// FORMAT_VideoInfo. Prefer FORMAT_VideoInfo since some cameras (ZureCam) has
	// been seen having problem with MJPEG and FORMAT_VideoInfo2 Interlace flag is
	// only supported in FORMAT_VideoInfo2
	bool supportFORMAT_VideoInfo2 = false;
	bool supportFORMAT_VideoInfo = false;
	bool foundInterlacedFormat = false;
	GUID preferedVideoFormat = FORMAT_VideoInfo;
	for (int32_t tmp = 0; tmp < count; ++tmp)
	{
		hr = streamConfig->GetStreamCaps(tmp, &pmt, reinterpret_cast<BYTE*>(&caps));
		if (!FAILED(hr))
		{
			if (pmt->majortype == MEDIATYPE_Video &&
				pmt->formattype == FORMAT_VideoInfo2) {

				supportFORMAT_VideoInfo2 = true;
				VIDEOINFOHEADER2* h = reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat);
				assert(h);
				foundInterlacedFormat |=
					h->dwInterlaceFlags &
					(AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOnly);
			}
			if (pmt->majortype == MEDIATYPE_Video && pmt->formattype == FORMAT_VideoInfo)
			{
				supportFORMAT_VideoInfo = true;
			}
		}
	}
	if (supportFORMAT_VideoInfo2)
	{
		if (supportFORMAT_VideoInfo && !foundInterlacedFormat)
		{
			preferedVideoFormat = FORMAT_VideoInfo;
		}
		else
		{
			preferedVideoFormat = FORMAT_VideoInfo2;
		}
	}

	for (int32_t tmp = 0; tmp < count; ++tmp)
	{
		hr = streamConfig->GetStreamCaps(tmp, &pmt, reinterpret_cast<BYTE*>(&caps));
		if (FAILED(hr))
		{
			RELEASE_AND_CLEAR(videoControlConfig);
			RELEASE_AND_CLEAR(streamConfig);
			RELEASE_AND_CLEAR(outputCapturePin);
			RELEASE_AND_CLEAR(captureDevice);
			return -1;
		}

		if (pmt->majortype == MEDIATYPE_Video &&
			pmt->formattype == preferedVideoFormat)
		{
			VideoCaptureCapabilityWindows capability;
			int64_t avgTimePerFrame = 0;

			if (pmt->formattype == FORMAT_VideoInfo) {
				VIDEOINFOHEADER* h = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
				assert(h);
				capability.directShowCapabilityIndex = tmp;
				capability.width = h->bmiHeader.biWidth;
				capability.height = h->bmiHeader.biHeight;
				avgTimePerFrame = h->AvgTimePerFrame;
			}
			if (pmt->formattype == FORMAT_VideoInfo2) {
				VIDEOINFOHEADER2* h = reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat);
				assert(h);
				capability.directShowCapabilityIndex = tmp;
				capability.width = h->bmiHeader.biWidth;
				capability.height = h->bmiHeader.biHeight;
				capability.interlaced =
					h->dwInterlaceFlags &
					(AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOnly);
				avgTimePerFrame = h->AvgTimePerFrame;
			}

			if (hrVC == S_OK)
			{
				LONGLONG* frameDurationList;
				LONGLONG maxFPS;
				long listSize;
				SIZE size;
				size.cx = capability.width;
				size.cy = capability.height;

				// GetMaxAvailableFrameRate doesn't return max frame rate always
				// eg: Logitech Notebook. This may be due to a bug in that API
				// because GetFrameRateList array is reversed in the above camera. So
				// a util method written. Can't assume the first value will return
				// the max fps.
				hrVC = videoControlConfig->GetFrameRateList(outputCapturePin, tmp, size, &listSize, &frameDurationList);

				// On some odd cameras, you may get a 0 for duration.
				// GetMaxOfFrameArray returns the lowest duration (highest FPS)
				if (hrVC == S_OK && listSize > 0 && 0 != (maxFPS = GetMaxOfFrameArray(frameDurationList, listSize)))
				{
					capability.maxFPS = static_cast<int>(10000000 / maxFPS);
					capability.supportFrameRateControl = true;
				}
				else  // use existing method
				{
					if (avgTimePerFrame > 0)
						capability.maxFPS = static_cast<int>(10000000 / avgTimePerFrame);
					else
						capability.maxFPS = 0;
				}
			}
			else  // use existing method in case IAMVideoControl is not supported
			{
				if (avgTimePerFrame > 0)
					capability.maxFPS = static_cast<int>(10000000 / avgTimePerFrame);
				else
					capability.maxFPS = 0;
			}

			// can't switch MEDIATYPE :~(
			if (pmt->subtype == MEDIASUBTYPE_I420)
			{
				capability.videoType = VideoType::kI420;
			}
			else if (pmt->subtype == MEDIASUBTYPE_IYUV)
			{
				capability.videoType = VideoType::kIYUV;
			}
			else if (pmt->subtype == MEDIASUBTYPE_RGB24)
			{
				capability.videoType = VideoType::kRGB24;
			}
			else if (pmt->subtype == MEDIASUBTYPE_YUY2)
			{
				capability.videoType = VideoType::kYUY2;
			}
			else if (pmt->subtype == MEDIASUBTYPE_RGB565)
			{
				capability.videoType = VideoType::kRGB565;
			}
			else if (pmt->subtype == MEDIASUBTYPE_MJPG)
			{
				capability.videoType = VideoType::kMJPEG;
			}
			else if (pmt->subtype == MEDIASUBTYPE_dvsl ||
				pmt->subtype == MEDIASUBTYPE_dvsd ||
				pmt->subtype == MEDIASUBTYPE_dvhd)  // If this is an external DV camera
			{
				capability.videoType = VideoType::kYUY2;  // MS DV filter seems to create this type
			}
			else if (pmt->subtype == MEDIASUBTYPE_UYVY)  // Seen used by Declink capture cards
			{
				capability.videoType = VideoType::kUYVY;
			}
			else if (pmt->subtype == MEDIASUBTYPE_HDYC)  // Seen used by Declink capture cards. Uses
									// BT. 709 color. Not entiry correct to use
									// UYVY. http://en.wikipedia.org/wiki/YCbCr
			{
				capability.videoType = VideoType::kUYVY;
			}
			else
			{
				WCHAR strGuid[39];
				StringFromGUID2(pmt->subtype, strGuid, 39);
				continue;
			}

			m_captureCapabilityVec.push_back(capability);
			m_captureCapabilityWinVec.push_back(capability);
		}
		DeleteMediaType(pmt);
		pmt = NULL;
	}
	RELEASE_AND_CLEAR(streamConfig);
	RELEASE_AND_CLEAR(videoControlConfig);
	RELEASE_AND_CLEAR(outputCapturePin);
	RELEASE_AND_CLEAR(captureDevice);  // Release the capture device

	// Store the new used device name
	m_lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
	m_lastUsedDeviceName = (char*)realloc(m_lastUsedDeviceName, m_lastUsedDeviceNameLength + 1);
	memcpy(m_lastUsedDeviceName, deviceUniqueIdUTF8, m_lastUsedDeviceNameLength + 1);

	return static_cast<int32_t>(m_captureCapabilityVec.size());
}
int DeviceInfoImpl::getDeviceInfo(unsigned int deviceNumber,
	char* deviceNameUTF8,
	unsigned int deviceNameLength,
	char* deviceUniqueIdUTF8,
	unsigned int deviceUniqueIdUTF8Length,
	char* productUniqueIdUTF8,
	unsigned int productUniqueIdUTF8Length)
{
	// enumerate all video capture devices
	RELEASE_AND_CLEAR(m_pDsMonikerDevEnum);

	HRESULT hr = m_pDsDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &m_pDsMonikerDevEnum, 0);
	if (hr != NOERROR)
	{
		return 0;
	}

	m_pDsMonikerDevEnum->Reset();
	ULONG cFetched;
	IMoniker* pM;
	int index = 0;
	while (S_OK == m_pDsMonikerDevEnum->Next(1, &pM, &cFetched))
	{
		IPropertyBag* pBag;
		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
		if (S_OK == hr)
		{
			// Find the description or friendly name.
			VARIANT varName;
			VariantInit(&varName);
			hr = pBag->Read(L"Description", &varName, 0);
			if (FAILED(hr)) {
				hr = pBag->Read(L"FriendlyName", &varName, 0);
			}
			if (SUCCEEDED(hr))
			{
				// ignore all VFW drivers
				if ((wcsstr(varName.bstrVal, (L"(VFW)")) == NULL) &&
					(_wcsnicmp(varName.bstrVal, (L"Google Camera Adapter"), 21) != 0)) {
					// Found a valid device.
					if (index == static_cast<int>(deviceNumber)) {
						int convResult = 0;
						if (deviceNameLength > 0) {
							convResult = WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1,
								(char*)deviceNameUTF8,
								deviceNameLength, NULL, NULL);
							if (convResult == 0)
							{
								return -1;
							}
						}
						if (deviceUniqueIdUTF8Length > 0) {
							hr = pBag->Read(L"DevicePath", &varName, 0);
							if (FAILED(hr)) {
								strncpy_s((char*)deviceUniqueIdUTF8, deviceUniqueIdUTF8Length,
									(char*)deviceNameUTF8, convResult);
							}
							else
							{
								convResult = WideCharToMultiByte(
									CP_UTF8, 0, varName.bstrVal, -1, (char*)deviceUniqueIdUTF8,
									deviceUniqueIdUTF8Length, NULL, NULL);
								if (convResult == 0)
								{
									return -1;
								}
								if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0)
								{
									getProductId(deviceUniqueIdUTF8, productUniqueIdUTF8, productUniqueIdUTF8Length);
								}
							}
						}
					}
					++index;  // increase the number of valid devices
				}
			}
			VariantClear(&varName);
			pBag->Release();
			pM->Release();
		}
	}
	if (deviceNameLength)
	{
	}
	return index;
}