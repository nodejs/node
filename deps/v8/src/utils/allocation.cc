// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/allocation.h"

#include <stdlib.h>  // For free, malloc.
#include "src/base/bits.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/sanitizer/lsan-page-allocator.h"
#include "src/utils/memcopy.h"
#include "src/utils/vector.h"

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
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

class PageAllocatorInitializer {
 public:
  PageAllocatorInitializer() {
    page_allocator_ = V8::GetCurrentPlatform()->GetPageAllocator();
    if (page_allocator_ == nullptr) {
      static base::LeakyObject<base::PageAllocator> default_page_allocator;
      page_allocator_ = default_page_allocator.get();
    }
#if defined(LEAK_SANITIZER)
    static base::LeakyObject<base::LsanPageAllocator> lsan_allocator(
        page_allocator_);
    page_allocator_ = lsan_allocator.get();
#endif
  }

  PageAllocator* page_allocator() const { return page_allocator_; }

  void SetPageAllocatorForTesting(PageAllocator* allocator) {
    page_allocator_ = allocator;
  }

 private:
  PageAllocator* page_allocator_;
};

DEFINE_LAZY_LEAKY_OBJECT_GETTER(PageAllocatorInitializer,
                                GetPageTableInitializer)

// We will attempt allocation this many times. After each failure, we call
// OnCriticalMemoryPressure to try to free some memory.
const int kAllocationTries = 2;

}  // namespace

v8::PageAllocator* GetPlatformPageAllocator() {
  DCHECK_NOT_NULL(GetPageTableInitializer()->page_allocator());
  return GetPageTableInitializer()->page_allocator();
}

v8::PageAllocator* SetPlatformPageAllocatorForTesting(
    v8::PageAllocator* new_page_allocator) {
  v8::PageAllocator* old_page_allocator = GetPlatformPageAllocator();
  GetPageTableInitializer()->SetPageAllocatorForTesting(new_page_allocator);
  return old_page_allocator;
}

void* Malloced::operator new(size_t size) {
  void* result = AllocWithRetry(size);
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Malloced operator new");
  }
  return result;
}

void Malloced::operator delete(void* p) { free(p); }

char* StrDup(const char* str) {
  size_t length = strlen(str);
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}

char* StrNDup(const char* str, size_t n) {
  size_t length = strlen(str);
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
  DCHECK_LE(alignof(void*), alignment);
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

void AlignedFree(void* ptr) {
#if V8_OS_WIN
  _aligned_free(ptr);
#elif V8_LIBC_BIONIC
  // Using free is not correct in general, but for V8_LIBC_BIONIC it is.
  free(ptr);
#else
  free(ptr);
#endif
}

size_t AllocatePageSize() {
  return GetPlatformPageAllocator()->AllocatePageSize();
}

size_t CommitPageSize() { return GetPlatformPageAllocator()->CommitPageSize(); }

void SetRandomMmapSeed(int64_t seed) {
  GetPlatformPageAllocator()->SetRandomMmapSeed(seed);
}

void* GetRandomMmapAddr() {
  return GetPlatformPageAllocator()->GetRandomMmapAddr();
}

void* AllocatePages(v8::PageAllocator* page_allocator, void* hint, size_t size,
                    size_t alignment, PageAllocator::Permission access) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK_EQ(hint, AlignedAddress(hint, alignment));
  DCHECK(IsAligned(size, page_allocator->AllocatePageSize()));
  if (FLAG_randomize_all_allocations) {
    hint = page_allocator->GetRandomMmapAddr();
  }
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = page_allocator->AllocatePages(hint, size, alignment, access);
    if (result != nullptr) break;
    size_t request_size = size + alignment - page_allocator->AllocatePageSize();
    if (!OnCriticalMemoryPressure(request_size)) break;
  }
  return result;
}

bool FreePages(v8::PageAllocator* page_allocator, void* address,
               const size_t size) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK(IsAligned(size, page_allocator->AllocatePageSize()));
  return page_allocator->FreePages(address, size);
}

bool ReleasePages(v8::PageAllocator* page_allocator, void* address, size_t size,
                  size_t new_size) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK_LT(new_size, size);
  DCHECK(IsAligned(new_size, page_allocator->CommitPageSize()));
  return page_allocator->ReleasePages(address, size, new_size);
}

bool SetPermissions(v8::PageAllocator* page_allocator, void* address,
                    size_t size, PageAllocator::Permission access) {
  DCHECK_NOT_NULL(page_allocator);
  return page_allocator->SetPermissions(address, size, access);
}

bool OnCriticalMemoryPressure(size_t length) {
  // TODO(bbudge) Rework retry logic once embedders implement the more
  // informative overload.
  if (!V8::GetCurrentPlatform()->OnCriticalMemoryPressure(length)) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  }
  return true;
}

VirtualMemory::VirtualMemory() = default;

VirtualMemory::VirtualMemory(v8::PageAllocator* page_allocator, size_t size,
                             void* hint, size_t alignment, JitPermission jit)
    : page_allocator_(page_allocator) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK(IsAligned(size, page_allocator_->CommitPageSize()));
  size_t page_size = page_allocator_->AllocatePageSize();
  alignment = RoundUp(alignment, page_size);
  PageAllocator::Permission permissions =
      jit == kMapAsJittable ? PageAllocator::kNoAccessWillJitLater
                            : PageAllocator::kNoAccess;
  Address address = reinterpret_cast<Address>(AllocatePages(
      page_allocator_, hint, RoundUp(size, page_size), alignment, permissions));
  if (address != kNullAddress) {
    DCHECK(IsAligned(address, alignment));
    region_ = base::AddressRegion(address, size);
  }
}

VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    Free();
  }
}

void VirtualMemory::Reset() {
  page_allocator_ = nullptr;
  region_ = base::AddressRegion();
}

bool VirtualMemory::SetPermissions(Address address, size_t size,
                                   PageAllocator::Permission access) {
  CHECK(InVM(address, size));
  bool result =
      v8::internal::SetPermissions(page_allocator_, address, size, access);
  DCHECK(result);
  return result;
}

size_t VirtualMemory::Release(Address free_start) {
  DCHECK(IsReserved());
  DCHECK(IsAligned(free_start, page_allocator_->CommitPageSize()));
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.

  const size_t old_size = region_.size();
  const size_t free_size = old_size - (free_start - region_.begin());
  CHECK(InVM(free_start, free_size));
  region_.set_size(old_size - free_size);
  CHECK(ReleasePages(page_allocator_, reinterpret_cast<void*>(region_.begin()),
                     old_size, region_.size()));
  return free_size;
}

void VirtualMemory::Free() {
  DCHECK(IsReserved());
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  v8::PageAllocator* page_allocator = page_allocator_;
  base::AddressRegion region = region_;
  Reset();
  // FreePages expects size to be aligned to allocation granularity however
  // ReleasePages may leave size at only commit granularity. Align it here.
  CHECK(FreePages(page_allocator, reinterpret_cast<void*>(region.begin()),
                  RoundUp(region.size(), page_allocator->AllocatePageSize())));
}

void VirtualMemory::FreeReadOnly() {
  DCHECK(IsReserved());
  // The only difference to Free is that it doesn't call Reset which would write
  // to the VirtualMemory object.
  v8::PageAllocator* page_allocator = page_allocator_;
  base::AddressRegion region = region_;

  // FreePages expects size to be aligned to allocation granularity however
  // ReleasePages may leave size at only commit granularity. Align it here.
  CHECK(FreePages(page_allocator, reinterpret_cast<void*>(region.begin()),
                  RoundUp(region.size(), page_allocator->AllocatePageSize())));
}

}  // namespace internal
}  // namespace v8
