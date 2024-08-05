
#include <initguid.h>  // Must come before the dshow_helps.h include so
// that DEFINE_GUID() entries will be defined in this
// object file.

#include "dshow_helps.h"

#include <cguid.h>


// This returns minimum :), which will give max frame rate...
LONGLONG GetMaxOfFrameArray(LONGLONG* maxFps, long size)
{
	LONGLONG maxFPS = maxFps[0];
	for (int i = 0; i < size; i++)
	{
		if (maxFPS > maxFps[i])
			maxFPS = maxFps[i];
	}
	return maxFPS;
}

BOOL PinMatchesCategory(IPin* pPin, REFGUID Category)
{
	BOOL bFound = FALSE;
	IKsPropertySet* pKs = NULL;
	HRESULT hr = pPin->QueryInterface(IID_PPV_ARGS(&pKs));
	if (SUCCEEDED(hr))
	{
		GUID PinCategory;
		DWORD cbReturned;
		hr = pKs->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &PinCategory, sizeof(GUID), &cbReturned);
		if (SUCCEEDED(hr) && (cbReturned == sizeof(GUID)))
		{
			bFound = (PinCategory == Category);
		}
		pKs->Release();
	}
	return bFound;
}
IPin* GetOutputPin(IBaseFilter* filter, REFGUID Category)
{
	HRESULT hr;
	IPin* pin = NULL;
	IEnumPins* pPinEnum = NULL;
	filter->EnumPins(&pPinEnum);
	if (pPinEnum == NULL)
	{
		return NULL;
	}
	// get first unconnected pin
	hr = pPinEnum->Reset();  // set to first pin
	while (S_OK == pPinEnum->Next(1, &pin, NULL))
	{
		PIN_DIRECTION pPinDir;
		pin->QueryDirection(&pPinDir);
		if (PINDIR_OUTPUT == pPinDir)  // This is an output pin
		{
			if (Category == GUID_NULL || PinMatchesCategory(pin, Category))
			{
				pPinEnum->Release();
				return pin;
			}
		}
		pin->Release();
		pin = NULL;
	}
	pPinEnum->Release();
	return NULL;
}
IPin* GetInputPin(IBaseFilter* filter)
{
	HRESULT hr;
	IPin* pin = NULL;
	IEnumPins* pPinEnum = NULL;
	filter->EnumPins(&pPinEnum);
	if (pPinEnum == NULL)
	{
		return NULL;
	}

	// get first unconnected pin
	hr = pPinEnum->Reset();  // set to first pin

	while (S_OK == pPinEnum->Next(1, &pin, NULL))
	{
		PIN_DIRECTION pPinDir;
		pin->QueryDirection(&pPinDir);
		if (PINDIR_INPUT == pPinDir)  // This is an input pin
		{
			IPin* tempPin = NULL;
			if (S_OK != pin->ConnectedTo(&tempPin))  // The pint is not connected
			{
				pPinEnum->Release();
				return pin;
			}
		}
		pin->Release();
	}
	pPinEnum->Release();
	return NULL;
}