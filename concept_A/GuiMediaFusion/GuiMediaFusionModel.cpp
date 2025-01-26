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
	if (mediaLib_GStreamerInit(argc, argv) != errorStateGui::NO_ERR)
	{

	}
	return errorStateGui::NO_ERR;
}

