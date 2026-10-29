COMMENT @
 <copyright>
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
</copyright>
@

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; ;;; Intel Processor Trace Marker Functionality
;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


_TEXT	segment

        public	__itt_pt_mark
        public	__itt_pt_event
        public	__itt_pt_mark_event
        public  __itt_pt_mark_threshold
        public  __itt_pt_write
        public  __itt_pt_byte

        align   10h

;;; void __itt_pt_mark(unsigned char index);

__itt_pt_mark   proc

        and     rcx,0ffh
        lea     r10,offset __itt_pt_mark_call_table
        lea     rcx,[r10 + rcx * 4]
        jmp	rcx

        align   04h

        dd      0, 1, 2, 3      ;;; GUID

        dd      0fadefadeh      ;;; magic marker

__itt_pt_mark_call_table:

        dd      256 dup(0000c2c3h OR (( $ - offset __itt_pt_mark_call_table) SHL 14))
        
__itt_pt_mark   endp

        align   10h

__itt_pt_byte   proc

        and     rcx,0ffh
        lea     r10,offset __itt_pt_byte_call_table
        add     rcx,r10
        jmp     rcx

        align   04h

        dd      0, 1, 2, 3      ;;; GUID

        dd      0fadedeafh      ;;; magic marker

__itt_pt_byte_call_table:

        db      256 dup(0c3h)

__itt_pt_byte   endp

        align   10h

__itt_pt_event  proc

        rdpmc

        mov     cl,al
        call    __itt_pt_byte
        mov     cl,ah
        call    __itt_pt_byte
        shr     eax,16
        mov     cl,al
        call    __itt_pt_byte
        mov     cl,ah
        call    __itt_pt_byte

        mov     cl,dl
        call    __itt_pt_byte
        mov     cl,dh
        call    __itt_pt_byte
        shr     edx,16
        mov     cl,dl
        call    __itt_pt_byte
        mov     cl,dh
        call    __itt_pt_byte

        ret

__itt_pt_event  endp

        align   10h

__itt_pt_mark_event     proc

        test    rcx,1
        jnz     odd
        mov     r8,rcx
        xor     rcx,rcx
        call    __itt_pt_event
        mov     rcx,r8
        jmp     __itt_pt_mark

odd:
        call    __itt_pt_mark
        xor     rcx,rcx
        jmp     __itt_pt_event

__itt_pt_mark_event     endp

        align   10h

__itt_pt_flush  proc

        lea     rax,offset __itt_pt_mark_flush_1
        jmp     rax

        align   10h
        nop
__itt_pt_mark_flush_1:
        lea     rax,offset __itt_pt_mark_flush_2
        jmp     rax

        align   10h
        nop
        nop
__itt_pt_mark_flush_2:
        lea     rax,offset __itt_pt_mark_flush_3
        jmp     rax

        align   10h
        nop
        nop
        nop
__itt_pt_mark_flush_3:
        ret

__itt_pt_flush  endp

        align   10h

;;; int __itt_pt_mark_threshold(unsigned char index, unsigned long long* tmp, int threshold);

__itt_pt_mark_threshold proc
        mov     r9,rcx  ;;; index
        mov     r10,rdx ;;; tmp
        xor     rdx,rdx
        xor     rax,rax
        test    rcx,1
        jnz     mark_end
mark_begin:
        mov     rcx,(1 SHL 30) + 1
        rdpmc
        shl     rdx,32
        or      rdx,rax
        mov     [r10],rdx
        mov     rcx,r9
        jmp     __itt_pt_mark
mark_end:
        mov     rcx,(1 SHL 30) + 1
        rdpmc
        shl     rdx,32
        or      rdx,rax
        sub     rdx,[r10]
        cmp     rdx,r8  ;;; threshold
        mov     rcx,r9
        jnc     found
        jmp     __itt_pt_mark
found:
        call    __itt_pt_mark
        jmp     __itt_pt_flush

__itt_pt_mark_threshold endp

;;; PTWRITE

        align   10h

;;; void __itt_pt_write(unsigned long long value);

        dd      0, 1, 2, 3      ;;; GUID

__itt_pt_write  proc

;;;        ptwrite rcx
        db      0F3h, 48h, 0Fh, 0AEh, 0E1h
        ret

__itt_pt_write  endp

;;;

_TEXT	ends
        end
