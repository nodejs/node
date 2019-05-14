// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGISTER_ARCH_H_
#define V8_REGISTER_ARCH_H_

#include "src/register.h"
#include "src/reglist.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/register-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/register-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/register-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/register-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/register-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/register-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/register-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/s390/register-s390.h"
#else
#error Unknown architecture.
#endif

#endif  // V8_REGISTER_ARCH_H_
