// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <src/heap/base/stack.h>

// Save all callee-saved registers in the specified buffer.
// extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);
//
// We cannot rely on clang generating the function and right symbol mangling
// as `__attribute__((naked))` does not prevent clang from generating TSAN
// function entry stubs (`__tsan_func_entry`). Even with
// `__attribute__((no_sanitize_thread)` annotation clang generates the entry
// stub.
// See https://bugs.llvm.org/show_bug.cgi?id=45400.
//
// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.
// _WIN64 Defined as 1 when the compilation target is 64-bit ARM or x64.
// Otherwise, undefined.

#ifdef _WIN64
// Source: https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention

// 7 64-bit registers + 1 for alignment purposes = 8 * 1 = 8 intprt_t
// 10 128-bit registers = 10 * 2 = 20 intptr_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters == 28,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(".globl SaveCalleeSavedRegisters             \n"
    "SaveCalleeSavedRegisters:                   \n"
    // %rcx: [ intptr_t* buffer ]
    // %rbp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %rbp                                 \n"
    "  mov %rsp, %rbp                            \n"
    // Save the callee-saved registers.
    "  mov %rsi, 0(%rcx)                         \n"
    "  mov %rdi, 8(%rcx)                         \n"
    "  mov %rbx, 16(%rcx)                        \n"
    "  mov %r12, 24(%rcx)                        \n"
    "  mov %r13, 32(%rcx)                        \n"
    "  mov %r14, 40(%rcx)                        \n"
    "  mov %r15, 48(%rcx)                        \n"
    // Skip one slot to achieve proper alignment and use aligned instructions,
    // as we are sure that the buffer is properly aligned.
    "  movdqa %xmm6, 64(%rcx)                    \n"
    "  movdqa %xmm7, 80(%rcx)                    \n"
    "  movdqa %xmm8, 96(%rcx)                    \n"
    "  movdqa %xmm9, 112(%rcx)                   \n"
    "  movdqa %xmm10, 128(%rcx)                  \n"
    "  movdqa %xmm11, 144(%rcx)                  \n"
    "  movdqa %xmm12, 160(%rcx)                  \n"
    "  movdqa %xmm13, 176(%rcx)                  \n"
    "  movdqa %xmm14, 192(%rcx)                  \n"
    "  movdqa %xmm15, 208(%rcx)                  \n"
    // Return.
    "  pop %rbp                                  \n"
    "  ret                                       \n");

#else  // !_WIN64
// Source: https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-1.0.pdf

// 5 64-bit registers = 5 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters == 5,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 8, "Mismatch in word size");

asm(
#ifdef __APPLE__
    ".globl _SaveCalleeSavedRegisters            \n"
    ".private_extern _SaveCalleeSavedRegisters   \n"
    "_SaveCalleeSavedRegisters:                  \n"
#else   // !__APPLE__
    ".globl SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function   \n"
    ".hidden SaveCalleeSavedRegisters            \n"
    "SaveCalleeSavedRegisters:                   \n"
#endif  // !__APPLE__
    // %rdi: [ intptr_t* buffer ]
    // %rbp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %rbp                                 \n"
    "  mov %rsp, %rbp                            \n"
    // Save the callee-saved registers.
    "  mov %rbx, 0(%rdi)                         \n"
    "  mov %r12, 8(%rdi)                         \n"
    "  mov %r13, 16(%rdi)                        \n"
    "  mov %r14, 24(%rdi)                        \n"
    "  mov %r15, 32(%rdi)                        \n"
    // Restore %rbp as it was used as frame pointer and return.
    "  pop %rbp                                  \n"
    "  ret                                       \n");

#endif  // !_WIN64
