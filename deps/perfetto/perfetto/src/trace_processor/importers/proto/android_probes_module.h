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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_MODULE_H_

#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/proto/android_probes_parser.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/timestamped_trace_piece.h"

#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

class AndroidProbesModule : public ProtoImporterModule {
 public:
  explicit AndroidProbesModule(TraceProcessorContext* context);

  ModuleResult TokenizePacket(const protos::pbzero::TracePacket_Decoder&,
                              TraceBlobView* packet,
                              int64_t packet_timestamp,
                              PacketSequenceState*,
                              uint32_t field_id) override;

  void ParsePacket(const protos::pbzero::TracePacket::Decoder& decoder,
                   const TimestampedTracePiece& ttp,
                   uint32_t field_id) override;

  void ParseTraceConfig(
      const protos::pbzero::TraceConfig::Decoder& decoder) override;

 private:
  AndroidProbesParser parser_;
  TraceProcessorContext* context_ = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_MODULE_H_
