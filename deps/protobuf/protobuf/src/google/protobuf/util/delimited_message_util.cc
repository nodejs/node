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

#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/io/coded_stream.h>

namespace google {
namespace protobuf {
namespace util {

bool SerializeDelimitedToFileDescriptor(const MessageLite& message,
                                        int file_descriptor) {
  io::FileOutputStream output(file_descriptor);
  return SerializeDelimitedToZeroCopyStream(message, &output);
}

bool SerializeDelimitedToOstream(const MessageLite& message,
                                 std::ostream* output) {
  {
    io::OstreamOutputStream zero_copy_output(output);
    if (!SerializeDelimitedToZeroCopyStream(message, &zero_copy_output))
      return false;
  }
  return output->good();
}

bool ParseDelimitedFromZeroCopyStream(MessageLite* message,
                                      io::ZeroCopyInputStream* input,
                                      bool* clean_eof) {
  io::CodedInputStream coded_input(input);
  return ParseDelimitedFromCodedStream(message, &coded_input, clean_eof);
}

bool ParseDelimitedFromCodedStream(MessageLite* message,
                                   io::CodedInputStream* input,
                                   bool* clean_eof) {
  if (clean_eof != NULL) *clean_eof = false;
  int start = input->CurrentPosition();

  // Read the size.
  uint32 size;
  if (!input->ReadVarint32(&size)) {
    if (clean_eof != NULL) *clean_eof = input->CurrentPosition() == start;
    return false;
  }

  // Tell the stream not to read beyond that size.
  io::CodedInputStream::Limit limit = input->PushLimit(size);

  // Parse the message.
  if (!message->MergeFromCodedStream(input)) return false;
  if (!input->ConsumedEntireMessage()) return false;

  // Release the limit.
  input->PopLimit(limit);

  return true;
}

bool SerializeDelimitedToZeroCopyStream(const MessageLite& message,
                                        io::ZeroCopyOutputStream* output) {
  io::CodedOutputStream coded_output(output);
  return SerializeDelimitedToCodedStream(message, &coded_output);
}

bool SerializeDelimitedToCodedStream(const MessageLite& message,
                                     io::CodedOutputStream* output) {
  // Write the size.
  size_t size = message.ByteSizeLong();
  if (size > INT_MAX) return false;

  output->WriteVarint32(size);

  // Write the content.
  uint8* buffer = output->GetDirectBufferForNBytesAndAdvance(size);
  if (buffer != NULL) {
    // Optimization: The message fits in one buffer, so use the faster
    // direct-to-array serialization path.
    message.SerializeWithCachedSizesToArray(buffer);
  } else {
    // Slightly-slower path when the message is multiple buffers.
    message.SerializeWithCachedSizes(output);
    if (output->HadError()) return false;
  }

  return true;
}

}  // namespace util
}  // namespace protobuf
}  // namespace google
