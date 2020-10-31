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

#include "perfetto/ext/base/paged_memory.h"

#include <algorithm>
#include <cmath>

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <Windows.h>
#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <sys/mman.h>
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/container_annotations.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace base {

namespace {

constexpr size_t kGuardSize = kPageSize;

#if TRACK_COMMITTED_SIZE()
constexpr size_t kCommitChunkSize = kPageSize * 1024;  // 4mB
#endif                                                 // TRACK_COMMITTED_SIZE()

}  // namespace

// static
PagedMemory PagedMemory::Allocate(size_t size, int flags) {
  PERFETTO_DCHECK(size % kPageSize == 0);
  size_t outer_size = size + kGuardSize * 2;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  void* ptr = VirtualAlloc(nullptr, outer_size, MEM_RESERVE, PAGE_NOACCESS);
  if (!ptr && (flags & kMayFail))
    return PagedMemory();
  PERFETTO_CHECK(ptr);
  char* usable_region = reinterpret_cast<char*>(ptr) + kGuardSize;
#else   // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  void* ptr = mmap(nullptr, outer_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED && (flags & kMayFail))
    return PagedMemory();
  PERFETTO_CHECK(ptr && ptr != MAP_FAILED);
  char* usable_region = reinterpret_cast<char*>(ptr) + kGuardSize;
  int res = mprotect(ptr, kGuardSize, PROT_NONE);
  res |= mprotect(usable_region + size, kGuardSize, PROT_NONE);
  PERFETTO_CHECK(res == 0);
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

  auto memory = PagedMemory(usable_region, size);
#if TRACK_COMMITTED_SIZE()
  size_t initial_commit = size;
  if (flags & kDontCommit)
    initial_commit = std::min(initial_commit, kCommitChunkSize);
  memory.EnsureCommitted(initial_commit);
#endif  // TRACK_COMMITTED_SIZE()
  return memory;
}

PagedMemory::PagedMemory() {}

// clang-format off
PagedMemory::PagedMemory(char* p, size_t size) : p_(p), size_(size) {
  ANNOTATE_NEW_BUFFER(p_, size_, committed_size_)
}

PagedMemory::PagedMemory(PagedMemory&& other) noexcept {
  *this = other;
  other.p_ = nullptr;
}
// clang-format on

PagedMemory& PagedMemory::operator=(PagedMemory&& other) {
  this->~PagedMemory();
  new (this) PagedMemory(std::move(other));
  return *this;
}

PagedMemory::~PagedMemory() {
  if (!p_)
    return;
  PERFETTO_CHECK(size_);
  char* start = p_ - kGuardSize;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  BOOL res = VirtualFree(start, 0, MEM_RELEASE);
  PERFETTO_CHECK(res != 0);
#else   // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  const size_t outer_size = size_ + kGuardSize * 2;
  int res = munmap(start, outer_size);
  PERFETTO_CHECK(res == 0);
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  ANNOTATE_DELETE_BUFFER(p_, size_, committed_size_)
}

bool PagedMemory::AdviseDontNeed(void* p, size_t size) {
  PERFETTO_DCHECK(p_);
  PERFETTO_DCHECK(p >= p_);
  PERFETTO_DCHECK(static_cast<char*>(p) + size <= p_ + size_);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) || PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
  // Discarding pages on Windows has more CPU cost than is justified for the
  // possible memory savings.
  return false;
#else   // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) ||
        // PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
  // http://man7.org/linux/man-pages/man2/madvise.2.html
  int res = madvise(p, size, MADV_DONTNEED);
  PERFETTO_DCHECK(res == 0);
  return true;
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) ||
        // PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
}

#if TRACK_COMMITTED_SIZE()
void PagedMemory::EnsureCommitted(size_t committed_size) {
  PERFETTO_DCHECK(committed_size > 0u);
  PERFETTO_DCHECK(committed_size <= size_);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  if (committed_size_ >= committed_size)
    return;
  // Rounding up.
  size_t delta = committed_size - committed_size_;
  size_t num_additional_chunks =
      (delta + kCommitChunkSize - 1) / kCommitChunkSize;
  PERFETTO_DCHECK(num_additional_chunks * kCommitChunkSize >= delta);
  // Don't commit more than the total size.
  size_t commit_size = std::min(num_additional_chunks * kCommitChunkSize,
                                size_ - committed_size_);
  void* res = VirtualAlloc(p_ + committed_size_, commit_size, MEM_COMMIT,
                           PAGE_READWRITE);
  PERFETTO_CHECK(res);
  ANNOTATE_CHANGE_SIZE(p_, size_, committed_size_,
                       committed_size_ + commit_size)
  committed_size_ += commit_size;
#else   // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  // mmap commits automatically as needed, so we only track here for ASAN.
  committed_size = std::max(committed_size_, committed_size);
  ANNOTATE_CHANGE_SIZE(p_, size_, committed_size_, committed_size)
  committed_size_ = committed_size;
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
}
#endif  // TRACK_COMMITTED_SIZE()

}  // namespace base
}  // namespace perfetto
