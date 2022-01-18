// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/security/vm-cage.h"

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/cpu.h"
#include "src/base/lazy-instance.h"
#include "src/base/utils/random-number-generator.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"

#if defined(V8_OS_WIN)
#include <windows.h>
// This has to come after windows.h.
#include <versionhelpers.h>  // For IsWindows8Point1OrGreater().
#endif

namespace v8 {
namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

// A PageAllocator that allocates pages inside a given virtual address range
// like the BoundedPageAllocator, except that only a (small) part of the range
// has actually been reserved. As such, this allocator relies on page
// allocation hints for the OS to obtain pages inside the non-reserved part.
// This allocator is used on OSes where reserving virtual address space (and
// thus a virtual memory cage) is too expensive, notabley Windows pre 8.1.
class FakeBoundedPageAllocator : public v8::PageAllocator {
 public:
  FakeBoundedPageAllocator(v8::PageAllocator* page_allocator, Address start,
                           size_t size, size_t reserved_size)
      : page_allocator_(page_allocator),
        start_(start),
        size_(size),
        reserved_size_(reserved_size),
        end_of_reserved_region_(start + reserved_size) {
    // The size is required to be a power of two so that obtaining a random
    // address inside the managed region simply requires a fixed number of
    // random bits as offset.
    DCHECK(base::bits::IsPowerOfTwo(size));
    DCHECK_LT(reserved_size, size);

    if (FLAG_random_seed != 0) {
      rng_.SetSeed(FLAG_random_seed);
    }

    reserved_region_page_allocator_ =
        std::make_unique<base::BoundedPageAllocator>(
            page_allocator_, start_, reserved_size_,
            page_allocator_->AllocatePageSize(),
            base::PageInitializationMode::kAllocatedPagesMustBeZeroInitialized);
  }

  ~FakeBoundedPageAllocator() override = default;

  size_t AllocatePageSize() override {
    return page_allocator_->AllocatePageSize();
  }

  size_t CommitPageSize() override { return page_allocator_->CommitPageSize(); }

  void SetRandomMmapSeed(int64_t seed) override { rng_.SetSeed(seed); }

  void* GetRandomMmapAddr() override {
    // Generate a random number between 0 and size_, then add that to the start
    // address to obtain a random mmap address. We deliberately don't use our
    // provided page allocator's GetRandomMmapAddr here since that could be
    // biased, while we want uniformly distributed random numbers here.
    Address addr = rng_.NextInt64() % size_ + start_;
    addr = RoundDown(addr, AllocatePageSize());
    void* ptr = reinterpret_cast<void*>(addr);
    DCHECK(Contains(ptr, 1));
    return ptr;
  }

  void* AllocatePages(void* hint, size_t size, size_t alignment,
                      Permission access) override {
    DCHECK(IsAligned(size, AllocatePageSize()));
    DCHECK(IsAligned(alignment, AllocatePageSize()));

    // First, try allocating the memory inside the reserved region.
    void* ptr = reserved_region_page_allocator_->AllocatePages(
        hint, size, alignment, access);
    if (ptr) return ptr;

    // Then, fall back to allocating memory outside of the reserved region
    // through page allocator hints.

    // Somewhat arbitrary size limitation to ensure that the loop below for
    // finding a fitting base address hint terminates quickly.
    if (size >= size_ / 2) return nullptr;

    if (!hint || !Contains(hint, size)) hint = GetRandomMmapAddr();

    static constexpr int kMaxAttempts = 10;
    for (int i = 0; i < kMaxAttempts; i++) {
      // If the hint wouldn't result in the entire allocation being inside the
      // managed region, simply retry. There is at least a 50% chance of
      // getting a usable address due to the size restriction above.
      while (!Contains(hint, size)) {
        hint = GetRandomMmapAddr();
      }

      ptr = page_allocator_->AllocatePages(hint, size, alignment, access);
      if (ptr && Contains(ptr, size)) {
        return ptr;
      } else if (ptr) {
        page_allocator_->FreePages(ptr, size);
      }

      // Retry at a different address.
      hint = GetRandomMmapAddr();
    }

    return nullptr;
  }

  bool FreePages(void* address, size_t size) override {
    return AllocatorFor(address)->FreePages(address, size);
  }

  bool ReleasePages(void* address, size_t size, size_t new_length) override {
    return AllocatorFor(address)->ReleasePages(address, size, new_length);
  }

  bool SetPermissions(void* address, size_t size,
                      Permission permissions) override {
    return AllocatorFor(address)->SetPermissions(address, size, permissions);
  }

  bool DiscardSystemPages(void* address, size_t size) override {
    return AllocatorFor(address)->DiscardSystemPages(address, size);
  }

  bool DecommitPages(void* address, size_t size) override {
    return AllocatorFor(address)->DecommitPages(address, size);
  }

 private:
  bool Contains(void* ptr, size_t length) {
    Address addr = reinterpret_cast<Address>(ptr);
    return (addr >= start_) && ((addr + length) < (start_ + size_));
  }

  v8::PageAllocator* AllocatorFor(void* ptr) {
    Address addr = reinterpret_cast<Address>(ptr);
    if (addr < end_of_reserved_region_) {
      DCHECK_GE(addr, start_);
      return reserved_region_page_allocator_.get();
    } else {
      return page_allocator_;
    }
  }

  // The page allocator through which pages inside the region are allocated.
  v8::PageAllocator* const page_allocator_;
  // The bounded page allocator managing the sub-region that was actually
  // reserved.
  std::unique_ptr<base::BoundedPageAllocator> reserved_region_page_allocator_;

  // Random number generator for generating random addresses.
  base::RandomNumberGenerator rng_;

  // The start of the virtual memory region in which to allocate pages. This is
  // also the start of the sub-region that was reserved.
  const Address start_;
  // The total size of the address space in which to allocate pages.
  const size_t size_;
  // The size of the sub-region that has actually been reserved.
  const size_t reserved_size_;
  // The end of the sub-region that has actually been reserved.
  const Address end_of_reserved_region_;
};

// Best-effort helper function to determine the size of the userspace virtual
// address space. Used to determine appropriate cage size and placement.
static Address DetermineAddressSpaceLimit() {
#ifndef V8_TARGET_ARCH_64_BIT
#error Unsupported target architecture.
#endif

  // Assume 48 bits by default, which seems to be the most common configuration.
  constexpr unsigned kDefaultVirtualAddressBits = 48;
  // 36 bits should realistically be the lowest value we could ever see.
  constexpr unsigned kMinVirtualAddressBits = 36;
  constexpr unsigned kMaxVirtualAddressBits = 64;

  constexpr size_t kMinVirtualAddressSpaceSize = 1ULL << kMinVirtualAddressBits;
  static_assert(kMinVirtualAddressSpaceSize >= kVirtualMemoryCageMinimumSize,
                "The minimum cage size should be smaller or equal to the "
                "smallest possible userspace address space. Otherwise, larger "
                "parts of the cage will not be usable on those platforms.");

#ifdef V8_TARGET_ARCH_X64
  base::CPU cpu;
  Address virtual_address_bits = kDefaultVirtualAddressBits;
  if (cpu.exposes_num_virtual_address_bits()) {
    virtual_address_bits = cpu.num_virtual_address_bits();
  }
#else
  // TODO(saelo) support ARM and possibly other CPUs as well.
  Address virtual_address_bits = kDefaultVirtualAddressBits;
#endif

  // Guard against nonsensical values.
  if (virtual_address_bits < kMinVirtualAddressBits ||
      virtual_address_bits > kMaxVirtualAddressBits) {
    virtual_address_bits = kDefaultVirtualAddressBits;
  }

  // Assume virtual address space is split 50/50 between userspace and kernel.
  Address userspace_virtual_address_bits = virtual_address_bits - 1;
  Address address_space_limit = 1ULL << userspace_virtual_address_bits;

#if defined(V8_OS_WIN_X64)
  if (!IsWindows8Point1OrGreater()) {
    // On Windows pre 8.1 userspace is limited to 8TB on X64. See
    // https://docs.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases
    address_space_limit = 8ULL * TB;
  }
#endif  // V8_OS_WIN_X64

  // TODO(saelo) we could try allocating memory in the upper half of the address
  // space to see if it is really usable.
  return address_space_limit;
}

bool V8VirtualMemoryCage::Initialize(PageAllocator* page_allocator) {
  // Take the number of virtual address bits into account when determining the
  // size of the cage. For example, if there are only 39 bits available, split
  // evenly between userspace and kernel, then userspace can only address 256GB
  // and so we use a quarter of that, 64GB, as maximum cage size.
  Address address_space_limit = DetermineAddressSpaceLimit();
  size_t max_cage_size = address_space_limit / 4;
  size_t cage_size = std::min(kVirtualMemoryCageSize, max_cage_size);
  size_t size_to_reserve = cage_size;

  // If the size is less than the minimum cage size though, we fall back to
  // creating a fake cage. This happens for CPUs with only 36 virtual address
  // bits, in which case the cage size would end up being only 8GB.
  bool create_fake_cage = false;
  if (cage_size < kVirtualMemoryCageMinimumSize) {
    static_assert((8ULL * GB) >= kFakeVirtualMemoryCageMinReservationSize,
                  "Minimum reservation size for a fake cage must be at most "
                  "8GB to support CPUs with only 36 virtual address bits");
    size_to_reserve = cage_size;
    cage_size = kVirtualMemoryCageMinimumSize;
    create_fake_cage = true;
  }

#if defined(V8_OS_WIN)
  if (!IsWindows8Point1OrGreater()) {
    // On Windows pre 8.1, reserving virtual memory is an expensive operation,
    // apparently because the OS already charges for the memory required for
    // all page table entries. For example, a 1TB reservation increases private
    // memory usage by 2GB. As such, it is not possible to create a proper
    // virtual memory cage there and so a fake cage is created which doesn't
    // reserve most of the virtual memory, and so doesn't incur the cost, but
    // also doesn't provide the desired security benefits.
    size_to_reserve = kFakeVirtualMemoryCageMinReservationSize;
    create_fake_cage = true;
  }
#endif  // V8_OS_WIN

  // In any case, the (fake) cage must be at most as large as our address space.
  DCHECK_LE(cage_size, address_space_limit);

  if (create_fake_cage) {
    return InitializeAsFakeCage(page_allocator, cage_size, size_to_reserve);
  } else {
    // TODO(saelo) if this fails, we could still fall back to creating a fake
    // cage. Decide if that would make sense.
    const bool use_guard_regions = true;
    return Initialize(page_allocator, cage_size, use_guard_regions);
  }
}

bool V8VirtualMemoryCage::Initialize(v8::PageAllocator* page_allocator,
                                     size_t size, bool use_guard_regions) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK_GE(size, kVirtualMemoryCageMinimumSize);

  // Currently, we allow the cage to be smaller than the requested size. This
  // way, we can gracefully handle cage reservation failures during the initial
  // rollout and can collect data on how often these occur. In the future, we
  // will likely either require the cage to always have a fixed size or will
  // design CagedPointers (pointers that are guaranteed to point into the cage,
  // e.g. because they are stored as offsets from the cage base) in a way that
  // doesn't reduce the cage's security properties if it has a smaller size.
  // Which of these options is ultimately taken likey depends on how frequently
  // cage reservation failures occur in practice.
  size_t reservation_size;
  while (!reservation_base_ && size >= kVirtualMemoryCageMinimumSize) {
    reservation_size = size;
    if (use_guard_regions) {
      reservation_size += 2 * kVirtualMemoryCageGuardRegionSize;
    }

    // Technically, we should use kNoAccessWillJitLater here instead since the
    // cage will contain JIT pages. However, currently this is not required as
    // PA anyway uses MAP_JIT for V8 mappings. Further, we want to eventually
    // move JIT pages out of the cage, at which point we'd like to forbid
    // making pages inside the cage executable, and so don't want MAP_JIT.
    Address hint = RoundDown(
        reinterpret_cast<Address>(page_allocator->GetRandomMmapAddr()),
        kVirtualMemoryCageAlignment);
    reservation_base_ = reinterpret_cast<Address>(page_allocator->AllocatePages(
        reinterpret_cast<void*>(hint), reservation_size,
        kVirtualMemoryCageAlignment, PageAllocator::kNoAccess));
    if (!reservation_base_) {
      size /= 2;
    }
  }

  if (!reservation_base_) return false;

  base_ = reservation_base_;
  if (use_guard_regions) {
    base_ += kVirtualMemoryCageGuardRegionSize;
  }

  page_allocator_ = page_allocator;
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = reservation_size;

  cage_page_allocator_ = std::make_unique<base::BoundedPageAllocator>(
      page_allocator_, base_, size_, page_allocator_->AllocatePageSize(),
      base::PageInitializationMode::kAllocatedPagesMustBeZeroInitialized);

  initialized_ = true;
  is_fake_cage_ = false;

  return true;
}

bool V8VirtualMemoryCage::InitializeAsFakeCage(
    v8::PageAllocator* page_allocator, size_t size, size_t size_to_reserve) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK(base::bits::IsPowerOfTwo(size_to_reserve));
  CHECK_GE(size, kVirtualMemoryCageMinimumSize);
  CHECK_LT(size_to_reserve, size);

  // Use a custom random number generator here to ensure that we get uniformly
  // distributed random numbers. We figure out the available address space
  // ourselves, and so are potentially better positioned to determine a good
  // base address for the cage than the embedder-provided GetRandomMmapAddr().
  base::RandomNumberGenerator rng;
  if (FLAG_random_seed != 0) {
    rng.SetSeed(FLAG_random_seed);
  }

  // We try to ensure that base + size is still (mostly) within the process'
  // address space, even though we only reserve a fraction of the memory. For
  // that, we attempt to map the cage into the first half of the usable address
  // space. This keeps the implementation simple and should, In any realistic
  // scenario, leave plenty of space after the cage reservation.
  Address address_space_end = DetermineAddressSpaceLimit();
  Address highest_allowed_address = address_space_end / 2;
  DCHECK(base::bits::IsPowerOfTwo(highest_allowed_address));
  constexpr int kMaxAttempts = 10;
  for (int i = 1; i <= kMaxAttempts; i++) {
    Address hint = rng.NextInt64() % highest_allowed_address;
    hint = RoundDown(hint, kVirtualMemoryCageAlignment);

    reservation_base_ = reinterpret_cast<Address>(page_allocator->AllocatePages(
        reinterpret_cast<void*>(hint), size_to_reserve,
        kVirtualMemoryCageAlignment, PageAllocator::kNoAccess));

    if (!reservation_base_) return false;

    // Take this base if it meets the requirements or if this is the last
    // attempt.
    if (reservation_base_ <= highest_allowed_address || i == kMaxAttempts)
      break;

    // Can't use this base, so free the reservation and try again
    page_allocator->FreePages(reinterpret_cast<void*>(reservation_base_),
                              size_to_reserve);
    reservation_base_ = kNullAddress;
  }
  DCHECK(reservation_base_);

  base_ = reservation_base_;
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = size_to_reserve;
  initialized_ = true;
  is_fake_cage_ = true;
  page_allocator_ = page_allocator;
  cage_page_allocator_ = std::make_unique<FakeBoundedPageAllocator>(
      page_allocator_, base_, size_, reservation_size_);

  return true;
}

void V8VirtualMemoryCage::TearDown() {
  if (initialized_) {
    cage_page_allocator_.reset();
    CHECK(page_allocator_->FreePages(reinterpret_cast<void*>(reservation_base_),
                                     reservation_size_));
    base_ = kNullAddress;
    end_ = kNullAddress;
    size_ = 0;
    reservation_base_ = kNullAddress;
    reservation_size_ = 0;
    initialized_ = false;
    is_fake_cage_ = false;
    page_allocator_ = nullptr;
  }
  disabled_ = false;
}

#endif  // V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

#ifdef V8_VIRTUAL_MEMORY_CAGE
DEFINE_LAZY_LEAKY_OBJECT_GETTER(V8VirtualMemoryCage,
                                GetProcessWideVirtualMemoryCage)
#endif

}  // namespace internal
}  // namespace v8
