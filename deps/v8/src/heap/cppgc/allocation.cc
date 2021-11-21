// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"

#include "include/cppgc/internal/api-constants.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/object-allocator.h"

#if defined(__clang__) && !defined(DEBUG) && V8_HAS_ATTRIBUTE_ALWAYS_INLINE
#define CPPGC_FORCE_ALWAYS_INLINE __attribute__((always_inline))
#else
#define CPPGC_FORCE_ALWAYS_INLINE
#endif

namespace cppgc {
namespace internal {

STATIC_ASSERT(api_constants::kLargeObjectSizeThreshold ==
              kLargeObjectSizeThreshold);

// Using CPPGC_FORCE_ALWAYS_INLINE to guide LTO for inlining the allocation
// fast path.
// static
CPPGC_FORCE_ALWAYS_INLINE void* MakeGarbageCollectedTraitInternal::Allocate(
    cppgc::AllocationHandle& handle, size_t size, GCInfoIndex index) {
  return static_cast<ObjectAllocator&>(handle).AllocateObject(size, index);
}

// Using CPPGC_FORCE_ALWAYS_INLINE to guide LTO for inlining the allocation
// fast path.
// static
CPPGC_FORCE_ALWAYS_INLINE void* MakeGarbageCollectedTraitInternal::Allocate(
    cppgc::AllocationHandle& handle, size_t size, GCInfoIndex index,
    CustomSpaceIndex space_index) {
  return static_cast<ObjectAllocator&>(handle).AllocateObject(size, index,
                                                              space_index);
}

}  // namespace internal
}  // namespace cppgc
