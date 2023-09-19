// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_VIRTUAL_MEMORY_H_
#define V8_HEAP_CPPGC_VIRTUAL_MEMORY_H_

#include <cstdint>

#include "include/cppgc/platform.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

// Represents and controls an area of reserved memory.
class V8_EXPORT_PRIVATE VirtualMemory {
 public:
  // Empty VirtualMemory object, controlling no reserved memory.
  VirtualMemory() = default;

  // Reserves virtual memory containing an area of the given size that is
  // aligned per |alignment| rounded up to the |page_allocator|'s allocate page
  // size. The |size| is aligned with |page_allocator|'s commit page size.
  VirtualMemory(PageAllocator*, size_t size, size_t alignment,
                void* hint = nullptr);

  // Releases the reserved memory, if any, controlled by this VirtualMemory
  // object.
  ~VirtualMemory() V8_NOEXCEPT;

  VirtualMemory(VirtualMemory&&) V8_NOEXCEPT;
  VirtualMemory& operator=(VirtualMemory&&) V8_NOEXCEPT;

  // Returns whether the memory has been reserved.
  bool IsReserved() const { return start_ != nullptr; }

  void* address() const {
    DCHECK(IsReserved());
    return start_;
  }

  size_t size() const {
    DCHECK(IsReserved());
    return size_;
  }

 private:
  // Resets to the default state.
  void Reset();

  PageAllocator* page_allocator_ = nullptr;
  void* start_ = nullptr;
  size_t size_ = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_VIRTUAL_MEMORY_H_
