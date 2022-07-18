// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_CAGED_HEAP_H_
#define V8_HEAP_CPPGC_CAGED_HEAP_H_

#include <limits>
#include <memory>
#include <set>

#include "include/cppgc/internal/caged-heap.h"
#include "include/cppgc/platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/mutex.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/virtual-memory.h"

namespace cppgc {
namespace internal {

namespace testing {
class TestWithHeap;
}

struct CagedHeapLocalData;
class BasePage;
class LargePage;

class V8_EXPORT_PRIVATE CagedHeap final {
 public:
  using AllocatorType = v8::base::BoundedPageAllocator;

  template <typename RetType = uintptr_t>
  static RetType OffsetFromAddress(const void* address) {
    static_assert(
        std::numeric_limits<RetType>::max() >= (kCagedHeapReservationSize - 1),
        "The return type should be large enough");
    return reinterpret_cast<uintptr_t>(address) &
           (kCagedHeapReservationAlignment - 1);
  }

  static uintptr_t BaseFromAddress(const void* address) {
    return reinterpret_cast<uintptr_t>(address) &
           ~(kCagedHeapReservationAlignment - 1);
  }

  static bool IsWithinNormalPageReservation(const void* address) {
    return OffsetFromAddress(address) < kCagedHeapNormalPageReservationSize;
  }

  static void InitializeIfNeeded(PageAllocator&);

  static CagedHeap& Instance();

  CagedHeap(const CagedHeap&) = delete;
  CagedHeap& operator=(const CagedHeap&) = delete;

  AllocatorType& normal_page_allocator() {
    return *normal_page_bounded_allocator_;
  }
  const AllocatorType& normal_page_allocator() const {
    return *normal_page_bounded_allocator_;
  }

  AllocatorType& large_page_allocator() {
    return *large_page_bounded_allocator_;
  }
  const AllocatorType& large_page_allocator() const {
    return *large_page_bounded_allocator_;
  }

  void NotifyLargePageCreated(LargePage* page);
  void NotifyLargePageDestroyed(LargePage* page);

  BasePage& LookupPageFromInnerPointer(void* ptr) const;
  LargePage& LookupLargePageFromInnerPointer(void* ptr) const;

  bool IsOnHeap(const void* address) const {
    DCHECK_EQ(reserved_area_.address(),
              reinterpret_cast<void*>(CagedHeapBase::GetBase()));
    return reinterpret_cast<void*>(BaseFromAddress(address)) ==
           reserved_area_.address();
  }

  void* base() const { return reserved_area_.address(); }

 private:
  friend class v8::base::LeakyObject<CagedHeap>;
  friend class testing::TestWithHeap;

  explicit CagedHeap(PageAllocator& platform_allocator);

  void ResetForTesting();

  static CagedHeap* instance_;

  const VirtualMemory reserved_area_;
  // BoundedPageAllocator is thread-safe, no need to use external
  // synchronization.
  std::unique_ptr<AllocatorType> normal_page_bounded_allocator_;
  std::unique_ptr<AllocatorType> large_page_bounded_allocator_;

  std::set<LargePage*> large_pages_;
  // TODO(chromium:1325007): Since writes are rare, consider using read-write
  // lock to speed up reading.
  mutable v8::base::Mutex large_pages_mutex_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_CAGED_HEAP_H_
