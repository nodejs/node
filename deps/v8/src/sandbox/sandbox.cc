// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/sandbox.h"

#include <memory>

#include "include/v8-internal.h"
#include "include/v8-platform.h"
#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/cpu/cpu.h"
#include "src/base/emulated-virtual-address-subspace.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/sys-info.h"
#include "src/base/utils/random-number-generator.h"
#include "src/base/virtual-address-space-page-allocator.h"
#include "src/base/virtual-address-space.h"
#include "src/flags/flags.h"
#include "src/objects/js-objects.h"
#include "src/sandbox/hardware-support.h"
#include "src/sandbox/sandboxed-pointer.h"
#include "src/sandbox/testing.h"
#include "src/utils/allocation.h"

#if V8_OS_LINUX
#include <sys/mman.h>
#endif

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

namespace {

// Exclude a large virtual reservation from core dumps. Without this, the
// kernel walks the multi-TB sandbox reservation when writing a coredump,
// which can take minutes even though almost none of it is resident. See
// core(5) on MADV_DONTDUMP. No-op on non-Linux platforms; other OSes either
// already skip PROT_NONE mappings or don't expose an equivalent knob.
void ExcludeReservationFromCoreDump(Address base, size_t size) {
#if V8_OS_LINUX
  // Best-effort: ignore failures (e.g. old kernels without MADV_DONTDUMP).
  madvise(reinterpret_cast<void*>(base), size, MADV_DONTDUMP);
#endif
}

// Default in-sandbox allocator that is shared by all ArraybufferAllocator
// default implementations and the per-IsolateGroup in-sandbox allocator.
//
// This allocator is not optimized for performance! See
// `v8::V8::SetInSandboxAllocator()` to set a different allocator.
//
// The allocator only lazily initializes itself on first operation as it is
// assumed that it is replaced with a different allocator for production use.
class DefaultInSandboxAllocator final : public v8::Allocator {
 public:
  explicit DefaultInSandboxAllocator(Sandbox* sandbox);
  ~DefaultInSandboxAllocator();

  DefaultInSandboxAllocator(const DefaultInSandboxAllocator&) = delete;
  DefaultInSandboxAllocator& operator=(const DefaultInSandboxAllocator&) =
      delete;

  void* Allocate(size_t length) override;
  void* AllocateUninitialized(size_t length) override;
  void* AllocateUninitializedOrCrash(size_t length) override;
  void Free(void* data) override;

 private:
  // Use a region allocator with a "page size" of 128 bytes as a reasonable
  // compromise between the number of regions it has to manage and the amount
  // of memory wasted due to rounding allocation sizes up to the page size.
  static constexpr size_t kAllocationGranularity = 128;
  // The backing memory's accessible region is grown in chunks of this size.
  static constexpr size_t kChunkSize = 1 * MB;

  V8_NOINLINE V8_PRESERVE_MOST void LazyInitialize();

  bool is_initialized() const { return static_cast<bool>(region_alloc_); }

  std::unique_ptr<base::RegionAllocator> region_alloc_;
  size_t end_of_accessible_region_ = 0;
  Sandbox* sandbox_ = nullptr;
  base::Mutex mutex_;
};

DefaultInSandboxAllocator::DefaultInSandboxAllocator(Sandbox* sandbox)
    : sandbox_(sandbox) {}

void DefaultInSandboxAllocator::LazyInitialize() {
  CHECK(sandbox_->is_initialized());
  constexpr size_t max_backing_memory_size = 8ULL * GB;
  constexpr size_t min_backing_memory_size = 1ULL * GB;
  size_t backing_memory_size = max_backing_memory_size;
  Address backing_memory_base = 0;
  while (!backing_memory_base &&
         backing_memory_size >= min_backing_memory_size) {
    backing_memory_base = sandbox_->address_space()->AllocatePages(
        VirtualAddressSpace::kNoHint, backing_memory_size, kChunkSize,
        PagePermissions::kNoAccess);
    if (!backing_memory_base) {
      backing_memory_size /= 2;
    }
  }
  if (!backing_memory_base) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Could not reserve backing memory for ArrayBufferAllocators");
  }
  DCHECK(IsAligned(backing_memory_base, kChunkSize));

  region_alloc_ = std::make_unique<base::RegionAllocator>(
      backing_memory_base, backing_memory_size, kAllocationGranularity);
  end_of_accessible_region_ = region_alloc_->begin();

  // Install an on-merge callback to discard or decommit unused pages.
  region_alloc_->set_on_merge_callback([this](Address start, size_t size) {
    mutex_.AssertHeld();
    Address end = start + size;
    if (end == region_alloc_->end() &&
        start <= end_of_accessible_region_ - kChunkSize) {
      // Can shrink the accessible region.
      Address new_end_of_accessible_region = RoundUp(start, kChunkSize);
      size_t size_to_decommit =
          end_of_accessible_region_ - new_end_of_accessible_region;
      if (!sandbox_->address_space()->DecommitPages(
              new_end_of_accessible_region, size_to_decommit)) {
        V8::FatalProcessOutOfMemory(nullptr, "SandboxedArrayBufferAllocator()");
      }
      end_of_accessible_region_ = new_end_of_accessible_region;
    } else if (size >= 2 * kChunkSize) {
      // Can discard pages. The pages stay accessible, so the size of the
      // accessible region doesn't change.
      Address chunk_start = RoundUp(start, kChunkSize);
      Address chunk_end = RoundDown(start + size, kChunkSize);
      if (!sandbox_->address_space()->DiscardSystemPages(
              chunk_start, chunk_end - chunk_start)) {
        V8::FatalProcessOutOfMemory(nullptr, "SandboxedArrayBufferAllocator()");
      }
    }
  });
}

DefaultInSandboxAllocator::~DefaultInSandboxAllocator() {
  // The sandbox may already have been torn down, in which case there's no
  // need to free any memory.
  if (is_initialized() && sandbox_->is_initialized()) {
    sandbox_->address_space()->FreePages(region_alloc_->begin(),
                                         region_alloc_->size());
  }
  sandbox_ = nullptr;
  region_alloc_.reset(nullptr);
}

void* DefaultInSandboxAllocator::Allocate(size_t length) {
  base::MutexGuard guard(&mutex_);

  if (!is_initialized()) [[unlikely]] {
    LazyInitialize();
  }

  length = RoundUp(length, kAllocationGranularity);
  Address region = region_alloc_->AllocateRegion(length);
  if (region == base::RegionAllocator::kAllocationFailure) return nullptr;

  // Check if the memory is inside the accessible region. If not, grow it.
  Address end = region + length;
  size_t length_to_memset = length;
  if (end > end_of_accessible_region_) {
    Address new_end_of_accessible_region = RoundUp(end, kChunkSize);
    size_t size = new_end_of_accessible_region - end_of_accessible_region_;
    if (!sandbox_->address_space()->SetPagePermissions(
            end_of_accessible_region_, size, PagePermissions::kReadWrite)) {
      if (!region_alloc_->FreeRegion(region)) {
        V8::FatalProcessOutOfMemory(
            nullptr, "SandboxedArrayBufferAllocator::Allocate()");
      }
      return nullptr;
    }

    // The pages that were inaccessible are guaranteed to be zeroed, so only
    // memset until the previous end of the accessible region.
    length_to_memset = end_of_accessible_region_ - region;
    end_of_accessible_region_ = new_end_of_accessible_region;
  }

  void* mem = reinterpret_cast<void*>(region);
  memset(mem, 0, length_to_memset);
  return mem;
}

void* DefaultInSandboxAllocator::AllocateUninitialized(size_t length) {
  return Allocate(length);
}

void* DefaultInSandboxAllocator::AllocateUninitializedOrCrash(size_t length) {
  base::MutexGuard guard(&mutex_);

  if (!is_initialized()) [[unlikely]] {
    LazyInitialize();
  }

  length = RoundUp(length, kAllocationGranularity);
  Address region = region_alloc_->AllocateRegion(length);
  if (region == base::RegionAllocator::kAllocationFailure) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "SandboxedArrayBufferAllocator::AllocateUninitializedOrCrash()");
  }

  // Check if the memory is inside the accessible region. If not, grow it.
  Address end = region + length;
  if (end > end_of_accessible_region_) {
    Address new_end_of_accessible_region = RoundUp(end, kChunkSize);
    size_t size = new_end_of_accessible_region - end_of_accessible_region_;
    if (!sandbox_->address_space()->SetPagePermissions(
            end_of_accessible_region_, size, PagePermissions::kReadWrite)) {
      V8::FatalProcessOutOfMemory(
          nullptr,
          "SandboxedArrayBufferAllocator::AllocateUninitializedOrCrash()");
    }
    end_of_accessible_region_ = new_end_of_accessible_region;
  }

  return reinterpret_cast<void*>(region);
}

void DefaultInSandboxAllocator::Free(void* data) {
  if (!data) return;

  base::MutexGuard guard(&mutex_);
  CHECK(is_initialized());
  region_alloc_->FreeRegion(reinterpret_cast<Address>(data));
}

}  // namespace

bool Sandbox::smi_address_range_reserved_ = false;

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
thread_local Sandbox* Sandbox::current_ = nullptr;
// static
Sandbox* Sandbox::current_non_inlined() { return current_; }
// static
void Sandbox::set_current_non_inlined(Sandbox* sandbox) { current_ = sandbox; }
#endif  // V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES

Sandbox* Sandbox::default_sandbox_ = nullptr;

// Best-effort function to determine the approximate size of the virtual
// address space that can be addressed by this process. Used to determine
// appropriate sandbox size and placement.
// The value returned by this function will always be a power of two.
static Address DetermineAddressSpaceLimit() {
#ifndef V8_TARGET_ARCH_64_BIT
#error Unsupported target architecture.
#endif

  // Assume 48 bits by default, which seems to be the most common configuration.
  constexpr unsigned kDefaultVirtualAddressBits = 48;
  // 36 bits should realistically be the lowest value we could ever see.
  constexpr unsigned kMinVirtualAddressBits = 36;
  constexpr unsigned kMaxVirtualAddressBits = 64;

  unsigned hardware_virtual_address_bits = kDefaultVirtualAddressBits;
#if defined(V8_TARGET_ARCH_X64)
  base::CPU cpu;
  if (cpu.exposes_num_virtual_address_bits()) {
    hardware_virtual_address_bits = cpu.num_virtual_address_bits();
  }
#endif  // V8_TARGET_ARCH_X64

#if defined(V8_TARGET_ARCH_ARM64) && defined(V8_TARGET_OS_ANDROID)
  // On Arm64 Android assume a 40-bit virtual address space (39 bits for
  // userspace and kernel each) as that appears to be the most common
  // configuration and there seems to be no easy way to retrieve the actual
  // number of virtual address bits from the CPU in userspace.
  hardware_virtual_address_bits = 40;
#elif defined(V8_TARGET_OS_IOS)
  // On iOS, we only get 64 GB of userspace virtual address space even with the
  // "jumbo" extended virtual addressing entitlement, so assume a 37-bit virtual
  // address space (36 bits for userspace and kernel each). Ensure that this
  // results in `hardware_virtual_address_bits` being at least the minimum (36)
  // otherwise we will override it with the default value (48) incorrectly.
  hardware_virtual_address_bits = 37;
#elif defined(V8_HOST_ARCH_RISCV64)
  // RISC-V supports multiple virtual addressing modes (Sv39, Sv48, Sv57).
  // Detect the active mode at runtime via /proc/cpuinfo to avoid assuming
  // 48-bit VA on Sv39 hardware, where userspace only has 256GB.
  // Uses V8_HOST_ARCH since /proc/cpuinfo reflects the host CPU.
  {
    base::CPU cpu;
    switch (cpu.riscv_mmu()) {
      case base::CPU::RV_MMU_MODE::kRiscvSV39:
        hardware_virtual_address_bits = 39;
        break;
      case base::CPU::RV_MMU_MODE::kRiscvSV48:
        hardware_virtual_address_bits = 48;
        break;
      case base::CPU::RV_MMU_MODE::kRiscvSV57:
        hardware_virtual_address_bits = 57;
        break;
    }
  }
#elif defined(V8_TARGET_ARCH_LOONG64)
  // Some hardwares like 2k3000 only have 40-bit virtual address space, 39 bits
  // userspace and kernel each.
  hardware_virtual_address_bits = 40;
#endif

  // Assume virtual address space is split 50/50 between userspace and kernel.
  hardware_virtual_address_bits -= 1;

  // Check if there is a software-imposed limits on the size of the address
  // space. For example, older Windows versions limit the address space to 8TB:
  // https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases).
  Address software_limit = base::SysInfo::AddressSpaceEnd();
  // Compute the next power of two that is larger or equal to the limit.
  unsigned software_virtual_address_bits =
      64 - base::bits::CountLeadingZeros(software_limit - 1);

  // The available address space is the smaller of the two limits.
  unsigned virtual_address_bits =
      std::min(hardware_virtual_address_bits, software_virtual_address_bits);

  // Guard against nonsensical values.
  if (virtual_address_bits < kMinVirtualAddressBits ||
      virtual_address_bits > kMaxVirtualAddressBits) {
    virtual_address_bits = kDefaultVirtualAddressBits;
  }

  return 1ULL << virtual_address_bits;
}

void Sandbox::Initialize(v8::Platform* platform, v8::VirtualAddressSpace* vas) {
  // Take the size of the virtual address space into account when determining
  // the size of the address space reservation backing the sandbox. For
  // example, if we only have a 40-bit address space, split evenly between
  // userspace and kernel, then userspace can only address 512GB and so we use
  // a quarter of that, 128GB, as maximum reservation size.
  Address address_space_limit = DetermineAddressSpaceLimit();
  // Note: this is technically the maximum reservation size excluding the guard
  // regions (which are not created for partially-reserved sandboxes).
  size_t max_reservation_size = address_space_limit / 4;

  // In any case, the sandbox should be smaller than our address space since we
  // otherwise wouldn't always be able to allocate objects inside of it.
  CHECK_LT(kSandboxSize, address_space_limit);

  if (!vas->CanAllocateSubspaces()) {
    // If we cannot create virtual memory subspaces, we fall back to creating a
    // partially reserved sandbox. This will happen for example on older
    // Windows versions (before Windows 10) where the necessary memory
    // management APIs, in particular, VirtualAlloc2, are not available.
    // Since reserving virtual memory is an expensive operation on Windows
    // before version 8.1 (reserving 1TB of address space will increase private
    // memory usage by around 2GB), we only reserve the minimal amount of
    // address space here. This way, we don't incur the cost of reserving
    // virtual memory, but also don't get the desired security properties as
    // unrelated mappings may end up inside the sandbox.
    max_reservation_size = kSandboxMinimumReservationSize;
  }

#if defined(V8_TARGET_OS_IOS)
  // If we don't override this, we will attempt to reserve 16 GB (sandbox size)
  // + 72 GB (guard region size) + 260 GB (trailing guard region size) which
  // will fail since iOS only provides ~63 GB of virtual address space of which
  // only ~51 GB can be mapped in practice. Also, the code assumes that the
  // partially reserved sandbox mode has a reservation size strictly less than
  // the sandbox size which is 16 GB for iOS - using `address_space_limit / 4`
  // gives us 16 GB which won't work so use the minimum size i.e. 8 GB instead.
  max_reservation_size = kSandboxMinimumReservationSize;
#endif

  // If the maximum reservation size is less than the size of the sandbox, we
  // can only create a partially-reserved sandbox.
  bool success;
  size_t reservation_size = std::min(kSandboxSize, max_reservation_size);
  DCHECK(base::bits::IsPowerOfTwo(reservation_size));
  if (reservation_size < kSandboxSize) {
    DCHECK_GE(max_reservation_size, kSandboxMinimumReservationSize);
    success = InitializeAsPartiallyReservedSandbox(platform, vas, kSandboxSize,
                                                   reservation_size);
  } else {
    DCHECK_EQ(kSandboxSize, reservation_size);
    constexpr bool use_guard_regions = true;
    success = Initialize(platform, vas, kSandboxSize, use_guard_regions);
  }

  // Fall back to creating a (smaller) partially reserved sandbox.
  while (!success && reservation_size > kSandboxMinimumReservationSize) {
    static_assert(kFallbackToPartiallyReservedSandboxAllowed);
    reservation_size /= 2;
    DCHECK_GE(reservation_size, kSandboxMinimumReservationSize);
    success = InitializeAsPartiallyReservedSandbox(platform, vas, kSandboxSize,
                                                   reservation_size);
  }

  if (!success) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "Failed to reserve the virtual address space for the V8 sandbox");
  }

  if (v8_flags.sandbox_prohibit_insecure_mode) {
    if (is_partially_reserved() || !smi_address_range_is_inaccessible()) {
      V8::FatalProcessOutOfMemory(
          nullptr,
          "Failed to initialize sandbox in a secure mode which is required by "
          "--sandbox_prohibit_insecure_mode.");
    }
  }

#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
  if (trap_handler::RegisterV8Sandbox(base(), size())) {
    trap_handler_initialized_ = true;
  } else {
    V8::FatalProcessOutOfMemory(
        nullptr, "Failed to allocate sandbox record for trap handling.");
  }
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  if (SandboxHardwareSupport::IsActive()) {
    CHECK_EQ(address_space_->ActiveMemoryProtectionKey(),
             SandboxHardwareSupport::SandboxPkey());
  }
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  DCHECK(initialized_);
}

bool Sandbox::Initialize(v8::Platform* platform, v8::VirtualAddressSpace* vas,
                         size_t size, bool use_guard_regions) {
  CHECK(!initialized_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK(vas->CanAllocateSubspaces());

  size_t reservation_size = size;
  size_t true_reservation_size = size;

  if (use_guard_regions) {
    reservation_size += 2 * kSandboxGuardRegionSize;
    true_reservation_size =
        reservation_size + kAdditionalTrailingGuardRegionSize;
  }

  Address hint = RoundDown(vas->RandomPageAddress(), kSandboxAlignment);

  // There should be no executable pages mapped inside the sandbox since
  // those could be corrupted by an attacker and therefore pose a security
  // risk. Furthermore, allowing executable mappings in the sandbox requires
  // MAP_JIT on macOS, which causes fork() to become excessively slow
  // (multiple seconds or even minutes for a 1TB sandbox on macOS 12.X), in
  // turn causing tests to time out. As such, the maximum page permission
  // inside the sandbox should be read + write.
  const PagePermissions kSandboxMaxPermissions = PagePermissions::kReadWrite;

  // When sandbox hardware support is available and active, the sandbox address
  // space uses a dedicated memory protection key.
  std::optional<VirtualAddressSpace::MemoryProtectionKeyId> sandbox_pkey =
      std::nullopt;
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  if (SandboxHardwareSupport::IsActive()) {
    CHECK_NE(SandboxHardwareSupport::SandboxPkey(),
             base::MemoryProtectionKey::kNoMemoryProtectionKey);
    sandbox_pkey = SandboxHardwareSupport::SandboxPkey();
  }
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  address_space_ =
      vas->AllocateSubspace(hint, true_reservation_size, kSandboxAlignment,
                            kSandboxMaxPermissions, sandbox_pkey);
  if (!address_space_) return false;
  address_space_->SetName(kSandboxAddressSpaceName);
  ExcludeReservationFromCoreDump(address_space_->base(),
                                 address_space_->size());

  reservation_base_ = address_space_->base();
  base_ = reservation_base_ + (use_guard_regions ? kSandboxGuardRegionSize : 0);
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = reservation_size;
  sandbox_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          address_space_.get());
  in_sandbox_allocator_ = std::make_shared<DefaultInSandboxAllocator>(this);

  if (use_guard_regions) {
    Address front = reservation_base_;
    Address back = end_;
    // These must succeed since nothing was allocated in the subspace yet.
    CHECK(address_space_->AllocateGuardRegion(front, kSandboxGuardRegionSize));
    CHECK(address_space_->AllocateGuardRegion(
        back, kSandboxGuardRegionSize + kAdditionalTrailingGuardRegionSize));
  }

  // Also try to reserve the first 4GB of the process' address space. This
  // mitigates Smi<->HeapObject confusion bugs in which we end up treating a
  // Smi value as a pointer.
  if (!smi_address_range_reserved_) {
    // Make the guard region extend a little past the first 4GB to also catch
    // accesses to in-object properties which are bounded by the JSObject
    // instance size.
    static_assert(kSmiAddressRangePadding > JSObject::kMaxInstanceSize);
    constexpr Address kRangeEnd = kSmiAddressRange + kSmiAddressRangePadding;
    const size_t zero_segment_size = platform->GetZeroSegmentSize();
    if (zero_segment_size >= kRangeEnd) {
      smi_address_range_reserved_ = true;
    } else {
      const size_t step = address_space_->allocation_granularity();
      const Address aligned_end = RoundUp(kRangeEnd, step);
      for (Address start = 0; start <= 1 * MB; start += step) {
        if (vas->AllocateGuardRegion(start, aligned_end - start)) {
          smi_address_range_reserved_ = true;
          break;
        }
      }
    }
  }

  initialized_ = true;

  FinishInitialization();

  DCHECK(!is_partially_reserved());
  return true;
}

bool Sandbox::InitializeAsPartiallyReservedSandbox(v8::Platform* platform,
                                                   v8::VirtualAddressSpace* vas,
                                                   size_t size,
                                                   size_t size_to_reserve) {
  CHECK(!initialized_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK(base::bits::IsPowerOfTwo(size_to_reserve));
  CHECK_LT(size_to_reserve, size);

  // Use a custom random number generator here to ensure that we get uniformly
  // distributed random numbers. We figure out the available address space
  // ourselves, and so are potentially better positioned to determine a good
  // base address for the sandbox than the embedder.
  base::RandomNumberGenerator rng;
  if (v8_flags.random_seed != 0) {
    rng.SetSeed(v8_flags.random_seed);
  }

  // We try to ensure that base + size is still (mostly) within the process'
  // address space, even though we only reserve a fraction of the memory. For
  // that, we attempt to map the sandbox into the first half of the usable
  // address space. This keeps the implementation simple and should, In any
  // realistic scenario, leave plenty of space after the actual reservation.
  Address address_space_end = DetermineAddressSpaceLimit();
  Address highest_allowed_address = address_space_end / 2;
  DCHECK(base::bits::IsPowerOfTwo(highest_allowed_address));
  constexpr int kMaxAttempts = 10;
  for (int i = 1; i <= kMaxAttempts; i++) {
    Address hint = rng.NextInt64() % highest_allowed_address;
    hint = RoundDown(hint, kSandboxAlignment);

    reservation_base_ = vas->AllocatePages(
        hint, size_to_reserve, kSandboxAlignment, PagePermissions::kNoAccess);

    if (!reservation_base_) return false;

    // Take this base if it meets the requirements or if this is the last
    // attempt.
    if (reservation_base_ <= highest_allowed_address || i == kMaxAttempts) {
      break;
    }

    // Can't use this base, so free the reservation and try again
    vas->FreePages(reservation_base_, size_to_reserve);
    reservation_base_ = kNullAddress;
  }
  DCHECK(reservation_base_);

  base_ = reservation_base_;
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = size_to_reserve;
  initialized_ = true;
  ExcludeReservationFromCoreDump(reservation_base_, reservation_size_);
  address_space_ = std::make_unique<base::EmulatedVirtualAddressSubspace>(
      vas, reservation_base_, reservation_size_, size_);
  sandbox_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          address_space_.get());
  in_sandbox_allocator_ = std::make_shared<DefaultInSandboxAllocator>(this);

  FinishInitialization();

  DCHECK(is_partially_reserved());
  return true;
}

void Sandbox::FinishInitialization() {
#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  // We do this even for the case of partially-reserved sandbox because, while
  // being an unsafe setup, tests and fuzzers shouldn't report crashes in this
  // region.
  SandboxTesting::RegisterSafeMemoryRegion(
      address_space_->base(), address_space_->size(),
      SandboxTesting::kReadAndWriteAccessIsSafe);
#endif

  // Reserve the last page in the sandbox. This way, we can place inaccessible
  // "objects" (e.g. the empty backing store buffer) there that are guaranteed
  // to cause a fault on any accidental access.
  // Further, this also prevents the accidental construction of invalid
  // SandboxedPointers: if an ArrayBuffer is placed right at the end of the
  // sandbox, an ArrayBufferView could be constructed with byteLength=0 and
  // offset=buffer.byteLength, which would lead to a pointer that points just
  // outside of the sandbox.
  size_t allocation_granularity = address_space_->allocation_granularity();
  bool success = address_space_->AllocateGuardRegion(
      end_ - allocation_granularity, allocation_granularity);
  // If the sandbox is partially-reserved, this operation may fail, for example
  // if the last page is outside of the mappable address space of the process.
  CHECK(success || is_partially_reserved());

  InitializeConstants();
}

void Sandbox::InitializeConstants() {
  // Place the empty backing store buffer at the end of the sandbox, so that any
  // accidental access to it will most likely hit a guard page.
  constants_.set_empty_backing_store_buffer(end_ - 1);
}

void Sandbox::TearDown() {
  if (initialized_) {
#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
    if (trap_handler_initialized_) {
      trap_handler::UnregisterV8Sandbox(base(), size());
      trap_handler_initialized_ = false;
    }
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
    SandboxTesting::UnregisterSafeMemoryRegion(address_space_->base());
#endif

    // This could be an allocator passed over the API. Destroy it with
    // everything else still being intact.
    in_sandbox_allocator_.reset();
    // This destroys the sub space and frees the underlying reservation.
    address_space_.reset();
    sandbox_page_allocator_.reset();
    base_ = kNullAddress;
    end_ = kNullAddress;
    size_ = 0;
    reservation_base_ = kNullAddress;
    reservation_size_ = 0;
    initialized_ = false;
    constants_.Reset();
  }
}

// static
void Sandbox::InitializeDefaultOncePerProcess(v8::Platform* platform,
                                              v8::VirtualAddressSpace* vas) {
  static base::LeakyObject<Sandbox> default_sandbox;
  default_sandbox_ = default_sandbox.get();

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  set_current(default_sandbox_);
#endif
  default_sandbox_->Initialize(platform, vas);
}

// static
void Sandbox::TearDownDefault() {
  GetDefault()->TearDown();

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  set_current(nullptr);
#endif
}

// static
Sandbox* Sandbox::New(v8::Platform* platform, v8::VirtualAddressSpace* vas) {
  if (!COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL) {
    FATAL(
        "Creation of new sandboxes requires enabling "
        "multiple pointer compression cages at build-time");
  }
  Sandbox* sandbox = new Sandbox;
  sandbox->Initialize(platform, vas);
  CHECK(!v8_flags.sandbox_testing && !v8_flags.sandbox_fuzzing);
  return sandbox;
}

void Sandbox::set_in_sandbox_allocator(std::shared_ptr<Allocator> allocator) {
  in_sandbox_allocator_ = std::move(allocator);
}

#endif  // V8_ENABLE_SANDBOX

}  // namespace internal
}  // namespace v8
