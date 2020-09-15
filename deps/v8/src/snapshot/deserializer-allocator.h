// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_DESERIALIZER_ALLOCATOR_H_
#define V8_SNAPSHOT_DESERIALIZER_ALLOCATOR_H_

#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/objects/heap-object.h"
#include "src/roots/roots.h"
#include "src/snapshot/references.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

class Deserializer;
class StartupDeserializer;

class DeserializerAllocator final {
 public:
  DeserializerAllocator() = default;

  void Initialize(Heap* heap);

  // ------- Allocation Methods -------
  // Methods related to memory allocation during deserialization.

  Address Allocate(SnapshotSpace space, int size);

  void MoveToNextChunk(SnapshotSpace space);
  void SetAlignment(AllocationAlignment alignment) {
    DCHECK_EQ(kWordAligned, next_alignment_);
    DCHECK_LE(kWordAligned, alignment);
    DCHECK_LE(alignment, kDoubleUnaligned);
    next_alignment_ = static_cast<AllocationAlignment>(alignment);
  }

  HeapObject GetMap(uint32_t index);
  HeapObject GetLargeObject(uint32_t index);
  HeapObject GetObject(SnapshotSpace space, uint32_t chunk_index,
                       uint32_t chunk_offset);

  // ------- Reservation Methods -------
  // Methods related to memory reservations (prior to deserialization).

  V8_EXPORT_PRIVATE void DecodeReservation(
      const std::vector<SerializedData::Reservation>& res);
  bool ReserveSpace();

  bool ReservationsAreFullyUsed() const;

  // ------- Misc Utility Methods -------

  void RegisterDeserializedObjectsForBlackAllocation();

 private:
  // Raw allocation without considering alignment.
  Address AllocateRaw(SnapshotSpace space, int size);

 private:
  static constexpr int kNumberOfPreallocatedSpaces =
      static_cast<int>(SnapshotSpace::kNumberOfPreallocatedSpaces);
  static constexpr int kNumberOfSpaces =
      static_cast<int>(SnapshotSpace::kNumberOfSpaces);

  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];

#ifdef DEBUG
  // Record the previous object allocated for DCHECKs.
  Address previous_allocation_start_ = kNullAddress;
  int previous_allocation_size_ = 0;
#endif

  // The alignment of the next allocation.
  AllocationAlignment next_alignment_ = kWordAligned;

  // All required maps are pre-allocated during reservation. {next_map_index_}
  // stores the index of the next map to return from allocation.
  uint32_t next_map_index_ = 0;
  std::vector<Address> allocated_maps_;

  // Allocated large objects are kept in this map and may be fetched later as
  // back-references.
  std::vector<HeapObject> deserialized_large_objects_;

  // ReadOnlyRoots and heap are null until Initialize is called.
  Heap* heap_ = nullptr;
  ReadOnlyRoots roots_ = ReadOnlyRoots(static_cast<Address*>(nullptr));

  DISALLOW_COPY_AND_ASSIGN(DeserializerAllocator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_DESERIALIZER_ALLOCATOR_H_
