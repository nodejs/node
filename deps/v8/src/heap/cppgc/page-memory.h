// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PAGE_MEMORY_H_
#define V8_HEAP_CPPGC_PAGE_MEMORY_H_

#include <array>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "include/cppgc/platform.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE MemoryRegion final {
 public:
  MemoryRegion() = default;
  MemoryRegion(Address base, size_t size) : base_(base), size_(size) {
    DCHECK(base);
    DCHECK_LT(0u, size);
  }

  Address base() const { return base_; }
  size_t size() const { return size_; }
  Address end() const { return base_ + size_; }

  bool Contains(ConstAddress addr) const {
    return (reinterpret_cast<uintptr_t>(addr) -
            reinterpret_cast<uintptr_t>(base_)) < size_;
  }

  bool Contains(const MemoryRegion& other) const {
    return base_ <= other.base() && other.end() <= end();
  }

 private:
  Address base_ = nullptr;
  size_t size_ = 0;
};

// PageMemory provides the backing of a single normal or large page.
class V8_EXPORT_PRIVATE PageMemory final {
 public:
  PageMemory(MemoryRegion overall, MemoryRegion writeable)
      : overall_(overall), writable_(writeable) {
    DCHECK(overall.Contains(writeable));
  }

  const MemoryRegion writeable_region() const { return writable_; }
  const MemoryRegion overall_region() const { return overall_; }

 private:
  MemoryRegion overall_;
  MemoryRegion writable_;
};

class V8_EXPORT_PRIVATE PageMemoryRegion {
 public:
  virtual ~PageMemoryRegion();

  const MemoryRegion reserved_region() const { return reserved_region_; }
  bool is_large() const { return is_large_; }

  // Lookup writeable base for an |address| that's contained in
  // PageMemoryRegion. Filters out addresses that are contained in non-writeable
  // regions (e.g. guard pages).
  inline Address Lookup(ConstAddress address) const;

  // Disallow copy/move.
  PageMemoryRegion(const PageMemoryRegion&) = delete;
  PageMemoryRegion& operator=(const PageMemoryRegion&) = delete;

  virtual void UnprotectForTesting() = 0;

 protected:
  PageMemoryRegion(PageAllocator*, MemoryRegion, bool);

  PageAllocator* const allocator_;
  const MemoryRegion reserved_region_;
  const bool is_large_;
};

// NormalPageMemoryRegion serves kNumPageRegions normal-sized PageMemory object.
class V8_EXPORT_PRIVATE NormalPageMemoryRegion final : public PageMemoryRegion {
 public:
  static constexpr size_t kNumPageRegions = 10;

  explicit NormalPageMemoryRegion(PageAllocator*);
  ~NormalPageMemoryRegion() override;

  const PageMemory GetPageMemory(size_t index) const {
    DCHECK_LT(index, kNumPageRegions);
    return PageMemory(
        MemoryRegion(reserved_region().base() + kPageSize * index, kPageSize),
        MemoryRegion(
            reserved_region().base() + kPageSize * index + kGuardPageSize,
            kPageSize - 2 * kGuardPageSize));
  }

  // Allocates a normal page at |writeable_base| address. Changes page
  // protection.
  void Allocate(Address writeable_base);

  // Frees a normal page at at |writeable_base| address. Changes page
  // protection.
  void Free(Address);

  inline Address Lookup(ConstAddress) const;

  void UnprotectForTesting() final;

 private:
  void ChangeUsed(size_t index, bool value) {
    DCHECK_LT(index, kNumPageRegions);
    DCHECK_EQ(value, !page_memories_in_use_[index]);
    page_memories_in_use_[index] = value;
  }

  size_t GetIndex(ConstAddress address) const {
    return static_cast<size_t>(address - reserved_region().base()) >>
           kPageSizeLog2;
  }

  std::array<bool, kNumPageRegions> page_memories_in_use_ = {};
};

// LargePageMemoryRegion serves a single large PageMemory object.
class V8_EXPORT_PRIVATE LargePageMemoryRegion final : public PageMemoryRegion {
 public:
  LargePageMemoryRegion(PageAllocator*, size_t);
  ~LargePageMemoryRegion() override;

  const PageMemory GetPageMemory() const {
    return PageMemory(
        MemoryRegion(reserved_region().base(), reserved_region().size()),
        MemoryRegion(reserved_region().base() + kGuardPageSize,
                     reserved_region().size() - 2 * kGuardPageSize));
  }

  inline Address Lookup(ConstAddress) const;

  void UnprotectForTesting() final;
};

// A PageMemoryRegionTree is a binary search tree of PageMemoryRegions sorted
// by reserved base addresses.
//
// The tree does not keep its elements alive but merely provides indexing
// capabilities.
class V8_EXPORT_PRIVATE PageMemoryRegionTree final {
 public:
  PageMemoryRegionTree();
  ~PageMemoryRegionTree();

  void Add(PageMemoryRegion*);
  void Remove(PageMemoryRegion*);

  inline PageMemoryRegion* Lookup(ConstAddress) const;

 private:
  std::map<ConstAddress, PageMemoryRegion*> set_;
};

// A pool of PageMemory objects represented by the writeable base addresses.
//
// The pool does not keep its elements alive but merely provides pooling
// capabilities.
class V8_EXPORT_PRIVATE NormalPageMemoryPool final {
 public:
  static constexpr size_t kNumPoolBuckets = 16;

  using Result = std::pair<NormalPageMemoryRegion*, Address>;

  NormalPageMemoryPool();
  ~NormalPageMemoryPool();

  void Add(size_t, NormalPageMemoryRegion*, Address);
  Result Take(size_t);

 private:
  std::vector<Result> pool_[kNumPoolBuckets];
};

// A backend that is used for allocating and freeing normal and large pages.
//
// Internally maintaints a set of PageMemoryRegions. The backend keeps its used
// regions alive.
class V8_EXPORT_PRIVATE PageBackend final {
 public:
  explicit PageBackend(PageAllocator*);
  ~PageBackend();

  // Allocates a normal page from the backend.
  //
  // Returns the writeable base of the region.
  Address AllocateNormalPageMemory(size_t);

  // Returns normal page memory back to the backend. Expects the
  // |writeable_base| returned by |AllocateNormalMemory()|.
  void FreeNormalPageMemory(size_t, Address writeable_base);

  // Allocates a large page from the backend.
  //
  // Returns the writeable base of the region.
  Address AllocateLargePageMemory(size_t size);

  // Returns large page memory back to the backend. Expects the |writeable_base|
  // returned by |AllocateLargePageMemory()|.
  void FreeLargePageMemory(Address writeable_base);

  // Returns the writeable base if |address| is contained in a valid page
  // memory.
  inline Address Lookup(ConstAddress) const;

  // Disallow copy/move.
  PageBackend(const PageBackend&) = delete;
  PageBackend& operator=(const PageBackend&) = delete;

 private:
  PageAllocator* allocator_;
  NormalPageMemoryPool page_pool_;
  PageMemoryRegionTree page_memory_region_tree_;
  std::vector<std::unique_ptr<PageMemoryRegion>> normal_page_memory_regions_;
  std::unordered_map<PageMemoryRegion*, std::unique_ptr<PageMemoryRegion>>
      large_page_memory_regions_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PAGE_MEMORY_H_
