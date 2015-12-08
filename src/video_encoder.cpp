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


#define FS_PATH "/persistent/"

static bool probed_encoder = false;

VideoEncoder::VideoEncoder(pp::Instance* _instance, WebmMuxer& _muxer) :
		instance(_instance),
		handle(instance),
		muxer(_muxer),
		video_profile(PP_VIDEOPROFILE_VP8_ANY),
		frame_format(PP_VIDEOFRAME_FORMAT_I420),
		callback_factory(this),
		is_encoding(false),
		is_encode_ticking(false),
		last_encode_tick_(0)	{
	if(!probed_encoder){
		ProbeEncoder();
	}

}

VideoEncoder::~VideoEncoder() {

}
void VideoEncoder::Encode(pp::Size _requested_size,
		pp::Resource _resource_track, PP_VideoProfile _video_profile) {

	requested_size = _requested_size;
	video_track = pp::MediaStreamVideoTrack(_resource_track);
	video_profile = _video_profile;

	muxer.ConfigureVideo(requested_size.width(),requested_size.height());

	ConfigureTrack();
}


void VideoEncoder::ConfigureTrack() {
	if (encoder_size.IsEmpty())
		frame_size = requested_size;
	else
		frame_size = encoder_size;

	int32 attrib_list[] = { PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT,	frame_format,
							PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH,frame_size.width(),
							PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT, frame_size.height(),
							PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE };

	video_track.Configure(attrib_list,
			callback_factory.NewCallback(
					&VideoEncoder::OnConfiguredTrack));
}

void VideoEncoder::OnConfiguredTrack(int32 result) {
	if (result != PP_OK) {
		LogError(result, "Erro ao configurar a trilha");
		return;
	}

	if (is_encoding) {
		StartTrackFrames();
		ScheduleNextEncode();
	} else{
		StartEncoder();
	}

}

void VideoEncoder::ProbeEncoder() {
	video_encoder = pp::VideoEncoder(handle);
	video_encoder.GetSupportedProfiles(
			callback_factory.NewCallbackWithOutput(
					&VideoEncoder::OnEncoderProbed));
}

void VideoEncoder::OnEncoderProbed(int32 result,
		const std::vector<PP_VideoProfileDescription> profiles) {
	probed_encoder = (result == PP_OK);
}

void VideoEncoder::StartEncoder() {
	video_encoder = pp::VideoEncoder(handle);
	frames_timestamps.clear();

	int32 error = video_encoder.Initialize(
										frame_format,
										frame_size,
										video_profile,
										2000000,
										PP_HARDWAREACCELERATION_WITHFALLBACK,
										callback_factory.NewCallback(&VideoEncoder::OnInitializedEncoder)
									);

	if (error != PP_OK_COMPLETIONPENDING) {
		LogError(error, "Não foi possível inicializar encoder");
		return;
	}
}

void VideoEncoder::OnInitializedEncoder(int32 result) {
	if (result != PP_OK) {
		LogError(result, "Falha na inicialização do encoder");
		return;
	}

	is_encoding = true;

	if (video_encoder.GetFrameCodedSize(&encoder_size) != PP_OK) {
		LogError(result, "Não foi possível adquirir o tamanho do frame do encoder");
		return;
	}

	video_encoder.GetBitstreamBuffer(
			callback_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));

	if (encoder_size != frame_size)
		ConfigureTrack();
	else {
		StartTrackFrames();
		ScheduleNextEncode();
	}
}

void VideoEncoder::ScheduleNextEncode() {
	// Avoid scheduling more than once at a time.
	if (is_encode_ticking)
		return;

	PP_Time now = pp::Module::Get()->core()->GetTime();
	PP_Time tick = 1.0 / 30;

	PP_Time delta = tick
			- std::max(std::min(now - last_encode_tick_ - tick, tick), 0.0);

	(static_cast<VideoEncoderInstance*>(instance))->encoderThread().message_loop().PostWork(
			callback_factory.NewCallback(
					&VideoEncoder::GetEncoderFrameTick), delta * 1000);

	last_encode_tick_ = now;
	is_encode_ticking = true;
}

void VideoEncoder::GetEncoderFrameTick(int32 result) {
	is_encode_ticking = false;

	if (is_encoding) {
		if (!current_track_frame.is_null()) {
			pp::VideoFrame frame = current_track_frame;
			current_track_frame.detach();
			GetEncoderFrame(frame);
		}
		ScheduleNextEncode();
	}
}

void VideoEncoder::GetEncoderFrame(const pp::VideoFrame& track_frame) {
	video_encoder.GetVideoFrame(
			callback_factory.NewCallbackWithOutput(
					&VideoEncoder::OnEncoderFrame, track_frame));
}

void VideoEncoder::OnEncoderFrame(int32 result,
		pp::VideoFrame encoder_frame, pp::VideoFrame track_frame) {
	if (result == PP_ERROR_ABORTED) {
		video_track.RecycleFrame(track_frame);
		return;
	}
	if (result != PP_OK) {
		video_track.RecycleFrame(track_frame);
		LogError(result, "Não foi possível pegar o frame do encoder de vídeo");
		return;
	}

	track_frame.GetSize(&frame_size);

	if (frame_size != encoder_size) {
		video_track.RecycleFrame(track_frame);
		LogError(PP_ERROR_FAILED, "MediaStreamVideoTrack tem o tamaho do frame inválido");
		return;
	}

	if (CopyVideoFrame(encoder_frame, track_frame) == PP_OK)
		EncodeFrame(encoder_frame);
	video_track.RecycleFrame(track_frame);
}

int32 VideoEncoder::CopyVideoFrame(pp::VideoFrame dest,
		pp::VideoFrame src) {
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
	frames_timestamps.push_back(
			static_cast<uint64>(frame.GetTimestamp() * 1000));
	video_encoder.Encode(frame, PP_FALSE,
			callback_factory.NewCallback(&VideoEncoder::OnEncodeDone));
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

	uint64 timestamp_ns = frames_timestamps.front() * 1000 * 1000;
	byte* frame_data = static_cast<byte*>(buffer.buffer);
	uint64 size = buffer.size;
	bool key_frame = buffer.key_frame;
	muxer.AddVideoFrame(frame_data, size, timestamp_ns, key_frame);

	frames_timestamps.pop_front();
	video_encoder.RecycleBitstreamBuffer(buffer);

	video_encoder.GetBitstreamBuffer(
			callback_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));
}

void VideoEncoder::StartTrackFrames() {
	is_receiving_track_frames = true;
	video_track.GetFrame(
			callback_factory.NewCallbackWithOutput(
					&VideoEncoder::OnTrackFrame));
}

void VideoEncoder::OnTrackFrame(int32 result, pp::VideoFrame frame) {
	if (result == PP_ERROR_ABORTED)
		return;

	if (!current_track_frame.is_null()) {
		video_track.RecycleFrame(current_track_frame);
		current_track_frame.detach();
	}

	if (result != PP_OK) {
		LogError(result, "Não foi possível capturar o frame da trilha");
		return;
	}

	current_track_frame = frame;
	if (is_receiving_track_frames)
		video_track.GetFrame(
				callback_factory.NewCallbackWithOutput(
						&VideoEncoder::OnTrackFrame));
}

void VideoEncoder::StopEncode() {
	video_encoder.Close();
	StopTrackFrames();
	video_track.Close();
	muxer.Finish();
	is_encoding = false;
}

void VideoEncoder::StopTrackFrames() {
	is_receiving_track_frames = false;
	if (!current_track_frame.is_null()) {
		video_track.RecycleFrame(current_track_frame);
		current_track_frame.detach();
	}
}

