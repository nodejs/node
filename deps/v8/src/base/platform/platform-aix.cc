// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform specific code for AIX goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ucontext.h>

#include <errno.h>
#include <fcntl.h>  // open
#include <limits.h>
#include <stdarg.h>
#include <strings.h>    // index
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // getpagesize

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {


class AIXTimezoneCache : public PosixTimezoneCache {
  const char* LocalTimezone(double time) override;

  double LocalTimeOffset() override;

  ~AIXTimezoneCache() override {}
};

const char* AIXTimezoneCache::LocalTimezone(double time) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time / msPerSecond));
  struct tm tm;
  struct tm* t = localtime_r(&tv, &tm);
  if (NULL == t) return "";
  return tzname[0];  // The location of the timezone string on AIX.
}

double AIXTimezoneCache::LocalTimeOffset() {
  // On AIX, struct tm does not contain a tm_gmtoff field.
  time_t utc = time(NULL);
  DCHECK(utc != -1);
  struct tm tm;
  struct tm* loc = localtime_r(&utc, &tm);
  DCHECK(loc != NULL);
  return static_cast<double>((mktime(loc) - utc) * msPerSecond);
}

TimezoneCache* OS::CreateTimezoneCache() { return new AIXTimezoneCache(); }

// Constants used for mmap.
static const int kMmapFd = -1;
static const int kMmapFdOffset = 0;

void* OS::Allocate(const size_t requested, size_t* allocated,
                   OS::MemoryPermission access, void* hint) {
  const size_t msize = RoundUp(requested, getpagesize());
  int prot = GetProtectionFromMemoryPermission(access);
  void* mbase = mmap(hint, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, kMmapFd,
                     kMmapFdOffset);

  if (mbase == MAP_FAILED) return NULL;
  *allocated = msize;
  return mbase;
}

// static
void* OS::ReserveRegion(size_t size, void* hint) {
  void* result = mmap(hint, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
                      kMmapFd, kMmapFdOffset);

  if (result == MAP_FAILED) return nullptr;

  return result;
}

// static
void* OS::ReserveAlignedRegion(size_t size, size_t alignment, void* hint,
                               size_t* allocated) {
  DCHECK((alignment % OS::AllocateAlignment()) == 0);
  hint = AlignedAddress(hint, alignment);
  size_t request_size =
      RoundUp(size + alignment, static_cast<intptr_t>(OS::AllocateAlignment()));
  void* result = ReserveRegion(request_size, hint);
  if (result == nullptr) {
    *allocated = 0;
    return nullptr;
  }

  uint8_t* base = static_cast<uint8_t*>(result);
  uint8_t* aligned_base = RoundUp(base, alignment);
  DCHECK_LE(base, aligned_base);

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    OS::Free(base, prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, OS::AllocateAlignment());
  DCHECK_LE(aligned_size, request_size);

  if (aligned_size != request_size) {
    size_t suffix_size = request_size - aligned_size;
    OS::Free(aligned_base + aligned_size, suffix_size);
    request_size -= suffix_size;
  }

  DCHECK(aligned_size == request_size);

  *allocated = aligned_size;
  return static_cast<void*>(aligned_base);
}

// static
bool OS::CommitRegion(void* address, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);

  if (mprotect(address, size, prot) == -1) return false;

  return true;
}

// static
bool OS::UncommitRegion(void* address, size_t size) {
  return mprotect(address, size, PROT_NONE) != -1;
}

// static
bool OS::ReleaseRegion(void* address, size_t size) {
  return munmap(address, size) == 0;
}

// static
bool OS::ReleasePartialRegion(void* address, size_t size) {
  return munmap(address, size) == 0;
}

// static
bool OS::HasLazyCommits() { return true; }

static unsigned StringToLong(char* buffer) {
  return static_cast<unsigned>(strtol(buffer, NULL, 16));  // NOLINT
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  static const int MAP_LENGTH = 1024;
  int fd = open("/proc/self/maps", O_RDONLY);
  if (fd < 0) return result;
  while (true) {
    char addr_buffer[11];
    addr_buffer[0] = '0';
    addr_buffer[1] = 'x';
    addr_buffer[10] = 0;
    ssize_t rc = read(fd, addr_buffer + 2, 8);
    if (rc < 8) break;
    unsigned start = StringToLong(addr_buffer);
    rc = read(fd, addr_buffer + 2, 1);
    if (rc < 1) break;
    if (addr_buffer[2] != '-') break;
    rc = read(fd, addr_buffer + 2, 8);
    if (rc < 8) break;
    unsigned end = StringToLong(addr_buffer);
    char buffer[MAP_LENGTH];
    int bytes_read = -1;
    do {
      bytes_read++;
      if (bytes_read >= MAP_LENGTH - 1) break;
      rc = read(fd, buffer + bytes_read, 1);
      if (rc < 1) break;
    } while (buffer[bytes_read] != '\n');
    buffer[bytes_read] = 0;
    // Ignore mappings that are not executable.
    if (buffer[3] != 'x') continue;
    char* start_of_path = index(buffer, '/');
    // There may be no filename in this line.  Skip to next.
    if (start_of_path == NULL) continue;
    buffer[bytes_read] = 0;
    result.push_back(SharedLibraryAddress(start_of_path, start, end));
  }
  close(fd);
  return result;
}

void OS::SignalCodeMovingGC(void* hint) {}

}  // namespace base
}  // namespace v8
