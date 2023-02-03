// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <src/heap/base/stack.h>

// Save all callee-saved registers in the specified buffer.
// extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

// See asm/x64/save_registers_asm.cc for why the function is not generated
// using clang.
//
// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.

// S390 ABI source:
// http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_zSeries.html

// 10 64-bit registers = 10 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters == 10,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(".globl SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function   \n"
    ".hidden SaveCalleeSavedRegisters            \n"
    "SaveCalleeSavedRegisters:                   \n"
    // r2: [ intptr_t* buffer ]
    // Save the callee-saved registers: r6-r13, r14 and sp(r15).
    "  stmg %r6, %sp, 0(%r2)                     \n"
    // Return.
    "  br %r14                                   \n");
