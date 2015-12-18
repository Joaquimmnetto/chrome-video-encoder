/*
 * video_encoder_instance.h
 *
 *  Created on: 18 de nov de 2015
 *      Author: joaquim
 */

#ifndef VIDEOENCODERINSTANCE_H_
#define VIDEOENCODERINSTANCE_H_

#include <string>
#include <vector>
#include <deque>

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/instance_handle.h>
#include <ppapi/cpp/module.h>
#include <ppapi/c/pp_codecs.h>
#include <ppapi/utility/threading/simple_thread.h>

#include "libwebm/mkvmuxer.hpp"
#include "libwebm/mkvwriter.hpp"

#include "video_encoder.h"
#include "audio_encoder.h"
#include "tipos.h"

class VideoEncoderInstance: public pp::Instance {
public:
	VideoEncoderInstance(PP_Instance instance, pp::Module* module);
	virtual ~VideoEncoderInstance();

	virtual void HandleMessage(const pp::Var& var_message);

	inline pp::SimpleThread& encoderThread(){	return video_encoder_thread; }

private:
	/**Inicializa o sistema de arquivos do HTMl5*/
	void InitializeFileSystem(const std::string& fsPath);
	/**Método utilizado como ponto de entrada de video_encoder_thread*/
	void EncodeWorker(int, pp::Size video_size, PP_VideoProfile video_profile);

	/**Necessário para a inicialização das threads*/
	pp::InstanceHandle handle;
	//Se definida, permite o uso do encoder de áudio, que não está funcionando atualmente.
#ifdef USING_AUDIO
	AudioEncoder* audio_enc;
#endif

	/**Encoder de vídeo*/
	VideoEncoder* video_enc;
	/**Muxer que será usado para criar o arquivo de video(ou video+audio) resultante*/
	WebmMuxer* muxer;
	/**Thread em que o encoding de vídeo roda*/
	pp::SimpleThread video_encoder_thread;
	/**Thread em que o encoding de áudio roda*/
	pp::SimpleThread audio_encoder_thread;
	/**Nome do arquivo a ser salvo pelo muxer*/
	std::string file_name;

	std::vector<pp::Resource> video_track_res;

	pp::Resource audio_track_res;
};

#endif /* VIDEO_ENCODER_INSTANCE_H_ */
