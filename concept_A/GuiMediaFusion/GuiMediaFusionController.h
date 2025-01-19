#pragma once

#include "GuiMediaFusion.h"
#include "GuiMediaFusionModel.h"

class GuiMediaFusionController : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionController();
	~GuiMediaFusionController();

public slots:
	void handleViewRequest(QString);


private:
	GuiMediaFusion* MF_View = nullptr;
	GuiMediaFusionModel* MF_Model = nullptr;

	errorStateGui connectToView();
	errorStateGui connectToModel();

};