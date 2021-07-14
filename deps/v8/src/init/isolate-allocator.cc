// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/isolate-allocator.h"

#include "src/base/bounded-page-allocator.h"
#include "src/common/ptr-compr.h"
#include "src/execution/isolate.h"
#include "src/heap/code-range.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS
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

struct PtrComprCageReservationParams
    : public VirtualMemoryCage::ReservationParams {
  PtrComprCageReservationParams() {
    page_allocator = GetPlatformPageAllocator();

    // This is only used when there is a per-Isolate cage, in which case the
    // Isolate is allocated within the cage, and the Isolate root is also the
    // cage base.
    const size_t kIsolateRootBiasPageSize =
        COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL
            ? GetIsolateRootBiasPageSize(page_allocator)
            : 0;
    reservation_size = kPtrComprCageReservationSize + kIsolateRootBiasPageSize;
    base_alignment = kPtrComprCageBaseAlignment;
    base_bias_size = kIsolateRootBiasPageSize;

    // Simplify BoundedPageAllocator's life by configuring it to use same page
    // size as the Heap will use (MemoryChunk::kPageSize).
    page_size =
        RoundUp(size_t{1} << kPageSizeBits, page_allocator->AllocatePageSize());
    requested_start_hint =
        reinterpret_cast<Address>(page_allocator->GetRandomMmapAddr());
  }
};
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(VirtualMemoryCage, GetProcessWidePtrComprCage)
}  // anonymous namespace

// static
void IsolateAllocator::FreeProcessWidePtrComprCageForTesting() {
  if (std::shared_ptr<CodeRange> code_range =
          CodeRange::GetProcessWideCodeRange()) {
    code_range->Free();
  }
  GetProcessWidePtrComprCage()->Free();
}
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE

// static
void IsolateAllocator::InitializeOncePerProcess() {
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  PtrComprCageReservationParams params;
  if (!GetProcessWidePtrComprCage()->InitReservation(params)) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "Failed to reserve virtual memory for process-wide V8 "
        "pointer compression cage");
  }
#endif
}

IsolateAllocator::IsolateAllocator() {
#if defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  PtrComprCageReservationParams params;
  if (!isolate_ptr_compr_cage_.InitReservation(params)) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "Failed to reserve memory for Isolate V8 pointer compression cage");
  }
  page_allocator_ = isolate_ptr_compr_cage_.page_allocator();
  CommitPagesForIsolate();
#elif defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
  // Allocate Isolate in C++ heap when sharing a cage.
  CHECK(GetProcessWidePtrComprCage()->IsReserved());
  page_allocator_ = GetProcessWidePtrComprCage()->page_allocator();
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
  if (isolate_ptr_compr_cage_.reservation()->IsReserved()) {
    // The actual memory will be freed when the |isolate_ptr_compr_cage_| will
    // die.
    return;
  }
#endif

  // The memory was allocated in C++ heap.
  ::operator delete(isolate_memory_);
}

VirtualMemoryCage* IsolateAllocator::GetPtrComprCage() {
#if defined V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  return &isolate_ptr_compr_cage_;
#elif defined V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  return GetProcessWidePtrComprCage();
#else
  return nullptr;
#endif
}

const VirtualMemoryCage* IsolateAllocator::GetPtrComprCage() const {
  return const_cast<IsolateAllocator*>(this)->GetPtrComprCage();
}

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
void IsolateAllocator::CommitPagesForIsolate() {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  CHECK(isolate_ptr_compr_cage_.IsReserved());
  Address isolate_root = isolate_ptr_compr_cage_.base();
  CHECK(IsAligned(isolate_root, kPtrComprCageBaseAlignment));
  CHECK_GE(isolate_ptr_compr_cage_.reservation()->size(),
           kPtrComprCageReservationSize +
               GetIsolateRootBiasPageSize(platform_page_allocator));
  CHECK(isolate_ptr_compr_cage_.reservation()->InVM(
      isolate_root, kPtrComprCageReservationSize));

  size_t page_size = page_allocator_->AllocatePageSize();
  Address isolate_address = isolate_root - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);

  // Inform the bounded page allocator about reserved pages.
  {
    Address reserved_region_address = isolate_root;
    size_t reserved_region_size =
        RoundUp(isolate_end, page_size) - reserved_region_address;

    CHECK(isolate_ptr_compr_cage_.page_allocator()->AllocatePagesAt(
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

    // We are using |isolate_ptr_compr_cage_.reservation()| directly here
    // because |page_allocator_| has bigger commit page size than we actually
    // need.
    CHECK(isolate_ptr_compr_cage_.reservation()->SetPermissions(
        committed_region_address, committed_region_size,
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
