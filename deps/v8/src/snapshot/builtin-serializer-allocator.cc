// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/builtin-serializer-allocator.h"

#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

SerializerReference BuiltinSerializerAllocator::Allocate(AllocationSpace space,
                                                         uint32_t size) {
  DCHECK_EQ(space, CODE_SPACE);
  DCHECK_GT(size, 0);

  // Builtin serialization & deserialization does not use the reservation
  // system.  Instead of worrying about chunk indices and offsets, we simply
  // need to generate unique offsets here.

  const uint32_t virtual_chunk_index = 0;
  const auto ref = SerializerReference::BackReference(
      CODE_SPACE, virtual_chunk_index, virtual_chunk_offset_);

  virtual_chunk_size_ += size;
  virtual_chunk_offset_ += kObjectAlignment;  // Needs to be aligned.

  return ref;
}

#ifdef DEBUG
bool BuiltinSerializerAllocator::BackReferenceIsAlreadyAllocated(
    SerializerReference reference) const {
  DCHECK(reference.is_back_reference());
  AllocationSpace space = reference.space();
  DCHECK_EQ(space, CODE_SPACE);
  DCHECK_EQ(reference.chunk_index(), 0);
  return reference.chunk_offset() < virtual_chunk_offset_;
}
#endif  // DEBUG

std::vector<SerializedData::Reservation>
BuiltinSerializerAllocator::EncodeReservations() const {
  return std::vector<SerializedData::Reservation>();
}

void BuiltinSerializerAllocator::OutputStatistics() {
  DCHECK(FLAG_serialization_statistics);

  PrintF("  Spaces (bytes):\n");

  STATIC_ASSERT(NEW_SPACE == 0);
  for (int space = 0; space < kNumberOfSpaces; space++) {
    PrintF("%16s", AllocationSpaceName(static_cast<AllocationSpace>(space)));
  }
  PrintF("\n");

  STATIC_ASSERT(NEW_SPACE == 0);
  for (int space = 0; space < kNumberOfSpaces; space++) {
    uint32_t space_size = (space == CODE_SPACE) ? virtual_chunk_size_ : 0;
    PrintF("%16d", space_size);
  }
  PrintF("\n");
}

}  // namespace internal
}  // namespace v8
