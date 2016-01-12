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

#include "ksarm.h"

    OPT 1   ; re-enable listing

    TTL Lib\Runtime\Library\arm\arm_DeferredParsingThunk.asm


    EXPORT  |?DeferredParsingThunk@JavascriptFunction@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ|      ;Js:JavascriptFunction::DeferredParsingThunk(function, info, values[0], values[1], ..., values[n-2], values[n-1])
    IMPORT  |?DeferredParse@JavascriptFunction@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAPAVScriptFunction@2@@Z|  ;Js::JavascriptMethod JavascriptFunction::DeferredParse(ScriptFunction** function)

    TEXTAREA

    NESTED_ENTRY ?DeferredParsingThunk@JavascriptFunction@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ
    PROLOG_PUSH r0-r3,r11,lr  ; save volatile registers

    mov     r0, sp              ; Pass the address of the function at the saved r0 in case it need to be boxed
    bl      |?DeferredParse@JavascriptFunction@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAPAVScriptFunction@2@@Z| ; retrieve entrypoint
    mov     r12,r0              ; back up entryPoint in R12

    EPILOG_POP r0-r3,r11,lr         ; restore arguments and return address
    EPILOG_NOP bx      r12          ; jump (tail call) to new entryPoint

    NESTED_END

    END
