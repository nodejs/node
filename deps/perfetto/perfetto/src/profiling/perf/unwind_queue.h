/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_PERF_UNWIND_QUEUE_H_
#define SRC_PROFILING_PERF_UNWIND_QUEUE_H_

#include <array>
#include <atomic>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace profiling {

struct WriteView {
  bool valid;  // if false: buffer is full, and |write_pos| is invalid
  uint64_t write_pos;
};

struct ReadView {
  uint64_t read_pos;
  uint64_t write_pos;
};

// Single-writer, single-reader ring buffer of fixed-size entries (of any
// default-constructible type). Size of the buffer is static for the lifetime of
// UnwindQueue, and must be a power of two.
// Writer side appends entries one at a time, and must stop if there
// is no available capacity.
// Reader side sees all unconsumed entries, and can advance the reader position
// by any amount.
template <typename T, uint32_t QueueSize>
class UnwindQueue {
 public:
  UnwindQueue() {
    static_assert(QueueSize != 0 && ((QueueSize & (QueueSize - 1)) == 0),
                  "not a power of two");
  }

  UnwindQueue(const UnwindQueue&) = delete;
  UnwindQueue& operator=(const UnwindQueue&) = delete;
  UnwindQueue(UnwindQueue&&) = delete;
  UnwindQueue& operator=(UnwindQueue&&) = delete;

  T& at(uint64_t pos) { return data_[pos % QueueSize]; }

  WriteView BeginWrite() {
    uint64_t rd = rd_pos_.load(std::memory_order_acquire);
    uint64_t wr = wr_pos_.load(std::memory_order_relaxed);

    PERFETTO_DCHECK(wr >= rd);
    if (wr - rd >= QueueSize)
      return WriteView{false, 0};  // buffer fully occupied

    return WriteView{true, wr};
  }

  void CommitWrite() { wr_pos_.fetch_add(1u, std::memory_order_release); }

  ReadView BeginRead() {
    uint64_t wr = wr_pos_.load(std::memory_order_acquire);
    uint64_t rd = rd_pos_.load(std::memory_order_relaxed);

    PERFETTO_DCHECK(wr >= rd && wr - rd <= QueueSize);
    return ReadView{rd, wr};
  }

  void CommitNewReadPosition(uint64_t pos) {
    rd_pos_.store(pos, std::memory_order_release);
  }

 private:
  std::array<T, QueueSize> data_;
  std::atomic<uint64_t> wr_pos_{0};
  std::atomic<uint64_t> rd_pos_{0};
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_PERF_UNWIND_QUEUE_H_
