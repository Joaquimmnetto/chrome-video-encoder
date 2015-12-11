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
		handle(this),
#ifdef USING_AUDIO
		audio_enc(0),
#endif
		video_enc(0),
		muxer(0),
		video_encoder_thread(handle),
		audio_encoder_thread(handle)
{
	InitializeFileSystem(FS_PATH);
}

VideoEncoderInstance::~VideoEncoderInstance() {
	umount(FS_PATH);
	nacl_io_uninit();
	delete_and_nulify(video_enc);
#ifdef USING_AUDIO
	delete_and_nulify(audio_enc);
#endif
	delete_and_nulify(muxer);

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


pp::Resource _video_track_res;
pp::Size _video_size;
PP_VideoProfile _video_profile;
VideoEncoder* _video_enc = 0;

void VideoEncoderWorker(void* params, int result);
#ifdef USING_AUDIO
	pp::Resource _audio_track_res;
	AudioEncoder* _audio_enc;
	void AudioEncoderWorker(void* params, int result);
#endif

void VideoEncoderInstance::HandleMessage( const pp::Var& var_message ) {

	if (!var_message.is_dictionary()) {
		LogError(pp::Var(PP_ERROR_BADARGUMENT).DebugString(), "Mensagem inválida!");
		return;
	}

	pp::VarDictionary dict_message(var_message);
	std::string command = dict_message.Get("command").AsString();
	Log("Comando recebido:" << command);


	if (command == "start") {
		if(video_enc && video_enc->is_encoding()){
			Log("Encode já em execução, chame stop antes de iniciar novamente.");
			return;
		}

		muxer = new WebmMuxer(*this);
		video_enc = new VideoEncoder(this, *muxer);
#ifdef USING_AUDIO
		audio_enc = new AudioEncoder(this, *muxer);
#endif

		pp::Size requested_size = pp::Size(dict_message.Get("width").AsInt(),
				dict_message.Get("height").AsInt());


		pp::Var var_video_track = dict_message.Get("video_track");
		if (!var_video_track.is_resource()) {
			LogError(PP_ERROR_BADARGUMENT, "video_track não é um resource");
			return;
		}

		video_track_res = var_video_track.AsResource();

#ifdef USING_AUDIO
		pp::Var var_audio_track = dict_message.Get("audio_track");
		if (!var_audio_track.is_resource()) {
			LogError(PP_ERROR_BADARGUMENT, "audio_track não é um resource");
			return;
		}

		audio_track_res = var_audio_track.AsResource();
#endif

		//Variáveis tem que serem passadas assim porque o callback da PPAPI não suporta tantos argumentos(máximo de 3).
		_video_track_res = video_track_res;
		_video_profile = PP_VIDEOPROFILE_VP8_ANY;
		_video_size  = requested_size;
		_video_enc = video_enc;

		file_name = dict_message.Get("file_name").AsString();

		Log("Argumentos:");
		Log("Encoder: VP8");
		Log("Video Size: (" << _video_size.width() << "," << _video_size.height() <<")");
		Log("File name:" << file_name);



		muxer->SetFileName(file_name);

		video_encoder_thread.Start();
		pp::CompletionCallback video_callback(&VideoEncoderWorker,0);
		video_encoder_thread.message_loop().PostWork(video_callback,0);


#ifdef USING_AUDIO
		_audio_enc = &audio_enc;

		audio_encoder_thread.Start();
		pp::CompletionCallback audio_callback(&AudioEncoderWorker,0);
		audio_encoder_thread.message_loop().PostWork(audio_callback,0);
#endif
	Log("Comando executado com sucesso");
	}
	else if(command == "change_track"){
		pp::Var new_vid_track = dict_message.Get("video_track");
		if(!new_vid_track.is_resource()){
			Log("video_track não é um recurso válido");
			return;
		}
		video_enc->SetTrack(new_vid_track.AsResource());
	}
	else if (command == "stop") {
		if(!video_enc){
			Log("Inicie um encode antes de tentar parar!");
			return;
		}
		if(!video_enc->is_encoding()){
			Log("Encoder já parado");
		}
		else{
			Log("Parando encode...");
			video_enc->StopEncode();
#ifdef USING_AUDIO
			audio_enc.Stop();
			delete_and_nulify(audio_enc);
#endif
			delete_and_nulify(video_enc);
			delete_and_nulify(muxer);
		}
		Log("Comando executado com sucesso");
	} else {
		LogError(PP_ERROR_BADARGUMENT,"Comando inválido!");
	}

}


void VideoEncoderWorker(void* params, int result){
	_video_enc->Encode(_video_size,_video_track_res,_video_profile);
}

void AudioEncoderWorker(void* params, int result){
#ifdef USING_AUDIO
	_audio_enc->Start(_audio_track_res);
#endif
}
