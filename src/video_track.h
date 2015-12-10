/*
 * videotrack.h
 *
 *  Created on: 10 de dez de 2015
 *      Author: joaquim
 */

#ifndef VIDEO_TRACK_H_
#define VIDEO_TRACK_H_

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/size.h>
#include <ppapi/cpp/video_frame.h>
#include <ppapi/cpp/media_stream_video_track.h>

#include <ppapi/utility/completion_callback_factory.h>

#include "tipos.h"

class VideoTrack {
public:
	VideoTrack( pp::Instance* instance, pp::Size frame_size, pp::Resource track_res );

	~VideoTrack();

	void StartTracking();

	void ConfigureCallback( int config_res );

	void RecycleFrame( pp::VideoFrame& frame );

	void StopTracking();

	pp::VideoFrame current_frame;
private:
	pp::Instance* instance;
	pp::Size frame_size;

	pp::MediaStreamVideoTrack track;
	pp::CompletionCallbackFactory<VideoTrack> cb_factory;

	bool tracking;


	void TrackFramesLoop(int getframe_res, pp::VideoFrame frame);
};

#endif /* VIDEO_TRACK_H_ */
