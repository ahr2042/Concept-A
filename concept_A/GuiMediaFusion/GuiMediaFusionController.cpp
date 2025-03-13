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
		size_t length = strlen(mainArgv[i]) + 1; 
		argv[i] = new char[length];   
		#ifdef _WIN32
		if (strcpy_s(argv[i], length, mainArgv[i]) != 0) {
		#else
		if (strcpy(argv[i], mainArgv[i]) == nullptr) {
		#endif
			std::cerr << "Error copying argument " << i << std::endl;

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
	InitialGuiConfiguration();
	MF_View->show();		
}


void GuiMediaFusionController::InitialGuiConfiguration()
{
	QStringList sourcesOrSkinks;
	sourcesOrSkinks.reserve(sizeof(sourcesTypes) / sizeof(sourcesTypes[0]));
	for (const auto& value : sourcesTypes) {
		sourcesOrSkinks.append(value);
	}
	MF_View->setCombobox(GUI_ELEMENTS::SOURCES, sourcesOrSkinks);

	sourcesOrSkinks.clear();
	sourcesOrSkinks.reserve(sizeof(sinksTypes) / sizeof(sinksTypes[0]));
	for (const auto& value : sinksTypes) {
		sourcesOrSkinks.append(value);
	}
	MF_View->setCombobox(GUI_ELEMENTS::SINKS, sourcesOrSkinks);
}

GuiMediaFusionController::~GuiMediaFusionController()
{
	// Free the allocated memory
	for (int i = 0; i < argc; ++i) {
		delete[] argv[i]; // Free each string
}
	delete[] argv; // Free the array of pointers
}

void GuiMediaFusionController::handleModelRequest(GUI_ELEMENTS targetElement, QStringList value)
{	
	MF_View->setCombobox(targetElement, value);
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
	if (connect(MF_Model, &GuiMediaFusionModel::updateInfo, this, &GuiMediaFusionController::handleModelRequest))
	{
		return errorStateGui::MODEL_CONNECT_ERR;
	}
	return errorStateGui::NO_ERR;
}



void GuiMediaFusionController::handleViewRequest(GUI_ELEMENTS requesterElement, QVariant optional)
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

