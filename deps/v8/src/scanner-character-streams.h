// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SCANNER_CHARACTER_STREAMS_H_
#define V8_SCANNER_CHARACTER_STREAMS_H_

#include "src/scanner.h"

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
  static const size_t kBufferSize = 512;
  static const size_t kPushBackStepSize = 16;

  virtual size_t SlowSeekForward(size_t delta);
  virtual bool ReadBlock();
  virtual void SlowPushBack(uc16 character);

  virtual size_t BufferSeekForward(size_t delta) = 0;
  virtual size_t FillBuffer(size_t position) = 0;

  const uc16* pushback_limit_;
  uc16 buffer_[kBufferSize];
};


// Generic string stream.
class GenericStringUtf16CharacterStream: public BufferedUtf16CharacterStream {
 public:
  GenericStringUtf16CharacterStream(Handle<String> data, size_t start_position,
                                    size_t end_position);
  virtual ~GenericStringUtf16CharacterStream();

  virtual bool SetBookmark();
  virtual void ResetToBookmark();

 protected:
  static const size_t kNoBookmark = -1;

  virtual size_t BufferSeekForward(size_t delta);
  virtual size_t FillBuffer(size_t position);

  Handle<String> string_;
  size_t length_;
  size_t bookmark_;
};


// Utf16 stream based on a literal UTF-8 string.
class Utf8ToUtf16CharacterStream: public BufferedUtf16CharacterStream {
 public:
  Utf8ToUtf16CharacterStream(const byte* data, size_t length);
  virtual ~Utf8ToUtf16CharacterStream();

  static size_t CopyChars(uint16_t* dest, size_t length, const byte* src,
                          size_t* src_pos, size_t src_length);

 protected:
  virtual size_t BufferSeekForward(size_t delta);
  virtual size_t FillBuffer(size_t char_position);
  void SetRawPosition(size_t char_position);

  const byte* raw_data_;
  size_t raw_data_length_;  // Measured in bytes, not characters.
  size_t raw_data_pos_;
  // The character position of the character at raw_data[raw_data_pos_].
  // Not necessarily the same as pos_.
  size_t raw_character_position_;
};


// ExternalStreamingStream is a wrapper around an ExternalSourceStream (see
// include/v8.h) subclass implemented by the embedder.
class ExternalStreamingStream : public BufferedUtf16CharacterStream {
 public:
  ExternalStreamingStream(ScriptCompiler::ExternalSourceStream* source_stream,
                          v8::ScriptCompiler::StreamedSource::Encoding encoding)
      : source_stream_(source_stream),
        encoding_(encoding),
        current_data_(NULL),
        current_data_offset_(0),
        current_data_length_(0),
        utf8_split_char_buffer_length_(0),
        bookmark_(0),
        bookmark_utf8_split_char_buffer_length_(0) {}

  virtual ~ExternalStreamingStream() {
    delete[] current_data_;
    bookmark_buffer_.Dispose();
    bookmark_data_.Dispose();
  }

  size_t BufferSeekForward(size_t delta) override {
    // We never need to seek forward when streaming scripts. We only seek
    // forward when we want to parse a function whose location we already know,
    // and when streaming, we don't know the locations of anything we haven't
    // seen yet.
    UNREACHABLE();
    return 0;
  }

  size_t FillBuffer(size_t position) override;

  virtual bool SetBookmark() override;
  virtual void ResetToBookmark() override;

 private:
  void HandleUtf8SplitCharacters(size_t* data_in_buffer);
  void FlushCurrent();

  ScriptCompiler::ExternalSourceStream* source_stream_;
  v8::ScriptCompiler::StreamedSource::Encoding encoding_;
  const uint8_t* current_data_;
  size_t current_data_offset_;
  size_t current_data_length_;
  // For converting UTF-8 characters which are split across two data chunks.
  uint8_t utf8_split_char_buffer_[4];
  size_t utf8_split_char_buffer_length_;

  // Bookmark support. See comments in ExternalStreamingStream::SetBookmark
  // for additional details.
  size_t bookmark_;
  Vector<uint16_t> bookmark_buffer_;
  Vector<uint8_t> bookmark_data_;
  uint8_t bookmark_utf8_split_char_buffer_[4];
  size_t bookmark_utf8_split_char_buffer_length_;
};


// UTF16 buffer to read characters from an external string.
class ExternalTwoByteStringUtf16CharacterStream: public Utf16CharacterStream {
 public:
  ExternalTwoByteStringUtf16CharacterStream(Handle<ExternalTwoByteString> data,
                                            int start_position,
                                            int end_position);
  virtual ~ExternalTwoByteStringUtf16CharacterStream();

  virtual void PushBack(uc32 character) {
    DCHECK(buffer_cursor_ > raw_data_);
    buffer_cursor_--;
    pos_--;
  }

  virtual bool SetBookmark();
  virtual void ResetToBookmark();

 protected:
  virtual size_t SlowSeekForward(size_t delta) {
    // Fast case always handles seeking.
    return 0;
  }
  virtual bool ReadBlock() {
    // Entire string is read at start.
    return false;
  }
  Handle<ExternalTwoByteString> source_;
  const uc16* raw_data_;  // Pointer to the actual array of characters.

 private:
  static const size_t kNoBookmark = -1;

  size_t bookmark_;
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_CHARACTER_STREAMS_H_
