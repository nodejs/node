// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CONSTANT_POOL_H_
#define V8_CODEGEN_CONSTANT_POOL_H_

#if V8_TARGET_ARCH_ARM64
#include "src/codegen/arm64/constant-pool-arm64.h"
#elif V8_TARGET_ARCH_PPC64
#include "src/codegen/ppc/constant-pool-ppc.h"
#elif V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64
#include "src/codegen/riscv/constant-pool-riscv.h"
#endif

#endif  // V8_CODEGEN_CONSTANT_POOL_H_
