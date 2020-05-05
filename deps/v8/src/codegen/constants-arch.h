// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CONSTANTS_ARCH_H_
#define V8_CODEGEN_CONSTANTS_ARCH_H_

#if V8_TARGET_ARCH_ARM
#include "src/codegen/arm/constants-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/codegen/arm64/constants-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/constants-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/codegen/mips/constants-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/codegen/mips64/constants-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/codegen/ppc/constants-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/codegen/s390/constants-s390.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/codegen/x64/constants-x64.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

#endif  // V8_CODEGEN_CONSTANTS_ARCH_H_
