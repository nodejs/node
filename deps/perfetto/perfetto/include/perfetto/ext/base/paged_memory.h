/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_EXT_BASE_PAGED_MEMORY_H_
#define INCLUDE_PERFETTO_EXT_BASE_PAGED_MEMORY_H_

#include <memory>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/container_annotations.h"

// We need to track the committed size on windows and when ASAN is enabled.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) || defined(ADDRESS_SANITIZER)
#define TRACK_COMMITTED_SIZE() 1
#else
#define TRACK_COMMITTED_SIZE() 0
#endif

namespace perfetto {
namespace base {

class PagedMemory {
 public:
  // Initializes an invalid PagedMemory pointing to nullptr.
  PagedMemory();

  ~PagedMemory();

  PagedMemory(PagedMemory&& other) noexcept;
  PagedMemory& operator=(PagedMemory&& other);

  enum AllocationFlags {
    // By default, Allocate() crashes if the underlying mmap fails (e.g., if out
    // of virtual address space). When this flag is provided, an invalid
    // PagedMemory pointing to nullptr is returned in this case instead.
    kMayFail = 1 << 0,

    // By default, Allocate() commits the allocated memory immediately. When
    // this flag is provided, the memory virtual address space may only be
    // reserved and the user should call EnsureCommitted() before writing to
    // memory addresses.
    kDontCommit = 1 << 1,
  };

  // Allocates |size| bytes using mmap(MAP_ANONYMOUS). The returned memory is
  // guaranteed to be page-aligned and guaranteed to be zeroed. |size| must be a
  // multiple of 4KB (a page size). For |flags|, see the AllocationFlags enum
  // above.
  static PagedMemory Allocate(size_t size, int flags = 0);

  // Hint to the OS that the memory range is not needed and can be discarded.
  // The memory remains accessible and its contents may be retained, or they
  // may be zeroed. This function may be a NOP on some platforms. Returns true
  // if implemented.
  bool AdviseDontNeed(void* p, size_t size);

  // Ensures that at least the first |committed_size| bytes of the allocated
  // memory region are committed. The implementation may commit memory in larger
  // chunks above |committed_size|. Crashes if the memory couldn't be committed.
#if TRACK_COMMITTED_SIZE()
  void EnsureCommitted(size_t committed_size);
#else   // TRACK_COMMITTED_SIZE()
  void EnsureCommitted(size_t /*committed_size*/) {}
#endif  // TRACK_COMMITTED_SIZE()

  inline void* Get() const noexcept { return p_; }
  inline bool IsValid() const noexcept { return !!p_; }
  inline size_t size() const noexcept { return size_; }

 private:
  PagedMemory(char* p, size_t size);

  PagedMemory(const PagedMemory&) = delete;
  // Defaulted for implementation of move constructor + assignment.
  PagedMemory& operator=(const PagedMemory&) = default;

  char* p_ = nullptr;
  size_t size_ = 0;

#if TRACK_COMMITTED_SIZE()
  size_t committed_size_ = 0u;
#endif  // TRACK_COMMITTED_SIZE()
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_PAGED_MEMORY_H_
