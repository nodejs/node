// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_EXPLICIT_MANAGEMENT_H_
#define INCLUDE_CPPGC_EXPLICIT_MANAGEMENT_H_

#include <cstddef>

#include "cppgc/allocation.h"
#include "cppgc/internal/logging.h"
#include "cppgc/type-traits.h"

namespace cppgc {
namespace internal {

V8_EXPORT void FreeUnreferencedObject(void*);
V8_EXPORT bool Resize(void*, size_t);

}  // namespace internal

namespace subtle {

/**
 * Informs the garbage collector that `object` can be immediately reclaimed. The
 * destructor may not be invoked immediately but only on next garbage
 * collection.
 *
 * It is up to the embedder to guarantee that no other object holds a reference
 * to `object` after calling `FreeUnreferencedObject()`. In case such a
 * reference exists, it's use results in a use-after-free.
 *
 * \param object Reference to an object that is of type `GarbageCollected` and
 *   should be immediately reclaimed.
 */
template <typename T>
void FreeUnreferencedObject(T* object) {
  static_assert(IsGarbageCollectedTypeV<T>,
                "Object must be of type GarbageCollected.");
  if (!object) return;
  internal::FreeUnreferencedObject(object);
}

/**
 * Tries to resize `object` of type `T` with additional bytes on top of
 * sizeof(T). Resizing is only useful with trailing inlined storage, see e.g.
 * `MakeGarbageCollected(AllocationHandle&, AdditionalBytes)`.
 *
 * `Resize()` performs growing or shrinking as needed and may skip the operation
 * for internal reasons, see return value.
 *
 * It is up to the embedder to guarantee that in case of shrinking a larger
 * object down, the reclaimed area is not used anymore. Any subsequent use
 * results in a use-after-free.
 *
 * \param object Reference to an object that is of type `GarbageCollected` and
 *   should be resized.
 * \param additional_bytes Bytes in addition to sizeof(T) that the object should
 *   provide.
 * \returns true when the operation was successful and the result can be relied
 *   on, and false otherwise.
 */
template <typename T>
bool Resize(T& object, AdditionalBytes additional_bytes) {
  static_assert(IsGarbageCollectedTypeV<T>,
                "Object must be of type GarbageCollected.");
  return internal::Resize(&object, sizeof(T) + additional_bytes.value);
}

}  // namespace subtle
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_EXPLICIT_MANAGEMENT_H_
