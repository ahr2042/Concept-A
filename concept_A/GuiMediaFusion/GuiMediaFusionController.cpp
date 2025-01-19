#include "stdafx.h"
#include "GuiMediaFusionController.h"



GuiMediaFusionController::GuiMediaFusionController()
{
	MF_View = new GuiMediaFusion();	
	MF_Model = new GuiMediaFusionModel();
	connectToView();
	connectToModel();
	MF_View->show();
}

GuiMediaFusionController::~GuiMediaFusionController()
{
}

errorStateGui GuiMediaFusionController::connectToView()
{
	if (connect(MF_View, &GuiMediaFusion::guiManagementViewRequest, this, &GuiMediaFusionController::handleViewRequest))
	{
		return errorStateGui::VIEW_CONNECT_ERR;
	}	
	return errorStateGui::NO_ERR;
}

errorStateGui GuiMediaFusionController::connectToModel()
{
	return errorStateGui::NO_ERR;
}



void GuiMediaFusionController::handleViewRequest(QString)
{

}
