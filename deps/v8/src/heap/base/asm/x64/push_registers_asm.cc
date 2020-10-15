// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// We cannot rely on clang generating the function and right symbol mangling
// as `__attribite__((naked))` does not prevent clang from generating TSAN
// function entry stubs (`__tsan_func_entry`). Even with
// `__attribute__((no_sanitize_thread)` annotation clang generates the entry
// stub.
// See https://bugs.llvm.org/show_bug.cgi?id=45400.

// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.
// _WIN64 Defined as 1 when the compilation target is 64-bit ARM or x64.
// Otherwise, undefined.
#ifdef _WIN64

// We maintain 16-byte alignment at calls. There is an 8-byte return address
// on the stack and we push 72 bytes which maintains 16-byte stack alignment
// at the call.
// Source: https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention
asm(".globl PushAllRegistersAndIterateStack             \n"
    "PushAllRegistersAndIterateStack:                   \n"
    // rbp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %rbp                                        \n"
    "  mov %rsp, %rbp                                   \n"
    // Dummy for alignment.
    "  push $0xCDCDCD                                   \n"
    "  push %rsi                                        \n"
    "  push %rdi                                        \n"
    "  push %rbx                                        \n"
    "  push %r12                                        \n"
    "  push %r13                                        \n"
    "  push %r14                                        \n"
    "  push %r15                                        \n"
    // Pass 1st parameter (rcx) unchanged (Stack*).
    // Pass 2nd parameter (rdx) unchanged (StackVisitor*).
    // Save 3rd parameter (r8; IterateStackCallback)
    "  mov %r8, %r9                                     \n"
    // Pass 3rd parameter as rsp (stack pointer).
    "  mov %rsp, %r8                                    \n"
    // Call the callback.
    "  call *%r9                                        \n"
    // Pop the callee-saved registers.
    "  add $64, %rsp                                    \n"
    // Restore rbp as it was used as frame pointer.
    "  pop %rbp                                         \n"
    "  ret                                              \n");

#else  // !_WIN64

// We maintain 16-byte alignment at calls. There is an 8-byte return address
// on the stack and we push 56 bytes which maintains 16-byte stack alignment
// at the call.
// Source: https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-1.0.pdf
asm(
#ifdef __APPLE__
    ".globl _PushAllRegistersAndIterateStack            \n"
    ".private_extern _PushAllRegistersAndIterateStack   \n"
    "_PushAllRegistersAndIterateStack:                  \n"
#else   // !__APPLE__
    ".globl PushAllRegistersAndIterateStack             \n"
    ".type PushAllRegistersAndIterateStack, %function   \n"
    ".hidden PushAllRegistersAndIterateStack            \n"
    "PushAllRegistersAndIterateStack:                   \n"
#endif  // !__APPLE__
    // rbp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %rbp                                        \n"
    "  mov %rsp, %rbp                                   \n"
    // Dummy for alignment.
    "  push $0xCDCDCD                                   \n"
    "  push %rbx                                        \n"
    "  push %r12                                        \n"
    "  push %r13                                        \n"
    "  push %r14                                        \n"
    "  push %r15                                        \n"
    // Pass 1st parameter (rdi) unchanged (Stack*).
    // Pass 2nd parameter (rsi) unchanged (StackVisitor*).
    // Save 3rd parameter (rdx; IterateStackCallback)
    "  mov %rdx, %r8                                    \n"
    // Pass 3rd parameter as rsp (stack pointer).
    "  mov %rsp, %rdx                                   \n"
    // Call the callback.
    "  call *%r8                                        \n"
    // Pop the callee-saved registers.
    "  add $48, %rsp                                    \n"
    // Restore rbp as it was used as frame pointer.
    "  pop %rbp                                         \n"
    "  ret                                              \n");

#endif  // !_WIN64
