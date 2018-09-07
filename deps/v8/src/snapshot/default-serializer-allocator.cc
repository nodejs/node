// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/default-serializer-allocator.h"

#include "src/heap/heap-inl.h"
#include "src/snapshot/references.h"
#include "src/snapshot/serializer.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

DefaultSerializerAllocator::DefaultSerializerAllocator(
    Serializer<DefaultSerializerAllocator>* serializer)
    : serializer_(serializer) {
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    pending_chunk_[i] = 0;
  }
}

void DefaultSerializerAllocator::UseCustomChunkSize(uint32_t chunk_size) {
  custom_chunk_size_ = chunk_size;
}

static uint32_t PageSizeOfSpace(int space) {
  return static_cast<uint32_t>(
      MemoryAllocator::PageAreaSize(static_cast<AllocationSpace>(space)));
}

uint32_t DefaultSerializerAllocator::TargetChunkSize(int space) {
  if (custom_chunk_size_ == 0) return PageSizeOfSpace(space);
  DCHECK_LE(custom_chunk_size_, PageSizeOfSpace(space));
  return custom_chunk_size_;
}

SerializerReference DefaultSerializerAllocator::Allocate(AllocationSpace space,
                                                         uint32_t size) {
  DCHECK(space >= 0 && space < kNumberOfPreallocatedSpaces);
  DCHECK(size > 0 && size <= PageSizeOfSpace(space));

  // Maps are allocated through AllocateMap.
  DCHECK_NE(MAP_SPACE, space);

  uint32_t old_chunk_size = pending_chunk_[space];
  uint32_t new_chunk_size = old_chunk_size + size;
  // Start a new chunk if the new size exceeds the target chunk size.
  // We may exceed the target chunk size if the single object size does.
  if (new_chunk_size > TargetChunkSize(space) && old_chunk_size != 0) {
    serializer_->PutNextChunk(space);
    completed_chunks_[space].push_back(pending_chunk_[space]);
    pending_chunk_[space] = 0;
    new_chunk_size = size;
  }
  uint32_t offset = pending_chunk_[space];
  pending_chunk_[space] = new_chunk_size;
  return SerializerReference::BackReference(
      space, static_cast<uint32_t>(completed_chunks_[space].size()), offset);
}

SerializerReference DefaultSerializerAllocator::AllocateMap() {
  // Maps are allocated one-by-one when deserializing.
  return SerializerReference::MapReference(num_maps_++);
}

SerializerReference DefaultSerializerAllocator::AllocateLargeObject(
    uint32_t size) {
  // Large objects are allocated one-by-one when deserializing. We do not
  // have to keep track of multiple chunks.
  large_objects_total_size_ += size;
  return SerializerReference::LargeObjectReference(seen_large_objects_index_++);
}

SerializerReference DefaultSerializerAllocator::AllocateOffHeapBackingStore() {
  DCHECK_NE(0, seen_backing_stores_index_);
  return SerializerReference::OffHeapBackingStoreReference(
      seen_backing_stores_index_++);
}

#ifdef DEBUG
bool DefaultSerializerAllocator::BackReferenceIsAlreadyAllocated(
    SerializerReference reference) const {
  DCHECK(reference.is_back_reference());
  AllocationSpace space = reference.space();
  if (space == LO_SPACE) {
    return reference.large_object_index() < seen_large_objects_index_;
  } else if (space == MAP_SPACE) {
    return reference.map_index() < num_maps_;
  } else if (space == RO_SPACE &&
             serializer_->isolate()->heap()->deserialization_complete()) {
    // If not deserializing the isolate itself, then we create BackReferences
    // for all RO_SPACE objects without ever allocating.
    return true;
  } else {
    size_t chunk_index = reference.chunk_index();
    if (chunk_index == completed_chunks_[space].size()) {
      return reference.chunk_offset() < pending_chunk_[space];
    } else {
      return chunk_index < completed_chunks_[space].size() &&
             reference.chunk_offset() < completed_chunks_[space][chunk_index];
    }
  }
}
#endif

std::vector<SerializedData::Reservation>
DefaultSerializerAllocator::EncodeReservations() const {
  std::vector<SerializedData::Reservation> out;

  for (int i = FIRST_SPACE; i < kNumberOfPreallocatedSpaces; i++) {
    for (size_t j = 0; j < completed_chunks_[i].size(); j++) {
      out.emplace_back(completed_chunks_[i][j]);
    }

    if (pending_chunk_[i] > 0 || completed_chunks_[i].size() == 0) {
      out.emplace_back(pending_chunk_[i]);
    }
    out.back().mark_as_last();
  }

  STATIC_ASSERT(MAP_SPACE == kNumberOfPreallocatedSpaces);
  out.emplace_back(num_maps_ * Map::kSize);
  out.back().mark_as_last();

  STATIC_ASSERT(LO_SPACE == MAP_SPACE + 1);
  out.emplace_back(large_objects_total_size_);
  out.back().mark_as_last();

  return out;
}

void DefaultSerializerAllocator::OutputStatistics() {
  DCHECK(FLAG_serialization_statistics);

  PrintF("  Spaces (bytes):\n");

  for (int space = FIRST_SPACE; space < kNumberOfSpaces; space++) {
    PrintF("%16s", AllocationSpaceName(static_cast<AllocationSpace>(space)));
  }
  PrintF("\n");

  for (int space = FIRST_SPACE; space < kNumberOfPreallocatedSpaces; space++) {
    size_t s = pending_chunk_[space];
    for (uint32_t chunk_size : completed_chunks_[space]) s += chunk_size;
    PrintF("%16" PRIuS, s);
  }

  STATIC_ASSERT(MAP_SPACE == kNumberOfPreallocatedSpaces);
  PrintF("%16d", num_maps_ * Map::kSize);

  STATIC_ASSERT(LO_SPACE == MAP_SPACE + 1);
  PrintF("%16d\n", large_objects_total_size_);
}

}  // namespace internal
}  // namespace v8
