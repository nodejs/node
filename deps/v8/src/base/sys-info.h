// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_SYS_INFO_H_
#define V8_BASE_SYS_INFO_H_

#include <stdint.h>

#include "src/base/base-export.h"
#include "src/base/compiler-specific.h"

namespace v8 {
namespace base {

class V8_BASE_EXPORT SysInfo final {
 public:
  // Returns the number of logical processors/core on the current machine.
  static int NumberOfProcessors();

  // Returns the number of bytes of physical memory on the current machine.
  static int64_t AmountOfPhysicalMemory();

  // Returns the number of bytes of virtual memory of this process. A return
  // value of zero means that there is no limit on the available virtual memory.
  static int64_t AmountOfVirtualMemory();

  // Returns the end of the virtual address space available to this process.
  // Memory mappings at or above this address cannot be addressed by this
  // process, so all pointer values will be below this value.
  // If the virtual address space is not limited, this will return -1.
  static uintptr_t AddressSpaceEnd();
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_SYS_INFO_H_
