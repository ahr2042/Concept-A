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

IMFSourceReader* pReader = nullptr;
IMFMediaSource* pSource = nullptr;

// Function to initialize Media Foundation and set up video capture
HRESULT InitializeMediaFoundation()
{
    HRESULT hr = MFStartup(MF_VERSION);
    CHECK_HR(hr, "Failed to initialize Media Foundation");

    IMFAttributes* pAttributes = nullptr;

    // Create an attribute store to specify capture settings
    hr = MFCreateAttributes(&pAttributes, 1);
    CHECK_HR(hr, "Failed to create attribute store");

    // Request video capture devices (such as webcams)
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    CHECK_HR(hr, "Failed to set video capture attribute");

    // Enumerate video capture devices
    IMFActivate** ppDevices = nullptr;
    UINT32 deviceCount = 0;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
    CHECK_HR(hr, "Failed to enumerate video capture devices");

    if (deviceCount == 0)
    {
        std::cout << "No video capture devices found.\n";
        return E_FAIL;
    }

    for (UINT32 i = 0; i < deviceCount; ++i)
    {
        WCHAR* deviceName = nullptr;
        UINT32 nameLength = 0;

        // Get the friendly name of the device
        hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &deviceName, &nameLength);
        if (SUCCEEDED(hr))
        {
            std::wstring ws(deviceName);
            std::string v_s_deviceNAme = std::string(ws.begin(), ws.end());
            // Convert from WCHAR to standard string for display
            std::cout << "Device " << i << ": " << v_s_deviceNAme << std::endl;
            std::wcout << L"Device " << i << L": " << deviceName << std::endl;
        }
    }
    // Activate the first device to get an IMFMediaSource
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    CHECK_HR(hr, "Failed to activate video capture device (IMFMediaSource)");

    // Create the source reader from the media source
    hr = MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &pReader);
    CHECK_HR(hr, "Failed to create source reader from media source");

    // Release the attributes and device enumerator resources
    for (UINT32 i = 0; i < deviceCount; i++)
    {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    pAttributes->Release();

    return S_OK;
}

// Function to capture a video frame
HRESULT CaptureVideoFrame()
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

int main()
{
    HRESULT hr = InitializeMediaFoundation();
    if (FAILED(hr))
    {
        std::cout << "Failed to initialize Media Foundation.\n";
        return -1;
    }

    std::cout << "Starting video capture...\n";

    // Capture 100 frames as an example
    //for (int i = 0; i < 100; ++i)
    //{
    //    hr = CaptureVideoFrame();
    //    if (FAILED(hr))
    //    {
    //        std::cout << "Failed to capture video frame.\n";
    //        break;
    //    }
    //    Sleep(30); // Simulate frame rate (30 fps)
    //}

    if (pReader)
    {
        pReader->Release();
    }
    if (pSource)
    {
        pSource->Release();
    }

    MFShutdown();

    return 0;
}
