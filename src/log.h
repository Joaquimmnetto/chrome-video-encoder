#ifndef LOG_H
#define LOG_H

#define LOG_FRAMES 1

#ifndef INSTANCE
	#define INSTANCE
#endif

#include <sstream>
#include <ppapi/cpp/var.h>


std::stringstream __logStream;
#define Log(exp) __logStream.str(std::string()); __logStream << "Em " << __FILE__ <<":"<< __LINE__ <<" - " << exp; INSTANCE.LogToConsole(PP_LOGLEVEL_LOG,__logStream.str())
#define LogError(error, exp) __logStream.str(std::string()); __logStream << "Em " << __FILE__ <<":"<< __LINE__ <<" - " << "Erro: "<< error << " : " << exp; INSTANCE.LogToConsole(PP_LOGLEVEL_ERROR,__logStream.str())

#endif
//chrome-extension://iifpnkedoaiclkodeheciokndhgaalhj/test-multi-track.html
