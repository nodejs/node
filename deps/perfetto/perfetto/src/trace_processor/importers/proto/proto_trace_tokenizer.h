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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/importers/gzip/gzip_utils.h"
#include "src/trace_processor/importers/proto/proto_incremental_state.h"
#include "src/trace_processor/trace_blob_view.h"

namespace protozero {
struct ConstBytes;
}

namespace perfetto {

namespace protos {
namespace pbzero {
class TracePacket_Decoder;
}  // namespace pbzero
}  // namespace protos

namespace trace_processor {

class PacketSequenceState;
class TraceProcessorContext;
class TraceSorter;
class TraceStorage;

// Reads a protobuf trace in chunks and extracts boundaries of trace packets
// (or subfields, for the case of ftrace) with their timestamps.
class ProtoTraceTokenizer : public ChunkedTraceReader {
 public:
  // |reader| is the abstract method of getting chunks of size |chunk_size_b|
  // from a trace file with these chunks parsed into |trace|.
  explicit ProtoTraceTokenizer(TraceProcessorContext*);
  ~ProtoTraceTokenizer() override;

  // ChunkedTraceReader implementation.
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t size) override;
  void NotifyEndOfFile() override;

 private:
  using ConstBytes = protozero::ConstBytes;
  util::Status ParseInternal(std::unique_ptr<uint8_t[]> owned_buf,
                             uint8_t* data,
                             size_t size);
  util::Status ParsePacket(TraceBlobView);
  util::Status ParseClockSnapshot(ConstBytes blob, uint32_t seq_id);
  void HandleIncrementalStateCleared(
      const protos::pbzero::TracePacket_Decoder&);
  void HandlePreviousPacketDropped(const protos::pbzero::TracePacket_Decoder&);
  void ParseTracePacketDefaults(const protos::pbzero::TracePacket_Decoder&,
                                TraceBlobView trace_packet_defaults);
  void ParseInternedData(const protos::pbzero::TracePacket_Decoder&,
                         TraceBlobView interned_data);
  PacketSequenceState* GetIncrementalStateForPacketSequence(
      uint32_t sequence_id) {
    if (!incremental_state)
      incremental_state.reset(new ProtoIncrementalState(context_));
    return incremental_state->GetOrCreateStateForPacketSequence(sequence_id);
  }

  TraceProcessorContext* context_;

  // Used to glue together trace packets that span across two (or more)
  // Parse() boundaries.
  std::vector<uint8_t> partial_buf_;

  // Temporary. Currently trace packets do not have a timestamp, so the
  // timestamp given is latest_timestamp_.
  int64_t latest_timestamp_ = 0;

  // Stores incremental state and references to interned data, e.g. for track
  // event protos.
  std::unique_ptr<ProtoIncrementalState> incremental_state;

  // Allows support for compressed trace packets.
  GzipDecompressor decompressor_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_
