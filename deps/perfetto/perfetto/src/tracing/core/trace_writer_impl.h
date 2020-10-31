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

#ifndef SRC_TRACING_CORE_TRACE_WRITER_IMPL_H_
#define SRC_TRACING_CORE_TRACE_WRITER_IMPL_H_

#include "perfetto/base/proc_utils.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/protozero/message_handle.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "perfetto/tracing/buffer_exhausted_policy.h"
#include "src/tracing/core/patch_list.h"

namespace perfetto {

class SharedMemoryArbiterImpl;

// See //include/perfetto/tracing/core/trace_writer.h for docs.
class TraceWriterImpl : public TraceWriter,
                        public protozero::ScatteredStreamWriter::Delegate {
 public:
  // TracePacketHandle is defined in trace_writer.h
  TraceWriterImpl(SharedMemoryArbiterImpl*,
                  WriterID,
                  MaybeUnboundBufferID buffer_id,
                  BufferExhaustedPolicy);
  ~TraceWriterImpl() override;

  // TraceWriter implementation. See documentation in trace_writer.h.
  TracePacketHandle NewTracePacket() override;
  void Flush(std::function<void()> callback = {}) override;
  WriterID writer_id() const override;
  uint64_t written() const override {
    return protobuf_stream_writer_.written();
  }

  void ResetChunkForTesting() { cur_chunk_ = SharedMemoryABI::Chunk(); }
  bool drop_packets_for_testing() const { return drop_packets_; }

 private:
  TraceWriterImpl(const TraceWriterImpl&) = delete;
  TraceWriterImpl& operator=(const TraceWriterImpl&) = delete;

  // ScatteredStreamWriter::Delegate implementation.
  protozero::ContiguousMemoryRange GetNewBuffer() override;

  // The per-producer arbiter that coordinates access to the shared memory
  // buffer from several threads.
  SharedMemoryArbiterImpl* const shmem_arbiter_;

  // ID of the current writer.
  const WriterID id_;

  // This is copied into the commit request by SharedMemoryArbiter. See comments
  // in data_source_config.proto for |target_buffer|. If this is a reservation
  // for a buffer ID in case of a startup trace writer, SharedMemoryArbiterImpl
  // will also translate the reservation ID to the actual buffer ID.
  const MaybeUnboundBufferID target_buffer_;

  // Whether GetNewChunk() should stall or return an invalid chunk if the SMB is
  // exhausted.
  const BufferExhaustedPolicy buffer_exhausted_policy_;

  // Monotonic (% wrapping) sequence id of the chunk. Together with the WriterID
  // this allows the Service to reconstruct the linear sequence of packets.
  ChunkID next_chunk_id_ = 0;

  // The chunk we are holding onto (if any).
  SharedMemoryABI::Chunk cur_chunk_;

  // Passed to protozero message to write directly into |cur_chunk_|. It
  // keeps track of the write pointer. It calls us back (GetNewBuffer()) when
  // |cur_chunk_| is filled.
  protozero::ScatteredStreamWriter protobuf_stream_writer_;

  // The packet returned via NewTracePacket(). Its owned by this class,
  // TracePacketHandle has just a pointer to it.
  std::unique_ptr<protos::pbzero::TracePacket> cur_packet_;

  // The start address of |cur_packet_| within |cur_chunk_|. Used to figure out
  // fragments sizes when a TracePacket write is interrupted by GetNewBuffer().
  uint8_t* cur_fragment_start_ = nullptr;

  // true if we received a call to GetNewBuffer() after NewTracePacket(),
  // false if GetNewBuffer() happened during NewTracePacket() prologue, while
  // starting the TracePacket header.
  bool fragmenting_packet_ = false;

  // Set to |true| when the current chunk contains the maximum number of packets
  // a chunk can contain. When this is |true|, the next packet requires starting
  // a new chunk.
  bool reached_max_packets_per_chunk_ = false;

  // If we fail to acquire a new chunk when the arbiter operates in
  // SharedMemory::BufferExhaustedPolicy::kDrop mode, the trace writer enters a
  // mode in which data is written to a local garbage chunk and dropped.
  bool drop_packets_ = false;

  // Whether the trace writer should try to acquire a new chunk from the SMB
  // when the next TracePacket is started because it filled the garbage chunk at
  // least once since the last attempt.
  bool retry_new_chunk_after_packet_ = false;

  // Points to the size field of the last packet we wrote to the current chunk.
  // If the chunk was already returned, this is reset to |nullptr|.
  uint8_t* last_packet_size_field_ = nullptr;

  // When a packet is fragmented across different chunks, the |size_field| of
  // the outstanding nested protobuf messages is redirected onto Patch entries
  // in this list at the time the Chunk is returned (because at that point we
  // have to release the ownership of the current Chunk). This list will be
  // later sent out-of-band to the tracing service, who will patch the required
  // chunks, if they are still around.
  PatchList patch_list_;

  // PID of the process that created the trace writer. Used for a DCHECK that
  // aims to detect unsupported process forks while tracing.
  const base::PlatformProcessId process_id_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_TRACE_WRITER_IMPL_H_
