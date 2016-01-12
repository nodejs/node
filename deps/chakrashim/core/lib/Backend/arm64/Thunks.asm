;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
    OPT 2   ; disable listing
#include "ksarm64.h"
    OPT 1   ; re-enable listing

    TTL Lib\Backend\arm64\Thunks.asm

    ;Js::Var NativeCodeGenerator::CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
    EXPORT  |?CheckCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ|

    ;Js::JavascriptMethod NativeCodeGenerator::CheckCodeGen(Js::JavascriptFunction * function)
    IMPORT  |?CheckCodeGen@NativeCodeGenerator@@SAP6APEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZPEAVScriptFunction@3@@Z|

    TEXTAREA

;;============================================================================================================
; NativeCodeGenerator::CheckCodeGenThunk
;;============================================================================================================
    ;Js::Var NativeCodeGenerator::CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)

    NESTED_ENTRY ?CheckCodeGenThunk@NativeCodeGenerator@@SAPEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!  ; save volatile registers
    stp   x0, x1, [sp, #16]
    stp   x2, x3, [sp, #32]
    stp   x4, x5, [sp, #48]
    stp   x6, x7, [sp, #64]

    bl   |?CheckCodeGen@NativeCodeGenerator@@SAP6APEAXPEAVRecyclableObject@Js@@UCallInfo@3@ZZPEAVScriptFunction@3@@Z|  ; call  NativeCodeGenerator::CheckCodeGen
    mov  x16, x0                    ; back up entryPoint in R16

    ldp   x6, x7, [sp, #64]         ; restore arguments and return address
    ldp   x4, x5, [sp, #48]
    ldp   x2, x3, [sp, #32]
    ldp   x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!

    EPILOG_NOP  br   x16            ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
    END
