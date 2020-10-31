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

#include "src/tracing/test/fake_packet.h"

#include <ostream>

#include "perfetto/base/logging.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/protozero/proto_utils.h"
#include "src/tracing/core/trace_buffer.h"

using protozero::proto_utils::ParseVarInt;
using protozero::proto_utils::WriteVarInt;

namespace perfetto {

FakePacketFragment::FakePacketFragment(size_t size, char prefix) {
  // |size| has to be at least == 1, because one byte will be taken just by the
  // varint header.
  PERFETTO_CHECK(size >= 1);

  // Finding the |payload_size| from |size| is quite tricky:
  // A packet with 127 bytes of payload requires:
  //   1B header (to represent the number 127 in varint) + 127 = 128 bytes.
  // A packet with 128 bytes payload requires:
  //   2B header (to represent the number 128 in varint) + 128 = 130 bytes.
  // So the only way to generate a packet of 129 bytes in total we need to
  // generate a redundant varint header of 2 bytes + 127 bytes of payload.
  if (size <= 128) {
    header_[0] = static_cast<uint8_t>(size - 1);
    header_size_ = 1;
    payload_.resize(size - 1);
  } else if (size == 129) {
    header_[0] = 0x80 | 127;
    header_size_ = 2;
    payload_.resize(127);
  } else {
    WriteVarInt(size - 2, &header_[0]);
    header_size_ = 2;
    payload_.resize(size - 2);
  }
  // Fills the payload as follow: X00-X01-X02 (where X == |prefix|);
  for (size_t i = 0; i < payload_.size(); i++) {
    switch (i % 4) {
      case 0:
        payload_[i] = prefix;
        break;
      case 1:
        payload_[i] = '0' + ((i / 4 / 10) % 10);
        break;
      case 2:
        payload_[i] = '0' + ((i / 4) % 10);
        break;
      case 3:
        payload_[i] = '-';
        break;
    }
  }
}

FakePacketFragment::FakePacketFragment(const void* payload,
                                       size_t payload_size) {
  PERFETTO_CHECK(payload_size <= 4096 - 2);
  payload_.assign(reinterpret_cast<const char*>(payload), payload_size);
  uint8_t* end = WriteVarInt(payload_.size(), &header_[0]);
  header_size_ = static_cast<size_t>(end - &header_[0]);
}

void FakePacketFragment::CopyInto(std::vector<uint8_t>* data) const {
  data->insert(data->end(), &header_[0], &header_[0] + header_size_);
  data->insert(data->end(), payload_.begin(), payload_.end());
}

size_t FakePacketFragment::GetSizeHeader() const {
  uint64_t size = 0;
  ParseVarInt(&header_[0], &header_[0] + header_size_, &size);
  return static_cast<size_t>(size);
}

bool FakePacketFragment::operator==(const FakePacketFragment& o) const {
  if (payload_ != o.payload_)
    return false;
  PERFETTO_CHECK(GetSizeHeader() == o.GetSizeHeader());
  return true;
}

std::ostream& operator<<(std::ostream& os, const FakePacketFragment& packet) {
  return os << "{len:" << packet.payload().size() << ", payload:\""
            << packet.payload() << "\"}";
}

FakeChunk::FakeChunk(TraceBuffer* t, ProducerID p, WriterID w, ChunkID c)
    : trace_buffer_{t}, producer_id{p}, writer_id{w}, chunk_id{c} {}

FakeChunk& FakeChunk::AddPacket(size_t size, char seed, uint8_t packet_flag) {
  PERFETTO_DCHECK(size <= 4096);
  PERFETTO_CHECK(
      !(packet_flag &
        SharedMemoryABI::ChunkHeader::kFirstPacketContinuesFromPrevChunk) ||
      num_packets == 0);
  PERFETTO_CHECK(
      !(flags & SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk));
  flags |= packet_flag;
  FakePacketFragment(size, seed).CopyInto(&data);
  num_packets++;
  return *this;
}

FakeChunk& FakeChunk::AddPacket(std::initializer_list<uint8_t> raw) {
  data.insert(data.end(), raw.begin(), raw.end());
  num_packets++;
  return *this;
}

FakeChunk& FakeChunk::IncrementNumPackets() {
  num_packets++;
  return *this;
}

FakeChunk& FakeChunk::SetFlags(uint8_t flags_to_set) {
  flags |= flags_to_set;
  return *this;
}

FakeChunk& FakeChunk::ClearBytes(size_t offset, size_t len) {
  PERFETTO_DCHECK(offset + len <= data.size());
  memset(data.data() + offset, 0, len);
  return *this;
}

FakeChunk& FakeChunk::SetUID(uid_t u) {
  uid = u;
  return *this;
}

FakeChunk& FakeChunk::PadTo(size_t chunk_size) {
  PERFETTO_CHECK(chunk_size >=
                 data.size() + TraceBuffer::InlineChunkHeaderSize);
  size_t padding_size =
      chunk_size - (data.size() + TraceBuffer::InlineChunkHeaderSize);
  std::string padding_data(padding_size, 0);
  data.insert(data.end(), padding_data.begin(), padding_data.end());
  return *this;
}

size_t FakeChunk::CopyIntoTraceBuffer(bool chunk_complete) {
  trace_buffer_->CopyChunkUntrusted(producer_id, uid, writer_id, chunk_id,
                                    num_packets, flags, chunk_complete,
                                    data.data(), data.size());
  return data.size() + TraceBuffer::InlineChunkHeaderSize;
}

}  // namespace perfetto
