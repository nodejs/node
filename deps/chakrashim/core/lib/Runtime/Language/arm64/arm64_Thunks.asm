;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
    OPT 2   ; disable listing
#include "ksarm64.h"
    OPT 1   ; re-enable listing


    TTL Lib\Runtime\Language\arm64\arm64_DelayDynamicInterpreterThunk.asm

    ;Var InterpreterStackFrame::DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?DelayDynamicInterpreterThunk@InterpreterStackFrame@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ|
    ;Var DynamicProfileInfo::EnsureDynamicProfileInfoThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?EnsureDynamicProfileInfoThunk@DynamicProfileInfo@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ|
    ; Var ScriptContext::ProfileModeDeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    EXPORT  |?ProfileModeDeferredParsingThunk@ScriptContext@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ|

    ;JavascriptMethod InterpreterStackFrame::EnsureDynamicInterpreterThunk(Js::ScriptFunction * function)
    IMPORT  |?EnsureDynamicInterpreterThunk@InterpreterStackFrame@Js@@CAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z|
    ;JavascriptMethod DynamicProfileInfo::EnsureDynamicProfileInfoThunk(Js::ScriptFunction * function)
    IMPORT  |?EnsureDynamicProfileInfo@DynamicProfileInfo@Js@@CAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z|
    ;JavascriptMethod ScriptContext::ProfileModeDeferredParse(ScriptFunction **function)
    IMPORT  |?ProfileModeDeferredParse@ScriptContext@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAPEAVScriptFunction@2@@Z|
    ;JavascriptMethod ScriptContext::ProfileModeDeferredDeserialize(ScriptFunction *function)
    IMPORT  |?ProfileModeDeferredDeserialize@ScriptContext@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z|

    TEXTAREA

;;============================================================================================================
;; InterpreterStackFrame::DelayDynamicInterpreterThunk
;;============================================================================================================
    ;Var InterpreterStackFrame::DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?DelayDynamicInterpreterThunk@InterpreterStackFrame@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!  ; save volatile registers
    stp   x0, x1, [sp, #16]
    stp   x2, x3, [sp, #32]
    stp   x4, x5, [sp, #48]
    stp   x6, x7, [sp, #64]

    bl   |?EnsureDynamicInterpreterThunk@InterpreterStackFrame@Js@@CAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z|  ; call InterpreterStackFrame::EnsureDynamicInterpreterThunk
    mov  x16, x0                  ; back up entryPoint in x16

    ldp   x6, x7, [sp, #64] ; restore arguments and return address
    ldp   x4, x5, [sp, #48]
    ldp   x2, x3, [sp, #32]
    ldp   x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!
    EPILOG_NOP br x16             ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; DynamicProfileInfo::EnsureDynamicProfileInfoThunk
;;============================================================================================================
    ;Var DynamicProfileInfo::EnsureDynamicProfileInfoThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?EnsureDynamicProfileInfoThunk@DynamicProfileInfo@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!  ; save volatile registers
    stp   x0, x1, [sp, #16]
    stp   x2, x3, [sp, #32]
    stp   x4, x5, [sp, #48]
    stp   x6, x7, [sp, #64]

    bl   |?EnsureDynamicProfileInfo@DynamicProfileInfo@Js@@CAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z| ; call DynamicProfileInfo::EnsureDynamicProfileInfo
    mov  x16, x0                  ; back up entryPoint in x16

    ldp   x6, x7, [sp, #64] ; restore arguments and return address
    ldp   x4, x5, [sp, #48]
    ldp   x2, x3, [sp, #32]
    ldp   x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!
    EPILOG_NOP br x16             ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; ScriptContext::ProfileModeDeferredParsingThunk
;;============================================================================================================
    ;; Var ScriptContext::ProfileModeDeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?ProfileModeDeferredParsingThunk@ScriptContext@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!  ; save volatile registers
    stp   x0, x1, [sp, #16]
    stp   x2, x3, [sp, #32]
    stp   x4, x5, [sp, #48]
    stp   x6, x7, [sp, #64]

    mov  x0, sp                   ; Pass the address of the function at the saved x0 in case it need to be boxed
    bl   |?ProfileModeDeferredParse@ScriptContext@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAPEAVScriptFunction@2@@Z| ; call ScriptContext::ProfileModeDeferredParse
    mov  x16, x0                  ; back up entryPoint in x16

    ldp   x6, x7, [sp, #64] ; restore arguments and return address
    ldp   x4, x5, [sp, #48]
    ldp   x2, x3, [sp, #32]
    ldp   x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!
    EPILOG_NOP br x16             ; jump (tail call) to new entryPoint

    NESTED_END

;;============================================================================================================
;; ScriptContext::ProfileModeDeferredDeserializeThunk
;;============================================================================================================
    ;; Var ScriptContext::ProfileModeDeferredDeserializeThunk(RecyclableObject* function, CallInfo callInfo, ...)
    NESTED_ENTRY ?ProfileModeDeferredDeserializeThunk@ScriptContext@Js@@SAPEAXPEAVRecyclableObject@2@UCallInfo@2@ZZ

    PROLOG_SAVE_REG_PAIR fp, lr, #-80!  ; save volatile registers
    stp   x0, x1, [sp, #16]
    stp   x2, x3, [sp, #32]
    stp   x4, x5, [sp, #48]
    stp   x6, x7, [sp, #64]

    bl   |?ProfileModeDeferredDeserialize@ScriptContext@Js@@SAP6APEAXPEAVRecyclableObject@2@UCallInfo@2@ZZPEAVScriptFunction@2@@Z| ; call ScriptContext::ProfileModeDeferredDeserialize
    mov  x16, x0                  ; back up entryPoint in x16

    ldp   x6, x7, [sp, #64] ; restore arguments and return address
    ldp   x4, x5, [sp, #48]
    ldp   x2, x3, [sp, #32]
    ldp   x0, x1, [sp, #16]
    EPILOG_RESTORE_REG_PAIR fp, lr, #80!
    EPILOG_NOP br x16             ; jump (tail call) to new entryPoint

    NESTED_END
;;============================================================================================================
    END
