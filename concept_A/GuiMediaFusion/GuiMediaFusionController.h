#pragma once

#include "GuiMediaFusion.h"
#include <variant>
class GuiMediaFusionController : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionController();
	errorStateGui setGuiElement(GUI_ELEMENTS, std::variant<int,std::string>);

public slots:
	void handleViewRequest(GUI_ELEMENTS, QVariant);


private:
	GuiMediaFusion* MF_View = nullptr;

	errorStateGui connectToView();
	
	void InitialGuiConfiguration();

};