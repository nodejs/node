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

#ifndef V8_SCANNER_CHARACTER_STREAMS_H_
#define V8_SCANNER_CHARACTER_STREAMS_H_

#include "scanner.h"

namespace v8 {
namespace internal {

// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos_ pointing to any position,
// even positions before the current).
class BufferedUC16CharacterStream: public UC16CharacterStream {
 public:
  BufferedUC16CharacterStream();
  virtual ~BufferedUC16CharacterStream();

  virtual void PushBack(uc32 character);

 protected:
  static const unsigned kBufferSize = 512;
  static const unsigned kPushBackStepSize = 16;

  virtual unsigned SlowSeekForward(unsigned delta);
  virtual bool ReadBlock();
  virtual void SlowPushBack(uc16 character);

  virtual unsigned BufferSeekForward(unsigned delta) = 0;
  virtual unsigned FillBuffer(unsigned position, unsigned length) = 0;

  const uc16* pushback_limit_;
  uc16 buffer_[kBufferSize];
};


// Generic string stream.
class GenericStringUC16CharacterStream: public BufferedUC16CharacterStream {
 public:
  GenericStringUC16CharacterStream(Handle<String> data,
                                   unsigned start_position,
                                   unsigned end_position);
  virtual ~GenericStringUC16CharacterStream();

 protected:
  virtual unsigned BufferSeekForward(unsigned delta);
  virtual unsigned FillBuffer(unsigned position, unsigned length);

  Handle<String> string_;
  unsigned start_position_;
  unsigned length_;
};


// UC16 stream based on a literal UTF-8 string.
class Utf8ToUC16CharacterStream: public BufferedUC16CharacterStream {
 public:
  Utf8ToUC16CharacterStream(const byte* data, unsigned length);
  virtual ~Utf8ToUC16CharacterStream();

 protected:
  virtual unsigned BufferSeekForward(unsigned delta);
  virtual unsigned FillBuffer(unsigned char_position, unsigned length);
  void SetRawPosition(unsigned char_position);

  const byte* raw_data_;
  unsigned raw_data_length_;  // Measured in bytes, not characters.
  unsigned raw_data_pos_;
  // The character position of the character at raw_data[raw_data_pos_].
  // Not necessarily the same as pos_.
  unsigned raw_character_position_;
};


// UTF16 buffer to read characters from an external string.
class ExternalTwoByteStringUC16CharacterStream: public UC16CharacterStream {
 public:
  ExternalTwoByteStringUC16CharacterStream(Handle<ExternalTwoByteString> data,
                                           int start_position,
                                           int end_position);
  virtual ~ExternalTwoByteStringUC16CharacterStream();

  virtual void PushBack(uc32 character) {
    ASSERT(buffer_cursor_ > raw_data_);
    buffer_cursor_--;
    pos_--;
  }

 protected:
  virtual unsigned SlowSeekForward(unsigned delta) {
    // Fast case always handles seeking.
    return 0;
  }
  virtual bool ReadBlock() {
    // Entire string is read at start.
    return false;
  }
  Handle<ExternalTwoByteString> source_;
  const uc16* raw_data_;  // Pointer to the actual array of characters.
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_CHARACTER_STREAMS_H_
