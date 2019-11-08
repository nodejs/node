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

IsolateAllocator::IsolateAllocator(IsolateAllocationMode mode) {
#if V8_TARGET_ARCH_64_BIT
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_reservation_address = InitReservation();
    CommitPagesForIsolate(heap_reservation_address);
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

Address IsolateAllocator::InitReservation() {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  const size_t kIsolateRootBiasPageSize =
      GetIsolateRootBiasPageSize(platform_page_allocator);

  // Reserve a |4Gb + kIsolateRootBiasPageSize| region such as that the
  // resevation address plus |kIsolateRootBiasPageSize| is 4Gb aligned.
  const size_t reservation_size =
      kPtrComprHeapReservationSize + kIsolateRootBiasPageSize;
  const size_t base_alignment = kPtrComprIsolateRootAlignment;

  const int kMaxAttempts = 4;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Address hint = RoundDown(reinterpret_cast<Address>(
                                 platform_page_allocator->GetRandomMmapAddr()),
                             base_alignment) -
                   kIsolateRootBiasPageSize;

    // Within this reservation there will be a sub-region with proper alignment.
    VirtualMemory padded_reservation(platform_page_allocator,
                                     reservation_size * 2,
                                     reinterpret_cast<void*>(hint));
    if (!padded_reservation.IsReserved()) break;

    // Find properly aligned sub-region inside the reservation.
    Address address =
        RoundUp(padded_reservation.address() + kIsolateRootBiasPageSize,
                base_alignment) -
        kIsolateRootBiasPageSize;
    CHECK(padded_reservation.InVM(address, reservation_size));

#if defined(V8_OS_FUCHSIA)
    // Fuchsia does not respect given hints so as a workaround we will use
    // overreserved address space region instead of trying to re-reserve
    // a subregion.
    bool overreserve = true;
#else
    // For the last attempt use the overreserved region to avoid an OOM crash.
    // This case can happen if there are many isolates being created in
    // parallel that race for reserving the regions.
    bool overreserve = (attempt == kMaxAttempts - 1);
#endif

    if (overreserve) {
      if (padded_reservation.InVM(address, reservation_size)) {
        reservation_ = std::move(padded_reservation);
        return address;
      }
    } else {
      // Now free the padded reservation and immediately try to reserve an exact
      // region at aligned address. We have to do this dancing because the
      // reservation address requirement is more complex than just a certain
      // alignment and not all operating systems support freeing parts of
      // reserved address space regions.
      padded_reservation.Free();

      VirtualMemory reservation(platform_page_allocator, reservation_size,
                                reinterpret_cast<void*>(address));
      if (!reservation.IsReserved()) break;

      // The reservation could still be somewhere else but we can accept it
      // if it has the required alignment.
      Address address =
          RoundUp(reservation.address() + kIsolateRootBiasPageSize,
                  base_alignment) -
          kIsolateRootBiasPageSize;

      if (reservation.address() == address) {
        reservation_ = std::move(reservation);
        CHECK_EQ(reservation_.size(), reservation_size);
        return address;
      }
    }
  }
  V8::FatalProcessOutOfMemory(nullptr,
                              "Failed to reserve memory for new V8 Isolate");
  return kNullAddress;
}

void IsolateAllocator::CommitPagesForIsolate(Address heap_reservation_address) {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  const size_t kIsolateRootBiasPageSize =
      GetIsolateRootBiasPageSize(platform_page_allocator);

  Address isolate_root = heap_reservation_address + kIsolateRootBiasPageSize;
  CHECK(IsAligned(isolate_root, kPtrComprIsolateRootAlignment));

  CHECK(reservation_.InVM(
      heap_reservation_address,
      kPtrComprHeapReservationSize + kIsolateRootBiasPageSize));

  // Simplify BoundedPageAllocator's life by configuring it to use same page
  // size as the Heap will use (MemoryChunk::kPageSize).
  size_t page_size = RoundUp(size_t{1} << kPageSizeBits,
                             platform_page_allocator->AllocatePageSize());

  page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      platform_page_allocator, isolate_root, kPtrComprHeapReservationSize,
      page_size);
  page_allocator_ = page_allocator_instance_.get();

  Address isolate_address = isolate_root - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);

  // Inform the bounded page allocator about reserved pages.
  {
    Address reserved_region_address = isolate_root;
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
      MemsetPointer(reinterpret_cast<Address*>(committed_region_address),
                    kZapValue, committed_region_size / kSystemPointerSize);
    }
  }
  isolate_memory_ = reinterpret_cast<void*>(isolate_address);
}
#endif  // V8_TARGET_ARCH_64_BIT

}  // namespace internal
}  // namespace v8
