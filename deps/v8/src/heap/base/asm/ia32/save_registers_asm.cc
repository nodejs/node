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
// The following assumes cdecl calling convention.
// Source: https://en.wikipedia.org/wiki/X86_calling_conventions#cdecl

// 3 32-bit registers = 3 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters == 3,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 4, "Mismatch in word size");

asm(
#ifdef _WIN32
    ".globl _SaveCalleeSavedRegisters            \n"
    "_SaveCalleeSavedRegisters:                  \n"
#else   // !_WIN32
    ".globl SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function   \n"
    ".hidden SaveCalleeSavedRegisters            \n"
    "SaveCalleeSavedRegisters:                   \n"
#endif  // !_WIN32
    // 8: [ intptr_t* buffer ]
    // 4: [ ret              ]
    // 0: [ saved %ebp       ]
    // %ebp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %ebp                                        \n"
    "  movl %esp, %ebp                                  \n"
    // Load the buffer's address in %ecx.
    "  movl 8(%ebp), %ecx                               \n"
    // Save the callee-saved registers.
    "  movl %ebx, 0(%ecx)                               \n"
    "  movl %esi, 4(%ecx)                               \n"
    "  movl %edi, 8(%ecx)                               \n"
    // Restore %ebp as it was used as frame pointer and return.
    "  pop %ebp                                         \n"
    "  ret                                              \n");
