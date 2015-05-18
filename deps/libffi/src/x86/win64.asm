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

	mov	eax, DWORD PTR 48[rbp]
	add	rax, 15
	and	rax, -16
	call	__chkstk
	sub	rsp, rax
	lea	rax, QWORD PTR [rsp+32]
	mov	QWORD PTR 0[rbp], rax

	mov	rdx, QWORD PTR 40[rbp]
	mov	rcx, QWORD PTR 0[rbp]
	call	QWORD PTR 32[rbp]

	mov	rsp, QWORD PTR 0[rbp]

	movlpd	xmm3, QWORD PTR [rsp+24]
	movd	r9, xmm3

	movlpd	xmm2, QWORD PTR [rsp+16]
	movd	r8, xmm2

	movlpd	xmm1, QWORD PTR [rsp+8]
	movd	rdx, xmm1

	movlpd	xmm0, QWORD PTR [rsp]
	movd	rcx, xmm0

	call	QWORD PTR 72[rbp]
ret_struct4b$:
 	cmp	DWORD PTR 56[rbp], (14 + 3)
 	jne	ret_struct2b$

	mov	rcx, QWORD PTR 64[rbp]
	mov	DWORD PTR [rcx], eax
	jmp	ret_void$

ret_struct2b$:
 	cmp	DWORD PTR 56[rbp], (14 + 2)
 	jne	ret_struct1b$

	mov	rcx, QWORD PTR 64[rbp]
	mov	WORD PTR [rcx], ax
	jmp	ret_void$

ret_struct1b$:
 	cmp	DWORD PTR 56[rbp], (14 + 1)
 	jne	ret_uint8$

	mov	rcx, QWORD PTR 64[rbp]
	mov	BYTE PTR [rcx], al
	jmp	ret_void$

ret_uint8$:
 	cmp	DWORD PTR 56[rbp], 5
 	jne	ret_sint8$

	mov	rcx, QWORD PTR 64[rbp]
	movzx   rax, al
	mov	QWORD PTR [rcx], rax
	jmp	ret_void$

ret_sint8$:
 	cmp	DWORD PTR 56[rbp], 6
 	jne	ret_uint16$

	mov	rcx, QWORD PTR 64[rbp]
	movsx   rax, al
	mov	QWORD PTR [rcx], rax
	jmp	ret_void$

ret_uint16$:
 	cmp	DWORD PTR 56[rbp], 7
 	jne	ret_sint16$

	mov	rcx, QWORD PTR 64[rbp]
	movzx   rax, ax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_sint16$:
 	cmp	DWORD PTR 56[rbp], 8
 	jne	ret_uint32$

	mov	rcx, QWORD PTR 64[rbp]
	movsx   rax, ax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_uint32$:
 	cmp	DWORD PTR 56[rbp], 9
 	jne	ret_sint32$

	mov	rcx, QWORD PTR 64[rbp]
	mov     eax, eax
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_sint32$:
 	cmp	DWORD PTR 56[rbp], 10
 	jne	ret_float$

	mov	rcx, QWORD PTR 64[rbp]
	cdqe
	mov	QWORD PTR [rcx], rax
	jmp	SHORT ret_void$

ret_float$:
 	cmp	DWORD PTR 56[rbp], 2
 	jne	SHORT ret_double$

 	mov	rax, QWORD PTR 64[rbp]
 	movss	DWORD PTR [rax], xmm0
 	jmp	SHORT ret_void$

ret_double$:
 	cmp	DWORD PTR 56[rbp], 3
 	jne	SHORT ret_sint64$

 	mov	rax, QWORD PTR 64[rbp]
 	movlpd	QWORD PTR [rax], xmm0
 	jmp	SHORT ret_void$

ret_sint64$:
  	cmp	DWORD PTR 56[rbp], 12
  	jne	ret_void$

 	mov	rcx, QWORD PTR 64[rbp]
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
