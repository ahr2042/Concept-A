#include "stdafx.h"
#include "GuiMediaFusionController.h"
#include <iostream>
#include "MediaFusionGCV_API.h"


GuiMediaFusionController::GuiMediaFusionController()
{
	MF_View = new GuiMediaFusion();	
	connectToView();
	InitialGuiConfiguration();
	MF_View->show();	
}

errorStateGui GuiMediaFusionController::setGuiElement(GUI_ELEMENTS, std::variant<int, std::string>)
{
	return errorStateGui::NO_ERR;
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


errorStateGui GuiMediaFusionController::connectToView()
{
	if (connect(MF_View, &GuiMediaFusion::viewClassRequest, this, &GuiMediaFusionController::handleViewRequest))
	{
		return errorStateGui::VIEW_CONNECT_ERR;
	}	
	return errorStateGui::NO_ERR;
}


void GuiMediaFusionController::handleViewRequest(GUI_ELEMENTS requesterElement, QVariant optional)
{
	switch (requesterElement)
	{
	case TURN_ON:
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

