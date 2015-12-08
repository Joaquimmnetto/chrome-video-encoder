/*
 * VideoRecorder.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */

#ifndef VIDEORECORDER_H_
#define VIDEORECORDER_H_

#include <string>
#include <ppapi/cpp/size.h>
#include <ppapi/c/ppb_video_frame.h>
#include <ppapi/c/pp_codecs.h>

#include "VideoEncoder.h"
#include "VideoTrack.h"



class VideoRecorder {
public:
	VideoRecorder(pp::Resource resTrack,std::string filename);

	bool configure(const pp::Size& frameSize, PP_VideoProfile profile,
			PP_VideoFrame_Format frameFormat, int bitrate, int fps);

	bool start();
	void stop();
	virtual ~VideoRecorder();

private:
	std::string filename;
	VideoTrack track;
	VideoEncoder encoder;


};

#endif /* VIDEORECORDER_H_ */
