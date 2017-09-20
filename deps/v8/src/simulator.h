// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SIMULATOR_H_
#define V8_SIMULATOR_H_

#if V8_TARGET_ARCH_IA32
#include "src/ia32/simulator-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/simulator-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/simulator-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/simulator-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/simulator-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/simulator-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/simulator-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/s390/simulator-s390.h"
#else
#error Unsupported target architecture.
#endif

#endif  // V8_SIMULATOR_H_
