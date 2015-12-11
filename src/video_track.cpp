/*
 * videotrack.cpp
 *
 *  Created on: 10 de dez de 2015
 *      Author: joaquim
 */

#include "video_track.h"

#define INSTANCE (*instance)
#include "log.h"



VideoTrack::VideoTrack(pp::Instance* _instance, pp::Size _frame_size, pp::Resource track_res):
											instance(_instance), frame_size(_frame_size), track(track_res), cb_factory(this), tracking(false)
{}

VideoTrack::~VideoTrack() {}

void VideoTrack::StartTracking() {
	if(tracking){
		Log("Já iniciado, pare antes de iniciar a track novamente");
		return;
	}

	int32 attrib_list[] = { PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT,	PP_VIDEOFRAME_FORMAT_I420,
									PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH,frame_size.width(),
									PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT, frame_size.height(),
									PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE };
	track.Configure(attrib_list,cb_factory.NewCallback(&VideoTrack::ConfigureCallback));

	Log("Track iniciada");

}

void VideoTrack::ConfigureCallback(int config_res){
	if(config_res != PP_OK){
		LogError(config_res," Enquanto configurava a trilha de frames");
		return;
	}
	else{
		tracking = true;
	}
	if(!tracking){
		Log("Track parada");
		return;
	}

	track.GetFrame(cb_factory.NewCallbackWithOutput(&VideoTrack::TrackFramesLoop));

}

void VideoTrack::RecycleFrame(pp::VideoFrame& frame){
	track.RecycleFrame(frame);
}

void VideoTrack::TrackFramesLoop(int getframe_res, pp::VideoFrame frame){
	if (getframe_res == PP_ERROR_ABORTED)
		return;

	if (!current_frame.is_null()) {
		track.RecycleFrame(current_frame);
		current_frame.detach();
	}

	if (getframe_res != PP_OK) {
		LogError(getframe_res, "Não foi possível capturar o frame da trilha");
		return;
	}

	current_frame = frame;
	if (tracking){
		track.GetFrame(cb_factory.NewCallbackWithOutput(&VideoTrack::TrackFramesLoop));
	}

}

void VideoTrack::StopTracking() {
	tracking = false;
	if (!current_frame.is_null()) {
		track.RecycleFrame(current_frame);
		current_frame.detach();
	}
	track.Close();
}
