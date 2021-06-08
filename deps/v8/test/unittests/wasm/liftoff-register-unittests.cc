// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler-defs.h"
#if V8_TARGET_ARCH_IA32
#include "src/execution/ia32/frame-constants-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/execution/x64/frame-constants-x64.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/execution/mips/frame-constants-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/execution/mips64/frame-constants-mips64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/execution/arm/frame-constants-arm.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/execution/arm64/frame-constants-arm64.h"
#elif V8_TARGET_ARCH_S390X
#include "src/execution/s390/frame-constants-s390.h"
#elif V8_TARGET_ARCH_PPC64
#include "src/execution/ppc/frame-constants-ppc.h"
#elif V8_TARGET_ARCH_RISCV64
#include "src/execution/riscv64/frame-constants-riscv64.h"
#endif

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace wasm {

// The registers used by Liftoff and the registers spilled by the
// WasmDebugBreak builtin should match.
STATIC_ASSERT(kLiftoffAssemblerGpCacheRegs ==
              WasmDebugBreakFrameConstants::kPushedGpRegs);

STATIC_ASSERT(kLiftoffAssemblerFpCacheRegs ==
              WasmDebugBreakFrameConstants::kPushedFpRegs);
}  // namespace wasm
}  // namespace internal
}  // namespace v8
