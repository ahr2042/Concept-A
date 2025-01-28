#include "stdafx.h"
#include "GuiMediaFusionModel.h"

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

