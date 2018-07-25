// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASSEMBLER_ARCH_H_
#define V8_ASSEMBLER_ARCH_H_

#include "src/assembler.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/assembler-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/assembler-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/assembler-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/assembler-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/assembler-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/assembler-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/s390/assembler-s390.h"
#else
#error Unknown architecture.
#endif

#endif  // V8_ASSEMBLER_ARCH_H_
