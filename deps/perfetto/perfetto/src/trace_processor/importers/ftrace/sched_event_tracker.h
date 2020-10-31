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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_SCHED_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_SCHED_EVENT_TRACKER_H_

#include <array>
#include <limits>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/destructible.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class EventTracker;

// Tracks sched events and stores them into the storage as sched slices.
class SchedEventTracker : public Destructible {
 public:
  // Declared public for testing only.
  explicit SchedEventTracker(TraceProcessorContext*);
  SchedEventTracker(const SchedEventTracker&) = delete;
  SchedEventTracker& operator=(const SchedEventTracker&) = delete;
  ~SchedEventTracker() override;
  static SchedEventTracker* GetOrCreate(TraceProcessorContext* context) {
    if (!context->sched_tracker) {
      context->sched_tracker.reset(new SchedEventTracker(context));
    }
    return static_cast<SchedEventTracker*>(context->sched_tracker.get());
  }

  // This method is called when a sched_switch event is seen in the trace.
  // Virtual for testing.
  virtual void PushSchedSwitch(uint32_t cpu,
                               int64_t timestamp,
                               uint32_t prev_pid,
                               base::StringView prev_comm,
                               int32_t prev_prio,
                               int64_t prev_state,
                               uint32_t next_pid,
                               base::StringView next_comm,
                               int32_t next_prio);

  // This method is called when parsing a sched_switch encoded in the compact
  // format.
  void PushSchedSwitchCompact(uint32_t cpu,
                              int64_t ts,
                              int64_t prev_state,
                              uint32_t next_pid,
                              int32_t next_prio,
                              StringId next_comm_id);

  // This method is called when parsing a sched_waking encoded in the compact
  // format. Note that the default encoding is handled by
  // |EventTracker::PushInstant|.
  void PushSchedWakingCompact(uint32_t cpu,
                              int64_t ts,
                              uint32_t wakee_pid,
                              int32_t target_cpu,
                              int32_t prio,
                              StringId comm_id);

  // Called at the end of trace to flush any events which are pending to the
  // storage.
  void FlushPendingEvents();

 private:
  // Information retained from the preceding sched_switch seen on a given cpu.
  struct PendingSchedInfo {
    // The pending scheduling slice that the next event will complete.
    uint32_t pending_slice_storage_idx = std::numeric_limits<uint32_t>::max();

    // pid/utid/prio corresponding to the last sched_switch seen on this cpu
    // (its "next_*" fields). There is some duplication with respect to the
    // slices storage, but we don't always have a slice when decoding events in
    // the compact format.
    uint32_t last_pid = std::numeric_limits<uint32_t>::max();
    UniqueTid last_utid = std::numeric_limits<UniqueTid>::max();
    int32_t last_prio = std::numeric_limits<int32_t>::max();
  };

  uint32_t AddRawEventAndStartSlice(uint32_t cpu,
                                    int64_t ts,
                                    UniqueTid prev_utid,
                                    uint32_t prev_pid,
                                    StringId prev_comm_id,
                                    int32_t prev_prio,
                                    int64_t prev_state,
                                    UniqueTid next_utid,
                                    uint32_t next_pid,
                                    StringId next_comm_id,
                                    int32_t next_prio);

  void ClosePendingSlice(uint32_t slice_idx, int64_t ts, int64_t prev_state);

  // Infromation retained from the preceding sched_switch seen on a given cpu.
  std::array<PendingSchedInfo, kMaxCpus> pending_sched_per_cpu_{};

  static constexpr uint8_t kSchedSwitchMaxFieldId = 7;
  std::array<StringId, kSchedSwitchMaxFieldId + 1> sched_switch_field_ids_;
  StringId sched_switch_id_;

  static constexpr uint8_t kSchedWakingMaxFieldId = 5;
  std::array<StringId, kSchedWakingMaxFieldId + 1> sched_waking_field_ids_;
  StringId sched_waking_id_;

  TraceProcessorContext* const context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_SCHED_EVENT_TRACKER_H_
