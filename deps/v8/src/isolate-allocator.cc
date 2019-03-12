// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate-allocator.h"
#include "src/base/bounded-page-allocator.h"
#include "src/isolate.h"
#include "src/ptr-compr.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

IsolateAllocator::IsolateAllocator(IsolateAllocationMode mode) {
#if V8_TARGET_ARCH_64_BIT
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_base = InitReservation();
    CommitPagesForIsolate(heap_base);
    return;
  }
#endif  // V8_TARGET_ARCH_64_BIT

  // Allocate Isolate in C++ heap.
  CHECK_EQ(mode, IsolateAllocationMode::kInCppHeap);
  page_allocator_ = GetPlatformPageAllocator();
  isolate_memory_ = ::operator new(sizeof(Isolate));
  DCHECK(!reservation_.IsReserved());
}

IsolateAllocator::~IsolateAllocator() {
  if (reservation_.IsReserved()) {
    // The actual memory will be freed when the |reservation_| will die.
    return;
  }

  // The memory was allocated in C++ heap.
  ::operator delete(isolate_memory_);
}

#if V8_TARGET_ARCH_64_BIT
Address IsolateAllocator::InitReservation() {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  // Reserve a 4Gb region so that the middle is 4Gb aligned.
  // The VirtualMemory API does not support such an constraint so we have to
  // implement it manually here.
  size_t reservation_size = kPtrComprHeapReservationSize;
  size_t base_alignment = kPtrComprIsolateRootAlignment;

  const int kMaxAttempts = 3;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Address hint = RoundDown(reinterpret_cast<Address>(
                                 platform_page_allocator->GetRandomMmapAddr()),
                             base_alignment) +
                   kPtrComprIsolateRootBias;

    // Within this reservation there will be a sub-region with proper alignment.
    VirtualMemory padded_reservation(platform_page_allocator,
                                     reservation_size * 2,
                                     reinterpret_cast<void*>(hint));
    if (!padded_reservation.IsReserved()) break;

    // Find such a sub-region inside the reservation that it's middle is
    // |base_alignment|-aligned.
    Address address =
        RoundUp(padded_reservation.address() + kPtrComprIsolateRootBias,
                base_alignment) -
        kPtrComprIsolateRootBias;
    CHECK(padded_reservation.InVM(address, reservation_size));

    // Now free the padded reservation and immediately try to reserve an exact
    // region at aligned address. We have to do this dancing because the
    // reservation address requirement is more complex than just a certain
    // alignment and not all operating systems support freeing parts of reserved
    // address space regions.
    padded_reservation.Free();

    VirtualMemory reservation(platform_page_allocator, reservation_size,
                              reinterpret_cast<void*>(address));
    if (!reservation.IsReserved()) break;

    // The reservation could still be somewhere else but we can accept it
    // if the reservation has the required alignment.
    Address aligned_address =
        RoundUp(reservation.address() + kPtrComprIsolateRootBias,
                base_alignment) -
        kPtrComprIsolateRootBias;

    if (reservation.address() == aligned_address) {
      reservation_ = std::move(reservation);
      break;
    }
  }
  if (!reservation_.IsReserved()) {
    V8::FatalProcessOutOfMemory(nullptr,
                                "Failed to reserve memory for new V8 Isolate");
  }

  CHECK_EQ(reservation_.size(), reservation_size);

  Address heap_base = reservation_.address() + kPtrComprIsolateRootBias;
  CHECK(IsAligned(heap_base, base_alignment));

  return heap_base;
}

void IsolateAllocator::CommitPagesForIsolate(Address heap_base) {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  // Simplify BoundedPageAllocator's life by configuring it to use same page
  // size as the Heap will use (MemoryChunk::kPageSize).
  size_t page_size = RoundUp(size_t{1} << kPageSizeBits,
                             platform_page_allocator->AllocatePageSize());

  page_allocator_instance_ = base::make_unique<base::BoundedPageAllocator>(
      platform_page_allocator, reservation_.address(), reservation_.size(),
      page_size);
  page_allocator_ = page_allocator_instance_.get();

  Address isolate_address = heap_base - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);

  // Inform the bounded page allocator about reserved pages.
  {
    Address reserved_region_address = RoundDown(isolate_address, page_size);
    size_t reserved_region_size =
        RoundUp(isolate_end, page_size) - reserved_region_address;

    CHECK(page_allocator_instance_->AllocatePagesAt(
        reserved_region_address, reserved_region_size,
        PageAllocator::Permission::kNoAccess));
  }

  // Commit pages where the Isolate will be stored.
  {
    size_t commit_page_size = platform_page_allocator->CommitPageSize();
    Address committed_region_address =
        RoundDown(isolate_address, commit_page_size);
    size_t committed_region_size =
        RoundUp(isolate_end, commit_page_size) - committed_region_address;

    // We are using |reservation_| directly here because |page_allocator_| has
    // bigger commit page size than we actually need.
    CHECK(reservation_.SetPermissions(committed_region_address,
                                      committed_region_size,
                                      PageAllocator::kReadWrite));

    if (Heap::ShouldZapGarbage()) {
      for (Address address = committed_region_address;
           address < committed_region_size; address += kSystemPointerSize) {
        Memory<Address>(address) = static_cast<Address>(kZapValue);
      }
    }
  }
  isolate_memory_ = reinterpret_cast<void*>(isolate_address);
}
#endif  // V8_TARGET_ARCH_64_BIT

}  // namespace internal
}  // namespace v8
