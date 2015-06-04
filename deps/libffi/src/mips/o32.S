/* -----------------------------------------------------------------------
   o32.S - Copyright (c) 1996, 1998, 2005  Red Hat, Inc.
   
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

/* Only build this code if we are compiling for o32 */	

#if defined(FFI_MIPS_O32)
	
#define callback a0
#define bytes	 a2
#define flags	 a3
		
#define SIZEOF_FRAME	(4 * FFI_SIZEOF_ARG + 2 * FFI_SIZEOF_ARG)
#define A3_OFF		(SIZEOF_FRAME + 3 * FFI_SIZEOF_ARG)
#define FP_OFF		(SIZEOF_FRAME - 2 * FFI_SIZEOF_ARG)
#define RA_OFF		(SIZEOF_FRAME - 1 * FFI_SIZEOF_ARG)

	.abicalls
	.text
	.align	2
	.globl	ffi_call_O32
	.ent	ffi_call_O32
ffi_call_O32:	
$LFB0:
	# Prologue
	SUBU	$sp, SIZEOF_FRAME	# Frame size
$LCFI0:
	REG_S	$fp, FP_OFF($sp)	# Save frame pointer
$LCFI1:
	REG_S	ra, RA_OFF($sp)		# Save return address
$LCFI2:
	move	$fp, $sp

$LCFI3:
	move	t9, callback		# callback function pointer
	REG_S	flags, A3_OFF($fp)	# flags

	# Allocate at least 4 words in the argstack
	LI	v0, 4 * FFI_SIZEOF_ARG
	blt	bytes, v0, sixteen

	ADDU	v0, bytes, 7	# make sure it is aligned 
	and	v0, -8		# to an 8 byte boundry

sixteen:
	SUBU	$sp, v0		# move the stack pointer to reflect the
				# arg space

	ADDU	a0, $sp, 4 * FFI_SIZEOF_ARG

	jalr	t9
	
	REG_L	t0, A3_OFF($fp)		# load the flags word
	SRL	t2, t0, 4		# shift our arg info
	and     t0, ((1<<4)-1)          # mask out the return type
		
	ADDU	$sp, 4 * FFI_SIZEOF_ARG		# adjust $sp to new args

	bnez	t0, pass_d			# make it quick for int
	REG_L	a0, 0*FFI_SIZEOF_ARG($sp)	# just go ahead and load the
	REG_L	a1, 1*FFI_SIZEOF_ARG($sp)	# four regs.
	REG_L	a2, 2*FFI_SIZEOF_ARG($sp)
	REG_L	a3, 3*FFI_SIZEOF_ARG($sp)
	b	call_it

pass_d:
	bne	t0, FFI_ARGS_D, pass_f
	l.d	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	REG_L	a2,   2*FFI_SIZEOF_ARG($sp)	# passing a double
	REG_L	a3,   3*FFI_SIZEOF_ARG($sp)
	b	call_it

pass_f:	
	bne	t0, FFI_ARGS_F, pass_d_d
	l.s	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	REG_L	a1,   1*FFI_SIZEOF_ARG($sp)	# passing a float
	REG_L	a2,   2*FFI_SIZEOF_ARG($sp)
	REG_L	a3,   3*FFI_SIZEOF_ARG($sp)
	b	call_it		

pass_d_d:		
	bne	t0, FFI_ARGS_DD, pass_f_f
	l.d	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	l.d	$f14, 2*FFI_SIZEOF_ARG($sp)	# passing two doubles
	b	call_it

pass_f_f:	
	bne	t0, FFI_ARGS_FF, pass_d_f
	l.s	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	l.s	$f14, 1*FFI_SIZEOF_ARG($sp)	# passing two floats
	REG_L	a2,   2*FFI_SIZEOF_ARG($sp)
	REG_L	a3,   3*FFI_SIZEOF_ARG($sp)
	b	call_it

pass_d_f:		
	bne	t0, FFI_ARGS_DF, pass_f_d
	l.d	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	l.s	$f14, 2*FFI_SIZEOF_ARG($sp)	# passing double and float
	REG_L	a3,   3*FFI_SIZEOF_ARG($sp)
	b	call_it

pass_f_d:		
 # assume that the only other combination must be float then double
 #	bne	t0, FFI_ARGS_F_D, call_it
	l.s	$f12, 0*FFI_SIZEOF_ARG($sp)	# load $fp regs from args
	l.d	$f14, 2*FFI_SIZEOF_ARG($sp)	# passing double and float

call_it:	
	# Load the function pointer
	REG_L	t9, SIZEOF_FRAME + 5*FFI_SIZEOF_ARG($fp)

	# If the return value pointer is NULL, assume no return value.
	REG_L	t1, SIZEOF_FRAME + 4*FFI_SIZEOF_ARG($fp)
	beqz	t1, noretval

	bne     t2, FFI_TYPE_INT, retlonglong
	jalr	t9
	REG_L	t0, SIZEOF_FRAME + 4*FFI_SIZEOF_ARG($fp)
	REG_S	v0, 0(t0)
	b	epilogue

retlonglong:
	# Really any 64-bit int, signed or not.
	bne	t2, FFI_TYPE_UINT64, retfloat
	jalr	t9
	REG_L	t0, SIZEOF_FRAME + 4*FFI_SIZEOF_ARG($fp)
	REG_S	v1, 4(t0)
	REG_S	v0, 0(t0)
	b	epilogue

retfloat:
	bne     t2, FFI_TYPE_FLOAT, retdouble
	jalr	t9
	REG_L	t0, SIZEOF_FRAME + 4*FFI_SIZEOF_ARG($fp)
	s.s	$f0, 0(t0)
	b	epilogue

retdouble:	
	bne	t2, FFI_TYPE_DOUBLE, noretval
	jalr	t9
	REG_L	t0, SIZEOF_FRAME + 4*FFI_SIZEOF_ARG($fp)
	s.d	$f0, 0(t0)
	b	epilogue
	
noretval:	
	jalr	t9
	
	# Epilogue
epilogue:	
	move	$sp, $fp	
	REG_L	$fp, FP_OFF($sp)	# Restore frame pointer
	REG_L	ra, RA_OFF($sp)		# Restore return address
	ADDU	$sp, SIZEOF_FRAME	# Fix stack pointer
	j	ra

$LFE0:
	.end	ffi_call_O32


/* ffi_closure_O32. Expects address of the passed-in ffi_closure
	in t4 ($12). Stores any arguments passed in registers onto the
	stack, then calls ffi_closure_mips_inner_O32, which
	then decodes them.
	
	Stack layout:

	 3 - a3 save
	 2 - a2 save
	 1 - a1 save
	 0 - a0 save, original sp
	-1 - ra save
	-2 - fp save
	-3 - $16 (s0) save
	-4 - cprestore
	-5 - return value high (v1)
	-6 - return value low (v0)
	-7 - f14 (le high, be low)
	-8 - f14 (le low, be high)
	-9 - f12 (le high, be low)
       -10 - f12 (le low, be high)
       -11 - Called function a3 save
       -12 - Called function a2 save
       -13 - Called function a1 save
       -14 - Called function a0 save, our sp and fp point here
	 */
	
#define SIZEOF_FRAME2	(14 * FFI_SIZEOF_ARG)
#define A3_OFF2		(SIZEOF_FRAME2 + 3 * FFI_SIZEOF_ARG)
#define A2_OFF2		(SIZEOF_FRAME2 + 2 * FFI_SIZEOF_ARG)
#define A1_OFF2		(SIZEOF_FRAME2 + 1 * FFI_SIZEOF_ARG)
#define A0_OFF2		(SIZEOF_FRAME2 + 0 * FFI_SIZEOF_ARG)
#define RA_OFF2		(SIZEOF_FRAME2 - 1 * FFI_SIZEOF_ARG)
#define FP_OFF2		(SIZEOF_FRAME2 - 2 * FFI_SIZEOF_ARG)
#define S0_OFF2		(SIZEOF_FRAME2 - 3 * FFI_SIZEOF_ARG)
#define GP_OFF2		(SIZEOF_FRAME2 - 4 * FFI_SIZEOF_ARG)
#define V1_OFF2		(SIZEOF_FRAME2 - 5 * FFI_SIZEOF_ARG)
#define V0_OFF2		(SIZEOF_FRAME2 - 6 * FFI_SIZEOF_ARG)
#define FA_1_1_OFF2	(SIZEOF_FRAME2 - 7 * FFI_SIZEOF_ARG)
#define FA_1_0_OFF2	(SIZEOF_FRAME2 - 8 * FFI_SIZEOF_ARG)
#define FA_0_1_OFF2	(SIZEOF_FRAME2 - 9 * FFI_SIZEOF_ARG)
#define FA_0_0_OFF2	(SIZEOF_FRAME2 - 10 * FFI_SIZEOF_ARG)

	.text
	.align	2
	.globl	ffi_closure_O32
	.ent	ffi_closure_O32
ffi_closure_O32:
$LFB1:
	# Prologue
	.frame	$fp, SIZEOF_FRAME2, ra
	.set	noreorder
	.cpload	t9
	.set	reorder
	SUBU	$sp, SIZEOF_FRAME2
	.cprestore GP_OFF2
$LCFI4:
	REG_S	$16, S0_OFF2($sp)	 # Save s0
	REG_S	$fp, FP_OFF2($sp)	 # Save frame pointer
	REG_S	ra, RA_OFF2($sp)	 # Save return address
$LCFI6:
	move	$fp, $sp

$LCFI7:
	# Store all possible argument registers. If there are more than
	# four arguments, then they are stored above where we put a3.
	REG_S	a0, A0_OFF2($fp)
	REG_S	a1, A1_OFF2($fp)
	REG_S	a2, A2_OFF2($fp)
	REG_S	a3, A3_OFF2($fp)

	# Load ABI enum to s0
	REG_L	$16, 20($12)	# cif pointer follows tramp.
	REG_L	$16, 0($16)	# abi is first member.

	li	$13, 1		# FFI_O32
	bne	$16, $13, 1f	# Skip fp save if FFI_O32_SOFT_FLOAT
	
	# Store all possible float/double registers.
	s.d	$f12, FA_0_0_OFF2($fp)
	s.d	$f14, FA_1_0_OFF2($fp)
1:	
	# Call ffi_closure_mips_inner_O32 to do the work.
	la	t9, ffi_closure_mips_inner_O32
	move	a0, $12	 # Pointer to the ffi_closure
	addu	a1, $fp, V0_OFF2
	addu	a2, $fp, A0_OFF2
	addu	a3, $fp, FA_0_0_OFF2
	jalr	t9

	# Load the return value into the appropriate register.
	move	$8, $2
	li	$9, FFI_TYPE_VOID
	beq	$8, $9, closure_done

	li	$13, 1		# FFI_O32
	bne	$16, $13, 1f	# Skip fp restore if FFI_O32_SOFT_FLOAT

	li	$9, FFI_TYPE_FLOAT
	l.s	$f0, V0_OFF2($fp)
	beq	$8, $9, closure_done

	li	$9, FFI_TYPE_DOUBLE
	l.d	$f0, V0_OFF2($fp)
	beq	$8, $9, closure_done
1:	
	REG_L	$3, V1_OFF2($fp)
	REG_L	$2, V0_OFF2($fp)

closure_done:
	# Epilogue
	move	$sp, $fp
	REG_L	$16, S0_OFF2($sp)	 # Restore s0
	REG_L	$fp, FP_OFF2($sp)	 # Restore frame pointer
	REG_L	ra,  RA_OFF2($sp)	 # Restore return address
	ADDU	$sp, SIZEOF_FRAME2
	j	ra
$LFE1:
	.end	ffi_closure_O32

/* DWARF-2 unwind info. */

	.section	.eh_frame,"a",@progbits
$Lframe0:
	.4byte	$LECIE0-$LSCIE0	 # Length of Common Information Entry
$LSCIE0:
	.4byte	0x0	 # CIE Identifier Tag
	.byte	0x1	 # CIE Version
	.ascii "zR\0"	 # CIE Augmentation
	.uleb128 0x1	 # CIE Code Alignment Factor
	.sleb128 4	 # CIE Data Alignment Factor
	.byte	0x1f	 # CIE RA Column
	.uleb128 0x1	 # Augmentation size
	.byte	0x00	 # FDE Encoding (absptr)
	.byte	0xc	 # DW_CFA_def_cfa
	.uleb128 0x1d
	.uleb128 0x0
	.align	2
$LECIE0:
$LSFDE0:
	.4byte	$LEFDE0-$LASFDE0	 # FDE Length
$LASFDE0:
	.4byte	$LASFDE0-$Lframe0	 # FDE CIE offset
	.4byte	$LFB0	 # FDE initial location
	.4byte	$LFE0-$LFB0	 # FDE address range
	.uleb128 0x0	 # Augmentation size
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI0-$LFB0
	.byte	0xe	 # DW_CFA_def_cfa_offset
	.uleb128 0x18
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI2-$LCFI0
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x1e	 # $fp
	.sleb128 -2	 # SIZEOF_FRAME2 - 2*FFI_SIZEOF_ARG($sp)
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x1f	 # $ra
	.sleb128 -1	 # SIZEOF_FRAME2 - 1*FFI_SIZEOF_ARG($sp)
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI3-$LCFI2
	.byte	0xc	 # DW_CFA_def_cfa
	.uleb128 0x1e
	.uleb128 0x18
	.align	2
$LEFDE0:
$LSFDE1:
	.4byte	$LEFDE1-$LASFDE1	 # FDE Length
$LASFDE1:
	.4byte	$LASFDE1-$Lframe0	 # FDE CIE offset
	.4byte	$LFB1	 # FDE initial location
	.4byte	$LFE1-$LFB1	 # FDE address range
	.uleb128 0x0	 # Augmentation size
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI4-$LFB1
	.byte	0xe	 # DW_CFA_def_cfa_offset
	.uleb128 0x38
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI6-$LCFI4
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x10	 # $16
	.sleb128 -3	 # SIZEOF_FRAME2 - 3*FFI_SIZEOF_ARG($sp)
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x1e	 # $fp
	.sleb128 -2	 # SIZEOF_FRAME2 - 2*FFI_SIZEOF_ARG($sp)
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x1f	 # $ra
	.sleb128 -1	 # SIZEOF_FRAME2 - 1*FFI_SIZEOF_ARG($sp)
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	$LCFI7-$LCFI6
	.byte	0xc	 # DW_CFA_def_cfa
	.uleb128 0x1e
	.uleb128 0x38
	.align	2
$LEFDE1:

#endif
