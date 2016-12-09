// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for QNX goes here. For the POSIX-compatible
// parts the implementation is in platform-posix.cc.

#include <backtrace.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ucontext.h>

// QNX requires memory pages to be marked as executable.
// Otherwise, the OS raises an exception when executing code in that page.
#include <errno.h>
#include <fcntl.h>      // open
#include <stdarg.h>
#include <strings.h>    // index
#include <sys/mman.h>   // mmap & munmap
#include <sys/procfs.h>
#include <sys/stat.h>   // open
#include <unistd.h>     // sysconf

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform.h"


namespace v8 {
namespace base {

// 0 is never a valid thread id on Qnx since tids and pids share a
// name space and pid 0 is reserved (see man 2 kill).
static const pthread_t kNoThread = (pthread_t) 0;


#ifdef __arm__

bool OS::ArmUsingHardFloat() {
  // GCC versions 4.6 and above define __ARM_PCS or __ARM_PCS_VFP to specify
  // the Floating Point ABI used (PCS stands for Procedure Call Standard).
  // We use these as well as a couple of other defines to statically determine
  // what FP ABI used.
  // GCC versions 4.4 and below don't support hard-fp.
  // GCC versions 4.5 may support hard-fp without defining __ARM_PCS or
  // __ARM_PCS_VFP.

#define GCC_VERSION (__GNUC__ * 10000                                          \
                     + __GNUC_MINOR__ * 100                                    \
                     + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40600
#if defined(__ARM_PCS_VFP)
  return true;
#else
  return false;
#endif

#elif GCC_VERSION < 40500
  return false;

#else
#if defined(__ARM_PCS_VFP)
  return true;
#elif defined(__ARM_PCS) || defined(__SOFTFP__) || defined(__SOFTFP) || \
      !defined(__VFP_FP__)
  return false;
#else
#error "Your version of GCC does not report the FP ABI compiled for."          \
       "Please report it on this issue"                                        \
       "http://code.google.com/p/v8/issues/detail?id=2140"

#endif
#endif
#undef GCC_VERSION
}

#endif  // __arm__


const char* OS::LocalTimezone(double time, TimezoneCache* cache) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm tm;
  struct tm* t = localtime_r(&tv, &tm);
  if (NULL == t) return "";
  return t->tm_zone;
}


double OS::LocalTimeOffset(TimezoneCache* cache) {
  time_t tv = time(NULL);
  struct tm tm;
  struct tm* t = localtime_r(&tv, &tm);
  // tm_gmtoff includes any daylight savings offset, so subtract it.
  return static_cast<double>(t->tm_gmtoff * msPerSecond -
                             (t->tm_isdst > 0 ? 3600 * msPerSecond : 0));
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, AllocateAlignment());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* addr = OS::GetRandomMmapAddr();
  void* mbase = mmap(addr, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mbase == MAP_FAILED) return NULL;
  *allocated = msize;
  return mbase;
}


std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  procfs_mapinfo *mapinfos = NULL, *mapinfo;
  int proc_fd, num, i;

  struct {
    procfs_debuginfo info;
    char buff[PATH_MAX];
  } map;

  char buf[PATH_MAX + 1];
  snprintf(buf, PATH_MAX + 1, "/proc/%d/as", getpid());

  if ((proc_fd = open(buf, O_RDONLY)) == -1) {
    close(proc_fd);
    return result;
  }

  /* Get the number of map entries.  */
  if (devctl(proc_fd, DCMD_PROC_MAPINFO, NULL, 0, &num) != EOK) {
    close(proc_fd);
    return result;
  }

  mapinfos = reinterpret_cast<procfs_mapinfo *>(
      malloc(num * sizeof(procfs_mapinfo)));
  if (mapinfos == NULL) {
    close(proc_fd);
    return result;
  }

  /* Fill the map entries.  */
  if (devctl(proc_fd, DCMD_PROC_PAGEDATA,
      mapinfos, num * sizeof(procfs_mapinfo), &num) != EOK) {
    free(mapinfos);
    close(proc_fd);
    return result;
  }

  for (i = 0; i < num; i++) {
    mapinfo = mapinfos + i;
    if (mapinfo->flags & MAP_ELF) {
      map.info.vaddr = mapinfo->vaddr;
      if (devctl(proc_fd, DCMD_PROC_MAPDEBUG, &map, sizeof(map), 0) != EOK) {
        continue;
      }
      result.push_back(SharedLibraryAddress(
          map.info.path, mapinfo->vaddr, mapinfo->vaddr + mapinfo->size));
    }
  }
  free(mapinfos);
  close(proc_fd);
  return result;
}


void OS::SignalCodeMovingGC() {
}


// Constants used for mmap.
static const int kMmapFd = -1;
static const int kMmapFdOffset = 0;


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  DCHECK((alignment % OS::AllocateAlignment()) == 0);
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* reservation = mmap(OS::GetRandomMmapAddr(),
                           request_size,
                           PROT_NONE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_LAZY,
                           kMmapFd,
                           kMmapFdOffset);
  if (reservation == MAP_FAILED) return;

  uint8_t* base = static_cast<uint8_t*>(reservation);
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

  address_ = static_cast<void*>(aligned_base);
  size_ = aligned_size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address(), size());
    DCHECK(result);
    USE(result);
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


void VirtualMemory::Reset() {
  address_ = NULL;
  size_ = 0;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return CommitRegion(address, size, is_executable);
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return UncommitRegion(address, size);
}


bool VirtualMemory::Guard(void* address) {
  OS::Guard(address, OS::CommitPageSize());
  return true;
}


void* VirtualMemory::ReserveRegion(size_t size) {
  void* result = mmap(OS::GetRandomMmapAddr(),
                      size,
                      PROT_NONE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_LAZY,
                      kMmapFd,
                      kMmapFdOffset);

  if (result == MAP_FAILED) return NULL;

  return result;
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(base,
                         size,
                         prot,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                         kMmapFd,
                         kMmapFdOffset)) {
    return false;
  }

  return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  return mmap(base,
              size,
              PROT_NONE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_LAZY,
              kMmapFd,
              kMmapFdOffset) != MAP_FAILED;
}


bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  return munmap(base, size) == 0;
}


bool VirtualMemory::HasLazyCommits() {
  return false;
}

}  // namespace base
}  // namespace v8
