/*
 * PPApiMkvWriter.h
 *
 *  Created on: 5 de nov de 2015
 *      Author: joaquim
 */

#ifndef PPAPIMKVWRITER_H_
#define PPAPIMKVWRITER_H_

#include "../libwebm/mkvmuxer.hpp"
#include "../include/FileSystem.h"
#include <string>

class PPApiMkvWriter: public mkvmuxer::IMkvWriter {
public:

	PPApiMkvWriter(pp::InstanceHandle& handle, std::string filename);

	// Writes out |len| bytes of |buf|. Returns 0 on success.
	virtual int Write(const void* buf, uint32_t len);

	// Returns the offset of the output position from the beginning of the
	// output.
	virtual int64_t Position() const;

	// Set the current File position. Returns 0 on success.
	virtual int Position(int64_t position);

	// Returns true if the writer is seekable.
	virtual bool Seekable() const;

	// Element start notification. Called whenever an element identifier is about
	// to be written to the stream. |element_id| is the element identifier, and
	// |position| is the location in the WebM stream where the first octet of the
	// element identifier will be written.
	// Note: the |MkvId| enumeration in webmids.hpp defines element values.
	virtual void ElementStartNotify(uint64_t element_id, int64_t position);

	virtual ~PPApiMkvWriter();

private:
	std::string filename;

	FileSystem fs;

};

#endif /* PPAPIMKVWRITER_H_ */
