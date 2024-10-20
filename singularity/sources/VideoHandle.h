#pragma once
#include <opencv2/videoio.hpp>

class VideoHandle : public cv::VideoCapture
{
public:
	VideoHandle();
	~VideoHandle();

	int f_i_updateCameraCount();

private:
	
};