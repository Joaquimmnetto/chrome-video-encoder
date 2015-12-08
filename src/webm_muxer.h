/*
 * WebmMuxer.h
 *
 *  Created on: 24 de nov de 2015
 *      Author: joaquim
 */

#ifndef WEBM_MUXER_H_
#define WEBM_MUXER_H_

#include <string>
#include <deque>

#include <ppapi/cpp/instance.h>
#include <ppapi/utility/threading/simple_thread.h>

#include "defines.h"

#include "libwebm/mkvmuxer.hpp"
#include "libwebm/mkvwriter.hpp"

class WebmMuxer {
public:
	WebmMuxer(pp::Instance& instance);

	void ConfigureVideo( int video_width, int video_height );
	void ConfigureAudio( int audio_sample_rate, int audio_channels );

	bool PushAudioFrame(byte* data, uint32 length, uint64 timestamp);
	bool PushVideoFrame(byte* data, uint32 length, uint64 timestamp, bool key_frame);

	bool AddVideoFrame(byte* data, uint32 length, uint64 timestamp, bool key_frame);
	bool Finish();
	///Escreve todos os frames em arquivo. Certifique-se que tanto a gravação de áudio como a de vídeo foram devidamente finalizadas.
	bool WriteFrames(std::string file_name);

	inline void SetFileName(const std::string _file_name) {file_name = _file_name;}

	virtual ~WebmMuxer();
private:
	pp::Instance& instance;
	std::string file_name;

	mkvmuxer::Segment segment;
	mkvmuxer::MkvWriter writer;

	std::deque< mkvmuxer::Frame* > audio_queue;
	std::deque< mkvmuxer::Frame* > video_queue;

	int audio_track_num;
	int video_track_num;

	int video_width;
	int video_height;
	int audio_sample_rate;
	int audio_channels;

	bool initialized;
	bool finished;

	int Init( std::string file_name );
	mkvmuxer::Frame* createFrame( byte* data, uint32 length, uint64 timestamp, int track, bool key_frame );




};

#endif /* WEBM_MUXER_H_ */
