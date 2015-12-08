#ifdef USING_AUDIO
/*
 * AudioEncoder.h
 *
 *  Created on: 20 de nov de 2015
 *      Author: joaquim
 */

#ifndef AUDIO_ENCODER_H_
#define AUDIO_ENCODER_H_


#include "vorbis/vorbisenc.h"

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/media_stream_audio_track.h>
#include <ppapi/cpp/audio_buffer.h>
#include <ppapi/utility/completion_callback_factory.h>

#include "webm_muxer.h"
#include "defines.h"



class AudioEncoder {
public:
	AudioEncoder(pp::Instance* instance, WebmMuxer& muxer);
	void Start(pp::Resource& _track);
	void Stop();
	virtual ~AudioEncoder();

private:

	vorbis_info v_info;
	vorbis_dsp_state v_dsp;
//	vorbis_comment v_comment;
	vorbis_block v_block;

	ogg_stream_state o_stream;
	ogg_packet o_pack;

	pp::Instance* instance;
	WebmMuxer& muxer;
	pp::MediaStreamAudioTrack track;
	pp::CompletionCallbackFactory<AudioEncoder> callback_fact;

	long sample_rate;
	long channels;

	bool initialized;

	bool stopped;

	int Init(long _channels, long _sample_rate);
	void Clear();

	void CEncodeLoop(PPB_MediaStreamAudioTrack* c_track, PP_Resource c_track_res, PPB_AudioBuffer* c_buffer);
	void OnBuffer(int result, pp::AudioBuffer buffer);
	void Encode(int16* data, uint32 data_size);




};

#endif /* AUDIO_ENCODER_H_ */
#endif
