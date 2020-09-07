// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_CAGED_HEAP_H_
#define V8_HEAP_CPPGC_CAGED_HEAP_H_

#include <memory>

#include "include/cppgc/platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/virtual-memory.h"

namespace cppgc {
namespace internal {

struct CagedHeapLocalData;
class HeapBase;

class CagedHeap final {
 public:
  using AllocatorType = v8::base::BoundedPageAllocator;

  CagedHeap(HeapBase* heap, PageAllocator* platform_allocator);

  CagedHeap(const CagedHeap&) = delete;
  CagedHeap& operator=(const CagedHeap&) = delete;

  AllocatorType& allocator() { return *bounded_allocator_; }
  const AllocatorType& allocator() const { return *bounded_allocator_; }

  CagedHeapLocalData& local_data() {
    return *static_cast<CagedHeapLocalData*>(reserved_area_.address());
  }
  const CagedHeapLocalData& local_data() const {
    return *static_cast<CagedHeapLocalData*>(reserved_area_.address());
  }

  static uintptr_t OffsetFromAddress(void* address) {
    return reinterpret_cast<uintptr_t>(address) &
           (kCagedHeapReservationAlignment - 1);
  }

 private:
  VirtualMemory reserved_area_;
  std::unique_ptr<AllocatorType> bounded_allocator_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_CAGED_HEAP_H_
