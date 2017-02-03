// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include <cstdlib>

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

namespace v8 {
namespace internal {

Segment* AccountingAllocator::AllocateSegment(size_t bytes) {
  void* memory = malloc(bytes);
  if (memory) {
    base::AtomicWord current =
        base::NoBarrier_AtomicIncrement(&current_memory_usage_, bytes);
    base::AtomicWord max = base::NoBarrier_Load(&max_memory_usage_);
    while (current > max) {
      max = base::NoBarrier_CompareAndSwap(&max_memory_usage_, max, current);
    }
  }
  return reinterpret_cast<Segment*>(memory);
}

void AccountingAllocator::FreeSegment(Segment* memory) {
  base::NoBarrier_AtomicIncrement(
      &current_memory_usage_, -static_cast<base::AtomicWord>(memory->size()));
  memory->ZapHeader();
  free(memory);
}

size_t AccountingAllocator::GetCurrentMemoryUsage() const {
  return base::NoBarrier_Load(&current_memory_usage_);
}

size_t AccountingAllocator::GetMaxMemoryUsage() const {
  return base::NoBarrier_Load(&max_memory_usage_);
}

}  // namespace internal
}  // namespace v8
