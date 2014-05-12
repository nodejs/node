// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SCANNER_CHARACTER_STREAMS_H_
#define V8_SCANNER_CHARACTER_STREAMS_H_

#include "scanner.h"

namespace v8 {
namespace internal {

// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos_ pointing to any position,
// even positions before the current).
class BufferedUtf16CharacterStream: public Utf16CharacterStream {
 public:
  BufferedUtf16CharacterStream();
  virtual ~BufferedUtf16CharacterStream();

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
class GenericStringUtf16CharacterStream: public BufferedUtf16CharacterStream {
 public:
  GenericStringUtf16CharacterStream(Handle<String> data,
                                    unsigned start_position,
                                    unsigned end_position);
  virtual ~GenericStringUtf16CharacterStream();

 protected:
  virtual unsigned BufferSeekForward(unsigned delta);
  virtual unsigned FillBuffer(unsigned position, unsigned length);

  Handle<String> string_;
  unsigned length_;
};


// Utf16 stream based on a literal UTF-8 string.
class Utf8ToUtf16CharacterStream: public BufferedUtf16CharacterStream {
 public:
  Utf8ToUtf16CharacterStream(const byte* data, unsigned length);
  virtual ~Utf8ToUtf16CharacterStream();

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
class ExternalTwoByteStringUtf16CharacterStream: public Utf16CharacterStream {
 public:
  ExternalTwoByteStringUtf16CharacterStream(Handle<ExternalTwoByteString> data,
                                            int start_position,
                                            int end_position);
  virtual ~ExternalTwoByteStringUtf16CharacterStream();

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
