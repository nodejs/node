;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;Var Js:JavascriptFunction::DeferredParsingThunk(function, info, values[0], values[1], ..., values[n-2], values[n-1])
;
;   This method should be called as follows
;       varResult = JavascriptFunction::DeferredParsingThunk(function, info, values[0], values[1], ..., values[n-2], values[n-1]);
;
;   and does the following:
;           entryPoint = JavascriptFunction::DeferredParse(&function);
;           return entryPoint(function, info, values[0], values[1], ..., values[n-2], values[n-1]);
;   where n = info.Count
;
    OPT 2   ; disable listing

#include "ksarm64.h"

    OPT 1   ; re-enable listing

    TTL Lib\Runtime\Library\arm64\arm64_DeferredParsingThunk.asm


    EXPORT  |?DeferredParsingThunk@JavascriptFunction@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ|      ;Js:JavascriptFunction::DeferredParsingThunk(function, info, values[0], values[1], ..., values[n-2], values[n-1])
    IMPORT  |?DeferredParse@JavascriptFunction@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAPEAVScriptFunction@2@@Z|  ;Js::JavascriptMethod JavascriptFunction::DeferredParse(ScriptFunction** function)

    TEXTAREA

    NESTED_ENTRY ?DeferredParsingThunk@JavascriptFunction@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!
    stp     x0, x1, [sp, #16]
    stp     x2, x3, [sp, #32]
    stp     x4, x5, [sp, #48]
    stp     x6, x7, [sp, #64]

    ; Pass the address of the function at the saved x0
    mov     x0, sp
    add     x0, x0, #16
    bl      |?DeferredParse@JavascriptFunction@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAPEAVScriptFunction@2@@Z| ; retrieve entrypoint
    mov     x16, x0                     ; back up entryPoint in x16

    ldp     x6, x7, [sp, #64]
    ldp     x4, x5, [sp, #48]
    ldp     x2, x3, [sp, #32]
    ldp     x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!
    EPILOG_NOP br x16                   ; tail call to new entryPoint

    NESTED_END

    END
