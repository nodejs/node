// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/caged-heap.h"

#include <map>

#include "src/heap/cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if !defined(CPPGC_CAGED_HEAP)
#error "Must be compiled with caged heap enabled"
#endif

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
#include "src/heap/cppgc/member.h"

namespace cppgc {
namespace internal {

static_assert(api_constants::kCagedHeapReservationSize ==
              kCagedHeapReservationSize);
static_assert(api_constants::kCagedHeapReservationAlignment ==
              kCagedHeapReservationAlignment);
static_assert(api_constants::kCagedHeapNormalPageReservationSize ==
              kCagedHeapNormalPageReservationSize);

uintptr_t CagedHeapBase::g_heap_base_ = 0u;

CagedHeap* CagedHeap::instance_ = nullptr;

namespace {

VirtualMemory ReserveCagedHeap(PageAllocator& platform_allocator) {
  DCHECK_EQ(0u,
            kCagedHeapReservationSize % platform_allocator.AllocatePageSize());

  static constexpr size_t kAllocationTries = 4;
  for (size_t i = 0; i < kAllocationTries; ++i) {
#if defined(CPPGC_POINTER_COMPRESSION)
    // If pointer compression is enabled, reserve 2x of cage size and leave the
    // half that has the least significant bit of the most significant halfword
    // set. This is needed for compression to make sure that compressed normal
    // pointers have the most significant bit set to 1, so that on decompression
    // the bit will be sign-extended. This saves us a branch and 'or' operation
    // during compression.
    // TODO(chromium:1325007): Provide API in PageAllocator to left trim
    // allocations and return the half of the reservation back to the OS.
    static constexpr size_t kTryReserveSize = 2 * kCagedHeapReservationSize;
    static constexpr size_t kTryReserveAlignment =
        2 * kCagedHeapReservationAlignment;
#else   // !defined(CPPGC_POINTER_COMPRESSION)
    static constexpr size_t kTryReserveSize = kCagedHeapReservationSize;
    static constexpr size_t kTryReserveAlignment =
        kCagedHeapReservationAlignment;
#endif  // !defined(CPPGC_POINTER_COMPRESSION)
    void* hint = reinterpret_cast<void*>(RoundDown(
        reinterpret_cast<uintptr_t>(platform_allocator.GetRandomMmapAddr()),
        kTryReserveAlignment));

    VirtualMemory memory(&platform_allocator, kTryReserveSize,
                         kTryReserveAlignment, hint);
    if (memory.IsReserved()) return memory;
  }

  GetGlobalOOMHandler()("Oilpan: CagedHeap reservation.");
}

}  // namespace

// static
void CagedHeap::InitializeIfNeeded(PageAllocator& platform_allocator) {
  static v8::base::LeakyObject<CagedHeap> caged_heap(platform_allocator);
}

// static
CagedHeap& CagedHeap::Instance() {
  DCHECK_NOT_NULL(instance_);
  return *instance_;
}

CagedHeap::CagedHeap(PageAllocator& platform_allocator)
    : reserved_area_(ReserveCagedHeap(platform_allocator)) {
  using CagedAddress = CagedHeap::AllocatorType::Address;

#if defined(CPPGC_POINTER_COMPRESSION)
  static constexpr size_t kBaseOffset = kCagedHeapReservationSize;
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  static constexpr size_t kBaseOffset = 0;
#endif  //! defined(CPPGC_POINTER_COMPRESSION)

  void* const cage_start =
      static_cast<uint8_t*>(reserved_area_.address()) + kBaseOffset;

  CagedHeapBase::g_heap_base_ = reinterpret_cast<uintptr_t>(cage_start);

#if defined(CPPGC_POINTER_COMPRESSION)
  // With pointer compression only single heap per thread is allowed.
  CHECK(!CageBaseGlobal::IsSet());
  CageBaseGlobalUpdater::UpdateCageBase(CagedHeapBase::g_heap_base_);
#endif  // defined(CPPGC_POINTER_COMPRESSION)

  if (!platform_allocator.SetPermissions(
          cage_start,
          RoundUp(sizeof(CagedHeapLocalData),
                  platform_allocator.CommitPageSize()),
          PageAllocator::kReadWrite)) {
    GetGlobalOOMHandler()("Oilpan: CagedHeap commit CageHeapLocalData.");
  }

  const CagedAddress caged_heap_start = RoundUp(
      reinterpret_cast<CagedAddress>(cage_start) + sizeof(CagedHeapLocalData),
      kPageSize);
  const size_t local_data_size_with_padding =
      caged_heap_start - reinterpret_cast<CagedAddress>(cage_start);

  normal_page_bounded_allocator_ = std::make_unique<
      v8::base::BoundedPageAllocator>(
      &platform_allocator, caged_heap_start,
      kCagedHeapNormalPageReservationSize - local_data_size_with_padding,
      kPageSize,
      v8::base::PageInitializationMode::kAllocatedPagesMustBeZeroInitialized,
      v8::base::PageFreeingMode::kMakeInaccessible);

  large_page_bounded_allocator_ = std::make_unique<
      v8::base::BoundedPageAllocator>(
      &platform_allocator,
      reinterpret_cast<uintptr_t>(cage_start) +
          kCagedHeapNormalPageReservationSize,
      kCagedHeapNormalPageReservationSize, kPageSize,
      v8::base::PageInitializationMode::kAllocatedPagesMustBeZeroInitialized,
      v8::base::PageFreeingMode::kMakeInaccessible);

  instance_ = this;
}

void CagedHeap::NotifyLargePageCreated(LargePage* page) {
  DCHECK(page);
  v8::base::MutexGuard guard(&large_pages_mutex_);
  auto result = large_pages_.insert(page);
  USE(result);
  DCHECK(result.second);
}

void CagedHeap::NotifyLargePageDestroyed(LargePage* page) {
  DCHECK(page);
  v8::base::MutexGuard guard(&large_pages_mutex_);
  auto size = large_pages_.erase(page);
  USE(size);
  DCHECK_EQ(1u, size);
}

BasePage& CagedHeap::LookupPageFromInnerPointer(void* ptr) const {
  DCHECK(IsOnHeap(ptr));
  if (V8_LIKELY(CagedHeapBase::IsWithinNormalPageReservation(ptr))) {
    return *NormalPage::FromPayload(ptr);
  } else {
    return LookupLargePageFromInnerPointer(ptr);
  }
}

LargePage& CagedHeap::LookupLargePageFromInnerPointer(void* ptr) const {
  v8::base::MutexGuard guard(&large_pages_mutex_);
  auto it = large_pages_.upper_bound(static_cast<LargePage*>(ptr));
  DCHECK_NE(large_pages_.begin(), it);
  auto* page = *std::next(it, -1);
  DCHECK(page);
  DCHECK(page->PayloadContains(static_cast<ConstAddress>(ptr)));
  return *page;
}

void CagedHeap::ResetForTesting() {
  v8::base::MutexGuard guard(&large_pages_mutex_);
  // Clear the large pages to support tests within the same process.
  large_pages_.clear();
}

// static
BasePageHandle& CagedHeapBase::LookupLargePageFromInnerPointer(void* address) {
  auto& page = CagedHeap::Instance().LookupLargePageFromInnerPointer(address);
  return page;
}

}  // namespace internal
}  // namespace cppgc
