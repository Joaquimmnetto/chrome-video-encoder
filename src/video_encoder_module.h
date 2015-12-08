/*
 * videoencodermodule.h
 *
 *  Created on: 19 de nov de 2015
 *      Author: joaquim
 */

#ifndef VIDEOENCODERMODULE_H_
#define VIDEOENCODERMODULE_H_

#include <ppapi/cpp/module.h>
#include <ppapi/c/pp_instance.h>

class VideoEncoderModule: public pp::Module {
public:
	VideoEncoderModule() : pp::Module() {}
	virtual ~VideoEncoderModule() {}

	virtual pp::Instance* CreateInstance(PP_Instance instance);
};


#endif /* VIDEO_ENCODER_MODULE_H_ */
