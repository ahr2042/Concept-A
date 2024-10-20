#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mferror.h>
#include <iostream>

// Helper macro for checking HRESULTs
#define CHECK_HR(hr, msg) if (FAILED(hr)) { std::cout << msg << "\n"; return hr; }

class VideoHandle 
{
public:
	VideoHandle();
	~VideoHandle();

	int f_i_updateCameraCount();

private:
	HRESULT f_o_initializeMediaFoundation();
	int f_i_getNumberOfVideoDevices();
	HRESULT f_o_deinitializeMediaFoundation();
	HRESULT f_o_activateAndCreateSourceReader(int i_i_deviceNumber);
	std::string f_s_getDeviceFriendlyName(int i_i_deviceNumber);
	HRESULT f_o_captureVideoFrame();

	IMFActivate** ppDevices = nullptr;
	UINT32 deviceCount = 0;
	IMFSourceReader* pReader = nullptr;
	IMFMediaSource* pSource = nullptr;
	IMFAttributes* pAttributes = nullptr;


};