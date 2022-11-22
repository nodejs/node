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
        movzbl  4(%esp), %eax
//        and     $0xff, %eax
        lea     __itt_pt_mark_call_table(,%eax,4), %eax
        jmp     *%eax
        .align  4

        .long   0, 1, 2, 3      // GUID
        .long   0xfadefade

__itt_pt_mark_call_table:
///        .fill 256,4,(0x0000c2c3 | (( . - __itt_pt_mark_call_table) << 14))
        ret
        ret    $0x0
        ret
        ret    $0x1
        ret
        ret    $0x2
        ret
        ret    $0x3
        ret
        ret    $0x4
        ret
        ret    $0x5
        ret
        ret    $0x6
        ret
        ret    $0x7
        ret
        ret    $0x8
        ret
        ret    $0x9
        ret
        ret    $0xa
        ret
        ret    $0xb
        ret
        ret    $0xc
        ret
        ret    $0xd
        ret
        ret    $0xe
        ret
        ret    $0xf

        ret
        ret    $0x10
        ret
        ret    $0x11
        ret
        ret    $0x12
        ret
        ret    $0x13
        ret
        ret    $0x14
        ret
        ret    $0x15
        ret
        ret    $0x16
        ret
        ret    $0x17
        ret
        ret    $0x18
        ret
        ret    $0x19
        ret
        ret    $0x1a
        ret
        ret    $0x1b
        ret
        ret    $0x1c
        ret
        ret    $0x1d
        ret
        ret    $0x1e
        ret
        ret    $0x1f

        ret
        ret    $0x20
        ret
        ret    $0x21
        ret
        ret    $0x22
        ret
        ret    $0x23
        ret
        ret    $0x24
        ret
        ret    $0x25
        ret
        ret    $0x26
        ret
        ret    $0x27
        ret
        ret    $0x28
        ret
        ret    $0x29
        ret
        ret    $0x2a
        ret
        ret    $0x2b
        ret
        ret    $0x2c
        ret
        ret    $0x2d
        ret
        ret    $0x2e
        ret
        ret    $0x2f

        ret
        ret    $0x30
        ret
        ret    $0x31
        ret
        ret    $0x32
        ret
        ret    $0x33
        ret
        ret    $0x34
        ret
        ret    $0x35
        ret
        ret    $0x36
        ret
        ret    $0x37
        ret
        ret    $0x38
        ret
        ret    $0x39
        ret
        ret    $0x3a
        ret
        ret    $0x3b
        ret
        ret    $0x3c
        ret
        ret    $0x3d
        ret
        ret    $0x3e
        ret
        ret    $0x3f

        ret
        ret    $0x40
        ret
        ret    $0x41
        ret
        ret    $0x42
        ret
        ret    $0x43
        ret
        ret    $0x44
        ret
        ret    $0x45
        ret
        ret    $0x46
        ret
        ret    $0x47
        ret
        ret    $0x48
        ret
        ret    $0x49
        ret
        ret    $0x4a
        ret
        ret    $0x4b
        ret
        ret    $0x4c
        ret
        ret    $0x4d
        ret
        ret    $0x4e
        ret
        ret    $0x4f

        ret
        ret    $0x50
        ret
        ret    $0x51
        ret
        ret    $0x52
        ret
        ret    $0x53
        ret
        ret    $0x54
        ret
        ret    $0x55
        ret
        ret    $0x56
        ret
        ret    $0x57
        ret
        ret    $0x58
        ret
        ret    $0x59
        ret
        ret    $0x5a
        ret
        ret    $0x5b
        ret
        ret    $0x5c
        ret
        ret    $0x5d
        ret
        ret    $0x5e
        ret
        ret    $0x5f

        ret
        ret    $0x60
        ret
        ret    $0x61
        ret
        ret    $0x62
        ret
        ret    $0x63
        ret
        ret    $0x64
        ret
        ret    $0x65
        ret
        ret    $0x66
        ret
        ret    $0x67
        ret
        ret    $0x68
        ret
        ret    $0x69
        ret
        ret    $0x6a
        ret
        ret    $0x6b
        ret
        ret    $0x6c
        ret
        ret    $0x6d
        ret
        ret    $0x6e
        ret
        ret    $0x6f

        ret
        ret    $0x70
        ret
        ret    $0x71
        ret
        ret    $0x72
        ret
        ret    $0x73
        ret
        ret    $0x74
        ret
        ret    $0x75
        ret
        ret    $0x76
        ret
        ret    $0x77
        ret
        ret    $0x78
        ret
        ret    $0x79
        ret
        ret    $0x7a
        ret
        ret    $0x7b
        ret
        ret    $0x7c
        ret
        ret    $0x7d
        ret
        ret    $0x7e
        ret
        ret    $0x7f

        ret
        ret    $0x80
        ret
        ret    $0x81
        ret
        ret    $0x82
        ret
        ret    $0x83
        ret
        ret    $0x84
        ret
        ret    $0x85
        ret
        ret    $0x86
        ret
        ret    $0x87
        ret
        ret    $0x88
        ret
        ret    $0x89
        ret
        ret    $0x8a
        ret
        ret    $0x8b
        ret
        ret    $0x8c
        ret
        ret    $0x8d
        ret
        ret    $0x8e
        ret
        ret    $0x8f

        ret
        ret    $0x90
        ret
        ret    $0x91
        ret
        ret    $0x92
        ret
        ret    $0x93
        ret
        ret    $0x94
        ret
        ret    $0x95
        ret
        ret    $0x96
        ret
        ret    $0x97
        ret
        ret    $0x98
        ret
        ret    $0x99
        ret
        ret    $0x9a
        ret
        ret    $0x9b
        ret
        ret    $0x9c
        ret
        ret    $0x9d
        ret
        ret    $0x9e
        ret
        ret    $0x9f

        ret
        ret    $0xa0
        ret
        ret    $0xa1
        ret
        ret    $0xa2
        ret
        ret    $0xa3
        ret
        ret    $0xa4
        ret
        ret    $0xa5
        ret
        ret    $0xa6
        ret
        ret    $0xa7
        ret
        ret    $0xa8
        ret
        ret    $0xa9
        ret
        ret    $0xaa
        ret
        ret    $0xab
        ret
        ret    $0xac
        ret
        ret    $0xad
        ret
        ret    $0xae
        ret
        ret    $0xaf

        ret
        ret    $0xb0
        ret
        ret    $0xb1
        ret
        ret    $0xb2
        ret
        ret    $0xb3
        ret
        ret    $0xb4
        ret
        ret    $0xb5
        ret
        ret    $0xb6
        ret
        ret    $0xb7
        ret
        ret    $0xb8
        ret
        ret    $0xb9
        ret
        ret    $0xba
        ret
        ret    $0xbb
        ret
        ret    $0xbc
        ret
        ret    $0xbd
        ret
        ret    $0xbe
        ret
        ret    $0xbf

        ret
        ret    $0xc0
        ret
        ret    $0xc1
        ret
        ret    $0xc2
        ret
        ret    $0xc3
        ret
        ret    $0xc4
        ret
        ret    $0xc5
        ret
        ret    $0xc6
        ret
        ret    $0xc7
        ret
        ret    $0xc8
        ret
        ret    $0xc9
        ret
        ret    $0xca
        ret
        ret    $0xcb
        ret
        ret    $0xcc
        ret
        ret    $0xcd
        ret
        ret    $0xce
        ret
        ret    $0xcf

        ret
        ret    $0xd0
        ret
        ret    $0xd1
        ret
        ret    $0xd2
        ret
        ret    $0xd3
        ret
        ret    $0xd4
        ret
        ret    $0xd5
        ret
        ret    $0xd6
        ret
        ret    $0xd7
        ret
        ret    $0xd8
        ret
        ret    $0xd9
        ret
        ret    $0xda
        ret
        ret    $0xdb
        ret
        ret    $0xdc
        ret
        ret    $0xdd
        ret
        ret    $0xde
        ret
        ret    $0xdf

        ret
        ret    $0xe0
        ret
        ret    $0xe1
        ret
        ret    $0xe2
        ret
        ret    $0xe3
        ret
        ret    $0xe4
        ret
        ret    $0xe5
        ret
        ret    $0xe6
        ret
        ret    $0xe7
        ret
        ret    $0xe8
        ret
        ret    $0xe9
        ret
        ret    $0xea
        ret
        ret    $0xeb
        ret
        ret    $0xec
        ret
        ret    $0xed
        ret
        ret    $0xee
        ret
        ret    $0xef

        ret
        ret    $0xf0
        ret
        ret    $0xf1
        ret
        ret    $0xf2
        ret
        ret    $0xf3
        ret
        ret    $0xf4
        ret
        ret    $0xf5
        ret
        ret    $0xf6
        ret
        ret    $0xf7
        ret
        ret    $0xf8
        ret
        ret    $0xf9
        ret
        ret    $0xfa
        ret
        ret    $0xfb
        ret
        ret    $0xfc
        ret
        ret    $0xfd
        ret
        ret    $0xfe
        ret
        ret    $0xff

        .align  16

__itt_pt_byte:

        movl    4(%esp), %ecx

__itt_pt_byte_:

        and     $0xff, %ecx
        lea     __itt_pt_byte_call_table(,%ecx,1), %ecx
        jmp     *%ecx

        .align  4

        .long   0, 1, 2, 3      // GUID
        .long   0xfadedeaf

__itt_pt_byte_call_table:

        .fill   256,1,0xc3

        .align  16

__itt_pt_event:

        push   %ecx
        mov    8(%esp), %ecx
        rdpmc

        mov     %al,%cl
        call    __itt_pt_byte_
        shr     $8,%eax
        mov     %al,%cl
        call    __itt_pt_byte_
        shr     $8,%eax
        mov     %al,%cl
        call    __itt_pt_byte_
        shr     $8,%eax
        mov     %al,%cl
        call    __itt_pt_byte_

        mov     %dl,%cl
        call    __itt_pt_byte_
        shr     $8,%edx
        mov     %dl,%cl
        call    __itt_pt_byte_
        shr     $8,%edx
        mov     %dl,%cl
        call    __itt_pt_byte_
        shr     $8,%edx
        mov     %dl,%cl
        call    __itt_pt_byte_

        pop    %ecx
        ret

        .align  16

__itt_pt_mark_event:

        testl   $1,4(%esp)
        jnz     odd
        pushl   $0
        call    __itt_pt_event
        add     $2,%esp
        jmp     __itt_pt_mark

odd:
        pushl  4(%esp)
        call    __itt_pt_mark
        add     $2,%esp
        movl    $0,4(%esp)
        jmp     __itt_pt_event


        .align  16

__itt_pt_flush:

        lea     __itt_pt_mark_flush_1,%eax
        jmp     *%eax

        .align   16
        nop
__itt_pt_mark_flush_1:
        lea     __itt_pt_mark_flush_2,%eax
        jmp     *%eax

        .align   16
        nop
        nop
__itt_pt_mark_flush_2:
        lea     __itt_pt_mark_flush_3,%eax
        jmp     *%eax

        .align   16
        nop
        nop
        nop
__itt_pt_mark_flush_3:
        ret

        .align  16

// int __itt_pt_mark_threshold(unsigned char index, unsigned long long* tmp, int threshold);

__itt_pt_mark_threshold:
        //  4(%esp) == index
        //  8(%esp) == tmp
        // 12(%esp) == threshold
        xor     %edx,%edx
        xor     %eax,%eax

        testl   $1,4(%esp)
        jnz     mark_end
mark_begin:
        mov     $((1 << 30) + 1),%ecx
        rdpmc
        mov     8(%esp), %ecx
        mov     %eax, (%ecx)
        mov     %edx,4(%ecx)
        jmp     __itt_pt_mark
mark_end:
        mov     $((1 << 30) + 1),%ecx
        rdpmc
        mov     8(%esp), %ecx
        sub      (%ecx), %eax
        sbb     4(%ecx), %edx

        sub     12(%esp), %eax // threshold
        jnc     found
        sbb     $0, %edx
        jnc     found
        jmp     __itt_pt_mark
found:
        call    __itt_pt_mark
        jmp     __itt_pt_flush

// PTWRITE

        .align  16

// void __itt_pt_write(unsigned long long value);

        .long   0, 1, 2, 3      // GUID

__itt_pt_write:

//        ptwrite dword ptr [esp + 4]
        .byte   0xF3, 0x0F, 0xAE, 0x64, 0x24, 0x04
        ret
