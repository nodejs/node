;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;Var arm_CallFunction(JavascriptFunction* function, CallInfo info, Var* values, JavascriptMethod entryPoint)
;
;   This method should be called as follows
;       varResult = arm_CallFunction((JavascriptFunction*)function, args.Info, args.Values, entryPoint);
;
;   and makes the following call
;           return entryPoint(function, info, values[0], values[1], ..., values[n-2], values[n-1]);
;   where n = info.Count >= 2 (so we know we'll need to pass two values in registers).
;
;   ARM calling convention is:
;       r0        parameter 1 = function
;       r1        parameter 2 = info
;       r2        values[0]
;       r3        values[1]
;       [sp+0]    values[2]
;       [sp+4]    values[3]
;       ...
;
;       (4 bytes) return values are passed in r0.
;
;   Since the 1st two parameters are the same for this method and the entry point, we don't need to touch them.
;
    OPT 2   ; disable listing

#include "ksarm.h"

#if defined(_CONTROL_FLOW_GUARD)
        IMPORT __guard_check_icall_fptr
#endif

    OPT 1   ; re-enable listing

    TTL Lib\Runtime\Library\arm\arm_CallFunction.asm

    EXPORT  arm_CallFunction
    IMPORT  __chkstk            ;See \\cpvsbuild\drops\dev11\Main\raw\current\sources\vctools\crt\crtw32\startup\arm\chkstk.asm.

    TEXTAREA

    NESTED_ENTRY arm_CallFunction

    ; IMPORTANT: the number of pushed registers (even vs odd) must be consistent with 8-bytes align check below.
    PROLOG_PUSH r4-r7,r11,lr   ; r6 is saved for stack alignment, this does: add r11,sp,#10 as well
#if defined(_CONTROL_FLOW_GUARD)
    PROLOG_PUSH r8-r9          ; extra register to save info across icall check, and one register to maintain alignment
#endif
    PROLOG_STACK_SAVE r7       ; mov r7,sp -- save stack frame for restoring at the epilog.


    ;All but two values go onto the stack ... calculate the number of values on the stack.
    mov     r4,r1               ;r4 = callInfo.
    bfc     r4,#24,#8           ;clear high order 8 bits of r6 -- clean callInfo.Flags which shares same word as callInfo.Count.
    sub     r4,r4,#2            ;subtract 2 == number of values that we can pass via registers.
    mov     r5,r4,lsl #2        ;r5 = space needed on the stack for values.

    ; Adjust sp to meet ABI requirement: stack must be 8-bytes aligned at any function boundary.
    ; Each reg is 4 bytes, so as we push 4 (even) regs above, r1 == odd number of regs breaks 8-byte alignment, even is OK.
    asrs    r12,r4,#1           ; r-shift r1 into carry - set carry flag if r4 is odd. Note: the value in r12 is not used.
    addcs   r4,r4,#1            ; if carry is set, add space for one more argument on stack to guarantee 8-byte alignment.
    ; // TODO: Evanesco: it seems that keeping old values on stack can cause false-positive jcroot's. We should see if that's the case.

    ;Probe stack. This is required when we need more than 1 page of stack space.
    ;__chkstk will mark required number of pages as committed / throw stack overflow exception if can't.
    ;  - input:  r4 = the number of WORDS (word = 4 bytes) to allocate,
    ;  - output: r4 = total number of BYTES probed/allocated.
    blx     __chkstk            ;now r4 = the space to allocate, r5 = actual space needed for values on stack, r4 >= r5.

    ;offset stack by the amount of space we'll need.
    sub     sp,sp,r4

    mov     r4,r3                               ;copy entryPoint to r4 so we can use r3 as a scratch variable

    add     r2,r2,#8                            ;add 8 to r2 (so it is the address of the first value to be placed on the stack).
    mov     r12,#0                              ;offset for getting values/storing on stack.

|argN|
    cmp     r5,r12
    beq     arg2                                ;if r5 == r12, no more values need to go onto the stack.

        ldr     r3,[r2,r12]                     ;r3 = *(r2 + r12)
        str     r3,[sp,r12]                     ;*(sp + r12) = r3

        add     r12,r12,#4                      ;offset increases by 4.
    b   argN                                    ;goto argN

|arg2|
    ; Verify that the call target is valid, and process last two arguments
#if defined(_CONTROL_FLOW_GUARD)
    mov     r5, r0    ; save argument registers
    mov     r6, r1
    mov     r8, r2

    mov     r0, r4    ; the target address to check
    mov32   r12, __guard_check_icall_fptr
    ldr     r12, [r12]
    blx     r12

    mov     r0, r5    ; restore argument registers
    mov     r1, r6
    mov     r2, r8
#endif

    ;Push values[1] into r3
    ldr     r3,[r2,#-4]                         ;r3 = *(r2-4) = values[1]

    ;Push values[0] into r2
    ldr     r2,[r2,#-8]                         ;r2 = *(r2-8) = values[0]

    blx     r4                                  ;call r4 (== entry point)

    EPILOG_STACK_RESTORE r7
#if defined(_CONTROL_FLOW_GUARD)
    EPILOG_POP r8-r9
#endif
    EPILOG_POP r4-r7,r11,pc

    NESTED_END

    END
