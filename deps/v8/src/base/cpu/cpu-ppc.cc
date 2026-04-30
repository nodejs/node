// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/cpu/cpu.h"

#if V8_OS_LINUX
#include <linux/auxvec.h>  // AT_HWCAP
#include <sys/auxv.h>      // getauxval()
#endif
#if V8_OS_LINUX && V8_HOST_ARCH_PPC64
#include <elf.h>
#endif
#if V8_OS_AIX
#include <sys/systemcfg.h>  // _system_configuration
#ifndef POWER_9
#define POWER_9 0x20000
#endif
#ifndef POWER_10
#define POWER_10 0x40000
#endif
#endif
#if V8_OS_POSIX
#include <unistd.h>  // sysconf()
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"

namespace v8::base {

void CPU::DetectFeatures() {
#ifndef USE_SIMULATOR
#if V8_OS_LINUX
  // Read processor info from /proc/self/auxv.
  char* auxv_cpu_type = nullptr;
  FILE* fp = base::Fopen("/proc/self/auxv", "r");
  if (fp != nullptr) {
    Elf64_auxv_t entry;
    for (;;) {
      size_t n = fread(&entry, sizeof(entry), 1, fp);
      if (n == 0 || entry.a_type == AT_NULL) {
        break;
      }
      switch (entry.a_type) {
        case AT_PLATFORM:
          auxv_cpu_type = reinterpret_cast<char*>(entry.a_un.a_val);
          break;
        case AT_ICACHEBSIZE:
          icache_line_size_ = entry.a_un.a_val;
          break;
        case AT_DCACHEBSIZE:
          dcache_line_size_ = entry.a_un.a_val;
          break;
      }
    }
    base::Fclose(fp);
  }

  part_ = -1;
  if (auxv_cpu_type) {
    if (strcmp(auxv_cpu_type, "power11") == 0) {
      part_ = kPPCPower11;
    } else if (strcmp(auxv_cpu_type, "power10") == 0) {
      part_ = kPPCPower10;
    } else if (strcmp(auxv_cpu_type, "power9") == 0) {
      part_ = kPPCPower9;
    }
  }

#elif V8_OS_AIX
  switch (_system_configuration.implementation) {
    case POWER_10:
      part_ = kPPCPower10;
      break;
    case POWER_9:
      part_ = kPPCPower9;
      break;
  }
#endif  // V8_OS_AIX
#endif  // !USE_SIMULATOR
}

}  // namespace v8::base
