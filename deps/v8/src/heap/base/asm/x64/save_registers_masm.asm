;; Copyright 2020 the V8 project authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; MASM syntax
;; https://docs.microsoft.com/en-us/cpp/assembler/masm/microsoft-macro-assembler-reference?view=vs-2019

public SaveCalleeSavedRegisters

.code
    ;; Save all callee-saved registers in the specified buffer.
    ;; extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

SaveCalleeSavedRegisters:
    ;; %rcx: [ intptr_t* buffer ]
    ;; %rbp is callee-saved. Maintain proper frame pointer for debugging.
    push rbp
    mov rbp, rsp
    ;; Save the callee-saved registers.
    mov qword ptr [rcx], rsi
    mov qword ptr [rcx + 8], rdi
    mov qword ptr [rcx + 16], rbx
    mov qword ptr [rcx + 24], r12
    mov qword ptr [rcx + 32], r13
    mov qword ptr [rcx + 40], r14
    mov qword ptr [rcx + 48], r15
    ;; Skip one slot to achieve proper alignment and use aligned instructions,
    ;; as we are sure that the buffer is properly aligned.
    movdqa xmmword ptr [rcx + 64], xmm6
    movdqa xmmword ptr [rcx + 80], xmm7
    movdqa xmmword ptr [rcx + 96], xmm8
    movdqa xmmword ptr [rcx + 112], xmm9
    movdqa xmmword ptr [rcx + 128], xmm10
    movdqa xmmword ptr [rcx + 144], xmm11
    movdqa xmmword ptr [rcx + 160], xmm12
    movdqa xmmword ptr [rcx + 176], xmm13
    movdqa xmmword ptr [rcx + 192], xmm14
    movdqa xmmword ptr [rcx + 208], xmm15
    ;; Restore %rbp as it was used as frame pointer and return.
    pop rbp
    ret

end
