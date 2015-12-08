/*
 * FileSystem.h
 *
 *  Created on: 27 de out de 2015
 *      Author: joaquim
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <vector>
#include <string>

#include <ppapi/cpp/file_io.h>
#include <ppapi/cpp/file_ref.h>
#include <ppapi/cpp/file_system.h>
#include <ppapi/cpp/instance_handle.h>

#include <ppapi/utility/threading/simple_thread.h>
#include <ppapi/utility/completion_callback_factory.h>


class FileSystem {
public:
	FileSystem(pp::InstanceHandle& _instance);

	void open();
	void seekTo(int64_t position);
	int64_t getPosition() const;
	void write(const std::string& file, const std::string& content);
	void write(const std::string& file, const char* content, uint32_t size, bool ownership = false);
	std::vector<char> read(std::string file);

	int getLastResult() const;

	virtual ~FileSystem();

private:
	pp::InstanceHandle& iHandle;
	pp::FileSystem fileSystem;
	pp::SimpleThread fileThread;
	pp::CompletionCallbackFactory<FileSystem> callbackFactory;

	bool threadStarted;
	bool systemOpen;
	int lastResult;

	int64_t offset;

	void _open(int);
	void _write(int,const std::string& file, const char* content, uint32_t size);
	int prepareFile(const std::string& file, pp::FileIO* fileIO, PP_FileInfo* info);
	void deleteCallback(int, const char* memory);

};

#endif /* FILESYSTEM_H_ */
