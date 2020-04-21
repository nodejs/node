// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/heap-inl.h"

namespace cppgc {
namespace internal {

STATIC_ASSERT(api_constants::kLargeObjectSizeThreshold ==
              kLargeObjectSizeThreshold);

// static
void* MakeGarbageCollectedTraitInternal::Allocate(cppgc::Heap* heap,
                                                  size_t size,
                                                  GCInfoIndex index) {
  DCHECK_NOT_NULL(heap);
  return Heap::From(heap)->Allocate(size, index);
}

}  // namespace internal
}  // namespace cppgc
