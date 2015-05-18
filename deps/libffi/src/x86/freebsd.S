/* -----------------------------------------------------------------------
   freebsd.S - Copyright (c) 1996, 1998, 2001, 2002, 2003, 2005  Red Hat, Inc.
	       Copyright (c) 2008  Björn König
	
   X86 Foreign Function Interface for FreeBSD

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

#ifndef __x86_64__

#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>

.text

.globl ffi_prep_args

	.align 4
.globl ffi_call_SYSV
        .type    ffi_call_SYSV,@function

ffi_call_SYSV:
.LFB1:
        pushl %ebp
.LCFI0:
        movl  %esp,%ebp
.LCFI1:
	/* Make room for all of the new args.  */
	movl  16(%ebp),%ecx
	subl  %ecx,%esp

	movl  %esp,%eax

	/* Place all of the ffi_prep_args in position  */
	pushl 12(%ebp)
	pushl %eax
	call  *8(%ebp)

	/* Return stack to previous state and call the function  */
	addl  $8,%esp	

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
	call  1f

.Lstore_table:
	.long	noretval-.Lstore_table	/* FFI_TYPE_VOID */
	.long	retint-.Lstore_table	/* FFI_TYPE_INT */
	.long	retfloat-.Lstore_table	/* FFI_TYPE_FLOAT */
	.long	retdouble-.Lstore_table	/* FFI_TYPE_DOUBLE */
	.long	retlongdouble-.Lstore_table	/* FFI_TYPE_LONGDOUBLE */
	.long	retuint8-.Lstore_table	/* FFI_TYPE_UINT8 */
	.long	retsint8-.Lstore_table	/* FFI_TYPE_SINT8 */
	.long	retuint16-.Lstore_table	/* FFI_TYPE_UINT16 */
	.long	retsint16-.Lstore_table	/* FFI_TYPE_SINT16 */
	.long	retint-.Lstore_table	/* FFI_TYPE_UINT32 */
	.long	retint-.Lstore_table	/* FFI_TYPE_SINT32 */
	.long	retint64-.Lstore_table	/* FFI_TYPE_UINT64 */
	.long	retint64-.Lstore_table	/* FFI_TYPE_SINT64 */
	.long	retstruct-.Lstore_table	/* FFI_TYPE_STRUCT */
	.long	retint-.Lstore_table	/* FFI_TYPE_POINTER */
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
        .size    ffi_call_SYSV,.ffi_call_SYSV_end-ffi_call_SYSV

	.align	4
FFI_HIDDEN (ffi_closure_SYSV)
.globl ffi_closure_SYSV
	.type	ffi_closure_SYSV, @function

ffi_closure_SYSV:
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
#if defined HAVE_HIDDEN_VISIBILITY_ATTRIBUTE || !defined __PIC__
	call	ffi_closure_SYSV_inner
#else
	movl	%ebx, 8(%esp)
.LCFI7:
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	call	ffi_closure_SYSV_inner@PLT
	movl	8(%esp), %ebx
#endif
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
	movl	%ebp, %esp
	popl	%ebp
	ret	$4
.LFE2:
	.size	ffi_closure_SYSV, .-ffi_closure_SYSV

#if !FFI_NO_RAW_API

#define RAW_CLOSURE_CIF_OFFSET ((FFI_TRAMPOLINE_SIZE + 3) & ~3)
#define RAW_CLOSURE_FUN_OFFSET (RAW_CLOSURE_CIF_OFFSET + 4)
#define RAW_CLOSURE_USER_DATA_OFFSET (RAW_CLOSURE_FUN_OFFSET + 4)
#define CIF_FLAGS_OFFSET 20

	.align	4
FFI_HIDDEN (ffi_closure_raw_SYSV)
.globl ffi_closure_raw_SYSV
	.type	ffi_closure_raw_SYSV, @function

ffi_closure_raw_SYSV:
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
	.size	ffi_closure_raw_SYSV, .-ffi_closure_raw_SYSV
#endif

	.section	.eh_frame,EH_FRAME_FLAGS,@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1	/* Length of Common Information Entry */
.LSCIE1:
	.long	0x0	/* CIE Identifier Tag */
	.byte	0x1	/* CIE Version */
#ifdef __PIC__
	.ascii "zR\0"	/* CIE Augmentation */
#else
	.ascii "\0"	/* CIE Augmentation */
#endif
	.byte	0x1	/* .uleb128 0x1; CIE Code Alignment Factor */
	.byte	0x7c	/* .sleb128 -4; CIE Data Alignment Factor */
	.byte	0x8	/* CIE RA Column */
#ifdef __PIC__
	.byte	0x1	/* .uleb128 0x1; Augmentation size */
	.byte	0x1b	/* FDE Encoding (pcrel sdata4) */
#endif
	.byte	0xc	/* DW_CFA_def_cfa */
	.byte	0x4	/* .uleb128 0x4 */
	.byte	0x4	/* .uleb128 0x4 */
	.byte	0x88	/* DW_CFA_offset, column 0x8 */
	.byte	0x1	/* .uleb128 0x1 */
	.align 4
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1	/* FDE Length */
.LASFDE1:
	.long	.LASFDE1-.Lframe1	/* FDE CIE offset */
#ifdef __PIC__
	.long	.LFB1-.	/* FDE initial location */
#else
	.long	.LFB1	/* FDE initial location */
#endif
	.long	.LFE1-.LFB1	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI0-.LFB1
	.byte	0xe	/* DW_CFA_def_cfa_offset */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 */
	.byte	0x2	/* .uleb128 0x2 */
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI1-.LCFI0
	.byte	0xd	/* DW_CFA_def_cfa_register */
	.byte	0x5	/* .uleb128 0x5 */
	.align 4
.LEFDE1:
.LSFDE2:
	.long	.LEFDE2-.LASFDE2	/* FDE Length */
.LASFDE2:
	.long	.LASFDE2-.Lframe1	/* FDE CIE offset */
#ifdef __PIC__
	.long	.LFB2-.	/* FDE initial location */
#else
	.long	.LFB2
#endif
	.long	.LFE2-.LFB2	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI2-.LFB2
	.byte	0xe	/* DW_CFA_def_cfa_offset */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 */
	.byte	0x2	/* .uleb128 0x2 */
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI3-.LCFI2
	.byte	0xd	/* DW_CFA_def_cfa_register */
	.byte	0x5	/* .uleb128 0x5 */
#if !defined HAVE_HIDDEN_VISIBILITY_ATTRIBUTE && defined __PIC__
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI7-.LCFI3
	.byte	0x83	/* DW_CFA_offset, column 0x3 */
	.byte	0xa	/* .uleb128 0xa */
#endif
	.align 4
.LEFDE2:

#if !FFI_NO_RAW_API

.LSFDE3:
	.long	.LEFDE3-.LASFDE3	/* FDE Length */
.LASFDE3:
	.long	.LASFDE3-.Lframe1	/* FDE CIE offset */
#ifdef __PIC__
	.long	.LFB3-.	/* FDE initial location */
#else
	.long	.LFB3
#endif
	.long	.LFE3-.LFB3	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI4-.LFB3
	.byte	0xe	/* DW_CFA_def_cfa_offset */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 */
	.byte	0x2	/* .uleb128 0x2 */
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI5-.LCFI4
	.byte	0xd	/* DW_CFA_def_cfa_register */
	.byte	0x5	/* .uleb128 0x5 */
	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI6-.LCFI5
	.byte	0x86	/* DW_CFA_offset, column 0x6 */
	.byte	0x3	/* .uleb128 0x3 */
	.align 4
.LEFDE3:

#endif

#endif /* ifndef __x86_64__ */
