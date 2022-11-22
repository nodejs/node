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
//
// We maintain 16-byte alignment.
//
// Calling convention source:
// https://en.wikipedia.org/wiki/Calling_convention#ARM_(A64)

// 11 64-bit registers = 11 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters == 11,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(
#if defined(__APPLE__)
    ".globl _SaveCalleeSavedRegisters            \n"
    ".private_extern _SaveCalleeSavedRegisters   \n"
    ".p2align 2                                  \n"
    "_SaveCalleeSavedRegisters:                  \n"
#else  // !defined(__APPLE__)
    ".globl SaveCalleeSavedRegisters             \n"
#if !defined(_WIN64)
    ".type SaveCalleeSavedRegisters, %function   \n"
    ".hidden SaveCalleeSavedRegisters            \n"
#endif  // !defined(_WIN64)
    ".p2align 2                                  \n"
    "SaveCalleeSavedRegisters:                   \n"
#endif  // !defined(__APPLE__)
    // $x0: [ intptr_t* buffer ]
    // Save the callee-saved registers: x19-x29.
    "  stp x19, x20, [x0], #16                   \n"
    "  stp x21, x22, [x0], #16                   \n"
    "  stp x23, x24, [x0], #16                   \n"
    "  stp x25, x26, [x0], #16                   \n"
    "  stp x27, x28, [x0], #16                   \n"
    "  str x29, [x0]                             \n"
    // Return.
    "  ret                                       \n");
