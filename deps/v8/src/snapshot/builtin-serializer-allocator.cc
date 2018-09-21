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

  const auto ref =
      SerializerReference::BuiltinReference(next_builtin_reference_index_);

  allocated_bytes_ += size;
  next_builtin_reference_index_++;

  return ref;
}

#ifdef DEBUG
bool BuiltinSerializerAllocator::BackReferenceIsAlreadyAllocated(
    SerializerReference reference) const {
  DCHECK(reference.is_builtin_reference());
  return reference.builtin_index() < next_builtin_reference_index_;
}
#endif  // DEBUG

std::vector<SerializedData::Reservation>
BuiltinSerializerAllocator::EncodeReservations() const {
  return std::vector<SerializedData::Reservation>();
}

void BuiltinSerializerAllocator::OutputStatistics() {
  DCHECK(FLAG_serialization_statistics);

  PrintF("  Spaces (bytes):\n");

  for (int space = FIRST_SPACE; space < kNumberOfSpaces; space++) {
    PrintF("%16s", AllocationSpaceName(static_cast<AllocationSpace>(space)));
  }
  PrintF("\n");

  for (int space = FIRST_SPACE; space < kNumberOfSpaces; space++) {
    uint32_t space_size = (space == CODE_SPACE) ? allocated_bytes_ : 0;
    PrintF("%16d", space_size);
  }
  PrintF("\n");
}

}  // namespace internal
}  // namespace v8
