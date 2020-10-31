// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Adapted from the patch of kenton@google.com (Kenton Varda)
// See https://github.com/protocolbuffers/protobuf/pull/710 for details.

#ifndef GOOGLE_PROTOBUF_UTIL_DELIMITED_MESSAGE_UTIL_H__
#define GOOGLE_PROTOBUF_UTIL_DELIMITED_MESSAGE_UTIL_H__

#include <ostream>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {

// Write a single size-delimited message from the given stream. Delimited
// format allows a single file or stream to contain multiple messages,
// whereas normally writing multiple non-delimited messages to the same
// stream would cause them to be merged. A delimited message is a varint
// encoding the message size followed by a message of exactly that size.
//
// Note that if you want to *read* a delimited message from a file descriptor
// or istream, you will need to construct an io::FileInputStream or
// io::OstreamInputStream (implementations of io::ZeroCopyStream) and use the
// utility function ParseDelimitedFromZeroCopyStream(). You must then
// continue to use the same ZeroCopyInputStream to read all further data from
// the stream until EOF. This is because these ZeroCopyInputStream
// implementations are buffered: they read a big chunk of data at a time,
// then parse it. As a result, they may read past the end of the delimited
// message. There is no way for them to push the extra data back into the
// underlying source, so instead you must keep using the same stream object.
bool PROTOBUF_EXPORT SerializeDelimitedToFileDescriptor(
    const MessageLite& message, int file_descriptor);

bool PROTOBUF_EXPORT SerializeDelimitedToOstream(const MessageLite& message,
                                                 std::ostream* output);

// Read a single size-delimited message from the given stream. Delimited
// format allows a single file or stream to contain multiple messages,
// whereas normally parsing consumes the entire input. A delimited message
// is a varint encoding the message size followed by a message of exactly
// that size.
//
// If |clean_eof| is not NULL, then it will be set to indicate whether the
// stream ended cleanly. That is, if the stream ends without this method
// having read any data at all from it, then *clean_eof will be set true,
// otherwise it will be set false. Note that these methods return false
// on EOF, but they also return false on other errors, so |clean_eof| is
// needed to distinguish a clean end from errors.
bool PROTOBUF_EXPORT ParseDelimitedFromZeroCopyStream(
    MessageLite* message, io::ZeroCopyInputStream* input, bool* clean_eof);

bool PROTOBUF_EXPORT ParseDelimitedFromCodedStream(MessageLite* message,
                                                   io::CodedInputStream* input,
                                                   bool* clean_eof);

// Write a single size-delimited message from the given stream. Delimited
// format allows a single file or stream to contain multiple messages,
// whereas normally writing multiple non-delimited messages to the same
// stream would cause them to be merged. A delimited message is a varint
// encoding the message size followed by a message of exactly that size.
bool PROTOBUF_EXPORT SerializeDelimitedToZeroCopyStream(
    const MessageLite& message, io::ZeroCopyOutputStream* output);

bool PROTOBUF_EXPORT SerializeDelimitedToCodedStream(
    const MessageLite& message, io::CodedOutputStream* output);

}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_DELIMITED_MESSAGE_UTIL_H__
