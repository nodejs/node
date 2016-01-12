;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
    OPT 2   ; disable listing
#include "ksarm.h"
    OPT 1   ; re-enable listing

    TTL Lib\Runtime\Language\arm\arm_DelayDynamicInterpreterThunk.asm

    ;Var InterpreterStackFrame::DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?DelayDynamicInterpreterThunk@InterpreterStackFrame@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ|
    ;Var DynamicProfileInfo::EnsureDynamicProfileInfoThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?EnsureDynamicProfileInfoThunk@DynamicProfileInfo@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ|
    ; Var ScriptContext::ProfileModeDeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?ProfileModeDeferredParsingThunk@ScriptContext@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ|

    ;JavascriptMethod InterpreterStackFrame::EnsureDynamicInterpreterThunk(Js::ScriptFunction * function)
    IMPORT  |?EnsureDynamicInterpreterThunk@InterpreterStackFrame@Js@@CAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z|
    ;JavascriptMethod DynamicProfileInfo::EnsureDynamicProfileInfoThunk(Js::ScriptFunction * function)
    IMPORT  |?EnsureDynamicProfileInfo@DynamicProfileInfo@Js@@CAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z|
    ;JavascriptMethod ScriptContext::ProfileModeDeferredParse(ScriptFunction **function)
    IMPORT  |?ProfileModeDeferredParse@ScriptContext@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAPAVScriptFunction@2@@Z|
    ;JavascriptMethod ScriptContext::ProfileModeDeferredDeserialize(ScriptFunction *function)
    IMPORT  |?ProfileModeDeferredDeserialize@ScriptContext@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z|

    TEXTAREA

;;============================================================================================================
;; InterpreterStackFrame::DelayDynamicInterpreterThunk
;;============================================================================================================
    ;Var InterpreterStackFrame::DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?DelayDynamicInterpreterThunk@InterpreterStackFrame@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_PUSH r0-r5,r11,lr      ; save volatile registers and non-volatile registers; r5 is pushed for aligned purposes

    bl   |?EnsureDynamicInterpreterThunk@InterpreterStackFrame@Js@@CAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z|  ; call InterpreterStackFrame::EnsureDynamicInterpreterThunk

#if defined(_CONTROL_FLOW_GUARD)
    mov     r4, r0                ; save entryPoint in r4

    mov32   r12, __guard_check_icall_fptr
    ldr     r12, [r12]
    blx     r12

    mov     r0, r4                ; restore entryPoint in r0
#endif

    mov  r12, r0                  ; back up entryPoint in R12

    EPILOG_POP r0-r5,r11,lr       ; restore arguments and return address

    EPILOG_NOP  bx   r12          ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; DynamicProfileInfo::EnsureDynamicProfileInfoThunk
;;============================================================================================================
    ;Var DynamicProfileInfo::EnsureDynamicProfileInfoThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?EnsureDynamicProfileInfoThunk@DynamicProfileInfo@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_PUSH r0-r3,r11,lr      ; save volatile registers

    bl   |?EnsureDynamicProfileInfo@DynamicProfileInfo@Js@@CAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z| ; call DynamicProfileInfo::EnsureDynamicProfileInfo
    mov  r12, r0                  ; back up entryPoint in R12

    EPILOG_POP r0-r3,r11,lr       ; restore arguments and return address

    EPILOG_NOP  bx   r12          ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; ScriptContext::ProfileModeDeferredParsingThunk
;;============================================================================================================
    ;; Var ScriptContext::ProfileModeDeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?ProfileModeDeferredParsingThunk@ScriptContext@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_PUSH r0-r3,r11,lr      ; save volatile registers

    mov  r0, sp                   ; Pass the address of the function at the saved r0 in case it need to be boxed
    bl   |?ProfileModeDeferredParse@ScriptContext@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAPAVScriptFunction@2@@Z| ; call ScriptContext::ProfileModeDeferredParse
    mov  r12, r0                  ; back up entryPoint in R12

    EPILOG_POP r0-r3,r11,lr       ; restore arguments and return address

    EPILOG_NOP  bx   r12          ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; ScriptContext::ProfileModeDeferredDeserializeThunk
;;============================================================================================================
    ;; Var ScriptContext::ProfileModeDeferredDeserializeThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?ProfileModeDeferredDeserializeThunk@ScriptContext@Js@@SAPAXPAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_PUSH r0-r3,r11,lr      ; save volatile registers

    bl   |?ProfileModeDeferredDeserialize@ScriptContext@Js@@SAP6APAXPAVRecyclableObject@2@UCallInfo@2@ZZPAVScriptFunction@2@@Z| ; call ScriptContext::ProfileModeDeferredDeserialize
    mov  r12, r0                  ; back up entryPoint in R12

    EPILOG_POP r0-r3,r11,lr       ; restore arguments and return address

    EPILOG_NOP  bx   r12          ; jump (tail call) to new entryPoint

    NESTED_END
;;============================================================================================================
    END
