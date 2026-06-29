// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CODE_GEN_STATE_INL_H_
#define V8_MAGLEV_MAGLEV_CODE_GEN_STATE_INL_H_

#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"

namespace v8 {
namespace internal {
namespace maglev {

inline BasicBlock* MaglevCodeGenState::RealJumpTarget(BasicBlock* block) {
  if (!real_jump_target_[block->id()]) {
    real_jump_target_[block->id()] = block->ComputeRealJumpTarget();
  }
  return real_jump_target_[block->id()];
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_CODE_GEN_STATE_INL_H_
