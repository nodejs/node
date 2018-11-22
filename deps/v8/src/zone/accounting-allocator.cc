// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include <cstdlib>

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

#include "src/allocation.h"

namespace v8 {
namespace internal {

Segment* AccountingAllocator::GetSegment(size_t bytes) {
  Segment* result = AllocateSegment(bytes);
  if (result != nullptr) result->Initialize(bytes);
  return result;
}

Segment* AccountingAllocator::AllocateSegment(size_t bytes) {
  void* memory = AllocWithRetry(bytes);
  if (memory != nullptr) {
    base::AtomicWord current =
        base::Relaxed_AtomicIncrement(&current_memory_usage_, bytes);
    base::AtomicWord peak = base::Relaxed_Load(&peak_memory_usage_);
    while (current > peak) {
      peak = base::Relaxed_CompareAndSwap(&peak_memory_usage_, peak, current);
    }
  }
  return reinterpret_cast<Segment*>(memory);
}

void AccountingAllocator::ReturnSegment(Segment* segment) {
  segment->ZapContents();
  FreeSegment(segment);
}

void AccountingAllocator::FreeSegment(Segment* memory) {
  base::Relaxed_AtomicIncrement(&current_memory_usage_,
                                -static_cast<base::AtomicWord>(memory->size()));
  memory->ZapHeader();
  free(memory);
}

size_t AccountingAllocator::GetPeakMemoryUsage() const {
  return base::Relaxed_Load(&peak_memory_usage_);
}

size_t AccountingAllocator::GetCurrentMemoryUsage() const {
  return base::Relaxed_Load(&current_memory_usage_);
}

}  // namespace internal
}  // namespace v8
