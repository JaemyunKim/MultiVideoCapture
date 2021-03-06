#include "VideoCaptureType.hpp"

#include <chrono>
#include <thread>
#include <mutex>
std::mutex gMtxStatus;
std::mutex gMtxPrintMsg;


VideoCaptureType::VideoCaptureType() {
	this->release();
	mResolution = { 640, 480 };
	mFps = 30.f;
	mIsSet = false;
}


VideoCaptureType::~VideoCaptureType() {
	this->release();
}


bool VideoCaptureType::open(int index) {
	return this->open(index, -1);
}


bool VideoCaptureType::open(int index, int apiPreference) {
	// check camera status
	if (mStatus == CAM_STATUS_OPENED) {
		std::lock_guard<std::mutex> lock(gMtxPrintMsg);
		std::cout << "camera " << index << " is already opened" << std::endl;
		return false;
	}
	else if (mStatus == CAM_STATUS_SETTING) {
		std::lock_guard<std::mutex> lock(gMtxPrintMsg);
		std::cout << "camera " << index << " is already opened and is on setting" << std::endl;
		return false;
	}
	else if (mStatus == CAM_STATUS_OPENING) {
		std::lock_guard<std::mutex> lock(gMtxPrintMsg);
		std::cout << "camera " << index << " is opening" << std::endl;
		return false;
	}

	// try to open the camera
	release();	// handling the camera disconnected previously
	mStatus = CAM_STATUS_OPENING;
	mCamId = index;
	bool cam_status = false;
	if (apiPreference == -1)
		cam_status = cv::VideoCapture::open(index);
	else
		cam_status = cv::VideoCapture::open(index, apiPreference);

	if (cam_status == true && cv::VideoCapture::grab() == true) {
		mStatus = CAM_STATUS_OPENED;
		if (mIsSet)
			this->set(mResolution, mFps);
	}
	else {
		release();
		std::lock_guard<std::mutex> lock(gMtxPrintMsg);
		std::cout << "can't open the camera " << index << std::endl;
	}

	return cam_status;
}


bool VideoCaptureType::isOpened() const {
	if (mStatus == CAM_STATUS_OPENED || mStatus == CAM_STATUS_SETTING)
		return true;
	else
		return false;
}


CamStatus VideoCaptureType::status() const {
	return mStatus;
}


void VideoCaptureType::release() {
	mStatus = CAM_STATUS_CLOSED;

	cv::VideoCapture::release();
}


bool VideoCaptureType::retrieve(FrameType& frame, int flag) {
	bool status = cv::VideoCapture::retrieve(frame.mat(), flag);

	return status;
}


bool VideoCaptureType::read(FrameType& frame) {
	if (cv::VideoCapture::grab() && mStatus == CAM_STATUS_OPENED) {
		frame.setTimestamp(std::chrono::system_clock::now());
		this->retrieve(frame);
	}
	else if (mStatus == CAM_STATUS_SETTING) {
		frame.release();
	}
	else {
		//release();
		mStatus = CAM_STATUS_CLOSED;
		frame.release();
	}

	return !frame.empty();
}


VideoCaptureType& VideoCaptureType::operator >> (FrameType& frame) {
	read(frame);

	return *this;
}


bool VideoCaptureType::set(cv::Size resolution, float fps) {
	mStatus = CAM_STATUS_SETTING;
	mIsSet = true;

	// get old settings
	cv::Size oldSize((int)cv::VideoCapture::get(cv::CAP_PROP_FRAME_WIDTH), (int)cv::VideoCapture::get(cv::CAP_PROP_FRAME_HEIGHT));
	double oldFps = cv::VideoCapture::get(cv::CAP_PROP_FPS);
	double oldAutofocus = cv::VideoCapture::get(cv::CAP_PROP_AUTOFOCUS);

	if (resolution == oldSize && fps == oldFps)
		return true;

	if (resolution == cv::Size(-1, -1))	resolution = mResolution;
	else	mResolution = resolution;
	if (mFps == -1.f)	fps = mFps;
	else	mFps = fps;

	// disable autofocus
	if (oldAutofocus != 0) {
		cv::VideoCapture::set(cv::CAP_PROP_AUTOFOCUS, 0);
	}

	// set resolution and fps
	bool statusSize = true, statusFps = true;
	if (resolution != oldSize) {
		statusSize =
			cv::VideoCapture::set(cv::CAP_PROP_FRAME_WIDTH, resolution.width) &&
			cv::VideoCapture::set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);
	}
	if (fps != oldFps) {
		statusFps = cv::VideoCapture::set(cv::CAP_PROP_FPS, fps);
	}

	if (statusSize && statusFps) {
		mStatus = CAM_STATUS_OPENED;
		return true;
	}
	else {
		// rollback resolution
		cv::VideoCapture::set(cv::CAP_PROP_FRAME_WIDTH, oldSize.width);
		cv::VideoCapture::set(cv::CAP_PROP_FRAME_HEIGHT, oldSize.height);

		// rollback fps
		cv::VideoCapture::set(cv::CAP_PROP_FPS, oldFps);

		mStatus = CAM_STATUS_OPENED;
		return false;
	}
}


double VideoCaptureType::get(int propId) const {
	switch (propId)
	{
	case cv::CAP_PROP_FRAME_WIDTH:
		return mResolution.width;
	case cv::CAP_PROP_FRAME_HEIGHT:
		return mResolution.height;
	case cv::CAP_PROP_FPS:
		return mFps;
	default:
		return cv::VideoCapture::get(propId);
	}
}
