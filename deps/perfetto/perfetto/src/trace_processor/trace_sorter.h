/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_TRACE_SORTER_H_
#define SRC_TRACE_PROCESSOR_TRACE_SORTER_H_

#include <vector>

#include "perfetto/ext/base/circular_queue.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_blob_view.h"

namespace Json {
class Value;
}  // namespace Json

namespace perfetto {
namespace trace_processor {

class FuchsiaProviderView;
class PacketSequenceState;
struct SystraceLine;

// This class takes care of sorting events parsed from the trace stream in
// arbitrary order and pushing them to the next pipeline stages (parsing) in
// order. In order to support streaming use-cases, sorting happens within a
// max window. Events are held in the TraceSorter staging area (events_) until
// either (1) the (max - min) timestamp > window_size; (2) trace EOF.
//
// This class is designed around the assumption that:
// - Most events come from ftrace.
// - Ftrace events are sorted within each cpu most of the times.
//
// Due to this, this class is oprerates as a streaming merge-sort of N+1 queues
// (N = num cpus + 1 for non-ftrace events). Each queue in turn gets sorted (if
// necessary) before proceeding with the global merge-sort-extract.
// When an event is pushed through, it is just appeneded to the end of one of
// the N queues. While appending, we keep track of the fact that the queue
// is still ordered or just lost ordering. When an out-of-order event is
// detected on a queue we keep track of: (1) the offset within the queue where
// the chaos begun, (2) the timestamp that broke the ordering.
// When we decide to extract events from the queues into the next stages of
// the trace processor, we re-sort the events in the queue. Rather than
// re-sorting everything all the times, we use the above knowledge to restrict
// sorting to the (hopefully smaller) tail of the |events_| staging area.
// At any time, the first partition of |events_| [0 .. sort_start_idx_) is
// ordered, and the second partition [sort_start_idx_.. end] is not.
// We use a logarithmic bound search operation to figure out what is the index
// within the first partition where sorting should start, and sort all events
// from there to the end.
class TraceSorter {
 public:
  TraceSorter(std::unique_ptr<TraceParser> parser, int64_t window_size_ns);

  inline void PushTracePacket(int64_t timestamp,
                              PacketSequenceState* state,
                              TraceBlobView packet) {
    DCHECK_ftrace_batch_cpu(kNoBatch);
    auto* queue = GetQueue(0);
    queue->Append(TimestampedTracePiece(timestamp, packet_idx_++,
                                        std::move(packet),
                                        state->current_generation()));
    MaybeExtractEvents(queue);
  }

  inline void PushJsonValue(int64_t timestamp,
                            std::unique_ptr<Json::Value> json_value) {
    auto* queue = GetQueue(0);
    queue->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, std::move(json_value)));
    MaybeExtractEvents(queue);
  }

  inline void PushFuchsiaRecord(int64_t timestamp,
                                std::unique_ptr<FuchsiaRecord> record) {
    DCHECK_ftrace_batch_cpu(kNoBatch);
    auto* queue = GetQueue(0);
    queue->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, std::move(record)));
    MaybeExtractEvents(queue);
  }

  inline void PushSystraceLine(std::unique_ptr<SystraceLine> systrace_line) {
    DCHECK_ftrace_batch_cpu(kNoBatch);
    auto* queue = GetQueue(0);
    int64_t timestamp = systrace_line->ts;
    queue->Append(TimestampedTracePiece(timestamp, packet_idx_++,
                                        std::move(systrace_line)));
    MaybeExtractEvents(queue);
  }

  inline void PushFtraceEvent(uint32_t cpu,
                              int64_t timestamp,
                              TraceBlobView event) {
    set_ftrace_batch_cpu_for_DCHECK(cpu);
    GetQueue(cpu + 1)->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, std::move(event)));

    // The caller must call FinalizeFtraceEventBatch() after having pushed a
    // batch of ftrace events. This is to amortize the overhead of handling
    // global ordering and doing that in batches only after all ftrace events
    // for a bundle are pushed.
  }

  // As with |PushFtraceEvent|, doesn't immediately sort the affected queues.
  // TODO(rsavitski): if a trace has a mix of normal & "compact" events (being
  // pushed through this function), the ftrace batches will no longer be fully
  // sorted by timestamp. In such situations, we will have to sort at the end of
  // the batch. We can do better as both sub-sequences are sorted however.
  // Consider adding extra queues, or pushing them in a merge-sort fashion
  // instead.
  inline void PushInlineFtraceEvent(uint32_t cpu,
                                    int64_t timestamp,
                                    InlineSchedSwitch inline_sched_switch) {
    set_ftrace_batch_cpu_for_DCHECK(cpu);
    GetQueue(cpu + 1)->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, inline_sched_switch));
  }
  inline void PushInlineFtraceEvent(uint32_t cpu,
                                    int64_t timestamp,
                                    InlineSchedWaking inline_sched_waking) {
    set_ftrace_batch_cpu_for_DCHECK(cpu);
    GetQueue(cpu + 1)->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, inline_sched_waking));
  }

  inline void PushTrackEventPacket(int64_t timestamp,
                                   std::unique_ptr<TrackEventData> data) {
    auto* queue = GetQueue(0);
    queue->Append(
        TimestampedTracePiece(timestamp, packet_idx_++, std::move(data)));
    MaybeExtractEvents(queue);
  }

  inline void FinalizeFtraceEventBatch(uint32_t cpu) {
    DCHECK_ftrace_batch_cpu(cpu);
    set_ftrace_batch_cpu_for_DCHECK(kNoBatch);
    MaybeExtractEvents(GetQueue(cpu + 1));
  }

  // Extract all events ignoring the window.
  void ExtractEventsForced() {
    SortAndExtractEventsBeyondWindow(/*window_size_ns=*/0);
    queues_.resize(0);
  }

  // Sets the window size to be the size specified (which should be lower than
  // any previous window size specified) and flushes any data beyond
  // this window size.
  // It is undefined to call this function with a window size greater than than
  // the current size.
  void SetWindowSizeNs(int64_t window_size_ns) {
    PERFETTO_DCHECK(window_size_ns <= window_size_ns_);

    PERFETTO_DLOG("Setting window size to be %" PRId64 " ns", window_size_ns);
    window_size_ns_ = window_size_ns;

    // Fast path: if, globally, we are within the window size, then just exit.
    if (global_max_ts_ - global_min_ts_ < window_size_ns)
      return;
    SortAndExtractEventsBeyondWindow(window_size_ns_);
  }

  int64_t max_timestamp() const { return global_max_ts_; }

 private:
  static constexpr uint32_t kNoBatch = std::numeric_limits<uint32_t>::max();

  struct Queue {
    inline void Append(TimestampedTracePiece ttp) {
      const int64_t timestamp = ttp.timestamp;
      events_.emplace_back(std::move(ttp));
      min_ts_ = std::min(min_ts_, timestamp);

      // Events are often seen in order.
      if (PERFETTO_LIKELY(timestamp >= max_ts_)) {
        max_ts_ = timestamp;
      } else {
        // The event is breaking ordering. The first time it happens, keep
        // track of which index we are at. We know that everything before that
        // is sorted (because events were pushed monotonically). Everything
        // after that index, instead, will need a sorting pass before moving
        // events to the next pipeline stage.
        if (sort_start_idx_ == 0) {
          PERFETTO_DCHECK(events_.size() >= 2);
          sort_start_idx_ = events_.size() - 1;
          sort_min_ts_ = timestamp;
        } else {
          sort_min_ts_ = std::min(sort_min_ts_, timestamp);
        }
      }

      PERFETTO_DCHECK(min_ts_ <= max_ts_);
    }

    bool needs_sorting() const { return sort_start_idx_ != 0; }
    void Sort();

    base::CircularQueue<TimestampedTracePiece> events_;
    int64_t min_ts_ = std::numeric_limits<int64_t>::max();
    int64_t max_ts_ = 0;
    size_t sort_start_idx_ = 0;
    int64_t sort_min_ts_ = std::numeric_limits<int64_t>::max();
  };

  // This method passes any events older than window_size_ns to the
  // parser to be parsed and then stored.
  void SortAndExtractEventsBeyondWindow(int64_t windows_size_ns);

  inline Queue* GetQueue(size_t index) {
    if (PERFETTO_UNLIKELY(index >= queues_.size()))
      queues_.resize(index + 1);
    return &queues_[index];
  }

  inline void MaybeExtractEvents(Queue* queue) {
    DCHECK_ftrace_batch_cpu(kNoBatch);
    global_max_ts_ = std::max(global_max_ts_, queue->max_ts_);
    global_min_ts_ = std::min(global_min_ts_, queue->min_ts_);

    // Fast path: if, globally, we are within the window size, then just exit.
    if (global_max_ts_ - global_min_ts_ < window_size_ns_)
      return;
    SortAndExtractEventsBeyondWindow(window_size_ns_);
  }

  std::unique_ptr<TraceParser> parser_;

  // queues_[0] is the general (non-ftrace) queue.
  // queues_[1] is the ftrace queue for CPU(0).
  // queues_[x] is the ftrace queue for CPU(x - 1).
  std::vector<Queue> queues_;

  // Events are propagated to the next stage only after (max - min) timestamp
  // is larger than this value.
  int64_t window_size_ns_;

  // max(e.timestamp for e in queues_).
  int64_t global_max_ts_ = 0;

  // min(e.timestamp for e in queues_).
  int64_t global_min_ts_ = std::numeric_limits<int64_t>::max();

  // Monotonic increasing value used to index timestamped trace pieces.
  uint64_t packet_idx_ = 0;

  // Used for performance tests. True when setting TRACE_PROCESSOR_SORT_ONLY=1.
  bool bypass_next_stage_for_testing_ = false;

#if PERFETTO_DCHECK_IS_ON()
  // Used only for DCHECK-ing that FinalizeFtraceEventBatch() is called.
  uint32_t ftrace_batch_cpu_ = kNoBatch;

  inline void DCHECK_ftrace_batch_cpu(uint32_t cpu) {
    PERFETTO_DCHECK(ftrace_batch_cpu_ == kNoBatch || ftrace_batch_cpu_ == cpu);
  }

  inline void set_ftrace_batch_cpu_for_DCHECK(uint32_t cpu) {
    PERFETTO_DCHECK(ftrace_batch_cpu_ == cpu || ftrace_batch_cpu_ == kNoBatch ||
                    cpu == kNoBatch);
    ftrace_batch_cpu_ = cpu;
  }
#else
  inline void DCHECK_ftrace_batch_cpu(uint32_t) {}
  inline void set_ftrace_batch_cpu_for_DCHECK(uint32_t) {}
#endif
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TRACE_SORTER_H_
