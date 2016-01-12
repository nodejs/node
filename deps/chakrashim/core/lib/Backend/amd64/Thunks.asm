;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
include ksamd64.inc

        _TEXT SEGMENT

ifdef _CONTROL_FLOW_GUARD
    extrn __guard_check_icall_fptr:QWORD
endif

;;============================================================================================================
;; NativeCodeGenerator::CheckCodeGenThunk
;;============================================================================================================

extrn ?CheckCodeGen@NativeCodeGenerator@@SAP6APEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZPEAVScriptFunction@3@@Z : PROC
align 16
?CheckCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ PROC FRAME
        ;; save volatile registers
        mov qword ptr [rsp + 8h],  rcx
        mov qword ptr [rsp + 10h], rdx
        mov qword ptr [rsp + 18h], r8
        mov qword ptr [rsp + 20h], r9

        push rbp
        .pushreg rbp
        lea  rbp, [rsp]
        .setframe rbp, 0
        .endprolog


ifdef _CONTROL_FLOW_GUARD
        sub rsp, 30h                            ;allocate stack space for the callee params(min 4 slots is mandate + 1 for saving call target + 1 for alignment)
        call ?CheckCodeGen@NativeCodeGenerator@@SAP6APEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZPEAVScriptFunction@3@@Z

        mov [rsp + 28h], rax                    ;save rax (call target) [6th slot will have call target and 5th slot is left untouched]
        mov rcx, rax                            ; __guard_check_icall_fptr requires the call target in rcx.
        call [__guard_check_icall_fptr]         ; verify that the call target is valid

        add rsp, 28h                            ;de-allocate stack space for the callee params(min 4 slots is mandate + 1 for alignment )
        pop rax                                 ;restore call target
else
        sub rsp, 20h                            ;allocate stack space for the callee params(min 4 slots is mandate)
        call ?CheckCodeGen@NativeCodeGenerator@@SAP6APEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZPEAVScriptFunction@3@@Z
        add rsp, 20h                            ;de-allocate stack space for the callee params(min 4 slots is mandate)
endif

        ;EPILOGUE starts here

        lea rsp, [rbp]
        pop rbp

        ;; restore volatile registers
        mov rcx, qword ptr [rsp + 8h]
        mov rdx, qword ptr [rsp + 10h]
        mov r8,  qword ptr [rsp + 18h]
        mov r9,  qword ptr [rsp + 20h]

        rex_jmp_reg rax
?CheckCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ ENDP

;;============================================================================================================
;; NativeCodeGenerator::CheckAsmJsCodeGenThunk
;;============================================================================================================

extrn ?CheckAsmJsCodeGen@NativeCodeGenerator@@SAPEAXPEAVScriptFunction@Js@@@Z : PROC
align 16
?CheckAsmJsCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ PROC FRAME
        ;; save volatile registers
        mov qword ptr [rsp + 8h],  rcx
        mov qword ptr [rsp + 10h], rdx
        mov qword ptr [rsp + 18h], r8
        mov qword ptr [rsp + 20h], r9

        push rbp
        .pushreg rbp
        lea  rbp, [rsp]
        .setframe rbp, 0
        .endprolog

        sub rsp, 60h

        ; spill potential floating point arguments to stack
        movups xmmword ptr [rsp + 30h], xmm1
        movups xmmword ptr [rsp + 40h], xmm2
        movups xmmword ptr [rsp + 50h], xmm3

ifdef _CONTROL_FLOW_GUARD
        call ?CheckAsmJsCodeGen@NativeCodeGenerator@@SAPEAXPEAVScriptFunction@Js@@@Z

        mov rcx, rax                            ; __guard_check_icall_fptr requires the call target in rcx.
        call [__guard_check_icall_fptr]         ; verify that the call target is valid
        mov rax, rcx ; CFG is guaranteed not to mess up rcx
else
        call ?CheckAsmJsCodeGen@NativeCodeGenerator@@SAPEAXPEAVScriptFunction@Js@@@Z
endif

        ;EPILOGUE starts here
        movups xmm1, xmmword ptr [rsp + 30h]
        movups xmm2, xmmword ptr [rsp + 40h]
        movups xmm3, xmmword ptr [rsp + 50h]

        lea rsp, [rbp]
        pop rbp

        ;; restore volatile registers
        mov rcx, qword ptr [rsp + 8h]
        mov rdx, qword ptr [rsp + 10h]
        mov r8,  qword ptr [rsp + 18h]
        mov r9,  qword ptr [rsp + 20h]

        rex_jmp_reg rax
?CheckAsmJsCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ ENDP

;;============================================================================================================

        _TEXT ENDS
        end
