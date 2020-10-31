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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_MODULE_H_

#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/proto_incremental_state.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

// Importer module for heap and CPU sampling profile data.
// TODO(eseckler): Currently handles only StreamingProfilePackets. Also move
// other profiling data import functionality into this module.
class ProfileModule : public ProtoImporterModule {
 public:
  explicit ProfileModule(TraceProcessorContext* context);
  ~ProfileModule() override;

  ModuleResult TokenizePacket(
      const protos::pbzero::TracePacket::Decoder& decoder,
      TraceBlobView* packet,
      int64_t packet_timestamp,
      PacketSequenceState* state,
      uint32_t field_id) override;

  void ParsePacket(const protos::pbzero::TracePacket::Decoder& decoder,
                   const TimestampedTracePiece& ttp,
                   uint32_t field_id) override;

 private:
  ModuleResult TokenizeStreamingProfilePacket(
      PacketSequenceState*,
      TraceBlobView* packet,
      protozero::ConstBytes streaming_profile_packet);
  void ParseStreamingProfilePacket(
      int64_t timestamp,
      PacketSequenceStateGeneration*,
      protozero::ConstBytes streaming_profile_packet);

  TraceProcessorContext* context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_MODULE_H_
