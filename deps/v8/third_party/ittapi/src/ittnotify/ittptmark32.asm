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

.686p
.xmm
.model  FLAT

_TEXT	segment

        public	___itt_pt_mark
        public	___itt_pt_event
        public	___itt_pt_mark_event
        public  ___itt_pt_mark_threshold
        public  ___itt_pt_write
        public  ___itt_pt_byte

        align   10h

;;; void __itt_pt_mark(unsigned char index);

___itt_pt_mark  proc    near

        movzx   eax,byte ptr [esp + 4]
        lea     eax,__itt_pt_mark_call_table[eax * 4]
        jmp	eax

        align   04h

        dd      0, 1, 2, 3      ;;; GUID

        dd      0fadefadeh      ;;; magic marker

__itt_pt_mark_call_table:

        dd      256 dup(0000c2c3h OR (( $ - offset __itt_pt_mark_call_table) SHL 14))

___itt_pt_mark  endp


___itt_pt_byte   proc   near

        mov      ecx,[esp + 4]

___itt_pt_byte_::

        and     ecx,0ffh
        lea     ecx,__itt_pt_byte_call_table[ecx]
        jmp	ecx

        align   04h

        dd      0, 1, 2, 3      ;;; GUID

        dd      0fadedeafh      ;;; magic marker

__itt_pt_byte_call_table:

        db      256 dup(0c3h)

___itt_pt_byte   endp

        align   10h

___itt_pt_event  proc   near

        push    ecx
        mov     ecx,[esp + 8]
        rdpmc

        mov     cl,al
        call    ___itt_pt_byte_
        mov     cl,ah
        call    ___itt_pt_byte_
        shr     eax,16
        mov     cl,al
        call    ___itt_pt_byte_
        mov     cl,ah
        call    ___itt_pt_byte_

        mov     cl,dl
        call    ___itt_pt_byte_
        mov     cl,dh
        call    ___itt_pt_byte_
        shr     edx,16
        mov     cl,dl
        call    ___itt_pt_byte_
        mov     cl,dh
        call    ___itt_pt_byte_

        pop     ecx
        ret

___itt_pt_event  endp

        align   10h

___itt_pt_mark_event    proc    near

        test    byte ptr [esp + 4],1
        jnz     odd
        push    0
        call    ___itt_pt_event
        add     esp,4
        jmp     ___itt_pt_mark

odd:
        push    dword ptr [esp + 4]
        call    ___itt_pt_mark
        add     esp,4
        mov     dword ptr [esp + 4],0
        jmp     ___itt_pt_event

___itt_pt_mark_event    endp

        align   10h

___itt_pt_flush proc    near

        lea     eax,offset __itt_pt_mark_flush_1
        jmp     eax

        align   10h
        nop
__itt_pt_mark_flush_1:
        lea     eax,offset __itt_pt_mark_flush_2
        jmp     eax

        align   10h
        nop
        nop
__itt_pt_mark_flush_2:
        lea     eax,offset __itt_pt_mark_flush_3
        jmp     eax

        align   10h
        nop
        nop
        nop
__itt_pt_mark_flush_3:
        ret

___itt_pt_flush endp

        align   10h

;;; int __itt_pt_mark_threshold(unsigned char index, unsigned long long* tmp, int threshold);

___itt_pt_mark_threshold        proc    near
        test    byte ptr [esp + 4],1    ;;; index
        jnz     mark_end
mark_begin:
        mov     ecx,(1 SHL 30) + 1
        rdpmc
        mov     ecx,[esp + 8]   ;;; tmp
        mov     [ecx + 0],eax
        mov     [ecx + 4],edx
        jmp     ___itt_pt_mark
mark_end:
        mov     ecx,(1 SHL 30) + 1
        rdpmc
        mov     ecx,[esp + 8]   ;;; tmp
        sub     eax,[ecx + 0]
        sbb     edx,[ecx + 4]
        or      edx,edx
        jnz     found
        cmp     edx,[esp + 12]  ;;; threshold
        jnc     found
        jmp     ___itt_pt_mark
found:
        call    ___itt_pt_mark
        jmp     ___itt_pt_flush

___itt_pt_mark_threshold        endp

;;; PTWRITE

        align   10h

;;; void __itt_pt_write(unsigned long long value);

        dd      0, 1, 2, 3      ;;; GUID

___itt_pt_write proc

;;;        ptwrite dword ptr [esp + 4]
        db      0F3h, 0Fh, 0AEh, 64h, 24h, 04h
        ret

___itt_pt_write endp

;;;

_TEXT	ends
        end
