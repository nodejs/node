// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

.att_syntax

.text

#ifdef V8_TARGET_OS_MACOSX

.globl _PushAllRegistersAndIterateStack
_PushAllRegistersAndIterateStack:

#else  // !V8_TARGET_OS_MACOSX

.type PushAllRegistersAndIterateStack, %function
.global PushAllRegistersAndIterateStack
.hidden PushAllRegistersAndIterateStack
PushAllRegistersAndIterateStack:

#endif  // !V8_TARGET_OS_MACOSX

    // Push all callee-saved registers to get them on the stack for conservative
    // stack scanning.
    //
    // We maintain 16-byte alignment at calls. There is an 8-byte return address
    // on the stack and we push 56 bytes which maintains 16-byte stack alignment
    // at the call.
    // Source: https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-1.0.pdf
    //
    // rbp is callee-saved. Maintain proper frame pointer for debugging.
    push %rbp
    mov %rsp, %rbp
    push $0xCDCDCD  // Dummy for alignment.
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    // Pass 1st parameter (rdi) unchanged (Stack*).
    // Pass 2nd parameter (rsi) unchanged (StackVisitor*).
    // Save 3rd parameter (rdx; IterateStackCallback)
    mov %rdx, %r8
    // Pass 3rd parameter as rsp (stack pointer).
    mov %rsp, %rdx
    // Call the callback.
    call *%r8
    // Pop the callee-saved registers.
    add $48, %rsp
    // Restore rbp as it was used as frame pointer.
    pop %rbp
    ret
