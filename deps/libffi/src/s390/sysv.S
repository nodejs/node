/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 2000 Software AG
            Copyright (c) 2008 Red Hat, Inc.
 
   S390 Foreign Function Interface
 
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

#ifndef __s390x__
 
.text

	# r2:	cif->bytes
	# r3:	&ecif
	# r4:	ffi_prep_args
	# r5:	ret_type
	# r6:	ecif.rvalue
	# ov:	fn 
 
	# This assumes we are using gas.
	.globl	ffi_call_SYSV
	.type	ffi_call_SYSV,%function
ffi_call_SYSV:
.LFB1:
	stm	%r6,%r15,24(%r15)		# Save registers
.LCFI0:
	basr	%r13,0				# Set up base register
.Lbase:
	lr	%r11,%r15			# Set up frame pointer
.LCFI1:
	sr	%r15,%r2
	ahi	%r15,-96-48			# Allocate stack
	lr	%r8,%r6				# Save ecif.rvalue
	sr	%r9,%r9
	ic	%r9,.Ltable-.Lbase(%r13,%r5)	# Load epilog address
	l	%r7,96(%r11)			# Load function address
	st	%r11,0(%r15)			# Set up back chain
	ahi	%r11,-48			# Register save area
.LCFI2:

	la	%r2,96(%r15)			# Save area
						# r3 already holds &ecif
	basr	%r14,%r4			# Call ffi_prep_args

	lm	%r2,%r6,0(%r11)			# Load arguments
	ld	%f0,32(%r11)
	ld	%f2,40(%r11)
	la	%r14,0(%r13,%r9)		# Set return address
	br	%r7				# ... and call function

.LretNone:					# Return void
	l	%r4,48+56(%r11)
	lm	%r6,%r15,48+24(%r11)
	br	%r4

.LretFloat:
	l	%r4,48+56(%r11)
	ste	%f0,0(%r8)			# Return float
	lm	%r6,%r15,48+24(%r11)
	br	%r4
 
.LretDouble:
	l	%r4,48+56(%r11)
	std	%f0,0(%r8)			# Return double
	lm	%r6,%r15,48+24(%r11)
	br	%r4

.LretInt32:
	l	%r4,48+56(%r11)
	st	%r2,0(%r8)			# Return int
	lm	%r6,%r15,48+24(%r11)
	br	%r4
 
.LretInt64:
	l	%r4,48+56(%r11)
	stm	%r2,%r3,0(%r8)			# Return long long
	lm	%r6,%r15,48+24(%r11)
	br	%r4
 
.Ltable:
	.byte	.LretNone-.Lbase		# FFI390_RET_VOID
	.byte	.LretNone-.Lbase		# FFI390_RET_STRUCT
	.byte	.LretFloat-.Lbase		# FFI390_RET_FLOAT
	.byte	.LretDouble-.Lbase		# FFI390_RET_DOUBLE
	.byte	.LretInt32-.Lbase		# FFI390_RET_INT32
	.byte	.LretInt64-.Lbase		# FFI390_RET_INT64

.LFE1: 
.ffi_call_SYSV_end:
	.size	 ffi_call_SYSV,.ffi_call_SYSV_end-ffi_call_SYSV


	.globl	ffi_closure_SYSV
	.type	ffi_closure_SYSV,%function
ffi_closure_SYSV:
.LFB2:
	stm	%r12,%r15,48(%r15)		# Save registers
.LCFI10:
	basr	%r13,0				# Set up base register
.Lcbase:
	stm	%r2,%r6,8(%r15)			# Save arguments
	std	%f0,64(%r15)
	std	%f2,72(%r15)
	lr	%r1,%r15			# Set up stack frame
	ahi	%r15,-96
.LCFI11:
	l	%r12,.Lchelper-.Lcbase(%r13)	# Get helper function
	lr	%r2,%r0				# Closure
	la	%r3,8(%r1)			# GPRs
	la	%r4,64(%r1)			# FPRs
	la	%r5,96(%r1)			# Overflow
	st	%r1,0(%r15)			# Set up back chain

	bas	%r14,0(%r12,%r13)		# Call helper

	l	%r4,96+56(%r15)
	ld	%f0,96+64(%r15)			# Load return registers
	lm	%r2,%r3,96+8(%r15)
	lm	%r12,%r15,96+48(%r15)
	br	%r4

	.align 4
.Lchelper:
	.long	ffi_closure_helper_SYSV-.Lcbase

.LFE2: 

.ffi_closure_SYSV_end:
	.size	 ffi_closure_SYSV,.ffi_closure_SYSV_end-ffi_closure_SYSV


	.section	.eh_frame,EH_FRAME_FLAGS,@progbits
.Lframe1:
	.4byte	.LECIE1-.LSCIE1	# Length of Common Information Entry
.LSCIE1:
	.4byte	0x0	# CIE Identifier Tag
	.byte	0x1	# CIE Version
	.ascii "zR\0"	# CIE Augmentation
	.uleb128 0x1	# CIE Code Alignment Factor
	.sleb128 -4	# CIE Data Alignment Factor
	.byte	0xe	# CIE RA Column
	.uleb128 0x1	# Augmentation size
	.byte	0x1b	# FDE Encoding (pcrel sdata4)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0xf
	.uleb128 0x60
	.align	4
.LECIE1:
.LSFDE1:
	.4byte	.LEFDE1-.LASFDE1	# FDE Length
.LASFDE1:
	.4byte	.LASFDE1-.Lframe1	# FDE CIE offset
	.4byte	.LFB1-.	# FDE initial location
	.4byte	.LFE1-.LFB1	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI0-.LFB1
	.byte	0x8f	# DW_CFA_offset, column 0xf
	.uleb128 0x9
	.byte	0x8e	# DW_CFA_offset, column 0xe
	.uleb128 0xa
	.byte	0x8d	# DW_CFA_offset, column 0xd
	.uleb128 0xb
	.byte	0x8c	# DW_CFA_offset, column 0xc
	.uleb128 0xc
	.byte	0x8b	# DW_CFA_offset, column 0xb
	.uleb128 0xd
	.byte	0x8a	# DW_CFA_offset, column 0xa
	.uleb128 0xe
	.byte	0x89	# DW_CFA_offset, column 0x9
	.uleb128 0xf
	.byte	0x88	# DW_CFA_offset, column 0x8
	.uleb128 0x10
	.byte	0x87	# DW_CFA_offset, column 0x7
	.uleb128 0x11
	.byte	0x86	# DW_CFA_offset, column 0x6
	.uleb128 0x12
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI1-.LCFI0
	.byte	0xd	# DW_CFA_def_cfa_register
	.uleb128 0xb
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI2-.LCFI1
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0x90
	.align	4
.LEFDE1:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2	# FDE Length
.LASFDE2:
	.4byte	.LASFDE2-.Lframe1	# FDE CIE offset
	.4byte	.LFB2-.	# FDE initial location
	.4byte	.LFE2-.LFB2	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI10-.LFB2
	.byte	0x8f	# DW_CFA_offset, column 0xf
	.uleb128 0x9
	.byte	0x8e	# DW_CFA_offset, column 0xe
	.uleb128 0xa
	.byte	0x8d	# DW_CFA_offset, column 0xd
	.uleb128 0xb
	.byte	0x8c	# DW_CFA_offset, column 0xc
	.uleb128 0xc
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI11-.LCFI10
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0xc0
	.align	4
.LEFDE2:

#else
 
.text
 
	# r2:	cif->bytes
	# r3:	&ecif
	# r4:	ffi_prep_args
	# r5:	ret_type
	# r6:	ecif.rvalue
	# ov:	fn 
 
	# This assumes we are using gas.
	.globl	ffi_call_SYSV
	.type	ffi_call_SYSV,%function
ffi_call_SYSV:
.LFB1:
	stmg	%r6,%r15,48(%r15)		# Save registers
.LCFI0:
	larl	%r13,.Lbase			# Set up base register
	lgr	%r11,%r15			# Set up frame pointer
.LCFI1:
	sgr	%r15,%r2
	aghi	%r15,-160-80			# Allocate stack
	lgr	%r8,%r6				# Save ecif.rvalue
	llgc	%r9,.Ltable-.Lbase(%r13,%r5)	# Load epilog address
	lg	%r7,160(%r11)			# Load function address
	stg	%r11,0(%r15)			# Set up back chain
	aghi	%r11,-80			# Register save area
.LCFI2:

	la	%r2,160(%r15)			# Save area
						# r3 already holds &ecif
	basr	%r14,%r4			# Call ffi_prep_args

	lmg	%r2,%r6,0(%r11)			# Load arguments
	ld	%f0,48(%r11)
	ld	%f2,56(%r11)
	ld	%f4,64(%r11)
	ld	%f6,72(%r11)
	la	%r14,0(%r13,%r9)		# Set return address
	br	%r7				# ... and call function

.Lbase:
.LretNone:					# Return void
	lg	%r4,80+112(%r11)
	lmg	%r6,%r15,80+48(%r11)
	br	%r4

.LretFloat:
	lg	%r4,80+112(%r11)
	ste	%f0,0(%r8)			# Return float
	lmg	%r6,%r15,80+48(%r11)
	br	%r4
 
.LretDouble:
	lg	%r4,80+112(%r11)
	std	%f0,0(%r8)			# Return double
	lmg	%r6,%r15,80+48(%r11)
	br	%r4

.LretInt32:
	lg	%r4,80+112(%r11)
	st	%r2,0(%r8)			# Return int
	lmg	%r6,%r15,80+48(%r11)
	br	%r4
 
.LretInt64:
	lg	%r4,80+112(%r11)
	stg	%r2,0(%r8)			# Return long
	lmg	%r6,%r15,80+48(%r11)
	br	%r4
 
.Ltable:
	.byte	.LretNone-.Lbase		# FFI390_RET_VOID
	.byte	.LretNone-.Lbase		# FFI390_RET_STRUCT
	.byte	.LretFloat-.Lbase		# FFI390_RET_FLOAT
	.byte	.LretDouble-.Lbase		# FFI390_RET_DOUBLE
	.byte	.LretInt32-.Lbase		# FFI390_RET_INT32
	.byte	.LretInt64-.Lbase		# FFI390_RET_INT64

.LFE1: 
.ffi_call_SYSV_end:
	.size	 ffi_call_SYSV,.ffi_call_SYSV_end-ffi_call_SYSV


	.globl	ffi_closure_SYSV
	.type	ffi_closure_SYSV,%function
ffi_closure_SYSV:
.LFB2:
	stmg	%r14,%r15,112(%r15)		# Save registers
.LCFI10:
	stmg	%r2,%r6,16(%r15)		# Save arguments
	std	%f0,128(%r15)
	std	%f2,136(%r15)
	std	%f4,144(%r15)
	std	%f6,152(%r15)
	lgr	%r1,%r15			# Set up stack frame
	aghi	%r15,-160
.LCFI11:
	lgr	%r2,%r0				# Closure
	la	%r3,16(%r1)			# GPRs
	la	%r4,128(%r1)			# FPRs
	la	%r5,160(%r1)			# Overflow
	stg	%r1,0(%r15)			# Set up back chain

	brasl	%r14,ffi_closure_helper_SYSV	# Call helper

	lg	%r14,160+112(%r15)
	ld	%f0,160+128(%r15)		# Load return registers
	lg	%r2,160+16(%r15)
	la	%r15,160(%r15)
	br	%r14
.LFE2: 

.ffi_closure_SYSV_end:
	.size	 ffi_closure_SYSV,.ffi_closure_SYSV_end-ffi_closure_SYSV



	.section	.eh_frame,EH_FRAME_FLAGS,@progbits
.Lframe1:
	.4byte	.LECIE1-.LSCIE1	# Length of Common Information Entry
.LSCIE1:
	.4byte	0x0	# CIE Identifier Tag
	.byte	0x1	# CIE Version
	.ascii "zR\0"	# CIE Augmentation
	.uleb128 0x1	# CIE Code Alignment Factor
	.sleb128 -8	# CIE Data Alignment Factor
	.byte	0xe	# CIE RA Column
	.uleb128 0x1	# Augmentation size
	.byte	0x1b	# FDE Encoding (pcrel sdata4)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0xf
	.uleb128 0xa0
	.align	8
.LECIE1:
.LSFDE1:
	.4byte	.LEFDE1-.LASFDE1	# FDE Length
.LASFDE1:
	.4byte	.LASFDE1-.Lframe1	# FDE CIE offset
	.4byte	.LFB1-.	# FDE initial location
	.4byte	.LFE1-.LFB1	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI0-.LFB1
	.byte	0x8f	# DW_CFA_offset, column 0xf
	.uleb128 0x5
	.byte	0x8e	# DW_CFA_offset, column 0xe
	.uleb128 0x6
	.byte	0x8d	# DW_CFA_offset, column 0xd
	.uleb128 0x7
	.byte	0x8c	# DW_CFA_offset, column 0xc
	.uleb128 0x8
	.byte	0x8b	# DW_CFA_offset, column 0xb
	.uleb128 0x9
	.byte	0x8a	# DW_CFA_offset, column 0xa
	.uleb128 0xa
	.byte	0x89	# DW_CFA_offset, column 0x9
	.uleb128 0xb
	.byte	0x88	# DW_CFA_offset, column 0x8
	.uleb128 0xc
	.byte	0x87	# DW_CFA_offset, column 0x7
	.uleb128 0xd
	.byte	0x86	# DW_CFA_offset, column 0x6
	.uleb128 0xe
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI1-.LCFI0
	.byte	0xd	# DW_CFA_def_cfa_register
	.uleb128 0xb
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI2-.LCFI1
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0xf0
	.align	8
.LEFDE1:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2	# FDE Length
.LASFDE2:
	.4byte	.LASFDE2-.Lframe1	# FDE CIE offset
	.4byte	.LFB2-.	# FDE initial location
	.4byte	.LFE2-.LFB2	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI10-.LFB2
	.byte	0x8f	# DW_CFA_offset, column 0xf
	.uleb128 0x5
	.byte	0x8e	# DW_CFA_offset, column 0xe
	.uleb128 0x6
	.byte	0x4	# DW_CFA_advance_loc4
	.4byte	.LCFI11-.LCFI10
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0x140
	.align	8
.LEFDE2:

#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
