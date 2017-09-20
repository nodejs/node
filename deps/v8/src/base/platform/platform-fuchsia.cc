// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

void* OS::Allocate(const size_t requested, size_t* allocated,
                   OS::MemoryPermission access, void* hint) {
  const size_t msize = RoundUp(requested, AllocateAlignment());
  int prot = GetProtectionFromMemoryPermission(access);
  void* mbase = mmap(hint, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mbase == MAP_FAILED) return NULL;
  *allocated = msize;
  return mbase;
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return std::vector<SharedLibraryAddress>();
}

void OS::SignalCodeMovingGC() {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
}

VirtualMemory::VirtualMemory() : address_(NULL), size_(0) {}

VirtualMemory::VirtualMemory(size_t size, void* hint)
    : address_(ReserveRegion(size, hint)), size_(size) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
}

VirtualMemory::VirtualMemory(size_t size, size_t alignment, void* hint)
    : address_(NULL), size_(0) {}

VirtualMemory::~VirtualMemory() {}

void VirtualMemory::Reset() {}

bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return false;
}

bool VirtualMemory::Uncommit(void* address, size_t size) { return false; }

bool VirtualMemory::Guard(void* address) { return false; }

// static
void* VirtualMemory::ReserveRegion(size_t size, void* hint) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return NULL;
}

// static
bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return false;
}

// static
bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return false;
}

// static
bool VirtualMemory::ReleasePartialRegion(void* base, size_t size,
                                         void* free_start, size_t free_size) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return false;
}

// static
bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return false;
}

// static
bool VirtualMemory::HasLazyCommits() {
  CHECK(false);  // TODO(fuchsia): Port, https://crbug.com/731217.
  return false;
}

}  // namespace base
}  // namespace v8
