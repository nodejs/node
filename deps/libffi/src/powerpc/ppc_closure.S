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
#include <powerpc/asm.h>

	.file   "ppc_closure.S"

#ifndef __powerpc64__

ENTRY(ffi_closure_SYSV)
.LFB1:
	stwu %r1,-144(%r1)
.LCFI0:
	mflr %r0
.LCFI1:
	stw %r0,148(%r1)

# we want to build up an areas for the parameters passed
# in registers (both floating point and integer)

	# so first save gpr 3 to gpr 10 (aligned to 4)
	stw   %r3, 16(%r1)
	stw   %r4, 20(%r1)
	stw   %r5, 24(%r1)
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

	# set up registers for the routine that actually does the work
	# get the context pointer from the trampoline
	mr %r3,%r11

	# now load up the pointer to the result storage
	addi %r4,%r1,112

	# now load up the pointer to the saved gpr registers
	addi %r5,%r1,16

	# now load up the pointer to the saved fpr registers */
	addi %r6,%r1,48

	# now load up the pointer to the outgoing parameter
	# stack in the previous frame
	# i.e. the previous frame pointer + 8
	addi %r7,%r1,152

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
	bctr			# jump to it
.LFE1:

# Each of the ret_typeX code fragments has to be exactly 16 bytes long
# (4 instructions). For cache effectiveness we align to a 16 byte boundary
# first.
	.align 4
# case FFI_TYPE_VOID
.Lret_type0:
	mtlr %r0
	addi %r1,%r1,144
	blr
	nop

# case FFI_TYPE_INT
	lwz %r3,112+0(%r1)
	mtlr %r0
.Lfinish:
	addi %r1,%r1,144
	blr

# case FFI_TYPE_FLOAT
#ifndef __NO_FPRS__
	lfs %f1,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
#else
	nop
	nop
	nop
#endif
	blr

# case FFI_TYPE_DOUBLE
#ifndef __NO_FPRS__
	lfd %f1,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
#else
	nop
	nop
	nop
#endif
	blr

# case FFI_TYPE_LONGDOUBLE
#ifndef __NO_FPRS__
	lfd %f1,112+0(%r1)
	lfd %f2,112+8(%r1)
	mtlr %r0
	b .Lfinish
#else
	nop
	nop
	nop
	blr
#endif

# case FFI_TYPE_UINT8
	lbz %r3,112+3(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_SINT8
	lbz %r3,112+3(%r1)
	extsb %r3,%r3
	mtlr %r0
	b .Lfinish

# case FFI_TYPE_UINT16
	lhz %r3,112+2(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_SINT16
	lha %r3,112+2(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_UINT32
	lwz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_SINT32
	lwz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_UINT64
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	mtlr %r0
	b .Lfinish

# case FFI_TYPE_SINT64
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	mtlr %r0
	b .Lfinish

# case FFI_TYPE_STRUCT
	mtlr %r0
	addi %r1,%r1,144
	blr
	nop

# case FFI_TYPE_POINTER
	lwz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_TYPE_UINT128
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	lwz %r5,112+8(%r1)
	bl .Luint128

# The return types below are only used when the ABI type is FFI_SYSV.
# case FFI_SYSV_TYPE_SMALL_STRUCT + 1. One byte struct.
	lbz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_SYSV_TYPE_SMALL_STRUCT + 2. Two byte struct.
	lhz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_SYSV_TYPE_SMALL_STRUCT + 3. Three byte struct.
	lwz %r3,112+0(%r1)
	srwi %r3,%r3,8
	mtlr %r0
	b .Lfinish

# case FFI_SYSV_TYPE_SMALL_STRUCT + 4. Four byte struct.
	lwz %r3,112+0(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

# case FFI_SYSV_TYPE_SMALL_STRUCT + 5. Five byte struct.
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,24
	b .Lstruct567

# case FFI_SYSV_TYPE_SMALL_STRUCT + 6. Six byte struct.
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,16
	b .Lstruct567

# case FFI_SYSV_TYPE_SMALL_STRUCT + 7. Seven byte struct.
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	li %r5,8
	b .Lstruct567

# case FFI_SYSV_TYPE_SMALL_STRUCT + 8. Eight byte struct.
	lwz %r3,112+0(%r1)
	lwz %r4,112+4(%r1)
	mtlr %r0
	b .Lfinish

.Lstruct567:
	subfic %r6,%r5,32
	srw %r4,%r4,%r5
	slw %r6,%r3,%r6
	srw %r3,%r3,%r5
	or %r4,%r6,%r4
	mtlr %r0
	addi %r1,%r1,144
	blr

.Luint128:
	lwz %r6,112+12(%r1)
	mtlr %r0
	addi %r1,%r1,144
	blr

END(ffi_closure_SYSV)

	.section	".eh_frame",EH_FRAME_FLAGS,@progbits
.Lframe1:
	.4byte	.LECIE1-.LSCIE1	 # Length of Common Information Entry
.LSCIE1:
	.4byte	0x0	 # CIE Identifier Tag
	.byte	0x1	 # CIE Version
#if defined _RELOCATABLE || defined __PIC__
	.ascii "zR\0"	 # CIE Augmentation
#else
	.ascii "\0"	 # CIE Augmentation
#endif
	.uleb128 0x1	 # CIE Code Alignment Factor
	.sleb128 -4	 # CIE Data Alignment Factor
	.byte	0x41	 # CIE RA Column
#if defined _RELOCATABLE || defined __PIC__
	.uleb128 0x1	 # Augmentation size
	.byte	0x1b	 # FDE Encoding (pcrel sdata4)
#endif
	.byte	0xc	 # DW_CFA_def_cfa
	.uleb128 0x1
	.uleb128 0x0
	.align 2
.LECIE1:
.LSFDE1:
	.4byte	.LEFDE1-.LASFDE1	 # FDE Length
.LASFDE1:
	.4byte	.LASFDE1-.Lframe1	 # FDE CIE offset
#if defined _RELOCATABLE || defined __PIC__
	.4byte	.LFB1-.	 # FDE initial location
#else
	.4byte	.LFB1	 # FDE initial location
#endif
	.4byte	.LFE1-.LFB1	 # FDE address range
#if defined _RELOCATABLE || defined __PIC__
	.uleb128 0x0	 # Augmentation size
#endif
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	.LCFI0-.LFB1
	.byte	0xe	 # DW_CFA_def_cfa_offset
	.uleb128 144
	.byte	0x4	 # DW_CFA_advance_loc4
	.4byte	.LCFI1-.LCFI0
	.byte	0x11	 # DW_CFA_offset_extended_sf
	.uleb128 0x41
	.sleb128 -1
	.align 2
.LEFDE1:

#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
