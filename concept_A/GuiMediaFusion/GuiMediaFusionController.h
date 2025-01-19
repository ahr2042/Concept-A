#pragma once

#include "GuiMediaFusion.h"

class GuiMediaFusionController : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionController();

public slots:
	void handleViewRequest(QString);


private:
	GuiMediaFusion* guiMediaFusionView = nullptr;

};