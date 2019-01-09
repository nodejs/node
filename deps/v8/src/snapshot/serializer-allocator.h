// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_ALLOCATOR_H_
#define V8_SNAPSHOT_SERIALIZER_ALLOCATOR_H_

#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

class Serializer;

class SerializerAllocator final {
 public:
  explicit SerializerAllocator(Serializer* serializer);

  SerializerReference Allocate(AllocationSpace space, uint32_t size);
  SerializerReference AllocateMap();
  SerializerReference AllocateLargeObject(uint32_t size);
  SerializerReference AllocateOffHeapBackingStore();

  void UseCustomChunkSize(uint32_t chunk_size);

#ifdef DEBUG
  bool BackReferenceIsAlreadyAllocated(
      SerializerReference back_reference) const;
#endif

  std::vector<SerializedData::Reservation> EncodeReservations() const;

  void OutputStatistics();

 private:
  // We try to not exceed this size for every chunk. We will not succeed for
  // larger objects though.
  uint32_t TargetChunkSize(int space);

  static constexpr int kNumberOfPreallocatedSpaces =
      SerializerDeserializer::kNumberOfPreallocatedSpaces;
  static constexpr int kNumberOfSpaces =
      SerializerDeserializer::kNumberOfSpaces;

  // Objects from the same space are put into chunks for bulk-allocation
  // when deserializing. We have to make sure that each chunk fits into a
  // page. So we track the chunk size in pending_chunk_ of a space, but
  // when it exceeds a page, we complete the current chunk and start a new one.
  uint32_t pending_chunk_[kNumberOfPreallocatedSpaces];
  std::vector<uint32_t> completed_chunks_[kNumberOfPreallocatedSpaces];

  // Number of maps that we need to allocate.
  uint32_t num_maps_ = 0;

  // We map serialized large objects to indexes for back-referencing.
  uint32_t large_objects_total_size_ = 0;
  uint32_t seen_large_objects_index_ = 0;

  // Used to keep track of the off-heap backing stores used by TypedArrays/
  // ArrayBuffers. Note that the index begins at 1 and not 0, because when a
  // TypedArray has an on-heap backing store, the backing_store pointer in the
  // corresponding ArrayBuffer will be null, which makes it indistinguishable
  // from index 0.
  uint32_t seen_backing_stores_index_ = 1;

  uint32_t custom_chunk_size_ = 0;

  // The current serializer.
  Serializer* const serializer_;

  DISALLOW_COPY_AND_ASSIGN(SerializerAllocator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_ALLOCATOR_H_
