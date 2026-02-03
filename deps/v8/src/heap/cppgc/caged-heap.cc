// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/caged-heap.h"

#include <bit>
#include <map>

#include "src/heap/cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if !defined(CPPGC_CAGED_HEAP)
#error "Must be compiled with caged heap enabled"
#endif

#include "include/cppgc/internal/api-constants.h"
#include "include/cppgc/internal/caged-heap-local-data.h"
#include "include/cppgc/member.h"
#include "include/cppgc/platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/caged-heap.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/member-storage.h"

namespace cppgc {
namespace internal {

uintptr_t CagedHeapBase::g_heap_base_ = 0u;
size_t CagedHeapBase::g_age_table_size_ = 0u;

CagedHeap* CagedHeap::instance_ = nullptr;

// We cannot unmap subregions on Windows and neither can we with
// LsanPageAllocator.
#if !defined(LEAK_SANITIZER) && !defined(V8_OS_WIN)
constexpr bool kUnmapSubregions = true;
#else
constexpr bool kUnmapSubregions = false;
#endif

// static
CagedHeap::Reservation CagedHeap::ReserveCagedHeap(
    PageAllocator& platform_allocator) {
  DCHECK_EQ(0u, api_constants::kCagedHeapMaxReservationSize %
                    platform_allocator.AllocatePageSize());
  static constexpr size_t kAllocationTries = 4;

#if defined(CPPGC_POINTER_COMPRESSION)
  // We want compressed pointers to have the most significant bit set to 1.
  // That way, on decompression the bit will be sign-extended. This saves us a
  // branch and 'or' operation during compression.
  //
  // We achieve this by over-reserving the cage and selecting a sub-region
  // that has the bit battern we need.
  //
  // TODO(chromium:1325007): Provide API in PageAllocator to left trim
  // allocations and return unused portions of the reservation back to the OS.
  static constexpr size_t kUsefulReservationSize =
      api_constants::kCagedHeapMaxReservationSize;
  static constexpr size_t kTryReserveSize = 2 * kUsefulReservationSize;
  static constexpr size_t kReservationAlignment =
      api_constants::kCagedHeapReservationAlignment;
  static constexpr size_t kMaskedOutLSB =
      static_cast<size_t>(1) << std::countr_zero(kUsefulReservationSize);

  DCHECK_EQ(kReservationAlignment % platform_allocator.AllocatePageSize(), 0);

  void* hint = reinterpret_cast<void*>(RoundDown(
      reinterpret_cast<uintptr_t>(platform_allocator.GetRandomMmapAddr()),
      kReservationAlignment));

  // First, try to reserve 32GB blob and pick the half in which the LSB of the
  // masked out part is 1. This will internally try to reserve 48GB -
  // SystemPageSize, which may fail on system with small virtual address space.
  void* start = platform_allocator.AllocatePages(
      hint, kTryReserveSize, kReservationAlignment, PageAllocator::kNoAccess);
  if (V8_LIKELY(start)) {
    const uintptr_t lower_half = reinterpret_cast<uintptr_t>(start);
    const uintptr_t upper_half =
        reinterpret_cast<uintptr_t>(start) + kUsefulReservationSize;
    if (lower_half & kMaskedOutLSB) {
      if constexpr (kUnmapSubregions) {
        platform_allocator.FreePages(reinterpret_cast<void*>(upper_half),
                                     kUsefulReservationSize);
        return {.memory = VirtualMemory(&platform_allocator,
                                        reinterpret_cast<void*>(lower_half),
                                        kUsefulReservationSize),
                .offset_into_cage_start = 0};
      }
      return {.memory = VirtualMemory(&platform_allocator,
                                      reinterpret_cast<void*>(lower_half),
                                      kTryReserveSize),
              .offset_into_cage_start = 0};
    }
    DCHECK(upper_half & kMaskedOutLSB);
    if constexpr (kUnmapSubregions) {
      platform_allocator.FreePages(reinterpret_cast<void*>(lower_half),
                                   kUsefulReservationSize);
      return {.memory = VirtualMemory(&platform_allocator,
                                      reinterpret_cast<void*>(upper_half),
                                      kUsefulReservationSize),
              .offset_into_cage_start = 0};
    }
    return {.memory = VirtualMemory(&platform_allocator,
                                    reinterpret_cast<void*>(lower_half),
                                    kTryReserveSize),
            .offset_into_cage_start = kUsefulReservationSize};
  }

  // Otherwise, try to reserve kUsefulReservationSize and hope the LSB of the
  // masked out part is 1.
  for (size_t i = 0; i < kAllocationTries; ++i) {
    hint = reinterpret_cast<void*>(RoundDown(
        reinterpret_cast<uintptr_t>(platform_allocator.GetRandomMmapAddr()),
        kReservationAlignment));
    VirtualMemory memory(&platform_allocator, kUsefulReservationSize,
                         kReservationAlignment, hint);
    if (!memory.IsReserved()) {
      continue;
    }
    if (reinterpret_cast<uintptr_t>(memory.address()) & kMaskedOutLSB) {
      return {.memory = std::move(memory), .offset_into_cage_start = 0};
    }
  }
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  static constexpr size_t kTryReserveSize =
      api_constants::kCagedHeapMaxReservationSize;
  static constexpr size_t kTryReserveAlignment =
      api_constants::kCagedHeapReservationAlignment;

  void* hint = reinterpret_cast<void*>(RoundDown(
      reinterpret_cast<uintptr_t>(platform_allocator.GetRandomMmapAddr()),
      kTryReserveAlignment));

  for (size_t i = 0; i < kAllocationTries; ++i) {
    VirtualMemory memory(&platform_allocator, kTryReserveSize,
                         kTryReserveAlignment, hint);
    if (memory.IsReserved()) {
      return {.memory = std::move(memory), .offset_into_cage_start = 0};
    }
  }
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

  GetGlobalOOMHandler()("Oilpan: CagedHeap reservation.");
}

// static
void CagedHeap::InitializeIfNeeded(PageAllocator& platform_allocator,
                                   size_t desired_heap_size) {
  static v8::base::LeakyObject<CagedHeap> caged_heap(platform_allocator,
                                                     desired_heap_size);
}

// static
CagedHeap& CagedHeap::Instance() {
  DCHECK_NOT_NULL(instance_);
  return *instance_;
}

CagedHeap::CagedHeap(PageAllocator& platform_allocator,
                     size_t desired_heap_size)
    : reservation_(ReserveCagedHeap(platform_allocator)) {
  using CagedAddress = CagedHeap::AllocatorType::Address;

  void* const cage_start =
      static_cast<uint8_t*>(reservation_.memory.address()) +
      reservation_.offset_into_cage_start;

  CagedHeapBase::g_heap_base_ = reinterpret_cast<uintptr_t>(cage_start);

#if defined(CPPGC_POINTER_COMPRESSION)
  // With pointer compression only single heap per thread is allowed.
  CHECK(!CageBaseGlobal::IsSet());
  CageBaseGlobalUpdater::UpdateCageBase(CagedHeapBase::g_heap_base_);
#endif  // defined(CPPGC_POINTER_COMPRESSION)

  const size_t total_heap_size = std::clamp<size_t>(
      v8::base::bits::RoundUpToPowerOfTwo64(desired_heap_size),
      api_constants::kCagedHeapDefaultReservationSize,
      api_constants::kCagedHeapMaxReservationSize);

  const size_t local_data_size =
      CagedHeapLocalData::CalculateLocalDataSizeForHeapSize(total_heap_size);
  const CagedAddress caged_heap_start = RoundUp(
      reinterpret_cast<CagedAddress>(cage_start) + local_data_size, kPageSize);
  const size_t local_data_size_with_padding =
      caged_heap_start - reinterpret_cast<CagedAddress>(cage_start);

  page_bounded_allocator_ = std::make_unique<v8::base::BoundedPageAllocator>(
      &platform_allocator, caged_heap_start,
      total_heap_size - local_data_size_with_padding, kPageSize,
      v8::base::PageInitializationMode::kAllocatedPagesMustBeZeroInitialized,
      v8::base::PageFreeingMode::kMakeInaccessible);

  instance_ = this;
  CagedHeapBase::g_age_table_size_ = AgeTable::CalculateAgeTableSizeForHeapSize(
      api_constants::kCagedHeapDefaultReservationSize);
}

void CagedHeap::CommitAgeTable(PageAllocator& platform_allocator) {
  if (!platform_allocator.SetPermissions(
          reinterpret_cast<void*>(CagedHeapBase::g_heap_base_),
          RoundUp(CagedHeapBase::g_age_table_size_,
                  platform_allocator.CommitPageSize()),
          PageAllocator::kReadWrite)) {
    GetGlobalOOMHandler()("Oilpan: CagedHeap commit CageHeapLocalData.");
  }
}

}  // namespace internal
}  // namespace cppgc
