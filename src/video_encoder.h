/*
 * video_encoder_instance.h
 *
 *  Created on: 18 de nov de 2015
 *      Author: joaquim
 */

#ifndef VIDEOENCODER_H_
#define VIDEOENCODER_H_

#include <string>
#include <vector>
#include <deque>

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/instance_handle.h>

#include <ppapi/cpp/media_stream_video_track.h>
#include <ppapi/cpp/video_frame.h>
#include <ppapi/cpp/video_encoder.h>

#include <ppapi/utility/threading/simple_thread.h>

#include <ppapi/utility/completion_callback_factory.h>

#include "libwebm/mkvmuxer.hpp"
#include "libwebm/mkvwriter.hpp"

#include "webm_muxer.h"

/**
 * Classe que codifica os frames recebidos do navegador. Têm três fluxos principais: Trilha de frames, Encoder, e BitstreamBuffer.
 *
 * A trilha de frames mostra os frames obtidos do navegador, que são utilizados pelo Encoder. Este quando termina seu serviço, envia
 * os frames para o BitstreamBuffer para serem devidamente tratados.
 *
 *Ps: A organização poderia estar melhor, eu tentei separar tudo em classses distintas e tal, mas o encoder simplesmente recusa-se a funcionar
 *		com um código organizado.
 *
 * */
class VideoEncoder {
public:
	/**
	 * Inicializa um encoder novo
	 *
	 * @param instance - instância do NaCl em que esse encoder está rodando
	 * @param muxer - WebmMuxer que será usado para criar o arquivo .webm correspondente ao vídeo
	 */
	VideoEncoder(pp::Instance* instance, WebmMuxer& muxer);
	virtual ~VideoEncoder();
	/**
	 * Inicia o encoding do video.
	 *
	 * @param requested_size -
	 * @param resource_track - Objeto pp::Resource da trilha que contêm os frames a serem encodados.
	 * @param video_profile - PP_VideoProfile especificando qual será o tipo do encoder.
	 */
	void Encode(pp::Size requested_size,
			pp::Resource resource_track, PP_VideoProfile video_profile);
	/**
	 * Encerra o encode, fechando a track associada e criando o arquivo .webm do muxer associado.
	 */
	void StopEncode();
private:
	/**
	 * Faz a pré inicialização do encoder, verificando quais são os profiles de encoding disponíveis. Necessário para a inicialização do encoder.
	 */
	void ProbeEncoder();
	/**
	 * Callback de ProbeEncoder(), possibilita o tratamento dos profiles de encoding recebidos.
	 */
	void OnEncoderProbed(int32 result,
			const std::vector<PP_VideoProfileDescription> profiles);
	/**
	 * Inicializa a trilha associada ao encoder.
	 */
	void ConfigureTrack();
	/**
	 * Callback de ConfigureTrack(), chama StartEncoder()
	 */
	void OnConfiguredTrack(int32 result);
	/**
	 * Inicializa o encoder.
	 */
	void StartEncoder();
	/**
	 * callback de StartEncoder(), chama o BitstreamBuffer, e inicializa os loops captura de frames e de encoding,
	 *  chamando respectivamente StartTrackFrames() e ScheduleNextEncode().
	 */
	void OnInitializedEncoder(int32 result);
	/**
	 * Loop de encoding, agenda GetEncoderFrameTick a uma taxa fixa (1/30).
	 */
	void ScheduleNextEncode();
	/**
	 * Chamado por ScheduleNextEncode(), pega frame da track e chama o próximo método do fluxo, GetEncoderFrame()
	 */
	void GetEncoderFrameTick(int32 result);
	/**
	 * Cria um frame vazio para ser preenchido com as informações encodadas;
	 */
	void GetEncoderFrame(const pp::VideoFrame& track_frame);
	/**
	 * Callback de GetEncoderFrame(), prepara o frame do encoder(encoder_frame) para ser encodado.
	 */
	void OnEncoderFrame(int32 result, pp::VideoFrame encoder_frame,
			pp::VideoFrame track_frame);
	/**
	 * Método de utilidade usado por OnEncoderFrame. Copia o objeto pp::VideoFrame em src para dest.
	 */
	int32 CopyVideoFrame(pp::VideoFrame dest, pp::VideoFrame src);
	/**
	 * Finalmente, realiza o encoding do frame preparado, e armazena sua timestamp para ser usada no processo de muxagem.
	 */
	void EncodeFrame(const pp::VideoFrame& frame);
	/**
	 * Callback de EncodeFrame, verifica se o processo de encoding foi feito com sucesso.
	 */
	void OnEncodeDone(int32 result);
	/**
	 * Metodo de loop do BitstreamBuffer. Este buffer é preenchido por frames encodados por EncodeFrame(). Este método pega estes frames
	 * e os passa ao WebmMuxer para a criação do arquivo .webm de vídeo.
	 */
	void OnGetBitstreamBuffer(int32 result, PP_BitstreamBuffer buffer);
	/**
	 * Inicia o loop da track de frames. O Encoder puxará frames daqui para encodar.
	 */
	void StartTrackFrames();
	/**
	 * Encerra o loop da track de frames.
	 */
	void StopTrackFrames();
	/**
	 * Preenche current_track_frame e recicla os frames.
	 */
	void OnTrackFrame(int32 result, pp::VideoFrame frame);

	void EncodeWorker(int result);


	/**Instância atual utilizada pelo NaCl*/
	pp::Instance* instance;
	/**Handle desta instância, necessário para inicializar o encoder da PPAPI*/
	pp::InstanceHandle handle;
	/**Muxer Webm para criação do arquivo .webm*/
	WebmMuxer& muxer;
	/**Tipo do encoder que será usado(Padrão VP8)*/
	PP_VideoProfile video_profile;
	/**Formato da imagem do frame (Padrão I420)*/
	PP_VideoFrame_Format frame_format;
	/**Tamanho passado pelo usuário para ser utilizado no encoding*/
	pp::Size requested_size;
	/**Tamanho do frame da track de frames*/
	pp::Size frame_size;
	/**Tamanho do frame a ser passado para o encoder*/
	pp::Size encoder_size;
	/**Objeto encoder da PPAPI*/
	pp::VideoEncoder video_encoder;
	/**Objeto trilha da PPAPI*/
	pp::MediaStreamVideoTrack video_track;
	/**Factory necessária para a criação de callbacks*/
	pp::CompletionCallbackFactory<VideoEncoder> callback_factory;

	/**Frame atual da trilha, é utilizado como entrada pelo encoder*/
	pp::VideoFrame current_track_frame;

	/**Se o encoder está ativo*/
	bool is_encoding;
	/**Se há um frame sendo encodado neste momento*/
	bool is_encode_ticking;
	/**Se a track está ativa*/
	bool is_receiving_track_frames;

	/**Timestamp indicando o tempo do último encode*/
	PP_Time last_encode_tick_;
	/**Fila com as timestamps dos frames encodados. São retirados pelo BitstreamBuffer para serem passadas ao muxer.*/
	std::deque<uint64> frames_timestamps;

};

#endif /* VIDEO_ENCODER_INSTANCE_H_ */