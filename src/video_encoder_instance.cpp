// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Modified by ...


#include "video_encoder_instance.h"
#include <sys/mount.h>

#include <nacl_io/nacl_io.h>

#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <ppapi/c/pp_errors.h>

#include <ppapi/utility/threading/simple_thread.h>

#define INSTANCE (*this)
#include "log.h"

#define FS_PATH "/persistent/"

VideoEncoderInstance::VideoEncoderInstance(PP_Instance instance,
		pp::Module* module) :
		pp::Instance(instance),
		muxer(*this),
#ifdef USING_AUDIO
		audio_enc(this, muxer),
#endif
		video_enc(this, muxer),
		handle(this),
		video_encoder_thread(handle),
		audio_encoder_thread(handle)
{
	InitializeFileSystem(FS_PATH);
}

VideoEncoderInstance::~VideoEncoderInstance() {
	umount(FS_PATH);
	nacl_io_uninit();

}

void VideoEncoderInstance::InitializeFileSystem(const std::string& fsPath) {

	int initRes = nacl_io_init_ppapi(pp_instance(),
			pp::Module::Get()->get_browser_interface());
	if (initRes != PP_OK) {
		LogError(initRes,"Ao inicializar nacl_io");
	}

	int mntRes = mount("", 	//src
			FS_PATH, 		//target
			"html5fs",		//filesystem type
			0, //flags
			"type=PERSISTENT,expected_size=1073741824" //html5 stuff
			);
	if (mntRes != 0) {
		LogError(mntRes,"Ao montar sistema de arquivos ");
	}

}


pp::Resource* _track_res;
pp::Size _video_size;
PP_VideoProfile _video_profile;
VideoEncoder* _video_enc;
void VideoEncoderWorker(void* params, int result);
#ifdef USING_AUDIO
	AudioEncoder* _audio_enc;
	void AudioEncoderWorker(void* params, int result);
#endif

void VideoEncoderInstance::HandleMessage(const pp::Var& var_message) {

	if (!var_message.is_dictionary()) {
		LogError(pp::Var(PP_ERROR_BADARGUMENT).DebugString(), "Mensagem inválida!");
		return;
	}

	pp::VarDictionary dict_message(var_message);
	std::string command = dict_message.Get("command").AsString();
	Log("Comando recebido:" << command);
	Log("mensagem: " << var_message.AsString());
	if (command == "start") {

		pp::Size requested_size = pp::Size(dict_message.Get("width").AsInt(),
				dict_message.Get("height").AsInt());

		pp::Var var_video_track = dict_message.Get("video_track");
		if (!var_video_track.is_resource()) {
			LogError(PP_ERROR_BADARGUMENT, "video_track não é um resource");
			return;
		}
		track_res[0] = var_video_track.AsResource();
#ifdef USING_AUDIO
		pp::Var var_audio_track = dict_message.Get("audio_track");
		if (!var_audio_track.is_resource()) {
			LogError(PP_ERROR_BADARGUMENT, "audio_track não é um resource");
			return;
		}

		track_res[1] = var_audio_track.AsResource();
#endif

		_track_res = track_res;
		_video_profile = PP_VIDEOPROFILE_VP8_ANY;
		_video_size  = requested_size;
		_video_enc = &video_enc;

		file_name = dict_message.Get("file_name").AsString();
		muxer.SetFileName(file_name);
		video_encoder_thread.Start();
		pp::CompletionCallback video_callback(&VideoEncoderWorker,0);
		video_encoder_thread.message_loop().PostWork(video_callback,0);

		Log("Comando executado com sucesso");
#ifdef USING_AUDIO
		_audio_enc = &audio_enc;

		audio_encoder_thread.Start();
		pp::CompletionCallback audio_callback(&AudioEncoderWorker,0);
		audio_encoder_thread.message_loop().PostWork(audio_callback,0);
#endif

	} else if (command == "stop") {
		Log("Parando encode...");
		video_enc.StopEncode();
#ifdef USING_AUDIO
		audio_enc.Stop();
#endif
		Log("Comando executado com sucesso");
	} else {
		LogError(PP_ERROR_BADARGUMENT,"Comando inválido!");
	}
}
pp::SimpleThread& VideoEncoderInstance::encoderThread(){
	return video_encoder_thread;
}

void VideoEncoderWorker(void* params, int result){
	_video_enc->Encode(_video_size,_track_res[0],_video_profile);
}

void AudioEncoderWorker(void* params, int result){
#ifdef USING_AUDIO
	_audio_enc->Start(_track_res[1]);
#endif
}

//
//void VideoEncoderInstance::EncodeWorker(int, pp::Size video_size, PP_VideoProfile video_profile) {
//	Log("Starting encoder...");
//	video_enc.Encode(video_size,track_res[0],video_profile);
//	audio_enc.Start(track_res[1]);
//
//}

