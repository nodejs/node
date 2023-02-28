/* -----------------------------------------------------------------------
   unix64.S - Copyright (c) 2013  The Written Word, Inc.
	    - Copyright (c) 2008  Red Hat, Inc
	    - Copyright (c) 2002  Bo Thorsen <bo@suse.de>

   x86-64 Foreign Function Interface 

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

#ifdef __x86_64__
#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>
#include "internal64.h"
#include "asmnames.h"

	.text

/* This macro allows the safe creation of jump tables without an
   actual table.  The entry points into the table are all 8 bytes.
   The use of ORG asserts that we're at the correct location.  */
/* ??? The clang assembler doesn't handle .org with symbolic expressions.  */
#if defined(__clang__) || defined(__APPLE__) || (defined (__sun__) && defined(__svr4__))
# define E(BASE, X)	.balign 8
#else
# define E(BASE, X)	.balign 8; .org BASE + X * 8
#endif

/* ffi_call_unix64 (void *args, unsigned long bytes, unsigned flags,
	            void *raddr, void (*fnaddr)(void));

   Bit o trickiness here -- ARGS+BYTES is the base of the stack frame
   for this function.  This has been allocated by ffi_call.  We also
   deallocate some of the stack that has been alloca'd.  */

	.balign	8
	.globl	C(ffi_call_unix64)
	FFI_HIDDEN(C(ffi_call_unix64))

C(ffi_call_unix64):
L(UW0):
	movq	(%rsp), %r10		/* Load return address.  */
	leaq	(%rdi, %rsi), %rax	/* Find local stack base.  */
	movq	%rdx, (%rax)		/* Save flags.  */
	movq	%rcx, 8(%rax)		/* Save raddr.  */
	movq	%rbp, 16(%rax)		/* Save old frame pointer.  */
	movq	%r10, 24(%rax)		/* Relocate return address.  */
	movq	%rax, %rbp		/* Finalize local stack frame.  */

	/* New stack frame based off rbp.  This is a itty bit of unwind
	   trickery in that the CFA *has* changed.  There is no easy way
	   to describe it correctly on entry to the function.  Fortunately,
	   it doesn't matter too much since at all points we can correctly
	   unwind back to ffi_call.  Note that the location to which we
	   moved the return address is (the new) CFA-8, so from the
	   perspective of the unwind info, it hasn't moved.  */
L(UW1):
	/* cfi_def_cfa(%rbp, 32) */
	/* cfi_rel_offset(%rbp, 16) */

	movq	%rdi, %r10		/* Save a copy of the register area. */
	movq	%r8, %r11		/* Save a copy of the target fn.  */
	movl	%r9d, %eax		/* Set number of SSE registers.  */

	/* Load up all argument registers.  */
	movq	(%r10), %rdi
	movq	0x08(%r10), %rsi
	movq	0x10(%r10), %rdx
	movq	0x18(%r10), %rcx
	movq	0x20(%r10), %r8
	movq	0x28(%r10), %r9
	movl	0xb0(%r10), %eax
	testl	%eax, %eax
	jnz	L(load_sse)
L(ret_from_load_sse):

	/* Deallocate the reg arg area, except for r10, then load via pop.  */
	leaq	0xb8(%r10), %rsp
	popq	%r10

	/* Call the user function.  */
	call	*%r11

	/* Deallocate stack arg area; local stack frame in redzone.  */
	leaq	24(%rbp), %rsp

	movq	0(%rbp), %rcx		/* Reload flags.  */
	movq	8(%rbp), %rdi		/* Reload raddr.  */
	movq	16(%rbp), %rbp		/* Reload old frame pointer.  */
L(UW2):
	/* cfi_remember_state */
	/* cfi_def_cfa(%rsp, 8) */
	/* cfi_restore(%rbp) */

	/* The first byte of the flags contains the FFI_TYPE.  */
	cmpb	$UNIX64_RET_LAST, %cl
	movzbl	%cl, %r10d
	leaq	L(store_table)(%rip), %r11
	ja	L(sa)
	leaq	(%r11, %r10, 8), %r10

	/* Prep for the structure cases: scratch area in redzone.  */
	leaq	-20(%rsp), %rsi
	jmp	*%r10

	.balign	8
L(store_table):
E(L(store_table), UNIX64_RET_VOID)
	ret
E(L(store_table), UNIX64_RET_UINT8)
	movzbl	%al, %eax
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_UINT16)
	movzwl	%ax, %eax
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_UINT32)
	movl	%eax, %eax
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_SINT8)
	movsbq	%al, %rax
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_SINT16)
	movswq	%ax, %rax
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_SINT32)
	cltq
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_INT64)
	movq	%rax, (%rdi)
	ret
E(L(store_table), UNIX64_RET_XMM32)
	movd	%xmm0, (%rdi)
	ret
E(L(store_table), UNIX64_RET_XMM64)
	movq	%xmm0, (%rdi)
	ret
E(L(store_table), UNIX64_RET_X87)
	fstpt	(%rdi)
	ret
E(L(store_table), UNIX64_RET_X87_2)
	fstpt	(%rdi)
	fstpt	16(%rdi)
	ret
E(L(store_table), UNIX64_RET_ST_XMM0_RAX)
	movq	%rax, 8(%rsi)
	jmp	L(s3)
E(L(store_table), UNIX64_RET_ST_RAX_XMM0)
	movq	%xmm0, 8(%rsi)
	jmp	L(s2)
E(L(store_table), UNIX64_RET_ST_XMM0_XMM1)
	movq	%xmm1, 8(%rsi)
	jmp	L(s3)
E(L(store_table), UNIX64_RET_ST_RAX_RDX)
	movq	%rdx, 8(%rsi)
L(s2):
	movq	%rax, (%rsi)
	shrl	$UNIX64_SIZE_SHIFT, %ecx
	rep movsb
	ret
	.balign 8
L(s3):
	movq	%xmm0, (%rsi)
	shrl	$UNIX64_SIZE_SHIFT, %ecx
	rep movsb
	ret

L(sa):	call	PLT(C(abort))

	/* Many times we can avoid loading any SSE registers at all.
	   It's not worth an indirect jump to load the exact set of
	   SSE registers needed; zero or all is a good compromise.  */
	.balign 2
L(UW3):
	/* cfi_restore_state */
L(load_sse):
	movdqa	0x30(%r10), %xmm0
	movdqa	0x40(%r10), %xmm1
	movdqa	0x50(%r10), %xmm2
	movdqa	0x60(%r10), %xmm3
	movdqa	0x70(%r10), %xmm4
	movdqa	0x80(%r10), %xmm5
	movdqa	0x90(%r10), %xmm6
	movdqa	0xa0(%r10), %xmm7
	jmp	L(ret_from_load_sse)

L(UW4):
ENDF(C(ffi_call_unix64))

/* 6 general registers, 8 vector registers,
   32 bytes of rvalue, 8 bytes of alignment.  */
#define ffi_closure_OFS_G	0
#define ffi_closure_OFS_V	(6*8)
#define ffi_closure_OFS_RVALUE	(ffi_closure_OFS_V + 8*16)
#define ffi_closure_FS		(ffi_closure_OFS_RVALUE + 32 + 8)

/* The location of rvalue within the red zone after deallocating the frame.  */
#define ffi_closure_RED_RVALUE	(ffi_closure_OFS_RVALUE - ffi_closure_FS)

	.balign	2
	.globl	C(ffi_closure_unix64_sse)
	FFI_HIDDEN(C(ffi_closure_unix64_sse))

C(ffi_closure_unix64_sse):
L(UW5):
	subq	$ffi_closure_FS, %rsp
L(UW6):
	/* cfi_adjust_cfa_offset(ffi_closure_FS) */

	movdqa	%xmm0, ffi_closure_OFS_V+0x00(%rsp)
	movdqa	%xmm1, ffi_closure_OFS_V+0x10(%rsp)
	movdqa	%xmm2, ffi_closure_OFS_V+0x20(%rsp)
	movdqa	%xmm3, ffi_closure_OFS_V+0x30(%rsp)
	movdqa	%xmm4, ffi_closure_OFS_V+0x40(%rsp)
	movdqa	%xmm5, ffi_closure_OFS_V+0x50(%rsp)
	movdqa	%xmm6, ffi_closure_OFS_V+0x60(%rsp)
	movdqa	%xmm7, ffi_closure_OFS_V+0x70(%rsp)
	jmp	L(sse_entry1)

L(UW7):
ENDF(C(ffi_closure_unix64_sse))

	.balign	2
	.globl	C(ffi_closure_unix64)
	FFI_HIDDEN(C(ffi_closure_unix64))

C(ffi_closure_unix64):
L(UW8):
	subq	$ffi_closure_FS, %rsp
L(UW9):
	/* cfi_adjust_cfa_offset(ffi_closure_FS) */
L(sse_entry1):
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

#ifdef __ILP32__
	movl	FFI_TRAMPOLINE_SIZE(%r10), %edi		/* Load cif */
	movl	FFI_TRAMPOLINE_SIZE+4(%r10), %esi	/* Load fun */
	movl	FFI_TRAMPOLINE_SIZE+8(%r10), %edx	/* Load user_data */
#else
	movq	FFI_TRAMPOLINE_SIZE(%r10), %rdi		/* Load cif */
	movq	FFI_TRAMPOLINE_SIZE+8(%r10), %rsi	/* Load fun */
	movq	FFI_TRAMPOLINE_SIZE+16(%r10), %rdx	/* Load user_data */
#endif
L(do_closure):
	leaq	ffi_closure_OFS_RVALUE(%rsp), %rcx	/* Load rvalue */
	movq	%rsp, %r8				/* Load reg_args */
	leaq	ffi_closure_FS+8(%rsp), %r9		/* Load argp */
	call	PLT(C(ffi_closure_unix64_inner))

	/* Deallocate stack frame early; return value is now in redzone.  */
	addq	$ffi_closure_FS, %rsp
L(UW10):
	/* cfi_adjust_cfa_offset(-ffi_closure_FS) */

	/* The first byte of the return value contains the FFI_TYPE.  */
	cmpb	$UNIX64_RET_LAST, %al
	movzbl	%al, %r10d
	leaq	L(load_table)(%rip), %r11
	ja	L(la)
	leaq	(%r11, %r10, 8), %r10
	leaq	ffi_closure_RED_RVALUE(%rsp), %rsi
	jmp	*%r10

	.balign	8
L(load_table):
E(L(load_table), UNIX64_RET_VOID)
	ret
E(L(load_table), UNIX64_RET_UINT8)
	movzbl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_UINT16)
	movzwl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_UINT32)
	movl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_SINT8)
	movsbl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_SINT16)
	movswl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_SINT32)
	movl	(%rsi), %eax
	ret
E(L(load_table), UNIX64_RET_INT64)
	movq	(%rsi), %rax
	ret
E(L(load_table), UNIX64_RET_XMM32)
	movd	(%rsi), %xmm0
	ret
E(L(load_table), UNIX64_RET_XMM64)
	movq	(%rsi), %xmm0
	ret
E(L(load_table), UNIX64_RET_X87)
	fldt	(%rsi)
	ret
E(L(load_table), UNIX64_RET_X87_2)
	fldt	16(%rsi)
	fldt	(%rsi)
	ret
E(L(load_table), UNIX64_RET_ST_XMM0_RAX)
	movq	8(%rsi), %rax
	jmp	L(l3)
E(L(load_table), UNIX64_RET_ST_RAX_XMM0)
	movq	8(%rsi), %xmm0
	jmp	L(l2)
E(L(load_table), UNIX64_RET_ST_XMM0_XMM1)
	movq	8(%rsi), %xmm1
	jmp	L(l3)
E(L(load_table), UNIX64_RET_ST_RAX_RDX)
	movq	8(%rsi), %rdx
L(l2):
	movq	(%rsi), %rax
	ret
	.balign	8
L(l3):
	movq	(%rsi), %xmm0
	ret

L(la):	call	PLT(C(abort))

L(UW11):
ENDF(C(ffi_closure_unix64))

	.balign	2
	.globl	C(ffi_go_closure_unix64_sse)
	FFI_HIDDEN(C(ffi_go_closure_unix64_sse))

C(ffi_go_closure_unix64_sse):
L(UW12):
	subq	$ffi_closure_FS, %rsp
L(UW13):
	/* cfi_adjust_cfa_offset(ffi_closure_FS) */

	movdqa	%xmm0, ffi_closure_OFS_V+0x00(%rsp)
	movdqa	%xmm1, ffi_closure_OFS_V+0x10(%rsp)
	movdqa	%xmm2, ffi_closure_OFS_V+0x20(%rsp)
	movdqa	%xmm3, ffi_closure_OFS_V+0x30(%rsp)
	movdqa	%xmm4, ffi_closure_OFS_V+0x40(%rsp)
	movdqa	%xmm5, ffi_closure_OFS_V+0x50(%rsp)
	movdqa	%xmm6, ffi_closure_OFS_V+0x60(%rsp)
	movdqa	%xmm7, ffi_closure_OFS_V+0x70(%rsp)
	jmp	L(sse_entry2)

L(UW14):
ENDF(C(ffi_go_closure_unix64_sse))

	.balign	2
	.globl	C(ffi_go_closure_unix64)
	FFI_HIDDEN(C(ffi_go_closure_unix64))

C(ffi_go_closure_unix64):
L(UW15):
	subq	$ffi_closure_FS, %rsp
L(UW16):
	/* cfi_adjust_cfa_offset(ffi_closure_FS) */
L(sse_entry2):
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

#ifdef __ILP32__
	movl	4(%r10), %edi		/* Load cif */
	movl	8(%r10), %esi		/* Load fun */
	movl	%r10d, %edx		/* Load closure (user_data) */
#else
	movq	8(%r10), %rdi		/* Load cif */
	movq	16(%r10), %rsi		/* Load fun */
	movq	%r10, %rdx		/* Load closure (user_data) */
#endif
	jmp	L(do_closure)

L(UW17):
ENDF(C(ffi_go_closure_unix64))

/* Sadly, OSX cctools-as doesn't understand .cfi directives at all.  */

#ifdef __APPLE__
.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EHFrame0:
#elif defined(HAVE_AS_X86_64_UNWIND_SECTION_TYPE)
.section .eh_frame,"a",@unwind
#else
.section .eh_frame,"a",@progbits
#endif

#ifdef HAVE_AS_X86_PCREL
# define PCREL(X)	X - .
#else
# define PCREL(X)	X@rel
#endif

/* Simplify advancing between labels.  Assume DW_CFA_advance_loc1 fits.  */
#define ADV(N, P)	.byte 2, L(N)-L(P)

	.balign 8
L(CIE):
	.set	L(set0),L(ECIE)-L(SCIE)
	.long	L(set0)			/* CIE Length */
L(SCIE):
	.long	0			/* CIE Identifier Tag */
	.byte	1			/* CIE Version */
	.ascii	"zR\0"			/* CIE Augmentation */
	.byte	1			/* CIE Code Alignment Factor */
	.byte	0x78			/* CIE Data Alignment Factor */
	.byte	0x10			/* CIE RA Column */
	.byte	1			/* Augmentation size */
	.byte	0x1b			/* FDE Encoding (pcrel sdata4) */
	.byte	0xc, 7, 8		/* DW_CFA_def_cfa, %rsp offset 8 */
	.byte	0x80+16, 1		/* DW_CFA_offset, %rip offset 1*-8 */
	.balign 8
L(ECIE):

	.set	L(set1),L(EFDE1)-L(SFDE1)
	.long	L(set1)			/* FDE Length */
L(SFDE1):
	.long	L(SFDE1)-L(CIE)		/* FDE CIE offset */
	.long	PCREL(L(UW0))		/* Initial location */
	.long	L(UW4)-L(UW0)		/* Address range */
	.byte	0			/* Augmentation size */
	ADV(UW1, UW0)
	.byte	0xc, 6, 32		/* DW_CFA_def_cfa, %rbp 32 */
	.byte	0x80+6, 2		/* DW_CFA_offset, %rbp 2*-8 */
	ADV(UW2, UW1)
	.byte	0xa			/* DW_CFA_remember_state */
	.byte	0xc, 7, 8		/* DW_CFA_def_cfa, %rsp 8 */
	.byte	0xc0+6			/* DW_CFA_restore, %rbp */
	ADV(UW3, UW2)
	.byte	0xb			/* DW_CFA_restore_state */
	.balign	8
L(EFDE1):

	.set	L(set2),L(EFDE2)-L(SFDE2)
	.long	L(set2)			/* FDE Length */
L(SFDE2):
	.long	L(SFDE2)-L(CIE)		/* FDE CIE offset */
	.long	PCREL(L(UW5))		/* Initial location */
	.long	L(UW7)-L(UW5)		/* Address range */
	.byte	0			/* Augmentation size */
	ADV(UW6, UW5)
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	ffi_closure_FS + 8, 1	/* uleb128, assuming 128 <= FS < 255 */
	.balign	8
L(EFDE2):

	.set	L(set3),L(EFDE3)-L(SFDE3)
	.long	L(set3)			/* FDE Length */
L(SFDE3):
	.long	L(SFDE3)-L(CIE)		/* FDE CIE offset */
	.long	PCREL(L(UW8))		/* Initial location */
	.long	L(UW11)-L(UW8)		/* Address range */
	.byte	0			/* Augmentation size */
	ADV(UW9, UW8)
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	ffi_closure_FS + 8, 1	/* uleb128, assuming 128 <= FS < 255 */
	ADV(UW10, UW9)
	.byte	0xe, 8			/* DW_CFA_def_cfa_offset 8 */
L(EFDE3):

	.set	L(set4),L(EFDE4)-L(SFDE4)
	.long	L(set4)			/* FDE Length */
L(SFDE4):
	.long	L(SFDE4)-L(CIE)		/* FDE CIE offset */
	.long	PCREL(L(UW12))		/* Initial location */
	.long	L(UW14)-L(UW12)		/* Address range */
	.byte	0			/* Augmentation size */
	ADV(UW13, UW12)
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	ffi_closure_FS + 8, 1	/* uleb128, assuming 128 <= FS < 255 */
	.balign	8
L(EFDE4):

	.set	L(set5),L(EFDE5)-L(SFDE5)
	.long	L(set5)			/* FDE Length */
L(SFDE5):
	.long	L(SFDE5)-L(CIE)		/* FDE CIE offset */
	.long	PCREL(L(UW15))		/* Initial location */
	.long	L(UW17)-L(UW15)		/* Address range */
	.byte	0			/* Augmentation size */
	ADV(UW16, UW15)
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.byte	ffi_closure_FS + 8, 1	/* uleb128, assuming 128 <= FS < 255 */
	.balign	8
L(EFDE5):
#ifdef __APPLE__
	.subsections_via_symbols
	.section __LD,__compact_unwind,regular,debug

	/* compact unwind for ffi_call_unix64 */
	.quad    C(ffi_call_unix64)
	.set     L1,L(UW4)-L(UW0)
	.long    L1
	.long    0x04000000 /* use dwarf unwind info */
	.quad    0
	.quad    0

	/* compact unwind for ffi_closure_unix64_sse */
	.quad    C(ffi_closure_unix64_sse)
	.set     L2,L(UW7)-L(UW5)
	.long    L2
	.long    0x04000000 /* use dwarf unwind info */
	.quad    0
	.quad    0

	/* compact unwind for ffi_closure_unix64 */
	.quad    C(ffi_closure_unix64)
	.set     L3,L(UW11)-L(UW8)
	.long    L3
	.long    0x04000000 /* use dwarf unwind info */
	.quad    0
	.quad    0

	/* compact unwind for ffi_go_closure_unix64_sse */
	.quad    C(ffi_go_closure_unix64_sse)
	.set     L4,L(UW14)-L(UW12)
	.long    L4
	.long    0x04000000 /* use dwarf unwind info */
	.quad    0
	.quad    0

	/* compact unwind for ffi_go_closure_unix64 */
	.quad    C(ffi_go_closure_unix64)
	.set     L5,L(UW17)-L(UW15)
	.long    L5
	.long    0x04000000 /* use dwarf unwind info */
	.quad    0
	.quad    0
#endif

#endif /* __x86_64__ */
#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
