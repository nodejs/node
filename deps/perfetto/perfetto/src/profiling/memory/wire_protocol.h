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

// The data types used for communication between heapprofd and the client
// embedded in processes that are being profiled.

#ifndef SRC_PROFILING_MEMORY_WIRE_PROTOCOL_H_
#define SRC_PROFILING_MEMORY_WIRE_PROTOCOL_H_

#include <inttypes.h>
#include <unwindstack/Elf.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>

#include "src/profiling/memory/shared_ring_buffer.h"

namespace perfetto {

namespace base {
class UnixSocketRaw;
}

namespace profiling {

struct ClientConfiguration {
  // On average, sample one allocation every interval bytes,
  // If interval == 1, sample every allocation.
  // Must be >= 1.
  uint64_t interval;
  bool block_client;
  uint64_t block_client_timeout_us;
  bool disable_fork_teardown;
  bool disable_vfork_detection;
};

// Types needed for the wire format used for communication between the client
// and heapprofd. The basic format of a record is
// record size (uint64_t) | record type (RecordType = uint64_t) | record
// If record type is malloc, the record format is AllocMetdata | raw stack.
// If the record type is free, the record is a sequence of FreeBatchEntry.

// Use uint64_t to make sure the following data is aligned as 64bit is the
// strongest alignment requirement.

// C++11 std::max is not constexpr.
constexpr size_t constexpr_max(size_t x, size_t y) {
  return x > y ? x : y;
}

// clang-format makes this unreadable. Turning it off for this block.
// clang-format off
constexpr size_t kMaxRegisterDataSize =
  constexpr_max(
    constexpr_max(
      constexpr_max(
        constexpr_max(
            constexpr_max(
              sizeof(uint32_t) * unwindstack::ARM_REG_LAST,
              sizeof(uint64_t) * unwindstack::ARM64_REG_LAST),
            sizeof(uint32_t) * unwindstack::X86_REG_LAST),
          sizeof(uint64_t) * unwindstack::X86_64_REG_LAST),
        sizeof(uint32_t) * unwindstack::MIPS_REG_LAST),
      sizeof(uint64_t) * unwindstack::MIPS64_REG_LAST
  );
// clang-format on

constexpr size_t kFreeBatchSize = 1024;

enum class RecordType : uint64_t {
  Free = 0,
  Malloc = 1,
};

struct AllocMetadata {
  uint64_t sequence_number;
  // Size of the allocation that was made.
  uint64_t alloc_size;
  // Total number of bytes attributed to this allocation.
  uint64_t sample_size;
  // Pointer returned by malloc(2) for this allocation.
  uint64_t alloc_address;
  // Current value of the stack pointer.
  uint64_t stack_pointer;
  // Offset of the data at stack_pointer from the start of this record.
  uint64_t stack_pointer_offset;
  uint64_t clock_monotonic_coarse_timestamp;
  alignas(uint64_t) char register_data[kMaxRegisterDataSize];
  // CPU architecture of the client.
  unwindstack::ArchEnum arch;
};

struct FreeBatchEntry {
  uint64_t sequence_number;
  uint64_t addr;
};

struct FreeBatch {
  uint64_t num_entries;
  uint64_t clock_monotonic_coarse_timestamp;
  FreeBatchEntry entries[kFreeBatchSize];

  FreeBatch() { num_entries = 0; }
};

enum HandshakeFDs : size_t {
  kHandshakeMaps = 0,
  kHandshakeMem,
  kHandshakePageIdle,
  kHandshakeSize,
};

struct WireMessage {
  RecordType record_type;

  AllocMetadata* alloc_header;
  FreeBatch* free_header;

  char* payload;
  size_t payload_size;
};

bool SendWireMessage(SharedRingBuffer* buf, const WireMessage& msg);

// Parse message received over the wire.
// |buf| has to outlive |out|.
// If buf is not a valid message, return false.
bool ReceiveWireMessage(char* buf, size_t size, WireMessage* out);

constexpr const char* kHeapprofdSocketEnvVar = "ANDROID_SOCKET_heapprofd";
constexpr const char* kHeapprofdSocketFile = "/dev/socket/heapprofd";

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_WIRE_PROTOCOL_H_
