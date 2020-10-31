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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_PARSER_H_

#include "perfetto/base/build_config.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/proto/args_table_utils.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/timestamped_trace_piece.h"

#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace Json {
class Value;
}

namespace perfetto {

namespace trace_processor {

// Field numbers to be added to args table automatically via reflection
//
// TODO(ddrone): replace with a predicate on field id to import new fields
// automatically
static constexpr uint16_t kReflectFields[] = {24, 25, 26, 27, 28, 29};

class PacketSequenceStateGeneration;
class TraceProcessorContext;

class TrackEventParser {
 public:
  explicit TrackEventParser(TraceProcessorContext* context);

  void ParseTrackDescriptor(protozero::ConstBytes);
  UniquePid ParseProcessDescriptor(protozero::ConstBytes);
  UniqueTid ParseThreadDescriptor(protozero::ConstBytes);

  void ParseTrackEvent(int64_t ts,
                       TrackEventData* event_data,
                       protozero::ConstBytes);

 private:
  class EventImporter;

  void ParseChromeProcessDescriptor(UniquePid, protozero::ConstBytes);
  void ParseChromeThreadDescriptor(UniqueTid, protozero::ConstBytes);
  void ParseCounterDescriptor(TrackId, protozero::ConstBytes);

  TraceProcessorContext* context_;
  ProtoToArgsTable proto_to_args_;

  const StringId counter_name_thread_time_id_;
  const StringId counter_name_thread_instruction_count_id_;
  const StringId task_file_name_args_key_id_;
  const StringId task_function_name_args_key_id_;
  const StringId task_line_number_args_key_id_;
  const StringId log_message_body_key_id_;
  const StringId raw_legacy_event_id_;
  const StringId legacy_event_passthrough_utid_id_;
  const StringId legacy_event_category_key_id_;
  const StringId legacy_event_name_key_id_;
  const StringId legacy_event_phase_key_id_;
  const StringId legacy_event_duration_ns_key_id_;
  const StringId legacy_event_thread_timestamp_ns_key_id_;
  const StringId legacy_event_thread_duration_ns_key_id_;
  const StringId legacy_event_thread_instruction_count_key_id_;
  const StringId legacy_event_thread_instruction_delta_key_id_;
  const StringId legacy_event_use_async_tts_key_id_;
  const StringId legacy_event_unscoped_id_key_id_;
  const StringId legacy_event_global_id_key_id_;
  const StringId legacy_event_local_id_key_id_;
  const StringId legacy_event_id_scope_key_id_;
  const StringId legacy_event_bind_id_key_id_;
  const StringId legacy_event_bind_to_enclosing_key_id_;
  const StringId legacy_event_flow_direction_key_id_;
  const StringId flow_direction_value_in_id_;
  const StringId flow_direction_value_out_id_;
  const StringId flow_direction_value_inout_id_;
  const StringId chrome_legacy_ipc_class_args_key_id_;
  const StringId chrome_legacy_ipc_line_args_key_id_;

  std::array<StringId, 38> chrome_legacy_ipc_class_ids_;
  std::array<StringId, 9> chrome_process_name_ids_;
  std::array<StringId, 14> chrome_thread_name_ids_;
  std::array<StringId, 4> counter_unit_ids_;

  std::vector<uint16_t> reflect_fields_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_PARSER_H_
