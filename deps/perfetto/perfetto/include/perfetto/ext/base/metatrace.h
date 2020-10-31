/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_METATRACE_H_
#define INCLUDE_PERFETTO_EXT_BASE_METATRACE_H_

#include <array>
#include <atomic>
#include <functional>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/metatrace_events.h"
#include "perfetto/ext/base/thread_annotations.h"
#include "perfetto/ext/base/utils.h"

// A facility to trace execution of the perfetto codebase itself.
// The meta-tracing framework is organized into three layers:
//
// 1. A static ring-buffer in base/ (this file) that supports concurrent writes
//    and a single reader.
//    The responsibility of this layer is to store events and counters as
//    efficiently as possible without re-entering any tracing code.
//    This is really a static-storage-based ring-buffer based on a POD array.
//    This layer does NOT deal with serializing the meta-trace buffer.
//    It posts a task when it's half full and expects something outside of
//    base/ to drain the ring-buffer and serialize it, eventually writing it
//    into the trace itself, before it gets 100% full.
//
// 2. A class in tracing/core which takes care of serializing the meta-trace
//    buffer into the trace using a TraceWriter. See metatrace_writer.h .
//
// 3. A data source in traced_probes that, when be enabled via the trace config,
//    injects metatrace events into the trace. See metatrace_data_source.h .
//
// The available events and tags are defined in metatrace_events.h .

namespace perfetto {

namespace base {
class TaskRunner;
}  // namespace base

namespace metatrace {

// Meta-tracing is organized in "tags" that can be selectively enabled. This is
// to enable meta-tracing only of one sub-system. This word has one "enabled"
// bit for each tag. 0 -> meta-tracing off.
extern std::atomic<uint32_t> g_enabled_tags;

// Time of the Enable() call. Used as a reference for keeping delta timestmaps
// in Record.
extern std::atomic<uint64_t> g_enabled_timestamp;

// Enables meta-tracing for one or more tags. Once enabled it will discard any
// further Enable() calls and return false until disabled,
// |read_task| is a closure that will be called enqueued |task_runner| when the
// meta-tracing ring buffer is half full. The task is expected to read the ring
// buffer using RingBuffer::GetReadIterator() and serialize the contents onto a
// file or into the trace itself.
// Must be called on the |task_runner| passed.
// |task_runner| must have static lifetime.
bool Enable(std::function<void()> read_task, base::TaskRunner*, uint32_t tags);

// Disables meta-tracing.
// Must be called on the same |task_runner| as Enable().
void Disable();

inline uint64_t TraceTimeNowNs() {
  return static_cast<uint64_t>(base::GetBootTimeNs().count());
}

// Returns a relaxed view of whether metatracing is enabled for the given tag.
// Useful for skipping unnecessary argument computation if metatracing is off.
inline bool IsEnabled(uint32_t tag) {
  auto enabled_tags = g_enabled_tags.load(std::memory_order_relaxed);
  if (PERFETTO_LIKELY((enabled_tags & tag) == 0))
    return false;
  else
    return true;
}

// Holds the data for a metatrace event or counter.
struct Record {
  static constexpr uint16_t kTypeMask = 0x8000;
  static constexpr uint16_t kTypeCounter = 0x8000;
  static constexpr uint16_t kTypeEvent = 0;

  uint64_t timestamp_ns() const {
    auto base_ns = g_enabled_timestamp.load(std::memory_order_relaxed);
    PERFETTO_DCHECK(base_ns);
    return base_ns + ((static_cast<uint64_t>(timestamp_ns_high) << 32) |
                      timestamp_ns_low);
  }

  void set_timestamp(uint64_t ts) {
    auto t_start = g_enabled_timestamp.load(std::memory_order_relaxed);
    uint64_t diff = ts - t_start;
    PERFETTO_DCHECK(diff < (1ull << 48));
    timestamp_ns_low = static_cast<uint32_t>(diff);
    timestamp_ns_high = static_cast<uint16_t>(diff >> 32);
  }

  // We can't just memset() this class because on MSVC std::atomic<> is not
  // trivially constructible anymore. Also std::atomic<> has a deleted copy
  // constructor so we cant just do "*this = Record()" either.
  // See http://bit.ly/339Jlzd .
  void clear() {
    this->~Record();
    new (this) Record();
  }

  // This field holds the type (counter vs event) in the MSB and event ID (as
  // defined in metatrace_events.h) in the lowest 15 bits. It is also used also
  // as a linearization point: this is always written after all the other
  // fields with a release-store. This is so the reader can determine whether it
  // can safely process the other event fields after a load-acquire.
  std::atomic<uint16_t> type_and_id{};

  // Timestamp is stored as a 48-bits value diffed against g_enabled_timestamp.
  // This gives us 78 hours from Enabled().
  uint16_t timestamp_ns_high = 0;
  uint32_t timestamp_ns_low = 0;

  uint32_t thread_id = 0;

  union {
    // Only one of the two elements can be zero initialized, clang complains
    // about "initializing multiple members of union" otherwise.
    uint32_t duration_ns = 0;  // If type == event.
    int32_t counter_value;  // If type == counter.
  };
};

// Hold the meta-tracing data into a statically allocated array.
// This class uses static storage (as opposite to being a singleton) to:
// - Have the guarantee of always valid storage, so that meta-tracing can be
//   safely used in any part of the codebase, including base/ itself.
// - Avoid barriers that thread-safe static locals would require.
class RingBuffer {
 public:
  static constexpr size_t kCapacity = 4096;  // 4096 * 16 bytes = 64K.

  // This iterator is not idempotent and will bump the read index in the buffer
  // at the end of the reads. There can be only one reader at any time.
  // Usage: for (auto it = RingBuffer::GetReadIterator(); it; ++it) { it->... }
  class ReadIterator {
   public:
    ReadIterator(ReadIterator&& other) {
      PERFETTO_DCHECK(other.valid_);
      cur_ = other.cur_;
      end_ = other.end_;
      valid_ = other.valid_;
      other.valid_ = false;
    }

    ~ReadIterator() {
      if (!valid_)
        return;
      PERFETTO_DCHECK(cur_ >= RingBuffer::rd_index_);
      PERFETTO_DCHECK(cur_ <= RingBuffer::wr_index_);
      RingBuffer::rd_index_.store(cur_, std::memory_order_release);
    }

    explicit operator bool() const { return cur_ < end_; }
    const Record* operator->() const { return RingBuffer::At(cur_); }
    const Record& operator*() const { return *operator->(); }

    // This is for ++it. it++ is deliberately not supported.
    ReadIterator& operator++() {
      PERFETTO_DCHECK(cur_ < end_);
      // Once a record has been read, mark it as free clearing its type_and_id,
      // so if we encounter it in another read iteration while being written
      // we know it's not fully written yet.
      // The memory_order_relaxed below is enough because:
      // - The reader is single-threaded and doesn't re-read the same records.
      // - Before starting a read batch, the reader has an acquire barrier on
      //   |rd_index_|.
      // - After terminating a read batch, the ~ReadIterator dtor updates the
      //   |rd_index_| with a release-store.
      // - Reader and writer are typically kCapacity/2 apart. So unless an
      //   overrun happens a writer won't reuse a newly released record any time
      //   soon. If an overrun happens, everything is busted regardless.
      At(cur_)->type_and_id.store(0, std::memory_order_relaxed);
      ++cur_;
      return *this;
    }

   private:
    friend class RingBuffer;
    ReadIterator(uint64_t begin, uint64_t end)
        : cur_(begin), end_(end), valid_(true) {}
    ReadIterator& operator=(const ReadIterator&) = delete;
    ReadIterator(const ReadIterator&) = delete;

    uint64_t cur_;
    uint64_t end_;
    bool valid_;
  };

  static Record* At(uint64_t index) {
    // Doesn't really have to be pow2, but if not the compiler will emit
    // arithmetic operations to compute the modulo instead of a bitwise AND.
    static_assert(!(kCapacity & (kCapacity - 1)), "kCapacity must be pow2");
    PERFETTO_DCHECK(index >= rd_index_);
    PERFETTO_DCHECK(index <= wr_index_);
    return &records_[index % kCapacity];
  }

  // Must be called on the same task runner passed to Enable()
  static ReadIterator GetReadIterator() {
    PERFETTO_DCHECK(RingBuffer::IsOnValidTaskRunner());
    return ReadIterator(rd_index_.load(std::memory_order_acquire),
                        wr_index_.load(std::memory_order_acquire));
  }

  static Record* AppendNewRecord();
  static void Reset();

  static bool has_overruns() {
    return has_overruns_.load(std::memory_order_acquire);
  }

  // Can temporarily return a value >= kCapacity but is eventually consistent.
  // This would happen in case of overruns until threads hit the --wr_index_
  // in AppendNewRecord().
  static uint64_t GetSizeForTesting() {
    auto wr_index = wr_index_.load(std::memory_order_relaxed);
    auto rd_index = rd_index_.load(std::memory_order_relaxed);
    PERFETTO_DCHECK(wr_index >= rd_index);
    return wr_index - rd_index;
  }

 private:
  friend class ReadIterator;

  // Returns true if the caller is on the task runner passed to Enable().
  // Used only for DCHECKs.
  static bool IsOnValidTaskRunner();

  static std::array<Record, kCapacity> records_;
  static std::atomic<bool> read_task_queued_;
  static std::atomic<uint64_t> wr_index_;
  static std::atomic<uint64_t> rd_index_;
  static std::atomic<bool> has_overruns_;
  static Record bankruptcy_record_;  // Used in case of overruns.
};

inline void TraceCounter(uint32_t tag, uint16_t id, int32_t value) {
  // memory_order_relaxed is okay because the storage has static lifetime.
  // It is safe to accidentally log an event soon after disabling.
  auto enabled_tags = g_enabled_tags.load(std::memory_order_relaxed);
  if (PERFETTO_LIKELY((enabled_tags & tag) == 0))
    return;
  Record* record = RingBuffer::AppendNewRecord();
  record->thread_id = static_cast<uint32_t>(base::GetThreadId());
  record->set_timestamp(TraceTimeNowNs());
  record->counter_value = value;
  record->type_and_id.store(Record::kTypeCounter | id,
                            std::memory_order_release);
}

class ScopedEvent {
 public:
  ScopedEvent(uint32_t tag, uint16_t event_id) {
    auto enabled_tags = g_enabled_tags.load(std::memory_order_relaxed);
    if (PERFETTO_LIKELY((enabled_tags & tag) == 0))
      return;
    event_id_ = event_id;
    record_ = RingBuffer::AppendNewRecord();
    record_->thread_id = static_cast<uint32_t>(base::GetThreadId());
    record_->set_timestamp(TraceTimeNowNs());
  }

  ~ScopedEvent() {
    if (PERFETTO_LIKELY(!record_))
      return;
    auto now = TraceTimeNowNs();
    record_->duration_ns = static_cast<uint32_t>(now - record_->timestamp_ns());
    record_->type_and_id.store(Record::kTypeEvent | event_id_,
                               std::memory_order_release);
  }

 private:
  Record* record_ = nullptr;
  uint16_t event_id_ = 0;
  ScopedEvent(const ScopedEvent&) = delete;
  ScopedEvent& operator=(const ScopedEvent&) = delete;
};

// Boilerplate to derive a unique variable name for the event.
#define PERFETTO_METATRACE_UID2(a, b) a##b
#define PERFETTO_METATRACE_UID(x) PERFETTO_METATRACE_UID2(metatrace_, x)

#define PERFETTO_METATRACE_SCOPED(TAG, ID)                                \
  ::perfetto::metatrace::ScopedEvent PERFETTO_METATRACE_UID(__COUNTER__)( \
      ::perfetto::metatrace::TAG, ::perfetto::metatrace::ID)

#define PERFETTO_METATRACE_COUNTER(TAG, ID, VALUE)                \
  ::perfetto::metatrace::TraceCounter(::perfetto::metatrace::TAG, \
                                      ::perfetto::metatrace::ID,  \
                                      static_cast<int32_t>(VALUE))

}  // namespace metatrace
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_METATRACE_H_
