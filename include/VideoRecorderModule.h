/*
 * VideoRecorderModule.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */

#ifndef VIDEORECORDERMODULE_H_
#define VIDEORECORDERMODULE_H_

#include <ppapi/cpp/module.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/utility/completion_callback_factory.h>



static pp::Instance* currentInstance = NULL;
static pp::InstanceHandle* handle;

class VideoRecorderModule : public pp::Module {
public:
	VideoRecorderModule();

	virtual pp::Instance* CreateInstance(PP_Instance instance);

	static pp::Instance& getInstance();
	static pp::InstanceHandle& getHandle();
	virtual ~VideoRecorderModule();

private:
};

#endif /* VIDEORECORDERMODULE_H_ */
