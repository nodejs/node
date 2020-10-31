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

#include "src/profiling/memory/wire_protocol.h"

#include <sys/socket.h>
#include <sys/types.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_socket.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {

bool operator==(const AllocMetadata& one, const AllocMetadata& other);
bool operator==(const AllocMetadata& one, const AllocMetadata& other) {
  return std::tie(one.sequence_number, one.alloc_size, one.alloc_address,
                  one.stack_pointer, one.stack_pointer_offset, one.arch) ==
             std::tie(other.sequence_number, other.alloc_size,
                      other.alloc_address, other.stack_pointer,
                      other.stack_pointer_offset, other.arch) &&
         memcmp(one.register_data, other.register_data, kMaxRegisterDataSize) ==
             0;
}

bool operator==(const FreeBatch& one, const FreeBatch& other);
bool operator==(const FreeBatch& one, const FreeBatch& other) {
  if (one.num_entries != other.num_entries)
    return false;
  for (size_t i = 0; i < one.num_entries; ++i) {
    if (std::tie(one.entries[i].sequence_number, one.entries[i].addr) !=
        std::tie(other.entries[i].sequence_number, other.entries[i].addr))
      return false;
  }
  return true;
}

namespace {

base::ScopedFile CopyFD(int fd) {
  int sv[2];
  PERFETTO_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  base::UnixSocketRaw send_sock(base::ScopedFile(sv[0]),
                                base::SockFamily::kUnix,
                                base::SockType::kStream);
  base::UnixSocketRaw recv_sock(base::ScopedFile(sv[1]),
                                base::SockFamily::kUnix,
                                base::SockType::kStream);
  char msg[] = "a";
  PERFETTO_CHECK(send_sock.Send(msg, sizeof(msg), &fd, 1));
  base::ScopedFile res;
  recv_sock.Receive(msg, sizeof(msg), &res, 1);
  return res;
}

constexpr auto kShmemSize = 1048576;

TEST(WireProtocolTest, AllocMessage) {
  char payload[] = {0x77, 0x77, 0x77, 0x00};
  WireMessage msg = {};
  msg.record_type = RecordType::Malloc;
  AllocMetadata metadata = {};
  metadata.sequence_number = 0xA1A2A3A4A5A6A7A8;
  metadata.alloc_size = 0xB1B2B3B4B5B6B7B8;
  metadata.alloc_address = 0xC1C2C3C4C5C6C7C8;
  metadata.stack_pointer = 0xD1D2D3D4D5D6D7D8;
  metadata.stack_pointer_offset = 0xE1E2E3E4E5E6E7E8;
  metadata.arch = unwindstack::ARCH_X86;
  for (size_t i = 0; i < kMaxRegisterDataSize; ++i)
    metadata.register_data[i] = 0x66;
  msg.alloc_header = &metadata;
  msg.payload = payload;
  msg.payload_size = sizeof(payload);

  auto shmem_client = SharedRingBuffer::Create(kShmemSize);
  ASSERT_TRUE(shmem_client);
  ASSERT_TRUE(shmem_client->is_valid());
  auto shmem_server = SharedRingBuffer::Attach(CopyFD(shmem_client->fd()));

  ASSERT_TRUE(SendWireMessage(&shmem_client.value(), msg));

  auto buf = shmem_server->BeginRead();
  ASSERT_TRUE(buf);
  WireMessage recv_msg;
  ASSERT_TRUE(ReceiveWireMessage(reinterpret_cast<char*>(buf.data), buf.size,
                                 &recv_msg));
  shmem_server->EndRead(std::move(buf));

  ASSERT_EQ(recv_msg.record_type, msg.record_type);
  ASSERT_EQ(*recv_msg.alloc_header, *msg.alloc_header);
  ASSERT_EQ(recv_msg.payload_size, msg.payload_size);
  ASSERT_STREQ(recv_msg.payload, msg.payload);
}

TEST(WireProtocolTest, FreeMessage) {
  WireMessage msg = {};
  msg.record_type = RecordType::Free;
  FreeBatch batch = {};
  batch.num_entries = kFreeBatchSize;
  for (size_t i = 0; i < kFreeBatchSize; ++i) {
    batch.entries[i].sequence_number = 0x111111111111111;
    batch.entries[i].addr = 0x222222222222222;
  }
  msg.free_header = &batch;

  auto shmem_client = SharedRingBuffer::Create(kShmemSize);
  ASSERT_TRUE(shmem_client);
  ASSERT_TRUE(shmem_client->is_valid());
  auto shmem_server = SharedRingBuffer::Attach(CopyFD(shmem_client->fd()));

  ASSERT_TRUE(SendWireMessage(&shmem_client.value(), msg));

  auto buf = shmem_server->BeginRead();
  ASSERT_TRUE(buf);
  WireMessage recv_msg;
  ASSERT_TRUE(ReceiveWireMessage(reinterpret_cast<char*>(buf.data), buf.size,
                                 &recv_msg));
  shmem_server->EndRead(std::move(buf));

  ASSERT_EQ(recv_msg.record_type, msg.record_type);
  ASSERT_EQ(*recv_msg.free_header, *msg.free_header);
  ASSERT_EQ(recv_msg.payload_size, msg.payload_size);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
