#include "pch.h"
#include "MS_MediaFoundation.h"

MS_MediaFoundation::MS_MediaFoundation()
{    
}

MS_MediaFoundation::~MS_MediaFoundation()
{

}


int MS_MediaFoundation::f_i_initializeMediaFoundation()
{
    HRESULT hr = MFStartup(MF_VERSION);
    CHECK_HR(hr, (int)e_ErrorState::MF_STARTUP_ERR);

    // Create an attribute store to specify capture settings
    hr = MFCreateAttributes(&pAttributes, 1);
    CHECK_HR(hr, (int)e_ErrorState::MF_CREATE_ATTRIBUTES_ERR);

    // Request video capture devices (such as webcams)
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    CHECK_HR(hr, (int)e_ErrorState::MF_SET_GUID_ERR);

    // Enumerate video capture devices
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
    CHECK_HR(hr, (int)e_ErrorState::MF_ENUM_DEVICE_SOURCES_ERR);

    if (deviceCount == 0)
    {
        return (int)e_ErrorState::MF_NO_VIDEO_DEVICE_ERR;
    }
    return (int)e_ErrorState::NO_ERR;
}

int MS_MediaFoundation::f_i_deinitializeMediaFoundation()
{
    // Release the attributes and device enumerator resources
    for (UINT32 i = 0; i < deviceCount; i++)
    {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    pAttributes->Release();

    return e_ErrorState::NO_ERR;
}

void MS_MediaFoundation::f_v_releaseResources()
{
    if (pReader)
    {
        pReader->Release();
    }
    if (pSource)
    {
        pSource->Release();
    }

    MFShutdown();
}

int MS_MediaFoundation::f_i_activateAndCreateSourceReader(int i_i_deviceNumber)
{
    // Activate the first device to get an IMFMediaSource
   HRESULT hr = ppDevices[i_i_deviceNumber]->ActivateObject(IID_PPV_ARGS(&pSource));
   CHECK_HR(hr, (int)e_ErrorState::MF_ACTIVATE_DEVICE_ERR);

    // Create the source reader from the media source
    hr = MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &pReader);
    CHECK_HR(hr, (int)e_ErrorState::MF_SOURCE_READER_ERR);

    return (int)e_ErrorState::NO_ERR;
}

void MS_MediaFoundation::f_v_getDeviceFriendlyName()
{
    if (deviceCount == 0)
    {
        f_i_deinitializeMediaFoundation();
        return;
    }
    WCHAR* deviceName = nullptr;
    UINT32 nameLength = 0;
    
    std::string v_str_deviceNAme;
    for (UINT32 i = 0; i < deviceCount; ++i)
    {
        // Get the friendly name of the device
        HRESULT hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &deviceName, &nameLength);
        if (SUCCEEDED(hr))
        {
            std::wstring ws(deviceName);
            v_str_deviceNAme = std::string(ws.begin(), ws.end());
        }
        devicesNames += v_str_deviceNAme + ";";
    }        
}

int MS_MediaFoundation::f_i_captureVideoFrame()
{
    if (!pReader)
    {
        return e_ErrorState::MF_READER_PTR_ERR;
    }

    IMFSample* pSample = nullptr;
    DWORD streamIndex, flags;
    LONGLONG llTimeStamp;

    HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
    CHECK_HR(hr, (int)e_ErrorState::MF_SAMPLE_READ_ERR);

    if (pSample)
    {        //std::cout << "Captured a video frame!\n";

        pSample->Release();
    }

    return e_ErrorState::NO_ERR;
}


