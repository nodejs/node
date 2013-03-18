// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include <stdarg.h>
#include <limits.h>

#include "v8.h"

#include "conversions-inl.h"
#include "v8conversions.h"
#include "dtoa.h"
#include "factory.h"
#include "strtod.h"

namespace v8 {
namespace internal {

namespace {

// C++-style iterator adaptor for StringCharacterStream
// (unlike C++ iterators the end-marker has different type).
class StringCharacterStreamIterator {
 public:
  class EndMarker {};

  explicit StringCharacterStreamIterator(StringCharacterStream* stream);

  uint16_t operator*() const;
  void operator++();
  bool operator==(EndMarker const&) const { return end_; }
  bool operator!=(EndMarker const& m) const { return !end_; }

 private:
  StringCharacterStream* const stream_;
  uint16_t current_;
  bool end_;
};


StringCharacterStreamIterator::StringCharacterStreamIterator(
    StringCharacterStream* stream) : stream_(stream) {
  ++(*this);
}

uint16_t StringCharacterStreamIterator::operator*() const {
  return current_;
}


void StringCharacterStreamIterator::operator++() {
  end_ = !stream_->HasMore();
  if (!end_) {
    current_ = stream_->GetNext();
  }
}
}  // End anonymous namespace.


double StringToDouble(UnicodeCache* unicode_cache,
                      String* str, int flags, double empty_string_val) {
  StringShape shape(str);
  // TODO(dcarney): Use a Visitor here.
  if (shape.IsSequentialAscii()) {
    const uint8_t* begin = SeqOneByteString::cast(str)->GetChars();
    const uint8_t* end = begin + str->length();
    return InternalStringToDouble(unicode_cache, begin, end, flags,
                                  empty_string_val);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToDouble(unicode_cache, begin, end, flags,
                                  empty_string_val);
  } else {
    ConsStringIteratorOp op;
    StringCharacterStream stream(str, &op);
    return InternalStringToDouble(unicode_cache,
                                  StringCharacterStreamIterator(&stream),
                                  StringCharacterStreamIterator::EndMarker(),
                                  flags,
                                  empty_string_val);
  }
}


double StringToInt(UnicodeCache* unicode_cache,
                   String* str,
                   int radix) {
  StringShape shape(str);
  // TODO(dcarney): Use a Visitor here.
  if (shape.IsSequentialAscii()) {
    const uint8_t* begin = SeqOneByteString::cast(str)->GetChars();
    const uint8_t* end = begin + str->length();
    return InternalStringToInt(unicode_cache, begin, end, radix);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToInt(unicode_cache, begin, end, radix);
  } else {
    ConsStringIteratorOp op;
    StringCharacterStream stream(str, &op);
    return InternalStringToInt(unicode_cache,
                               StringCharacterStreamIterator(&stream),
                               StringCharacterStreamIterator::EndMarker(),
                               radix);
  }
}

} }  // namespace v8::internal
