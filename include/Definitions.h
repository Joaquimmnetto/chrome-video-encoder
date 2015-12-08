/*
 * Definitions.h
 *
 *  Created on: 23 de out de 2015
 *      Author: joaquim
 */
#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_
//#ifdef SET_CALLBACK
//	#undef SET_CALLBACK
//#endif
//
//#ifdef CALLBACK
//	#undef CALLBACK
//#endif
//
//#ifdef CALLBACK_OUT
//	#undef CALLBACK_OUT
//#endif
//
//#ifdef CALLBACK_OUT_2
//	#undef CALLBACK_OUT_2
//#endif

#include <ppapi/utility/completion_callback_factory.h>
#include <sstream>
//#include "../include/VideoRecorderModule.h"

std::ostringstream __logStream;
#endif

#define CALLBACK_NAME callbackFactory
#define CREATE_CALLBACK(clazz) pp::CompletionCallbackFactory<clazz> CALLBACK_NAME
#define INIT_CALLBACK CALLBACK_NAME(this)
#define CALLBACK callbackFactory.NewCallback
#define CALLBACK_OUT callbackFactory.NewCallbackWithOutput
#define INSTANCE VideoRecorderModule::getInstance()
#define LOG(exp) __logStream.str(""); __logStream << exp; INSTANCE.LogToConsole(PP_LOGLEVEL_LOG,__logStream.str())


