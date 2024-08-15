#pragma once

#include <string.h>

enum
{
	kMaxVideoDeviceUniqueNameLength = 1024
};
enum
{
	kMaxVideoDeviceNameLength = 256
};
enum
{
	kMaxVideoDeviceProductLength = 128
};

enum VideoRotation {
	kVideoRotation_0 = 0,
	kVideoRotation_90 = 90,
	kVideoRotation_180 = 180,
	kVideoRotation_270 = 270
};

enum class VideoType {
	kUnknown,
	kI420,
	kIYUV,
	kRGB24,
	kABGR,
	kARGB,
	kARGB4444,
	kRGB565,
	kARGB1555,
	kYUY2,
	kYV12,
	kUYVY,
	kMJPEG,
	kNV21,
	kNV12,
	kBGRA,
};

struct VideoCaptureCapability
{
	int width;
	int height;
	int maxFPS;
	VideoType videoType;
	bool interlaced;

	VideoCaptureCapability()
	{
		width = 0;
		height = 0;
		maxFPS = 0;
		videoType = VideoType::kUnknown;
		interlaced = false;
	};
	bool operator!=(const VideoCaptureCapability& other) const
	{
		if (width != other.width)
			return true;
		if (height != other.height)
			return true;
		if (maxFPS != other.maxFPS)
			return true;
		if (videoType != other.videoType)
			return true;
		if (interlaced != other.interlaced)
			return true;
		return false;
	}
	bool operator==(const VideoCaptureCapability& other) const 
	{
		return !operator!=(other);
	}
};
struct VideoCaptureCapabilityWindows : public VideoCaptureCapability
{
	unsigned int directShowCapabilityIndex;
	bool supportFrameRateControl;

	VideoCaptureCapabilityWindows()
	{
		directShowCapabilityIndex = 0;
		supportFrameRateControl = false;
	}
};

/* External Capture interface. Returned by Create
 and implemented by the capture module.
 */
class VideoCaptureExternal 
{
public:
	// |capture_time| must be specified in the NTP time format in milliseconds.
	virtual int IncomingFrame(unsigned char *videoFrame,
		size_t videoFrameLength,
		const VideoCaptureCapability& frameInfo,
		long long captureTime = 0) = 0;

protected:
	~VideoCaptureExternal() {}
};

struct VideoFrame
{
	const unsigned char *data;
	unsigned int lineSizeY;
	unsigned int lineSizeU;
	unsigned int lineSizeV;
	unsigned int width;
	unsigned int height;
	unsigned long long timeStamp;

	VideoFrame()
	{
		data = NULL;
		lineSizeY = 0;
		lineSizeU = 0;
		lineSizeV = 0;
		timeStamp = 0;

		count = 0;
		skipped = 0;
	}

	int skipped;
	int count;
};
struct VideoFrameCache
{
	VideoFrameCache()
	{
		emptyFrames = kVideoCacheFrameCount;
		readIndex = 0;
		writeIndex = 0;
	}
	unsigned int emptyFrames;
	unsigned int readIndex;
	unsigned int writeIndex;

	enum
	{
		kVideoCacheFrameCount = 6
	};
	struct VideoFrame cache[kVideoCacheFrameCount];
};