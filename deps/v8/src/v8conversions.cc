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

// C++-style iterator adaptor for StringInputBuffer
// (unlike C++ iterators the end-marker has different type).
class StringInputBufferIterator {
 public:
  class EndMarker {};

  explicit StringInputBufferIterator(StringInputBuffer* buffer);

  int operator*() const;
  void operator++();
  bool operator==(EndMarker const&) const { return end_; }
  bool operator!=(EndMarker const& m) const { return !end_; }

 private:
  StringInputBuffer* const buffer_;
  int current_;
  bool end_;
};


StringInputBufferIterator::StringInputBufferIterator(
    StringInputBuffer* buffer) : buffer_(buffer) {
  ++(*this);
}

int StringInputBufferIterator::operator*() const {
  return current_;
}


void StringInputBufferIterator::operator++() {
  end_ = !buffer_->has_more();
  if (!end_) {
    current_ = buffer_->GetNext();
  }
}
}  // End anonymous namespace.


double StringToDouble(UnicodeCache* unicode_cache,
                      String* str, int flags, double empty_string_val) {
  StringShape shape(str);
  if (shape.IsSequentialAscii()) {
    const char* begin = SeqOneByteString::cast(str)->GetChars();
    const char* end = begin + str->length();
    return InternalStringToDouble(unicode_cache, begin, end, flags,
                                  empty_string_val);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToDouble(unicode_cache, begin, end, flags,
                                  empty_string_val);
  } else {
    StringInputBuffer buffer(str);
    return InternalStringToDouble(unicode_cache,
                                  StringInputBufferIterator(&buffer),
                                  StringInputBufferIterator::EndMarker(),
                                  flags,
                                  empty_string_val);
  }
}


double StringToInt(UnicodeCache* unicode_cache,
                   String* str,
                   int radix) {
  StringShape shape(str);
  if (shape.IsSequentialAscii()) {
    const char* begin = SeqOneByteString::cast(str)->GetChars();
    const char* end = begin + str->length();
    return InternalStringToInt(unicode_cache, begin, end, radix);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToInt(unicode_cache, begin, end, radix);
  } else {
    StringInputBuffer buffer(str);
    return InternalStringToInt(unicode_cache,
                               StringInputBufferIterator(&buffer),
                               StringInputBufferIterator::EndMarker(),
                               radix);
  }
}

} }  // namespace v8::internal
