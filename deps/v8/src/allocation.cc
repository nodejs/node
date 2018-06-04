// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/allocation.h"

#include <stdlib.h>  // For free, malloc.
#include "src/base/bits.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
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

// TODO(bbudge) Simplify this once all embedders implement a page allocator.
struct InitializePageAllocator {
  static void Construct(void* page_allocator_ptr_arg) {
    auto page_allocator_ptr =
        reinterpret_cast<v8::PageAllocator**>(page_allocator_ptr_arg);
    v8::PageAllocator* page_allocator =
        V8::GetCurrentPlatform()->GetPageAllocator();
    if (page_allocator == nullptr) {
      static v8::base::PageAllocator default_allocator;
      page_allocator = &default_allocator;
    }
    *page_allocator_ptr = page_allocator;
  }
};

static base::LazyInstance<v8::PageAllocator*, InitializePageAllocator>::type
    page_allocator = LAZY_INSTANCE_INITIALIZER;

v8::PageAllocator* GetPageAllocator() { return page_allocator.Get(); }

// We will attempt allocation this many times. After each failure, we call
// OnCriticalMemoryPressure to try to free some memory.
const int kAllocationTries = 2;

}  // namespace

void* Malloced::New(size_t size) {
  void* result = AllocWithRetry(size);
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Malloced operator new");
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

void* AllocWithRetry(size_t size) {
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = malloc(size);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size)) break;
  }
  return result;
}

void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_LE(V8_ALIGNOF(void*), alignment);
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = AlignedAllocInternal(size, alignment);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size + alignment)) break;
  }
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "AlignedAlloc");
  }
  return result;
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

size_t AllocatePageSize() { return GetPageAllocator()->AllocatePageSize(); }

size_t CommitPageSize() { return GetPageAllocator()->CommitPageSize(); }

void SetRandomMmapSeed(int64_t seed) {
  GetPageAllocator()->SetRandomMmapSeed(seed);
}

void* GetRandomMmapAddr() { return GetPageAllocator()->GetRandomMmapAddr(); }

void* AllocatePages(void* address, size_t size, size_t alignment,
                    PageAllocator::Permission access) {
  DCHECK_EQ(address, AlignedAddress(address, alignment));
  DCHECK_EQ(0UL, size & (GetPageAllocator()->AllocatePageSize() - 1));
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result =
        GetPageAllocator()->AllocatePages(address, size, alignment, access);
    if (result != nullptr) break;
    size_t request_size = size + alignment - AllocatePageSize();
    if (!OnCriticalMemoryPressure(request_size)) break;
  }
#if defined(LEAK_SANITIZER)
  if (result != nullptr) {
    __lsan_register_root_region(result, size);
  }
#endif
  return result;
}

bool FreePages(void* address, const size_t size) {
  DCHECK_EQ(0UL, size & (GetPageAllocator()->AllocatePageSize() - 1));
  bool result = GetPageAllocator()->FreePages(address, size);
#if defined(LEAK_SANITIZER)
  if (result) {
    __lsan_unregister_root_region(address, size);
  }
#endif
  return result;
}

bool ReleasePages(void* address, size_t size, size_t new_size) {
  DCHECK_LT(new_size, size);
  bool result = GetPageAllocator()->ReleasePages(address, size, new_size);
#if defined(LEAK_SANITIZER)
  if (result) {
    __lsan_unregister_root_region(address, size);
    __lsan_register_root_region(address, new_size);
  }
#endif
  return result;
}

bool SetPermissions(void* address, size_t size,
                    PageAllocator::Permission access) {
  return GetPageAllocator()->SetPermissions(address, size, access);
}

byte* AllocatePage(void* address, size_t* allocated) {
  size_t page_size = AllocatePageSize();
  void* result =
      AllocatePages(address, page_size, page_size, PageAllocator::kReadWrite);
  if (result != nullptr) *allocated = page_size;
  return static_cast<byte*>(result);
}

bool OnCriticalMemoryPressure(size_t length) {
  // TODO(bbudge) Rework retry logic once embedders implement the more
  // informative overload.
  if (!V8::GetCurrentPlatform()->OnCriticalMemoryPressure(length)) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  }
  return true;
}

VirtualMemory::VirtualMemory() : address_(nullptr), size_(0) {}

VirtualMemory::VirtualMemory(size_t size, void* hint, size_t alignment)
    : address_(nullptr), size_(0) {
  size_t page_size = AllocatePageSize();
  size_t alloc_size = RoundUp(size, page_size);
  address_ =
      AllocatePages(hint, alloc_size, alignment, PageAllocator::kNoAccess);
  if (address_ != nullptr) {
    size_ = alloc_size;
  }
}

VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    Free();
  }
}

void VirtualMemory::Reset() {
  address_ = nullptr;
  size_ = 0;
}

bool VirtualMemory::SetPermissions(void* address, size_t size,
                                   PageAllocator::Permission access) {
  CHECK(InVM(address, size));
  bool result = v8::internal::SetPermissions(address, size, access);
  DCHECK(result);
  USE(result);
  return result;
}

size_t VirtualMemory::Release(void* free_start) {
  DCHECK(IsReserved());
  DCHECK(IsAddressAligned(static_cast<Address>(free_start), CommitPageSize()));
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  const size_t free_size = size_ - (reinterpret_cast<size_t>(free_start) -
                                    reinterpret_cast<size_t>(address_));
  CHECK(InVM(free_start, free_size));
  DCHECK_LT(address_, free_start);
  DCHECK_LT(free_start, reinterpret_cast<void*>(
                            reinterpret_cast<size_t>(address_) + size_));
  CHECK(ReleasePages(address_, size_, size_ - free_size));
  size_ -= free_size;
  return free_size;
}

void VirtualMemory::Free() {
  DCHECK(IsReserved());
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  void* address = address_;
  size_t size = size_;
  CHECK(InVM(address, size));
  Reset();
  // FreePages expects size to be aligned to allocation granularity. Trimming
  // may leave size at only commit granularity. Align it here.
  CHECK(FreePages(address, RoundUp(size, AllocatePageSize())));
}

void VirtualMemory::TakeControl(VirtualMemory* from) {
  DCHECK(!IsReserved());
  address_ = from->address_;
  size_ = from->size_;
  from->Reset();
}

bool AllocVirtualMemory(size_t size, void* hint, VirtualMemory* result) {
  VirtualMemory vm(size, hint);
  if (vm.IsReserved()) {
    result->TakeControl(&vm);
    return true;
  }
  return false;
}

bool AlignedAllocVirtualMemory(size_t size, size_t alignment, void* hint,
                               VirtualMemory* result) {
  VirtualMemory vm(size, hint, alignment);
  if (vm.IsReserved()) {
    result->TakeControl(&vm);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8
