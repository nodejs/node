// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/isolate-allocator.h"
#include "src/base/bounded-page-allocator.h"
#include "src/common/ptr-compr.h"
#include "src/execution/isolate.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

IsolateAllocator::IsolateAllocator() {
#if defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  isolate_cage_.InitReservationOrDie();
  page_allocator_ = isolate_cage_.page_allocator();
  CommitPagesForIsolate(isolate_cage_.base());
#elif defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
  // Allocate Isolate in C++ heap when sharing a cage.
  CHECK(PtrComprCage::GetProcessWideCage()->IsReserved());
  page_allocator_ = PtrComprCage::GetProcessWideCage()->page_allocator();
  isolate_memory_ = ::operator new(sizeof(Isolate));
#else
  // Allocate Isolate in C++ heap.
  page_allocator_ = GetPlatformPageAllocator();
  isolate_memory_ = ::operator new(sizeof(Isolate));
#endif  // V8_COMPRESS_POINTERS

  CHECK_NOT_NULL(page_allocator_);
}

IsolateAllocator::~IsolateAllocator() {
#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  if (isolate_cage_.reservation_.IsReserved()) {
    // The actual memory will be freed when the |isolate_cage_| will die.
    return;
  }
#endif

  // The memory was allocated in C++ heap.
  ::operator delete(isolate_memory_);
}

Address IsolateAllocator::GetPtrComprCageBaseAddress() const {
#if defined V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  return isolate_cage_.base();
#elif defined V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  return PtrComprCage::GetProcessWideCage()->base();
#else
  return kNullAddress;
#endif
}

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
namespace {

// "IsolateRootBiasPage" is an optional region before the 4Gb aligned
// reservation. This "IsolateRootBiasPage" page is supposed to be used for
// storing part of the Isolate object when Isolate::isolate_root_bias() is
// not zero.
inline size_t GetIsolateRootBiasPageSize(
    v8::PageAllocator* platform_page_allocator) {
  return RoundUp(Isolate::isolate_root_bias(),
                 platform_page_allocator->AllocatePageSize());
}

}  // namespace

void IsolateAllocator::CommitPagesForIsolate(Address heap_reservation_address) {
  const size_t kIsolateRootBiasPageSize =
      GetIsolateRootBiasPageSize(page_allocator_);

  Address isolate_root = heap_reservation_address + kIsolateRootBiasPageSize;
  CHECK(IsAligned(isolate_root, kPtrComprCageBaseAlignment));

  CHECK(isolate_cage_.reservation_.InVM(
      heap_reservation_address,
      kPtrComprCageReservationSize + kIsolateRootBiasPageSize));

  size_t page_size = page_allocator_->AllocatePageSize();
  Address isolate_address = isolate_root - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);

  // Inform the bounded page allocator about reserved pages.
  {
    Address reserved_region_address = isolate_root;
    size_t reserved_region_size =
        RoundUp(isolate_end, page_size) - reserved_region_address;

    CHECK(isolate_cage_.page_allocator()->AllocatePagesAt(
        reserved_region_address, reserved_region_size,
        PageAllocator::Permission::kNoAccess));
  }

  // Commit pages where the Isolate will be stored.
  {
    size_t commit_page_size = page_allocator_->CommitPageSize();
    Address committed_region_address =
        RoundDown(isolate_address, commit_page_size);
    size_t committed_region_size =
        RoundUp(isolate_end, commit_page_size) - committed_region_address;

    // We are using |isolate_cage_.reservation_| directly here because
    // |page_allocator_| has bigger commit page size than we actually need.
    CHECK(isolate_cage_.reservation_.SetPermissions(committed_region_address,
                                                    committed_region_size,
                                                    PageAllocator::kReadWrite));

    if (Heap::ShouldZapGarbage()) {
      MemsetPointer(reinterpret_cast<Address*>(committed_region_address),
                    kZapValue, committed_region_size / kSystemPointerSize);
    }
  }
  isolate_memory_ = reinterpret_cast<void*>(isolate_address);
}
#endif  // V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE

}  // namespace internal
}  // namespace v8
