// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/utils.h"

#include "src/base/platform/platform.h"
#include "src/flags/flags.h"

namespace v8::internal::compiler::turboshaft {

#ifdef DEBUG
bool ShouldSkipOptimizationStep() {
  static std::atomic<uint64_t> counter{0};
  uint64_t current = counter++;
  if (current == v8_flags.turboshaft_opt_bisect_break) {
    base::OS::DebugBreak();
  }
  if (current >= v8_flags.turboshaft_opt_bisect_limit) {
    return true;
  }
  return false;
}
#endif  // DEBUG

}  // namespace v8::internal::compiler::turboshaft
