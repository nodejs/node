// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PAGE_MEMORY_INL_H_
#define V8_HEAP_CPPGC_PAGE_MEMORY_INL_H_

#include "src/heap/cppgc/page-memory.h"

namespace cppgc {
namespace internal {

// Returns true if the provided allocator supports committing at the required
// granularity.
inline bool SupportsCommittingGuardPages(PageAllocator* allocator) {
  return kGuardPageSize % allocator->CommitPageSize() == 0;
}

Address NormalPageMemoryRegion::Lookup(ConstAddress address) const {
  size_t index = GetIndex(address);
  if (!page_memories_in_use_[index]) return nullptr;
  const MemoryRegion writeable_region = GetPageMemory(index).writeable_region();
  return writeable_region.Contains(address) ? writeable_region.base() : nullptr;
}

Address LargePageMemoryRegion::Lookup(ConstAddress address) const {
  const MemoryRegion writeable_region = GetPageMemory().writeable_region();
  return writeable_region.Contains(address) ? writeable_region.base() : nullptr;
}

Address PageMemoryRegion::Lookup(ConstAddress address) const {
  DCHECK(reserved_region().Contains(address));
  return is_large()
             ? static_cast<const LargePageMemoryRegion*>(this)->Lookup(address)
             : static_cast<const NormalPageMemoryRegion*>(this)->Lookup(
                   address);
}

PageMemoryRegion* PageMemoryRegionTree::Lookup(ConstAddress address) const {
  auto it = set_.upper_bound(address);
  // This check also covers set_.size() > 0, since for empty vectors it is
  // guaranteed that begin() == end().
  if (it == set_.begin()) return nullptr;
  auto* result = std::next(it, -1)->second;
  if (address < result->reserved_region().end()) return result;
  return nullptr;
}

Address PageBackend::Lookup(ConstAddress address) const {
  PageMemoryRegion* pmr = page_memory_region_tree_.Lookup(address);
  return pmr ? pmr->Lookup(address) : nullptr;
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PAGE_MEMORY_INL_H_
