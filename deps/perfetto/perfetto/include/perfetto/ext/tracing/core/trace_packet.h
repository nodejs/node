/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACE_PACKET_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACE_PACKET_H_

#include <stddef.h>
#include <memory>
#include <tuple>

#include "perfetto/base/export.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/tracing/core/slice.h"

namespace perfetto {

// A wrapper around a byte buffer that contains a protobuf-encoded TracePacket
// (see trace_packet.proto). The TracePacket is decoded only if the Consumer
// requests that. This is to allow Consumer(s) to just stream the packet over
// the network or save it to a file without wasting time decoding it and without
// needing to depend on libprotobuf or the trace_packet.pb.h header.
// If the packets are saved / streamed and not just consumed locally, consumers
// should ensure to preserve the unknown fields in the proto. A consumer, in
// fact, might have an older version .proto which is newer on the producer.
class PERFETTO_EXPORT TracePacket {
 public:
  using const_iterator = Slices::const_iterator;

  // The field id of protos::Trace::packet, static_assert()-ed in the unittest.
  static constexpr uint32_t kPacketFieldNumber = 1;

  // Maximum size of the preamble returned by GetProtoPreamble().
  static constexpr size_t kMaxPreambleBytes = 8;

  TracePacket();
  ~TracePacket();
  TracePacket(TracePacket&&) noexcept;
  TracePacket& operator=(TracePacket&&);

  // Accesses all the raw slices in the packet, for saving them to file/network.
  const Slices& slices() const { return slices_; }

  // Mutator, used only by the service and tests.
  void AddSlice(Slice);

  // Does not copy / take ownership of the memory of the slice. The TracePacket
  // will be valid only as long as the original buffer is valid.
  void AddSlice(const void* start, size_t size);

  // Total size of all slices.
  size_t size() const { return size_; }

  // Generates a protobuf preamble suitable to represent this packet as a
  // repeated field within a root trace.proto message.
  // Returns a pointer to a buffer, owned by this class, containing the preamble
  // and its size.
  std::tuple<char*, size_t> GetProtoPreamble();

  // Returns the raw protobuf bytes of the slices, all stitched together into
  // a string. Only for testing.
  std::string GetRawBytesForTesting();

 private:
  TracePacket(const TracePacket&) = delete;
  TracePacket& operator=(const TracePacket&) = delete;

  Slices slices_;     // Not owned.
  size_t size_ = 0;   // SUM(slice.size for slice in slices_).
  char preamble_[kMaxPreambleBytes];  // Deliberately not initialized.

  // Remember to update the move operators and their unittest if adding new
  // fields. ConsumerIPCClientImpl::OnReadBuffersResponse() relies on
  // std::move(TracePacket) to clear up the moved-from instance.
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACE_PACKET_H_
