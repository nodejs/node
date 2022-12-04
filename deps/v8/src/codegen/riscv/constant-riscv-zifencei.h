// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIFENCEI_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIFENCEI_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {
enum OpcodeRISCVIFENCEI : uint32_t {
  RO_FENCE_I = MISC_MEM | (0b001 << kFunct3Shift),
};
}
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIFENCEI_H_
