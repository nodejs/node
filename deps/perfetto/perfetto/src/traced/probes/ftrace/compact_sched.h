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

#ifndef SRC_TRACED_PROBES_FTRACE_COMPACT_SCHED_H_
#define SRC_TRACED_PROBES_FTRACE_COMPACT_SCHED_H_

#include <stdint.h>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/packed_repeated_fields.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "src/traced/probes/ftrace/event_info_constants.h"
#include "src/traced/probes/ftrace/ftrace_config_utils.h"

namespace perfetto {

// The subset of the sched_switch event's format that is used when parsing and
// encoding into the compact format.
struct CompactSchedSwitchFormat {
  uint32_t event_id;
  uint16_t size;

  uint16_t next_pid_offset;
  FtraceFieldType next_pid_type;
  uint16_t next_prio_offset;
  FtraceFieldType next_prio_type;
  uint16_t prev_state_offset;
  FtraceFieldType prev_state_type;
  uint16_t next_comm_offset;
};

// The subset of the sched_waking event's format that is used when parsing and
// encoding into the compact format.
struct CompactSchedWakingFormat {
  uint32_t event_id;
  uint16_t size;

  uint16_t pid_offset;
  FtraceFieldType pid_type;
  uint16_t target_cpu_offset;
  FtraceFieldType target_cpu_type;
  uint16_t prio_offset;
  FtraceFieldType prio_type;
  uint16_t comm_offset;
};

// Pre-parsed format of a subset of scheduling events, for use during ftrace
// parsing if compact encoding is enabled. Holds a flag, |format_valid| to
// state whether the compile-time assumptions about the format held at runtime.
// If they didn't, we cannot use the compact encoding.
struct CompactSchedEventFormat {
  // If false, the rest of the struct is considered invalid.
  const bool format_valid;

  const CompactSchedSwitchFormat sched_switch;
  const CompactSchedWakingFormat sched_waking;
};

CompactSchedEventFormat ValidateFormatForCompactSched(
    const std::vector<Event>& events);

CompactSchedEventFormat InvalidCompactSchedEventFormatForTesting();

// Compact encoding configuration used at ftrace reading & parsing time.
struct CompactSchedConfig {
  CompactSchedConfig(bool _enabled) : enabled(_enabled) {}

  // If true, and sched_switch and/or sched_waking events are enabled, encode
  // them in a compact format instead of the normal form.
  const bool enabled = false;
};

CompactSchedConfig CreateCompactSchedConfig(
    const FtraceConfig& request,
    const CompactSchedEventFormat& compact_format);

CompactSchedConfig EnabledCompactSchedConfigForTesting();
CompactSchedConfig DisabledCompactSchedConfigForTesting();

// Collects fields of sched_switch events, allowing them to be written out
// in a compact encoding.
class CompactSchedSwitchBuffer {
 public:
  protozero::PackedVarInt& timestamp() { return timestamp_; }
  protozero::PackedVarInt& prev_state() { return prev_state_; }
  protozero::PackedVarInt& next_pid() { return next_pid_; }
  protozero::PackedVarInt& next_prio() { return next_prio_; }
  protozero::PackedVarInt& next_comm_index() { return next_comm_index_; }

  size_t size() const {
    // Caller should fill all per-field buffers at the same rate.
    return timestamp_.size();
  }

  inline void AppendTimestamp(uint64_t timestamp) {
    timestamp_.Append(timestamp - last_timestamp_);
    last_timestamp_ = timestamp;
  }

  void Write(
      protos::pbzero::FtraceEventBundle::CompactSched* compact_out) const;
  void Reset();

 private:
  // First timestamp in a bundle is absolute. The rest are all delta-encoded,
  // each relative to the preceding sched_switch timestamp.
  uint64_t last_timestamp_ = 0;

  protozero::PackedVarInt timestamp_;
  protozero::PackedVarInt prev_state_;
  protozero::PackedVarInt next_pid_;
  protozero::PackedVarInt next_prio_;
  // Interning indices of the next_comm values. See |CommInterner|.
  protozero::PackedVarInt next_comm_index_;
};

// As |CompactSchedSwitchBuffer|, but for sched_waking events.
class CompactSchedWakingBuffer {
 public:
  protozero::PackedVarInt& pid() { return pid_; }
  protozero::PackedVarInt& target_cpu() { return target_cpu_; }
  protozero::PackedVarInt& prio() { return prio_; }
  protozero::PackedVarInt& comm_index() { return comm_index_; }

  size_t size() const {
    // Caller should fill all per-field buffers at the same rate.
    return timestamp_.size();
  }

  inline void AppendTimestamp(uint64_t timestamp) {
    timestamp_.Append(timestamp - last_timestamp_);
    last_timestamp_ = timestamp;
  }

  void Write(
      protos::pbzero::FtraceEventBundle::CompactSched* compact_out) const;
  void Reset();

 private:
  uint64_t last_timestamp_ = 0;

  protozero::PackedVarInt timestamp_;
  protozero::PackedVarInt pid_;
  protozero::PackedVarInt target_cpu_;
  protozero::PackedVarInt prio_;
  // Interning indices of the comm values. See |CommInterner|.
  protozero::PackedVarInt comm_index_;
};

class CommInterner {
 public:
  static constexpr size_t kExpectedCommLength = 16;

  size_t InternComm(const char* ptr) {
    // Linearly scan existing string views, ftrace reader will
    // make sure this set doesn't grow too large.
    base::StringView transient_view(ptr);
    for (size_t i = 0; i < interned_comms_size_; i++) {
      if (transient_view == interned_comms_[i]) {
        return i;
      }
    }

    // Unique comm, intern it. Null byte is not copied over.
    char* start = intern_buf_ + intern_buf_write_pos_;
    size_t size = transient_view.size();
    memcpy(start, ptr, size);
    intern_buf_write_pos_ += size;

    size_t idx = interned_comms_size_;
    base::StringView safe_view(start, size);
    interned_comms_[interned_comms_size_++] = safe_view;

    PERFETTO_DCHECK(intern_buf_write_pos_ <= sizeof(intern_buf_));
    PERFETTO_DCHECK(interned_comms_size_ < kMaxElements);
    return idx;
  }

  size_t interned_comms_size() const { return interned_comms_size_; }

  void Write(
      protos::pbzero::FtraceEventBundle::CompactSched* compact_out) const;
  void Reset();

 private:
  // TODO(rsavitski): Consider making the storage dynamically-expandable instead
  // to not rely on sizing the buffer for the worst case.
  static constexpr size_t kMaxElements = 4096;

  char intern_buf_[kMaxElements * (kExpectedCommLength - 1)];
  size_t intern_buf_write_pos_ = 0;

  // Views into unique interned comm strings. Even if every event carries a
  // unique comm, the ftrace reader is expected to flush the compact buffer way
  // before this reaches capacity. This is since the cost of processing each
  // event grows with every unique interned comm (as the interning needs to
  // search all existing internings).
  std::array<base::StringView, kMaxElements> interned_comms_;
  uint32_t interned_comms_size_ = 0;
};

// Mutable state for buffering parts of scheduling events, that can later be
// written out in a compact format with |WriteAndReset|. Used by the ftrace
// reader.
class CompactSchedBuffer {
 public:
  CompactSchedSwitchBuffer& sched_switch() { return switch_; }
  CompactSchedWakingBuffer& sched_waking() { return waking_; }
  CommInterner& interner() { return interner_; }

  // Writes out the currently buffered events, and starts the next batch
  // internally.
  void WriteAndReset(protos::pbzero::FtraceEventBundle* bundle);

 private:
  CommInterner interner_;
  CompactSchedSwitchBuffer switch_;
  CompactSchedWakingBuffer waking_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_COMPACT_SCHED_H_
