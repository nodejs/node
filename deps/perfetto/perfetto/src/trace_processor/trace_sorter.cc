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

#include <algorithm>
#include <utility>

#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/importers/proto/proto_trace_parser.h"
#include "src/trace_processor/trace_sorter.h"

namespace perfetto {
namespace trace_processor {

TraceSorter::TraceSorter(std::unique_ptr<TraceParser> parser,
                         int64_t window_size_ns)
    : parser_(std::move(parser)), window_size_ns_(window_size_ns) {
  const char* env = getenv("TRACE_PROCESSOR_SORT_ONLY");
  bypass_next_stage_for_testing_ = env && !strcmp(env, "1");
  if (bypass_next_stage_for_testing_)
    PERFETTO_ELOG("TEST MODE: bypassing protobuf parsing stage");
}

void TraceSorter::Queue::Sort() {
  PERFETTO_DCHECK(needs_sorting());
  PERFETTO_DCHECK(sort_start_idx_ < events_.size());

  // If sort_min_ts_ has been set, it will no long be max_int, and so will be
  // smaller than max_ts_.
  PERFETTO_DCHECK(sort_min_ts_ < max_ts_);

  // We know that all events between [0, sort_start_idx_] are sorted. Within
  // this range, perform a bound search and find the iterator for the min
  // timestamp that broke the monotonicity. Re-sort from there to the end.
  auto sort_end = events_.begin() + static_cast<ssize_t>(sort_start_idx_);
  PERFETTO_DCHECK(std::is_sorted(events_.begin(), sort_end));
  auto sort_begin = std::lower_bound(events_.begin(), sort_end, sort_min_ts_,
                                     &TimestampedTracePiece::Compare);
  std::sort(sort_begin, events_.end());
  sort_start_idx_ = 0;
  sort_min_ts_ = 0;

  // At this point |events_| must be fully sorted.
  PERFETTO_DCHECK(std::is_sorted(events_.begin(), events_.end()));
}

// Removes all the events in |queues_| that are earlier than the given window
// size and moves them to the next parser stages, respecting global timestamp
// order. This function is a "extract min from N sorted queues", with some
// little cleverness: we know that events tend to be bursty, so events are
// not going to be randomly distributed on the N |queues_|.
// Upon each iteration this function finds the first two queues (if any) that
// have the oldest events, and extracts events from the 1st until hitting the
// min_ts of the 2nd. Imagine the queues are as follows:
//
//  q0           {min_ts: 10  max_ts: 30}
//  q1    {min_ts:5              max_ts: 35}
//  q2              {min_ts: 12    max_ts: 40}
//
// We know that we can extract all events from q1 until we hit ts=10 without
// looking at any other queue. After hitting ts=10, we need to re-look to all of
// them to figure out the next min-event.
// There are more suitable data structures to do this (e.g. keeping a min-heap
// to avoid re-scanning all the queues all the times) but doesn't seem worth it.
// With Android traces (that have 8 CPUs) this function accounts for ~1-3% cpu
// time in a profiler.
void TraceSorter::SortAndExtractEventsBeyondWindow(int64_t window_size_ns) {
  DCHECK_ftrace_batch_cpu(kNoBatch);

  constexpr int64_t kTsMax = std::numeric_limits<int64_t>::max();
  const bool was_empty = global_min_ts_ == kTsMax && global_max_ts_ == 0;
  int64_t extract_end_ts = global_max_ts_ - window_size_ns;
  size_t iterations = 0;
  for (;; iterations++) {
    size_t min_queue_idx = 0;  // The index of the queue with the min(ts).

    // The top-2 min(ts) among all queues.
    // queues_[min_queue_idx].events.timestamp == min_queue_ts[0].
    int64_t min_queue_ts[2]{kTsMax, kTsMax};

    // This loop identifies the queue which starts with the earliest event and
    // also remembers the earliest event of the 2nd queue (in min_queue_ts[1]).
    bool has_queues_with_expired_events = false;
    for (size_t i = 0; i < queues_.size(); i++) {
      auto& queue = queues_[i];
      if (queue.events_.empty())
        continue;
      PERFETTO_DCHECK(queue.min_ts_ >= global_min_ts_);
      PERFETTO_DCHECK(queue.max_ts_ <= global_max_ts_);
      if (queue.min_ts_ < min_queue_ts[0]) {
        min_queue_ts[1] = min_queue_ts[0];
        min_queue_ts[0] = queue.min_ts_;
        min_queue_idx = i;
        has_queues_with_expired_events = true;
      } else if (queue.min_ts_ < min_queue_ts[1]) {
        min_queue_ts[1] = queue.min_ts_;
      }
    }
    if (!has_queues_with_expired_events) {
      // All the queues have events that start after the window (i.e. they are
      // too recent and not eligible to be extracted given the current window).
      break;
    }

    Queue& queue = queues_[min_queue_idx];
    auto& events = queue.events_;
    if (queue.needs_sorting())
      queue.Sort();
    PERFETTO_DCHECK(queue.min_ts_ == events.front().timestamp);
    PERFETTO_DCHECK(queue.min_ts_ == global_min_ts_);

    // Now that we identified the min-queue, extract all events from it until
    // we hit either: (1) the min-ts of the 2nd queue or (2) the window limit,
    // whichever comes first.
    int64_t extract_until_ts = std::min(extract_end_ts, min_queue_ts[1]);
    size_t num_extracted = 0;
    for (auto& event : events) {
      int64_t timestamp = event.timestamp;
      if (timestamp > extract_until_ts)
        break;

      ++num_extracted;
      if (bypass_next_stage_for_testing_)
        continue;

      if (min_queue_idx == 0) {
        // queues_[0] is for non-ftrace packets.
        parser_->ParseTracePacket(timestamp, std::move(event));
      } else {
        // Ftrace queues start at offset 1. So queues_[1] = cpu[0] and so on.
        uint32_t cpu = static_cast<uint32_t>(min_queue_idx - 1);
        parser_->ParseFtracePacket(cpu, timestamp, std::move(event));
      }
    }  // for (event: events)

    if (!num_extracted) {
      // No events can be extracted from any of the queues. This means that
      // either we hit the window or all queues are empty.
      break;
    }

    // Now remove the entries from the event buffer and update the queue-local
    // and global time bounds.
    events.erase_front(num_extracted);

    // Update the global_{min,max}_ts to reflect the bounds after extraction.
    if (events.empty()) {
      queue.min_ts_ = kTsMax;
      queue.max_ts_ = 0;
      global_min_ts_ = min_queue_ts[1];

      // If we extraced the max entry from a queue (i.e. we emptied the queue)
      // we need to recompute the global max, because it might have been the one
      // just extracted.
      global_max_ts_ = 0;
      for (auto& q : queues_)
        global_max_ts_ = std::max(global_max_ts_, q.max_ts_);
    } else {
      queue.min_ts_ = queue.events_.front().timestamp;
      global_min_ts_ = std::min(queue.min_ts_, min_queue_ts[1]);
    }
  }  // for(;;)

  // We decide to extract events only when we know (using the global_{min,max}
  // bounds) that there are eligible events. We should never end up in a
  // situation where we call this function but then realize that there was
  // nothing to extract.
  PERFETTO_DCHECK(iterations > 0 || was_empty);

#if PERFETTO_DCHECK_IS_ON()
  // Check that the global min/max are consistent.
  int64_t dbg_min_ts = kTsMax;
  int64_t dbg_max_ts = 0;
  for (auto& q : queues_) {
    dbg_min_ts = std::min(dbg_min_ts, q.min_ts_);
    dbg_max_ts = std::max(dbg_max_ts, q.max_ts_);
  }
  PERFETTO_DCHECK(global_min_ts_ == dbg_min_ts);
  PERFETTO_DCHECK(global_max_ts_ == dbg_max_ts);
#endif
}

}  // namespace trace_processor
}  // namespace perfetto
