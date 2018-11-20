// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_SYS_INFO_H_
#define V8_BASE_SYS_INFO_H_

#include <stdint.h>
#include "src/base/compiler-specific.h"

namespace v8 {
namespace base {

class SysInfo FINAL {
 public:
  // Returns the number of logical processors/core on the current machine.
  static int NumberOfProcessors();

  // Returns the number of bytes of physical memory on the current machine.
  static int64_t AmountOfPhysicalMemory();

  // Returns the number of bytes of virtual memory of this process. A return
  // value of zero means that there is no limit on the available virtual memory.
  static int64_t AmountOfVirtualMemory();
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_SYS_INFO_H_
