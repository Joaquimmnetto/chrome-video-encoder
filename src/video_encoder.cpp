// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Modified by ...

#include "video_encoder.h"
#include "video_encoder_instance.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>
#include <vector>

#include <sys/mount.h>

#include <nacl_io/nacl_io.h>

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/cpp/input_event.h>

#include <ppapi/cpp/module.h>
#include <ppapi/cpp/rect.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>

#include <ppapi/cpp/video_frame.h>
#include <ppapi/utility/threading/simple_thread.h>

#define INSTANCE (*instance)
#include "log.h"

#define NS_EXP 1000000000; //10^9

#define FS_PATH "/persistent/"

static bool probed_encoder = false;

VideoEncoder::VideoEncoder(pp::Instance* _instance, WebmMuxer& _muxer) :
		instance(_instance), handle(instance), muxer(_muxer), track(0),  video_profile(
				PP_VIDEOPROFILE_VP8_ANY), frame_format(
				PP_VIDEOFRAME_FORMAT_I420), cb_factory(this), encoding(
				false), encode_ticking(false), last_tick(0) {

	if (!probed_encoder) {
		ProbeEncoder();
	}

}

VideoEncoder::~VideoEncoder() {
	delete_and_nulify(track);

}
void VideoEncoder::Encode(pp::Size _requested_size,	pp::Resource track_res, PP_VideoProfile _video_profile) {

	requested_size = _requested_size;
	frame_size = _requested_size;
	video_profile = _video_profile;

	muxer.ConfigureVideo(requested_size.width(), requested_size.height());

	SetTrack(track_res);

	StartEncoder();
}

void VideoEncoder::SetTrack(pp::Resource track_res){
	if(!pp::MediaStreamVideoTrack::IsMediaStreamVideoTrack(track_res)){
		Log("Track não é um recurso válido");
		return;
	}

	if(track && receiving_frames) {
		StopTrackingFrames();
	}

	delete_and_nulify(track);
	track = new VideoTrack(instance, frame_size, track_res);

	StartTrackFrames();
}

void VideoEncoder::ProbeEncoder() {
	encoder = pp::VideoEncoder(handle);
	encoder.GetSupportedProfiles(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnEncoderProbed));
}

void VideoEncoder::OnEncoderProbed(int32 result,
		const std::vector<PP_VideoProfileDescription> profiles) {
	Log("Encoder provado");
	probed_encoder = (result == PP_OK);
}

void VideoEncoder::StartEncoder() {
	encoder = pp::VideoEncoder(handle);
	timestamps.clear();
	Log("Inicializando Encoder");
	int32 error = encoder.Initialize(frame_format, frame_size, video_profile,
			2000000, PP_HARDWAREACCELERATION_WITHFALLBACK,
			cb_factory.NewCallback(&VideoEncoder::OnInitializedEncoder));

	if (error != PP_OK_COMPLETIONPENDING) {
		LogError(error, "Não foi possível inicializar encoder");
		return;
	}


}

void VideoEncoder::OnInitializedEncoder(int32 result) {
	Log("Encoder Inicializado");
	if (result != PP_OK) {
		LogError(result, "Falha na inicialização do encoder");
		return;
	}

	pp::Size encoder_size;
	if (encoder.GetFrameCodedSize(&encoder_size) != PP_OK) {
		LogError(result,"Não foi possível adquirir o tamanho do frame do encoder");
		return;
	}
	if(!encoder_size.IsEmpty()){
		frame_size = encoder_size;
	}

	encoding = true;
	encoder.GetBitstreamBuffer(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));


	StartTrackFrames();
	ScheduleNextEncode();

}

void VideoEncoder::ScheduleNextEncode() {

	if (encode_ticking)
		return;

	PP_Time now = pp::Module::Get()->core()->GetTime();
	PP_Time tick = 1.0 / 30;

	PP_Time delta = tick
			- std::max(std::min(now - last_tick - tick, tick), 0.0);

	(static_cast<VideoEncoderInstance*>(instance))->encoderThread().message_loop().PostWork(
			cb_factory.NewCallback(&VideoEncoder::GetEncoderFrameTick),
			delta * 1000);

	last_tick = now;
	encode_ticking = true;
}

void VideoEncoder::GetEncoderFrameTick(int32 result) {
	encode_ticking = false;

	if (encoding) {
		if (receiving_frames && track && !track->current_frame.is_null()) {
			pp::VideoFrame frame = track->current_frame;
			track->current_frame.detach();
			GetEncoderFrame(frame);
		}else{

		}
		ScheduleNextEncode();
	}
}

void VideoEncoder::GetEncoderFrame(const pp::VideoFrame& track_frame) {
	encoder.GetVideoFrame(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnEncoderFrame, track_frame));
}

void VideoEncoder::OnEncoderFrame(int32 result, pp::VideoFrame encoder_frame,
		pp::VideoFrame track_frame) {
	if (result == PP_ERROR_ABORTED) {
		track->RecycleFrame(track_frame);
		return;
	}
	if (result != PP_OK) {
		track->RecycleFrame(track_frame);
		LogError(result, "Não foi possível pegar o frame do encoder de vídeo");
		return;
	}
	pp::Size track_frame_size;
	track_frame.GetSize(&track_frame_size);

	if (track_frame_size != frame_size) {
		track->RecycleFrame(track_frame);
		LogError(PP_ERROR_FAILED,
				"MediaStreamVideoTrack tem o tamaho do frame inválido");
		return;
	}

	if (CopyVideoFrame(encoder_frame, track_frame) == PP_OK) {
		EncodeFrame(encoder_frame);
	}

	track->RecycleFrame(track_frame);
}

int32 VideoEncoder::CopyVideoFrame(pp::VideoFrame dest, pp::VideoFrame src) {
	if (dest.GetDataBufferSize() < src.GetDataBufferSize()) {
		std::ostringstream oss;
		oss << "Tamanho do buffer de destino inválido ao copiar o frame : "
				<< dest.GetDataBufferSize() << " < " << src.GetDataBufferSize();
		LogError(PP_ERROR_FAILED, oss.str());
		return PP_ERROR_FAILED;
	}

	dest.SetTimestamp(src.GetTimestamp());
	memcpy(dest.GetDataBuffer(), src.GetDataBuffer(), src.GetDataBufferSize());
	return PP_OK;
}

void VideoEncoder::EncodeFrame(const pp::VideoFrame& frame) {
	timestamps.push_back(
			static_cast<double>(frame.GetTimestamp()));

#ifdef LOG_FRAMES
	Log("Encoding frame " << frame.GetTimestamp() * NS_EXP);
#endif
	encoder.Encode(frame, PP_FALSE,
			cb_factory.NewCallback(&VideoEncoder::OnEncodeDone));
}

void VideoEncoder::OnEncodeDone(int32 result) {
	if (result == PP_ERROR_ABORTED) {
		return;
	}
	if (result != PP_OK) {
		LogError(result, "Falha ao encodar");
	}
}

void VideoEncoder::OnGetBitstreamBuffer(int32 result,
		PP_BitstreamBuffer buffer) {
	if (result == PP_ERROR_ABORTED)
		return;
	if (result != PP_OK) {
		LogError(result, "Não foi possível adquirir o Bitstream Buffer");
		return;
	}

	uint64 timestamp_ns = timestamps.front() * NS_EXP;
	byte* frame_data = static_cast<byte*>(buffer.buffer);
	uint64 size = buffer.size;
	bool key_frame = buffer.key_frame;

#ifdef LOG_FRAMES
	Log("Salvando frame " << timestamp_ns << " de tamanho "<< size);
#endif
	if (!muxer.AddVideoFrame(frame_data, size, timestamp_ns, key_frame)) {
		LogError(-99, "Enquanto salvando frame "<< timestamp_ns);
	}

	timestamps.pop_front();
	encoder.RecycleBitstreamBuffer(buffer);

	encoder.GetBitstreamBuffer(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));
}

void VideoEncoder::StartTrackFrames() {

	track->StartTracking();
	receiving_frames = true;
}

void VideoEncoder::StopEncode() {
	encoder.Close();
	StopTrackingFrames();

	encoding = false;
	muxer.Finish();
}

void VideoEncoder::StopTrackingFrames() {
	receiving_frames = false;
	track->StopTracking();

}

