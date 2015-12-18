/*
 * WebmMuxer.cpp
 *
 *  Created on: 24 de nov de 2015
 *      Author: joaquim
 */

#include "webm_muxer.h"

#define INSTANCE instance
#include "log.h"

WebmMuxer::WebmMuxer( pp::Instance& _instance ) :
		instance(_instance),pSegment(0), delayed_frame_count(0), initialized(false), finished(false) {
}

WebmMuxer::~WebmMuxer() {
	writer.Close();
	audio_queue.clear();
	video_queue.clear();
	delete_and_nulify(pSegment);
}

int WebmMuxer::Init( std::string file_name ) {
	if (initialized) {
		return true;
	}
	std::stringstream sfilename;
	sfilename << "/persistent/" << file_name;
	if (!writer.Open(sfilename.str().c_str())) {
		Log("Arquivo não pôde ser aberto");
		return false;
	}
	pSegment = new mkvmuxer::Segment();

	pSegment->Init(&writer);
	pSegment->set_mode(mkvmuxer::Segment::kFile);
	video_track_num = pSegment->AddVideoTrack(video_width, video_height, 1);
	audio_track_num = pSegment->AddAudioTrack(audio_sample_rate, audio_channels,
			2);
	pSegment->CuesTrack(video_track_num);

	last_frame.set_timestamp(0);

	initialized = true;
	finished = false;

	return true;

}

void WebmMuxer::ConfigureVideo( int _video_width, int _video_height ) {
	this->video_width = _video_width;
	this->video_height = _video_height;
}

void WebmMuxer::ConfigureAudio( int _audio_sample_rate, int _audio_channels ) {
	this->audio_sample_rate = _audio_sample_rate;
	this->audio_channels = _audio_channels;
}



//bool WebmMuxer::PushAudioFrame(byte* data, uint32 length, uint64 timestamp) {
//
//	mkvmuxer::Frame frame;
//	CreateFrame(data, length, timestamp, 2, false, frame);
//
//	if (frame == NULL) {
//		Log("Erro ao inicializar o frame de audio " << timestamp);
//		return false;
//	}
//
//	audio_queue.push_back(frame);
//	return true;
//
//}
//
//bool WebmMuxer::PushVideoFrame( byte* data, uint32 length, uint64 timestamp, bool key_frame ) {
//	mkvmuxer::Frame frame;
//	CreateFrame(data, length, timestamp, 1, key_frame, frame);
//
//	if (frame == NULL) {
//		Log("Erro ao inicializar o frame de video " << timestamp);
//		return false;
//	}
//	video_queue.push_back(frame);
//	return true;
//}

bool WebmMuxer::AddVideoFrame( byte* data, uint32 length, uint64 timestamp, bool key_frame ) {

	if (!Init(file_name)){
		LogError(-99,"Enquanto inicializando o muxer");
		return false;
	}

	mkvmuxer::Frame frame;
	CreateFrame(data, length, timestamp, video_track_num, key_frame, frame);

	if( !frame.IsValid() || (frame.timestamp() < last_frame.timestamp()) ) {
		Log("Frame " << frame.timestamp() << "atrasado na reprodução, pulando frame...");

		delayed_frame_count++;
	}else{
		if(delayed_frame_count > 0){
			SaveDelayed(frame.timestamp());
		}
		if(pSegment->AddGenericFrame(&frame)){
			last_frame.CopyFrom(frame);
			return true;
		}
	}

	return false;
}

bool WebmMuxer::Finish() {
	if (finished)
		return true;

	if (!pSegment->Finalize())
		return false;

	writer.Close();

	audio_queue.clear();
	video_queue.clear();

	finished = true;
	initialized = false;

	delete pSegment;

	return true;
}

void WebmMuxer::CreateFrame( byte* data, uint32 length,
		uint64 timestamp, int track, bool key_frame ,/*out*/mkvmuxer::Frame& frame) {

	if (frame.Init(data, length)) {
		frame.set_is_key(key_frame);
		frame.set_track_number(track);
		frame.set_timestamp(timestamp);
	}
}

void WebmMuxer::SaveDelayed(uint64 current_ts) {

	uint64 delay_ts = last_frame.timestamp();
	uint64 delay_delta = (current_ts - delay_ts)
			/ delayed_frame_count;

	for (int i = 0; delayed_frame_count > 0; i++) {
		last_frame.set_timestamp(delay_ts + i * delay_delta);

		Log("Salvando frame atrasado " << last_frame.timestamp());
		pSegment->AddGenericFrame(&last_frame);
		delayed_frame_count--;
	}
}

//
//bool WebmMuxer::WriteFrames( std::string file_name ) {
//
//	if (!initialized) {
//		Init(file_name);
//	}
//
//	bool result = false;
//	mkvmuxer::Frame* audio_frame;
//	mkvmuxer::Frame* video_frame;
//
//	while (!audio_queue.empty() || !video_queue.empty()) {
//		mkvmuxer::Frame frame;
//
//		if (!audio_queue.empty()) {
//			audio_frame = audio_queue.front();
//		}
//		if (!video_queue.empty()) {
//			video_frame = video_queue.front();
//		}
//
//		if (audio_frame->IsValid() && video_frame->IsValid()) {
//			frame.CopyFrom(
//					video_frame->timestamp() < audio_frame->timestamp() ?
//							*video_frame : *audio_frame);
//		} else if (audio_frame->IsValid()) {
//			frame.CopyFrom(*audio_frame);
//		} else if (video_frame->IsValid()) {
//			frame.CopyFrom(*video_frame);
//		}
//		result = pSegment->AddGenericFrame(&frame) || result;
//
//		(frame.timestamp() == audio_frame->timestamp()) ?
//				audio_queue.pop_front() : video_queue.pop_front();
//	}
//
//	return Finish() && result;
//}

