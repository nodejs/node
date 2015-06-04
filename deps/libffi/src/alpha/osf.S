/* -----------------------------------------------------------------------
   osf.S - Copyright (c) 1998, 2001, 2007, 2008, 2011 Red Hat
   
   Alpha/OSF Foreign Function Interface 

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

	.arch ev6
	.text

/* ffi_call_osf (void *args, unsigned long bytes, unsigned flags,
		 void *raddr, void (*fnaddr)(void));

   Bit o trickiness here -- ARGS+BYTES is the base of the stack frame
   for this function.  This has been allocated by ffi_call.  We also
   deallocate some of the stack that has been alloca'd.  */

	.align	3
	.globl	ffi_call_osf
	.ent	ffi_call_osf
	FFI_HIDDEN(ffi_call_osf)

ffi_call_osf:
	.frame	$15, 32, $26, 0
	.mask   0x4008000, -32
$LFB1:
	addq	$16,$17,$1
	mov	$16, $30
	stq	$26, 0($1)
	stq	$15, 8($1)
	stq	$18, 16($1)
	mov	$1, $15
$LCFI1:
	.prologue 0

	stq	$19, 24($1)
	mov	$20, $27

	# Load up all of the (potential) argument registers.
	ldq	$16, 0($30)
	ldt	$f16, 0($30)
	ldt	$f17, 8($30)
	ldq	$17, 8($30)
	ldt	$f18, 16($30)
	ldq	$18, 16($30)
	ldt	$f19, 24($30)
	ldq	$19, 24($30)
	ldt	$f20, 32($30)
	ldq	$20, 32($30)
	ldt	$f21, 40($30)
	ldq	$21, 40($30)

	# Deallocate the register argument area.
	lda	$30, 48($30)

	jsr	$26, ($27), 0
	ldgp	$29, 0($26)

	# If the return value pointer is NULL, assume no return value.
	ldq	$19, 24($15)
	ldq	$18, 16($15)
	ldq	$26, 0($15)
$LCFI2:
	beq	$19, $noretval

	# Store the return value out in the proper type.
	cmpeq	$18, FFI_TYPE_INT, $1
	bne	$1, $retint
	cmpeq	$18, FFI_TYPE_FLOAT, $2
	bne	$2, $retfloat
	cmpeq	$18, FFI_TYPE_DOUBLE, $3
	bne	$3, $retdouble

	.align	3
$noretval:
	ldq	$15, 8($15)
	ret

	.align	4
$retint:
	stq	$0, 0($19)
	nop
	ldq	$15, 8($15)
	ret

	.align	4
$retfloat:
	sts	$f0, 0($19)
	nop
	ldq	$15, 8($15)
	ret

	.align	4
$retdouble:
	stt	$f0, 0($19)
	nop
	ldq	$15, 8($15)
	ret
$LFE1:

	.end	ffi_call_osf

/* ffi_closure_osf(...)

   Receives the closure argument in $1.   */

	.align	3
	.globl	ffi_closure_osf
	.ent	ffi_closure_osf
	FFI_HIDDEN(ffi_closure_osf)

ffi_closure_osf:
	.frame	$30, 16*8, $26, 0
	.mask	0x4000000, -16*8
$LFB2:
	ldgp	$29, 0($27)
	subq	$30, 16*8, $30
$LCFI5:
	stq	$26, 0($30)
$LCFI6:
	.prologue 1

	# Store all of the potential argument registers in va_list format.
	stt	$f16, 4*8($30)
	stt	$f17, 5*8($30)
	stt	$f18, 6*8($30)
	stt	$f19, 7*8($30)
	stt	$f20, 8*8($30)
	stt	$f21, 9*8($30)
	stq	$16, 10*8($30)
	stq	$17, 11*8($30)
	stq	$18, 12*8($30)
	stq	$19, 13*8($30)
	stq	$20, 14*8($30)
	stq	$21, 15*8($30)

	# Call ffi_closure_osf_inner to do the bulk of the work.
	mov	$1, $16
	lda	$17, 2*8($30)
	lda	$18, 10*8($30)
	jsr	$26, ffi_closure_osf_inner
	ldgp	$29, 0($26)
	ldq	$26, 0($30)

	# Load up the return value in the proper type.
	lda	$1, $load_table
	s4addq	$0, $1, $1
	ldl	$1, 0($1)
	addq	$1, $29, $1
	jmp	$31, ($1), $load_32

	.align 4
$load_none:
	addq	$30, 16*8, $30
	ret

	.align 4
$load_float:
	lds	$f0, 16($30)
	nop
	addq	$30, 16*8, $30
	ret

	.align 4
$load_double:
	ldt	$f0, 16($30)
	nop
	addq	$30, 16*8, $30
	ret

	.align 4
$load_u8:
#ifdef __alpha_bwx__
	ldbu	$0, 16($30)
	nop
#else
	ldq	$0, 16($30)
	and	$0, 255, $0
#endif
	addq	$30, 16*8, $30
	ret

	.align 4
$load_s8:
#ifdef __alpha_bwx__
	ldbu	$0, 16($30)
	sextb	$0, $0
#else
	ldq	$0, 16($30)
	sll	$0, 56, $0
	sra	$0, 56, $0
#endif
	addq	$30, 16*8, $30
	ret

	.align 4
$load_u16:
#ifdef __alpha_bwx__
	ldwu	$0, 16($30)
	nop
#else
	ldq	$0, 16($30)
	zapnot	$0, 3, $0
#endif
	addq	$30, 16*8, $30
	ret

	.align 4
$load_s16:
#ifdef __alpha_bwx__
	ldwu	$0, 16($30)
	sextw	$0, $0
#else
	ldq	$0, 16($30)
	sll	$0, 48, $0
	sra	$0, 48, $0
#endif
	addq	$30, 16*8, $30
	ret

	.align 4
$load_32:
	ldl	$0, 16($30)
	nop
	addq	$30, 16*8, $30
	ret

	.align 4
$load_64:
	ldq	$0, 16($30)
	nop
	addq	$30, 16*8, $30
	ret
$LFE2:

	.end	ffi_closure_osf

#ifdef __ELF__
.section .rodata
#else
.rdata
#endif
$load_table:
	.gprel32 $load_none	# FFI_TYPE_VOID
	.gprel32 $load_32	# FFI_TYPE_INT
	.gprel32 $load_float	# FFI_TYPE_FLOAT
	.gprel32 $load_double	# FFI_TYPE_DOUBLE
	.gprel32 $load_none	# FFI_TYPE_LONGDOUBLE
	.gprel32 $load_u8	# FFI_TYPE_UINT8
	.gprel32 $load_s8	# FFI_TYPE_SINT8
	.gprel32 $load_u16	# FFI_TYPE_UINT16
	.gprel32 $load_s16	# FFI_TYPE_SINT16
	.gprel32 $load_32	# FFI_TYPE_UINT32
	.gprel32 $load_32	# FFI_TYPE_SINT32
	.gprel32 $load_64	# FFI_TYPE_UINT64
	.gprel32 $load_64	# FFI_TYPE_SINT64
	.gprel32 $load_none	# FFI_TYPE_STRUCT
	.gprel32 $load_64	# FFI_TYPE_POINTER

/* Assert that the table above is in sync with ffi.h.  */

#if	   FFI_TYPE_FLOAT != 2		\
	|| FFI_TYPE_DOUBLE != 3		\
	|| FFI_TYPE_UINT8 != 5		\
	|| FFI_TYPE_SINT8 != 6		\
	|| FFI_TYPE_UINT16 != 7		\
	|| FFI_TYPE_SINT16 != 8		\
	|| FFI_TYPE_UINT32 != 9		\
	|| FFI_TYPE_SINT32 != 10	\
	|| FFI_TYPE_UINT64 != 11	\
	|| FFI_TYPE_SINT64 != 12	\
	|| FFI_TYPE_STRUCT != 13	\
	|| FFI_TYPE_POINTER != 14	\
	|| FFI_TYPE_LAST != 14
#error "osf.S out of sync with ffi.h"
#endif

#ifdef __ELF__
# define UA_SI		.4byte
# define FDE_ENCODING	0x1b	/* pcrel sdata4 */
# define FDE_ENCODE(X)	.4byte X-.
# define FDE_ARANGE(X)	.4byte X
#elif defined __osf__
# define UA_SI		.align 0; .long
# define FDE_ENCODING	0x50	/* aligned absolute */
# define FDE_ENCODE(X)	.align 3; .quad X
# define FDE_ARANGE(X)	.align 0; .quad X
#endif

#ifdef __ELF__
	.section	.eh_frame,EH_FRAME_FLAGS,@progbits
#elif defined __osf__
	.data
	.align 3
	.globl _GLOBAL__F_ffi_call_osf
_GLOBAL__F_ffi_call_osf:
#endif
__FRAME_BEGIN__:
	UA_SI	$LECIE1-$LSCIE1	# Length of Common Information Entry
$LSCIE1:
	UA_SI	0x0		# CIE Identifier Tag
	.byte	0x1		# CIE Version
	.ascii "zR\0"		# CIE Augmentation
	.byte	0x1		# uleb128 0x1; CIE Code Alignment Factor
	.byte	0x78		# sleb128 -8; CIE Data Alignment Factor
	.byte	26		# CIE RA Column
	.byte	0x1		# uleb128 0x1; Augmentation size
	.byte	FDE_ENCODING	# FDE Encoding
	.byte	0xc		# DW_CFA_def_cfa
	.byte	30		# uleb128 column 30
	.byte	0		# uleb128 offset 0
	.align 3
$LECIE1:
$LSFDE1:
	UA_SI	$LEFDE1-$LASFDE1		# FDE Length
$LASFDE1:
	UA_SI	$LASFDE1-__FRAME_BEGIN__	# FDE CIE offset
	FDE_ENCODE($LFB1)			# FDE initial location
	FDE_ARANGE($LFE1-$LFB1)			# FDE address range
	.byte	0x0		# uleb128 0x0; Augmentation size

	.byte	0x4		# DW_CFA_advance_loc4
	UA_SI	$LCFI1-$LFB1
	.byte	0x9a		# DW_CFA_offset, column 26
	.byte	4		# uleb128 4*-8
	.byte	0x8f		# DW_CFA_offset, column 15
	.byte	0x3		# uleb128 3*-8
	.byte	0xc		# DW_CFA_def_cfa
	.byte	15		# uleb128 column 15
	.byte	32		# uleb128 offset 32

	.byte	0x4		# DW_CFA_advance_loc4
	UA_SI	$LCFI2-$LCFI1
	.byte	0xda		# DW_CFA_restore, column 26
	.align 3
$LEFDE1:

$LSFDE3:
	UA_SI	$LEFDE3-$LASFDE3		# FDE Length
$LASFDE3:
	UA_SI	$LASFDE3-__FRAME_BEGIN__	# FDE CIE offset
	FDE_ENCODE($LFB2)			# FDE initial location
	FDE_ARANGE($LFE2-$LFB2)			# FDE address range
	.byte	0x0		# uleb128 0x0; Augmentation size

	.byte	0x4		# DW_CFA_advance_loc4
	UA_SI	$LCFI5-$LFB2
	.byte	0xe		# DW_CFA_def_cfa_offset
	.byte	0x80,0x1	# uleb128 128

	.byte	0x4		# DW_CFA_advance_loc4
	UA_SI	$LCFI6-$LCFI5
	.byte	0x9a		# DW_CFA_offset, column 26
	.byte	16		# uleb128 offset 16*-8
	.align 3
$LEFDE3:
#if defined __osf__
	.align 0
	.long	0		# End of Table
#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif
