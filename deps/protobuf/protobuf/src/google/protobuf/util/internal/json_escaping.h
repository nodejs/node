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

#ifndef GOOGLE_PROTOBUF_UTIL_INTERNAL__JSON_ESCAPING_H__
#define GOOGLE_PROTOBUF_UTIL_INTERNAL__JSON_ESCAPING_H__

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/bytestream.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class JsonEscaping {
 public:
  // The minimum value of a unicode high-surrogate code unit in the utf-16
  // encoding. A high-surrogate is also known as a leading-surrogate.
  // See http://www.unicode.org/glossary/#high_surrogate_code_unit
  static const uint16 kMinHighSurrogate = 0xd800;

  // The maximum value of a unicide high-surrogate code unit in the utf-16
  // encoding. A high-surrogate is also known as a leading-surrogate.
  // See http://www.unicode.org/glossary/#high_surrogate_code_unit
  static const uint16 kMaxHighSurrogate = 0xdbff;

  // The minimum value of a unicode low-surrogate code unit in the utf-16
  // encoding. A low-surrogate is also known as a trailing-surrogate.
  // See http://www.unicode.org/glossary/#low_surrogate_code_unit
  static const uint16 kMinLowSurrogate = 0xdc00;

  // The maximum value of a unicode low-surrogate code unit in the utf-16
  // encoding. A low-surrogate is also known as a trailing surrogate.
  // See http://www.unicode.org/glossary/#low_surrogate_code_unit
  static const uint16 kMaxLowSurrogate = 0xdfff;

  // The minimum value of a unicode supplementary code point.
  // See http://www.unicode.org/glossary/#supplementary_code_point
  static const uint32 kMinSupplementaryCodePoint = 0x010000;

  // The minimum value of a unicode code point.
  // See http://www.unicode.org/glossary/#code_point
  static const uint32 kMinCodePoint = 0x000000;

  // The maximum value of a unicode code point.
  // See http://www.unicode.org/glossary/#code_point
  static const uint32 kMaxCodePoint = 0x10ffff;

  JsonEscaping() {}
  virtual ~JsonEscaping() {}

  // Escape the given ByteSource to the given ByteSink.
  static void Escape(strings::ByteSource* input, strings::ByteSink* output);

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(JsonEscaping);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_INTERNAL__JSON_ESCAPING_H__
