#include "pch.h"

#include <string>
#include <cstdint>
#include <cstring>
#include <vector>

#include "MFgetterLib.h"

#include "MS_MediaFoundation.h"


std::vector<MS_MediaFoundation*> m_vpo_MSMF;

int32_t mediaLib_create()
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;

	m_vpo_MSMF.push_back(new MS_MediaFoundation);
	return errorState;

}

int32_t mediaLib_init()
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;

	errorState = m_vpo_MSMF.at(0)->f_i_initializeMediaFoundation();
	if (errorState != e_ErrorState::NO_ERR)
		return errorState;

	m_vpo_MSMF.at(0)->f_v_getDeviceFriendlyName();

	return errorState;
}

int32_t mediaLib_setDevice(int32_t i_i_deviceId)
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;
	errorState = m_vpo_MSMF.at(0)->f_i_activateAndCreateSourceReader(i_i_deviceId);
	if (errorState == e_ErrorState::NO_ERR)
	{
		errorState = m_vpo_MSMF.at(0)->f_i_deinitializeMediaFoundation();
	}
	return errorState;

}

int32_t mediaLib_startStreaming()
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;
	errorState = m_vpo_MSMF.at(0)->f_i_captureVideoFrame();
	return errorState;
}

int32_t mediaLib_stopStreaming()
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;
	m_vpo_MSMF.at(0)->f_v_releaseResources();
	return errorState;

}

int32_t mediaLib_interpreteError(int32_t i_i_errorId, char* o_pc_interpretation)
{
	std::string v_str_source;
	switch ((e_ErrorState)i_i_errorId)
	{
	case MF_STARTUP_ERR:
		v_str_source = "Failed to initialize Media Foundation";
		break;
	case MF_CREATE_ATTRIBUTES_ERR:
		v_str_source = "Failed to create attribute store";
		break;
	case MF_SET_GUID_ERR:
		v_str_source = "Failed to set video capture attribute";
		break;
	case MF_ENUM_DEVICE_SOURCES_ERR:
		v_str_source = "Failed to enumerate video capture devices";
		break;
	case MF_NO_VIDEO_DEVICE_ERR:
		v_str_source = "No video capture devices found";
		break;
	case MF_ACTIVATE_DEVICE_ERR:
		v_str_source = "Failed to activate video capture device (IMFMediaSource)";
		break;
	case MF_SOURCE_READER_ERR:
		v_str_source = "Failed to create source reader from media source";
		break;
	case MF_SAMPLE_READ_ERR:
		v_str_source = "Failed to read sample";
		break;
	case MF_READER_PTR_ERR:
		v_str_source = "Reader pointer is not initialized";
		break;
	default:
		v_str_source = "Unkown Err" + std::to_string(i_i_errorId);
		break;
	}
	strcpy_s(o_pc_interpretation, MAX_PARAMETER_SIZE, v_str_source.c_str());
	return (int32_t)e_ErrorState::NO_ERR;
}


int32_t mediaLib_getDevicesNames(char* o_pc_names)
{
	int32_t errorState = (int32_t)e_ErrorState::NO_ERR;
	if (m_vpo_MSMF.at(0)->deviceCount != 0)
	{
		strcpy_s(o_pc_names, MAX_PARAMETER_SIZE, m_vpo_MSMF.at(0)->devicesNames.c_str());		
		return;
	}
	strcpy_s(o_pc_names, MAX_PARAMETER_SIZE, "\0");
	return errorState;
}
