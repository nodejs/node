/* -----------------------------------------------------------------------
   hpux32.S - Copyright (c) 2006 Free Software Foundation, Inc.
	                (c) 2008 Red Hat, Inc.
   based on src/pa/linux.S

   HP-UX PA Foreign Function Interface

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>

	.LEVEL 1.1
	.SPACE	$PRIVATE$
	.IMPORT	$global$,DATA
	.IMPORT	$$dyncall,MILLICODE
	.SUBSPA	$DATA$
	.align	4

	/* void ffi_call_pa32(void (*)(char *, extended_cif *),
			       extended_cif *ecif,
			       unsigned bytes,
			       unsigned flags,
			       unsigned *rvalue,
			       void (*fn)(void));
	 */

	.export	ffi_call_pa32,ENTRY,PRIV_LEV=3
	.import	ffi_prep_args_pa32,CODE

	.SPACE	$TEXT$
	.SUBSPA $CODE$
	.align	4

L$FB1
ffi_call_pa32
	.proc
	.callinfo	FRAME=64,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=4
	.entry
	stw	%rp, -20(%sp)
	copy	%r3, %r1
L$CFI11
	copy	%sp, %r3
L$CFI12

	/* Setup the stack for calling prep_args...
	   We want the stack to look like this:

	   [ Previous stack                            ] <- %r3

	   [ 64-bytes register save area               ] <- %r4

	   [ Stack space for actual call, passed as    ] <- %arg0
	   [     arg0 to ffi_prep_args_pa32           ]

	   [ Stack for calling prep_args               ] <- %sp
	 */

	stwm	%r1, 64(%sp)
	stw	%r4, 12(%r3)
L$CFI13
	copy	%sp, %r4

	addl	%arg2, %r4, %arg0	; arg stack
	stw	%arg3, -48(%r3)		; save flags we need it later

	/* Call prep_args:
	   %arg0(stack) -- set up above
	   %arg1(ecif)  -- same as incoming param
	   %arg2(bytes) -- same as incoming param */
	bl	ffi_prep_args_pa32,%r2
	ldo	64(%arg0), %sp
	ldo	-64(%sp), %sp

	/* now %sp should point where %arg0 was pointing.  */

	/* Load the arguments that should be passed in registers
	   The fp args are loaded by the prep_args function.  */
	ldw	-36(%sp), %arg0
	ldw	-40(%sp), %arg1
	ldw	-44(%sp), %arg2
	ldw	-48(%sp), %arg3

	/* in case the function is going to return a structure
	   we need to give it a place to put the result.  */
	ldw	-52(%r3), %ret0		; %ret0 <- rvalue
	ldw	-56(%r3), %r22		; %r22 <- function to call
	bl	$$dyncall, %r31		; Call the user function
	copy	%r31, %rp

	/* Prepare to store the result; we need to recover flags and rvalue.  */
	ldw	-48(%r3), %r21		; r21 <- flags
	ldw	-52(%r3), %r20		; r20 <- rvalue

	/* Store the result according to the return type.  The most
	   likely types should come first.  */

L$checkint
	comib,<>,n FFI_TYPE_INT, %r21, L$checkint8
	b	L$done
	stw	%ret0, 0(%r20)

L$checkint8
	comib,<>,n FFI_TYPE_UINT8, %r21, L$checkint16
	b	L$done
	stb	%ret0, 0(%r20)

L$checkint16
	comib,<>,n FFI_TYPE_UINT16, %r21, L$checkdbl
	b	L$done
	sth	%ret0, 0(%r20)

L$checkdbl
	comib,<>,n FFI_TYPE_DOUBLE, %r21, L$checkfloat
	b	L$done
	fstd	%fr4,0(%r20)

L$checkfloat
	comib,<>,n FFI_TYPE_FLOAT, %r21, L$checkll
	b	L$done
	fstw	%fr4L,0(%r20)

L$checkll
	comib,<>,n FFI_TYPE_UINT64, %r21, L$checksmst2
	stw	%ret0, 0(%r20)
	b	L$done
	stw	%ret1, 4(%r20)

L$checksmst2
	comib,<>,n FFI_TYPE_SMALL_STRUCT2, %r21, L$checksmst3
	/* 2-byte structs are returned in ret0 as ????xxyy.  */
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret0, 0(%r20)

L$checksmst3
	comib,<>,n FFI_TYPE_SMALL_STRUCT3, %r21, L$checksmst4
	/* 3-byte structs are returned in ret0 as ??xxyyzz.  */
	extru	%ret0, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret0, 0(%r20)

L$checksmst4
	comib,<>,n FFI_TYPE_SMALL_STRUCT4, %r21, L$checksmst5
	/* 4-byte structs are returned in ret0 as wwxxyyzz.  */
	extru	%ret0, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret0, 0(%r20)

L$checksmst5
	comib,<>,n FFI_TYPE_SMALL_STRUCT5, %r21, L$checksmst6
	/* 5 byte values are returned right justified:
	      ret0     ret1
	   5: ??????aa bbccddee */
	stbs,ma	%ret0, 1(%r20)
	extru	%ret1, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret1, 0(%r20)

L$checksmst6
	comib,<>,n FFI_TYPE_SMALL_STRUCT6, %r21, L$checksmst7
	/* 6 byte values are returned right justified:
	      ret0     ret1
	   6: ????aabb ccddeeff */
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	stbs,ma	%ret0, 1(%r20)
	extru	%ret1, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret1, 0(%r20)

L$checksmst7
	comib,<>,n FFI_TYPE_SMALL_STRUCT7, %r21, L$checksmst8
	/* 7 byte values are returned right justified:
	      ret0     ret1
	   7: ??aabbcc ddeeffgg */
	extru	%ret0, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	stbs,ma	%ret0, 1(%r20)
	extru	%ret1, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	b	L$done
	stb	%ret1, 0(%r20)

L$checksmst8
	comib,<>,n FFI_TYPE_SMALL_STRUCT8, %r21, L$done
	/* 8 byte values are returned right justified:
	      ret0     ret1
	   8: aabbccdd eeffgghh */
	extru	%ret0, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret0, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	stbs,ma	%ret0, 1(%r20)
	extru	%ret1, 7, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 15, 8, %r22
	stbs,ma	%r22, 1(%r20)
	extru	%ret1, 23, 8, %r22
	stbs,ma	%r22, 1(%r20)
	stb	%ret1, 0(%r20)

L$done
	/* all done, return */
	copy	%r4, %sp	; pop arg stack
	ldw	12(%r3), %r4
	ldwm	-64(%sp), %r3	; .. and pop stack
	ldw	-20(%sp), %rp
	bv	%r0(%rp)
	nop
	.exit
	.procend
L$FE1

	/* void ffi_closure_pa32(void);
	   Called with closure argument in %r21 */

	.SPACE $TEXT$
	.SUBSPA $CODE$
	.export ffi_closure_pa32,ENTRY,PRIV_LEV=3,RTNVAL=GR
	.import ffi_closure_inner_pa32,CODE
	.align 4
L$FB2
ffi_closure_pa32
	.proc
	.callinfo FRAME=64,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.entry

	stw	%rp, -20(%sp)
	copy	%r3, %r1
L$CFI21
	copy	%sp, %r3
L$CFI22
	stwm	%r1, 64(%sp)

	/* Put arguments onto the stack and call ffi_closure_inner.  */
	stw	%arg0, -36(%r3)
	stw	%arg1, -40(%r3)
	stw	%arg2, -44(%r3)
	stw	%arg3, -48(%r3)

	copy	%r21, %arg0
	bl	ffi_closure_inner_pa32, %r2
	copy    %r3, %arg1
	ldwm	-64(%sp), %r3
	ldw	-20(%sp), %rp
	ldw	-36(%sp), %ret0
	bv	%r0(%rp)
	ldw	-40(%sp), %ret1
	.exit
	.procend
L$FE2:

	.SPACE $PRIVATE$
	.SUBSPA $DATA$

	.align 4
	.EXPORT _GLOBAL__F_ffi_call_pa32,DATA
_GLOBAL__F_ffi_call_pa32
L$frame1:
	.word   L$ECIE1-L$SCIE1 ;# Length of Common Information Entry
L$SCIE1:
	.word   0x0     ;# CIE Identifier Tag
	.byte   0x1     ;# CIE Version
	.ascii "\0"     ;# CIE Augmentation
	.uleb128 0x1    ;# CIE Code Alignment Factor
	.sleb128 4      ;# CIE Data Alignment Factor
	.byte   0x2     ;# CIE RA Column
	.byte   0xc     ;# DW_CFA_def_cfa
	.uleb128 0x1e
	.uleb128 0x0
	.align 4
L$ECIE1:
L$SFDE1:
	.word   L$EFDE1-L$ASFDE1        ;# FDE Length
L$ASFDE1:
	.word   L$ASFDE1-L$frame1       ;# FDE CIE offset
	.word   L$FB1   ;# FDE initial location
	.word   L$FE1-L$FB1     ;# FDE address range

	.byte   0x4     ;# DW_CFA_advance_loc4
	.word   L$CFI11-L$FB1
	.byte	0x83	;# DW_CFA_offset, column 0x3
	.uleb128 0x0
	.byte   0x11    ;# DW_CFA_offset_extended_sf; save r2 at [r30-20]
	.uleb128 0x2
	.sleb128 -5

	.byte   0x4     ;# DW_CFA_advance_loc4
	.word   L$CFI12-L$CFI11
	.byte   0xd     ;# DW_CFA_def_cfa_register = r3
	.uleb128 0x3

	.byte   0x4     ;# DW_CFA_advance_loc4
	.word   L$CFI13-L$CFI12
	.byte	0x84	;# DW_CFA_offset, column 0x4
	.uleb128 0x3

	.align 4
L$EFDE1:

L$SFDE2:
	.word   L$EFDE2-L$ASFDE2        ;# FDE Length
L$ASFDE2:
	.word   L$ASFDE2-L$frame1       ;# FDE CIE offset
	.word   L$FB2   ;# FDE initial location
	.word   L$FE2-L$FB2     ;# FDE address range
	.byte   0x4     ;# DW_CFA_advance_loc4
	.word   L$CFI21-L$FB2
	.byte   0x83    ;# DW_CFA_offset, column 0x3
	.uleb128 0x0
	.byte   0x11    ;# DW_CFA_offset_extended_sf
	.uleb128 0x2
	.sleb128 -5

	.byte   0x4     ;# DW_CFA_advance_loc4
	.word   L$CFI22-L$CFI21
	.byte   0xd     ;# DW_CFA_def_cfa_register = r3
	.uleb128 0x3

	.align 4
L$EFDE2:
