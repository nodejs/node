// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-zifencei.h"

#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-zifencei.h"

namespace v8 {
namespace internal {

void AssemblerRISCVZifencei::fence_i() {
  GenInstrI(0b001, MISC_MEM, ToRegister(0), ToRegister(0), 0);
}
}  // namespace internal
}  // namespace v8
