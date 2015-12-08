/*
 * VideoEncoder.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */

#ifndef VIDEOENCODER_H_
#define VIDEOENCODER_H_

#include <string>
#include <deque>
#include <ppapi/c/pp_time.h>
#include <ppapi/c/ppb_video_encoder.h>
#include <ppapi/cpp/video_frame.h>
#include <ppapi/cpp/video_encoder.h>

#include "../libwebm/mkvmuxer.hpp"

#include "VideoTrack.h"
#include "Definitions.h"
#include "PPApiMkvWriter.h"



class VideoEncoder {
public:

	VideoEncoder(VideoTrack& track, std::string filename);
	bool init(pp::Size frameSize, PP_VideoProfile profile,
			PP_VideoFrame_Format frameFormat, int bitrate, int fps);

	pp::VideoEncoder& getPPApiEncoder();

	bool isInitialized() const;
	bool scheduleEncode();

	void stop();

	virtual ~VideoEncoder();

private:
	CREATE_CALLBACK(VideoEncoder);
	std::deque<uint64_t> framesTs;

	mkvmuxer::Segment segment;


	PPApiMkvWriter writer;

	VideoTrack& track;

	bool encodeTicking;
	bool encoding;
	bool initialized;
	int fps;
	PP_Time lastTick;

	pp::VideoEncoder encoder;

	void onSuppportedProfiles(int32_t result, const std::vector<PP_VideoProfileDescription> profiles);
	bool onInitialized(int result);
	void onBitstreamBuffer(int32_t result,PP_BitstreamBuffer buffer);
	void getEncoderFrame(int result);
	void onEncoderFrame(int result, pp::VideoFrame encoderFrame,
			pp::VideoFrame trackFrame);
	void onEncodeDone(int result);

	void encode(int result);

};

#endif /* VIDEOENCODER_H_ */
