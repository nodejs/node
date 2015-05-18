/* -----------------------------------------------------------------------
   darwin.S - Copyright (c) 1996, 1998, 2001, 2002, 2003, 2005  Red Hat, Inc.
	Copyright (C) 2008  Free Software Foundation, Inc.

   X86 Foreign Function Interface

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
   -----------------------------------------------------------------------
   */

#ifndef __x86_64__

#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>

.text

.globl _ffi_prep_args

	.align 4
.globl _ffi_call_SYSV

_ffi_call_SYSV:
.LFB1:
        pushl %ebp
.LCFI0:
        movl  %esp,%ebp
.LCFI1:
        subl $8,%esp
	/* Make room for all of the new args.  */
	movl  16(%ebp),%ecx
	subl  %ecx,%esp

	movl  %esp,%eax

	/* Place all of the ffi_prep_args in position  */
	subl  $8,%esp
	pushl 12(%ebp)
	pushl %eax
	call  *8(%ebp)

	/* Return stack to previous state and call the function  */
	addl  $16,%esp	

	call  *28(%ebp)

	/* Load %ecx with the return type code  */
	movl  20(%ebp),%ecx	

	/* Protect %esi.  We're going to pop it in the epilogue.  */
	pushl %esi

	/* If the return value pointer is NULL, assume no return value.  */
	cmpl  $0,24(%ebp)
	jne  0f

	/* Even if there is no space for the return value, we are 
	   obliged to handle floating-point values.  */
	cmpl  $FFI_TYPE_FLOAT,%ecx
	jne   noretval
	fstp  %st(0)

	jmp   epilogue
0:
	.align 4
	call 1f
.Lstore_table:
	.long   noretval-.Lstore_table		/* FFI_TYPE_VOID */
	.long   retint-.Lstore_table		/* FFI_TYPE_INT */
	.long   retfloat-.Lstore_table		/* FFI_TYPE_FLOAT */
	.long   retdouble-.Lstore_table		/* FFI_TYPE_DOUBLE */
	.long   retlongdouble-.Lstore_table     /* FFI_TYPE_LONGDOUBLE */
	.long   retuint8-.Lstore_table		/* FFI_TYPE_UINT8 */
	.long   retsint8-.Lstore_table		/* FFI_TYPE_SINT8 */
	.long   retuint16-.Lstore_table		/* FFI_TYPE_UINT16 */
	.long   retsint16-.Lstore_table		/* FFI_TYPE_SINT16 */
	.long   retint-.Lstore_table		/* FFI_TYPE_UINT32 */
	.long   retint-.Lstore_table		/* FFI_TYPE_SINT32 */
	.long   retint64-.Lstore_table		/* FFI_TYPE_UINT64 */
	.long   retint64-.Lstore_table		/* FFI_TYPE_SINT64 */
	.long   retstruct-.Lstore_table		/* FFI_TYPE_STRUCT */
	.long   retint-.Lstore_table		/* FFI_TYPE_POINTER */
	.long   retstruct1b-.Lstore_table	/* FFI_TYPE_SMALL_STRUCT_1B */
	.long   retstruct2b-.Lstore_table	/* FFI_TYPE_SMALL_STRUCT_2B */
1:
	pop  %esi
	add  (%esi, %ecx, 4), %esi
	jmp  *%esi

	/* Sign/zero extend as appropriate.  */
retsint8:
	movsbl  %al, %eax
	jmp  retint

retsint16:
	movswl  %ax, %eax
	jmp  retint

retuint8:
	movzbl  %al, %eax
	jmp  retint

retuint16:
	movzwl  %ax, %eax
	jmp  retint

retfloat:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	fstps (%ecx)
	jmp   epilogue

retdouble:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	fstpl (%ecx)
	jmp   epilogue

retlongdouble:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	fstpt (%ecx)
	jmp   epilogue

retint64:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	movl  %eax,0(%ecx)
	movl  %edx,4(%ecx)
	jmp   epilogue

retstruct1b:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	movb  %al,0(%ecx)
	jmp   epilogue

retstruct2b:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	movw  %ax,0(%ecx)
	jmp   epilogue

retint:
	/* Load %ecx with the pointer to storage for the return value  */
	movl  24(%ebp),%ecx
	movl  %eax,0(%ecx)

retstruct:
	/* Nothing to do!  */

noretval:
epilogue:
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret

.LFE1:
.ffi_call_SYSV_end:

	.align	4
FFI_HIDDEN (ffi_closure_SYSV)
.globl _ffi_closure_SYSV

_ffi_closure_SYSV:
.LFB2:
	pushl	%ebp
.LCFI2:
	movl	%esp, %ebp
.LCFI3:
	subl	$40, %esp
	leal	-24(%ebp), %edx
	movl	%edx, -12(%ebp)	/* resp */
	leal	8(%ebp), %edx
	movl	%edx, 4(%esp)	/* args = __builtin_dwarf_cfa () */
	leal	-12(%ebp), %edx
	movl	%edx, (%esp)	/* &resp */
	movl	%ebx, 8(%esp)
.LCFI7:
	call	L_ffi_closure_SYSV_inner$stub
	movl	8(%esp), %ebx
	movl	-12(%ebp), %ecx
	cmpl	$FFI_TYPE_INT, %eax
	je	.Lcls_retint

	/* Handle FFI_TYPE_UINT8, FFI_TYPE_SINT8, FFI_TYPE_UINT16,
	   FFI_TYPE_SINT16, FFI_TYPE_UINT32, FFI_TYPE_SINT32.  */
	cmpl	$FFI_TYPE_UINT64, %eax
	jge	0f
	cmpl	$FFI_TYPE_UINT8, %eax
	jge	.Lcls_retint

0:	cmpl	$FFI_TYPE_FLOAT, %eax
	je	.Lcls_retfloat
	cmpl	$FFI_TYPE_DOUBLE, %eax
	je	.Lcls_retdouble
	cmpl	$FFI_TYPE_LONGDOUBLE, %eax
	je	.Lcls_retldouble
	cmpl	$FFI_TYPE_SINT64, %eax
	je	.Lcls_retllong
	cmpl	$FFI_TYPE_SMALL_STRUCT_1B, %eax
	je	.Lcls_retstruct1b
	cmpl	$FFI_TYPE_SMALL_STRUCT_2B, %eax
	je	.Lcls_retstruct2b
	cmpl	$FFI_TYPE_STRUCT, %eax
	je	.Lcls_retstruct
.Lcls_epilogue:
	movl	%ebp, %esp
	popl	%ebp
	ret
.Lcls_retint:
	movl	(%ecx), %eax
	jmp	.Lcls_epilogue
.Lcls_retfloat:
	flds	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retdouble:
	fldl	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retldouble:
	fldt	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retllong:
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	jmp	.Lcls_epilogue
.Lcls_retstruct1b:
	movsbl	(%ecx), %eax
	jmp	.Lcls_epilogue
.Lcls_retstruct2b:
	movswl	(%ecx), %eax
	jmp	.Lcls_epilogue
.Lcls_retstruct:
	lea -8(%ebp),%esp
	movl	%ebp, %esp
	popl	%ebp
	ret $4
.LFE2:

#if !FFI_NO_RAW_API

#define RAW_CLOSURE_CIF_OFFSET ((FFI_TRAMPOLINE_SIZE + 3) & ~3)
#define RAW_CLOSURE_FUN_OFFSET (RAW_CLOSURE_CIF_OFFSET + 4)
#define RAW_CLOSURE_USER_DATA_OFFSET (RAW_CLOSURE_FUN_OFFSET + 4)
#define CIF_FLAGS_OFFSET 20

	.align	4
FFI_HIDDEN (ffi_closure_raw_SYSV)
.globl _ffi_closure_raw_SYSV

_ffi_closure_raw_SYSV:
.LFB3:
	pushl	%ebp
.LCFI4:
	movl	%esp, %ebp
.LCFI5:
	pushl	%esi
.LCFI6:
	subl	$36, %esp
	movl	RAW_CLOSURE_CIF_OFFSET(%eax), %esi	 /* closure->cif */
	movl	RAW_CLOSURE_USER_DATA_OFFSET(%eax), %edx /* closure->user_data */
	movl	%edx, 12(%esp)	/* user_data */
	leal	8(%ebp), %edx	/* __builtin_dwarf_cfa () */
	movl	%edx, 8(%esp)	/* raw_args */
	leal	-24(%ebp), %edx
	movl	%edx, 4(%esp)	/* &res */
	movl	%esi, (%esp)	/* cif */
	call	*RAW_CLOSURE_FUN_OFFSET(%eax)		 /* closure->fun */
	movl	CIF_FLAGS_OFFSET(%esi), %eax		 /* rtype */
	cmpl	$FFI_TYPE_INT, %eax
	je	.Lrcls_retint

	/* Handle FFI_TYPE_UINT8, FFI_TYPE_SINT8, FFI_TYPE_UINT16,
	   FFI_TYPE_SINT16, FFI_TYPE_UINT32, FFI_TYPE_SINT32.  */
	cmpl	$FFI_TYPE_UINT64, %eax
	jge	0f
	cmpl	$FFI_TYPE_UINT8, %eax
	jge	.Lrcls_retint
0:
	cmpl	$FFI_TYPE_FLOAT, %eax
	je	.Lrcls_retfloat
	cmpl	$FFI_TYPE_DOUBLE, %eax
	je	.Lrcls_retdouble
	cmpl	$FFI_TYPE_LONGDOUBLE, %eax
	je	.Lrcls_retldouble
	cmpl	$FFI_TYPE_SINT64, %eax
	je	.Lrcls_retllong
.Lrcls_epilogue:
	addl	$36, %esp
	popl	%esi
	popl	%ebp
	ret
.Lrcls_retint:
	movl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue
.Lrcls_retfloat:
	flds	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retdouble:
	fldl	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retldouble:
	fldt	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retllong:
	movl	-24(%ebp), %eax
	movl	-20(%ebp), %edx
	jmp	.Lrcls_epilogue
.LFE3:
#endif

.section __IMPORT,__jump_table,symbol_stubs,self_modifying_code+pure_instructions,5
L_ffi_closure_SYSV_inner$stub:
	.indirect_symbol _ffi_closure_SYSV_inner
	hlt ; hlt ; hlt ; hlt ; hlt


.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
	.set	L$set$0,LECIE1-LSCIE1
	.long	L$set$0
LSCIE1:
	.long	0x0
	.byte	0x1
	.ascii "zR\0"
	.byte	0x1
	.byte	0x7c
	.byte	0x8
	.byte	0x1
	.byte	0x10
	.byte	0xc
	.byte	0x5
	.byte	0x4
	.byte	0x88
	.byte	0x1
	.align 2
LECIE1:
.globl _ffi_call_SYSV.eh
_ffi_call_SYSV.eh:
LSFDE1:
	.set	L$set$1,LEFDE1-LASFDE1
	.long	L$set$1
LASFDE1:
	.long	LASFDE1-EH_frame1
	.long	.LFB1-.
	.set L$set$2,.LFE1-.LFB1
	.long L$set$2
	.byte	0x0
	.byte	0x4
	.set L$set$3,.LCFI0-.LFB1
	.long L$set$3
	.byte	0xe
	.byte	0x8
	.byte	0x84
	.byte	0x2
	.byte	0x4
	.set L$set$4,.LCFI1-.LCFI0
	.long L$set$4
	.byte	0xd
	.byte	0x4
	.align 2
LEFDE1:
.globl _ffi_closure_SYSV.eh
_ffi_closure_SYSV.eh:
LSFDE2:
	.set	L$set$5,LEFDE2-LASFDE2
	.long	L$set$5
LASFDE2:
	.long	LASFDE2-EH_frame1
	.long	.LFB2-.
	.set L$set$6,.LFE2-.LFB2
	.long L$set$6
	.byte	0x0
	.byte	0x4
	.set L$set$7,.LCFI2-.LFB2
	.long L$set$7
	.byte	0xe
	.byte	0x8
	.byte	0x84
	.byte	0x2
	.byte	0x4
	.set L$set$8,.LCFI3-.LCFI2
	.long L$set$8
	.byte	0xd
	.byte	0x4
	.align 2
LEFDE2:

#if !FFI_NO_RAW_API

.globl _ffi_closure_raw_SYSV.eh
_ffi_closure_raw_SYSV.eh:
LSFDE3:
	.set	L$set$10,LEFDE3-LASFDE3
	.long	L$set$10
LASFDE3:
	.long	LASFDE3-EH_frame1
	.long	.LFB3-.
	.set L$set$11,.LFE3-.LFB3
	.long L$set$11
	.byte	0x0
	.byte	0x4
	.set L$set$12,.LCFI4-.LFB3
	.long L$set$12
	.byte	0xe
	.byte	0x8
	.byte	0x84
	.byte	0x2
	.byte	0x4
	.set L$set$13,.LCFI5-.LCFI4
	.long L$set$13
	.byte	0xd
	.byte	0x4
	.byte	0x4
	.set L$set$14,.LCFI6-.LCFI5
	.long L$set$14
	.byte	0x85
	.byte	0x3
	.align 2
LEFDE3:

#endif

#endif /* ifndef __x86_64__ */
