/*
 * VideoRecorderInstance.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */

#ifndef VIDEORECORDERINSTANCE_H_
#define VIDEORECORDERINSTANCE_H_

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_codecs.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>

#include <ppapi/utility/threading/simple_thread.h>

#include "VideoRecorder.h"


class VideoRecorderInstance : public pp::Instance{
public:

	VideoRecorder* recorder;

	VideoRecorderInstance(PP_Instance instance);
	virtual void HandleMessage(const pp::Var& var_message);

	pp::SimpleThread& getThread() ;

	virtual ~VideoRecorderInstance();

private:
	pp::SimpleThread thread;

//	PP_VideoProfile toPPProfile(std::string profileStr);
};

#endif /* VIDEORECORDERINSTANCE_H_ */
