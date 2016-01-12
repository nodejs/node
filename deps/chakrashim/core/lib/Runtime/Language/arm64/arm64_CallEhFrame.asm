;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;
;   arm64_CallEhFrame() and arm64_CallCatch() both thunk into jitted code at the
;   start of an EH region. The purpose is to restore the frame pointer (fp)
;   and locals pointer (x28) to the appropriate values for executing the parent
;   function and to create a local frame that can be unwound using the parent
;   function's pdata. The parent's frame looks like this:
;
;-------------------
;   {x0-x7}     -- homed parameters
;   lr          -- address from which parent was called
;   fp          -- saved frame pointer, pointed to by current fp
;   arg obj
;   {x19-x28}   -- non-volatile registers: all of them are saved
;   {q8-q15}    -- non-volatile double registers: all of them are saved
;   locals area -- pointed to by x28
;   pointer to non-volatile register area above
;   stack args
;-------------------
;
;   The reason for the "pointer to non-volatile register area" is to allow the
;   unwinder to deallocate the locals area regardless of its size. So this thunk can skip
;   the allocation of the locals area altogether, and unwinding still works.
;   The unwind pseudo-codes for the above prolog look like:
;
;   1. Deallocate stack args (sp now points to "pointer to non-volatile register area")
;   2. Restore rN (rN now points to first saved register)
;   3. Copy rN to sp (sp now points to first saved register)
;   4. Restore {q8-q15} (non-volatile double registers restored)
;   5. Restore {x19-x28} (non-volatile registers restored, sp points to saved r11)
;   6. Restore fp
;   7. Load lr into pc and deallocate remaining stack.
;
;   The prologs for the assembly thunks allocate a frame that can be unwound by executing
;   the above steps, although we don't allocate a locals area and don't know the size of the
;   stack args. The caller doesn't return to this thunk; it executes its own epilog and
;   returns to the caller of the thunk (one of the runtime try helpers).


    ; Windows version

    OPT 2       ; disable listing

#include "ksarm64.h"

    OPT 1       ; re-enable listing

    TTL Lib\Common\arm\arm64_CallEhFrame.asm

    IMPORT __chkstk
    EXPORT  arm64_CallEhFrame

    TEXTAREA

    NESTED_ENTRY arm64_CallEhFrame

    ; Params:
    ; x0 -- thunk target
    ; x1 -- frame pointer
    ; x2 -- locals pointer
    ; x3 -- size of stack args area

    ; Home params and save registers
    PROLOG_SAVE_REG_PAIR fp, lr, #-80!
    PROLOG_NOP stp x0, x1, [sp, #16]
    PROLOG_NOP stp x2, x3, [sp, #32]
    PROLOG_NOP stp x4, x5, [sp, #48]
    PROLOG_NOP stp x6, x7, [sp, #64]
    PROLOG_STACK_ALLOC (10*8 + 8*16 + 32)
    PROLOG_NOP stp q8, q9, [sp, #(16 + 0*16)]
    PROLOG_NOP stp q10, q11, [sp, #(16 + 2*16)]
    PROLOG_NOP stp q12, q13, [sp, #(16 + 4*16)]
    PROLOG_NOP stp q14, q15, [sp, #(16 + 6*16)]
    PROLOG_SAVE_REG_PAIR x19, x20, #(16 + 8*16 + 0*8)
    PROLOG_SAVE_REG_PAIR x21, x22, #(16 + 8*16 + 2*8)
    PROLOG_SAVE_REG_PAIR x23, x24, #(16 + 8*16 + 4*8)
    PROLOG_SAVE_REG_PAIR x25, x26, #(16 + 8*16 + 6*8)
    PROLOG_SAVE_REG_PAIR x27, x28, #(16 + 8*16 + 8*8)
    ; Save a pointer to the saved registers
    mov     x16, sp
    str     x16, [sp, #0]

    ; Set up the frame pointer and locals pointer
    mov     x28, x2
    mov     fp, x1

    ; Allocate the arg out area, calling chkstk if necessary
    cmp     x3,#4095
    bgt     chkstk_call
    sub     sp,sp,x3

    ; Thunk to the jitted code (and don't return)
    br      x0

|chkstk_call|
    ; Call chkstk, passing a size/16 count in x15
    lsr     x15,x3,#4
    bl      |__chkstk|
    sub     sp,sp,x15,lsl #4

    ; Thunk to the jitted code (and don't return)
    br      x0

    NESTED_END arm64_CallEhFrame

    ; arm64_CallCatch() is similar to arm64_CallEhFrame() except that we also pass the catch object to the jitted code

    EXPORT  arm64_CallCatch

    TEXTAREA

    NESTED_ENTRY arm64_CallCatch

    ; Params:
    ; x0 -- thunk target
    ; x1 -- frame pointer
    ; x2 -- locals pointer
    ; x3 -- size of stack args area
    ; x4 -- exception object

    ; Home params and save registers
    PROLOG_SAVE_REG_PAIR fp, lr, #-80!
    PROLOG_NOP stp x0, x1, [sp, #16]
    PROLOG_NOP stp x2, x3, [sp, #32]
    PROLOG_NOP stp x4, x5, [sp, #48]
    PROLOG_NOP stp x6, x7, [sp, #64]
    PROLOG_STACK_ALLOC (10*8 + 8*16 + 32)
    PROLOG_NOP stp q8, q9, [sp, #(16 + 0*16)]
    PROLOG_NOP stp q10, q11, [sp, #(16 + 2*16)]
    PROLOG_NOP stp q12, q13, [sp, #(16 + 4*16)]
    PROLOG_NOP stp q14, q15, [sp, #(16 + 6*16)]
    PROLOG_SAVE_REG_PAIR x19, x20, #(16 + 8*16 + 0*8)
    PROLOG_SAVE_REG_PAIR x21, x22, #(16 + 8*16 + 2*8)
    PROLOG_SAVE_REG_PAIR x23, x24, #(16 + 8*16 + 4*8)
    PROLOG_SAVE_REG_PAIR x25, x26, #(16 + 8*16 + 6*8)
    PROLOG_SAVE_REG_PAIR x27, x28, #(16 + 8*16 + 8*8)
    ; Save a pointer to the saved registers
    mov     x16, sp
    str     x16, [sp, #0]

    ; Set up the frame pointer and locals pointer
    mov     x28, x2
    mov     fp, x1

    ; Allocate the arg out area, calling chkstk if necessary
    cmp     x3,#4095
    mov     x1, x4
    bgt     chkstk_call
    sub     sp,sp,x3

    ; Thunk to the jitted code (and don't return)
    br      x0

|chkstk_call_catch|
    ; Call chkstk, passing a size/16 count in x15
    lsr     x15,x3,#4
    bl      |__chkstk|
    sub     sp,sp,x15,lsl #4

    ; Thunk to the jitted code (and don't return)
    br      x0

    NESTED_END arm64_CallCatch

    END
