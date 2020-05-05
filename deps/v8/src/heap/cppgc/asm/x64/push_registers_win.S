;; Copyright 2020 the V8 project authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; MASM syntax
;; https://docs.microsoft.com/en-us/cpp/assembler/masm/microsoft-macro-assembler-reference?view=vs-2019

public PushAllRegistersAndIterateStack

.code
PushAllRegistersAndIterateStack:
    ;; Push all callee-saved registers to get them on the stack for conservative
    ;; stack scanning.
    ;;
    ;; We maintain 16-byte alignment at calls. There is an 8-byte return address
    ;; on the stack and we push 72 bytes which maintains 16-byte stack alignment
    ;; at the call.
    ;; Source: https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention
    ;;
    ;; rbp is callee-saved. Maintain proper frame pointer for debugging.
    push rbp
    mov rbp, rsp
    push 0CDCDCDh  ;; Dummy for alignment.
    push rsi
    push rdi
    push rbx
    push r12
    push r13
    push r14
    push r15
    ;; Pass 1st parameter (rcx) unchanged (Stack*).
    ;; Pass 2nd parameter (rdx) unchanged (StackVisitor*).
    ;; Save 3rd parameter (r8; IterateStackCallback)
    mov r9, r8
    ;; Pass 3rd parameter as rsp (stack pointer).
    mov r8, rsp
    ;; Call the callback.
    call r9
    ;; Pop the callee-saved registers.
    add rsp, 64
    ;; Restore rbp as it was used as frame pointer.
    pop rbp
    ret

end
