// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CODE_RANGE_H_
#define V8_HEAP_CODE_RANGE_H_

#include <unordered_map>
#include <vector>

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// The process-wide singleton that keeps track of code range regions with the
// intention to reuse free code range regions as a workaround for CFG memory
// leaks (see crbug.com/870054).
class CodeRangeAddressHint {
 public:
  // Returns the most recently freed code range start address for the given
  // size. If there is no such entry, then a random address is returned.
  V8_EXPORT_PRIVATE Address GetAddressHint(size_t code_range_size);

  V8_EXPORT_PRIVATE void NotifyFreedCodeRange(Address code_range_start,
                                              size_t code_range_size);

 private:
  base::Mutex mutex_;
  // A map from code range size to an array of recently freed code range
  // addresses. There should be O(1) different code range sizes.
  // The length of each array is limited by the peak number of code ranges,
  // which should be also O(1).
  std::unordered_map<size_t, std::vector<Address>> recently_freed_;
};

// A code range is a virtual memory cage that may contain executable code. It
// has the following layout.
//
// +------------+-----+----------------  ~~~  -+
// |     RW     | ... |    ...                 |
// +------------+-----+----------------- ~~~  -+
// ^            ^     ^
// start        base  allocatable base
//
// <------------>     <------------------------>
//   reserved            allocatable region
// <------------------------------------------->
//               code region
//
// The start of the reservation may include reserved page with read-write access
// as required by some platforms (Win64). The cage's page allocator does not
// control the optional reserved page in the beginning of the code region.
//
// The following conditions hold:
// 1) |reservation()->region()| >= |optional RW pages| +
//    |reservation()->page_allocator()|
// 2) |reservation()| is AllocatePageSize()-aligned
// 3) |reservation()->page_allocator()| (i.e. allocatable base) is
//    MemoryChunk::kAlignment-aligned
// 4) |base()| is CommitPageSize()-aligned
class CodeRange final : public VirtualMemoryCage {
 public:
  V8_EXPORT_PRIVATE ~CodeRange();

  uint8_t* embedded_blob_code_copy() const {
    // remap_embedded_builtins_mutex_ is designed to protect write contention to
    // embedded_blob_code_copy_. It is safe to be read without taking the
    // mutex. It is read to check if short builtins ought to be enabled because
    // a shared CodeRange has already remapped builtins and to find where the
    // instruction stream for a builtin is.
    //
    // For the first, this racing with an Isolate calling RemapEmbeddedBuiltins
    // may result in disabling short builtins, which is not a correctness issue.
    //
    // For the second, this racing with an Isolate calling RemapEmbeddedBuiltins
    // may result in an already running Isolate that did not have short builtins
    // enabled (due to max old generation size) to switch over to using remapped
    // builtins, which is also not a correctness issue as the remapped builtins
    // are byte-equivalent.
    //
    // Both these scenarios should be rare. The initial Isolate is usually
    // created by itself, i.e. without contention. Additionally, the first
    // Isolate usually remaps builtins on machines with enough memory, not
    // subsequent Isolates in the same process.
    return embedded_blob_code_copy_.load(std::memory_order_acquire);
  }

#ifdef V8_OS_WIN64
  // 64-bit Windows needs to track how many Isolates are using the CodeRange for
  // registering and unregistering of unwind info. Note that even though
  // CodeRanges are used with std::shared_ptr, std::shared_ptr::use_count should
  // not be used for synchronization as it's usually implemented with a relaxed
  // read.
  uint32_t AtomicIncrementUnwindInfoUseCount() {
    return unwindinfo_use_count_.fetch_add(1, std::memory_order_acq_rel);
  }

  uint32_t AtomicDecrementUnwindInfoUseCount() {
    return unwindinfo_use_count_.fetch_sub(1, std::memory_order_acq_rel);
  }
#endif  // V8_OS_WIN64

  bool InitReservation(v8::PageAllocator* page_allocator, size_t requested);

  void Free();

  // Remap and copy the embedded builtins into this CodeRange. This method is
  // idempotent and only performs the copy once. This property is so that this
  // method can be used uniformly regardless of having a per-Isolate or a shared
  // pointer cage. Returns the address of the copy.
  //
  // The builtins code region will be freed with the code range at tear down.
  //
  // When ENABLE_SLOW_DCHECKS is on, the contents of the embedded_blob_code are
  // compared against the already copied version.
  uint8_t* RemapEmbeddedBuiltins(Isolate* isolate,
                                 const uint8_t* embedded_blob_code,
                                 size_t embedded_blob_code_size);

  static std::shared_ptr<CodeRange> EnsureProcessWideCodeRange(
      v8::PageAllocator* page_allocator, size_t requested_size);

  // If InitializeProcessWideCodeRangeOnce has been called, returns the
  // initialized CodeRange. Otherwise returns an empty std::shared_ptr.
  V8_EXPORT_PRIVATE static std::shared_ptr<CodeRange> GetProcessWideCodeRange();

 private:
  // Used when short builtin calls are enabled, where embedded builtins are
  // copied into the CodeRange so calls can be nearer.
  std::atomic<uint8_t*> embedded_blob_code_copy_{nullptr};

  // When sharing a CodeRange among Isolates, calls to RemapEmbeddedBuiltins may
  // race during Isolate::Init.
  base::Mutex remap_embedded_builtins_mutex_;

#ifdef V8_OS_WIN64
  std::atomic<uint32_t> unwindinfo_use_count_{0};
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CODE_RANGE_H_
