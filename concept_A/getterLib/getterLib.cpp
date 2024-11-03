#include "pch.h"

#include <string>
#include <cstdint>
#include <cstring>
#include <vector>

#include "getterLib.h"

#include "MS_MediaFoundation.h"


std::vector<MS_MediaFoundation*> m_vpo_MSMF;

void API_f_v_create()
{
	m_vpo_MSMF.push_back(new MS_MediaFoundation);
}

int API_f_i_init()
{
	int v_i_errorState = e_ErrorState::NO_ERR;

	v_i_errorState = m_vpo_MSMF.at(0)->f_i_initializeMediaFoundation();
	if (v_i_errorState != e_ErrorState::NO_ERR)
		return v_i_errorState;

	m_vpo_MSMF.at(0)->f_v_getDeviceFriendlyName();

	return v_i_errorState;
}

int API_f_i_setDevice(int i_i_deviceId)
{
	int v_i_errorState = e_ErrorState::NO_ERR;
	v_i_errorState = m_vpo_MSMF.at(0)->f_i_activateAndCreateSourceReader(i_i_deviceId);
	v_i_errorState = m_vpo_MSMF.at(0)->f_i_deinitializeMediaFoundation();
	return v_i_errorState;

}

int API_f_i_startStreaming()
{
	int v_i_errorState = e_ErrorState::NO_ERR;
	v_i_errorState = m_vpo_MSMF.at(0)->f_i_captureVideoFrame();
	return v_i_errorState;
}

int API_f_i_stopStreaming()
{
	int v_i_errorState = e_ErrorState::NO_ERR;
	m_vpo_MSMF.at(0)->f_v_releaseResources();
	return v_i_errorState;

}

void API_f_v_interpreteError(int i_i_errorId, char* o_pc_interpretation)
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
}


void API_f_v_getDevicesNames(char* o_pc_names)
{
	if (m_vpo_MSMF.at(0)->deviceCount != 0)
	{
		strcpy_s(o_pc_names, MAX_PARAMETER_SIZE, m_vpo_MSMF.at(0)->devicesNames.c_str());		
	}
	strcpy_s(o_pc_names, MAX_PARAMETER_SIZE, "\0");
}