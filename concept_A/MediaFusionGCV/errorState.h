#pragma once
enum errorState
{
	ERR = -(1 << 7),
	CREATE_DEVICE_MONITOR_ERR,
	START_DEVICE_MONITOR_ERR,
	NO_VIDEO_DEVICE_FOUND_ERR,
	GET_DEVICE_INFO_READABLE_ERR,
	NO_ERR = 0
};