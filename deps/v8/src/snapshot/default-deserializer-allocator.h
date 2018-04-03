// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_DEFAULT_DESERIALIZER_ALLOCATOR_H_
#define V8_SNAPSHOT_DEFAULT_DESERIALIZER_ALLOCATOR_H_

#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

template <class AllocatorT>
class Deserializer;

class BuiltinDeserializer;
class StartupDeserializer;

class DefaultDeserializerAllocator final {
 public:
  DefaultDeserializerAllocator(
      Deserializer<DefaultDeserializerAllocator>* deserializer);

  // ------- Allocation Methods -------
  // Methods related to memory allocation during deserialization.

  Address Allocate(AllocationSpace space, int size);

  void MoveToNextChunk(AllocationSpace space);
  void SetAlignment(AllocationAlignment alignment) {
    DCHECK_EQ(kWordAligned, next_alignment_);
    DCHECK_LE(kWordAligned, alignment);
    DCHECK_LE(alignment, kDoubleUnaligned);
    next_alignment_ = static_cast<AllocationAlignment>(alignment);
  }

  HeapObject* GetMap(uint32_t index);
  HeapObject* GetLargeObject(uint32_t index);
  HeapObject* GetObject(AllocationSpace space, uint32_t chunk_index,
                        uint32_t chunk_offset);

  // ------- Reservation Methods -------
  // Methods related to memory reservations (prior to deserialization).

  void DecodeReservation(std::vector<SerializedData::Reservation> res);
  bool ReserveSpace();

  // Atomically reserves space for the two given deserializers. Guarantees
  // reservation for both without garbage collection in-between.
  static bool ReserveSpace(StartupDeserializer* startup_deserializer,
                           BuiltinDeserializer* builtin_deserializer);

  bool ReservationsAreFullyUsed() const;

  // ------- Misc Utility Methods -------

  void RegisterDeserializedObjectsForBlackAllocation();

 private:
  Isolate* isolate() const;

  // Raw allocation without considering alignment.
  Address AllocateRaw(AllocationSpace space, int size);

 private:
  static constexpr int kNumberOfPreallocatedSpaces =
      SerializerDeserializer::kNumberOfPreallocatedSpaces;
  static constexpr int kNumberOfSpaces =
      SerializerDeserializer::kNumberOfSpaces;

  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];

  // The alignment of the next allocation.
  AllocationAlignment next_alignment_ = kWordAligned;

  // All required maps are pre-allocated during reservation. {next_map_index_}
  // stores the index of the next map to return from allocation.
  uint32_t next_map_index_ = 0;
  std::vector<Address> allocated_maps_;

  // Allocated large objects are kept in this map and may be fetched later as
  // back-references.
  std::vector<HeapObject*> deserialized_large_objects_;

  // The current deserializer.
  Deserializer<DefaultDeserializerAllocator>* const deserializer_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDeserializerAllocator)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_DEFAULT_DESERIALIZER_ALLOCATOR_H_
