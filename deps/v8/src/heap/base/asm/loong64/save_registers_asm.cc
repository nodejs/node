// Copyright 2021 the V8 project authors. All rights reserved.
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

// 11 64-bit registers = 11 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters() == 11,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(".text                                        \n"
    ".global SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function    \n"
    ".hidden SaveCalleeSavedRegisters             \n"
    "SaveCalleeSavedRegisters:                    \n"
    // $a0: [ intptr_t* buffer ]
    // Save the callee-saved registers.
    "  st.d $s8, $a0, 0                           \n"
    "  st.d $sp, $a0, 8                           \n"
    "  st.d $fp, $a0, 16                          \n"
    "  st.d $s7, $a0, 24                          \n"
    "  st.d $s6, $a0, 32                          \n"
    "  st.d $s5, $a0, 40                          \n"
    "  st.d $s4, $a0, 48                          \n"
    "  st.d $s3, $a0, 56                          \n"
    "  st.d $s2, $a0, 64                          \n"
    "  st.d $s1, $a0, 72                          \n"
    "  st.d $s0, $a0, 80                          \n"
    // Return.
    "  jirl $zero, $ra, 0                         \n");
