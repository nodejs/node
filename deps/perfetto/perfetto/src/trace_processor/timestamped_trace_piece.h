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

#ifndef SRC_TRACE_PROCESSOR_TIMESTAMPED_TRACE_PIECE_H_
#define SRC_TRACE_PROCESSOR_TIMESTAMPED_TRACE_PIECE_H_

#include "perfetto/base/build_config.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_record.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/systrace/systrace_line.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/types/trace_processor_context.h"

// GCC can't figure out the relationship between TimestampedTracePiece's type
// and the union, and thus thinks that we may be moving or destroying
// uninitialized data in the move constructors / destructors. Disable those
// warnings for TimestampedTracePiece and the types it contains.
#if PERFETTO_BUILDFLAG(PERFETTO_COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

namespace perfetto {
namespace trace_processor {

struct InlineSchedSwitch {
  int64_t prev_state;
  int32_t next_pid;
  int32_t next_prio;
  StringId next_comm;
};

struct InlineSchedWaking {
  int32_t pid;
  int32_t target_cpu;
  int32_t prio;
  StringId comm;
};

struct TracePacketData {
  TraceBlobView packet;

  PacketSequenceStateGeneration* sequence_state;
};

struct TrackEventData : public TracePacketData {
  TrackEventData(TraceBlobView pv, PacketSequenceStateGeneration* generation)
      : TracePacketData{std::move(pv), generation} {}

  static constexpr size_t kMaxNumExtraCounters = 8;

  int64_t thread_timestamp = 0;
  int64_t thread_instruction_count = 0;
  int64_t counter_value = 0;
  std::array<int64_t, kMaxNumExtraCounters> extra_counter_values = {};
};

// A TimestampedTracePiece is (usually a reference to) a piece of a trace that
// is sorted by TraceSorter.
struct TimestampedTracePiece {
  enum class Type {
    kInvalid = 0,
    kFtraceEvent,
    kTracePacket,
    kInlineSchedSwitch,
    kInlineSchedWaking,
    kJsonValue,
    kFuchsiaRecord,
    kTrackEvent,
    kSystraceLine,
  };

  TimestampedTracePiece(int64_t ts,
                        uint64_t idx,
                        TraceBlobView tbv,
                        PacketSequenceStateGeneration* sequence_state)
      : packet_data{std::move(tbv), sequence_state},
        timestamp(ts),
        packet_idx(idx),
        type(Type::kTracePacket) {}

  TimestampedTracePiece(int64_t ts, uint64_t idx, TraceBlobView tbv)
      : ftrace_event(std::move(tbv)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kFtraceEvent) {}

  TimestampedTracePiece(int64_t ts,
                        uint64_t idx,
                        std::unique_ptr<Json::Value> value)
      : json_value(std::move(value)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kJsonValue) {}

  TimestampedTracePiece(int64_t ts,
                        uint64_t idx,
                        std::unique_ptr<FuchsiaRecord> fr)
      : fuchsia_record(std::move(fr)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kFuchsiaRecord) {}

  TimestampedTracePiece(int64_t ts,
                        uint64_t idx,
                        std::unique_ptr<TrackEventData> ted)
      : track_event_data(std::move(ted)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kTrackEvent) {}

  TimestampedTracePiece(int64_t ts,
                        uint64_t idx,
                        std::unique_ptr<SystraceLine> ted)
      : systrace_line(std::move(ted)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kSystraceLine) {}

  TimestampedTracePiece(int64_t ts, uint64_t idx, InlineSchedSwitch iss)
      : sched_switch(std::move(iss)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kInlineSchedSwitch) {}

  TimestampedTracePiece(int64_t ts, uint64_t idx, InlineSchedWaking isw)
      : sched_waking(std::move(isw)),
        timestamp(ts),
        packet_idx(idx),
        type(Type::kInlineSchedWaking) {}

  TimestampedTracePiece(TimestampedTracePiece&& ttp) noexcept {
    // Adopt |ttp|'s data. We have to use placement-new to fill the fields
    // because their original values may be uninitialized and thus
    // move-assignment won't work correctly.
    switch (ttp.type) {
      case Type::kInvalid:
        break;
      case Type::kFtraceEvent:
        new (&ftrace_event) TraceBlobView(std::move(ttp.ftrace_event));
        break;
      case Type::kTracePacket:
        new (&packet_data) TracePacketData(std::move(ttp.packet_data));
        break;
      case Type::kInlineSchedSwitch:
        new (&sched_switch) InlineSchedSwitch(std::move(ttp.sched_switch));
        break;
      case Type::kInlineSchedWaking:
        new (&sched_waking) InlineSchedWaking(std::move(ttp.sched_waking));
        break;
      case Type::kJsonValue:
        new (&json_value)
            std::unique_ptr<Json::Value>(std::move(ttp.json_value));
        break;
      case Type::kFuchsiaRecord:
        new (&fuchsia_record)
            std::unique_ptr<FuchsiaRecord>(std::move(ttp.fuchsia_record));
        break;
      case Type::kTrackEvent:
        new (&track_event_data)
            std::unique_ptr<TrackEventData>(std::move(ttp.track_event_data));
        break;
      case Type::kSystraceLine:
        new (&systrace_line)
            std::unique_ptr<SystraceLine>(std::move(ttp.systrace_line));
    }
    timestamp = ttp.timestamp;
    packet_idx = ttp.packet_idx;
    type = ttp.type;

    // Invalidate |ttp|.
    ttp.type = Type::kInvalid;
  }

  TimestampedTracePiece& operator=(TimestampedTracePiece&& ttp) {
    if (this != &ttp) {
      // First invoke the destructor and then invoke the move constructor
      // inline via placement-new to implement move-assignment.
      this->~TimestampedTracePiece();
      new (this) TimestampedTracePiece(std::move(ttp));
    }
    return *this;
  }

  TimestampedTracePiece(const TimestampedTracePiece&) = delete;
  TimestampedTracePiece& operator=(const TimestampedTracePiece&) = delete;

  ~TimestampedTracePiece() {
    switch (type) {
      case Type::kInvalid:
      case Type::kInlineSchedSwitch:
      case Type::kInlineSchedWaking:
        break;
      case Type::kFtraceEvent:
        ftrace_event.~TraceBlobView();
        break;
      case Type::kTracePacket:
        packet_data.~TracePacketData();
        break;
      case Type::kJsonValue:
        json_value.~unique_ptr();
        break;
      case Type::kFuchsiaRecord:
        fuchsia_record.~unique_ptr();
        break;
      case Type::kTrackEvent:
        track_event_data.~unique_ptr();
        break;
      case Type::kSystraceLine:
        systrace_line.~unique_ptr();
        break;
    }
  }

  // For std::lower_bound().
  static inline bool Compare(const TimestampedTracePiece& x, int64_t ts) {
    return x.timestamp < ts;
  }

  // For std::sort().
  inline bool operator<(const TimestampedTracePiece& o) const {
    return timestamp < o.timestamp ||
           (timestamp == o.timestamp && packet_idx < o.packet_idx);
  }

  // Fields ordered for packing.

  // Data for different types of TimestampedTracePiece.
  union {
    TraceBlobView ftrace_event;
    TracePacketData packet_data;
    InlineSchedSwitch sched_switch;
    InlineSchedWaking sched_waking;
    std::unique_ptr<Json::Value> json_value;
    std::unique_ptr<FuchsiaRecord> fuchsia_record;
    std::unique_ptr<TrackEventData> track_event_data;
    std::unique_ptr<SystraceLine> systrace_line;
  };

  int64_t timestamp;
  uint64_t packet_idx;
  Type type;
};

}  // namespace trace_processor
}  // namespace perfetto

#if PERFETTO_BUILDFLAG(PERFETTO_COMPILER_GCC)
#pragma GCC diagnostic pop
#endif

#endif  // SRC_TRACE_PROCESSOR_TIMESTAMPED_TRACE_PIECE_H_
