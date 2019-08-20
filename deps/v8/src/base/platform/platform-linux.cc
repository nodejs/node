// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for Linux goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

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
#include <strings.h>    // index
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // sysconf

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

#ifdef __arm__

bool OS::ArmUsingHardFloat() {
// GCC versions 4.6 and above define __ARM_PCS or __ARM_PCS_VFP to specify
// the Floating Point ABI used (PCS stands for Procedure Call Standard).
// We use these as well as a couple of other defines to statically determine
// what FP ABI used.
// GCC versions 4.4 and below don't support hard-fp.
// GCC versions 4.5 may support hard-fp without defining __ARM_PCS or
// __ARM_PCS_VFP.

#define GCC_VERSION \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40600 && !defined(__clang__)
#if defined(__ARM_PCS_VFP)
  return true;
#else
  return false;
#endif

#elif GCC_VERSION < 40500 && !defined(__clang__)
  return false;

#else
#if defined(__ARM_PCS_VFP)
  return true;
#elif defined(__ARM_PCS) || defined(__SOFTFP__) || defined(__SOFTFP) || \
    !defined(__VFP_FP__)
  return false;
#else
#error \
    "Your version of compiler does not report the FP ABI compiled for."     \
       "Please report it on this issue"                                        \
       "http://code.google.com/p/v8/issues/detail?id=2140"

#endif
#endif
#undef GCC_VERSION
}

#endif  // def __arm__

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  // This function assumes that the layout of the file is as follows:
  // hex_start_addr-hex_end_addr rwxp <unused data> [binary_file_name]
  // If we encounter an unexpected situation we abort scanning further entries.
  FILE* fp = fopen("/proc/self/maps", "r");
  if (fp == nullptr) return result;

  // Allocate enough room to be able to store a full file name.
  const int kLibNameLen = FILENAME_MAX + 1;
  char* lib_name = reinterpret_cast<char*>(malloc(kLibNameLen));

  // This loop will terminate once the scanning hits an EOF.
  while (true) {
    uintptr_t start, end, offset;
    char attr_r, attr_w, attr_x, attr_p;
    // Parse the addresses and permission bits at the beginning of the line.
    if (fscanf(fp, "%" V8PRIxPTR "-%" V8PRIxPTR, &start, &end) != 2) break;
    if (fscanf(fp, " %c%c%c%c", &attr_r, &attr_w, &attr_x, &attr_p) != 4) break;
    if (fscanf(fp, "%" V8PRIxPTR, &offset) != 1) break;

    int c;
    if (attr_r == 'r' && attr_w != 'w' && attr_x == 'x') {
      // Found a read-only executable entry. Skip characters until we reach
      // the beginning of the filename or the end of the line.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n') && (c != '/') && (c != '['));
      if (c == EOF) break;  // EOF: Was unexpected, just exit.

      // Process the filename if found.
      if ((c == '/') || (c == '[')) {
        // Push the '/' or '[' back into the stream to be read below.
        ungetc(c, fp);

        // Read to the end of the line. Exit if the read fails.
        if (fgets(lib_name, kLibNameLen, fp) == nullptr) break;

        // Drop the newline character read by fgets. We do not need to check
        // for a zero-length string because we know that we at least read the
        // '/' or '[' character.
        lib_name[strlen(lib_name) - 1] = '\0';
      } else {
        // No library name found, just record the raw address range.
        snprintf(lib_name, kLibNameLen, "%08" V8PRIxPTR "-%08" V8PRIxPTR, start,
                 end);
      }

#ifdef V8_OS_ANDROID
      size_t lib_name_length = strlen(lib_name);
      if (lib_name_length < 4 ||
          strncmp(&lib_name[lib_name_length - 4], ".apk", 4) != 0) {
        // Only adjust {start} based on {offset} if the file isn't the APK,
        // since we load the library directly from the APK and don't want to
        // apply the offset of the .so in the APK as the libraries offset.
        start -= offset;
      }
#else
      // Adjust {start} based on {offset}.
      start -= offset;
#endif

      result.push_back(SharedLibraryAddress(lib_name, start, end));
    } else {
      // Entry not describing executable data. Skip to end of line to set up
      // reading the next entry.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n'));
      if (c == EOF) break;
    }
  }
  free(lib_name);
  fclose(fp);
  return result;
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
  CHECK(Free(addr, size));
  fclose(f);
}

void OS::AdjustSchedulingParams() {}

}  // namespace base
}  // namespace v8
