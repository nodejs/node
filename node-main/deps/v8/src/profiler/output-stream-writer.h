// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_OUTPUT_STREAM_WRITER_H_
#define V8_PROFILER_OUTPUT_STREAM_WRITER_H_

#include <algorithm>
#include <charconv>
#include <string>
#include <system_error>

#include "include/v8-profiler.h"
#include "include/v8config.h"
#include "src/base/logging.h"
#include "src/base/vector.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

class OutputStreamWriter {
 public:
  explicit OutputStreamWriter(v8::OutputStream* stream)
      : stream_(stream),
        chunk_size_(stream->GetChunkSize()),
        chunk_(chunk_size_),
        chunk_pos_(0),
        aborted_(false) {
    DCHECK_GT(chunk_size_, 0);
  }
  bool aborted() { return aborted_; }
  void AddCharacter(char c) {
    DCHECK_NE(c, '\0');
    DCHECK(chunk_pos_ < chunk_size_);
    chunk_[chunk_pos_++] = c;
    MaybeWriteChunk();
  }
  void AddString(const char* s) {
    size_t len = strlen(s);
    DCHECK_GE(kMaxInt, len);
    const char* s_end = s + len;
    while (s < s_end) {
      int s_chunk_size =
          std::min(chunk_size_ - chunk_pos_, static_cast<int>(s_end - s));
      DCHECK_GT(s_chunk_size, 0);
      MemCopy(chunk_.begin() + chunk_pos_, s, s_chunk_size);
      s += s_chunk_size;
      chunk_pos_ += s_chunk_size;
      MaybeWriteChunk();
    }
  }
  template <typename T>
  void AddNumber(T n) {
    std::to_chars_result result =
        std::to_chars(chunk_.begin() + chunk_pos_, chunk_.end(), n);
    if (V8_LIKELY(result.ec == std::errc{})) {
      chunk_pos_ = static_cast<int>(result.ptr - chunk_.begin());
      MaybeWriteChunk();
    } else {
      // Expected to be the only possible reason for `to_chars` to fail.
      CHECK(result.ec == std::errc::value_too_large);
      // Write the current chunk and try again if we haven't already.
      CHECK_WITH_MSG(chunk_pos_ > 0,
                     "Chunk size insufficient to serialize number");
      WriteChunk();
      AddNumber(n);
    }
  }
  void Finalize() {
    if (aborted_) return;
    DCHECK(chunk_pos_ < chunk_size_);
    if (chunk_pos_ != 0) {
      WriteChunk();
    }
    stream_->EndOfStream();
  }

 private:
  void MaybeWriteChunk() {
    DCHECK(chunk_pos_ <= chunk_size_);
    if (chunk_pos_ == chunk_size_) {
      WriteChunk();
    }
  }
  void WriteChunk() {
    if (aborted_) return;
    if (stream_->WriteAsciiChunk(chunk_.begin(), chunk_pos_) ==
        v8::OutputStream::kAbort)
      aborted_ = true;
    chunk_pos_ = 0;
  }

  v8::OutputStream* stream_;
  int chunk_size_;
  base::ScopedVector<char> chunk_;
  int chunk_pos_;
  bool aborted_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_OUTPUT_STREAM_WRITER_H_
