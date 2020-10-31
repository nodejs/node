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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H__

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/stubs/status.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class ObjectWriter;

// An ObjectSource is anything that can write to an ObjectWriter.
// Implementation of this interface typically provide constructors or
// factory methods to create an instance based on some source data, for
// example, a character stream, or protobuf.
//
// Derived classes could be thread-unsafe.
class PROTOBUF_EXPORT ObjectSource {
 public:
  virtual ~ObjectSource() {}

  // Writes to the ObjectWriter
  virtual util::Status WriteTo(ObjectWriter* ow) const {
    return NamedWriteTo("", ow);
  }

  // Writes to the ObjectWriter with a custom name for the message.
  // This is useful when you chain ObjectSource together by embedding one
  // within another.
  virtual util::Status NamedWriteTo(StringPiece name,
                                      ObjectWriter* ow) const = 0;

 protected:
  ObjectSource() {}

 private:
  // Do not add any data members to this class.
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ObjectSource);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H__
