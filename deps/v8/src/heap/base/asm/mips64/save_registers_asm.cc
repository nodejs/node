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

// 9 64-bit registers = 9 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters() == 9,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(".text                                        \n"
    ".set noreorder                               \n"
    ".global SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function    \n"
    ".hidden SaveCalleeSavedRegisters             \n"
    "SaveCalleeSavedRegisters:                    \n"
    // $a0: [ intptr_t* buffer ]
    // Save the callee-saved registers.
    "  sd $gp, 64($a0)                            \n"
    "  sd $s7, 56($a0)                            \n"
    "  sd $s6, 48($a0)                            \n"
    "  sd $s5, 40($a0)                            \n"
    "  sd $s4, 32($a0)                            \n"
    "  sd $s3, 24($a0)                            \n"
    "  sd $s2, 16($a0)                            \n"
    "  sd $s1,  8($a0)                            \n"
    // ... one more in the delay slot!
    // Return.
    "  jr $ra                                     \n"
    // Delay slot:
    "  sd $s0,  0($a0)                            \n");
