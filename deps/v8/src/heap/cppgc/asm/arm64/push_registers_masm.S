; Copyright 2020 the V8 project authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

; This file is exactly the same as push_registers_asm.cc, just formatted for
; the Microsoft Arm Assembler.

    AREA |.text|, CODE, ALIGN=4, READONLY
    EXPORT PushAllRegistersAndIterateStack
PushAllRegistersAndIterateStack
    ; x19-x29 are callee-saved
    STP x19, x20, [sp, #-16]!
    STP x21, x22, [sp, #-16]!
    STP x23, x24, [sp, #-16]!
    STP x25, x26, [sp, #-16]!
    STP x27, x28, [sp, #-16]!
    STP fp, lr, [sp, #-16]!
    ; Maintain frame pointer
    MOV fp, sp
    ; Pass 1st parameter (x0) unchanged (Stack*).
    ; Pass 2nd parameter (x1) unchanged (StackVisitor*).
    ; Save 3rd parameter (x2; IterateStackCallback)
    MOV x7, x2
    ; Pass 3rd parameter as sp (stack pointer)
    MOV x2, sp
    BLR x7
    ; Load return address
    LDR lr, [sp, #8]
    ; Restore frame pointer and pop all callee-saved registers.
    LDR fp, [sp], #96
    RET
    END