// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/isolate-allocator.h"

#include "src/base/bounded-page-allocator.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/code-range.h"
#include "src/sandbox/sandbox.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS
struct PtrComprCageReservationParams
    : public VirtualMemoryCage::ReservationParams {
  PtrComprCageReservationParams() {
    page_allocator = GetPlatformPageAllocator();

    reservation_size = kPtrComprCageReservationSize;
    base_alignment = kPtrComprCageBaseAlignment;

    // Simplify BoundedPageAllocator's life by configuring it to use same page
    // size as the Heap will use (MemoryChunk::kPageSize).
    page_size =
        RoundUp(size_t{1} << kPageSizeBits, page_allocator->AllocatePageSize());
    requested_start_hint = RoundDown(
        reinterpret_cast<Address>(page_allocator->GetRandomMmapAddr()),
        base_alignment);
    jit = JitPermission::kNoJit;
  }
};
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(VirtualMemoryCage, GetProcessWidePtrComprCage)
}  // anonymous namespace

// static
void IsolateAllocator::FreeProcessWidePtrComprCageForTesting() {
  if (CodeRange* code_range = CodeRange::GetProcessWideCodeRange()) {
    code_range->Free();
  }
  GetProcessWidePtrComprCage()->Free();
}
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE

// static
void IsolateAllocator::InitializeOncePerProcess() {
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  PtrComprCageReservationParams params;
  base::AddressRegion existing_reservation;
#ifdef V8_ENABLE_SANDBOX
  // The pointer compression cage must be placed at the start of the sandbox.
  auto sandbox = GetProcessWideSandbox();
  CHECK(sandbox->is_initialized());
  Address base = sandbox->address_space()->AllocatePages(
      sandbox->base(), params.reservation_size, params.base_alignment,
      PagePermissions::kNoAccess);
  CHECK_EQ(sandbox->base(), base);
  existing_reservation = base::AddressRegion(base, params.reservation_size);
  params.page_allocator = sandbox->page_allocator();
#endif
  if (!GetProcessWidePtrComprCage()->InitReservation(params,
                                                     existing_reservation)) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "Failed to reserve virtual memory for process-wide V8 "
        "pointer compression cage");
  }
  V8HeapCompressionScheme::InitBase(GetProcessWidePtrComprCage()->base());
#ifdef V8_EXTERNAL_CODE_SPACE
  // Speculatively set the code cage base to the same value in case jitless
  // mode will be used. Once the process-wide CodeRange instance is created
  // the code cage base will be set accordingly.
  ExternalCodeCompressionScheme::InitBase(V8HeapCompressionScheme::base());
#endif  // V8_EXTERNAL_CODE_SPACE
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE
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
#elif defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
  CHECK(GetProcessWidePtrComprCage()->IsReserved());
  page_allocator_ = GetProcessWidePtrComprCage()->page_allocator();
#else
  page_allocator_ = GetPlatformPageAllocator();
#endif  // V8_COMPRESS_POINTERS

  // Allocate Isolate in C++ heap.
  isolate_memory_ = ::operator new(sizeof(Isolate));

  CHECK_NOT_NULL(page_allocator_);
}

IsolateAllocator::~IsolateAllocator() {
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

}  // namespace internal
}  // namespace v8
