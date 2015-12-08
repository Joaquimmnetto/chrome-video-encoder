/*
 * Utils.h
 *
 *  Created on: 26 de out de 2015
 *      Author: joaquim
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <ppapi/cpp/video_frame.h>
#include "VideoEncoder.h"
#include "VideoTrack.h"

int copyVideoFrame(pp::VideoFrame dest,pp::VideoFrame src);

bool waitTrackInit(const VideoTrack& track, int timeout);
bool waitEncoderInit(const VideoEncoder& encoder, int timeout);

#endif /* UTILS_H_ */
