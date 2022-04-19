// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/code-range.h"

#include "src/base/bits.h"
#include "src/base/lazy-instance.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/heap-inl.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

namespace {

// Mutex for creating process_wide_code_range_.
base::LazyMutex process_wide_code_range_creation_mutex_ =
    LAZY_MUTEX_INITIALIZER;

// Weak pointer holding the process-wide CodeRange, if one has been created. All
// Heaps hold a std::shared_ptr to this, so this is destroyed when no Heaps
// remain.
base::LazyInstance<std::weak_ptr<CodeRange>>::type process_wide_code_range_ =
    LAZY_INSTANCE_INITIALIZER;

DEFINE_LAZY_LEAKY_OBJECT_GETTER(CodeRangeAddressHint, GetCodeRangeAddressHint)

void FunctionInStaticBinaryForAddressHint() {}
}  // anonymous namespace

Address CodeRangeAddressHint::GetAddressHint(size_t code_range_size,
                                             size_t alignment) {
  base::MutexGuard guard(&mutex_);

  // Try to allocate code range in the preferred region where we can use
  // short instructions for calling/jumping to embedded builtins.
  base::AddressRegion preferred_region = Isolate::GetShortBuiltinsCallRegion();

  Address result = 0;
  auto it = recently_freed_.find(code_range_size);
  // No recently freed region has been found, try to provide a hint for placing
  // a code region.
  if (it == recently_freed_.end() || it->second.empty()) {
    if (V8_ENABLE_NEAR_CODE_RANGE_BOOL && !preferred_region.is_empty()) {
      auto memory_ranges = base::OS::GetFreeMemoryRangesWithin(
          preferred_region.begin(), preferred_region.end(), code_range_size,
          alignment);
      if (!memory_ranges.empty()) {
        result = memory_ranges.front().start;
        CHECK(IsAligned(result, alignment));
        return result;
      }
      // The empty memory_ranges means that GetFreeMemoryRangesWithin() API
      // is not supported, so use the lowest address from the preferred region
      // as a hint because it'll be at least as good as the fallback hint but
      // with a higher chances to point to the free address space range.
      return RoundUp(preferred_region.begin(), alignment);
    }
    return RoundUp(FUNCTION_ADDR(&FunctionInStaticBinaryForAddressHint),
                   alignment);
  }

  // Try to reuse near code range first.
  if (V8_ENABLE_NEAR_CODE_RANGE_BOOL && !preferred_region.is_empty()) {
    auto freed_regions_for_size = it->second;
    for (auto it_freed = freed_regions_for_size.rbegin();
         it_freed != freed_regions_for_size.rend(); ++it_freed) {
      Address code_range_start = *it_freed;
      if (preferred_region.contains(code_range_start, code_range_size)) {
        CHECK(IsAligned(code_range_start, alignment));
        freed_regions_for_size.erase((it_freed + 1).base());
        return code_range_start;
      }
    }
  }

  result = it->second.back();
  CHECK(IsAligned(result, alignment));
  it->second.pop_back();
  return result;
}

void CodeRangeAddressHint::NotifyFreedCodeRange(Address code_range_start,
                                                size_t code_range_size) {
  base::MutexGuard guard(&mutex_);
  recently_freed_[code_range_size].push_back(code_range_start);
}

CodeRange::~CodeRange() { Free(); }

// static
size_t CodeRange::GetWritableReservedAreaSize() {
  return kReservedCodeRangePages * MemoryAllocator::GetCommitPageSize();
}

bool CodeRange::InitReservation(v8::PageAllocator* page_allocator,
                                size_t requested) {
  DCHECK_NE(requested, 0);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    page_allocator = GetPlatformPageAllocator();
  }

  if (requested <= kMinimumCodeRangeSize) {
    requested = kMinimumCodeRangeSize;
  }
  const size_t reserved_area = GetWritableReservedAreaSize();
  if (requested < (kMaximalCodeRangeSize - reserved_area)) {
    requested += RoundUp(reserved_area, MemoryChunk::kPageSize);
    // Fullfilling both reserved pages requirement and huge code area
    // alignments is not supported (requires re-implementation).
    DCHECK_LE(kMinExpectedOSPageSize, page_allocator->AllocatePageSize());
  }
  DCHECK_IMPLIES(kPlatformRequiresCodeRange,
                 requested <= kMaximalCodeRangeSize);

  VirtualMemoryCage::ReservationParams params;
  params.page_allocator = page_allocator;
  params.reservation_size = requested;
  // base_alignment should be kAnyBaseAlignment when V8_ENABLE_NEAR_CODE_RANGE
  // is enabled so that InitReservation would not break the alignment in
  // GetAddressHint().
  const size_t allocate_page_size = page_allocator->AllocatePageSize();
  params.base_alignment =
      V8_EXTERNAL_CODE_SPACE_BOOL
          ? base::bits::RoundUpToPowerOfTwo(requested)
          : VirtualMemoryCage::ReservationParams::kAnyBaseAlignment;
  params.base_bias_size = RoundUp(reserved_area, allocate_page_size);
  params.page_size = MemoryChunk::kPageSize;
  params.requested_start_hint =
      GetCodeRangeAddressHint()->GetAddressHint(requested, allocate_page_size);

  if (!VirtualMemoryCage::InitReservation(params)) return false;

  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    // Ensure that the code range does not cross the 4Gb boundary and thus
    // default compression scheme of truncating the Code pointers to 32-bit
    // still work.
    Address base = page_allocator_->begin();
    Address last = base + page_allocator_->size() - 1;
    CHECK_EQ(GetPtrComprCageBaseAddress(base),
             GetPtrComprCageBaseAddress(last));
  }

  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space. See
  //   https://cs.chromium.org/chromium/src/components/crash/content/
  //     app/crashpad_win.cc?rcl=fd680447881449fba2edcf0589320e7253719212&l=204
  // for details.
  if (reserved_area > 0) {
    if (!reservation()->SetPermissions(reservation()->address(), reserved_area,
                                       PageAllocator::kReadWrite)) {
      return false;
    }
  }

  return true;
}

void CodeRange::Free() {
  if (IsReserved()) {
    GetCodeRangeAddressHint()->NotifyFreedCodeRange(
        reservation()->region().begin(), reservation()->region().size());
    VirtualMemoryCage::Free();
  }
}

uint8_t* CodeRange::RemapEmbeddedBuiltins(Isolate* isolate,
                                          const uint8_t* embedded_blob_code,
                                          size_t embedded_blob_code_size) {
  base::MutexGuard guard(&remap_embedded_builtins_mutex_);

  // Remap embedded builtins into the end of the address range controlled by
  // the BoundedPageAllocator.
  const base::AddressRegion code_region(page_allocator()->begin(),
                                        page_allocator()->size());
  CHECK_NE(code_region.begin(), kNullAddress);
  CHECK(!code_region.is_empty());

  uint8_t* embedded_blob_code_copy =
      embedded_blob_code_copy_.load(std::memory_order_acquire);
  if (embedded_blob_code_copy) {
    DCHECK(
        code_region.contains(reinterpret_cast<Address>(embedded_blob_code_copy),
                             embedded_blob_code_size));
    SLOW_DCHECK(memcmp(embedded_blob_code, embedded_blob_code_copy,
                       embedded_blob_code_size) == 0);
    return embedded_blob_code_copy;
  }

  const size_t kAllocatePageSize = page_allocator()->AllocatePageSize();
  const size_t kCommitPageSize = page_allocator()->CommitPageSize();
  size_t allocate_code_size =
      RoundUp(embedded_blob_code_size, kAllocatePageSize);

  // Allocate the re-embedded code blob in the end.
  void* hint = reinterpret_cast<void*>(code_region.end() - allocate_code_size);

  embedded_blob_code_copy =
      reinterpret_cast<uint8_t*>(page_allocator()->AllocatePages(
          hint, allocate_code_size, kAllocatePageSize,
          PageAllocator::kNoAccess));

  if (!embedded_blob_code_copy) {
    V8::FatalProcessOutOfMemory(
        isolate, "Can't allocate space for re-embedded builtins");
  }

  size_t code_size = RoundUp(embedded_blob_code_size, kCommitPageSize);
  if constexpr (base::OS::IsRemapPageSupported()) {
    // By default, the embedded builtins are not remapped, but copied. This
    // costs memory, since builtins become private dirty anonymous memory,
    // rather than shared, clean, file-backed memory for the embedded version.
    // If the OS supports it, we can remap the builtins *on top* of the space
    // allocated in the code range, making the "copy" shared, clean, file-backed
    // memory, and thus saving sizeof(builtins).
    //
    // Builtins should start at a page boundary, see
    // platform-embedded-file-writer-mac.cc. If it's not the case (e.g. if the
    // embedded builtins are not coming from the binary), fall back to copying.
    if (IsAligned(reinterpret_cast<uintptr_t>(embedded_blob_code),
                  kCommitPageSize)) {
      bool ok = base::OS::RemapPages(embedded_blob_code, code_size,
                                     embedded_blob_code_copy,
                                     base::OS::MemoryPermission::kReadExecute);

      if (ok) {
        embedded_blob_code_copy_.store(embedded_blob_code_copy,
                                       std::memory_order_release);
        return embedded_blob_code_copy;
      }
    }
  }

  if (!page_allocator()->SetPermissions(embedded_blob_code_copy, code_size,
                                        PageAllocator::kReadWrite)) {
    V8::FatalProcessOutOfMemory(isolate,
                                "Re-embedded builtins: set permissions");
  }
  memcpy(embedded_blob_code_copy, embedded_blob_code, embedded_blob_code_size);

  if (!page_allocator()->SetPermissions(embedded_blob_code_copy, code_size,
                                        PageAllocator::kReadExecute)) {
    V8::FatalProcessOutOfMemory(isolate,
                                "Re-embedded builtins: set permissions");
  }

  embedded_blob_code_copy_.store(embedded_blob_code_copy,
                                 std::memory_order_release);
  return embedded_blob_code_copy;
}

// static
std::shared_ptr<CodeRange> CodeRange::EnsureProcessWideCodeRange(
    v8::PageAllocator* page_allocator, size_t requested_size) {
  base::MutexGuard guard(process_wide_code_range_creation_mutex_.Pointer());
  std::shared_ptr<CodeRange> code_range = process_wide_code_range_.Get().lock();
  if (!code_range) {
    code_range = std::make_shared<CodeRange>();
    if (!code_range->InitReservation(page_allocator, requested_size)) {
      V8::FatalProcessOutOfMemory(
          nullptr, "Failed to reserve virtual memory for CodeRange");
    }
    *process_wide_code_range_.Pointer() = code_range;
  }
  return code_range;
}

// static
std::shared_ptr<CodeRange> CodeRange::GetProcessWideCodeRange() {
  return process_wide_code_range_.Get().lock();
}

}  // namespace internal
}  // namespace v8
