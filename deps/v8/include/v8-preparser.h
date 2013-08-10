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

#ifndef PREPARSER_H
#define PREPARSER_H

#include "v8.h"
#include "v8stdint.h"

namespace v8 {

// The result of preparsing is either a stack overflow error, or an opaque
// blob of data that can be passed back into the parser.
class V8_EXPORT PreParserData {
 public:
  PreParserData(size_t size, const uint8_t* data)
      : data_(data), size_(size) { }

  // Create a PreParserData value where stack_overflow reports true.
  static PreParserData StackOverflow() { return PreParserData(0, NULL); }

  // Whether the pre-parser stopped due to a stack overflow.
  // If this is the case, size() and data() should not be used.
  bool stack_overflow() { return size_ == 0u; }

  // The size of the data in bytes.
  size_t size() const { return size_; }

  // Pointer to the data.
  const uint8_t* data() const { return data_; }

 private:
  const uint8_t* const data_;
  const size_t size_;
};


// Interface for a stream of Unicode characters.
class V8_EXPORT UnicodeInputStream {  // NOLINT - V8_EXPORT is not a class name.
 public:
  virtual ~UnicodeInputStream();

  // Returns the next Unicode code-point in the input, or a negative value when
  // there is no more input in the stream.
  virtual int32_t Next() = 0;
};


// Preparse a JavaScript program. The source code is provided as a
// UnicodeInputStream. The max_stack_size limits the amount of stack
// space that the preparser is allowed to use. If the preparser uses
// more stack space than the limit provided, the result's stack_overflow()
// method will return true. Otherwise the result contains preparser
// data that can be used by the V8 parser to speed up parsing.
PreParserData V8_EXPORT Preparse(UnicodeInputStream* input,
                                size_t max_stack_size);

}  // namespace v8.

#endif  // PREPARSER_H
