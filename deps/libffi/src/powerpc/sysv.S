/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 1998 Geoffrey Keating
   Copyright (C) 2007 Free Software Foundation, Inc

   PowerPC Assembly glue.

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
#include <powerpc/asm.h>

#ifndef __powerpc64__
	.globl ffi_prep_args_SYSV
ENTRY(ffi_call_SYSV)
.LFB1:
	/* Save the old stack pointer as AP.  */
	mr	%r8,%r1

.LCFI0:
	/* Allocate the stack space we need.  */
	stwux	%r1,%r1,%r4
	/* Save registers we use.  */
	mflr	%r9
	stw	%r28,-16(%r8)
.LCFI1:
	stw	%r29,-12(%r8)
.LCFI2:
	stw	%r30, -8(%r8)
.LCFI3:
	stw	%r31, -4(%r8)
.LCFI4:
	stw	%r9,   4(%r8)
.LCFI5:

	/* Save arguments over call...  */
	mr	%r31,%r5	/* flags, */
	mr	%r30,%r6	/* rvalue, */
	mr	%r29,%r7	/* function address, */
	mr	%r28,%r8	/* our AP. */
.LCFI6:

	/* Call ffi_prep_args_SYSV.  */
	mr	%r4,%r1
	bl	ffi_prep_args_SYSV@local

	/* Now do the call.  */
	/* Set up cr1 with bits 4-7 of the flags.  */
	mtcrf	0x40,%r31
	/* Get the address to call into CTR.  */
	mtctr	%r29
	/* Load all those argument registers.  */
	lwz	%r3,-16-(8*4)(%r28)
	lwz	%r4,-16-(7*4)(%r28)
	lwz	%r5,-16-(6*4)(%r28)
	lwz	%r6,-16-(5*4)(%r28)
	bf-	5,1f
	nop
	lwz	%r7,-16-(4*4)(%r28)
	lwz	%r8,-16-(3*4)(%r28)
	lwz	%r9,-16-(2*4)(%r28)
	lwz	%r10,-16-(1*4)(%r28)
	nop
1:

#ifndef __NO_FPRS__
	/* Load all the FP registers.  */
	bf-	6,2f
	lfd	%f1,-16-(8*4)-(8*8)(%r28)
	lfd	%f2,-16-(8*4)-(7*8)(%r28)
	lfd	%f3,-16-(8*4)-(6*8)(%r28)
	lfd	%f4,-16-(8*4)-(5*8)(%r28)
	nop
	lfd	%f5,-16-(8*4)-(4*8)(%r28)
	lfd	%f6,-16-(8*4)-(3*8)(%r28)
	lfd	%f7,-16-(8*4)-(2*8)(%r28)
	lfd	%f8,-16-(8*4)-(1*8)(%r28)
#endif
2:

	/* Make the call.  */
	bctrl

	/* Now, deal with the return value.  */
	mtcrf	0x01,%r31 /* cr7  */
	bt-	31,L(small_struct_return_value)
	bt-	30,L(done_return_value)
#ifndef __NO_FPRS__
	bt-	29,L(fp_return_value)
#endif
	stw	%r3,0(%r30)
	bf+	28,L(done_return_value)
	stw	%r4,4(%r30)
	mtcrf	0x02,%r31 /* cr6  */
	bf	27,L(done_return_value)
	stw     %r5,8(%r30)
	stw	%r6,12(%r30)
	/* Fall through...  */

L(done_return_value):
	/* Restore the registers we used and return.  */
	lwz	%r9,   4(%r28)
	lwz	%r31, -4(%r28)
	mtlr	%r9
	lwz	%r30, -8(%r28)
	lwz	%r29,-12(%r28)
	lwz	%r28,-16(%r28)
	lwz	%r1,0(%r1)
	blr

#ifndef __NO_FPRS__
L(fp_return_value):
	bf	28,L(float_return_value)
	stfd	%f1,0(%r30)
	mtcrf   0x02,%r31 /* cr6  */
	bf	27,L(done_return_value)
	stfd	%f2,8(%r30)
	b	L(done_return_value)
L(float_return_value):
	stfs	%f1,0(%r30)
	b	L(done_return_value)
#endif

L(small_struct_return_value):
	extrwi	%r6,%r31,2,19         /* number of bytes padding = shift/8 */
	mtcrf	0x02,%r31	      /* copy flags to cr[24:27] (cr6) */
	extrwi	%r5,%r31,5,19         /* r5 <- number of bits of padding */
	subfic  %r6,%r6,4             /* r6 <- number of useful bytes in r3 */
	bf-	25,L(done_return_value) /* struct in r3 ? if not, done. */
/* smst_one_register: */
	slw	%r3,%r3,%r5           /* Left-justify value in r3 */
	mtxer	%r6                   /* move byte count to XER ... */
	stswx	%r3,0,%r30            /* ... and store that many bytes */
	bf+	26,L(done_return_value)  /* struct in r3:r4 ? */
	add	%r6,%r6,%r30          /* adjust pointer */
	stswi	%r4,%r6,4             /* store last four bytes */
	b	L(done_return_value)

.LFE1:
END(ffi_call_SYSV)

      .section	".eh_frame",EH_FRAME_FLAGS,@progbits
.Lframe1:
      .4byte    .LECIE1-.LSCIE1  /*  Length of Common Information Entry */
.LSCIE1:
      .4byte    0x0      /*  CIE Identifier Tag */
      .byte     0x1      /*  CIE Version */
#if defined _RELOCATABLE || defined __PIC__
      .ascii	"zR\0"   /*  CIE Augmentation */
#else
      .ascii	"\0"	 /*  CIE Augmentation */
#endif
      .uleb128  0x1      /*  CIE Code Alignment Factor */
      .sleb128  -4	 /*  CIE Data Alignment Factor */
      .byte     0x41     /*  CIE RA Column */
#if defined _RELOCATABLE || defined __PIC__
      .uleb128  0x1      /*  Augmentation size */
      .byte	0x1b	 /*  FDE Encoding (pcrel sdata4) */
#endif
      .byte     0xc      /*  DW_CFA_def_cfa */
      .uleb128  0x1
      .uleb128  0x0
      .align 2
.LECIE1:
.LSFDE1:
      .4byte    .LEFDE1-.LASFDE1         /*  FDE Length */
.LASFDE1:
      .4byte    .LASFDE1-.Lframe1         /*  FDE CIE offset */
#if defined _RELOCATABLE || defined __PIC__
      .4byte    .LFB1-.  /*  FDE initial location */
#else
      .4byte    .LFB1    /*  FDE initial location */
#endif
      .4byte    .LFE1-.LFB1      /*  FDE address range */
#if defined _RELOCATABLE || defined __PIC__
      .uleb128  0x0	 /*  Augmentation size */
#endif
      .byte     0x4      /*  DW_CFA_advance_loc4 */
      .4byte    .LCFI0-.LFB1
      .byte     0xd      /*  DW_CFA_def_cfa_register */
      .uleb128  0x08
      .byte     0x4      /*  DW_CFA_advance_loc4 */
      .4byte    .LCFI5-.LCFI0
      .byte     0x11     /*  DW_CFA_offset_extended_sf */
      .uleb128  0x41
      .sleb128  -1
      .byte     0x9f     /*  DW_CFA_offset, column 0x1f */
      .uleb128  0x1
      .byte     0x9e     /*  DW_CFA_offset, column 0x1e */
      .uleb128  0x2
      .byte     0x9d     /*  DW_CFA_offset, column 0x1d */
      .uleb128  0x3
      .byte     0x9c     /*  DW_CFA_offset, column 0x1c */
      .uleb128  0x4
      .byte     0x4      /*  DW_CFA_advance_loc4 */
      .4byte    .LCFI6-.LCFI5
      .byte     0xd      /*  DW_CFA_def_cfa_register */
      .uleb128  0x1c
      .align 2
.LEFDE1:
#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
