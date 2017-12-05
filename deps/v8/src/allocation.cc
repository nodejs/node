// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/allocation.h"

#include <stdlib.h>  // For free, malloc.
#include "src/base/bits.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/utils/random-number-generator.h"
#include "src/flags.h"
#include "src/utils.h"
#include "src/v8.h"

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace v8 {
namespace internal {

namespace {

void* AlignedAllocInternal(size_t size, size_t alignment) {
  void* ptr;
#if V8_OS_WIN
  ptr = _aligned_malloc(size, alignment);
#elif V8_LIBC_BIONIC
  // posix_memalign is not exposed in some Android versions, so we fall back to
  // memalign. See http://code.google.com/p/android/issues/detail?id=35391.
  ptr = memalign(alignment, size);
#else
  if (posix_memalign(&ptr, alignment, size)) ptr = nullptr;
#endif
  return ptr;
}

}  // namespace

void* Malloced::New(size_t size) {
  void* result = malloc(size);
  if (result == nullptr) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
    result = malloc(size);
    if (result == nullptr) {
      V8::FatalProcessOutOfMemory("Malloced operator new");
    }
  }
  return result;
}


void Malloced::Delete(void* p) {
  free(p);
}


char* StrDup(const char* str) {
  int length = StrLength(str);
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}


char* StrNDup(const char* str, int n) {
  int length = StrLength(str);
  if (n < length) length = n;
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}


void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_LE(V8_ALIGNOF(void*), alignment);
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  void* ptr = AlignedAllocInternal(size, alignment);
  if (ptr == nullptr) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
    ptr = AlignedAllocInternal(size, alignment);
    if (ptr == nullptr) {
      V8::FatalProcessOutOfMemory("AlignedAlloc");
    }
  }
  return ptr;
}


void AlignedFree(void *ptr) {
#if V8_OS_WIN
  _aligned_free(ptr);
#elif V8_LIBC_BIONIC
  // Using free is not correct in general, but for V8_LIBC_BIONIC it is.
  free(ptr);
#else
  free(ptr);
#endif
}

VirtualMemory::VirtualMemory() : address_(nullptr), size_(0) {}

VirtualMemory::VirtualMemory(size_t size, void* hint)
    : address_(base::OS::ReserveRegion(size, hint)), size_(size) {
#if defined(LEAK_SANITIZER)
  __lsan_register_root_region(address_, size_);
#endif
}

VirtualMemory::VirtualMemory(size_t size, size_t alignment, void* hint)
    : address_(nullptr), size_(0) {
  address_ = base::OS::ReserveAlignedRegion(size, alignment, hint, &size_);
#if defined(LEAK_SANITIZER)
  __lsan_register_root_region(address_, size_);
#endif
}

VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = base::OS::ReleaseRegion(address(), size());
    DCHECK(result);
    USE(result);
  }
}

void VirtualMemory::Reset() {
  address_ = nullptr;
  size_ = 0;
}

bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  CHECK(InVM(address, size));
  return base::OS::CommitRegion(address, size, is_executable);
}

bool VirtualMemory::Uncommit(void* address, size_t size) {
  CHECK(InVM(address, size));
  return base::OS::UncommitRegion(address, size);
}

bool VirtualMemory::Guard(void* address) {
  CHECK(InVM(address, base::OS::CommitPageSize()));
  base::OS::Guard(address, base::OS::CommitPageSize());
  return true;
}

size_t VirtualMemory::ReleasePartial(void* free_start) {
  DCHECK(IsReserved());
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  const size_t free_size = size_ - (reinterpret_cast<size_t>(free_start) -
                                    reinterpret_cast<size_t>(address_));
  CHECK(InVM(free_start, free_size));
  DCHECK_LT(address_, free_start);
  DCHECK_LT(free_start, reinterpret_cast<void*>(
                            reinterpret_cast<size_t>(address_) + size_));
#if defined(LEAK_SANITIZER)
  __lsan_unregister_root_region(address_, size_);
  __lsan_register_root_region(address_, size_ - free_size);
#endif
  const bool result = base::OS::ReleasePartialRegion(free_start, free_size);
  USE(result);
  DCHECK(result);
  size_ -= free_size;
  return free_size;
}

void VirtualMemory::Release() {
  DCHECK(IsReserved());
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  void* address = address_;
  size_t size = size_;
  CHECK(InVM(address, size));
  Reset();
  bool result = base::OS::ReleaseRegion(address, size);
  USE(result);
  DCHECK(result);
}

void VirtualMemory::TakeControl(VirtualMemory* from) {
  DCHECK(!IsReserved());
  address_ = from->address_;
  size_ = from->size_;
  from->Reset();
}

bool AllocVirtualMemory(size_t size, void* hint, VirtualMemory* result) {
  VirtualMemory first_try(size, hint);
  if (first_try.IsReserved()) {
    result->TakeControl(&first_try);
    return true;
  }

  V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  VirtualMemory second_try(size, hint);
  result->TakeControl(&second_try);
  return result->IsReserved();
}

bool AlignedAllocVirtualMemory(size_t size, size_t alignment, void* hint,
                               VirtualMemory* result) {
  VirtualMemory first_try(size, alignment, hint);
  if (first_try.IsReserved()) {
    result->TakeControl(&first_try);
    return true;
  }

  V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  VirtualMemory second_try(size, alignment, hint);
  result->TakeControl(&second_try);
  return result->IsReserved();
}

namespace {

struct RNGInitializer {
  static void Construct(void* mem) {
    auto rng = new (mem) base::RandomNumberGenerator();
    int64_t random_seed = FLAG_random_seed;
    if (random_seed) {
      rng->SetSeed(random_seed);
    }
  }
};

}  // namespace

static base::LazyInstance<base::RandomNumberGenerator, RNGInitializer>::type
    random_number_generator = LAZY_INSTANCE_INITIALIZER;

void* GetRandomMmapAddr() {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
  // Dynamic tools do not support custom mmap addresses.
  return NULL;
#endif
  uintptr_t raw_addr;
  random_number_generator.Pointer()->NextBytes(&raw_addr, sizeof(raw_addr));
#if V8_OS_POSIX
#if V8_TARGET_ARCH_X64
  // Currently available CPUs have 48 bits of virtual addressing.  Truncate
  // the hint address to 46 bits to give the kernel a fighting chance of
  // fulfilling our placement request.
  raw_addr &= V8_UINT64_C(0x3ffffffff000);
#elif V8_TARGET_ARCH_PPC64
#if V8_OS_AIX
  // AIX: 64 bits of virtual addressing, but we limit address range to:
  //   a) minimize Segment Lookaside Buffer (SLB) misses and
  raw_addr &= V8_UINT64_C(0x3ffff000);
  // Use extra address space to isolate the mmap regions.
  raw_addr += V8_UINT64_C(0x400000000000);
#elif V8_TARGET_BIG_ENDIAN
  // Big-endian Linux: 44 bits of virtual addressing.
  raw_addr &= V8_UINT64_C(0x03fffffff000);
#else
  // Little-endian Linux: 48 bits of virtual addressing.
  raw_addr &= V8_UINT64_C(0x3ffffffff000);
#endif
#elif V8_TARGET_ARCH_S390X
  // Linux on Z uses bits 22-32 for Region Indexing, which translates to 42 bits
  // of virtual addressing.  Truncate to 40 bits to allow kernel chance to
  // fulfill request.
  raw_addr &= V8_UINT64_C(0xfffffff000);
#elif V8_TARGET_ARCH_S390
  // 31 bits of virtual addressing.  Truncate to 29 bits to allow kernel chance
  // to fulfill request.
  raw_addr &= 0x1ffff000;
#else
  raw_addr &= 0x3ffff000;

#ifdef __sun
  // For our Solaris/illumos mmap hint, we pick a random address in the bottom
  // half of the top half of the address space (that is, the third quarter).
  // Because we do not MAP_FIXED, this will be treated only as a hint -- the
  // system will not fail to mmap() because something else happens to already
  // be mapped at our random address. We deliberately set the hint high enough
  // to get well above the system's break (that is, the heap); Solaris and
  // illumos will try the hint and if that fails allocate as if there were
  // no hint at all. The high hint prevents the break from getting hemmed in
  // at low values, ceding half of the address space to the system heap.
  raw_addr += 0x80000000;
#elif V8_OS_AIX
  // The range 0x30000000 - 0xD0000000 is available on AIX;
  // choose the upper range.
  raw_addr += 0x90000000;
#else
  // The range 0x20000000 - 0x60000000 is relatively unpopulated across a
  // variety of ASLR modes (PAE kernel, NX compat mode, etc) and on macos
  // 10.6 and 10.7.
  raw_addr += 0x20000000;
#endif
#endif
#else  // V8_OS_WIN
// The address range used to randomize RWX allocations in OS::Allocate
// Try not to map pages into the default range that windows loads DLLs
// Use a multiple of 64k to prevent committing unused memory.
// Note: This does not guarantee RWX regions will be within the
// range kAllocationRandomAddressMin to kAllocationRandomAddressMax
#ifdef V8_HOST_ARCH_64_BIT
  static const uintptr_t kAllocationRandomAddressMin = 0x0000000080000000;
  static const uintptr_t kAllocationRandomAddressMax = 0x000003FFFFFF0000;
#else
  static const uintptr_t kAllocationRandomAddressMin = 0x04000000;
  static const uintptr_t kAllocationRandomAddressMax = 0x3FFF0000;
#endif
  raw_addr <<= kPageSizeBits;
  raw_addr += kAllocationRandomAddressMin;
  raw_addr &= kAllocationRandomAddressMax;
#endif  // V8_OS_WIN
  return reinterpret_cast<void*>(raw_addr);
}

}  // namespace internal
}  // namespace v8
