;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;
;   arm_CallEhFrame() and arm_CallCatch() both thunk into jitted code at the
;   start of an EH region. The purpose is to restore the frame pointer (r11)
;   and locals pointer (r7) to the appropriate values for executing the parent
;   function and to create a local frame that can be unwound using the parent
;   function's pdata. The parent's frame looks like this:
;
;-------------------
;   {r0-r3}     -- homed parameters
;   lr          -- address from which parent was called
;   r11         -- saved frame pointer, pointed to by current r11
;   arg obj
;   {r4-r10}    -- non-volatile registers: all of them are saved
;   {d8-d15}    -- non-volatile double registers: all of them are saved
;   locals area -- pointed to by r7
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
;   4. Restore {d8-d15} (non-volatile double registers restored)
;   5. Restore {r4-r10,r12} (non-volatile registers restored, sp points to saved r11)
;   6. Restore r11
;   7. Load lr into pc and deallocate remaining stack.
;
;   The prologs for the assembly thunks allocate a frame that can be unwound by executing
;   the above steps, although we don't allocate a locals area and don't know the size of the
;   stack args. The caller doesn't return to this thunk; it executes its own epilog and
;   returns to the caller of the thunk (one of the runtime try helpers).


    ; Windows version

    OPT 2       ; disable listing

#include "ksarm.h"

    OPT 1       ; re-enable listing

    TTL Lib\Common\arm\arm_CallEhFrame.asm

#if 0 && defined(_CONTROL_FLOW_GUARD)
    IMPORT __guard_check_icall_fptr
#endif

    IMPORT __chkstk
    EXPORT  arm_CallEhFrame

    TEXTAREA

    NESTED_ENTRY arm_CallEhFrame

    ; Params:
    ; r0 -- thunk target
    ; r1 -- frame pointer
    ; r2 -- locals pointer
    ; r3 -- size of stack args area

    ; Home params and save registers
    ; Push r11 to create the equivalent of the stack nested function list (doesn't matter what is stored there)
    ; Push r12 to create the equivalent of the arguments object slot (doesn't matter what is stored there)
    PROLOG_PUSH {r0-r3}
    PROLOG_PUSH {r11,lr}
    PROLOG_PUSH {r4-r12}
    PROLOG_VPUSH {d8-d15}
    ; Save a pointer to the saved registers
    PROLOG_STACK_SAVE r4
    PROLOG_PUSH r4

    ; Set up the frame pointer and locals pointer
    mov r7,r2
    mov r11,r1

    ; Allocate the arg out area, calling chkstk if necessary
    cmp r3,4096
    bge chkstk_call
    sub sp,r3

    ; Verify that the call target is valid
#if 0 && defined(_CONTROL_FLOW_GUARD)
    ; we have used the r1-r3 info to set up the fake frame
    ; they aren't needed by the jitted code that we're going to thunk to
    ; so we don't preserve them across the __guard_check_icall_fptr call
    mov     r5, r0

    mov32   r12, __guard_check_icall_fptr
    ldr     r12, [r12]
    blx     r12

    mov     r0, r5
#endif

    ; Thunk to the jitted code (and don't return)
    bx  r0

|chkstk_call|
    ; Call chkstk, passing a DWORD count in r4
    lsr r4,r3,#2
    bl |__chkstk|
    ; r4 is now the byte count to be allocated
    sub sp,r4

    ; Thunk to the jitted code (and don't return)
    bx  r0

    NESTED_END arm_CallEhFrame

    ; arm_CallCatch() is similar to arm_CallEhFrame() except that we also pass the catch object to the jitted code

    EXPORT  arm_CallCatch

    TEXTAREA

    NESTED_ENTRY arm_CallCatch

    ; Params:
    ; r0 -- thunk target
    ; r1 -- frame pointer
    ; r2 -- locals pointer
    ; r3 -- size of stack args area
    ; [sp] -- exception object

    ; Home params and save registers
    ; Push r11 to create the equivalent of the stack nested function list (doesn't matter what is stored there)
    ; Push r12 to create the equivalent of the arguments object slot (doesn't matter what is stored there)
    PROLOG_PUSH {r0-r3}
    PROLOG_PUSH {r11,lr}
    PROLOG_PUSH {r4-r12}
    PROLOG_VPUSH {d8-d15}
    ; Save a pointer to the saved registers
    PROLOG_STACK_SAVE r4
    PROLOG_PUSH r4

    ; Set up the frame pointer and locals pointer
    mov r7,r2
    mov r11,r1

    ; Load the exception object from [sp, 16 (homed params) + 44 (saved registers) + 64 (double registers) + 4 (saved SP) == 128]
    ldr r1,[sp,#128]

    ; Allocate the arg out area, calling chkstk if necessary
    cmp r3,4096
    bge chkstk_call_catch
    sub sp,r3

    ; Verify that the call target is valid
#if 0 && defined(_CONTROL_FLOW_GUARD)
    ; we have used the r1-r3 info to set up the fake frame
    ; they aren't needed by the jitted code that we're going to thunk to
    ; so we don't preserve them across the __guard_check_icall_fptr call
    mov     r5, r0

    mov32   r12, __guard_check_icall_fptr
    ldr     r12, [r12]
    blx     r12

    mov     r0, r5
#endif

    ; Thunk to the jitted code (and don't return)
    bx  r0

|chkstk_call_catch|
    ; Call chkstk, passing a DWORD count in r4
    lsr r4,r3,#2
    bl |__chkstk|
    ; r4 is now the byte count to be allocated
    sub sp,r4

    ; Thunk to the jitted code (and don't return)
    bx r0

    NESTED_END arm_CallCatch

    END
