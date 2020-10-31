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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_MODULE_H_

#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/proto/heap_graph_tracker.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/timestamped_trace_piece.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

class HeapGraphModule : public ProtoImporterModule {
 public:
  explicit HeapGraphModule(TraceProcessorContext* context);

  void ParsePacket(const protos::pbzero::TracePacket::Decoder& decoder,
                   const TimestampedTracePiece& ttp,
                   uint32_t field_id) override;

  void NotifyEndOfFile() override;

 private:
  void ParseHeapGraph(uint32_t seq_id, int64_t ts, protozero::ConstBytes);
  void ParseDeobfuscationMapping(protozero::ConstBytes);
  void DeobfuscateClass(base::Optional<StringPool::Id> package_name_id,
                        StringPool::Id obfuscated_class_id,
                        const protos::pbzero::ObfuscatedClass::Decoder& cls);

  TraceProcessorContext* context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_MODULE_H_
