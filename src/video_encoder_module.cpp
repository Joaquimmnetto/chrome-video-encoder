/*
 * videoencodermodule.cpp
 *
 *  Created on: 19 de nov de 2015
 *      Author: joaquim
 */

#include "video_encoder_module.h"

#include "video_encoder_instance.h"


pp::Instance* VideoEncoderModule::CreateInstance(PP_Instance instance) {
	return new VideoEncoderInstance(instance, this);
}

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
	return new VideoEncoderModule();
}

}
