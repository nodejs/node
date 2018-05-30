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

// The functions in this file map the text segment of node into 2M pages.
// The algorithm is quite simple
// 1. Find the text region of node binary in memory
// 2. Move the text region to large pages

extern char __executable_start;
extern char __etext;
extern char __nodetext;

namespace node {
namespace largepages {
#define ALIGN(x, a)           (((x) + (a) - 1) & ~((a) - 1))
#define PAGE_ALIGN_UP(x, a)   ALIGN(x, a)
#define PAGE_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

struct TextRegion {
  void * from;
  void * to;
  int    totalHugePages;
  int64_t  offset;
  bool   found_text_region;
};

    static void printSystemError(int error) {
      fprintf(stderr, "Hugepages WARNING: %s\n", strerror(error));
      return;
    }

    static struct TextRegion find_node_text_region() {
      FILE *f;
      int64_t  start, end, offset, inode;
      char perm[5], dev[6], name[256];
      int ret;
      const size_t hugePageSize = 2L * 1024 * 1024;
      struct TextRegion nregion;

      nregion.found_text_region = false;

      f = fopen("/proc/self/maps", "r");
      ret = fscanf(f, "%lx-%lx %4s %lx %5s %ld %s\n",
                   &start, &end, perm, &offset, dev, &inode, name);
      fclose(f);

      if (ret == 7 &&
          perm[0] == 'r' && perm[1] == '-' && perm[2] == 'x') {
        // Checking if the region is from node binary and executable
        start = (unsigned int64_t) &__nodetext;
        char *from = reinterpret_cast<char *>PAGE_ALIGN_UP(start, hugePageSize);
        char *to = reinterpret_cast<char *>PAGE_ALIGN_DOWN(end, hugePageSize);

        if (from < to) {
          size_t size = (intptr_t)to - (intptr_t)from;
          nregion.found_text_region = true;
          nregion.from = from;
          nregion.to = to;
          nregion.offset = offset;
          nregion.totalHugePages = size/hugePageSize;
          return nregion;
        }
      }

      return nregion;
    }

    static bool isTransparentHugePagesEnabled() {
       std::ifstream ifs;

       ifs.open("/sys/kernel/mm/transparent_hugepage/enabled");
       if (!ifs) {
         fprintf(stderr, "WARNING: Couldn't check hugepages support\n");
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

    static bool isExplicitHugePagesEnabled() {
     int ret_status = false;
     std::string kw;
     std::ifstream file("/proc/meminfo");
     while (file >> kw) {
        if (kw == "HugePages_Total:") {
          int64_t hp_tot;
          file >> hp_tot;
          if (hp_tot > 0)
            ret_status = true;
          else
            ret_status = false;

          break;
        }
     }

     file.close();
     return ret_status;
    }

    //  Moving the text region to large pages. We need to be very careful.
    //  a) This function itself should not be moved.
    //     We use a gcc option to put it outside the ".text" section
    //  b) This function should not call any function(s) that might be moved.
    //    1. We map a new area and copy the original code there
    //    2. We mmap using HUGE_TLB
    //    3. If successful we copy the code there and unmap the original region.
    int
      __attribute__((__section__(".eh_frame")))
      __attribute__((__aligned__(2 * 1024 * 1024)))
      __attribute__((__noinline__))
      __attribute__((__optimize__("2")))
      move_text_region_to_large_pages(struct TextRegion r) {
        void *nmem = nullptr, *tmem = nullptr;
        int ret = 0;

        size_t size = (intptr_t)r.to - (intptr_t)r.from;
        void *start = r.from;

        // Allocate temporary region preparing for copy
        nmem = mmap(nullptr, size,
                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (nmem == MAP_FAILED) {
          printSystemError(errno);
          return -1;
        }

        memcpy(nmem, r.from, size);

        // use for transparent huge pages if enabled
        if (isTransparentHugePagesEnabled()) {
          tmem = mmap(start, size,
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1 , 0);
          if (tmem == MAP_FAILED) {
            printSystemError(errno);
            munmap(nmem, size);
            return -1;
          }

          if (tmem != start) {
            fprintf(stderr, "Unable to allocate hugepages.n");
            munmap(nmem, size);
            munmap(tmem, size);
            return -1;
          }

          ret = madvise(tmem, size, MADV_HUGEPAGE);
          if (ret == -1) {
            printSystemError(errno);
            ret = munmap(tmem, size);
            if (ret == -1) {
              printSystemError(errno);
            }
            ret = munmap(nmem, size);
            if (ret == -1) {
              printSystemError(errno);
            }

            return -1;
          }
        }

        memcpy(start, nmem, size);
        ret = mprotect(start, size, PROT_READ | PROT_EXEC);
        if (ret == -1) {
          printSystemError(errno);
          ret = munmap(tmem, size);
          if (ret == -1) {
            printSystemError(errno);
          }
          ret = munmap(nmem, size);
          if (ret == -1) {
            printSystemError(errno);
          }
          return -1;
        }

        // Release the old/temporary mapped region
        ret = munmap(nmem, size);
        if (ret == -1) {
          printSystemError(errno);
        }

        return ret;
    }

    // This is the primary API called from main
    int map_static_code_to_large_pages() {
      struct TextRegion n = find_node_text_region();
      if (n.found_text_region == false) {
        fprintf(stderr, "Hugepages WARNING: failed to map static code\n");
        return -1;
      }

      if (n.to <= reinterpret_cast<void *> (&move_text_region_to_large_pages))
        return move_text_region_to_large_pages(n);

      return -1;
    }

    bool isLargePagesEnabled() {
      return isExplicitHugePagesEnabled() || isTransparentHugePagesEnabled();
    }

}  // namespace largepages
}  // namespace node
