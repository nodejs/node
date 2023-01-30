// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <src/heap/base/stack.h>

// Save all callee-saved registers in the specified buffer.
// extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

// See asm/x64/save_registers_asm.cc for why the function is not generated
// using clang.
//
// Calling convention source:
// https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf Table 18.2

#if V8_HOST_ARCH_RISCV64
// 12 64-bit registers = 12 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters() == 12,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(".global SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function    \n"
    ".hidden SaveCalleeSavedRegisters             \n"
    "SaveCalleeSavedRegisters:                    \n"
    // a0: [ intptr_t* buffer ]
    // Save the callee-saved registers: s0-s11.
    "  sd s11, 88(a0)                             \n"
    "  sd s10, 80(a0)                             \n"
    "  sd s9, 72(a0)                              \n"
    "  sd s8, 64(a0)                              \n"
    "  sd s7, 56(a0)                              \n"
    "  sd s6, 48(a0)                              \n"
    "  sd s5, 40(a0)                              \n"
    "  sd s4, 32(a0)                              \n"
    "  sd s3, 24(a0)                              \n"
    "  sd s2, 16(a0)                              \n"
    "  sd s1,  8(a0)                              \n"
    "  sd s0,  0(a0)                              \n"
    // Return.
    "  jr ra                                      \n");
#elif V8_HOST_ARCH_RISCV32
// 12 32-bit registers = 12 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters() == 12,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 4, "Mismatch in word size");

asm(".global SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function    \n"
    ".hidden SaveCalleeSavedRegisters             \n"
    "SaveCalleeSavedRegisters:                    \n"
    // a0: [ intptr_t* buffer ]
    // Save the callee-saved registers: s0-s11.
    "  sw s11, 44(a0)                             \n"
    "  sw s10, 40(a0)                             \n"
    "  sw s9, 36(a0)                              \n"
    "  sw s8, 32(a0)                              \n"
    "  sw s7, 28(a0)                              \n"
    "  sw s6, 24(a0)                              \n"
    "  sw s5, 20(a0)                              \n"
    "  sw s4, 16(a0)                              \n"
    "  sw s3, 12(a0)                              \n"
    "  sw s2,  8(a0)                              \n"
    "  sw s1,  4(a0)                              \n"
    "  sw s0,  0(a0)                              \n"
    // Return.
    "  jr ra                                      \n");
#endif
