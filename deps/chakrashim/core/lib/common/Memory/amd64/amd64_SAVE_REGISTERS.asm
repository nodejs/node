;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------


include ksamd64.inc

        _TEXT SEGMENT

;void amd64_SAVE_REGISTERS(registers)
;
;   This method pushes the 16 general purpose registers into the passed in array.
;   By convention, the stack pointer will always be stored at registers[0]
;
;       void* registers[16];
;       amd64_SAVE_REGISTERS(registers);
;
amd64_SAVE_REGISTERS PROC
        mov [rcx+00h], rsp
        mov [rcx+08h], rax
        mov [rcx+10h], rbx
        mov [rcx+18h], rcx
        mov [rcx+20h], rdx
        mov [rcx+28h], rbp
        mov [rcx+30h], rsi
        mov [rcx+38h], rdi
        mov [rcx+40h], r8
        mov [rcx+48h], r9
        mov [rcx+50h], r10
        mov [rcx+58h], r11
        mov [rcx+60h], r12
        mov [rcx+68h], r13
        mov [rcx+70h], r14
        mov [rcx+78h], r15
        ret

amd64_SAVE_REGISTERS ENDP

        _TEXT ENDS
        end


