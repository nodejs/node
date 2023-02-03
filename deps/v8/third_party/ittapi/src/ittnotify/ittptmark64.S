/* <copyright>
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright (c) 2017-2020 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/

  BSD LICENSE

  Copyright (c) 2017-2020 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
</copyright> */

// /////////////////////////////////////////////////////////////////////////
////// Intel Processor Trace Marker Functionality
////////////////////////////////////////////////////////////////////////////

        .text
        .align  16
        .globl  __itt_pt_mark
        .globl  __itt_pt_event
        .globl  __itt_pt_mark_event
        .globl  __itt_pt_mark_threshold
        .globl  __itt_pt_byte
        .globl  __itt_pt_write

/// void __itt_pt_mark(unsigned char index);
__itt_pt_mark:
__itt_pt_mark_int:
        and     $0xff, %rdi
        call    __itt_pt_mark_pic
__itt_pt_mark_pic:
        popq    %rax
        lea     (__itt_pt_mark_call_table - __itt_pt_mark_pic) (%rax,%rdi,4), %rdi
        jmp     *%rdi

        .long   0, 1, 2, 3      // GUID
        .long   0xfadefade

__itt_pt_mark_call_table:
        retq
        retq    $0x0
        retq
        retq    $0x1
        retq
        retq    $0x2
        retq
        retq    $0x3
        retq
        retq    $0x4
        retq
        retq    $0x5
        retq
        retq    $0x6
        retq
        retq    $0x7
        retq
        retq    $0x8
        retq
        retq    $0x9
        retq
        retq    $0xa
        retq
        retq    $0xb
        retq
        retq    $0xc
        retq
        retq    $0xd
        retq
        retq    $0xe
        retq
        retq    $0xf

        retq
        retq    $0x10
        retq
        retq    $0x11
        retq
        retq    $0x12
        retq
        retq    $0x13
        retq
        retq    $0x14
        retq
        retq    $0x15
        retq
        retq    $0x16
        retq
        retq    $0x17
        retq
        retq    $0x18
        retq
        retq    $0x19
        retq
        retq    $0x1a
        retq
        retq    $0x1b
        retq
        retq    $0x1c
        retq
        retq    $0x1d
        retq
        retq    $0x1e
        retq
        retq    $0x1f

        retq
        retq    $0x20
        retq
        retq    $0x21
        retq
        retq    $0x22
        retq
        retq    $0x23
        retq
        retq    $0x24
        retq
        retq    $0x25
        retq
        retq    $0x26
        retq
        retq    $0x27
        retq
        retq    $0x28
        retq
        retq    $0x29
        retq
        retq    $0x2a
        retq
        retq    $0x2b
        retq
        retq    $0x2c
        retq
        retq    $0x2d
        retq
        retq    $0x2e
        retq
        retq    $0x2f

        retq
        retq    $0x30
        retq
        retq    $0x31
        retq
        retq    $0x32
        retq
        retq    $0x33
        retq
        retq    $0x34
        retq
        retq    $0x35
        retq
        retq    $0x36
        retq
        retq    $0x37
        retq
        retq    $0x38
        retq
        retq    $0x39
        retq
        retq    $0x3a
        retq
        retq    $0x3b
        retq
        retq    $0x3c
        retq
        retq    $0x3d
        retq
        retq    $0x3e
        retq
        retq    $0x3f

        retq
        retq    $0x40
        retq
        retq    $0x41
        retq
        retq    $0x42
        retq
        retq    $0x43
        retq
        retq    $0x44
        retq
        retq    $0x45
        retq
        retq    $0x46
        retq
        retq    $0x47
        retq
        retq    $0x48
        retq
        retq    $0x49
        retq
        retq    $0x4a
        retq
        retq    $0x4b
        retq
        retq    $0x4c
        retq
        retq    $0x4d
        retq
        retq    $0x4e
        retq
        retq    $0x4f

        retq
        retq    $0x50
        retq
        retq    $0x51
        retq
        retq    $0x52
        retq
        retq    $0x53
        retq
        retq    $0x54
        retq
        retq    $0x55
        retq
        retq    $0x56
        retq
        retq    $0x57
        retq
        retq    $0x58
        retq
        retq    $0x59
        retq
        retq    $0x5a
        retq
        retq    $0x5b
        retq
        retq    $0x5c
        retq
        retq    $0x5d
        retq
        retq    $0x5e
        retq
        retq    $0x5f

        retq
        retq    $0x60
        retq
        retq    $0x61
        retq
        retq    $0x62
        retq
        retq    $0x63
        retq
        retq    $0x64
        retq
        retq    $0x65
        retq
        retq    $0x66
        retq
        retq    $0x67
        retq
        retq    $0x68
        retq
        retq    $0x69
        retq
        retq    $0x6a
        retq
        retq    $0x6b
        retq
        retq    $0x6c
        retq
        retq    $0x6d
        retq
        retq    $0x6e
        retq
        retq    $0x6f

        retq
        retq    $0x70
        retq
        retq    $0x71
        retq
        retq    $0x72
        retq
        retq    $0x73
        retq
        retq    $0x74
        retq
        retq    $0x75
        retq
        retq    $0x76
        retq
        retq    $0x77
        retq
        retq    $0x78
        retq
        retq    $0x79
        retq
        retq    $0x7a
        retq
        retq    $0x7b
        retq
        retq    $0x7c
        retq
        retq    $0x7d
        retq
        retq    $0x7e
        retq
        retq    $0x7f

        retq
        retq    $0x80
        retq
        retq    $0x81
        retq
        retq    $0x82
        retq
        retq    $0x83
        retq
        retq    $0x84
        retq
        retq    $0x85
        retq
        retq    $0x86
        retq
        retq    $0x87
        retq
        retq    $0x88
        retq
        retq    $0x89
        retq
        retq    $0x8a
        retq
        retq    $0x8b
        retq
        retq    $0x8c
        retq
        retq    $0x8d
        retq
        retq    $0x8e
        retq
        retq    $0x8f

        retq
        retq    $0x90
        retq
        retq    $0x91
        retq
        retq    $0x92
        retq
        retq    $0x93
        retq
        retq    $0x94
        retq
        retq    $0x95
        retq
        retq    $0x96
        retq
        retq    $0x97
        retq
        retq    $0x98
        retq
        retq    $0x99
        retq
        retq    $0x9a
        retq
        retq    $0x9b
        retq
        retq    $0x9c
        retq
        retq    $0x9d
        retq
        retq    $0x9e
        retq
        retq    $0x9f

        retq
        retq    $0xa0
        retq
        retq    $0xa1
        retq
        retq    $0xa2
        retq
        retq    $0xa3
        retq
        retq    $0xa4
        retq
        retq    $0xa5
        retq
        retq    $0xa6
        retq
        retq    $0xa7
        retq
        retq    $0xa8
        retq
        retq    $0xa9
        retq
        retq    $0xaa
        retq
        retq    $0xab
        retq
        retq    $0xac
        retq
        retq    $0xad
        retq
        retq    $0xae
        retq
        retq    $0xaf

        retq
        retq    $0xb0
        retq
        retq    $0xb1
        retq
        retq    $0xb2
        retq
        retq    $0xb3
        retq
        retq    $0xb4
        retq
        retq    $0xb5
        retq
        retq    $0xb6
        retq
        retq    $0xb7
        retq
        retq    $0xb8
        retq
        retq    $0xb9
        retq
        retq    $0xba
        retq
        retq    $0xbb
        retq
        retq    $0xbc
        retq
        retq    $0xbd
        retq
        retq    $0xbe
        retq
        retq    $0xbf

        retq
        retq    $0xc0
        retq
        retq    $0xc1
        retq
        retq    $0xc2
        retq
        retq    $0xc3
        retq
        retq    $0xc4
        retq
        retq    $0xc5
        retq
        retq    $0xc6
        retq
        retq    $0xc7
        retq
        retq    $0xc8
        retq
        retq    $0xc9
        retq
        retq    $0xca
        retq
        retq    $0xcb
        retq
        retq    $0xcc
        retq
        retq    $0xcd
        retq
        retq    $0xce
        retq
        retq    $0xcf

        retq
        retq    $0xd0
        retq
        retq    $0xd1
        retq
        retq    $0xd2
        retq
        retq    $0xd3
        retq
        retq    $0xd4
        retq
        retq    $0xd5
        retq
        retq    $0xd6
        retq
        retq    $0xd7
        retq
        retq    $0xd8
        retq
        retq    $0xd9
        retq
        retq    $0xda
        retq
        retq    $0xdb
        retq
        retq    $0xdc
        retq
        retq    $0xdd
        retq
        retq    $0xde
        retq
        retq    $0xdf

        retq
        retq    $0xe0
        retq
        retq    $0xe1
        retq
        retq    $0xe2
        retq
        retq    $0xe3
        retq
        retq    $0xe4
        retq
        retq    $0xe5
        retq
        retq    $0xe6
        retq
        retq    $0xe7
        retq
        retq    $0xe8
        retq
        retq    $0xe9
        retq
        retq    $0xea
        retq
        retq    $0xeb
        retq
        retq    $0xec
        retq
        retq    $0xed
        retq
        retq    $0xee
        retq
        retq    $0xef

        retq
        retq    $0xf0
        retq
        retq    $0xf1
        retq
        retq    $0xf2
        retq
        retq    $0xf3
        retq
        retq    $0xf4
        retq
        retq    $0xf5
        retq
        retq    $0xf6
        retq
        retq    $0xf7
        retq
        retq    $0xf8
        retq
        retq    $0xf9
        retq
        retq    $0xfa
        retq
        retq    $0xfb
        retq
        retq    $0xfc
        retq
        retq    $0xfd
        retq
        retq    $0xfe
        retq
        retq    $0xff

        .align  16

__itt_pt_byte:
__itt_pt_byte_int:

        and     $0xff, %rdi
        call    __itt_pt_byte_pic
__itt_pt_byte_pic:
        popq    %rcx
        lea     (__itt_pt_byte_call_table - __itt_pt_byte_pic) (%rcx,%rdi,1), %rdi
        jmp     *%rdi

        .align  4

        .long   0, 1, 2, 3      // GUID

        .long   0xfadedeaf

__itt_pt_byte_call_table:

        .fill   256,1,0xc3

        .align  16

__itt_pt_event:
__itt_pt_event_int:

        pushq   %rcx
        mov     %rdi,%rcx
        rdpmc

        xor     %rdi, %rdi
        mov     %al, %dil
        call    __itt_pt_byte_int
        shr     $8, %eax
        mov     %al, %dil
        call    __itt_pt_byte_int
        shr     $8, %eax
        mov     %al, %dil
        call    __itt_pt_byte_int
        shr     $8, %eax
        mov     %al, %dil
        call    __itt_pt_byte_int

        mov     %dl, %dil
        call    __itt_pt_byte_int
        shr     $8, %edx
        mov     %dl, %dil
        call    __itt_pt_byte_int
        shr     $8, %edx
        mov     %dl, %dil
        call    __itt_pt_byte_int
        shr     $8, %edx
        mov     %dl, %dil
        call    __itt_pt_byte_int

        popq    %rcx
        ret

        .align  16

__itt_pt_mark_event:

        test    $1, %rdi
        jnz     odd
        mov     %rdi, %rsi
        xor     %rdi,%rdi
        call    __itt_pt_event_int
        mov     %rsi, %rdi
        jmp     __itt_pt_mark_int

odd:
        call    __itt_pt_mark_int
        xor     %rdi,%rdi
        jmp     __itt_pt_event_int


        .align  16

__itt_pt_flush:

        call    __itt_pt_flush_pic
__itt_pt_flush_pic:
        popq    %rdx
        lea     (__itt_pt_mark_flush_1 - __itt_pt_flush_pic) (%rdx), %rax
        jmp     *%rax

        .align   16
        nop
__itt_pt_mark_flush_1:
        lea     (__itt_pt_mark_flush_2 - __itt_pt_flush_pic) (%rdx), %rax
        jmp     *%rax

        .align   16
        nop
        nop
__itt_pt_mark_flush_2:
        lea     (__itt_pt_mark_flush_3 - __itt_pt_flush_pic) (%rdx), %rax
        jmp     *%rax

        .align   16
        nop
        nop
        nop
__itt_pt_mark_flush_3:
        ret

        .align  16

// int __itt_pt_mark_threshold(unsigned char index, unsigned long long* tmp, int threshold);

__itt_pt_mark_threshold:
        // rdi == index
        // rsi == tmp
        // rdx == threshold
        mov     %rdx, %r8 // r8 = threshold
        xor     %rdx, %rdx
        xor     %rax, %rax
        test    $1, %rdi
        jnz     mark_end
mark_begin:
        mov     $((1 << 30) + 1),%rcx
        rdpmc
        shl     $32, %rdx
        or      %rax, %rdx
        mov     %rdx, (%rsi)
        jmp     __itt_pt_mark_int
mark_end:
        mov     $((1 << 30) + 1),%rcx
        rdpmc
        shl     $32, %rdx
        or      %rax, %rdx
        sub     (%rsi), %rdx
        cmp     %r8, %rdx  // threshold
        jnc     found
        jmp     __itt_pt_mark_int
found:
        call    __itt_pt_mark_int
        jmp     __itt_pt_flush

// PTWRITE

        .align  16 

// void __itt_pt_write(unsigned long long value);

        .long   0, 1, 2, 3      // GUID

__itt_pt_write:

//        ptwrite rcx
        .byte   0xF3, 0x48, 0x0F, 0xAE, 0xE1
        ret
