;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
include ksamd64.inc

        _TEXT SEGMENT

ifdef _CONTROL_FLOW_GUARD
    extrn __guard_check_icall_fptr:QWORD
endif
extrn __chkstk: PROC

        ;; [rsp + 8h] = arg0.
        ;; r9         = args size.
        ;; r8         = spill size.
        ;; rdx        = original frame pointer.
        ;; rcx        = target.
align 16
amd64_CallWithFakeFrame PROC

        ;; Call __chkstk to ensure the stack is extended properly. It expects size in rax.
        mov  rax, r8
        add  rax, r9
        cmp  rax, 1000h
        jl   chkstk_done
        call __chkstk

chkstk_done:
        ;; The stack walker uses this marker to skip this frame.
        lea rax, amd64_ReturnFromCallWithFakeFrame
        mov [rsp+8h], rax

        mov rax, [rsp + 28h]

        push rbp
        mov rbp, rdx

        ;; Frame spill size.
        sub rsp, r8

        ;; Save callee-saved xmm registers
        movapd xmmword ptr [rsp + 90h], xmm15
        movapd xmmword ptr [rsp + 80h], xmm14
        movapd xmmword ptr [rsp + 70h], xmm13
        movapd xmmword ptr [rsp + 60h], xmm12
        movapd xmmword ptr [rsp + 50h], xmm11
        movapd xmmword ptr [rsp + 40h], xmm10
        movapd xmmword ptr [rsp + 30h], xmm9
        movapd xmmword ptr [rsp + 20h], xmm8
        movapd xmmword ptr [rsp + 10h], xmm7
        movapd xmmword ptr [rsp], xmm6

        ;; Save all callee saved registers.
        push r15
        push r14
        push r13
        push r12
        push rdi
        push rsi
        push rbx

        ;; Frame args size.
        sub  rsp, r9

        rex_jmp_reg rcx

amd64_CallWithFakeFrame ENDP

        ;; r9 = args size.
        ;; r8 = spill size.
align 16
amd64_ReturnFromCallWithFakeFrame PROC
        add  rsp, r9

        pop  rbx
        pop  rsi
        pop  rdi
        pop  r12
        pop  r13
        pop  r14
        pop  r15

        ;; Restore callee-saved xmm registers
        movapd xmm6, xmmword ptr [rsp]
        movapd xmm7, xmmword ptr [rsp + 10h]
        movapd xmm8, xmmword ptr [rsp + 20h]
        movapd xmm9, xmmword ptr [rsp + 30h]
        movapd xmm10, xmmword ptr [rsp + 40h]
        movapd xmm11, xmmword ptr [rsp + 50h]
        movapd xmm12, xmmword ptr [rsp + 60h]
        movapd xmm13, xmmword ptr [rsp + 70h]
        movapd xmm14, xmmword ptr [rsp + 80h]
        movapd xmm15, xmmword ptr [rsp + 90h]

        add  rsp, r8

        pop  rbp

        ;; Return to the real caller.
        ret
amd64_ReturnFromCallWithFakeFrame ENDP

        _TEXT ENDS
        end
