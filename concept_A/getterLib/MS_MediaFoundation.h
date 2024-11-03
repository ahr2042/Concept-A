#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mferror.h>
#include <iostream>

#include "e_ErrorState.h"

// Helper macro for checking HRESULTs
#define CHECK_HR(hr, errorCode) if (FAILED(hr)) { return errorCode; }

class MS_MediaFoundation 
{
public:
	MS_MediaFoundation();
	~MS_MediaFoundation();

	int f_i_initializeMediaFoundation();
	int f_i_activateAndCreateSourceReader(int i_i_deviceNumber);
	void f_v_getDeviceFriendlyName();
	int f_i_captureVideoFrame();
	int f_i_deinitializeMediaFoundation();
	void f_v_releaseResources();

	
	std::string devicesNames = nullptr;
	UINT32 deviceCount = 0;

private:

	IMFActivate** ppDevices = nullptr;	
	IMFSourceReader* pReader = nullptr;
	IMFMediaSource* pSource = nullptr;
	IMFAttributes* pAttributes = nullptr;
};