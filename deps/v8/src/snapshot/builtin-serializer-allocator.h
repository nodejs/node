// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_SERIALIZER_ALLOCATOR_H_
#define V8_SNAPSHOT_BUILTIN_SERIALIZER_ALLOCATOR_H_

#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

template <class AllocatorT>
class Serializer;

class BuiltinSerializerAllocator final {
 public:
  BuiltinSerializerAllocator(
      Serializer<BuiltinSerializerAllocator>* serializer) {}

  SerializerReference Allocate(AllocationSpace space, uint32_t size);
  SerializerReference AllocateMap() { UNREACHABLE(); }
  SerializerReference AllocateLargeObject(uint32_t size) { UNREACHABLE(); }
  SerializerReference AllocateOffHeapBackingStore() { UNREACHABLE(); }

#ifdef DEBUG
  bool BackReferenceIsAlreadyAllocated(
      SerializerReference back_reference) const;
#endif

  std::vector<SerializedData::Reservation> EncodeReservations() const;

  void OutputStatistics();

 private:
  static constexpr int kNumberOfPreallocatedSpaces =
      SerializerDeserializer::kNumberOfPreallocatedSpaces;
  static constexpr int kNumberOfSpaces =
      SerializerDeserializer::kNumberOfSpaces;

  // We need to track a faked offset to create back-references. The size is
  // kept simply to display statistics.
  uint32_t virtual_chunk_size_ = 0;
  uint32_t virtual_chunk_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BuiltinSerializerAllocator)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_SERIALIZER_ALLOCATOR_H_
