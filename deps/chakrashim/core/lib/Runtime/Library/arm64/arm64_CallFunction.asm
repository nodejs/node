;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;Var arm64_CallFunction(JavascriptFunction* function, CallInfo info, Var* values, JavascriptMethod entryPoint)
;
;   This method should be called as follows
;       varResult = arm64_CallFunction((JavascriptFunction*)function, args.Info, args.Values, entryPoint);
;
;   and makes the following call
;           return entryPoint(function, info, values[0], values[1], ..., values[n-2], values[n-1]);
;   where n = info.Count >= 6 (so we know we'll need to pass 6 values in registers).
;
;   ARM64 calling convention is:
;       x0        parameter 1 = function
;       x1        parameter 2 = info
;       x2        values[0]
;       x3        values[1]
;       x4        values[2]
;       x5        values[3]
;       x6        values[4]
;       x7        values[5]
;       [sp+0]    values[6]
;       [sp+8]    values[7]
;       ...
;
;       (8 bytes) return values are passed in x0.
;
;   Since the 1st two parameters are the same for this method and the entry point, we don't need to touch them.
;
    OPT 2   ; disable listing

#include "ksarm64.h"

#if defined(_CONTROL_FLOW_GUARD)
        IMPORT __guard_check_icall_fptr
#endif

    OPT 1   ; re-enable listing

    TTL Lib\Runtime\Library\arm64\arm64_CallFunction.asm

    EXPORT  arm64_CallFunction
    IMPORT  __chkstk            ;See \\cpvsbuild\drops\dev11\Main\raw\current\sources\vctools\crt\crtw32\startup\arm64\chkstk.asm.

    TEXTAREA

    NESTED_ENTRY arm64_CallFunction

    PROLOG_SAVE_REG_PAIR fp, lr, #-16!          ; save fp/lr (implicitly saves SP in FP)

    mov     x15, x3                             ; copy entry point to x15

#if _CONTROL_FLOW_GUARD
    adrp    x16, __guard_check_icall_fptr       ;
    ldr     x16, [x16, __guard_check_icall_fptr]; fetch address of guard check handler
    blr     x16                                 ; call it
#endif

    and     x4, x1, #0xffffff                   ; clear high order 40 bits of x1(callInfo)
                                                ;   (clean callInfo.Flags which shares same word as callInfo.Count)
    subs    x5, x4, #6                          ; more than 6 parameters?
    bgt     StackAlloc                          ; if so, allocate necessary stack

    adr     x5, CopyZero                        ; get bottom of parameter copy loop
    sub     x5, x5, x4, lsl #2                  ; compute address of where to start
    br      x5                                  ; branch there
CopyAll
    ldr     x7, [x2, #40]                       ; load remaining 6 registers here
    ldr     x6, [x2, #32]                       ;
    ldr     x5, [x2, #24]                       ;
    ldr     x4, [x2, #16]                       ;
    ldr     x3, [x2, #8]                        ;
    ldr     x2, [x2, #0]                        ;
CopyZero
    blr     x15                                 ; call saved entry point

    mov     sp, fp                              ; explicitly restore sp
    EPILOG_RESTORE_REG_PAIR fp, lr, #16!        ; restore FP/LR
    EPILOG_RETURN                               ; return

StackAlloc
    add     x15, x5, #1                         ; round (param_count - 6) up by 1
    lsr     x15, x15, #1                        ; divide by 2
    bl      __chkstk                            ; ensure stack is allocated
    sub     sp, sp, x15, lsl #4                 ; then allocate the space
    add     x6, x2, #48                         ; use x6 = source
    mov     x7, sp                              ; use x7 = dest
CopyLoop
    subs    x5, x5, #1                          ; decrement param count by 1
    ldr     x4, [x6], #8                        ; read param from source
    str     x4, [x7], #8                        ; store param to dest
    bne     CopyLoop                            ; loop until all copied
    mov     x15, x3                             ; recover entry point in x15
    b       CopyAll                             ; jump ahead to copy all 6 remaining parameters

    NESTED_END

    END
