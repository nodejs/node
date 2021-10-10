// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_VM_CAGE_H_
#define V8_INIT_VM_CAGE_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"

namespace v8 {

class PageAllocator;

namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE

/**
 * V8 Virtual Memory Cage.
 *
 * When the virtual memory cage is enabled, v8 will place most of its objects
 * inside a dedicated region of virtual address space. In particular, all v8
 * heaps, inside which objects reference themselves using compressed (32-bit)
 * pointers, are located at the start of the virtual memory cage (the "pointer
 * cage") and pure memory buffers like ArrayBuffer backing stores, which
 * themselves do not contain any pointers, are located in the remaining part of
 * the cage (the "data cage"). These buffers will eventually be referenced from
 * inside the v8 heap using offsets rather than pointers. It should then be
 * assumed that an attacker is able to corrupt data arbitrarily and concurrently
 * inside the virtual memory cage.
 *
 * As the embedder is responsible for providing ArrayBuffer allocators, v8
 * exposes a page allocator for the data cage to the embedder.
 *
 * TODO(chromium:1218005) Maybe don't call the sub-regions "cages" as well to
 * avoid confusion? In any case, the names should probably be identical to the
 * internal names for these virtual memory regions (where they are currently
 * called cages).
 * TODO(chromium:1218005) come up with a coherent naming scheme for this class
 * and the other "cages" in v8.
 */
class V8VirtualMemoryCage {
 public:
  // +-  ~~~  -+----------------+-----------------------  ~~~  -+-  ~~~  -+
  // |  32 GB  |      4 GB      |                               |  32 GB  |
  // +-  ~~~  -+----------------+-----------------------  ~~~  -+-  ~~~  -+
  // ^         ^                ^                               ^
  // Guard     Pointer Cage     Data Cage                       Guard
  // Region    (contains all    (contains all ArrayBuffer and   Region
  // (front)   V8 heaps)        WASM memory backing stores)     (back)
  //
  //           | base ---------------- size ------------------> |

  V8VirtualMemoryCage() = default;

  V8VirtualMemoryCage(const V8VirtualMemoryCage&) = delete;
  V8VirtualMemoryCage& operator=(V8VirtualMemoryCage&) = delete;

  bool is_initialized() const { return initialized_; }
  bool is_disabled() const { return disabled_; }
  bool is_enabled() const { return !disabled_; }

  bool Initialize(v8::PageAllocator* page_allocator);
  void Disable() {
    CHECK(!initialized_);
    disabled_ = true;
  }

  void TearDown();

  Address base() const { return base_; }
  size_t size() const { return size_; }

  Address pointer_cage_base() const { return base_; }
  size_t pointer_cage_size() const { return kVirtualMemoryCagePointerCageSize; }

  Address data_cage_base() const {
    return pointer_cage_base() + pointer_cage_size();
  }
  size_t data_cage_size() const { return size_ - pointer_cage_size(); }

  bool Contains(Address addr) const {
    return addr >= base_ && addr < base_ + size_;
  }

  bool Contains(void* ptr) const {
    return Contains(reinterpret_cast<Address>(ptr));
  }

  v8::PageAllocator* GetDataCagePageAllocator() {
    return data_cage_page_allocator_.get();
  }

 private:
  friend class SequentialUnmapperTest;

  // We allow tests to disable the guard regions around the cage. This is useful
  // for example for tests like the SequentialUnmapperTest which track page
  // allocations and so would incur a large overhead from the guard regions.
  bool Initialize(v8::PageAllocator* page_allocator, size_t total_size,
                  bool use_guard_regions);

  Address base_ = kNullAddress;
  size_t size_ = 0;
  bool has_guard_regions_ = false;
  bool initialized_ = false;
  bool disabled_ = false;
  v8::PageAllocator* page_allocator_ = nullptr;
  std::unique_ptr<v8::PageAllocator> data_cage_page_allocator_;
};

V8VirtualMemoryCage* GetProcessWideVirtualMemoryCage();

#endif  // V8_VIRTUAL_MEMORY_CAGE

V8_INLINE bool IsValidBackingStorePointer(void* ptr) {
#ifdef V8_VIRTUAL_MEMORY_CAGE
  Address addr = reinterpret_cast<Address>(ptr);
  return kAllowBackingStoresOutsideDataCage || addr == kNullAddress ||
         GetProcessWideVirtualMemoryCage()->Contains(addr);
#else
  return true;
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_VM_CAGE_H_
