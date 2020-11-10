;; Copyright 2020 the V8 project authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; MASM syntax
;; https://docs.microsoft.com/en-us/cpp/assembler/masm/microsoft-macro-assembler-reference?view=vs-2019

.model flat, C

public PushAllRegistersAndIterateStack

.code
PushAllRegistersAndIterateStack:
    ;; Push all callee-saved registers to get them on the stack for conservative
    ;; stack scanning.
    ;;
    ;; We maintain 16-byte alignment at calls. There is an 8-byte return address
    ;; on the stack and we push 72 bytes which maintains 16-byte stack alignment
    ;; at the call.
    ;;
    ;; The following assumes cdecl calling convention.
    ;; Source: https://docs.microsoft.com/en-us/cpp/cpp/cdecl?view=vs-2019
    ;;
    ;; [ IterateStackCallback ]
    ;; [ StackVisitor*        ]
    ;; [ Stack*               ]
    ;; [ ret                  ]
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    ;; Save 3rd parameter (IterateStackCallback).
    mov ecx, [ esp + 28 ]
    ;; Pass 3rd parameter as esp (stack pointer).
    push esp
    ;; Pass 2nd parameter (StackVisitor*).
    push [ esp + 28 ]
    ;; Pass 1st parameter (Stack*).
    push [ esp + 28 ]
    call ecx
    ;; Pop the callee-saved registers.
    add esp, 24
    ;; Restore rbp as it was used as frame pointer.
    pop ebp
    ret

end
