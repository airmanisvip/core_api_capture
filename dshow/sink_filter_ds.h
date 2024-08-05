#pragma once

#include <dshow.h>
#include <streams.h>  // Include base DS filter header files

#include "video_capture_defines.h"

/*
编译如果报错，尝试更改如下编译选项
“工程属性”——“C/C++”——"符合模式"——勾选“否”
*/

class CaptureSinkFilter;

/**
 *     input pin for camera input
 *
 */
class CaptureInputPin : public CBaseInputPin 
{
 public:
  VideoCaptureCapability _requestedCapability;
  VideoCaptureCapability _resultingCapability;
  HANDLE _threadHandle;

  CaptureInputPin(IN TCHAR* szName,
                  IN CaptureSinkFilter* pFilter,
                  IN CCritSec* pLock,
                  OUT HRESULT* pHr,
                  IN LPCWSTR pszName);
  virtual ~CaptureInputPin();

  HRESULT GetMediaType(IN int iPos, OUT CMediaType* pmt);
  HRESULT CheckMediaType(IN const CMediaType* pmt);
  STDMETHODIMP Receive(IN IMediaSample*);
  HRESULT SetMatchingMediaType(const VideoCaptureCapability& capability);
};

class CaptureSinkFilter : public CBaseFilter
{
public:
  CaptureSinkFilter(const IN TCHAR* tszName,
                    IN LPUNKNOWN punk,
                    OUT HRESULT* phr,
                    VideoCaptureExternal& captureObserver);
  virtual ~CaptureSinkFilter();

  //  --------------------------------------------------------------------
  //  class methods

  void ProcessCapturedFrame(unsigned char* pBuffer,
                            size_t length,
                            const VideoCaptureCapability& frameInfo);
  //  explicit receiver lock aquisition and release
  void LockReceive() { m_crtRecv.Lock(); }
  void UnlockReceive() { m_crtRecv.Unlock(); }
  //  explicit filter lock aquisition and release
  void LockFilter() { m_crtFilter.Lock(); }
  void UnlockFilter() { m_crtFilter.Unlock(); }
  void SetFilterGraph(IGraphBuilder* graph);  // Used if EVR

  //  --------------------------------------------------------------------
  //  COM interfaces
  DECLARE_IUNKNOWN;
  STDMETHODIMP SetMatchingMediaType(const VideoCaptureCapability& capability);

  //  --------------------------------------------------------------------
  //  CBaseFilter methods
  int GetPinCount();
  CBasePin* GetPin(IN int Index);
  STDMETHODIMP Pause();
  STDMETHODIMP Stop();
  STDMETHODIMP GetClassID(OUT CLSID* pCLSID);
  //  --------------------------------------------------------------------
  //  class factory calls this
  static CUnknown* CreateInstance(IN LPUNKNOWN punk, OUT HRESULT* phr);

 private:
  CCritSec m_crtFilter;  //  filter lock
  CCritSec m_crtRecv;    //  receiver lock; always acquire before filter lock
  CaptureInputPin* m_pInput;
  VideoCaptureExternal& _captureObserver;
};
