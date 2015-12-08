/*
 * VideoTrack.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */

#ifndef VIDEOTRACK_H_
#define VIDEOTRACK_H_


#include "Definitions.h"
#include <ppapi/utility/completion_callback_factory.h>
#include <ppapi/cpp/resource.h>
#include <ppapi/cpp/media_stream_video_track.h>
#include <ppapi/cpp/size.h>
#include <ppapi/c/ppb_video_frame.h>
#include <ppapi/cpp/video_frame.h>

class VideoTrack {
public:
	inline VideoTrack(): configured(false), trackingFrames(false){}
	VideoTrack(pp::Resource track);

	bool configure(pp::Size frameSize, PP_VideoFrame_Format frameFormat);

	bool start();
	void stop();

	bool isTrackingFrames() const;
	bool isConfigured() const;

	pp::MediaStreamVideoTrack& getPPApiTrack() 	;
	pp::VideoFrame getCurrentFrame() ;


	virtual ~VideoTrack();

private:
	CREATE_CALLBACK(VideoTrack);

	pp::MediaStreamVideoTrack track;
	pp::VideoFrame currentFrame;

	bool configured;
	bool trackingFrames;

	void onConfigured(int result);
	void onTrackFrame(int result, pp::VideoFrame frame);


};

#endif /* VIDEOTRACK_H_ */
