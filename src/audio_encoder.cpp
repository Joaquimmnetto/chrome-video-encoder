/*
 * AudioEncoder.cpp
 *
 *  Created on: 20 de nov de 2015
 *      Author: joaquim
 */
#ifdef USING_AUDIO
#include "audio_encoder.h"
#include <ppapi/c/ppb_audio_buffer.h>
#include <ppapi/c/ppb_media_stream_audio_track.h>
#include "libwebm/mkvmuxer.hpp"
#include <cmath>

#define INSTANCE (*instance)
#include "log.h"

#define USING_PPB_C_API 1

float Pcm16ToFloat(int16 pcm) {
	return pcm / 32768.0f;
}

AudioEncoder::AudioEncoder(pp::Instance* _instance, WebmMuxer& _muxer) :
		instance(_instance), muxer(_muxer), callback_fact(this), sample_rate(0), channels(
				0), initialized(false), stopped(false) {
}

AudioEncoder::~AudioEncoder() {
}

int AudioEncoder::Init(long _channels, long _sample_rate) {
	if (initialized) {
		return true;
	}

	Log("Initializing audio");
	vorbis_info_init(&v_info);

	this->channels = _channels;
	this->sample_rate = _sample_rate;
	Log("Channels " << channels << ", " << "Sample rate " << sample_rate);

	muxer.ConfigureAudio(sample_rate, channels);

	int result = vorbis_encode_init_vbr(&v_info, channels, sample_rate, 0.4);
	if (result < 0) {
		Log("Falha na inicialização do vorbis");
		return result;
	}
	result = ogg_stream_init(&o_stream, 1);
	if (result < 0) {
		Log("Falha na inicialização do stream ogg");
		return result;
	}

	result = vorbis_analysis_init(&v_dsp, &v_info);
	if (result < 0) {
		Log("Falha na inicialização do encoder vorbis");
		return result;
	}
	vorbis_block_init(&v_dsp, &v_block);

	initialized = true;
	Log("Initialized with success");
	return true;
}

void callback(void* data, int32 res) {
}

void AudioEncoder::Start(pp::Resource& _track) {
	Log("Audio Encoder started");

#ifdef USING_PPB_C_API
	PPB_MediaStreamAudioTrack* c_track =
			(PPB_MediaStreamAudioTrack*) pp::Module::Get()->GetBrowserInterface(
			PPB_MEDIASTREAMAUDIOTRACK_INTERFACE);
	PPB_AudioBuffer* c_buffer =
			(PPB_AudioBuffer*) pp::Module::Get()->GetBrowserInterface(
			PPB_AUDIOBUFFER_INTERFACE);

	CEncodeLoop(c_track, _track.pp_resource(), c_buffer);

#else
	this->track = pp::MediaStreamAudioTrack(_track);
	pp::CompletionCallbackWithOutput<pp::AudioBuffer> cb = callback_fact.NewCallbackWithOutput<pp::AudioBuffer>(&AudioEncoder::OnBuffer);
	track.GetBuffer(cb);
#endif
}

void AudioEncoder::Stop() {
	stopped = true;
}

void AudioEncoder::CEncodeLoop(PPB_MediaStreamAudioTrack* c_track,
		PP_Resource c_track_res, PPB_AudioBuffer* c_buffer) {

	PP_Resource c_buffer_res;
	int result = c_track->GetBuffer(c_track_res, &c_buffer_res,
			PP_BlockUntilComplete());
	if (result != PP_OK) {
		Log("Falha resgatando o buffer " << result);
		return;
	}

	if (!Init(c_buffer->GetNumberOfChannels(c_buffer_res),
			c_buffer->GetSampleRate(c_buffer_res))) {
		Log("Falha na inicialização do vorbis");
		return;
	}

	if (stopped) {
		Log("Parando encode de audio...");
		Clear();
		return;
	}

	int16* data = static_cast<int16*>(c_buffer->GetDataBuffer(c_buffer_res));
	uint32 data_size = c_buffer->GetDataBufferSize(c_buffer_res);

	Encode(data, data_size);

	c_track->RecycleBuffer(c_track_res, c_buffer_res);
	this->CEncodeLoop(c_track, c_track_res, c_buffer);

}

//Tentar fazer os pacotes ogg/vorbis no js, e so os enviar pro modulo.
void AudioEncoder::OnBuffer(int result, pp::AudioBuffer pp_buffer) {
	Log("OnBuffer:");
	if (result != PP_OK) {
		return;
	}
	this->Init(pp_buffer.GetNumberOfChannels(), pp_buffer.GetSampleRate());

	if (stopped) {
		Log("Parando encode de audio...");
		Clear();
		return;
	}

	int16* data = static_cast<int16*>(pp_buffer.GetDataBuffer());
	uint32 data_size = pp_buffer.GetDataBufferSize();
	Log("Calling encode...");
	Encode(data, data_size);
	Log("Encode finalized");
	track.RecycleBuffer(pp_buffer);
	track.GetBuffer(
			callback_fact.NewCallbackWithOutput(&AudioEncoder::OnBuffer));

}

void AudioEncoder::Encode(int16* data, uint32 data_size) {

	Log("data_size " << data_size);
	float** buffer = vorbis_analysis_buffer(&v_dsp, data_size);

	Log("Convertendo dados para float");


	///TODO: Bug aqui: por algum motivo, segmentation fault após a segunda run de encode.
	///Não é relacionado com data_size, aparentememente buffer não está sendo retornado com o tamanho esperado( data_size ).
	long smpl = 0;
	while (smpl < data_size) {
		for (int c = 0; c < channels; c++) {
			float value = Pcm16ToFloat(data[smpl]);
			Log( value );
			buffer[c][smpl] = value;
			smpl++;
		}
	}

	//marca os buffers reservados como preenchidos e prontos para serem comprimidos.
	int res = -1;
	Log("written samples " << smpl);
	res = vorbis_analysis_wrote(&v_dsp, smpl);
	Log("vorbis_analysis_wrote " << res);

	//Parte o input enviado em blocos e os 'cospe' enquanto houver blocos
	while ((res = vorbis_analysis_blockout(&v_dsp, &v_block))) {
		Log("Encoding block " << v_block.granulepos);
		res = vorbis_analysis(&v_block, 0);
		Log("vorbis_analysis " << res);
		res = vorbis_bitrate_addblock(&v_block);
		Log("vorbis_bitrate_addblock " << res);

		while ((res = vorbis_bitrate_flushpacket(&v_dsp, &o_pack))) {
			uint64 tcm_pos = o_pack.granulepos;
			uint64 timestamp = (((double) tcm_pos) / ((double) sample_rate))
					* pow(10.f, 9.f);
			Log("pushing audio frame " << timestamp);
			muxer.PushAudioFrame(o_pack.packet, o_pack.bytes, timestamp);
		}
		Log("vorbis_bitrate_flushpacket " << res);
	}

	Log("vorbis_analysis_blockout " << res);
}

void AudioEncoder::Clear() {
	ogg_stream_clear(&o_stream);
	vorbis_block_clear(&v_block);
	vorbis_dsp_clear(&v_dsp);
//	  vorbis_comment_clear(&v_c);
	vorbis_info_clear(&v_info);
}

#endif
