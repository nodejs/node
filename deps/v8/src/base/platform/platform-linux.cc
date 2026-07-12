// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for Linux goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#include "src/base/platform/platform-linux.h"

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>

// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <errno.h>
#include <fcntl.h>  // open
#include <stdarg.h>
#include <strings.h>   // index
#include <sys/mman.h>  // mmap & munmap & mremap
#include <sys/stat.h>  // open
#include <sys/sysmacros.h>
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // sysconf

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>

#include "src/base/logging.h"
#include "src/base/memory.h"

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

void OS::SignalCodeMovingGC() {
  // Support for ll_prof.py.
  //
  // The Linux profiler built into the kernel logs all mmap's with
  // PROT_EXEC so that analysis tools can properly attribute ticks. We
  // do a mmap with a name known by ll_prof.py and immediately munmap
  // it. This injects a GC marker into the stream of events generated
  // by the kernel and allows us to synchronize V8 code log and the
  // kernel log.
  long size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  FILE* f = fopen(OS::GetGCFakeMMapFile(), "w+");
  if (f == nullptr) {
    OS::PrintError("Failed to open %s\n", OS::GetGCFakeMMapFile());
    OS::Abort();
  }
  void* addr = mmap(OS::GetRandomMmapAddr(), size, PROT_READ | PROT_EXEC,
                    MAP_PRIVATE, fileno(f), 0);
  DCHECK_NE(MAP_FAILED, addr);
  Free(addr, size);
  fclose(f);
}

void OS::AdjustSchedulingParams() {}

void* OS::RemapShared(void* old_address, void* new_address, size_t size) {
  void* result =
      mremap(old_address, 0, size, MREMAP_FIXED | MREMAP_MAYMOVE, new_address);

  if (result == MAP_FAILED) {
    return nullptr;
  }
  DCHECK(result == new_address);
  return result;
}

std::optional<OS::MemoryRange> OS::GetFirstFreeMemoryRangeWithin(
    OS::Address boundary_start, OS::Address boundary_end, size_t minimum_size,
    size_t alignment) {
  std::optional<OS::MemoryRange> result;
  SignalSafeMapsParser parser;
  if (!parser.IsValid()) return {};

  // Search for the gaps between existing virtual memory (vm) areas. If the gap
  // contains enough space for the requested-size range that is within the
  // boundary, push the overlapped memory range to the vector.
  uintptr_t gap_start = 0;
  // This loop will terminate once the scanning hits an EOF or reaches the gap
  // at the higher address to the end of boundary.
  while (auto entry = parser.Next()) {
    // Visit the gap at the lower address to this vm.
    uintptr_t gap_end = entry->start;
    // Skip the gaps at the lower address to the start of boundary.
    if (gap_end > boundary_start) {
      // The available area is the overlap of the gap and boundary. Push
      // the overlapped memory range to the vector if there is enough space.
      const uintptr_t overlap_start =
          RoundUp(std::max(gap_start, boundary_start), alignment);
      const uintptr_t overlap_end =
          RoundDown(std::min(gap_end, boundary_end), alignment);
      if (overlap_start < overlap_end &&
          overlap_end - overlap_start >= minimum_size) {
        result = {overlap_start, overlap_end};
        break;
      }
    }
    // Continue to visit the next gap.
    gap_start = entry->end;
    if (gap_start >= boundary_end) break;
  }

  return result;
}

//  static
namespace {
// Parses /proc/self/maps.
std::unique_ptr<std::vector<MemoryRegion>> ParseProcSelfMaps(
    FILE* fp, std::function<bool(const MemoryRegion&)> predicate,
    bool early_stopping) {
  auto result = std::make_unique<std::vector<MemoryRegion>>();

  // Create parser. If fp is provided, use its fd.
  // Note: we must not close the fd if it belongs to fp.
  int fd = fp ? fileno(fp) : -1;
  SignalSafeMapsParser parser(fd, /*should_close_fd=*/fp == nullptr);
  if (!parser.IsValid()) return nullptr;

  while (auto region = parser.Next()) {
    if (predicate(*region)) {
      result->push_back(std::move(*region));
      if (early_stopping) break;
    }
  }

  if (!result->empty()) return result;

  return nullptr;
}

MemoryRegion FindEnclosingMapping(uintptr_t target_start, size_t size) {
  auto result = ParseProcSelfMaps(
      nullptr,
      [=](const MemoryRegion& region) {
        return region.start <= target_start && target_start + size < region.end;
      },
      true);
  if (result)
    return (*result)[0];
  else
    return {};
}
}  // namespace

// static
std::vector<OS::SharedLibraryAddress> GetSharedLibraryAddresses(FILE* fp) {
  auto regions = ParseProcSelfMaps(
      fp,
      [](const MemoryRegion& region) {
        return region.permissions == PagePermissions::kReadExecute;
      },
      false);

  if (!regions) return {};

  std::vector<OS::SharedLibraryAddress> result;
  for (const MemoryRegion& region : *regions) {
    uintptr_t start = region.start;
#ifdef V8_OS_ANDROID
    size_t len = strlen(region.pathname);
    if (len < 4 || strcmp(region.pathname + len - 4, ".apk") != 0) {
      // Only adjust {start} based on {offset} if the file isn't the APK,
      // since we load the library directly from the APK and don't want to
      // apply the offset of the .so in the APK as the libraries offset.
      start -= region.offset;
    }
#else
    start -= region.offset;
#endif
    result.emplace_back(region.pathname, start, region.end);
  }
  return result;
}

// static
std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  return ::v8::base::GetSharedLibraryAddresses(nullptr);
}

// static
bool OS::RemapPages(const void* address, size_t size, void* new_address,
                    MemoryPermission access) {
  uintptr_t address_addr = reinterpret_cast<uintptr_t>(address);

  DCHECK(IsAligned(address_addr, AllocatePageSize()));
  DCHECK(
      IsAligned(reinterpret_cast<uintptr_t>(new_address), AllocatePageSize()));
  DCHECK(IsAligned(size, AllocatePageSize()));

  MemoryRegion enclosing_region = FindEnclosingMapping(address_addr, size);
  // Not found.
  if (!enclosing_region.start) return false;

  // Anonymous mapping?
  if (strlen(enclosing_region.pathname) == 0) return false;

  // Since the file is already in use for executable code, this is most likely
  // to fail due to sandboxing, e.g. if open() is blocked outright.
  //
  // In Chromium on Android, the sandbox allows openat() but prohibits
  // open(). However, the libc uses openat() in its open() wrapper, and the
  // SELinux restrictions allow us to read from the path we want to look at,
  // so we are in the clear.
  //
  // Note that this may not be allowed by the sandbox on Linux (and Chrome
  // OS). On these systems, consider using mremap() with the MREMAP_DONTUNMAP
  // flag. However, since we need it on non-anonymous mapping, this would only
  // be available starting with version 5.13.
  int fd = open(enclosing_region.pathname, O_RDONLY);
  if (fd == -1) return false;

  // Now we have a file descriptor to the same path the data we want to remap
  // comes from. But... is it the *same* file? This is not guaranteed (e.g. in
  // case of updates), so to avoid hard-to-track bugs, check that the
  // underlying file is the same using the device number and the inode. Inodes
  // are not unique across filesystems, and can be reused. The check works
  // here though, since we have the problems:
  // - Inode uniqueness: check device numbers.
  // - Inode reuse: the initial file is still open, since we are running code
  //   from it. So its inode cannot have been reused.
  struct stat stat_buf;
  if (fstat(fd, &stat_buf)) {
    close(fd);
    return false;
  }

  // Not the same file.
  if (stat_buf.st_dev != enclosing_region.dev ||
      stat_buf.st_ino != enclosing_region.inode) {
    close(fd);
    return false;
  }

  size_t offset_in_mapping = address_addr - enclosing_region.start;
  size_t offset_in_file = enclosing_region.offset + offset_in_mapping;
  int protection = GetProtectionFromMemoryPermission(access);

  void* mapped_address = mmap(new_address, size, protection,
                              MAP_FIXED | MAP_PRIVATE, fd, offset_in_file);
  // mmap() keeps the file open.
  close(fd);

  if (mapped_address != new_address) {
    // Should not happen, MAP_FIXED should always map where we want.
    UNREACHABLE();
  }

  return true;
}

SignalSafeMapsParser::SignalSafeMapsParser(int fd, bool should_close_fd)
    : fd_(fd >= 0 ? fd : open("/proc/self/maps", O_RDONLY)),
      should_close_fd_(fd >= 0 ? should_close_fd : true),
      buffer_pos_(0),
      buffer_end_(0) {}

SignalSafeMapsParser::~SignalSafeMapsParser() {
  if (should_close_fd_ && fd_ >= 0) close(fd_);
}

std::optional<MemoryRegion> SignalSafeMapsParser::Next() {
  CHECK(IsValid());

  // The maps file consists of the following kind of lines:
  // 55ac243aa000-55ac243ac000 r--p 00000000 fe:01 31594735 /usr/bin/foo

  MemoryRegion entry;
  char delim;
  if (!ReadHex(&entry.start, &delim)) return std::nullopt;
  if (delim != '-') return std::nullopt;
  if (!ReadHex(&entry.end, &delim)) return std::nullopt;
  if (delim != ' ') return std::nullopt;

  for (int i = 0; i < 4; ++i) {
    if (!ReadChar(&entry.raw_permissions[i])) return std::nullopt;
  }
  entry.raw_permissions[4] = '\0';

  entry.permissions = PagePermissions::kNoAccess;
  if (entry.raw_permissions[0] == 'r')
    entry.permissions |= PagePermissions::kRead;
  if (entry.raw_permissions[1] == 'w')
    entry.permissions |= PagePermissions::kWrite;
  if (entry.raw_permissions[2] == 'x')
    entry.permissions |= PagePermissions::kExecute;

  char c;
  if (!ReadChar(&c)) return std::nullopt;
  if (c != ' ') return std::nullopt;

  if (!ReadHex(&entry.offset, &delim)) return std::nullopt;
  if (delim != ' ') return std::nullopt;

  uintptr_t major, minor;
  if (!ReadHex(&major, &delim)) return std::nullopt;
  if (delim != ':') return std::nullopt;
  if (!ReadHex(&minor, &delim)) return std::nullopt;
  if (delim != ' ') return std::nullopt;
  entry.dev = makedev(static_cast<unsigned int>(major),
                      static_cast<unsigned int>(minor));

  uintptr_t inode = 0;
  if (!ReadDecimal(&inode, &delim)) return std::nullopt;
  entry.inode = static_cast<ino_t>(inode);

  // Skip spaces.
  while (delim == ' ') {
    if (!ReadChar(&delim)) break;
  }

  // delim is now the first char of the pathname or newline.
  char current_char = delim;
  size_t path_len = 0;
  while (current_char != '\n') {
    if (path_len < MemoryRegion::kMaxPathnameSize - 1) {
      entry.pathname[path_len++] = current_char;
    }
    if (!ReadChar(&current_char)) break;
  }
  entry.pathname[path_len] = '\0';

  return entry;
}

bool SignalSafeMapsParser::ReadChar(char* out) {
  if (buffer_pos_ >= buffer_end_) {
    buffer_pos_ = 0;
    ssize_t bytes = read(fd_, buffer_, kBufferSize);
    if (bytes <= 0) return false;
    buffer_end_ = bytes;
  }
  *out = buffer_[buffer_pos_++];
  return true;
}

bool SignalSafeMapsParser::ReadHex(uintptr_t* out_val, char* out_delim) {
  *out_val = 0;
  while (true) {
    char c;
    if (!ReadChar(&c)) return false;
    if (c >= '0' && c <= '9') {
      *out_val = (*out_val << 4) | (c - '0');
    } else if (c >= 'a' && c <= 'f') {
      *out_val = (*out_val << 4) | (c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      *out_val = (*out_val << 4) | (c - 'A' + 10);
    } else {
      *out_delim = c;
      return true;
    }
  }
}

bool SignalSafeMapsParser::ReadDecimal(uintptr_t* out_val, char* out_delim) {
  *out_val = 0;
  while (true) {
    char c;
    if (!ReadChar(&c)) return false;
    if (c >= '0' && c <= '9') {
      *out_val = (*out_val * 10) + (c - '0');
    } else {
      *out_delim = c;
      return true;
    }
  }
}

}  // namespace base
}  // namespace v8
