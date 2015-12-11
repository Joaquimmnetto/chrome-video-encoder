/*
 * videoencodermodule.cpp
 *
 *  Created on: 19 de nov de 2015
 *      Author: joaquim
 */

#include "video_encoder_module.h"

#include "video_encoder_instance.h"


pp::Instance* VideoEncoderModule::CreateInstance(PP_Instance instance) {
	pp::Instance* inst = new VideoEncoderInstance(instance, this);

	inst->LogToConsole(PP_LOGLEVEL_LOG,"Instância criada");
	return inst;
}

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
	return new VideoEncoderModule();
}

}
