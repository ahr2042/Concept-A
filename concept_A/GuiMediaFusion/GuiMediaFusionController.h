#pragma once

#include "GuiMediaFusion.h"
#include "GuiMediaFusionModel.h"

class GuiMediaFusionController : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionController();
	GuiMediaFusionController(int mainArgc, char* mainArgv[]);
	~GuiMediaFusionController();

public slots:
	void handleViewRequest(GUI_ELEMENTS);
	void handleModelRequest(deviceProperties);


private:
	GuiMediaFusion* MF_View = nullptr;
	GuiMediaFusionModel* MF_Model = nullptr;

	errorStateGui connectToView();
	errorStateGui connectToModel();

	void InitialGuiConfiguration();

	int argc = 0;
	char** argv;

};