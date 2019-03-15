// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer-allocator.h"

#include "src/heap/heap-inl.h"  // crbug.com/v8/8499

namespace v8 {
namespace internal {

// We know the space requirements before deserialization and can
// pre-allocate that reserved space. During deserialization, all we need
// to do is to bump up the pointer for each space in the reserved
// space. This is also used for fixing back references.
// We may have to split up the pre-allocation into several chunks
// because it would not fit onto a single page. We do not have to keep
// track of when to move to the next chunk. An opcode will signal this.
// Since multiple large objects cannot be folded into one large object
// space allocation, we have to do an actual allocation when deserializing
// each large object. Instead of tracking offset for back references, we
// reference large objects by index.
Address DeserializerAllocator::AllocateRaw(AllocationSpace space, int size) {
  if (space == LO_SPACE) {
    AlwaysAllocateScope scope(heap_);
    // Note that we currently do not support deserialization of large code
    // objects.
    LargeObjectSpace* lo_space = heap_->lo_space();
    AllocationResult result = lo_space->AllocateRaw(size);
    HeapObject obj = result.ToObjectChecked();
    deserialized_large_objects_.push_back(obj);
    return obj->address();
  } else if (space == MAP_SPACE) {
    DCHECK_EQ(Map::kSize, size);
    return allocated_maps_[next_map_index_++];
  } else {
    DCHECK_LT(space, kNumberOfPreallocatedSpaces);
    Address address = high_water_[space];
    DCHECK_NE(address, kNullAddress);
    high_water_[space] += size;
#ifdef DEBUG
    // Assert that the current reserved chunk is still big enough.
    const Heap::Reservation& reservation = reservations_[space];
    int chunk_index = current_chunk_[space];
    DCHECK_LE(high_water_[space], reservation[chunk_index].end);
#endif
    if (space == CODE_SPACE) SkipList::Update(address, size);
    return address;
  }
}

Address DeserializerAllocator::Allocate(AllocationSpace space, int size) {
  Address address;
  HeapObject obj;

  if (next_alignment_ != kWordAligned) {
    const int reserved = size + Heap::GetMaximumFillToAlign(next_alignment_);
    address = AllocateRaw(space, reserved);
    obj = HeapObject::FromAddress(address);
    // If one of the following assertions fails, then we are deserializing an
    // aligned object when the filler maps have not been deserialized yet.
    // We require filler maps as padding to align the object.
    DCHECK(ReadOnlyRoots(heap_).free_space_map()->IsMap());
    DCHECK(ReadOnlyRoots(heap_).one_pointer_filler_map()->IsMap());
    DCHECK(ReadOnlyRoots(heap_).two_pointer_filler_map()->IsMap());
    obj = heap_->AlignWithFiller(obj, size, reserved, next_alignment_);
    address = obj->address();
    next_alignment_ = kWordAligned;
    return address;
  } else {
    return AllocateRaw(space, size);
  }
}

void DeserializerAllocator::MoveToNextChunk(AllocationSpace space) {
  DCHECK_LT(space, kNumberOfPreallocatedSpaces);
  uint32_t chunk_index = current_chunk_[space];
  const Heap::Reservation& reservation = reservations_[space];
  // Make sure the current chunk is indeed exhausted.
  CHECK_EQ(reservation[chunk_index].end, high_water_[space]);
  // Move to next reserved chunk.
  chunk_index = ++current_chunk_[space];
  CHECK_LT(chunk_index, reservation.size());
  high_water_[space] = reservation[chunk_index].start;
}

HeapObject DeserializerAllocator::GetMap(uint32_t index) {
  DCHECK_LT(index, next_map_index_);
  return HeapObject::FromAddress(allocated_maps_[index]);
}

HeapObject DeserializerAllocator::GetLargeObject(uint32_t index) {
  DCHECK_LT(index, deserialized_large_objects_.size());
  return deserialized_large_objects_[index];
}

HeapObject DeserializerAllocator::GetObject(AllocationSpace space,
                                            uint32_t chunk_index,
                                            uint32_t chunk_offset) {
  DCHECK_LT(space, kNumberOfPreallocatedSpaces);
  DCHECK_LE(chunk_index, current_chunk_[space]);
  Address address = reservations_[space][chunk_index].start + chunk_offset;
  if (next_alignment_ != kWordAligned) {
    int padding = Heap::GetFillToAlign(address, next_alignment_);
    next_alignment_ = kWordAligned;
    DCHECK(padding == 0 || HeapObject::FromAddress(address)->IsFiller());
    address += padding;
  }
  return HeapObject::FromAddress(address);
}

void DeserializerAllocator::DecodeReservation(
    const std::vector<SerializedData::Reservation>& res) {
  DCHECK_EQ(0, reservations_[FIRST_SPACE].size());
  int current_space = FIRST_SPACE;
  for (auto& r : res) {
    reservations_[current_space].push_back(
        {r.chunk_size(), kNullAddress, kNullAddress});
    if (r.is_last()) current_space++;
  }
  DCHECK_EQ(kNumberOfSpaces, current_space);
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) current_chunk_[i] = 0;
}

bool DeserializerAllocator::ReserveSpace() {
#ifdef DEBUG
  for (int i = FIRST_SPACE; i < kNumberOfSpaces; ++i) {
    DCHECK_GT(reservations_[i].size(), 0);
  }
#endif  // DEBUG
  DCHECK(allocated_maps_.empty());
  if (!heap_->ReserveSpace(reservations_, &allocated_maps_)) {
    return false;
  }
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    high_water_[i] = reservations_[i][0].start;
  }
  return true;
}

bool DeserializerAllocator::ReservationsAreFullyUsed() const {
  for (int space = 0; space < kNumberOfPreallocatedSpaces; space++) {
    const uint32_t chunk_index = current_chunk_[space];
    if (reservations_[space].size() != chunk_index + 1) {
      return false;
    }
    if (reservations_[space][chunk_index].end != high_water_[space]) {
      return false;
    }
  }
  return (allocated_maps_.size() == next_map_index_);
}

void DeserializerAllocator::RegisterDeserializedObjectsForBlackAllocation() {
  heap_->RegisterDeserializedObjectsForBlackAllocation(
      reservations_, deserialized_large_objects_, allocated_maps_);
}

}  // namespace internal
}  // namespace v8
