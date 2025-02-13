#include "stdafx.h"
#include "GuiMediaFusionModel.h"

#include <iostream>

GuiMediaFusionModel::GuiMediaFusionModel()
{
}

GuiMediaFusionModel::~GuiMediaFusionModel()
{
}

errorStateGui GuiMediaFusionModel::initGstreamer(int argc, char** argv)
{
	if (mediaLib_GStreamerInit(argc, argv) != errorState::NO_ERR)
	{

	}
	return errorStateGui::NO_ERR;
}

size_t GuiMediaFusionModel::createPipeline()
{	
	pipelineStash.push_back(new pipeLine);

	size_t stashSize = pipelineStash.size();
	//size_t piplineID = mediaLib_create();
	//pipelineStash[stashSize]->pipelineId = piplineID;

	
	return size_t();
}

errorStateGui GuiMediaFusionModel::getAvailableDevices(size_t piplineID)
{
	size_t devicesCount = 0;
	deviceProperties* devices = nullptr;
	if (mediaLib_getDevices(piplineID, devicesCount, &devices) == errorState::NO_ERR)
	{
		if (devices != nullptr)
		{
			for (int i = 0; i < devicesCount; i++)
			{ 
				std::cout << "Device number " << i + 1 << ":" << std::endl;
				//std::cout << foundDevices[i].deviceName << std::endl << foundDevices[i].formattedDeviceCapabilities << std::endl;
			}
		}
	}
	return errorStateGui::GET_AVAILABLE_DEVICES_ERR;
}
errorStateGui GuiMediaFusionModel::updatePipelineInfo(QString incomingInformation)
{
	incomingInformation.trimmed();
	//if (!incomingInformation.isEmpty())
	//{
	//	QStringList  incomingInformation.split(";", Qt::SkipEmptyParts);

	//}
	return errorStateGui::NO_ERR;	
}