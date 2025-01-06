#include "stdafx.h"
#include "GuiMediaFusion.h"
#include <QtWidgets/QApplication>
#include "MediaFusionGCV_API.h"
#include <iostream>

int main(int argc, char *argv[])
{
    mediaLib_GStreamerInit(argc, argv);
    int32_t objectID = mediaLib_create(SourceType::CAMERA_SOURCE, SCREEN_SINK, "pipeline");
    //deviceProperties* foundDevices = nullptr;
    //int32_t numberOfDevices = 0;
    //int32_t result = mediaLib_getDevices(objectID, numberOfDevices, &foundDevices);
    //if (result == 0 && foundDevices != nullptr)
    //{
    //    for (int i = 0; i < numberOfDevices; i++)
    //    {
    //        std::cout << "Device number " << i + 1 << ":" << std::endl;
    //        std::cout << foundDevices[i].deviceName << std::endl << foundDevices[i].formattedDeviceCapabilities << std::endl;
    //    }
    //}

    //mediaLib_setDevice(objectID, objectID, 0);
    mediaLib_startStreaming(objectID);
    while (true)
    {

    }


    
    return 0;
    //QApplication a(argc, argv);
    //GuiMediaFusion w;
    //w.show();
    //return a.exec();
}
