#include "stdafx.h"
#include "GuiMediaFusionController.h"
#include <iostream>



GuiMediaFusionController::GuiMediaFusionController()
{
	MF_View = new GuiMediaFusion();	
	MF_Model = new GuiMediaFusionModel();
	connectToView();
	connectToModel();
	MF_View->show();
}

GuiMediaFusionController::GuiMediaFusionController(int mainArgc, char* mainArgv[])
{
	argc = mainArgc;	
	argv = new char* [argc];
	// Copy each argument
	for (int i = 0; i < argc; ++i) {
		size_t length = strlen(mainArgv[i]) + 1; // Include space for the null terminator
		argv[i] = new char[length];   // Allocate memory for each string

		// Use strcpy_s to copy the string safely
		if (strcpy_s(argv[i], length, mainArgv[i]) != 0) {
			std::cerr << "Error copying argument " << i << std::endl;

			// Free memory allocated so far if an error occurs
			for (int j = 0; j <= i; ++j) {
				delete[] argv[j];
			}
			delete[] argv;			
		}
	}
	MF_View = new GuiMediaFusion();
	MF_Model = new GuiMediaFusionModel();
	connectToView();
	connectToModel();
	MF_View->show();


}

GuiMediaFusionController::~GuiMediaFusionController()
{
	// Free the allocated memory
	for (int i = 0; i < argc; ++i) {
		delete[] argv[i]; // Free each string
}
	delete[] argv; // Free the array of pointers
}

void GuiMediaFusionController::handleModelRequest(deviceProperties receivedProperties)
{
	QString emitterName = QObject::sender()->objectName();
	MF_View->setCombobox(GUI_ELEMENTS::SOURCES, receivedProperties.formattedDeviceCapabilities)
}

errorStateGui GuiMediaFusionController::connectToView()
{
	if (connect(MF_View, &GuiMediaFusion::viewClassRequest, this, &GuiMediaFusionController::handleViewRequest))
	{
		return errorStateGui::VIEW_CONNECT_ERR;
	}	
	return errorStateGui::NO_ERR;
}

errorStateGui GuiMediaFusionController::connectToModel()
{
	if (connect(MF_Model, &GuiMediaFusionModel::updateSourceDeviceInfo, this, &GuiMediaFusionController::handleModelRequest))
	{
		return errorStateGui::MODEL_CONNECT_ERR;
	}
	if (connect(MF_Model, &GuiMediaFusionModel::updateSinkDeviceInfo, this, &GuiMediaFusionController::handleModelRequest))
	{
		return errorStateGui::MODEL_CONNECT_ERR;
	}
	return errorStateGui::NO_ERR;
}



void GuiMediaFusionController::handleViewRequest(GUI_ELEMENTS requesterElement)
{
	switch (requesterElement)
	{
	case TURN_ON:
		MF_Model->initGstreamer(argc, argv);
		break;
	case INIT:
		break;
	case SOURCES:
		break;
	case SOURCE_CAPS:
		break;
	case SINKS:
		break;
	case SINK_CAPS:
		break;
	default:
		break;
	}
}

