// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/ptr-compr-cage.h"

#include "src/common/ptr-compr-inl.h"

namespace v8 {
namespace internal {

PtrComprCage::PtrComprCage() = default;

// static
void PtrComprCage::InitializeOncePerProcess() {
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  GetProcessWideCage()->InitReservationOrDie();
#endif
}

#ifdef V8_COMPRESS_POINTERS

PtrComprCage::~PtrComprCage() { Free(); }

bool PtrComprCage::InitReservation() {
  CHECK(!IsReserved());

  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  // Reserve a 4Gb region such as that the reservation address is 4Gb aligned.
  const size_t reservation_size = kPtrComprCageReservationSize;
  const size_t base_alignment = kPtrComprCageBaseAlignment;

  const int kMaxAttempts = 4;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Address hint = RoundDown(
        reinterpret_cast<Address>(platform_page_allocator->GetRandomMmapAddr()),
        base_alignment);

    // Within this reservation there will be a sub-region with proper alignment.
    VirtualMemory padded_reservation(platform_page_allocator,
                                     reservation_size * 2,
                                     reinterpret_cast<void*>(hint));
    if (!padded_reservation.IsReserved()) break;

    // Find properly aligned sub-region inside the reservation.
    Address address = RoundUp(padded_reservation.address(), base_alignment);
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
        base_ = address;
        break;
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
      Address address = RoundUp(reservation.address(), base_alignment);

      if (reservation.address() == address) {
        reservation_ = std::move(reservation);
        CHECK_EQ(reservation_.size(), reservation_size);
        base_ = address;
        break;
      }
    }
  }

  if (base_ == kNullAddress) return false;

  // Simplify BoundedPageAllocator's life by configuring it to use same page
  // size as the Heap will use (MemoryChunk::kPageSize).
  size_t page_size = RoundUp(size_t{1} << kPageSizeBits,
                             platform_page_allocator->AllocatePageSize());

  page_allocator_ = std::make_unique<base::BoundedPageAllocator>(
      platform_page_allocator, base_, kPtrComprCageReservationSize, page_size);

  return true;
}

void PtrComprCage::InitReservationOrDie() {
  if (!InitReservation()) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Failed to reserve memory for V8 pointer compression cage");
  }
}

void PtrComprCage::Free() {
  if (IsReserved()) {
    base_ = kNullAddress;
    page_allocator_.reset();
    reservation_.Free();
  }
}

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE

namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(PtrComprCage, GetSharedProcessWideCage)
}  // anonymous namespace

// static
PtrComprCage* PtrComprCage::GetProcessWideCage() {
  return GetSharedProcessWideCage();
}

#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#endif  // V8_COMPRESS_POINTERS

}  // namespace internal
}  // namespace v8
