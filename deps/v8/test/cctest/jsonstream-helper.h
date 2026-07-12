// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_JSONTREAM_HELPER_H_
#define V8_CCTEST_JSONTREAM_HELPER_H_

#include "include/v8-profiler.h"
#include "test/cctest/collector.h"

namespace v8 {
namespace internal {

class TestJSONStream : public v8::OutputStream {
 public:
  TestJSONStream() : eos_signaled_(0), abort_countdown_(-1) {}
  explicit TestJSONStream(int abort_countdown)
      : eos_signaled_(0), abort_countdown_(abort_countdown) {}
  ~TestJSONStream() override = default;
  void EndOfStream() override { ++eos_signaled_; }
  OutputStream::WriteResult WriteAsciiChunk(char* buffer,
                                            int chars_written) override {
    if (abort_countdown_ > 0) --abort_countdown_;
    if (abort_countdown_ == 0) return OutputStream::kAbort;
    CHECK_GT(chars_written, 0);
    v8::base::Vector<char> chunk = buffer_.AddBlock(chars_written, '\0');
    i::MemCopy(chunk.begin(), buffer, chars_written);
    return OutputStream::kContinue;
  }

  virtual WriteResult WriteUint32Chunk(uint32_t* buffer, int chars_written) {
    UNREACHABLE();
  }
  void WriteTo(v8::base::Vector<char> dest) { buffer_.WriteTo(dest); }
  int eos_signaled() { return eos_signaled_; }
  int size() { return buffer_.size(); }

 private:
  i::Collector<char> buffer_;
  int eos_signaled_;
  int abort_countdown_;
};

class OneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit OneByteResource(v8::base::Vector<char> string)
      : data_(string.begin()) {
    length_ = string.length();
  }
  const char* data() const override { return data_; }
  size_t length() const override { return length_; }

 private:
  const char* data_;
  size_t length_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_JSONTREAM_HELPER_H_
