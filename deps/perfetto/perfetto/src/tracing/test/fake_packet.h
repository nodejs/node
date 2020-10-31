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

#ifndef SRC_TRACING_TEST_FAKE_PACKET_H_
#define SRC_TRACING_TEST_FAKE_PACKET_H_

#include <stdint.h>

#include <array>
#include <initializer_list>
#include <string>
#include <vector>

#include "perfetto/ext/tracing/core/basic_types.h"

namespace perfetto {

class TraceBuffer;

class FakePacketFragment {
 public:
  // Generates a packet of given |size| with pseudo random payload. The |size|
  // argument includes the size of the varint header.
  FakePacketFragment(size_t size, char prefix);
  FakePacketFragment(const void* payload, size_t payload_size);
  void CopyInto(std::vector<uint8_t>* data) const;
  size_t GetSizeHeader() const;
  bool operator==(const FakePacketFragment& other) const;
  const std::string& payload() const { return payload_; }

 private:
  std::array<uint8_t, 2> header_{};
  size_t header_size_ = 0;
  std::string payload_;
};

std::ostream& operator<<(std::ostream&, const FakePacketFragment&);

class FakeChunk {
 public:
  FakeChunk(TraceBuffer* t, ProducerID p, WriterID w, ChunkID c);

  // Appends a packet of exactly |size| bytes (including the varint header
  // that states the size of the packet itself. The payload of the packet is
  // NOT a valid proto and is just filled with pseudo-random bytes generated
  // from |seed|.
  FakeChunk& AddPacket(size_t size, char seed, uint8_t packet_flag = 0);

  // Appends a packet with the given raw content (including varint header).
  FakeChunk& AddPacket(std::initializer_list<uint8_t>);

  // Increments the number of packets in the chunk without adding new data.
  FakeChunk& IncrementNumPackets();

  FakeChunk& SetFlags(uint8_t flags_to_set);

  FakeChunk& ClearBytes(size_t offset, size_t len);

  FakeChunk& SetUID(uid_t);

  // Extends the size of the FakeChunk to |chunk_size| by 0-filling.
  FakeChunk& PadTo(size_t chunk_size);

  // Returns the full size of the chunk including the ChunkRecord header.
  size_t CopyIntoTraceBuffer(bool chunk_complete = true);

 private:
  TraceBuffer* trace_buffer_;
  ProducerID producer_id;
  WriterID writer_id;
  ChunkID chunk_id;
  uint8_t flags = 0;
  uint16_t num_packets = 0;
  uid_t uid = kInvalidUid;
  std::vector<uint8_t> data;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_FAKE_PACKET_H_
