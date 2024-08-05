#pragma once

#include <dshow.h>

#define    RELEASE_AND_CLEAR(p) \
	if(p)						\
	{							\
		(p)->Release();			\
		(p) = NULL;				\
	}							\

DEFINE_GUID(MEDIASUBTYPE_I420,
	0x30323449,
	0x0000,
	0x0010,
	0x80,
	0x00,
	0x00,
	0xAA,
	0x00,
	0x38,
	0x9B,
	0x71);
DEFINE_GUID(MEDIASUBTYPE_HDYC,
	0x43594448,
	0x0000,
	0x0010,
	0x80,
	0x00,
	0x00,
	0xAA,
	0x00,
	0x38,
	0x9B,
	0x71);

LONGLONG GetMaxOfFrameArray(LONGLONG* maxFps, long size);
BOOL PinMatchesCategory(IPin* pPin, REFGUID Category);
IPin* GetOutputPin(IBaseFilter* filter, REFGUID Category);
IPin* GetInputPin(IBaseFilter* filter);