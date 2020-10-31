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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TOKENIZER_H_

#include <stdint.h>

#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class ChromeThreadDescriptor_Decoder;
class ProcessDescriptor_Decoder;
class ThreadDescriptor_Decoder;
class TracePacket_Decoder;
}  // namespace pbzero
}  // namespace protos

namespace trace_processor {

class PacketSequenceState;
class TraceProcessorContext;
class TraceBlobView;

class TrackEventTokenizer {
 public:
  explicit TrackEventTokenizer(TraceProcessorContext* context);

  ModuleResult TokenizeTrackDescriptorPacket(
      PacketSequenceState* state,
      const protos::pbzero::TracePacket_Decoder&,
      int64_t packet_timestamp);
  ModuleResult TokenizeThreadDescriptorPacket(
      PacketSequenceState* state,
      const protos::pbzero::TracePacket_Decoder&);
  void TokenizeTrackEventPacket(PacketSequenceState* state,
                                const protos::pbzero::TracePacket_Decoder&,
                                TraceBlobView* packet,
                                int64_t packet_timestamp);

 private:
  void TokenizeThreadDescriptor(
      PacketSequenceState* state,
      const protos::pbzero::ThreadDescriptor_Decoder&);

  TraceProcessorContext* context_;

  const StringId counter_name_thread_time_id_;
  const StringId counter_name_thread_instruction_count_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TOKENIZER_H_
