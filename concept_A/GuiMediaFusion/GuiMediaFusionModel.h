#pragma once
#include "MediaFusionGCV_API.h"
#include <qobject.h>
#include "errorStateGui.h"
class GuiMediaFusionModel : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionModel();
	~GuiMediaFusionModel();

	errorStateGui initGstreamer(int, char**);	

private:
};