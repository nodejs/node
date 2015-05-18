/* -----------------------------------------------------------------------
   darwin64.S - Copyright (c) 2006 Free Software Foundation, Inc.
	        Copyright (c) 2008 Red Hat, Inc.
   derived from unix64.S

   x86-64 Foreign Function Interface for Darwin.

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

#ifdef __x86_64__
#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>

	.file "darwin64.S"
.text

/* ffi_call_unix64 (void *args, unsigned long bytes, unsigned flags,
		    void *raddr, void (*fnaddr)(void));

   Bit o trickiness here -- ARGS+BYTES is the base of the stack frame
   for this function.  This has been allocated by ffi_call.  We also
   deallocate some of the stack that has been alloca'd.  */

	.align	3
	.globl	_ffi_call_unix64

_ffi_call_unix64:
LUW0:
	movq	(%rsp), %r10		/* Load return address.  */
	leaq	(%rdi, %rsi), %rax	/* Find local stack base.  */
	movq	%rdx, (%rax)		/* Save flags.  */
	movq	%rcx, 8(%rax)		/* Save raddr.  */
	movq	%rbp, 16(%rax)		/* Save old frame pointer.  */
	movq	%r10, 24(%rax)		/* Relocate return address.  */
	movq	%rax, %rbp		/* Finalize local stack frame.  */
LUW1:
	movq	%rdi, %r10		/* Save a copy of the register area. */
	movq	%r8, %r11		/* Save a copy of the target fn.  */
	movl	%r9d, %eax		/* Set number of SSE registers.  */

	/* Load up all argument registers.  */
	movq	(%r10), %rdi
	movq	8(%r10), %rsi
	movq	16(%r10), %rdx
	movq	24(%r10), %rcx
	movq	32(%r10), %r8
	movq	40(%r10), %r9
	testl	%eax, %eax
	jnz	Lload_sse
Lret_from_load_sse:

	/* Deallocate the reg arg area.  */
	leaq	176(%r10), %rsp

	/* Call the user function.  */
	call	*%r11

	/* Deallocate stack arg area; local stack frame in redzone.  */
	leaq	24(%rbp), %rsp

	movq	0(%rbp), %rcx		/* Reload flags.  */
	movq	8(%rbp), %rdi		/* Reload raddr.  */
	movq	16(%rbp), %rbp		/* Reload old frame pointer.  */
LUW2:

	/* The first byte of the flags contains the FFI_TYPE.  */
	movzbl	%cl, %r10d
	leaq	Lstore_table(%rip), %r11
	movslq	(%r11, %r10, 4), %r10
	addq	%r11, %r10
	jmp	*%r10

Lstore_table:
	.long	Lst_void-Lstore_table		/* FFI_TYPE_VOID */
	.long	Lst_sint32-Lstore_table		/* FFI_TYPE_INT */
	.long	Lst_float-Lstore_table		/* FFI_TYPE_FLOAT */
	.long	Lst_double-Lstore_table		/* FFI_TYPE_DOUBLE */
	.long	Lst_ldouble-Lstore_table	/* FFI_TYPE_LONGDOUBLE */
	.long	Lst_uint8-Lstore_table		/* FFI_TYPE_UINT8 */
	.long	Lst_sint8-Lstore_table		/* FFI_TYPE_SINT8 */
	.long	Lst_uint16-Lstore_table		/* FFI_TYPE_UINT16 */
	.long	Lst_sint16-Lstore_table		/* FFI_TYPE_SINT16 */
	.long	Lst_uint32-Lstore_table		/* FFI_TYPE_UINT32 */
	.long	Lst_sint32-Lstore_table		/* FFI_TYPE_SINT32 */
	.long	Lst_int64-Lstore_table		/* FFI_TYPE_UINT64 */
	.long	Lst_int64-Lstore_table		/* FFI_TYPE_SINT64 */
	.long	Lst_struct-Lstore_table		/* FFI_TYPE_STRUCT */
	.long	Lst_int64-Lstore_table		/* FFI_TYPE_POINTER */

	.text
	.align	3
Lst_void:
	ret
	.align	3
Lst_uint8:
	movzbq	%al, %rax
	movq	%rax, (%rdi)
	ret
	.align	3
Lst_sint8:
	movsbq	%al, %rax
	movq	%rax, (%rdi)
	ret
	.align	3
Lst_uint16:
	movzwq	%ax, %rax
	movq	%rax, (%rdi)
	.align	3
Lst_sint16:
	movswq	%ax, %rax
	movq	%rax, (%rdi)
	ret
	.align	3
Lst_uint32:
	movl	%eax, %eax
	movq	%rax, (%rdi)
	.align	3
Lst_sint32:
	cltq
	movq	%rax, (%rdi)
	ret
	.align	3
Lst_int64:
	movq	%rax, (%rdi)
	ret
	.align	3
Lst_float:
	movss	%xmm0, (%rdi)
	ret
	.align	3
Lst_double:
	movsd	%xmm0, (%rdi)
	ret
Lst_ldouble:
	fstpt	(%rdi)
	ret
	.align	3
Lst_struct:
	leaq	-20(%rsp), %rsi		/* Scratch area in redzone.  */

	/* We have to locate the values now, and since we don't want to
	   write too much data into the user's return value, we spill the
	   value to a 16 byte scratch area first.  Bits 8, 9, and 10
	   control where the values are located.  Only one of the three
	   bits will be set; see ffi_prep_cif_machdep for the pattern.  */
	movd	%xmm0, %r10
	movd	%xmm1, %r11
	testl	$0x100, %ecx
	cmovnz	%rax, %rdx
	cmovnz	%r10, %rax
	testl	$0x200, %ecx
	cmovnz	%r10, %rdx
	testl	$0x400, %ecx
	cmovnz	%r10, %rax
	cmovnz	%r11, %rdx
	movq	%rax, (%rsi)
	movq	%rdx, 8(%rsi)

	/* Bits 12-31 contain the true size of the structure.  Copy from
	   the scratch area to the true destination.  */
	shrl	$12, %ecx
	rep movsb
	ret

	/* Many times we can avoid loading any SSE registers at all.
	   It's not worth an indirect jump to load the exact set of
	   SSE registers needed; zero or all is a good compromise.  */
	.align	3
LUW3:
Lload_sse:
	movdqa	48(%r10), %xmm0
	movdqa	64(%r10), %xmm1
	movdqa	80(%r10), %xmm2
	movdqa	96(%r10), %xmm3
	movdqa	112(%r10), %xmm4
	movdqa	128(%r10), %xmm5
	movdqa	144(%r10), %xmm6
	movdqa	160(%r10), %xmm7
	jmp	Lret_from_load_sse

LUW4:
	.align	3
	.globl	_ffi_closure_unix64

_ffi_closure_unix64:
LUW5:
	/* The carry flag is set by the trampoline iff SSE registers
	   are used.  Don't clobber it before the branch instruction.  */
	leaq    -200(%rsp), %rsp
LUW6:
	movq	%rdi, (%rsp)
	movq    %rsi, 8(%rsp)
	movq    %rdx, 16(%rsp)
	movq    %rcx, 24(%rsp)
	movq    %r8, 32(%rsp)
	movq    %r9, 40(%rsp)
	jc      Lsave_sse
Lret_from_save_sse:

	movq	%r10, %rdi
	leaq	176(%rsp), %rsi
	movq	%rsp, %rdx
	leaq	208(%rsp), %rcx
	call	_ffi_closure_unix64_inner

	/* Deallocate stack frame early; return value is now in redzone.  */
	addq	$200, %rsp
LUW7:

	/* The first byte of the return value contains the FFI_TYPE.  */
	movzbl	%al, %r10d
	leaq	Lload_table(%rip), %r11
	movslq	(%r11, %r10, 4), %r10
	addq	%r11, %r10
	jmp	*%r10

Lload_table:
	.long	Lld_void-Lload_table		/* FFI_TYPE_VOID */
	.long	Lld_int32-Lload_table		/* FFI_TYPE_INT */
	.long	Lld_float-Lload_table		/* FFI_TYPE_FLOAT */
	.long	Lld_double-Lload_table		/* FFI_TYPE_DOUBLE */
	.long	Lld_ldouble-Lload_table		/* FFI_TYPE_LONGDOUBLE */
	.long	Lld_int8-Lload_table		/* FFI_TYPE_UINT8 */
	.long	Lld_int8-Lload_table		/* FFI_TYPE_SINT8 */
	.long	Lld_int16-Lload_table		/* FFI_TYPE_UINT16 */
	.long	Lld_int16-Lload_table		/* FFI_TYPE_SINT16 */
	.long	Lld_int32-Lload_table		/* FFI_TYPE_UINT32 */
	.long	Lld_int32-Lload_table		/* FFI_TYPE_SINT32 */
	.long	Lld_int64-Lload_table		/* FFI_TYPE_UINT64 */
	.long	Lld_int64-Lload_table		/* FFI_TYPE_SINT64 */
	.long	Lld_struct-Lload_table		/* FFI_TYPE_STRUCT */
	.long	Lld_int64-Lload_table		/* FFI_TYPE_POINTER */

	.text
	.align	3
Lld_void:
	ret
	.align	3
Lld_int8:
	movzbl	-24(%rsp), %eax
	ret
	.align	3
Lld_int16:
	movzwl	-24(%rsp), %eax
	ret
	.align	3
Lld_int32:
	movl	-24(%rsp), %eax
	ret
	.align	3
Lld_int64:
	movq	-24(%rsp), %rax
	ret
	.align	3
Lld_float:
	movss	-24(%rsp), %xmm0
	ret
	.align	3
Lld_double:
	movsd	-24(%rsp), %xmm0
	ret
	.align	3
Lld_ldouble:
	fldt	-24(%rsp)
	ret
	.align	3
Lld_struct:
	/* There are four possibilities here, %rax/%rdx, %xmm0/%rax,
	   %rax/%xmm0, %xmm0/%xmm1.  We collapse two by always loading
	   both rdx and xmm1 with the second word.  For the remaining,
	   bit 8 set means xmm0 gets the second word, and bit 9 means
	   that rax gets the second word.  */
	movq	-24(%rsp), %rcx
	movq	-16(%rsp), %rdx
	movq	-16(%rsp), %xmm1
	testl	$0x100, %eax
	cmovnz	%rdx, %rcx
	movd	%rcx, %xmm0
	testl	$0x200, %eax
	movq	-24(%rsp), %rax
	cmovnz	%rdx, %rax
	ret

	/* See the comment above Lload_sse; the same logic applies here.  */
	.align	3
LUW8:
Lsave_sse:
	movdqa	%xmm0, 48(%rsp)
	movdqa	%xmm1, 64(%rsp)
	movdqa	%xmm2, 80(%rsp)
	movdqa	%xmm3, 96(%rsp)
	movdqa	%xmm4, 112(%rsp)
	movdqa	%xmm5, 128(%rsp)
	movdqa	%xmm6, 144(%rsp)
	movdqa	%xmm7, 160(%rsp)
	jmp	Lret_from_save_sse

LUW9:
.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
	.set	L$set$0,LECIE1-LSCIE1		/* CIE Length */
	.long	L$set$0
LSCIE1:
	.long	0x0		/* CIE Identifier Tag */
	.byte	0x1		/* CIE Version */
	.ascii	"zR\0"		/* CIE Augmentation */
	.byte	0x1		/* uleb128 0x1; CIE Code Alignment Factor */
	.byte	0x78		/* sleb128 -8; CIE Data Alignment Factor */
	.byte	0x10		/* CIE RA Column */
	.byte	0x1		/* uleb128 0x1; Augmentation size */
	.byte	0x10		/* FDE Encoding (pcrel sdata4) */
	.byte	0xc		/* DW_CFA_def_cfa, %rsp offset 8 */
	.byte	0x7		/* uleb128 0x7 */
	.byte	0x8		/* uleb128 0x8 */
	.byte	0x90		/* DW_CFA_offset, column 0x10 */
	.byte	0x1
	.align	3
LECIE1:
	.globl _ffi_call_unix64.eh
_ffi_call_unix64.eh:
LSFDE1:
	.set	L$set$1,LEFDE1-LASFDE1	/* FDE Length */
	.long	L$set$1
LASFDE1:
	.long	LASFDE1-EH_frame1	/* FDE CIE offset */
	.quad	LUW0-.			/* FDE initial location */
	.set	L$set$2,LUW4-LUW0	/* FDE address range */
	.quad	L$set$2
	.byte	0x0			/* Augmentation size */
	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$3,LUW1-LUW0
	.long	L$set$3

	/* New stack frame based off rbp.  This is a itty bit of unwind
	   trickery in that the CFA *has* changed.  There is no easy way
	   to describe it correctly on entry to the function.  Fortunately,
	   it doesn't matter too much since at all points we can correctly
	   unwind back to ffi_call.  Note that the location to which we
	   moved the return address is (the new) CFA-8, so from the
	   perspective of the unwind info, it hasn't moved.  */
	.byte	0xc			/* DW_CFA_def_cfa, %rbp offset 32 */
	.byte	0x6
	.byte	0x20
	.byte	0x80+6			/* DW_CFA_offset, %rbp offset 2*-8 */
	.byte	0x2
	.byte	0xa			/* DW_CFA_remember_state */

	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$4,LUW2-LUW1
	.long	L$set$4
	.byte	0xc			/* DW_CFA_def_cfa, %rsp offset 8 */
	.byte	0x7
	.byte	0x8
	.byte	0xc0+6			/* DW_CFA_restore, %rbp */

	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$5,LUW3-LUW2
	.long	L$set$5
	.byte	0xb			/* DW_CFA_restore_state */

	.align	3
LEFDE1:
	.globl _ffi_closure_unix64.eh
_ffi_closure_unix64.eh:
LSFDE3:
	.set	L$set$6,LEFDE3-LASFDE3	/* FDE Length */
	.long	L$set$6
LASFDE3:
	.long	LASFDE3-EH_frame1	/* FDE CIE offset */
	.quad	LUW5-.			/* FDE initial location */
	.set	L$set$7,LUW9-LUW5	/* FDE address range */
	.quad	L$set$7
	.byte	0x0			/* Augmentation size */

	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$8,LUW6-LUW5
	.long	L$set$8
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	208,1			/* uleb128 208 */
	.byte	0xa			/* DW_CFA_remember_state */

	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$9,LUW7-LUW6
	.long	L$set$9
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	0x8

	.byte	0x4			/* DW_CFA_advance_loc4 */
	.set	L$set$10,LUW8-LUW7
	.long	L$set$10
	.byte	0xb			/* DW_CFA_restore_state */

	.align	3
LEFDE3:
	.subsections_via_symbols

#endif /* __x86_64__ */
