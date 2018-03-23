// Copyright (C) 2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom
// the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.
//
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>  // _O_RDWR
#include <limits.h>  // PATH_MAX
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>  // readlink

// The functions in this file map the text segment of node into 2M pages.
// The algorithm is simple
// Find the text region of node binary in memory
// 1: Examine the /proc/self/maps to determine the currently mapped text
// region and obtain the start and end
// Modify the start to point to the very beginning of node text segment
// (from variable nodetext setup in ld.script)
// Align the address of start and end to Large Page Boundaries
//
// 2: Move the text region to large pages
// Map a new area and copy the original code there
// Use mmap using the start address with MAP_FIXED so we get exactly the
// same virtual address
// Use madvise with MADV_HUGE_PAGE to use Anonymous 2M Pages
// If successful copy the code there and unmap the original region.

extern char __nodetext;

namespace node {

struct text_region {
  char* from;
  char* to;
  int   total_hugepages;
  bool  found_text_region;
};

static const size_t hps = 2L * 1024 * 1024;

static void PrintSystemError(int error) {
  fprintf(stderr, "Hugepages WARNING: %s\n", strerror(error));
  return;
}

inline int64_t hugepage_align_up(int64_t addr) {
  return (((addr) + (hps) - 1) & ~((hps) - 1));
}

inline int64_t hugepage_align_down(int64_t addr) {
  return ((addr) & ~((hps) - 1));
}

// The format of the maps file is the following
// address           perms offset  dev   inode       pathname
// 00400000-00452000 r-xp 00000000 08:02 173521      /usr/bin/dbus-daemon
// This is also handling the case where the first line is not the binary

static struct text_region FindNodeTextRegion() {
  std::ifstream ifs;
  std::string map_line;
  std::string permission;
  std::string dev;
  char dash;
  int64_t  start, end, offset, inode;
  struct text_region nregion;

  nregion.found_text_region = false;

  ifs.open("/proc/self/maps");
  if (!ifs) {
    fprintf(stderr, "Could not open /proc/self/maps\n");
    return nregion;
  }

  std::string exename;
  {
      char selfexe[PATH_MAX];
      ssize_t count = readlink("/proc/self/exe", selfexe, PATH_MAX);
      exename = std::string(selfexe, count);
  }

  while (std::getline(ifs, map_line)) {
    std::istringstream iss(map_line);
    iss >> std::hex >> start;
    iss >> dash;
    iss >> std::hex >> end;
    iss >> permission;
    iss >> offset;
    iss >> dev;
    iss >> inode;
    if (inode != 0) {
      std::string pathname;
      iss >> pathname;
      if (pathname == exename && permission == "r-xp") {
        start = reinterpret_cast<uint64_t>(&__nodetext);
        char* from = reinterpret_cast<char*>(hugepage_align_up(start));
        char* to = reinterpret_cast<char*>(hugepage_align_down(end));

        if (from < to) {
          size_t size = to - from;
          nregion.found_text_region = true;
          nregion.from = from;
          nregion.to = to;
          nregion.total_hugepages = size / hps;
        }
        break;
      }
    }
  }

  ifs.close();
  return nregion;
}

static bool IsTransparentHugePagesEnabled() {
  std::ifstream ifs;

  ifs.open("/sys/kernel/mm/transparent_hugepage/enabled");
  if (!ifs) {
    fprintf(stderr, "Could not open file: " \
                    "/sys/kernel/mm/transparent_hugepage/enabled\n");
    return false;
  }

  std::string always, madvise, never;
  if (ifs.is_open()) {
    while (ifs >> always >> madvise >> never) {}
  }

  int ret_status = false;

  if (always.compare("[always]") == 0)
    ret_status = true;
  else if (madvise.compare("[madvise]") == 0)
    ret_status = true;
  else if (never.compare("[never]") == 0)
    ret_status = false;

  ifs.close();
  return ret_status;
}

// Moving the text region to large pages. We need to be very careful.
// 1: This function itself should not be moved.
// We use a gcc attributes
// (__section__) to put it outside the ".text" section
// (__aligned__) to align it at 2M boundary
// (__noline__) to not inline this function
// 2: This function should not call any function(s) that might be moved.
// a. map a new area and copy the original code there
// b. mmap using the start address with MAP_FIXED so we get exactly
//    the same virtual address
// c. madvise with MADV_HUGE_PAGE
// d. If successful copy the code there and unmap the original region
int
__attribute__((__section__(".lpstub")))
__attribute__((__aligned__(hps)))
__attribute__((__noinline__))
MoveTextRegionToLargePages(const text_region& r) {
  void* nmem = nullptr;
  void* tmem = nullptr;
  int ret = 0;

  size_t size = r.to - r.from;
  void* start = r.from;

  // Allocate temporary region preparing for copy
  nmem = mmap(nullptr, size,
              PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (nmem == MAP_FAILED) {
    PrintSystemError(errno);
    return -1;
  }

  memcpy(nmem, r.from, size);

// We already know the original page is r-xp
// (PROT_READ, PROT_EXEC, MAP_PRIVATE)
// We want PROT_WRITE because we are writing into it.
// We want it at the fixed address and we use MAP_FIXED.
  tmem = mmap(start, size,
              PROT_READ | PROT_WRITE | PROT_EXEC,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1 , 0);
  if (tmem == MAP_FAILED) {
    PrintSystemError(errno);
    munmap(nmem, size);
    return -1;
  }

  ret = madvise(tmem, size, MADV_HUGEPAGE);
  if (ret == -1) {
    PrintSystemError(errno);
    ret = munmap(tmem, size);
    if (ret == -1) {
      PrintSystemError(errno);
    }
    ret = munmap(nmem, size);
    if (ret == -1) {
      PrintSystemError(errno);
    }

    return -1;
  }

  memcpy(start, nmem, size);
  ret = mprotect(start, size, PROT_READ | PROT_EXEC);
  if (ret == -1) {
    PrintSystemError(errno);
    ret = munmap(tmem, size);
    if (ret == -1) {
      PrintSystemError(errno);
    }
    ret = munmap(nmem, size);
    if (ret == -1) {
      PrintSystemError(errno);
    }
    return -1;
  }

  // Release the old/temporary mapped region
  ret = munmap(nmem, size);
  if (ret == -1) {
    PrintSystemError(errno);
  }

  return ret;
}

// This is the primary API called from main
int MapStaticCodeToLargePages() {
  struct text_region r = FindNodeTextRegion();
  if (r.found_text_region == false) {
    fprintf(stderr, "Hugepages WARNING: failed to find text region\n");
    return -1;
  }

  if (r.from > reinterpret_cast<void*>(&MoveTextRegionToLargePages))
    return MoveTextRegionToLargePages(r);

  return -1;
}

bool IsLargePagesEnabled() {
  return IsTransparentHugePagesEnabled();
}

}  // namespace node
