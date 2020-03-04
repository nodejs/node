// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for FreeBSD goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#include <sys/fcntl.h>  // open
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <unistd.h>     // getpagesize
// If you don't have execinfo.h then you need devel/libexecinfo from ports.
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <strings.h>    // index

#include <cmath>

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

static unsigned StringToLong(char* buffer) {
  return static_cast<unsigned>(strtol(buffer, nullptr, 16));  // NOLINT
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
    ssize_t bytes_read = read(fd, addr_buffer + 2, 8);
    if (bytes_read < 8) break;
    unsigned start = StringToLong(addr_buffer);
    bytes_read = read(fd, addr_buffer + 2, 1);
    if (bytes_read < 1) break;
    if (addr_buffer[2] != '-') break;
    bytes_read = read(fd, addr_buffer + 2, 8);
    if (bytes_read < 8) break;
    unsigned end = StringToLong(addr_buffer);
    char buffer[MAP_LENGTH];
    bytes_read = -1;
    do {
      bytes_read++;
      if (bytes_read >= MAP_LENGTH - 1) break;
      bytes_read = read(fd, buffer + bytes_read, 1);
      if (bytes_read < 1) break;
    } while (buffer[bytes_read] != '\n');
    buffer[bytes_read] = 0;
    // Ignore mappings that are not executable.
    if (buffer[3] != 'x') continue;
    char* start_of_path = index(buffer, '/');
    // There may be no filename in this line.  Skip to next.
    if (start_of_path == nullptr) continue;
    buffer[bytes_read] = 0;
    result.push_back(SharedLibraryAddress(start_of_path, start, end));
  }
  close(fd);
  return result;
}

void OS::SignalCodeMovingGC() {}

void OS::AdjustSchedulingParams() {}

}  // namespace base
}  // namespace v8
