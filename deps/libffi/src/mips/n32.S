/* -----------------------------------------------------------------------
   n32.S - Copyright (c) 1996, 1998, 2005, 2007, 2009, 2010  Red Hat, Inc.
   
   MIPS Foreign Function Interface 

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

/* Only build this code if we are compiling for n32 */	

#if defined(FFI_MIPS_N32)

#define callback a0
#define bytes	 a2
#define flags	 a3
#define raddr    a4
#define fn       a5

#define SIZEOF_FRAME	( 8 * FFI_SIZEOF_ARG )

#ifdef __GNUC__
	.abicalls
#endif
	.set mips4
	.text
	.align	2
	.globl	ffi_call_N32
	.ent	ffi_call_N32
ffi_call_N32:	
.LFB3:
	.frame	$fp, SIZEOF_FRAME, ra
	.mask	0xc0000000,-FFI_SIZEOF_ARG
	.fmask	0x00000000,0

	# Prologue
	SUBU	$sp, SIZEOF_FRAME			# Frame size
.LCFI0:
	REG_S	$fp, SIZEOF_FRAME - 2*FFI_SIZEOF_ARG($sp)	# Save frame pointer
	REG_S	ra, SIZEOF_FRAME - 1*FFI_SIZEOF_ARG($sp)	# Save return address
.LCFI1:
	move	$fp, $sp
.LCFI3:
	move	t9, callback	# callback function pointer
	REG_S	bytes, 2*FFI_SIZEOF_ARG($fp) # bytes
	REG_S	flags, 3*FFI_SIZEOF_ARG($fp) # flags
	REG_S	raddr, 4*FFI_SIZEOF_ARG($fp) # raddr
	REG_S	fn,    5*FFI_SIZEOF_ARG($fp) # fn

	# Allocate at least 4 words in the argstack
	move	v0, bytes
	bge	bytes, 4 * FFI_SIZEOF_ARG, bigger	
	LI	v0, 4 * FFI_SIZEOF_ARG
	b	sixteen

	bigger:	
	ADDU	t4, v0, 2 * FFI_SIZEOF_ARG -1	# make sure it is aligned 
	and	v0, t4, -2 * FFI_SIZEOF_ARG		# to a proper boundry.

sixteen:
	SUBU	$sp, $sp, v0	# move the stack pointer to reflect the
				# arg space

	move	a0, $sp         # 4 * FFI_SIZEOF_ARG
	ADDU	a3, $fp, 3 * FFI_SIZEOF_ARG

	# Call ffi_prep_args
	jal	t9
	
	# Copy the stack pointer to t9
	move	t9, $sp
	
	# Fix the stack if there are more than 8 64bit slots worth
	# of arguments.

	# Load the number of bytes
	REG_L	t6, 2*FFI_SIZEOF_ARG($fp)

	# Is it bigger than 8 * FFI_SIZEOF_ARG?
	daddiu	t8, t6, -(8 * FFI_SIZEOF_ARG)
	bltz	t8, loadregs

	ADDU	t9, t9, t8
	
loadregs:	

	REG_L	t6, 3*FFI_SIZEOF_ARG($fp)  # load the flags word into t6.

	and	t4, t6, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg1_floatp
	REG_L	a0, 0*FFI_SIZEOF_ARG(t9)
	b	arg1_next
arg1_floatp:	
	bne	t4, FFI_TYPE_FLOAT, arg1_doublep
	l.s	$f12, 0*FFI_SIZEOF_ARG(t9)
	b	arg1_next
arg1_doublep:	
	l.d	$f12, 0*FFI_SIZEOF_ARG(t9)
arg1_next:	
	
	SRL	t4, t6, 1*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg2_floatp
	REG_L	a1, 1*FFI_SIZEOF_ARG(t9)
	b	arg2_next
arg2_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg2_doublep
	l.s	$f13, 1*FFI_SIZEOF_ARG(t9)	
	b	arg2_next
arg2_doublep:	
	l.d	$f13, 1*FFI_SIZEOF_ARG(t9)	
arg2_next:	
	
	SRL	t4, t6, 2*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg3_floatp
	REG_L	a2, 2*FFI_SIZEOF_ARG(t9)
	b	arg3_next
arg3_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg3_doublep
	l.s	$f14, 2*FFI_SIZEOF_ARG(t9)	
	b	arg3_next
arg3_doublep:	
	l.d	$f14, 2*FFI_SIZEOF_ARG(t9)	
arg3_next:	
	
	SRL	t4, t6, 3*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg4_floatp
	REG_L	a3, 3*FFI_SIZEOF_ARG(t9)
	b	arg4_next
arg4_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg4_doublep
	l.s	$f15, 3*FFI_SIZEOF_ARG(t9)	
	b	arg4_next
arg4_doublep:	
	l.d	$f15, 3*FFI_SIZEOF_ARG(t9)	
arg4_next:	
	
	SRL	t4, t6, 4*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg5_floatp
	REG_L	a4, 4*FFI_SIZEOF_ARG(t9)
	b	arg5_next
arg5_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg5_doublep
	l.s	$f16, 4*FFI_SIZEOF_ARG(t9)	
	b	arg5_next
arg5_doublep:	
	l.d	$f16, 4*FFI_SIZEOF_ARG(t9)	
arg5_next:	
	
	SRL	t4, t6, 5*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg6_floatp
	REG_L	a5, 5*FFI_SIZEOF_ARG(t9)
	b	arg6_next
arg6_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg6_doublep
	l.s	$f17, 5*FFI_SIZEOF_ARG(t9)	
	b	arg6_next
arg6_doublep:	
	l.d	$f17, 5*FFI_SIZEOF_ARG(t9)	
arg6_next:	
	
	SRL	t4, t6, 6*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg7_floatp
	REG_L	a6, 6*FFI_SIZEOF_ARG(t9)
	b	arg7_next
arg7_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg7_doublep
	l.s	$f18, 6*FFI_SIZEOF_ARG(t9)	
	b	arg7_next
arg7_doublep:	
	l.d	$f18, 6*FFI_SIZEOF_ARG(t9)	
arg7_next:	
	
	SRL	t4, t6, 7*FFI_FLAG_BITS
	and	t4, ((1<<FFI_FLAG_BITS)-1)
	bnez	t4, arg8_floatp
	REG_L	a7, 7*FFI_SIZEOF_ARG(t9)
	b	arg8_next
arg8_floatp:
	bne	t4, FFI_TYPE_FLOAT, arg8_doublep
 	l.s	$f19, 7*FFI_SIZEOF_ARG(t9)	
	b	arg8_next
arg8_doublep:	
 	l.d	$f19, 7*FFI_SIZEOF_ARG(t9)	
arg8_next:	

callit:		
	# Load the function pointer
	REG_L	t9, 5*FFI_SIZEOF_ARG($fp)

	# If the return value pointer is NULL, assume no return value.
	REG_L	t5, 4*FFI_SIZEOF_ARG($fp)
	beqz	t5, noretval

	# Shift the return type flag over
	SRL	t6, 8*FFI_FLAG_BITS

	beq	t6, FFI_TYPE_SINT32, retint	
	bne     t6, FFI_TYPE_INT, retfloat
retint:
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	REG_S	v0, 0(t4)
	b	epilogue

retfloat:
	bne     t6, FFI_TYPE_FLOAT, retdouble
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.s	$f0, 0(t4)
	b	epilogue

retdouble:	
	bne	t6, FFI_TYPE_DOUBLE, retstruct_d
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.d	$f0, 0(t4)
	b	epilogue

retstruct_d:	
	bne	t6, FFI_TYPE_STRUCT_D, retstruct_f
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.d	$f0, 0(t4)
	b	epilogue
	
retstruct_f:	
	bne	t6, FFI_TYPE_STRUCT_F, retstruct_d_d
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.s	$f0, 0(t4)
	b	epilogue
	
retstruct_d_d:	
	bne	t6, FFI_TYPE_STRUCT_DD, retstruct_f_f
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.d	$f0, 0(t4)
	s.d	$f2, 8(t4)
	b	epilogue
	
retstruct_f_f:	
	bne	t6, FFI_TYPE_STRUCT_FF, retstruct_d_f
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.s	$f0, 0(t4)
	s.s	$f2, 4(t4)
	b	epilogue
	
retstruct_d_f:	
	bne	t6, FFI_TYPE_STRUCT_DF, retstruct_f_d
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.d	$f0, 0(t4)
	s.s	$f2, 8(t4)
	b	epilogue
	
retstruct_f_d:	
	bne	t6, FFI_TYPE_STRUCT_FD, retstruct_d_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	s.s	$f0, 0(t4)
	s.d	$f2, 8(t4)
	b	epilogue

retstruct_d_soft:
	bne	t6, FFI_TYPE_STRUCT_D_SOFT, retstruct_f_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sd	v0, 0(t4)
	b	epilogue
	
retstruct_f_soft:	
	bne	t6, FFI_TYPE_STRUCT_F_SOFT, retstruct_d_d_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sw	v0, 0(t4)
	b	epilogue
	
retstruct_d_d_soft:	
	bne	t6, FFI_TYPE_STRUCT_DD_SOFT, retstruct_f_f_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sd	v0, 0(t4)
	sd	v1, 8(t4)
	b	epilogue
	
retstruct_f_f_soft:	
	bne	t6, FFI_TYPE_STRUCT_FF_SOFT, retstruct_d_f_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sw	v0, 0(t4)
	sw	v1, 4(t4)
	b	epilogue
	
retstruct_d_f_soft:	
	bne	t6, FFI_TYPE_STRUCT_DF_SOFT, retstruct_f_d_soft
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sd	v0, 0(t4)
	sw	v1, 8(t4)
	b	epilogue
	
retstruct_f_d_soft:	
	bne	t6, FFI_TYPE_STRUCT_FD_SOFT, retstruct_small
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	sw	v0, 0(t4)
	sd	v1, 8(t4)
	b	epilogue
	
retstruct_small:	
	bne	t6, FFI_TYPE_STRUCT_SMALL, retstruct_small2
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	REG_S	v0, 0(t4)
	b	epilogue
	
retstruct_small2:	
	bne	t6, FFI_TYPE_STRUCT_SMALL2, retstruct
	jal	t9
	REG_L	t4, 4*FFI_SIZEOF_ARG($fp)
	REG_S	v0, 0(t4)
	REG_S	v1, 8(t4)
	b	epilogue
	
retstruct:	
noretval:	
	jal	t9
	
	# Epilogue
epilogue:	
	move	$sp, $fp	
	REG_L	$fp, SIZEOF_FRAME - 2*FFI_SIZEOF_ARG($sp) # Restore frame pointer
	REG_L	ra, SIZEOF_FRAME - 1*FFI_SIZEOF_ARG($sp)  # Restore return address
	ADDU	$sp, SIZEOF_FRAME		      # Fix stack pointer
	j	ra

.LFE3:
	.end	ffi_call_N32

/* ffi_closure_N32. Expects address of the passed-in ffi_closure in t0
   ($12). Stores any arguments passed in registers onto the stack,
   then calls ffi_closure_mips_inner_N32, which then decodes
   them.
	
	Stack layout:

	20 - Start of parameters, original sp
	19 - Called function a7 save
	18 - Called function a6 save
	17 - Called function a5 save
	16 - Called function a4 save
	15 - Called function a3 save
	14 - Called function a2 save
	13 - Called function a1 save
	12 - Called function a0 save
	11 - Called function f19
	10 - Called function f18
	 9 - Called function f17
	 8 - Called function f16
	 7 - Called function f15
         6 - Called function f14
         5 - Called function f13
         4 - Called function f12
	 3 - return value high (v1 or $f2)
	 2 - return value low (v0 or $f0)
	 1 - ra save
	 0 - gp save our sp  points here
	 */

#define SIZEOF_FRAME2	(20 * FFI_SIZEOF_ARG)
	
#define A7_OFF2		(19 * FFI_SIZEOF_ARG)
#define A6_OFF2		(18 * FFI_SIZEOF_ARG)
#define A5_OFF2		(17 * FFI_SIZEOF_ARG)
#define A4_OFF2		(16 * FFI_SIZEOF_ARG)
#define A3_OFF2		(15 * FFI_SIZEOF_ARG)
#define A2_OFF2		(14 * FFI_SIZEOF_ARG)
#define A1_OFF2		(13 * FFI_SIZEOF_ARG)
#define A0_OFF2		(12 * FFI_SIZEOF_ARG)	

#define F19_OFF2	(11 * FFI_SIZEOF_ARG)
#define F18_OFF2	(10 * FFI_SIZEOF_ARG)
#define F17_OFF2	(9  * FFI_SIZEOF_ARG)
#define F16_OFF2	(8  * FFI_SIZEOF_ARG)
#define F15_OFF2	(7  * FFI_SIZEOF_ARG)
#define F14_OFF2	(6  * FFI_SIZEOF_ARG)
#define F13_OFF2	(5  * FFI_SIZEOF_ARG)
#define F12_OFF2	(4  * FFI_SIZEOF_ARG)

#define V1_OFF2		(3  * FFI_SIZEOF_ARG)
#define V0_OFF2		(2  * FFI_SIZEOF_ARG)

#define RA_OFF2		(1  * FFI_SIZEOF_ARG)
#define GP_OFF2		(0  * FFI_SIZEOF_ARG)

	.align	2
	.globl	ffi_closure_N32
	.ent	ffi_closure_N32
ffi_closure_N32:
.LFB2:
	.frame	$sp, SIZEOF_FRAME2, ra
	.mask	0x90000000,-(SIZEOF_FRAME2 - RA_OFF2)
	.fmask	0x00000000,0
	SUBU	$sp, SIZEOF_FRAME2
.LCFI5:
	.cpsetup t9, GP_OFF2, ffi_closure_N32
	REG_S	ra, RA_OFF2($sp)	# Save return address
.LCFI6:
	# Store all possible argument registers. If there are more than
	# fit in registers, then they were stored on the stack.
	REG_S	a0, A0_OFF2($sp)
	REG_S	a1, A1_OFF2($sp)
	REG_S	a2, A2_OFF2($sp)
	REG_S	a3, A3_OFF2($sp)
	REG_S	a4, A4_OFF2($sp)
	REG_S	a5, A5_OFF2($sp)
	REG_S	a6, A6_OFF2($sp)
	REG_S	a7, A7_OFF2($sp)

	# Store all possible float/double registers.
	s.d	$f12, F12_OFF2($sp)
	s.d	$f13, F13_OFF2($sp)
	s.d	$f14, F14_OFF2($sp)
	s.d	$f15, F15_OFF2($sp)
	s.d	$f16, F16_OFF2($sp)
	s.d	$f17, F17_OFF2($sp)
	s.d	$f18, F18_OFF2($sp)
	s.d	$f19, F19_OFF2($sp)

	# Call ffi_closure_mips_inner_N32 to do the real work.
	LA	t9, ffi_closure_mips_inner_N32
	move	a0, $12	 # Pointer to the ffi_closure
	ADDU	a1, $sp, V0_OFF2
	ADDU	a2, $sp, A0_OFF2
	ADDU	a3, $sp, F12_OFF2
	jalr	t9

	# Return flags are in v0
	bne     v0, FFI_TYPE_SINT32, cls_retint
	lw	v0, V0_OFF2($sp)
	b	cls_epilogue

cls_retint:
	bne     v0, FFI_TYPE_INT, cls_retfloat
	REG_L	v0, V0_OFF2($sp)
	b	cls_epilogue

cls_retfloat:
	bne     v0, FFI_TYPE_FLOAT, cls_retdouble
	l.s	$f0, V0_OFF2($sp)
	b	cls_epilogue

cls_retdouble:	
	bne	v0, FFI_TYPE_DOUBLE, cls_retstruct_d
	l.d	$f0, V0_OFF2($sp)
	b	cls_epilogue

cls_retstruct_d:	
	bne	v0, FFI_TYPE_STRUCT_D, cls_retstruct_f
	l.d	$f0, V0_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_f:	
	bne	v0, FFI_TYPE_STRUCT_F, cls_retstruct_d_d
	l.s	$f0, V0_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_d_d:	
	bne	v0, FFI_TYPE_STRUCT_DD, cls_retstruct_f_f
	l.d	$f0, V0_OFF2($sp)
	l.d	$f2, V1_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_f_f:	
	bne	v0, FFI_TYPE_STRUCT_FF, cls_retstruct_d_f
	l.s	$f0, V0_OFF2($sp)
	l.s	$f2, V1_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_d_f:	
	bne	v0, FFI_TYPE_STRUCT_DF, cls_retstruct_f_d
	l.d	$f0, V0_OFF2($sp)
	l.s	$f2, V1_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_f_d:	
	bne	v0, FFI_TYPE_STRUCT_FD, cls_retstruct_small2
	l.s	$f0, V0_OFF2($sp)
	l.d	$f2, V1_OFF2($sp)
	b	cls_epilogue
	
cls_retstruct_small2:	
	REG_L	v0, V0_OFF2($sp)
	REG_L	v1, V1_OFF2($sp)
	
	# Epilogue
cls_epilogue:	
	REG_L	ra,  RA_OFF2($sp)	 # Restore return address
	.cpreturn
	ADDU	$sp, SIZEOF_FRAME2
	j	ra
.LFE2:	
	.end	ffi_closure_N32

#ifdef __GNUC__
        .section        .eh_frame,"aw",@progbits
.Lframe1:
        .4byte  .LECIE1-.LSCIE1		# length
.LSCIE1:
        .4byte  0x0			# CIE
        .byte   0x1			# Version 1
        .ascii  "\000"			# Augmentation
        .uleb128 0x1			# Code alignment 1
        .sleb128 -4			# Data alignment -4
        .byte   0x1f			# Return Address $31
        .byte   0xc			# DW_CFA_def_cfa
        .uleb128 0x1d			# in $sp
        .uleb128 0x0			# offset 0
        .align  EH_FRAME_ALIGN
.LECIE1:

.LSFDE1:
        .4byte  .LEFDE1-.LASFDE1	# length.
.LASFDE1:
        .4byte  .LASFDE1-.Lframe1	# CIE_pointer.
        FDE_ADDR_BYTES  .LFB3		# initial_location.
        FDE_ADDR_BYTES  .LFE3-.LFB3	# address_range.
        .byte   0x4			# DW_CFA_advance_loc4
        .4byte  .LCFI0-.LFB3		# to .LCFI0
        .byte   0xe			# DW_CFA_def_cfa_offset
        .uleb128 SIZEOF_FRAME		# adjust stack.by SIZEOF_FRAME
        .byte   0x4			# DW_CFA_advance_loc4
        .4byte  .LCFI1-.LCFI0		# to .LCFI1
        .byte   0x9e			# DW_CFA_offset of $fp
        .uleb128 2*FFI_SIZEOF_ARG/4	# 
        .byte   0x9f			# DW_CFA_offset of ra
        .uleb128 1*FFI_SIZEOF_ARG/4	# 
        .byte   0x4			# DW_CFA_advance_loc4
        .4byte  .LCFI3-.LCFI1		# to .LCFI3
        .byte   0xd			# DW_CFA_def_cfa_register
        .uleb128 0x1e			# in $fp
        .align  EH_FRAME_ALIGN
.LEFDE1:
.LSFDE3:
	.4byte	.LEFDE3-.LASFDE3	# length
.LASFDE3:
	.4byte	.LASFDE3-.Lframe1	# CIE_pointer.
	FDE_ADDR_BYTES	.LFB2		# initial_location.
	FDE_ADDR_BYTES	.LFE2-.LFB2	# address_range.
	.byte	0x4			# DW_CFA_advance_loc4
	.4byte	.LCFI5-.LFB2		# to .LCFI5
	.byte	0xe			# DW_CFA_def_cfa_offset
	.uleb128 SIZEOF_FRAME2		# adjust stack.by SIZEOF_FRAME
	.byte	0x4			# DW_CFA_advance_loc4
	.4byte	.LCFI6-.LCFI5		# to .LCFI6
	.byte	0x9c			# DW_CFA_offset of $gp ($28)
	.uleb128 (SIZEOF_FRAME2 - GP_OFF2)/4
	.byte	0x9f			# DW_CFA_offset of ra ($31)
	.uleb128 (SIZEOF_FRAME2 - RA_OFF2)/4
	.align	EH_FRAME_ALIGN
.LEFDE3:
#endif /* __GNUC__ */	
	
#endif
