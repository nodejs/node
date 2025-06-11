// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANTS_RISCV_H_
#define V8_CODEGEN_RISCV_CONSTANTS_RISCV_H_
#include "src/codegen/riscv/base-constants-riscv.h"
#include "src/codegen/riscv/constant-riscv-a.h"
#include "src/codegen/riscv/constant-riscv-b.h"
#include "src/codegen/riscv/constant-riscv-c.h"
#include "src/codegen/riscv/constant-riscv-d.h"
#include "src/codegen/riscv/constant-riscv-f.h"
#include "src/codegen/riscv/constant-riscv-i.h"
#include "src/codegen/riscv/constant-riscv-m.h"
#include "src/codegen/riscv/constant-riscv-v.h"
#include "src/codegen/riscv/constant-riscv-zfh.h"
#include "src/codegen/riscv/constant-riscv-zicsr.h"
#include "src/codegen/riscv/constant-riscv-zifencei.h"
namespace v8 {
namespace internal {
// The maximum size of the stack restore after a fast API call that pops the
// stack parameters of the call off the stack.
constexpr int kMaxSizeOfMoveAfterFastCall = 2 * kInstrSize;
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_CONSTANTS_RISCV_H_
