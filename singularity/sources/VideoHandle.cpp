#include "VideoHandle.h"

VideoHandle::VideoHandle()
{

}

VideoHandle::~VideoHandle()
{

}

int VideoHandle::f_i_updateCameraCount()
{
    return 0;
}

HRESULT VideoHandle::f_o_initializeMediaFoundation()
{
    HRESULT hr = MFStartup(MF_VERSION);
    CHECK_HR(hr, "Failed to initialize Media Foundation");

    // Create an attribute store to specify capture settings
    hr = MFCreateAttributes(&pAttributes, 1);
    CHECK_HR(hr, "Failed to create attribute store");

    // Request video capture devices (such as webcams)
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    CHECK_HR(hr, "Failed to set video capture attribute");

    // Enumerate video capture devices
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
    CHECK_HR(hr, "Failed to enumerate video capture devices");

    if (deviceCount == 0)
    {
        std::cout << "No video capture devices found.\n";
        return E_FAIL;
    }


}

int VideoHandle::f_i_getNumberOfVideoDevices()
{
    HRESULT v_o_result = S_OK;
    v_o_result =  f_o_initializeMediaFoundation();
    if (v_o_result != S_OK)
    {
        f_o_deinitializeMediaFoundation();
        return 0;
    }

    return deviceCount;
    
}

HRESULT VideoHandle::f_o_deinitializeMediaFoundation()
{
    // Release the attributes and device enumerator resources
    for (UINT32 i = 0; i < deviceCount; i++)
    {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    pAttributes->Release();

    return S_OK;
}

HRESULT VideoHandle::f_o_activateAndCreateSourceReader(int i_i_deviceNumber)
{
    // Activate the first device to get an IMFMediaSource
   HRESULT hr = ppDevices[i_i_deviceNumber]->ActivateObject(IID_PPV_ARGS(&pSource));
    CHECK_HR(hr, "Failed to activate video capture device (IMFMediaSource)");

    // Create the source reader from the media source
    hr = MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &pReader);
    CHECK_HR(hr, "Failed to create source reader from media source");

}

std::string VideoHandle::f_s_getDeviceFriendlyName(int i_i_deviceNumber)
{
    WCHAR* deviceName = nullptr;
    UINT32 nameLength = 0;
    
    std::string v_s_deviceNAme;

    // Get the friendly name of the device
    HRESULT hr = ppDevices[i_i_deviceNumber]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &deviceName, &nameLength);
    if (SUCCEEDED(hr))
    {
        std::wstring ws(deviceName);
        v_s_deviceNAme = std::string(ws.begin(), ws.end());
    }
    CoTaskMemFree(deviceName);
    return v_s_deviceNAme;
}

HRESULT VideoHandle::f_o_captureVideoFrame()
{
    if (!pReader)
    {
        return E_POINTER;
    }

    IMFSample* pSample = nullptr;
    DWORD streamIndex, flags;
    LONGLONG llTimeStamp;

    HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
    CHECK_HR(hr, "Failed to read sample");

    if (pSample)
    {
        std::cout << "Captured a video frame!\n";
        pSample->Release();
    }

    return hr;
}




