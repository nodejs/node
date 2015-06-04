#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>
	
/* Constants for ffi_call_win64 */	
#define STACK 0
#define PREP_ARGS_FN 32
#define ECIF 40
#define CIF_BYTES 48
#define CIF_FLAGS 56
#define RVALUE 64
#define FN 72

/* ffi_call_win64 (void (*prep_args_fn)(char *, extended_cif *),
                   extended_cif *ecif, unsigned bytes, unsigned flags,
                   unsigned *rvalue, void (*fn)());
 */

#ifdef _MSC_VER
PUBLIC	ffi_call_win64

EXTRN	__chkstk:NEAR
EXTRN	ffi_closure_win64_inner:NEAR

_TEXT	SEGMENT

;;; ffi_closure_win64 will be called with these registers set:
;;;    rax points to 'closure'
;;;    r11 contains a bit mask that specifies which of the
;;;    first four parameters are float or double
;;;
;;; It must move the parameters passed in registers to their stack location,
;;; call ffi_closure_win64_inner for the actual work, then return the result.
;;; 
ffi_closure_win64 PROC FRAME
	;; copy register arguments onto stack
	test	r11, 1
	jne	first_is_float	
	mov	QWORD PTR [rsp+8], rcx
	jmp	second
first_is_float:
	movlpd	QWORD PTR [rsp+8], xmm0

second:
	test	r11, 2
	jne	second_is_float	
	mov	QWORD PTR [rsp+16], rdx
	jmp	third
second_is_float:
	movlpd	QWORD PTR [rsp+16], xmm1

third:
	test	r11, 4
	jne	third_is_float	
	mov	QWORD PTR [rsp+24], r8
	jmp	fourth
third_is_float:
	movlpd	QWORD PTR [rsp+24], xmm2

fourth:
	test	r11, 8
	jne	fourth_is_float	
	mov	QWORD PTR [rsp+32], r9
	jmp	done
fourth_is_float:
	movlpd	QWORD PTR [rsp+32], xmm3

done:
        .ALLOCSTACK 40
	sub	rsp, 40
        .ENDPROLOG
	mov	rcx, rax	; context is first parameter
	mov	rdx, rsp	; stack is second parameter
	add	rdx, 48		; point to start of arguments
	mov	rax, ffi_closure_win64_inner
	call	rax		; call the real closure function
	add	rsp, 40
	movd	xmm0, rax	; If the closure returned a float,
                                ; ffi_closure_win64_inner wrote it to rax
	ret	0
ffi_closure_win64 ENDP

ffi_call_win64 PROC FRAME
        ;; copy registers onto stack
	mov	QWORD PTR [rsp+32], r9
	mov	QWORD PTR [rsp+24], r8
	mov	QWORD PTR [rsp+16], rdx
	mov	QWORD PTR [rsp+8], rcx
        .PUSHREG rbp
	push	rbp
        .ALLOCSTACK 48
	sub	rsp, 48					; 00000030H
        .SETFRAME rbp, 32
	lea	rbp, QWORD PTR [rsp+32]
        .ENDPROLOG

	mov	eax, DWORD PTR CIF_BYTES[rbp]
	add	rax, 15
	and	rax, -16
	call	__chkstk
	sub	rsp, rax
	lea	rax, QWORD PTR [rsp+32]
	mov	QWORD PTR STACK[rbp], rax

	mov	rdx, QWORD PTR ECIF[rbp]
	mov	rcx, QWORD PTR STACK[rbp]
	call	QWORD PTR PREP_ARGS_FN[rbp]

	mov	rsp, QWORD PTR STACK[rbp]

	movlpd	xmm3, QWORD PTR [rsp+24]
	movd	r9, xmm3

	movlpd	xmm2, QWORD PTR [rsp+16]
	movd	r8, xmm2

	movlpd	xmm1, QWORD PTR [rsp+8]
	movd	rdx, xmm1

	movlpd	xmm0, QWORD PTR [rsp]
	movd	rcx, xmm0

	call	QWORD PTR FN[rbp]
ret_struct4b$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SMALL_STRUCT_4B
 	jne	ret_struct2b$

	mov	rcx, QWORD PTR RVALUE[rbp]
	mov	DWORD PTR [rcx], eax
	jmp	ret_void$

ret_struct2b$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SMALL_STRUCT_2B
 	jne	ret_struct1b$

	mov	rcx, QWORD PTR RVALUE[rbp]
	mov	WORD PTR [rcx], ax
	jmp	ret_void$

ret_struct1b$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SMALL_STRUCT_1B
 	jne	ret_uint8$

	mov	rcx, QWORD PTR RVALUE[rbp]
	mov	BYTE PTR [rcx], al
	jmp	ret_void$

ret_uint8$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_UINT8
 	jne	ret_sint8$

	mov	rcx, QWORD PTR RVALUE[rbp]
	movzx   rax, al
	mov	QWORD PTR [rcx], rax
	jmp	ret_void$

ret_sint8$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SINT8
 	jne	ret_uint16$

	mov	rcx, QWORD PTR RVALUE[rbp]
	movsx   rax, al
	mov	QWORD PTR [rcx], rax
	jmp	ret_void$

ret_uint16$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_UINT16
 	jne	ret_sint16$

	mov	rcx, QWORD PTR RVALUE[rbp]
	movzx   rax, ax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_sint16$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SINT16
 	jne	ret_uint32$

	mov	rcx, QWORD PTR RVALUE[rbp]
	movsx   rax, ax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_uint32$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_UINT32
 	jne	ret_sint32$

	mov	rcx, QWORD PTR RVALUE[rbp]
	mov     eax, eax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_sint32$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SINT32
 	jne	ret_float$

	mov	rcx, QWORD PTR RVALUE[rbp]
	cdqe
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_float$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_FLOAT
 	jne	SHORT ret_double$

 	mov	rax, QWORD PTR RVALUE[rbp]
 	movss	DWORD PTR [rax], xmm0
 	jmp	SHORT ret_void$

ret_double$:
 	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_DOUBLE
 	jne	SHORT ret_sint64$

 	mov	rax, QWORD PTR RVALUE[rbp]
 	movlpd	QWORD PTR [rax], xmm0
 	jmp	SHORT ret_void$

ret_sint64$:
  	cmp	DWORD PTR CIF_FLAGS[rbp], FFI_TYPE_SINT64
  	jne	ret_void$

 	mov	rcx, QWORD PTR RVALUE[rbp]
 	mov	QWORD PTR [rcx], rax
 	jmp	SHORT ret_void$
	
ret_void$:
	xor	rax, rax

	lea	rsp, QWORD PTR [rbp+16]
	pop	rbp
	ret	0
ffi_call_win64 ENDP
_TEXT	ENDS
END

#else

#ifdef SYMBOL_UNDERSCORE
#define SYMBOL_NAME(name) _##name
#else
#define SYMBOL_NAME(name) name
#endif

.text

.extern SYMBOL_NAME(ffi_closure_win64_inner)

# ffi_closure_win64 will be called with these registers set:
#    rax points to 'closure'
#    r11 contains a bit mask that specifies which of the
#    first four parameters are float or double
#
# It must move the parameters passed in registers to their stack location,
# call ffi_closure_win64_inner for the actual work, then return the result.
# 
	.balign 16
        .globl SYMBOL_NAME(ffi_closure_win64)
SYMBOL_NAME(ffi_closure_win64):
	# copy register arguments onto stack
	test	$1,%r11
	jne	.Lfirst_is_float	
	mov	%rcx, 8(%rsp)
	jmp	.Lsecond
.Lfirst_is_float:
	movlpd	%xmm0, 8(%rsp)

.Lsecond:
	test	$2, %r11
	jne	.Lsecond_is_float	
	mov	%rdx, 16(%rsp)
	jmp	.Lthird
.Lsecond_is_float:
	movlpd	%xmm1, 16(%rsp)

.Lthird:
	test	$4, %r11
	jne	.Lthird_is_float	
	mov	%r8,24(%rsp)
	jmp	.Lfourth
.Lthird_is_float:
	movlpd	%xmm2, 24(%rsp)

.Lfourth:
	test	$8, %r11
	jne	.Lfourth_is_float	
	mov	%r9, 32(%rsp)
	jmp	.Ldone
.Lfourth_is_float:
	movlpd	%xmm3, 32(%rsp)

.Ldone:
#.ALLOCSTACK 40
	sub	$40, %rsp
#.ENDPROLOG
	mov	%rax, %rcx	# context is first parameter
	mov	%rsp, %rdx	# stack is second parameter
	add	$48, %rdx	# point to start of arguments
	mov	$SYMBOL_NAME(ffi_closure_win64_inner), %rax
	callq	*%rax		# call the real closure function
	add	$40, %rsp
	movq	%rax, %xmm0	# If the closure returned a float,
                                # ffi_closure_win64_inner wrote it to rax
	retq
.ffi_closure_win64_end:

	.balign 16
        .globl	SYMBOL_NAME(ffi_call_win64)
SYMBOL_NAME(ffi_call_win64):
        # copy registers onto stack
	mov	%r9,32(%rsp)
	mov	%r8,24(%rsp)
	mov	%rdx,16(%rsp)
	mov	%rcx,8(%rsp)
        #.PUSHREG rbp
	push	%rbp
        #.ALLOCSTACK 48
	sub	$48,%rsp
        #.SETFRAME rbp, 32
	lea	32(%rsp),%rbp
        #.ENDPROLOG

	mov	CIF_BYTES(%rbp),%eax
	add	$15, %rax
	and	$-16, %rax
	cmpq	$0x1000, %rax
	jb	Lch_done
Lch_probe:
	subq	$0x1000,%rsp
	orl	$0x0, (%rsp)
	subq	$0x1000,%rax
	cmpq	$0x1000,%rax
	ja	Lch_probe
Lch_done:
	subq	%rax, %rsp
	orl	$0x0, (%rsp)
	lea	32(%rsp), %rax
	mov	%rax, STACK(%rbp)

	mov	ECIF(%rbp), %rdx
	mov	STACK(%rbp), %rcx
	callq	*PREP_ARGS_FN(%rbp)

	mov	STACK(%rbp), %rsp

	movlpd	24(%rsp), %xmm3
	movd	%xmm3, %r9

	movlpd	16(%rsp), %xmm2
	movd	%xmm2, %r8

	movlpd	8(%rsp), %xmm1
	movd	%xmm1, %rdx

	movlpd	(%rsp), %xmm0
	movd	%xmm0, %rcx

	callq	*FN(%rbp)
.Lret_struct4b:
 	cmpl	$FFI_TYPE_SMALL_STRUCT_4B, CIF_FLAGS(%rbp)
 	jne .Lret_struct2b

	mov	RVALUE(%rbp), %rcx
	mov	%eax, (%rcx)
	jmp	.Lret_void

.Lret_struct2b:
	cmpl	$FFI_TYPE_SMALL_STRUCT_2B, CIF_FLAGS(%rbp)
	jne .Lret_struct1b
	
	mov	RVALUE(%rbp), %rcx
	mov	%ax, (%rcx)
	jmp .Lret_void
	
.Lret_struct1b:
	cmpl	$FFI_TYPE_SMALL_STRUCT_1B, CIF_FLAGS(%rbp)
	jne .Lret_uint8
	
	mov	RVALUE(%rbp), %rcx
	mov	%al, (%rcx)
	jmp .Lret_void

.Lret_uint8:
	cmpl	$FFI_TYPE_UINT8, CIF_FLAGS(%rbp)
	jne .Lret_sint8
	
        mov     RVALUE(%rbp), %rcx
        movzbq  %al, %rax
	movq    %rax, (%rcx)
	jmp .Lret_void

.Lret_sint8:
	cmpl	$FFI_TYPE_SINT8, CIF_FLAGS(%rbp)
	jne .Lret_uint16
	
        mov     RVALUE(%rbp), %rcx
        movsbq  %al, %rax
	movq    %rax, (%rcx)
	jmp .Lret_void

.Lret_uint16:
	cmpl	$FFI_TYPE_UINT16, CIF_FLAGS(%rbp)
	jne .Lret_sint16
	
        mov     RVALUE(%rbp), %rcx
        movzwq  %ax, %rax
	movq    %rax, (%rcx)
	jmp .Lret_void

.Lret_sint16:
	cmpl	$FFI_TYPE_SINT16, CIF_FLAGS(%rbp)
	jne .Lret_uint32
	
        mov     RVALUE(%rbp), %rcx
        movswq  %ax, %rax
	movq    %rax, (%rcx)
	jmp .Lret_void

.Lret_uint32:
	cmpl	$FFI_TYPE_UINT32, CIF_FLAGS(%rbp)
	jne .Lret_sint32
	
        mov     RVALUE(%rbp), %rcx
        movl    %eax, %eax
	movq    %rax, (%rcx)
	jmp .Lret_void

.Lret_sint32:
 	cmpl	$FFI_TYPE_SINT32, CIF_FLAGS(%rbp)
 	jne	.Lret_float

	mov	RVALUE(%rbp), %rcx
	cltq
	movq	%rax, (%rcx)
	jmp	.Lret_void

.Lret_float:
 	cmpl	$FFI_TYPE_FLOAT, CIF_FLAGS(%rbp)
 	jne	.Lret_double

 	mov	RVALUE(%rbp), %rax
 	movss	%xmm0, (%rax)
 	jmp	.Lret_void

.Lret_double:
 	cmpl	$FFI_TYPE_DOUBLE, CIF_FLAGS(%rbp)
 	jne	.Lret_sint64

 	mov	RVALUE(%rbp), %rax
 	movlpd	%xmm0, (%rax)
 	jmp	.Lret_void

.Lret_sint64:
  	cmpl	$FFI_TYPE_SINT64, CIF_FLAGS(%rbp)
  	jne	.Lret_void

 	mov	RVALUE(%rbp), %rcx
 	mov	%rax, (%rcx)
 	jmp	.Lret_void
	
.Lret_void:
	xor	%rax, %rax

	lea	16(%rbp), %rsp
	pop	%rbp
	retq
.ffi_call_win64_end:
#endif /* !_MSC_VER */

