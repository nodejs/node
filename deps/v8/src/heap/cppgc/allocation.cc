// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"

#include "include/cppgc/internal/api-constants.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/object-allocator.h"

namespace cppgc {
namespace internal {

STATIC_ASSERT(api_constants::kLargeObjectSizeThreshold ==
              kLargeObjectSizeThreshold);

// static
void* MakeGarbageCollectedTraitInternal::Allocate(
    cppgc::AllocationHandle& handle, size_t size, GCInfoIndex index) {
  return static_cast<ObjectAllocator&>(handle).AllocateObject(size, index);
}

// static
void* MakeGarbageCollectedTraitInternal::Allocate(
    cppgc::AllocationHandle& handle, size_t size, GCInfoIndex index,
    CustomSpaceIndex space_index) {
  return static_cast<ObjectAllocator&>(handle).AllocateObject(size, index,
                                                              space_index);
}

}  // namespace internal
}  // namespace cppgc
