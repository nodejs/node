/* -----------------------------------------------------------------------
   sysv.h - Copyright (c) 2003 Jakub Jelinek <jakub@redhat.com>
	    Copyright (c) 2008 Red Hat, Inc.

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
#include "internal.h"
#include <powerpc/asm.h>

	.file   "ppc_closure.S"

#ifndef POWERPC64

FFI_HIDDEN(ffi_closure_SYSV)
ENTRY(ffi_closure_SYSV)
	.cfi_startproc
	stwu %r1,-144(%r1)
	.cfi_def_cfa_offset 144
	mflr %r0
	stw %r0,148(%r1)
	.cfi_offset 65, 4

# we want to build up an areas for the parameters passed
# in registers (both floating point and integer)

	# so first save gpr 3 to gpr 10 (aligned to 4)
	stw   %r3, 16(%r1)
	stw   %r4, 20(%r1)
	stw   %r5, 24(%r1)

	# set up registers for the routine that does the work

	# closure->cif
	lwz %r3,FFI_TRAMPOLINE_SIZE(%r11)
	# closure->fun
	lwz %r4,FFI_TRAMPOLINE_SIZE+4(%r11)
	# closure->user_data
	lwz %r5,FFI_TRAMPOLINE_SIZE+8(%r11)

.Ldoclosure:
	stw   %r6, 28(%r1)
	stw   %r7, 32(%r1)
	stw   %r8, 36(%r1)
	stw   %r9, 40(%r1)
	stw   %r10,44(%r1)

#ifndef __NO_FPRS__
	# next save fpr 1 to fpr 8 (aligned to 8)
	stfd  %f1, 48(%r1)
	stfd  %f2, 56(%r1)
	stfd  %f3, 64(%r1)
	stfd  %f4, 72(%r1)
	stfd  %f5, 80(%r1)
	stfd  %f6, 88(%r1)
	stfd  %f7, 96(%r1)
	stfd  %f8, 104(%r1)
#endif

	# pointer to the result storage
	addi %r6,%r1,112

	# pointer to the saved gpr registers
	addi %r7,%r1,16

	# pointer to the saved fpr registers
	addi %r8,%r1,48

	# pointer to the outgoing parameter save area in the previous frame
	# i.e. the previous frame pointer + 8
	addi %r9,%r1,152

	# make the call
	bl ffi_closure_helper_SYSV@local
.Lret:
	# now r3 contains the return type
	# so use it to look up in a table
	# so we know how to deal with each type

	# look up the proper starting point in table
	# by using return type as offset

	mflr %r4		# move address of .Lret to r4
	slwi %r3,%r3,4		# now multiply return type by 16
	addi %r4, %r4, .Lret_type0 - .Lret
	lwz %r0,148(%r1)
	add %r3,%r3,%r4		# add contents of table to table address
	mtctr %r3
	mtlr %r0
	.cfi_restore 65
	bctr			# jump to it

# Each of the ret_typeX code fragments has to be exactly 16 bytes long
# (4 instructions). For cache effectiveness we align to a 16 byte boundary
# first.

	.align 4
.Lret_type0:

.macro E index
	.align 4
.endm

.macro epilogue
	addi %r1, %r1, 144
	.cfi_def_cfa_offset 0
	blr
	.cfi_def_cfa_offset 144
.endm

E PPC_LD_NONE
	epilogue

E PPC_LD_R3
	lwz %r3,112+0(%r1)
	epilogue

E PPC_LD_R3R4
.Lr3r4:
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	epilogue

E PPC_LD_F32
#ifdef __NO_FPRS__
	trap
#else
	lfs %f1,112+0(%r1)
	epilogue
#endif

E PPC_LD_F64
#ifdef __NO_FPRS__
	trap
#else
	lfd %f1,112+0(%r1)
	epilogue
#endif

E PPC_LD_F128
#ifdef __NO_FPRS__
	trap
#else
	lfd %f1,112+0(%r1)
	lfd %f2,112+8(%r1)
	epilogue
#endif

E PPC_LD_U8
#ifdef __LITTLE_ENDIAN__
	lbz %r3,112+0(%r1)
#else
	lbz %r3,112+3(%r1)
#endif
	epilogue

E PPC_LD_S8
#ifdef __LITTLE_ENDIAN__
	lbz %r3,112+0(%r1)
#else
	lbz %r3,112+3(%r1)
#endif
	extsb %r3,%r3
	epilogue

E PPC_LD_U16
#ifdef __LITTLE_ENDIAN__
	lhz %r3,112+0(%r1)
#else
	lhz %r3,112+2(%r1)
#endif
	epilogue

E PPC_LD_S16
#ifdef __LITTLE_ENDIAN__
	lha %r3,112+0(%r1)
#else
	lha %r3,112+2(%r1)
#endif
	epilogue

E PPC32_LD_R3R6
	lwz %r5,112+8(%r1)
	lwz %r6,112+12(%r1)
	b .Lr3r4

#ifndef __LITTLE_ENDIAN__
E PPC32_SYSV_LD_STRUCT_3
	lwz %r3,112+0(%r1)
	srwi %r3,%r3,8
	epilogue

E PPC32_SYSV_LD_STRUCT_5
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,24
	b .Lstruct567

E PPC32_SYSV_LD_STRUCT_6
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,16
	b .Lstruct567

E PPC32_SYSV_LD_STRUCT_7
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,8
	nop

.Lstruct567:
	subfic %r6,%r5,32
	srw %r4,%r4,%r5
	slw %r6,%r3,%r6
	srw %r3,%r3,%r5
	or %r4,%r6,%r4
	epilogue
#endif

	.cfi_endproc
END(ffi_closure_SYSV)


FFI_HIDDEN(ffi_go_closure_sysv)
ENTRY(ffi_go_closure_sysv)
	.cfi_startproc
	stwu %r1,-144(%r1)
	.cfi_def_cfa_offset 144
	mflr %r0
	stw %r0,148(%r1)
	.cfi_offset 65, 4

	stw   %r3, 16(%r1)
	stw   %r4, 20(%r1)
	stw   %r5, 24(%r1)

	# closure->cif
	lwz %r3,4(%r11)
	# closure->fun
	lwz %r4,8(%r11)
	# user_data
	mr %r5,%r11
	b .Ldoclosure
	.cfi_endproc
END(ffi_go_closure_sysv)

#ifdef FFI_EXEC_STATIC_TRAMP
	.text
	.align PPC_TRAMP_MAP_SHIFT
	FFI_HIDDEN (trampoline_code_table)
	.globl  trampoline_code_table
	.type   trampoline_code_table,@function
trampoline_code_table:
	.rept   PPC_TRAMP_MAP_SIZE / PPC_TRAMP_SIZE
	mflr    %r0
	bcl     20,31,$+4
	mflr    %r11
	addis   %r11,%r11,PPC_TRAMP_MAP_SIZE@ha
	mtlr    %r0
	lwz     %r0,(PPC_TRAMP_MAP_SIZE-4)@l(%r11)
	mtctr   %r0
	lwz     %r11,(PPC_TRAMP_MAP_SIZE-8)@l(%r11)
	bctr
	nop
	.endr
	.size   trampoline_code_table,.-trampoline_code_table
	.align PPC_TRAMP_MAP_SHIFT
#endif /* FFI_EXEC_STATIC_TRAMP */

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
#endif
