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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_

#include <array>
#include <limits>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Tracks sched events, instants, and counters.
class EventTracker {
 public:
  explicit EventTracker(TraceProcessorContext*);
  EventTracker(const EventTracker&) = delete;
  EventTracker& operator=(const EventTracker&) = delete;
  virtual ~EventTracker();

  // Adds a counter event to the counters table returning the index of the
  // newly added row.
  virtual base::Optional<CounterId> PushCounter(int64_t timestamp,
                                                double value,
                                                TrackId track_id);

  // Adds a counter event to the counters table for counter events which
  // should be associated with a process but only have a thread context
  // (e.g. rss_stat events).
  //
  // This function will resolve the utid to a upid when the events are
  // flushed (see |FlushPendingEvents()|).
  virtual base::Optional<CounterId> PushProcessCounterForThread(
      int64_t timestamp,
      double value,
      StringId name_id,
      UniqueTid utid);

  // This method is called when a instant event is seen in the trace.
  virtual InstantId PushInstant(int64_t timestamp,
                                StringId name_id,
                                int64_t ref,
                                RefType ref_type,
                                bool resolve_utid_to_upid = false);

  // Called at the end of trace to flush any events which are pending to the
  // storage.
  void FlushPendingEvents();

  // For SchedEventTracker.
  int64_t max_timestamp() const { return max_timestamp_; }
  void UpdateMaxTimestamp(int64_t ts) {
    max_timestamp_ = std::max(ts, max_timestamp_);
  }

 private:
  // Represents a counter event which is currently pending upid resolution.
  struct PendingUpidResolutionCounter {
    uint32_t row = 0;
    StringId name_id = kNullStringId;
    UniqueTid utid = 0;
  };

  // Represents a instant event which is currently pending upid resolution.
  struct PendingUpidResolutionInstant {
    uint32_t row = 0;
    UniqueTid utid = 0;
  };

  // Store the rows in the counters table which need upids resolved.
  std::vector<PendingUpidResolutionCounter> pending_upid_resolution_counter_;

  // Store the rows in the instants table which need upids resolved.
  std::vector<PendingUpidResolutionInstant> pending_upid_resolution_instant_;

  // Timestamp of the previous event. Used to discard events arriving out
  // of order.
  int64_t max_timestamp_ = 0;

  TraceProcessorContext* const context_;
};
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_
