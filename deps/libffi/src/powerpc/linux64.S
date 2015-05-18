/* -----------------------------------------------------------------------
   sysv.h - Copyright (c) 2003 Jakub Jelinek <jakub@redhat.com>
	    Copyright (c) 2008 Red Hat, Inc.

   PowerPC64 Assembly glue.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>

#ifdef __powerpc64__
	.hidden	ffi_call_LINUX64
	.globl	ffi_call_LINUX64
	.section	".opd","aw"
	.align	3
ffi_call_LINUX64:
#ifdef _CALL_LINUX
	.quad	.L.ffi_call_LINUX64,.TOC.@tocbase,0
	.type	ffi_call_LINUX64,@function
	.text
.L.ffi_call_LINUX64:
#else
	.hidden	.ffi_call_LINUX64
	.globl	.ffi_call_LINUX64
	.quad	.ffi_call_LINUX64,.TOC.@tocbase,0
	.size	ffi_call_LINUX64,24
	.type	.ffi_call_LINUX64,@function
	.text
.ffi_call_LINUX64:
#endif
.LFB1:
	mflr	%r0
	std	%r28, -32(%r1)
	std	%r29, -24(%r1)
	std	%r30, -16(%r1)
	std	%r31, -8(%r1)
	std	%r0, 16(%r1)

	mr	%r28, %r1	/* our AP.  */
.LCFI0:
	stdux	%r1, %r1, %r4
	mr	%r31, %r5	/* flags, */
	mr	%r30, %r6	/* rvalue, */
	mr	%r29, %r7	/* function address.  */
	std	%r2, 40(%r1)

	/* Call ffi_prep_args64.  */
	mr	%r4, %r1
#ifdef _CALL_LINUX
	bl	ffi_prep_args64
#else
	bl	.ffi_prep_args64
#endif

	ld	%r0, 0(%r29)
	ld	%r2, 8(%r29)
	ld	%r11, 16(%r29)

	/* Now do the call.  */
	/* Set up cr1 with bits 4-7 of the flags.  */
	mtcrf	0x40, %r31

	/* Get the address to call into CTR.  */
	mtctr	%r0
	/* Load all those argument registers.  */
	ld	%r3, -32-(8*8)(%r28)
	ld	%r4, -32-(7*8)(%r28)
	ld	%r5, -32-(6*8)(%r28)
	ld	%r6, -32-(5*8)(%r28)
	bf-	5, 1f
	ld	%r7, -32-(4*8)(%r28)
	ld	%r8, -32-(3*8)(%r28)
	ld	%r9, -32-(2*8)(%r28)
	ld	%r10, -32-(1*8)(%r28)
1:

	/* Load all the FP registers.  */
	bf-	6, 2f
	lfd	%f1, -32-(21*8)(%r28)
	lfd	%f2, -32-(20*8)(%r28)
	lfd	%f3, -32-(19*8)(%r28)
	lfd	%f4, -32-(18*8)(%r28)
	lfd	%f5, -32-(17*8)(%r28)
	lfd	%f6, -32-(16*8)(%r28)
	lfd	%f7, -32-(15*8)(%r28)
	lfd	%f8, -32-(14*8)(%r28)
	lfd	%f9, -32-(13*8)(%r28)
	lfd	%f10, -32-(12*8)(%r28)
	lfd	%f11, -32-(11*8)(%r28)
	lfd	%f12, -32-(10*8)(%r28)
	lfd	%f13, -32-(9*8)(%r28)
2:

	/* Make the call.  */
	bctrl

	/* This must follow the call immediately, the unwinder
	   uses this to find out if r2 has been saved or not.  */
	ld	%r2, 40(%r1)

	/* Now, deal with the return value.  */
	mtcrf	0x01, %r31
	bt-	30, .Ldone_return_value
	bt-	29, .Lfp_return_value
	std	%r3, 0(%r30)
	/* Fall through...  */

.Ldone_return_value:
	/* Restore the registers we used and return.  */
	mr	%r1, %r28
	ld	%r0, 16(%r28)
	ld	%r28, -32(%r1)
	mtlr	%r0
	ld	%r29, -24(%r1)
	ld	%r30, -16(%r1)
	ld	%r31, -8(%r1)
	blr

.Lfp_return_value:
	bf	28, .Lfloat_return_value
	stfd	%f1, 0(%r30)
	mtcrf	0x02, %r31 /* cr6  */
	bf	27, .Ldone_return_value
	stfd	%f2, 8(%r30)
	b	.Ldone_return_value
.Lfloat_return_value:
	stfs	%f1, 0(%r30)
	b	.Ldone_return_value
.LFE1:
	.long	0
	.byte	0,12,0,1,128,4,0,0
#ifdef _CALL_LINUX
	.size	ffi_call_LINUX64,.-.L.ffi_call_LINUX64
#else
	.size	.ffi_call_LINUX64,.-.ffi_call_LINUX64
#endif

	.section	.eh_frame,EH_FRAME_FLAGS,@progbits
.Lframe1:
	.4byte	.LECIE1-.LSCIE1	 # Length of Common Information Entry
.LSCIE1:
	.4byte	0x0	 # CIE Identifier Tag
	.byte	0x1	 # CIE Version
	.ascii "zR\0"	 # CIE Augmentation
	.uleb128 0x1	 # CIE Code Alignment Factor
	.sleb128 -8	 # CIE Data Alignment Factor
	.byte	0x41	 # CIE RA Column
	.uleb128 0x1	 # Augmentation size
	.byte	0x14	 # FDE Encoding (pcrel udata8)
	.byte	0xc	 # DW_CFA_def_cfa
	.uleb128 0x1
	.uleb128 0x0
	.align 3
.LECIE1:
.LSFDE1:
	.4byte	.LEFDE1-.LASFDE1	 # FDE Length
.LASFDE1:
	.4byte	.LASFDE1-.Lframe1	 # FDE CIE offset
	.8byte	.LFB1-.	 # FDE initial location
	.8byte	.LFE1-.LFB1	 # FDE address range
	.uleb128 0x0	 # Augmentation size
	.byte	0x2	 # DW_CFA_advance_loc1
	.byte	.LCFI0-.LFB1
	.byte	0xd	 # DW_CFA_def_cfa_register
	.uleb128 0x1c
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x41
	.sleb128 -2
	.byte	0x9f	 # DW_CFA_offset, column 0x1f
	.uleb128 0x1
	.byte	0x9e	 # DW_CFA_offset, column 0x1e
	.uleb128 0x2
	.byte	0x9d	 # DW_CFA_offset, column 0x1d
	.uleb128 0x3
	.byte	0x9c	 # DW_CFA_offset, column 0x1c
	.uleb128 0x4
	.align 3
.LEFDE1:
#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
