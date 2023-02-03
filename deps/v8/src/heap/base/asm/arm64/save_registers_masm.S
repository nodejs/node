; Copyright 2020 the V8 project authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

; This file is exactly the same as save_registers_asm.cc, just formatted for
; the Microsoft Arm Assembler.

; Save all callee-saved registers in the specified buffer.
; extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

    AREA |.text|, CODE, ALIGN=4, READONLY
    EXPORT SaveCalleeSavedRegisters
SaveCalleeSavedRegisters
    ; x0: [ intptr_t* buffer ]
    ; x19-x29 are callee-saved
    STP x19, x20, [x0], #16
    STP x21, x22, [x0], #16
    STP x23, x24, [x0], #16
    STP x25, x26, [x0], #16
    STP x27, x28, [x0], #16
    STR x29, [x0]
    ; Return.
    RET
    END
