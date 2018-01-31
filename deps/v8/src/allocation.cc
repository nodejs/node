// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/allocation.h"

#include <stdlib.h>  // For free, malloc.
#include "src/base/bits.h"
#include "src/base/logging.h"
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

byte* AllocateSystemPage(void* address, size_t* allocated) {
  size_t page_size = base::OS::AllocatePageSize();
  void* result = base::OS::Allocate(address, page_size, page_size,
                                    base::OS::MemoryPermission::kReadWrite);
  if (result != nullptr) *allocated = page_size;
  return static_cast<byte*>(result);
}

VirtualMemory::VirtualMemory() : address_(nullptr), size_(0) {}

VirtualMemory::VirtualMemory(size_t size, void* hint, size_t alignment)
    : address_(nullptr), size_(0) {
  size_t page_size = base::OS::AllocatePageSize();
  size_t alloc_size = RoundUp(size, page_size);
  address_ = base::OS::Allocate(hint, alloc_size, alignment,
                                base::OS::MemoryPermission::kNoAccess);
  if (address_ != nullptr) {
    size_ = alloc_size;
#if defined(LEAK_SANITIZER)
    __lsan_register_root_region(address_, size_);
#endif
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
                                   base::OS::MemoryPermission access) {
  CHECK(InVM(address, size));
  bool result = base::OS::SetPermissions(address, size, access);
  DCHECK(result);
  USE(result);
  return result;
}

size_t VirtualMemory::Release(void* free_start) {
  DCHECK(IsReserved());
  DCHECK(IsAddressAligned(static_cast<Address>(free_start),
                          base::OS::CommitPageSize()));
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
  CHECK(base::OS::Release(free_start, free_size));
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
#if defined(LEAK_SANITIZER)
  __lsan_unregister_root_region(address, size);
#endif
  CHECK(base::OS::Free(address, size));
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
  VirtualMemory first_try(size, hint, alignment);
  if (first_try.IsReserved()) {
    result->TakeControl(&first_try);
    return true;
  }

  V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  VirtualMemory second_try(size, hint, alignment);
  result->TakeControl(&second_try);
  return result->IsReserved();
}

}  // namespace internal
}  // namespace v8
