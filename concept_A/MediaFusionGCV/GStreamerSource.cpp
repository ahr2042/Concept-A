#include "pch.h"
#include "GStreamerSource.h"




// Destructor
GStreamerSource::~GStreamerSource() {
    if (sourceElement) {
        gst_object_unref(sourceElement);
    }
}

// Public method to get the source element
GstElement* GStreamerSource::getSourceElement() {
    return sourceElement;
}



GStreamerSource* GStreamerSource::createElement(std::string deviceName)
{
    
    return nullptr;
}
