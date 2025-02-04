#pragma once
#include "MediaFusionGCV_API.h"
#include <qobject.h>
#include "errorStateGui.h"
#include <vector>
class GuiMediaFusionModel : public QObject
{
	Q_OBJECT
public:
	GuiMediaFusionModel();
	~GuiMediaFusionModel();

	errorStateGui initGstreamer(int, char**);
	size_t createPipeline();

public slots:	
	errorStateGui updatePipelineInfo(QString);
signals:	
	void updateSourceDeviceInfo(deviceProperties);
	void updateSinkDeviceInfo(deviceProperties);

private:
	std::vector<pipeLine*> pipelineStash;
	errorStateGui getAvailableDevices(size_t);
	
	
};