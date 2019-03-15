// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include <cstdlib>

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

#include "src/allocation.h"
#include "src/asan.h"
#include "src/msan.h"

namespace v8 {
namespace internal {

AccountingAllocator::~AccountingAllocator() = default;

Segment* AccountingAllocator::AllocateSegment(size_t bytes) {
  void* memory = AllocWithRetry(bytes);
  if (memory == nullptr) return nullptr;

  size_t current =
      current_memory_usage_.fetch_add(bytes, std::memory_order_relaxed);
  size_t max = max_memory_usage_.load(std::memory_order_relaxed);
  while (current > max && !max_memory_usage_.compare_exchange_weak(
                              max, current, std::memory_order_relaxed)) {
    // {max} was updated by {compare_exchange_weak}; retry.
  }
  DCHECK_LE(sizeof(Segment), bytes);
  return new (memory) Segment(bytes);
}

void AccountingAllocator::ReturnSegment(Segment* segment) {
  segment->ZapContents();
  current_memory_usage_.fetch_sub(segment->total_size(),
                                  std::memory_order_relaxed);
  segment->ZapHeader();
  free(segment);
}

}  // namespace internal
}  // namespace v8
