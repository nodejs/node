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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_PARSER_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_parser.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class TracePacket_Decoder;
}  // namespace pbzero
}  // namespace protos

namespace trace_processor {

class ArgsTracker;
class PacketSequenceState;
class TraceProcessorContext;

class ProtoTraceParser : public TraceParser {
 public:
  using ConstBytes = protozero::ConstBytes;
  explicit ProtoTraceParser(TraceProcessorContext*);
  ~ProtoTraceParser() override;

  // TraceParser implementation.
  void ParseTracePacket(int64_t timestamp, TimestampedTracePiece) override;
  void ParseFtracePacket(uint32_t cpu,
                         int64_t timestamp,
                         TimestampedTracePiece) override;

  void ParseTracePacketImpl(int64_t ts,
                            TimestampedTracePiece,
                            const TracePacketData*,
                            const protos::pbzero::TracePacket_Decoder&);

  void ParseTraceStats(ConstBytes);
  void ParseProfilePacket(int64_t ts,
                          PacketSequenceStateGeneration*,
                          uint32_t seq_id,
                          ConstBytes);
  void ParsePerfSample(int64_t ts, PacketSequenceStateGeneration*, ConstBytes);
  void ParseChromeBenchmarkMetadata(ConstBytes);
  void ParseChromeEvents(int64_t ts, ConstBytes);
  void ParseMetatraceEvent(int64_t ts, ConstBytes);
  void ParseTraceConfig(ConstBytes);
  void ParseModuleSymbols(ConstBytes);
  void ParseTrigger(int64_t ts, ConstBytes);
  void ParseServiceEvent(int64_t ts, ConstBytes);
  void ParseSmapsPacket(int64_t ts, ConstBytes);

 private:
  TraceProcessorContext* context_;

  const StringId metatrace_id_;
  const StringId data_name_id_;
  const StringId raw_chrome_metadata_event_id_;
  const StringId raw_chrome_legacy_system_trace_event_id_;
  const StringId raw_chrome_legacy_user_trace_event_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_PARSER_H_
