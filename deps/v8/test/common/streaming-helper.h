// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_STREAMING_HELPER_H_
#define V8_COMMON_STREAMING_HELPER_H_

#include "include/v8-script.h"

namespace v8 {
namespace internal {

class TestSourceStream : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  explicit TestSourceStream(const char** chunks) : chunks_(chunks), index_(0) {}

  size_t GetMoreData(const uint8_t** src) override {
    // Unlike in real use cases, this function will never block.
    if (chunks_[index_] == nullptr) {
      return 0;
    }
    // Copy the data, since the caller takes ownership of it.
    size_t len = strlen(chunks_[index_]);
    // We don't need to zero-terminate since we return the length.
    uint8_t* copy = new uint8_t[len];
    memcpy(copy, chunks_[index_], len);
    *src = copy;
    ++index_;
    return len;
  }

  // Helper for constructing a string from chunks (the compilation needs it
  // too).
  static char* FullSourceString(const char** chunks) {
    size_t total_len = 0;
    for (size_t i = 0; chunks[i] != nullptr; ++i) {
      total_len += strlen(chunks[i]);
    }
    char* full_string = new char[total_len + 1];
    size_t offset = 0;
    for (size_t i = 0; chunks[i] != nullptr; ++i) {
      size_t len = strlen(chunks[i]);
      memcpy(full_string + offset, chunks[i], len);
      offset += len;
    }
    full_string[total_len] = 0;
    return full_string;
  }

 private:
  const char** chunks_;
  unsigned index_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_STREAMING_HELPER_H_
