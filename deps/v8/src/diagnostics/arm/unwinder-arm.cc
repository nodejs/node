// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8-unwinder-state.h"
#include "include/v8.h"
#include "src/diagnostics/unwinder.h"
#include "src/execution/frame-constants.h"

namespace v8 {

void GetCalleeSavedRegistersFromEntryFrame(void* fp,
                                           RegisterState* register_state) {
  const i::Address base_addr =
      reinterpret_cast<i::Address>(fp) +
      i::EntryFrameConstants::kDirectCallerGeneralRegistersOffset;

  if (!register_state->callee_saved) {
    register_state->callee_saved = std::make_unique<CalleeSavedRegisters>();
  }

  register_state->callee_saved->arm_r4 =
      reinterpret_cast<void*>(Load(base_addr + 0 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r5 =
      reinterpret_cast<void*>(Load(base_addr + 1 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r6 =
      reinterpret_cast<void*>(Load(base_addr + 2 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r7 =
      reinterpret_cast<void*>(Load(base_addr + 3 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r8 =
      reinterpret_cast<void*>(Load(base_addr + 4 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r9 =
      reinterpret_cast<void*>(Load(base_addr + 5 * i::kSystemPointerSize));
  register_state->callee_saved->arm_r10 =
      reinterpret_cast<void*>(Load(base_addr + 6 * i::kSystemPointerSize));
}

}  // namespace v8
