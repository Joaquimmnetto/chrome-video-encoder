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
		instance(_instance), handle(instance), muxer(_muxer), track(NULL), new_track(NULL), video_profile(PP_VIDEOPROFILE_VP8_ANY), frame_format(
				PP_VIDEOFRAME_FORMAT_I420), cb_factory(this), encoding(false), encode_ticking(
				false), last_tick(0), last_ts(0) {

	if (!probed_encoder) {
		ProbeEncoder();
	}

}

VideoEncoder::~VideoEncoder() {
	delete_and_nulify(track);
	tracks.clear();

}
void VideoEncoder::Encode(pp::Size _requested_size,
		PP_VideoProfile _video_profile) {

	frame_size = _requested_size;
	video_profile = _video_profile;
	Log(frame_size.width() << "," << frame_size.height());
	muxer.ConfigureVideo(frame_size.width(), frame_size.height());
	StartTrackFrames();
	StartEncoder();
}

void VideoEncoder::SetTrack(int track_id) {

	if (tracks.find(track_id) != tracks.end()) {
		if (track == NULL) {
			track = tracks[track_id];
		}
		else {
			new_track = tracks[track_id];
		}

	} else {
		LogError(-99, "Track " << track_id << " não encontrada");
		return;
	}

}

void VideoEncoder::AddTrack(int track_id, pp::Resource track_res) {
	if (encoding) {
		LogError(-99,
				"Adicione as trilhas antes do início da execução do encoder!");
		return;
	}

	if (!pp::MediaStreamVideoTrack::IsMediaStreamVideoTrack(track_res)) {
		LogError(-99, "Track não é um recurso válido");
		return;
	}

	if (tracks.find(track_id) == tracks.end()) {
		tracks[track_id] = new VideoTrack(instance, track_res);
	}
}

void VideoEncoder::ProbeEncoder() {
	encoder = pp::VideoEncoder(handle);
	encoder.GetSupportedProfiles(
			cb_factory.NewCallbackWithOutput(&VideoEncoder::OnEncoderProbed));
}

void VideoEncoder::OnEncoderProbed(int32 result,
		const std::vector<PP_VideoProfileDescription> profiles) {
	probed_encoder = (result == PP_OK);
}

void VideoEncoder::StartEncoder() {
	encoder = pp::VideoEncoder(handle);
	timestamps.clear();
	Log("Inicializando Encoder");
	Log(
			"Tamanho do frame base do encoder: (" << frame_size.width() << "," << frame_size.height() << ")");
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
		LogError(result,
				"Não foi possível adquirir o tamanho do frame do encoder");
		return;
	}
	if (!encoder_size.IsEmpty()) {
		frame_size = encoder_size;
	}

	encoding = true;
	encoder.GetBitstreamBuffer(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));

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
		if (new_track != NULL) {
			track = new_track;
			new_track = NULL;
		}
		if (receiving_frames && track && !track->current_frame.is_null()) {
			pp::VideoFrame frame = track->current_frame;
			track->current_frame.detach();
			GetEncoderFrame(frame);
		}
		ScheduleNextEncode();
	}
}

void VideoEncoder::GetEncoderFrame(const pp::VideoFrame& track_frame) {
	encoder.GetVideoFrame(
			cb_factory.NewCallbackWithOutput(&VideoEncoder::OnEncoderFrame,
					track_frame));
}

void VideoEncoder::OnEncoderFrame(int32 result, pp::VideoFrame encoder_frame,
		pp::VideoFrame track_frame) {

	if (result == PP_ERROR_ABORTED) {
		track->RecycleFrame(track_frame);
		return;
	}
	if (result != PP_OK) {
		track->RecycleFrame(track_frame);
		LogError(result,
				"Não foi possível pegar o frame do encoder de vídeo, pulando frame...");
		return;
	}

	if (track_frame.is_null()) {
		Log(
				"Frame " << track_frame.GetTimestamp() << " é nulo, pulando frame...");
		return;
	}

	if (CopyVideoFrame(encoder_frame, track_frame) == PP_OK) {
		EncodeFrame(encoder_frame);
	}

	track->RecycleFrame(track_frame);
}

int32 VideoEncoder::CopyVideoFrame(pp::VideoFrame dest, pp::VideoFrame src) {
	if (dest.GetDataBufferSize() < src.GetDataBufferSize()) {
		LogError(-99,
				"Ao copiar o frame : "<< dest.GetDataBufferSize() << " < " << src.GetDataBufferSize());
		return -99;
	}

	dest.SetTimestamp(src.GetTimestamp());
	memcpy(dest.GetDataBuffer(), src.GetDataBuffer(), src.GetDataBufferSize());
	return PP_OK;
}

void VideoEncoder::EncodeFrame(const pp::VideoFrame& frame) {
	timestamps.push_back(frame.GetTimestamp());
#ifdef LOG_FRAMES
	Log("Encoding frame " << frame.GetTimestamp() * NS_EXP);
#endif
	encoder.Encode(frame, PP_TRUE,
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
	if (result == PP_ERROR_ABORTED) {
		return;
	}
	if (result != PP_OK) {
		LogError(result, "Não foi possível adquirir o Bitstream Buffer");
		return;
	}

	uint64 timestamp_ns = timestamps.front() * NS_EXP
	;

	byte* frame_data = static_cast<byte*>(buffer.buffer);
	uint64 size = buffer.size;
	bool key_frame = buffer.key_frame;

#ifdef LOG_FRAMES
	Log("Salvando frame " << timestamp_ns << " de tamanho "<< size);
#endif
	if (timestamp_ns >= last_ts) {
		last_ts = timestamp_ns;
		if (!muxer.AddVideoFrame(frame_data, size, timestamp_ns, key_frame)) {
			LogError(-99, "Enquanto salvando frame "<< timestamp_ns);
		}

	} else {
		Log("Frame "<< timestamp_ns << " atrasado, não salvando");
	}

	timestamps.pop_front();
	encoder.RecycleBitstreamBuffer(buffer);

	encoder.GetBitstreamBuffer(
			cb_factory.NewCallbackWithOutput(
					&VideoEncoder::OnGetBitstreamBuffer));
}

void VideoEncoder::StartTrackFrames() {
	for (std::map<int, VideoTrack*>::iterator it = tracks.begin();
			it != tracks.end(); it++) {
		it->second->StartTracking(frame_size);
	}
	receiving_frames = true;
}

void VideoEncoder::StopEncode() {
	StopTrackingFrames();
	encoder.Close();

	tracks.clear();
	encoding = false;
	muxer.Finish();
}

void VideoEncoder::StopTrackingFrames() {
	receiving_frames = false;
	for (std::map<int, VideoTrack*>::iterator it = tracks.begin();
			it != tracks.end(); it++) {
		it->second->StopTracking();
	}
}

