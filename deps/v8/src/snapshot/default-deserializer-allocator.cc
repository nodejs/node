// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/default-deserializer-allocator.h"

#include "src/heap/heap-inl.h"
#include "src/snapshot/builtin-deserializer.h"
#include "src/snapshot/deserializer.h"
#include "src/snapshot/startup-deserializer.h"

namespace v8 {
namespace internal {

DefaultDeserializerAllocator::DefaultDeserializerAllocator(
    Deserializer<DefaultDeserializerAllocator>* deserializer)
    : deserializer_(deserializer) {}

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
Address DefaultDeserializerAllocator::AllocateRaw(AllocationSpace space,
                                                  int size) {
  if (space == LO_SPACE) {
    AlwaysAllocateScope scope(isolate());
    LargeObjectSpace* lo_space = isolate()->heap()->lo_space();
    // TODO(jgruber): May be cleaner to pass in executability as an argument.
    Executability exec =
        static_cast<Executability>(deserializer_->source()->Get());
    AllocationResult result = lo_space->AllocateRaw(size, exec);
    HeapObject* obj = result.ToObjectChecked();
    deserialized_large_objects_.push_back(obj);
    return obj->address();
  } else if (space == MAP_SPACE) {
    DCHECK_EQ(Map::kSize, size);
    return allocated_maps_[next_map_index_++];
  } else {
    DCHECK_LT(space, kNumberOfPreallocatedSpaces);
    Address address = high_water_[space];
    DCHECK_NOT_NULL(address);
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

Address DefaultDeserializerAllocator::Allocate(AllocationSpace space,
                                               int size) {
  Address address;
  HeapObject* obj;

  if (next_alignment_ != kWordAligned) {
    const int reserved = size + Heap::GetMaximumFillToAlign(next_alignment_);
    address = AllocateRaw(space, reserved);
    obj = HeapObject::FromAddress(address);
    // If one of the following assertions fails, then we are deserializing an
    // aligned object when the filler maps have not been deserialized yet.
    // We require filler maps as padding to align the object.
    Heap* heap = isolate()->heap();
    DCHECK(heap->free_space_map()->IsMap());
    DCHECK(heap->one_pointer_filler_map()->IsMap());
    DCHECK(heap->two_pointer_filler_map()->IsMap());
    obj = heap->AlignWithFiller(obj, size, reserved, next_alignment_);
    address = obj->address();
    next_alignment_ = kWordAligned;
    return address;
  } else {
    return AllocateRaw(space, size);
  }
}

void DefaultDeserializerAllocator::MoveToNextChunk(AllocationSpace space) {
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

HeapObject* DefaultDeserializerAllocator::GetMap(uint32_t index) {
  DCHECK_LT(index, next_map_index_);
  return HeapObject::FromAddress(allocated_maps_[index]);
}

HeapObject* DefaultDeserializerAllocator::GetLargeObject(uint32_t index) {
  DCHECK_LT(index, deserialized_large_objects_.size());
  return deserialized_large_objects_[index];
}

HeapObject* DefaultDeserializerAllocator::GetObject(AllocationSpace space,
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

void DefaultDeserializerAllocator::DecodeReservation(
    Vector<const SerializedData::Reservation> res) {
  DCHECK_EQ(0, reservations_[NEW_SPACE].size());
  STATIC_ASSERT(NEW_SPACE == 0);
  int current_space = NEW_SPACE;
  for (auto& r : res) {
    reservations_[current_space].push_back({r.chunk_size(), NULL, NULL});
    if (r.is_last()) current_space++;
  }
  DCHECK_EQ(kNumberOfSpaces, current_space);
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) current_chunk_[i] = 0;
}

bool DefaultDeserializerAllocator::ReserveSpace() {
#ifdef DEBUG
  for (int i = NEW_SPACE; i < kNumberOfSpaces; ++i) {
    DCHECK_GT(reservations_[i].size(), 0);
  }
#endif  // DEBUG
  DCHECK(allocated_maps_.empty());
  if (!isolate()->heap()->ReserveSpace(reservations_, &allocated_maps_)) {
    return false;
  }
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    high_water_[i] = reservations_[i][0].start;
  }
  return true;
}

// static
bool DefaultDeserializerAllocator::ReserveSpace(
    StartupDeserializer* startup_deserializer,
    BuiltinDeserializer* builtin_deserializer) {
  const int first_space = NEW_SPACE;
  const int last_space = SerializerDeserializer::kNumberOfSpaces;
  Isolate* isolate = startup_deserializer->isolate();

  // Create a set of merged reservations to reserve space in one go.
  // The BuiltinDeserializer's reservations are ignored, since our actual
  // requirements vary based on whether lazy deserialization is enabled.
  // Instead, we manually determine the required code-space.

  Heap::Reservation merged_reservations[kNumberOfSpaces];
  for (int i = first_space; i < last_space; i++) {
    merged_reservations[i] =
        startup_deserializer->allocator()->reservations_[i];
  }

  Heap::Reservation builtin_reservations =
      builtin_deserializer->allocator()
          ->CreateReservationsForEagerBuiltinsAndHandlers();
  DCHECK(!builtin_reservations.empty());

  for (const auto& c : builtin_reservations) {
    merged_reservations[CODE_SPACE].push_back(c);
  }

  if (!isolate->heap()->ReserveSpace(
          merged_reservations,
          &startup_deserializer->allocator()->allocated_maps_)) {
    return false;
  }

  DisallowHeapAllocation no_allocation;

  // Distribute the successful allocations between both deserializers.
  // There's nothing to be done here except for code space.

  {
    const int num_builtin_reservations =
        static_cast<int>(builtin_reservations.size());
    for (int i = num_builtin_reservations - 1; i >= 0; i--) {
      const auto& c = merged_reservations[CODE_SPACE].back();
      DCHECK_EQ(c.size, builtin_reservations[i].size);
      DCHECK_EQ(c.size, c.end - c.start);
      builtin_reservations[i].start = c.start;
      builtin_reservations[i].end = c.end;
      merged_reservations[CODE_SPACE].pop_back();
    }

    builtin_deserializer->allocator()->InitializeFromReservations(
        builtin_reservations);
  }

  // Write back startup reservations.

  for (int i = first_space; i < last_space; i++) {
    startup_deserializer->allocator()->reservations_[i].swap(
        merged_reservations[i]);
  }

  for (int i = first_space; i < kNumberOfPreallocatedSpaces; i++) {
    startup_deserializer->allocator()->high_water_[i] =
        startup_deserializer->allocator()->reservations_[i][0].start;
  }

  return true;
}

bool DefaultDeserializerAllocator::ReservationsAreFullyUsed() const {
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

void DefaultDeserializerAllocator::
    RegisterDeserializedObjectsForBlackAllocation() {
  isolate()->heap()->RegisterDeserializedObjectsForBlackAllocation(
      reservations_, deserialized_large_objects_, allocated_maps_);
}

Isolate* DefaultDeserializerAllocator::isolate() const {
  return deserializer_->isolate();
}

}  // namespace internal
}  // namespace v8
