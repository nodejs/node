// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(CPPGC_CAGED_HEAP)
#error "Must be compiled with caged heap enabled"
#endif

#include "src/heap/cppgc/caged-heap.h"

#include "include/cppgc/internal/caged-heap-local-data.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/logging.h"
#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

STATIC_ASSERT(api_constants::kCagedHeapReservationSize ==
              kCagedHeapReservationSize);
STATIC_ASSERT(api_constants::kCagedHeapReservationAlignment ==
              kCagedHeapReservationAlignment);

namespace {

VirtualMemory ReserveCagedHeap(PageAllocator* platform_allocator) {
  DCHECK_NOT_NULL(platform_allocator);
  DCHECK_EQ(0u,
            kCagedHeapReservationSize % platform_allocator->AllocatePageSize());

  static constexpr size_t kAllocationTries = 4;
  for (size_t i = 0; i < kAllocationTries; ++i) {
    void* hint = reinterpret_cast<void*>(RoundDown(
        reinterpret_cast<uintptr_t>(platform_allocator->GetRandomMmapAddr()),
        kCagedHeapReservationAlignment));

    VirtualMemory memory(platform_allocator, kCagedHeapReservationSize,
                         kCagedHeapReservationAlignment, hint);
    if (memory.IsReserved()) return memory;
  }

  FATAL("Fatal process out of memory: Failed to reserve memory for caged heap");
  UNREACHABLE();
}

std::unique_ptr<CagedHeap::AllocatorType> CreateBoundedAllocator(
    v8::PageAllocator* platform_allocator, void* caged_heap_start) {
  DCHECK(caged_heap_start);

  auto start =
      reinterpret_cast<CagedHeap::AllocatorType::Address>(caged_heap_start);

  return std::make_unique<CagedHeap::AllocatorType>(
      platform_allocator, start, kCagedHeapReservationSize, kPageSize);
}

}  // namespace

CagedHeap::CagedHeap(HeapBase* heap_base, PageAllocator* platform_allocator)
    : reserved_area_(ReserveCagedHeap(platform_allocator)) {
  DCHECK_NOT_NULL(heap_base);

  void* caged_heap_start = reserved_area_.address();
  CHECK(platform_allocator->SetPermissions(
      reserved_area_.address(),
      RoundUp(sizeof(CagedHeapLocalData), platform_allocator->CommitPageSize()),
      PageAllocator::kReadWrite));

  auto* local_data =
      new (reserved_area_.address()) CagedHeapLocalData(heap_base);
#if defined(CPPGC_YOUNG_GENERATION)
  local_data->age_table.Reset(platform_allocator);
#endif
  USE(local_data);

  caged_heap_start = reinterpret_cast<void*>(
      RoundUp(reinterpret_cast<uintptr_t>(caged_heap_start) +
                  sizeof(CagedHeapLocalData),
              kPageSize));
  bounded_allocator_ =
      CreateBoundedAllocator(platform_allocator, caged_heap_start);
}

}  // namespace internal
}  // namespace cppgc
