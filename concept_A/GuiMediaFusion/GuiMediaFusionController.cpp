#include "GuiMediaFusionController.h"

GuiMediaFusionController::GuiMediaFusionController()
{
	guiMediaFusionView = new GuiMediaFusion();
	connect(guiMediaFusionView, &GuiMediaFusion::guiRequest, this, &GuiMediaFusionController::handleViewRequest);
}