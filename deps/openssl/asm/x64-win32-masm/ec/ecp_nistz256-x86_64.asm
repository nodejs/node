OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR


ALIGN	64
$L$poly::
	DQ	0ffffffffffffffffh,000000000ffffffffh,00000000000000000h,0ffffffff00000001h


$L$RR::
	DQ	00000000000000003h,0fffffffbffffffffh,0fffffffffffffffeh,000000004fffffffdh

$L$One::
	DD	1,1,1,1,1,1,1,1
$L$Two::
	DD	2,2,2,2,2,2,2,2
$L$Three::
	DD	3,3,3,3,3,3,3,3
$L$ONE_mont::
	DQ	00000000000000001h,0ffffffff00000000h,0ffffffffffffffffh,000000000fffffffeh

PUBLIC	ecp_nistz256_mul_by_2

ALIGN	64
ecp_nistz256_mul_by_2	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_mul_by_2::
	mov	rdi,rcx
	mov	rsi,rdx


	push	r12
	push	r13

	mov	r8,QWORD PTR[rsi]
	xor	r13,r13
	mov	r9,QWORD PTR[8+rsi]
	add	r8,r8
	mov	r10,QWORD PTR[16+rsi]
	adc	r9,r9
	mov	r11,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[$L$poly]
	mov	rax,r8
	adc	r10,r10
	adc	r11,r11
	mov	rdx,r9
	adc	r13,0

	sub	r8,QWORD PTR[rsi]
	mov	rcx,r10
	sbb	r9,QWORD PTR[8+rsi]
	sbb	r10,QWORD PTR[16+rsi]
	mov	r12,r11
	sbb	r11,QWORD PTR[24+rsi]
	sbb	r13,0

	cmovc	r8,rax
	cmovc	r9,rdx
	mov	QWORD PTR[rdi],r8
	cmovc	r10,rcx
	mov	QWORD PTR[8+rdi],r9
	cmovc	r11,r12
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_mul_by_2::
ecp_nistz256_mul_by_2	ENDP



PUBLIC	ecp_nistz256_div_by_2

ALIGN	32
ecp_nistz256_div_by_2	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_div_by_2::
	mov	rdi,rcx
	mov	rsi,rdx


	push	r12
	push	r13

	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[8+rsi]
	mov	r10,QWORD PTR[16+rsi]
	mov	rax,r8
	mov	r11,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[$L$poly]

	mov	rdx,r9
	xor	r13,r13
	add	r8,QWORD PTR[rsi]
	mov	rcx,r10
	adc	r9,QWORD PTR[8+rsi]
	adc	r10,QWORD PTR[16+rsi]
	mov	r12,r11
	adc	r11,QWORD PTR[24+rsi]
	adc	r13,0
	xor	rsi,rsi
	test	rax,1

	cmovz	r8,rax
	cmovz	r9,rdx
	cmovz	r10,rcx
	cmovz	r11,r12
	cmovz	r13,rsi

	mov	rax,r9
	shr	r8,1
	shl	rax,63
	mov	rdx,r10
	shr	r9,1
	or	r8,rax
	shl	rdx,63
	mov	rcx,r11
	shr	r10,1
	or	r9,rdx
	shl	rcx,63
	shr	r11,1
	shl	r13,63
	or	r10,rcx
	or	r11,r13

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_div_by_2::
ecp_nistz256_div_by_2	ENDP



PUBLIC	ecp_nistz256_mul_by_3

ALIGN	32
ecp_nistz256_mul_by_3	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_mul_by_3::
	mov	rdi,rcx
	mov	rsi,rdx


	push	r12
	push	r13

	mov	r8,QWORD PTR[rsi]
	xor	r13,r13
	mov	r9,QWORD PTR[8+rsi]
	add	r8,r8
	mov	r10,QWORD PTR[16+rsi]
	adc	r9,r9
	mov	r11,QWORD PTR[24+rsi]
	mov	rax,r8
	adc	r10,r10
	adc	r11,r11
	mov	rdx,r9
	adc	r13,0

	sub	r8,-1
	mov	rcx,r10
	sbb	r9,QWORD PTR[(($L$poly+8))]
	sbb	r10,0
	mov	r12,r11
	sbb	r11,QWORD PTR[(($L$poly+24))]
	sbb	r13,0

	cmovc	r8,rax
	cmovc	r9,rdx
	cmovc	r10,rcx
	cmovc	r11,r12

	xor	r13,r13
	add	r8,QWORD PTR[rsi]
	adc	r9,QWORD PTR[8+rsi]
	mov	rax,r8
	adc	r10,QWORD PTR[16+rsi]
	adc	r11,QWORD PTR[24+rsi]
	mov	rdx,r9
	adc	r13,0

	sub	r8,-1
	mov	rcx,r10
	sbb	r9,QWORD PTR[(($L$poly+8))]
	sbb	r10,0
	mov	r12,r11
	sbb	r11,QWORD PTR[(($L$poly+24))]
	sbb	r13,0

	cmovc	r8,rax
	cmovc	r9,rdx
	mov	QWORD PTR[rdi],r8
	cmovc	r10,rcx
	mov	QWORD PTR[8+rdi],r9
	cmovc	r11,r12
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_mul_by_3::
ecp_nistz256_mul_by_3	ENDP



PUBLIC	ecp_nistz256_add

ALIGN	32
ecp_nistz256_add	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_add::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	r12
	push	r13

	mov	r8,QWORD PTR[rsi]
	xor	r13,r13
	mov	r9,QWORD PTR[8+rsi]
	mov	r10,QWORD PTR[16+rsi]
	mov	r11,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[$L$poly]

	add	r8,QWORD PTR[rdx]
	adc	r9,QWORD PTR[8+rdx]
	mov	rax,r8
	adc	r10,QWORD PTR[16+rdx]
	adc	r11,QWORD PTR[24+rdx]
	mov	rdx,r9
	adc	r13,0

	sub	r8,QWORD PTR[rsi]
	mov	rcx,r10
	sbb	r9,QWORD PTR[8+rsi]
	sbb	r10,QWORD PTR[16+rsi]
	mov	r12,r11
	sbb	r11,QWORD PTR[24+rsi]
	sbb	r13,0

	cmovc	r8,rax
	cmovc	r9,rdx
	mov	QWORD PTR[rdi],r8
	cmovc	r10,rcx
	mov	QWORD PTR[8+rdi],r9
	cmovc	r11,r12
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_add::
ecp_nistz256_add	ENDP



PUBLIC	ecp_nistz256_sub

ALIGN	32
ecp_nistz256_sub	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_sub::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	r12
	push	r13

	mov	r8,QWORD PTR[rsi]
	xor	r13,r13
	mov	r9,QWORD PTR[8+rsi]
	mov	r10,QWORD PTR[16+rsi]
	mov	r11,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[$L$poly]

	sub	r8,QWORD PTR[rdx]
	sbb	r9,QWORD PTR[8+rdx]
	mov	rax,r8
	sbb	r10,QWORD PTR[16+rdx]
	sbb	r11,QWORD PTR[24+rdx]
	mov	rdx,r9
	sbb	r13,0

	add	r8,QWORD PTR[rsi]
	mov	rcx,r10
	adc	r9,QWORD PTR[8+rsi]
	adc	r10,QWORD PTR[16+rsi]
	mov	r12,r11
	adc	r11,QWORD PTR[24+rsi]
	test	r13,r13

	cmovz	r8,rax
	cmovz	r9,rdx
	mov	QWORD PTR[rdi],r8
	cmovz	r10,rcx
	mov	QWORD PTR[8+rdi],r9
	cmovz	r11,r12
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_sub::
ecp_nistz256_sub	ENDP



PUBLIC	ecp_nistz256_neg

ALIGN	32
ecp_nistz256_neg	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_neg::
	mov	rdi,rcx
	mov	rsi,rdx


	push	r12
	push	r13

	xor	r8,r8
	xor	r9,r9
	xor	r10,r10
	xor	r11,r11
	xor	r13,r13

	sub	r8,QWORD PTR[rsi]
	sbb	r9,QWORD PTR[8+rsi]
	sbb	r10,QWORD PTR[16+rsi]
	mov	rax,r8
	sbb	r11,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[$L$poly]
	mov	rdx,r9
	sbb	r13,0

	add	r8,QWORD PTR[rsi]
	mov	rcx,r10
	adc	r9,QWORD PTR[8+rsi]
	adc	r10,QWORD PTR[16+rsi]
	mov	r12,r11
	adc	r11,QWORD PTR[24+rsi]
	test	r13,r13

	cmovz	r8,rax
	cmovz	r9,rdx
	mov	QWORD PTR[rdi],r8
	cmovz	r10,rcx
	mov	QWORD PTR[8+rdi],r9
	cmovz	r11,r12
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_neg::
ecp_nistz256_neg	ENDP




PUBLIC	ecp_nistz256_to_mont

ALIGN	32
ecp_nistz256_to_mont	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_to_mont::
	mov	rdi,rcx
	mov	rsi,rdx


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	lea	rdx,QWORD PTR[$L$RR]
	jmp	$L$mul_mont
$L$SEH_end_ecp_nistz256_to_mont::
ecp_nistz256_to_mont	ENDP







PUBLIC	ecp_nistz256_mul_mont

ALIGN	32
ecp_nistz256_mul_mont	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_mul_mont::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
$L$mul_mont::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	cmp	ecx,080100h
	je	$L$mul_montx
	mov	rbx,rdx
	mov	rax,QWORD PTR[rdx]
	mov	r9,QWORD PTR[rsi]
	mov	r10,QWORD PTR[8+rsi]
	mov	r11,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]

	call	__ecp_nistz256_mul_montq
	jmp	$L$mul_mont_done

ALIGN	32
$L$mul_montx::
	mov	rbx,rdx
	mov	rdx,QWORD PTR[rdx]
	mov	r9,QWORD PTR[rsi]
	mov	r10,QWORD PTR[8+rsi]
	mov	r11,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[((-128))+rsi]

	call	__ecp_nistz256_mul_montx
$L$mul_mont_done::
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_mul_mont::
ecp_nistz256_mul_mont	ENDP


ALIGN	32
__ecp_nistz256_mul_montq	PROC PRIVATE


	mov	rbp,rax
	mul	r9
	mov	r14,QWORD PTR[(($L$poly+8))]
	mov	r8,rax
	mov	rax,rbp
	mov	r9,rdx

	mul	r10
	mov	r15,QWORD PTR[(($L$poly+24))]
	add	r9,rax
	mov	rax,rbp
	adc	rdx,0
	mov	r10,rdx

	mul	r11
	add	r10,rax
	mov	rax,rbp
	adc	rdx,0
	mov	r11,rdx

	mul	r12
	add	r11,rax
	mov	rax,r8
	adc	rdx,0
	xor	r13,r13
	mov	r12,rdx










	mov	rbp,r8
	shl	r8,32
	mul	r15
	shr	rbp,32
	add	r9,r8
	adc	r10,rbp
	adc	r11,rax
	mov	rax,QWORD PTR[8+rbx]
	adc	r12,rdx
	adc	r13,0
	xor	r8,r8



	mov	rbp,rax
	mul	QWORD PTR[rsi]
	add	r9,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[8+rsi]
	add	r10,rcx
	adc	rdx,0
	add	r10,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[16+rsi]
	add	r11,rcx
	adc	rdx,0
	add	r11,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[24+rsi]
	add	r12,rcx
	adc	rdx,0
	add	r12,rax
	mov	rax,r9
	adc	r13,rdx
	adc	r8,0



	mov	rbp,r9
	shl	r9,32
	mul	r15
	shr	rbp,32
	add	r10,r9
	adc	r11,rbp
	adc	r12,rax
	mov	rax,QWORD PTR[16+rbx]
	adc	r13,rdx
	adc	r8,0
	xor	r9,r9



	mov	rbp,rax
	mul	QWORD PTR[rsi]
	add	r10,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[8+rsi]
	add	r11,rcx
	adc	rdx,0
	add	r11,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[16+rsi]
	add	r12,rcx
	adc	rdx,0
	add	r12,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[24+rsi]
	add	r13,rcx
	adc	rdx,0
	add	r13,rax
	mov	rax,r10
	adc	r8,rdx
	adc	r9,0



	mov	rbp,r10
	shl	r10,32
	mul	r15
	shr	rbp,32
	add	r11,r10
	adc	r12,rbp
	adc	r13,rax
	mov	rax,QWORD PTR[24+rbx]
	adc	r8,rdx
	adc	r9,0
	xor	r10,r10



	mov	rbp,rax
	mul	QWORD PTR[rsi]
	add	r11,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[8+rsi]
	add	r12,rcx
	adc	rdx,0
	add	r12,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[16+rsi]
	add	r13,rcx
	adc	rdx,0
	add	r13,rax
	mov	rax,rbp
	adc	rdx,0
	mov	rcx,rdx

	mul	QWORD PTR[24+rsi]
	add	r8,rcx
	adc	rdx,0
	add	r8,rax
	mov	rax,r11
	adc	r9,rdx
	adc	r10,0



	mov	rbp,r11
	shl	r11,32
	mul	r15
	shr	rbp,32
	add	r12,r11
	adc	r13,rbp
	mov	rcx,r12
	adc	r8,rax
	adc	r9,rdx
	mov	rbp,r13
	adc	r10,0



	sub	r12,-1
	mov	rbx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	rdx,r9
	sbb	r9,r15
	sbb	r10,0

	cmovc	r12,rcx
	cmovc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rbx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,rdx
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_mul_montq	ENDP








PUBLIC	ecp_nistz256_sqr_mont

ALIGN	32
ecp_nistz256_sqr_mont	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_sqr_mont::
	mov	rdi,rcx
	mov	rsi,rdx


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	cmp	ecx,080100h
	je	$L$sqr_montx
	mov	rax,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r15,QWORD PTR[16+rsi]
	mov	r8,QWORD PTR[24+rsi]

	call	__ecp_nistz256_sqr_montq
	jmp	$L$sqr_mont_done

ALIGN	32
$L$sqr_montx::
	mov	rdx,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r15,QWORD PTR[16+rsi]
	mov	r8,QWORD PTR[24+rsi]
	lea	rsi,QWORD PTR[((-128))+rsi]

	call	__ecp_nistz256_sqr_montx
$L$sqr_mont_done::
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_sqr_mont::
ecp_nistz256_sqr_mont	ENDP


ALIGN	32
__ecp_nistz256_sqr_montq	PROC PRIVATE
	mov	r13,rax
	mul	r14
	mov	r9,rax
	mov	rax,r15
	mov	r10,rdx

	mul	r13
	add	r10,rax
	mov	rax,r8
	adc	rdx,0
	mov	r11,rdx

	mul	r13
	add	r11,rax
	mov	rax,r15
	adc	rdx,0
	mov	r12,rdx


	mul	r14
	add	r11,rax
	mov	rax,r8
	adc	rdx,0
	mov	rbp,rdx

	mul	r14
	add	r12,rax
	mov	rax,r8
	adc	rdx,0
	add	r12,rbp
	mov	r13,rdx
	adc	r13,0


	mul	r15
	xor	r15,r15
	add	r13,rax
	mov	rax,QWORD PTR[rsi]
	mov	r14,rdx
	adc	r14,0

	add	r9,r9
	adc	r10,r10
	adc	r11,r11
	adc	r12,r12
	adc	r13,r13
	adc	r14,r14
	adc	r15,0

	mul	rax
	mov	r8,rax
	mov	rax,QWORD PTR[8+rsi]
	mov	rcx,rdx

	mul	rax
	add	r9,rcx
	adc	r10,rax
	mov	rax,QWORD PTR[16+rsi]
	adc	rdx,0
	mov	rcx,rdx

	mul	rax
	add	r11,rcx
	adc	r12,rax
	mov	rax,QWORD PTR[24+rsi]
	adc	rdx,0
	mov	rcx,rdx

	mul	rax
	add	r13,rcx
	adc	r14,rax
	mov	rax,r8
	adc	r15,rdx

	mov	rsi,QWORD PTR[(($L$poly+8))]
	mov	rbp,QWORD PTR[(($L$poly+24))]




	mov	rcx,r8
	shl	r8,32
	mul	rbp
	shr	rcx,32
	add	r9,r8
	adc	r10,rcx
	adc	r11,rax
	mov	rax,r9
	adc	rdx,0



	mov	rcx,r9
	shl	r9,32
	mov	r8,rdx
	mul	rbp
	shr	rcx,32
	add	r10,r9
	adc	r11,rcx
	adc	r8,rax
	mov	rax,r10
	adc	rdx,0



	mov	rcx,r10
	shl	r10,32
	mov	r9,rdx
	mul	rbp
	shr	rcx,32
	add	r11,r10
	adc	r8,rcx
	adc	r9,rax
	mov	rax,r11
	adc	rdx,0



	mov	rcx,r11
	shl	r11,32
	mov	r10,rdx
	mul	rbp
	shr	rcx,32
	add	r8,r11
	adc	r9,rcx
	adc	r10,rax
	adc	rdx,0
	xor	r11,r11



	add	r12,r8
	adc	r13,r9
	mov	r8,r12
	adc	r14,r10
	adc	r15,rdx
	mov	r9,r13
	adc	r11,0

	sub	r12,-1
	mov	r10,r14
	sbb	r13,rsi
	sbb	r14,0
	mov	rcx,r15
	sbb	r15,rbp
	sbb	r11,0

	cmovc	r12,r8
	cmovc	r13,r9
	mov	QWORD PTR[rdi],r12
	cmovc	r14,r10
	mov	QWORD PTR[8+rdi],r13
	cmovc	r15,rcx
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15

	DB	0F3h,0C3h		;repret
__ecp_nistz256_sqr_montq	ENDP

ALIGN	32
__ecp_nistz256_mul_montx	PROC PRIVATE


	mulx	r9,r8,r9
	mulx	r10,rcx,r10
	mov	r14,32
	xor	r13,r13
	mulx	r11,rbp,r11
	mov	r15,QWORD PTR[(($L$poly+24))]
	adc	r9,rcx
	mulx	r12,rcx,r12
	mov	rdx,r8
	adc	r10,rbp
	shlx	rbp,r8,r14
	adc	r11,rcx
	shrx	rcx,r8,r14
	adc	r12,0



	add	r9,rbp
	adc	r10,rcx

	mulx	rbp,rcx,r15
	mov	rdx,QWORD PTR[8+rbx]
	adc	r11,rcx
	adc	r12,rbp
	adc	r13,0
	xor	r8,r8



	mulx	rbp,rcx,QWORD PTR[((0+128))+rsi]
	adcx	r9,rcx
	adox	r10,rbp

	mulx	rbp,rcx,QWORD PTR[((8+128))+rsi]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD PTR[((16+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD PTR[((24+128))+rsi]
	mov	rdx,r9
	adcx	r12,rcx
	shlx	rcx,r9,r14
	adox	r13,rbp
	shrx	rbp,r9,r14

	adcx	r13,r8
	adox	r8,r8
	adc	r8,0



	add	r10,rcx
	adc	r11,rbp

	mulx	rbp,rcx,r15
	mov	rdx,QWORD PTR[16+rbx]
	adc	r12,rcx
	adc	r13,rbp
	adc	r8,0
	xor	r9,r9



	mulx	rbp,rcx,QWORD PTR[((0+128))+rsi]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD PTR[((8+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD PTR[((16+128))+rsi]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD PTR[((24+128))+rsi]
	mov	rdx,r10
	adcx	r13,rcx
	shlx	rcx,r10,r14
	adox	r8,rbp
	shrx	rbp,r10,r14

	adcx	r8,r9
	adox	r9,r9
	adc	r9,0



	add	r11,rcx
	adc	r12,rbp

	mulx	rbp,rcx,r15
	mov	rdx,QWORD PTR[24+rbx]
	adc	r13,rcx
	adc	r8,rbp
	adc	r9,0
	xor	r10,r10



	mulx	rbp,rcx,QWORD PTR[((0+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD PTR[((8+128))+rsi]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD PTR[((16+128))+rsi]
	adcx	r13,rcx
	adox	r8,rbp

	mulx	rbp,rcx,QWORD PTR[((24+128))+rsi]
	mov	rdx,r11
	adcx	r8,rcx
	shlx	rcx,r11,r14
	adox	r9,rbp
	shrx	rbp,r11,r14

	adcx	r9,r10
	adox	r10,r10
	adc	r10,0



	add	r12,rcx
	adc	r13,rbp

	mulx	rbp,rcx,r15
	mov	rbx,r12
	mov	r14,QWORD PTR[(($L$poly+8))]
	adc	r8,rcx
	mov	rdx,r13
	adc	r9,rbp
	adc	r10,0



	xor	eax,eax
	mov	rcx,r8
	sbb	r12,-1
	sbb	r13,r14
	sbb	r8,0
	mov	rbp,r9
	sbb	r9,r15
	sbb	r10,0

	cmovc	r12,rbx
	cmovc	r13,rdx
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,rbp
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_mul_montx	ENDP


ALIGN	32
__ecp_nistz256_sqr_montx	PROC PRIVATE
	mulx	r10,r9,r14
	mulx	r11,rcx,r15
	xor	eax,eax
	adc	r10,rcx
	mulx	r12,rbp,r8
	mov	rdx,r14
	adc	r11,rbp
	adc	r12,0
	xor	r13,r13


	mulx	rbp,rcx,r15
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,r8
	mov	rdx,r15
	adcx	r12,rcx
	adox	r13,rbp
	adc	r13,0


	mulx	r14,rcx,r8
	mov	rdx,QWORD PTR[((0+128))+rsi]
	xor	r15,r15
	adcx	r9,r9
	adox	r13,rcx
	adcx	r10,r10
	adox	r14,r15

	mulx	rbp,r8,rdx
	mov	rdx,QWORD PTR[((8+128))+rsi]
	adcx	r11,r11
	adox	r9,rbp
	adcx	r12,r12
	mulx	rax,rcx,rdx
	mov	rdx,QWORD PTR[((16+128))+rsi]
	adcx	r13,r13
	adox	r10,rcx
	adcx	r14,r14
DB	067h
	mulx	rbp,rcx,rdx
	mov	rdx,QWORD PTR[((24+128))+rsi]
	adox	r11,rax
	adcx	r15,r15
	adox	r12,rcx
	mov	rsi,32
	adox	r13,rbp
DB	067h,067h
	mulx	rax,rcx,rdx
	mov	rdx,r8
	adox	r14,rcx
	shlx	rcx,r8,rsi
	adox	r15,rax
	shrx	rax,r8,rsi
	mov	rbp,QWORD PTR[(($L$poly+24))]


	add	r9,rcx
	adc	r10,rax

	mulx	r8,rcx,rbp
	mov	rdx,r9
	adc	r11,rcx
	shlx	rcx,r9,rsi
	adc	r8,0
	shrx	rax,r9,rsi


	add	r10,rcx
	adc	r11,rax

	mulx	r9,rcx,rbp
	mov	rdx,r10
	adc	r8,rcx
	shlx	rcx,r10,rsi
	adc	r9,0
	shrx	rax,r10,rsi


	add	r11,rcx
	adc	r8,rax

	mulx	r10,rcx,rbp
	mov	rdx,r11
	adc	r9,rcx
	shlx	rcx,r11,rsi
	adc	r10,0
	shrx	rax,r11,rsi


	add	r8,rcx
	adc	r9,rax

	mulx	r11,rcx,rbp
	adc	r10,rcx
	adc	r11,0

	xor	rdx,rdx
	adc	r12,r8
	mov	rsi,QWORD PTR[(($L$poly+8))]
	adc	r13,r9
	mov	r8,r12
	adc	r14,r10
	adc	r15,r11
	mov	r9,r13
	adc	rdx,0

	xor	eax,eax
	sbb	r12,-1
	mov	r10,r14
	sbb	r13,rsi
	sbb	r14,0
	mov	r11,r15
	sbb	r15,rbp
	sbb	rdx,0

	cmovc	r12,r8
	cmovc	r13,r9
	mov	QWORD PTR[rdi],r12
	cmovc	r14,r10
	mov	QWORD PTR[8+rdi],r13
	cmovc	r15,r11
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15

	DB	0F3h,0C3h		;repret
__ecp_nistz256_sqr_montx	ENDP






PUBLIC	ecp_nistz256_from_mont

ALIGN	32
ecp_nistz256_from_mont	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_from_mont::
	mov	rdi,rcx
	mov	rsi,rdx


	push	r12
	push	r13

	mov	rax,QWORD PTR[rsi]
	mov	r13,QWORD PTR[(($L$poly+24))]
	mov	r9,QWORD PTR[8+rsi]
	mov	r10,QWORD PTR[16+rsi]
	mov	r11,QWORD PTR[24+rsi]
	mov	r8,rax
	mov	r12,QWORD PTR[(($L$poly+8))]



	mov	rcx,rax
	shl	r8,32
	mul	r13
	shr	rcx,32
	add	r9,r8
	adc	r10,rcx
	adc	r11,rax
	mov	rax,r9
	adc	rdx,0



	mov	rcx,r9
	shl	r9,32
	mov	r8,rdx
	mul	r13
	shr	rcx,32
	add	r10,r9
	adc	r11,rcx
	adc	r8,rax
	mov	rax,r10
	adc	rdx,0



	mov	rcx,r10
	shl	r10,32
	mov	r9,rdx
	mul	r13
	shr	rcx,32
	add	r11,r10
	adc	r8,rcx
	adc	r9,rax
	mov	rax,r11
	adc	rdx,0



	mov	rcx,r11
	shl	r11,32
	mov	r10,rdx
	mul	r13
	shr	rcx,32
	add	r8,r11
	adc	r9,rcx
	mov	rcx,r8
	adc	r10,rax
	mov	rsi,r9
	adc	rdx,0



	sub	r8,-1
	mov	rax,r10
	sbb	r9,r12
	sbb	r10,0
	mov	r11,rdx
	sbb	rdx,r13
	sbb	r13,r13

	cmovnz	r8,rcx
	cmovnz	r9,rsi
	mov	QWORD PTR[rdi],r8
	cmovnz	r10,rax
	mov	QWORD PTR[8+rdi],r9
	cmovz	r11,rdx
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11

	pop	r13
	pop	r12
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_from_mont::
ecp_nistz256_from_mont	ENDP


PUBLIC	ecp_nistz256_select_w5

ALIGN	32
ecp_nistz256_select_w5	PROC PUBLIC
	mov	eax,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	test	eax,32
	jnz	$L$avx2_select_w5
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_ecp_nistz256_select_w5::
DB	048h,08dh,060h,0e0h
DB	00fh,029h,070h,0e0h
DB	00fh,029h,078h,0f0h
DB	044h,00fh,029h,000h
DB	044h,00fh,029h,048h,010h
DB	044h,00fh,029h,050h,020h
DB	044h,00fh,029h,058h,030h
DB	044h,00fh,029h,060h,040h
DB	044h,00fh,029h,068h,050h
DB	044h,00fh,029h,070h,060h
DB	044h,00fh,029h,078h,070h
	movdqa	xmm0,XMMWORD PTR[$L$One]
	movd	xmm1,r8d

	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7

	movdqa	xmm8,xmm0
	pshufd	xmm1,xmm1,0

	mov	rax,16
$L$select_loop_sse_w5::

	movdqa	xmm15,xmm8
	paddd	xmm8,xmm0
	pcmpeqd	xmm15,xmm1

	movdqa	xmm9,XMMWORD PTR[rdx]
	movdqa	xmm10,XMMWORD PTR[16+rdx]
	movdqa	xmm11,XMMWORD PTR[32+rdx]
	movdqa	xmm12,XMMWORD PTR[48+rdx]
	movdqa	xmm13,XMMWORD PTR[64+rdx]
	movdqa	xmm14,XMMWORD PTR[80+rdx]
	lea	rdx,QWORD PTR[96+rdx]

	pand	xmm9,xmm15
	pand	xmm10,xmm15
	por	xmm2,xmm9
	pand	xmm11,xmm15
	por	xmm3,xmm10
	pand	xmm12,xmm15
	por	xmm4,xmm11
	pand	xmm13,xmm15
	por	xmm5,xmm12
	pand	xmm14,xmm15
	por	xmm6,xmm13
	por	xmm7,xmm14

	dec	rax
	jnz	$L$select_loop_sse_w5

	movdqu	XMMWORD PTR[rcx],xmm2
	movdqu	XMMWORD PTR[16+rcx],xmm3
	movdqu	XMMWORD PTR[32+rcx],xmm4
	movdqu	XMMWORD PTR[48+rcx],xmm5
	movdqu	XMMWORD PTR[64+rcx],xmm6
	movdqu	XMMWORD PTR[80+rcx],xmm7
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_ecp_nistz256_select_w5::
	DB	0F3h,0C3h		;repret
ecp_nistz256_select_w5	ENDP



PUBLIC	ecp_nistz256_select_w7

ALIGN	32
ecp_nistz256_select_w7	PROC PUBLIC
	mov	eax,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	test	eax,32
	jnz	$L$avx2_select_w7
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_ecp_nistz256_select_w7::
DB	048h,08dh,060h,0e0h
DB	00fh,029h,070h,0e0h
DB	00fh,029h,078h,0f0h
DB	044h,00fh,029h,000h
DB	044h,00fh,029h,048h,010h
DB	044h,00fh,029h,050h,020h
DB	044h,00fh,029h,058h,030h
DB	044h,00fh,029h,060h,040h
DB	044h,00fh,029h,068h,050h
DB	044h,00fh,029h,070h,060h
DB	044h,00fh,029h,078h,070h
	movdqa	xmm8,XMMWORD PTR[$L$One]
	movd	xmm1,r8d

	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5

	movdqa	xmm0,xmm8
	pshufd	xmm1,xmm1,0
	mov	rax,64

$L$select_loop_sse_w7::
	movdqa	xmm15,xmm8
	paddd	xmm8,xmm0
	movdqa	xmm9,XMMWORD PTR[rdx]
	movdqa	xmm10,XMMWORD PTR[16+rdx]
	pcmpeqd	xmm15,xmm1
	movdqa	xmm11,XMMWORD PTR[32+rdx]
	movdqa	xmm12,XMMWORD PTR[48+rdx]
	lea	rdx,QWORD PTR[64+rdx]

	pand	xmm9,xmm15
	pand	xmm10,xmm15
	por	xmm2,xmm9
	pand	xmm11,xmm15
	por	xmm3,xmm10
	pand	xmm12,xmm15
	por	xmm4,xmm11
	prefetcht0	[255+rdx]
	por	xmm5,xmm12

	dec	rax
	jnz	$L$select_loop_sse_w7

	movdqu	XMMWORD PTR[rcx],xmm2
	movdqu	XMMWORD PTR[16+rcx],xmm3
	movdqu	XMMWORD PTR[32+rcx],xmm4
	movdqu	XMMWORD PTR[48+rcx],xmm5
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_ecp_nistz256_select_w7::
	DB	0F3h,0C3h		;repret
ecp_nistz256_select_w7	ENDP



ALIGN	32
ecp_nistz256_avx2_select_w5	PROC PRIVATE
$L$avx2_select_w5::
	vzeroupper
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_ecp_nistz256_avx2_select_w5::
DB	048h,08dh,060h,0e0h
DB	0c5h,0f8h,029h,070h,0e0h
DB	0c5h,0f8h,029h,078h,0f0h
DB	0c5h,078h,029h,040h,000h
DB	0c5h,078h,029h,048h,010h
DB	0c5h,078h,029h,050h,020h
DB	0c5h,078h,029h,058h,030h
DB	0c5h,078h,029h,060h,040h
DB	0c5h,078h,029h,068h,050h
DB	0c5h,078h,029h,070h,060h
DB	0c5h,078h,029h,078h,070h
	vmovdqa	ymm0,YMMWORD PTR[$L$Two]

	vpxor	ymm2,ymm2,ymm2
	vpxor	ymm3,ymm3,ymm3
	vpxor	ymm4,ymm4,ymm4

	vmovdqa	ymm5,YMMWORD PTR[$L$One]
	vmovdqa	ymm10,YMMWORD PTR[$L$Two]

	vmovd	xmm1,r8d
	vpermd	ymm1,ymm2,ymm1

	mov	rax,8
$L$select_loop_avx2_w5::

	vmovdqa	ymm6,YMMWORD PTR[rdx]
	vmovdqa	ymm7,YMMWORD PTR[32+rdx]
	vmovdqa	ymm8,YMMWORD PTR[64+rdx]

	vmovdqa	ymm11,YMMWORD PTR[96+rdx]
	vmovdqa	ymm12,YMMWORD PTR[128+rdx]
	vmovdqa	ymm13,YMMWORD PTR[160+rdx]

	vpcmpeqd	ymm9,ymm5,ymm1
	vpcmpeqd	ymm14,ymm10,ymm1

	vpaddd	ymm5,ymm5,ymm0
	vpaddd	ymm10,ymm10,ymm0
	lea	rdx,QWORD PTR[192+rdx]

	vpand	ymm6,ymm6,ymm9
	vpand	ymm7,ymm7,ymm9
	vpand	ymm8,ymm8,ymm9
	vpand	ymm11,ymm11,ymm14
	vpand	ymm12,ymm12,ymm14
	vpand	ymm13,ymm13,ymm14

	vpxor	ymm2,ymm2,ymm6
	vpxor	ymm3,ymm3,ymm7
	vpxor	ymm4,ymm4,ymm8
	vpxor	ymm2,ymm2,ymm11
	vpxor	ymm3,ymm3,ymm12
	vpxor	ymm4,ymm4,ymm13

	dec	rax
	jnz	$L$select_loop_avx2_w5

	vmovdqu	YMMWORD PTR[rcx],ymm2
	vmovdqu	YMMWORD PTR[32+rcx],ymm3
	vmovdqu	YMMWORD PTR[64+rcx],ymm4
	vzeroupper
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_ecp_nistz256_avx2_select_w5::
	DB	0F3h,0C3h		;repret
ecp_nistz256_avx2_select_w5	ENDP



PUBLIC	ecp_nistz256_avx2_select_w7

ALIGN	32
ecp_nistz256_avx2_select_w7	PROC PUBLIC
$L$avx2_select_w7::
	vzeroupper
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_ecp_nistz256_avx2_select_w7::
DB	048h,08dh,060h,0e0h
DB	0c5h,0f8h,029h,070h,0e0h
DB	0c5h,0f8h,029h,078h,0f0h
DB	0c5h,078h,029h,040h,000h
DB	0c5h,078h,029h,048h,010h
DB	0c5h,078h,029h,050h,020h
DB	0c5h,078h,029h,058h,030h
DB	0c5h,078h,029h,060h,040h
DB	0c5h,078h,029h,068h,050h
DB	0c5h,078h,029h,070h,060h
DB	0c5h,078h,029h,078h,070h
	vmovdqa	ymm0,YMMWORD PTR[$L$Three]

	vpxor	ymm2,ymm2,ymm2
	vpxor	ymm3,ymm3,ymm3

	vmovdqa	ymm4,YMMWORD PTR[$L$One]
	vmovdqa	ymm8,YMMWORD PTR[$L$Two]
	vmovdqa	ymm12,YMMWORD PTR[$L$Three]

	vmovd	xmm1,r8d
	vpermd	ymm1,ymm2,ymm1


	mov	rax,21
$L$select_loop_avx2_w7::

	vmovdqa	ymm5,YMMWORD PTR[rdx]
	vmovdqa	ymm6,YMMWORD PTR[32+rdx]

	vmovdqa	ymm9,YMMWORD PTR[64+rdx]
	vmovdqa	ymm10,YMMWORD PTR[96+rdx]

	vmovdqa	ymm13,YMMWORD PTR[128+rdx]
	vmovdqa	ymm14,YMMWORD PTR[160+rdx]

	vpcmpeqd	ymm7,ymm4,ymm1
	vpcmpeqd	ymm11,ymm8,ymm1
	vpcmpeqd	ymm15,ymm12,ymm1

	vpaddd	ymm4,ymm4,ymm0
	vpaddd	ymm8,ymm8,ymm0
	vpaddd	ymm12,ymm12,ymm0
	lea	rdx,QWORD PTR[192+rdx]

	vpand	ymm5,ymm5,ymm7
	vpand	ymm6,ymm6,ymm7
	vpand	ymm9,ymm9,ymm11
	vpand	ymm10,ymm10,ymm11
	vpand	ymm13,ymm13,ymm15
	vpand	ymm14,ymm14,ymm15

	vpxor	ymm2,ymm2,ymm5
	vpxor	ymm3,ymm3,ymm6
	vpxor	ymm2,ymm2,ymm9
	vpxor	ymm3,ymm3,ymm10
	vpxor	ymm2,ymm2,ymm13
	vpxor	ymm3,ymm3,ymm14

	dec	rax
	jnz	$L$select_loop_avx2_w7


	vmovdqa	ymm5,YMMWORD PTR[rdx]
	vmovdqa	ymm6,YMMWORD PTR[32+rdx]

	vpcmpeqd	ymm7,ymm4,ymm1

	vpand	ymm5,ymm5,ymm7
	vpand	ymm6,ymm6,ymm7

	vpxor	ymm2,ymm2,ymm5
	vpxor	ymm3,ymm3,ymm6

	vmovdqu	YMMWORD PTR[rcx],ymm2
	vmovdqu	YMMWORD PTR[32+rcx],ymm3
	vzeroupper
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_ecp_nistz256_avx2_select_w7::
	DB	0F3h,0C3h		;repret
ecp_nistz256_avx2_select_w7	ENDP

ALIGN	32
__ecp_nistz256_add_toq	PROC PRIVATE
	xor	r11,r11
	add	r12,QWORD PTR[rbx]
	adc	r13,QWORD PTR[8+rbx]
	mov	rax,r12
	adc	r8,QWORD PTR[16+rbx]
	adc	r9,QWORD PTR[24+rbx]
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	cmovc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_add_toq	ENDP


ALIGN	32
__ecp_nistz256_sub_fromq	PROC PRIVATE
	sub	r12,QWORD PTR[rbx]
	sbb	r13,QWORD PTR[8+rbx]
	mov	rax,r12
	sbb	r8,QWORD PTR[16+rbx]
	sbb	r9,QWORD PTR[24+rbx]
	mov	rbp,r13
	sbb	r11,r11

	add	r12,-1
	mov	rcx,r8
	adc	r13,r14
	adc	r8,0
	mov	r10,r9
	adc	r9,r15
	test	r11,r11

	cmovz	r12,rax
	cmovz	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovz	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovz	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_sub_fromq	ENDP


ALIGN	32
__ecp_nistz256_subq	PROC PRIVATE
	sub	rax,r12
	sbb	rbp,r13
	mov	r12,rax
	sbb	rcx,r8
	sbb	r10,r9
	mov	r13,rbp
	sbb	r11,r11

	add	rax,-1
	mov	r8,rcx
	adc	rbp,r14
	adc	rcx,0
	mov	r9,r10
	adc	r10,r15
	test	r11,r11

	cmovnz	r12,rax
	cmovnz	r13,rbp
	cmovnz	r8,rcx
	cmovnz	r9,r10

	DB	0F3h,0C3h		;repret
__ecp_nistz256_subq	ENDP


ALIGN	32
__ecp_nistz256_mul_by_2q	PROC PRIVATE
	xor	r11,r11
	add	r12,r12
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	cmovc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_mul_by_2q	ENDP
PUBLIC	ecp_nistz256_point_double

ALIGN	32
ecp_nistz256_point_double	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_double::
	mov	rdi,rcx
	mov	rsi,rdx


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	cmp	ecx,080100h
	je	$L$point_doublex
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*5+8

$L$point_double_shortcutq::
	movdqu	xmm0,XMMWORD PTR[rsi]
	mov	rbx,rsi
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	mov	r12,QWORD PTR[((32+0))+rsi]
	mov	r13,QWORD PTR[((32+8))+rsi]
	mov	r8,QWORD PTR[((32+16))+rsi]
	mov	r9,QWORD PTR[((32+24))+rsi]
	mov	r14,QWORD PTR[(($L$poly+8))]
	mov	r15,QWORD PTR[(($L$poly+24))]
	movdqa	XMMWORD PTR[96+rsp],xmm0
	movdqa	XMMWORD PTR[(96+16)+rsp],xmm1
	lea	r10,QWORD PTR[32+rdi]
	lea	r11,QWORD PTR[64+rdi]
DB	102,72,15,110,199
DB	102,73,15,110,202
DB	102,73,15,110,211

	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_by_2q

	mov	rax,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	lea	rsi,QWORD PTR[((64-0))+rsi]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((0+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[32+rbx]
	mov	r9,QWORD PTR[((64+0))+rbx]
	mov	r10,QWORD PTR[((64+8))+rbx]
	mov	r11,QWORD PTR[((64+16))+rbx]
	mov	r12,QWORD PTR[((64+24))+rbx]
	lea	rsi,QWORD PTR[((64-0))+rbx]
	lea	rbx,QWORD PTR[32+rbx]
DB	102,72,15,126,215
	call	__ecp_nistz256_mul_montq
	call	__ecp_nistz256_mul_by_2q

	mov	r12,QWORD PTR[((96+0))+rsp]
	mov	r13,QWORD PTR[((96+8))+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r8,QWORD PTR[((96+16))+rsp]
	mov	r9,QWORD PTR[((96+24))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_add_toq

	mov	r12,QWORD PTR[((96+0))+rsp]
	mov	r13,QWORD PTR[((96+8))+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r8,QWORD PTR[((96+16))+rsp]
	mov	r9,QWORD PTR[((96+24))+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((0+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
DB	102,72,15,126,207
	call	__ecp_nistz256_sqr_montq
	xor	r9,r9
	mov	rax,r12
	add	r12,-1
	mov	r10,r13
	adc	r13,rsi
	mov	rcx,r14
	adc	r14,0
	mov	r8,r15
	adc	r15,rbp
	adc	r9,0
	xor	rsi,rsi
	test	rax,1

	cmovz	r12,rax
	cmovz	r13,r10
	cmovz	r14,rcx
	cmovz	r15,r8
	cmovz	r9,rsi

	mov	rax,r13
	shr	r12,1
	shl	rax,63
	mov	r10,r14
	shr	r13,1
	or	r12,rax
	shl	r10,63
	mov	rcx,r15
	shr	r14,1
	or	r13,r10
	shl	rcx,63
	mov	QWORD PTR[rdi],r12
	shr	r15,1
	mov	QWORD PTR[8+rdi],r13
	shl	r9,63
	or	r14,rcx
	or	r15,r9
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15
	mov	rax,QWORD PTR[64+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_by_2q

	lea	rbx,QWORD PTR[32+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_add_toq

	mov	rax,QWORD PTR[96+rsp]
	lea	rbx,QWORD PTR[96+rsp]
	mov	r9,QWORD PTR[((0+0))+rsp]
	mov	r10,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((0+0))+rsp]
	mov	r11,QWORD PTR[((16+0))+rsp]
	mov	r12,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montq

	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_by_2q

	mov	rax,QWORD PTR[((0+32))+rsp]
	mov	r14,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r15,QWORD PTR[((16+32))+rsp]
	mov	r8,QWORD PTR[((24+32))+rsp]
DB	102,72,15,126,199
	call	__ecp_nistz256_sqr_montq

	lea	rbx,QWORD PTR[128+rsp]
	mov	r8,r14
	mov	r9,r15
	mov	r14,rsi
	mov	r15,rbp
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	rbp,QWORD PTR[((0+8))+rsp]
	mov	rcx,QWORD PTR[((0+16))+rsp]
	mov	r10,QWORD PTR[((0+24))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_subq

	mov	rax,QWORD PTR[32+rsp]
	lea	rbx,QWORD PTR[32+rsp]
	mov	r14,r12
	xor	ecx,ecx
	mov	QWORD PTR[((0+0))+rsp],r12
	mov	r10,r13
	mov	QWORD PTR[((0+8))+rsp],r13
	cmovz	r11,r8
	mov	QWORD PTR[((0+16))+rsp],r8
	lea	rsi,QWORD PTR[((0-0))+rsp]
	cmovz	r12,r9
	mov	QWORD PTR[((0+24))+rsp],r9
	mov	r9,r14
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montq

DB	102,72,15,126,203
DB	102,72,15,126,207
	call	__ecp_nistz256_sub_fromq

	add	rsp,32*5+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_double::
ecp_nistz256_point_double	ENDP
PUBLIC	ecp_nistz256_point_add

ALIGN	32
ecp_nistz256_point_add	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_add::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	cmp	ecx,080100h
	je	$L$point_addx
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*18+8

	movdqu	xmm0,XMMWORD PTR[rsi]
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	movdqu	xmm4,XMMWORD PTR[64+rsi]
	movdqu	xmm5,XMMWORD PTR[80+rsi]
	mov	rbx,rsi
	mov	rsi,rdx
	movdqa	XMMWORD PTR[384+rsp],xmm0
	movdqa	XMMWORD PTR[(384+16)+rsp],xmm1
	movdqa	XMMWORD PTR[416+rsp],xmm2
	movdqa	XMMWORD PTR[(416+16)+rsp],xmm3
	movdqa	XMMWORD PTR[448+rsp],xmm4
	movdqa	XMMWORD PTR[(448+16)+rsp],xmm5
	por	xmm5,xmm4

	movdqu	xmm0,XMMWORD PTR[rsi]
	pshufd	xmm3,xmm5,1h
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	por	xmm5,xmm3
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	mov	rax,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	movdqa	XMMWORD PTR[480+rsp],xmm0
	pshufd	xmm4,xmm5,01eh
	movdqa	XMMWORD PTR[(480+16)+rsp],xmm1
	movdqu	xmm0,XMMWORD PTR[64+rsi]
	movdqu	xmm1,XMMWORD PTR[80+rsi]
	movdqa	XMMWORD PTR[512+rsp],xmm2
	movdqa	XMMWORD PTR[(512+16)+rsp],xmm3
	por	xmm5,xmm4
	pxor	xmm4,xmm4
	por	xmm1,xmm0
DB	102,72,15,110,199

	lea	rsi,QWORD PTR[((64-0))+rsi]
	mov	QWORD PTR[((544+0))+rsp],rax
	mov	QWORD PTR[((544+8))+rsp],r14
	mov	QWORD PTR[((544+16))+rsp],r15
	mov	QWORD PTR[((544+24))+rsp],r8
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sqr_montq

	pcmpeqd	xmm5,xmm4
	pshufd	xmm4,xmm1,1h
	por	xmm4,xmm1
	pshufd	xmm5,xmm5,0
	pshufd	xmm3,xmm4,01eh
	por	xmm4,xmm3
	pxor	xmm3,xmm3
	pcmpeqd	xmm4,xmm3
	pshufd	xmm4,xmm4,0
	mov	rax,QWORD PTR[((64+0))+rbx]
	mov	r14,QWORD PTR[((64+8))+rbx]
	mov	r15,QWORD PTR[((64+16))+rbx]
	mov	r8,QWORD PTR[((64+24))+rbx]
DB	102,72,15,110,203

	lea	rsi,QWORD PTR[((64-0))+rbx]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[544+rsp]
	lea	rbx,QWORD PTR[544+rsp]
	mov	r9,QWORD PTR[((0+96))+rsp]
	mov	r10,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((0+96))+rsp]
	mov	r11,QWORD PTR[((16+96))+rsp]
	mov	r12,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[416+rsp]
	lea	rbx,QWORD PTR[416+rsp]
	mov	r9,QWORD PTR[((0+224))+rsp]
	mov	r10,QWORD PTR[((8+224))+rsp]
	lea	rsi,QWORD PTR[((0+224))+rsp]
	mov	r11,QWORD PTR[((16+224))+rsp]
	mov	r12,QWORD PTR[((24+224))+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[512+rsp]
	lea	rbx,QWORD PTR[512+rsp]
	mov	r9,QWORD PTR[((0+256))+rsp]
	mov	r10,QWORD PTR[((8+256))+rsp]
	lea	rsi,QWORD PTR[((0+256))+rsp]
	mov	r11,QWORD PTR[((16+256))+rsp]
	mov	r12,QWORD PTR[((24+256))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[224+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromq

	or	r12,r13
	movdqa	xmm2,xmm4
	or	r12,r8
	or	r12,r9
	por	xmm2,xmm5
DB	102,73,15,110,220

	mov	rax,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+96))+rsp]
	mov	r10,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((0+96))+rsp]
	mov	r11,QWORD PTR[((16+96))+rsp]
	mov	r12,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[160+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[480+rsp]
	lea	rbx,QWORD PTR[480+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[160+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_sub_fromq

	or	r12,r13
	or	r12,r8
	or	r12,r9

DB	03eh
	jnz	$L$add_proceedq
DB	102,73,15,126,208
DB	102,73,15,126,217
	test	r8,r8
	jnz	$L$add_proceedq
	test	r9,r9
	jz	$L$add_doubleq

DB	102,72,15,126,199
	pxor	xmm0,xmm0
	movdqu	XMMWORD PTR[rdi],xmm0
	movdqu	XMMWORD PTR[16+rdi],xmm0
	movdqu	XMMWORD PTR[32+rdi],xmm0
	movdqu	XMMWORD PTR[48+rdi],xmm0
	movdqu	XMMWORD PTR[64+rdi],xmm0
	movdqu	XMMWORD PTR[80+rdi],xmm0
	jmp	$L$add_doneq

ALIGN	32
$L$add_doubleq::
DB	102,72,15,126,206
DB	102,72,15,126,199
	add	rsp,416
	jmp	$L$point_double_shortcutq

ALIGN	32
$L$add_proceedq::
	mov	rax,QWORD PTR[((0+64))+rsp]
	mov	r14,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r15,QWORD PTR[((16+64))+rsp]
	mov	r8,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+0))+rsp]
	mov	r10,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((0+0))+rsp]
	mov	r11,QWORD PTR[((16+0))+rsp]
	mov	r12,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[352+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((0+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[544+rsp]
	lea	rbx,QWORD PTR[544+rsp]
	mov	r9,QWORD PTR[((0+352))+rsp]
	mov	r10,QWORD PTR[((8+352))+rsp]
	lea	rsi,QWORD PTR[((0+352))+rsp]
	mov	r11,QWORD PTR[((16+352))+rsp]
	mov	r12,QWORD PTR[((24+352))+rsp]
	lea	rdi,QWORD PTR[352+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[rsp]
	lea	rbx,QWORD PTR[rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[160+rsp]
	lea	rbx,QWORD PTR[160+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_mul_montq




	xor	r11,r11
	add	r12,r12
	lea	rsi,QWORD PTR[96+rsp]
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	mov	rax,QWORD PTR[rsi]
	cmovc	r13,rbp
	mov	rbp,QWORD PTR[8+rsi]
	cmovc	r8,rcx
	mov	rcx,QWORD PTR[16+rsi]
	cmovc	r9,r10
	mov	r10,QWORD PTR[24+rsi]

	call	__ecp_nistz256_subq

	lea	rbx,QWORD PTR[128+rsp]
	lea	rdi,QWORD PTR[288+rsp]
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[((192+0))+rsp]
	mov	rbp,QWORD PTR[((192+8))+rsp]
	mov	rcx,QWORD PTR[((192+16))+rsp]
	mov	r10,QWORD PTR[((192+24))+rsp]
	lea	rdi,QWORD PTR[320+rsp]

	call	__ecp_nistz256_subq

	mov	QWORD PTR[rdi],r12
	mov	QWORD PTR[8+rdi],r13
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9
	mov	rax,QWORD PTR[128+rsp]
	lea	rbx,QWORD PTR[128+rsp]
	mov	r9,QWORD PTR[((0+224))+rsp]
	mov	r10,QWORD PTR[((8+224))+rsp]
	lea	rsi,QWORD PTR[((0+224))+rsp]
	mov	r11,QWORD PTR[((16+224))+rsp]
	mov	r12,QWORD PTR[((24+224))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[320+rsp]
	lea	rbx,QWORD PTR[320+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[320+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[256+rsp]
	lea	rdi,QWORD PTR[320+rsp]
	call	__ecp_nistz256_sub_fromq

DB	102,72,15,126,199

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[352+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((352+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[544+rsp]
	pand	xmm3,XMMWORD PTR[((544+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[448+rsp]
	pand	xmm3,XMMWORD PTR[((448+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[64+rdi],xmm2
	movdqu	XMMWORD PTR[80+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[288+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((288+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[480+rsp]
	pand	xmm3,XMMWORD PTR[((480+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[384+rsp]
	pand	xmm3,XMMWORD PTR[((384+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[rdi],xmm2
	movdqu	XMMWORD PTR[16+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[320+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((320+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[512+rsp]
	pand	xmm3,XMMWORD PTR[((512+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[416+rsp]
	pand	xmm3,XMMWORD PTR[((416+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[32+rdi],xmm2
	movdqu	XMMWORD PTR[48+rdi],xmm3

$L$add_doneq::
	add	rsp,32*18+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_add::
ecp_nistz256_point_add	ENDP
PUBLIC	ecp_nistz256_point_add_affine

ALIGN	32
ecp_nistz256_point_add_affine	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_add_affine::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	ecx,080100h
	and	ecx,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	cmp	ecx,080100h
	je	$L$point_add_affinex
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*15+8

	movdqu	xmm0,XMMWORD PTR[rsi]
	mov	rbx,rdx
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	movdqu	xmm4,XMMWORD PTR[64+rsi]
	movdqu	xmm5,XMMWORD PTR[80+rsi]
	mov	rax,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	movdqa	XMMWORD PTR[320+rsp],xmm0
	movdqa	XMMWORD PTR[(320+16)+rsp],xmm1
	movdqa	XMMWORD PTR[352+rsp],xmm2
	movdqa	XMMWORD PTR[(352+16)+rsp],xmm3
	movdqa	XMMWORD PTR[384+rsp],xmm4
	movdqa	XMMWORD PTR[(384+16)+rsp],xmm5
	por	xmm5,xmm4

	movdqu	xmm0,XMMWORD PTR[rbx]
	pshufd	xmm3,xmm5,1h
	movdqu	xmm1,XMMWORD PTR[16+rbx]
	movdqu	xmm2,XMMWORD PTR[32+rbx]
	por	xmm5,xmm3
	movdqu	xmm3,XMMWORD PTR[48+rbx]
	movdqa	XMMWORD PTR[416+rsp],xmm0
	pshufd	xmm4,xmm5,01eh
	movdqa	XMMWORD PTR[(416+16)+rsp],xmm1
	por	xmm1,xmm0
DB	102,72,15,110,199
	movdqa	XMMWORD PTR[448+rsp],xmm2
	movdqa	XMMWORD PTR[(448+16)+rsp],xmm3
	por	xmm3,xmm2
	por	xmm5,xmm4
	pxor	xmm4,xmm4
	por	xmm3,xmm1

	lea	rsi,QWORD PTR[((64-0))+rsi]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montq

	pcmpeqd	xmm5,xmm4
	pshufd	xmm4,xmm3,1h
	mov	rax,QWORD PTR[rbx]

	mov	r9,r12
	por	xmm4,xmm3
	pshufd	xmm5,xmm5,0
	pshufd	xmm3,xmm4,01eh
	mov	r10,r13
	por	xmm4,xmm3
	pxor	xmm3,xmm3
	mov	r11,r14
	pcmpeqd	xmm4,xmm3
	pshufd	xmm4,xmm4,0

	lea	rsi,QWORD PTR[((32-0))+rsp]
	mov	r12,r15
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[320+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[288+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((0+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[352+rsp]
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[((0+64))+rsp]
	mov	r14,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r15,QWORD PTR[((16+64))+rsp]
	mov	r8,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[((0+96))+rsp]
	mov	r14,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((0+96))+rsp]
	mov	r15,QWORD PTR[((16+96))+rsp]
	mov	r8,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_sqr_montq

	mov	rax,QWORD PTR[128+rsp]
	lea	rbx,QWORD PTR[128+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[160+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[320+rsp]
	lea	rbx,QWORD PTR[320+rsp]
	mov	r9,QWORD PTR[((0+128))+rsp]
	mov	r10,QWORD PTR[((8+128))+rsp]
	lea	rsi,QWORD PTR[((0+128))+rsp]
	mov	r11,QWORD PTR[((16+128))+rsp]
	mov	r12,QWORD PTR[((24+128))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montq




	xor	r11,r11
	add	r12,r12
	lea	rsi,QWORD PTR[192+rsp]
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	mov	rax,QWORD PTR[rsi]
	cmovc	r13,rbp
	mov	rbp,QWORD PTR[8+rsi]
	cmovc	r8,rcx
	mov	rcx,QWORD PTR[16+rsi]
	cmovc	r9,r10
	mov	r10,QWORD PTR[24+rsi]

	call	__ecp_nistz256_subq

	lea	rbx,QWORD PTR[160+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_sub_fromq

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	rbp,QWORD PTR[((0+8))+rsp]
	mov	rcx,QWORD PTR[((0+16))+rsp]
	mov	r10,QWORD PTR[((0+24))+rsp]
	lea	rdi,QWORD PTR[64+rsp]

	call	__ecp_nistz256_subq

	mov	QWORD PTR[rdi],r12
	mov	QWORD PTR[8+rdi],r13
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9
	mov	rax,QWORD PTR[352+rsp]
	lea	rbx,QWORD PTR[352+rsp]
	mov	r9,QWORD PTR[((0+160))+rsp]
	mov	r10,QWORD PTR[((8+160))+rsp]
	lea	rsi,QWORD PTR[((0+160))+rsp]
	mov	r11,QWORD PTR[((16+160))+rsp]
	mov	r12,QWORD PTR[((24+160))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montq

	mov	rax,QWORD PTR[96+rsp]
	lea	rbx,QWORD PTR[96+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((0+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_mul_montq

	lea	rbx,QWORD PTR[32+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_sub_fromq

DB	102,72,15,126,199

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[288+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((288+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[$L$ONE_mont]
	pand	xmm3,XMMWORD PTR[(($L$ONE_mont+16))]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[384+rsp]
	pand	xmm3,XMMWORD PTR[((384+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[64+rdi],xmm2
	movdqu	XMMWORD PTR[80+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[224+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((224+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[416+rsp]
	pand	xmm3,XMMWORD PTR[((416+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[320+rsp]
	pand	xmm3,XMMWORD PTR[((320+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[rdi],xmm2
	movdqu	XMMWORD PTR[16+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[256+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((256+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[448+rsp]
	pand	xmm3,XMMWORD PTR[((448+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[352+rsp]
	pand	xmm3,XMMWORD PTR[((352+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[32+rdi],xmm2
	movdqu	XMMWORD PTR[48+rdi],xmm3

	add	rsp,32*15+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_add_affine::
ecp_nistz256_point_add_affine	ENDP

ALIGN	32
__ecp_nistz256_add_tox	PROC PRIVATE
	xor	r11,r11
	adc	r12,QWORD PTR[rbx]
	adc	r13,QWORD PTR[8+rbx]
	mov	rax,r12
	adc	r8,QWORD PTR[16+rbx]
	adc	r9,QWORD PTR[24+rbx]
	mov	rbp,r13
	adc	r11,0

	xor	r10,r10
	sbb	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	cmovc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_add_tox	ENDP


ALIGN	32
__ecp_nistz256_sub_fromx	PROC PRIVATE
	xor	r11,r11
	sbb	r12,QWORD PTR[rbx]
	sbb	r13,QWORD PTR[8+rbx]
	mov	rax,r12
	sbb	r8,QWORD PTR[16+rbx]
	sbb	r9,QWORD PTR[24+rbx]
	mov	rbp,r13
	sbb	r11,0

	xor	r10,r10
	adc	r12,-1
	mov	rcx,r8
	adc	r13,r14
	adc	r8,0
	mov	r10,r9
	adc	r9,r15

	bt	r11,0
	cmovnc	r12,rax
	cmovnc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovnc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovnc	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_sub_fromx	ENDP


ALIGN	32
__ecp_nistz256_subx	PROC PRIVATE
	xor	r11,r11
	sbb	rax,r12
	sbb	rbp,r13
	mov	r12,rax
	sbb	rcx,r8
	sbb	r10,r9
	mov	r13,rbp
	sbb	r11,0

	xor	r9,r9
	adc	rax,-1
	mov	r8,rcx
	adc	rbp,r14
	adc	rcx,0
	mov	r9,r10
	adc	r10,r15

	bt	r11,0
	cmovc	r12,rax
	cmovc	r13,rbp
	cmovc	r8,rcx
	cmovc	r9,r10

	DB	0F3h,0C3h		;repret
__ecp_nistz256_subx	ENDP


ALIGN	32
__ecp_nistz256_mul_by_2x	PROC PRIVATE
	xor	r11,r11
	adc	r12,r12
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	xor	r10,r10
	sbb	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	cmovc	r13,rbp
	mov	QWORD PTR[rdi],r12
	cmovc	r8,rcx
	mov	QWORD PTR[8+rdi],r13
	cmovc	r9,r10
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9

	DB	0F3h,0C3h		;repret
__ecp_nistz256_mul_by_2x	ENDP

ALIGN	32
ecp_nistz256_point_doublex	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_doublex::
	mov	rdi,rcx
	mov	rsi,rdx


$L$point_doublex::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*5+8

$L$point_double_shortcutx::
	movdqu	xmm0,XMMWORD PTR[rsi]
	mov	rbx,rsi
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	mov	r12,QWORD PTR[((32+0))+rsi]
	mov	r13,QWORD PTR[((32+8))+rsi]
	mov	r8,QWORD PTR[((32+16))+rsi]
	mov	r9,QWORD PTR[((32+24))+rsi]
	mov	r14,QWORD PTR[(($L$poly+8))]
	mov	r15,QWORD PTR[(($L$poly+24))]
	movdqa	XMMWORD PTR[96+rsp],xmm0
	movdqa	XMMWORD PTR[(96+16)+rsp],xmm1
	lea	r10,QWORD PTR[32+rdi]
	lea	r11,QWORD PTR[64+rdi]
DB	102,72,15,110,199
DB	102,73,15,110,202
DB	102,73,15,110,211

	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_by_2x

	mov	rdx,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	lea	rsi,QWORD PTR[((64-128))+rsi]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((-128+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[32+rbx]
	mov	r9,QWORD PTR[((64+0))+rbx]
	mov	r10,QWORD PTR[((64+8))+rbx]
	mov	r11,QWORD PTR[((64+16))+rbx]
	mov	r12,QWORD PTR[((64+24))+rbx]
	lea	rsi,QWORD PTR[((64-128))+rbx]
	lea	rbx,QWORD PTR[32+rbx]
DB	102,72,15,126,215
	call	__ecp_nistz256_mul_montx
	call	__ecp_nistz256_mul_by_2x

	mov	r12,QWORD PTR[((96+0))+rsp]
	mov	r13,QWORD PTR[((96+8))+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r8,QWORD PTR[((96+16))+rsp]
	mov	r9,QWORD PTR[((96+24))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_add_tox

	mov	r12,QWORD PTR[((96+0))+rsp]
	mov	r13,QWORD PTR[((96+8))+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r8,QWORD PTR[((96+16))+rsp]
	mov	r9,QWORD PTR[((96+24))+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromx

	mov	rdx,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((-128+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
DB	102,72,15,126,207
	call	__ecp_nistz256_sqr_montx
	xor	r9,r9
	mov	rax,r12
	add	r12,-1
	mov	r10,r13
	adc	r13,rsi
	mov	rcx,r14
	adc	r14,0
	mov	r8,r15
	adc	r15,rbp
	adc	r9,0
	xor	rsi,rsi
	test	rax,1

	cmovz	r12,rax
	cmovz	r13,r10
	cmovz	r14,rcx
	cmovz	r15,r8
	cmovz	r9,rsi

	mov	rax,r13
	shr	r12,1
	shl	rax,63
	mov	r10,r14
	shr	r13,1
	or	r12,rax
	shl	r10,63
	mov	rcx,r15
	shr	r14,1
	or	r13,r10
	shl	rcx,63
	mov	QWORD PTR[rdi],r12
	shr	r15,1
	mov	QWORD PTR[8+rdi],r13
	shl	r9,63
	or	r14,rcx
	or	r15,r9
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15
	mov	rdx,QWORD PTR[64+rsp]
	lea	rbx,QWORD PTR[64+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_by_2x

	lea	rbx,QWORD PTR[32+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_add_tox

	mov	rdx,QWORD PTR[96+rsp]
	lea	rbx,QWORD PTR[96+rsp]
	mov	r9,QWORD PTR[((0+0))+rsp]
	mov	r10,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((-128+0))+rsp]
	mov	r11,QWORD PTR[((16+0))+rsp]
	mov	r12,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montx

	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_by_2x

	mov	rdx,QWORD PTR[((0+32))+rsp]
	mov	r14,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r15,QWORD PTR[((16+32))+rsp]
	mov	r8,QWORD PTR[((24+32))+rsp]
DB	102,72,15,126,199
	call	__ecp_nistz256_sqr_montx

	lea	rbx,QWORD PTR[128+rsp]
	mov	r8,r14
	mov	r9,r15
	mov	r14,rsi
	mov	r15,rbp
	call	__ecp_nistz256_sub_fromx

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	rbp,QWORD PTR[((0+8))+rsp]
	mov	rcx,QWORD PTR[((0+16))+rsp]
	mov	r10,QWORD PTR[((0+24))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_subx

	mov	rdx,QWORD PTR[32+rsp]
	lea	rbx,QWORD PTR[32+rsp]
	mov	r14,r12
	xor	ecx,ecx
	mov	QWORD PTR[((0+0))+rsp],r12
	mov	r10,r13
	mov	QWORD PTR[((0+8))+rsp],r13
	cmovz	r11,r8
	mov	QWORD PTR[((0+16))+rsp],r8
	lea	rsi,QWORD PTR[((0-128))+rsp]
	cmovz	r12,r9
	mov	QWORD PTR[((0+24))+rsp],r9
	mov	r9,r14
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montx

DB	102,72,15,126,203
DB	102,72,15,126,207
	call	__ecp_nistz256_sub_fromx

	add	rsp,32*5+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_doublex::
ecp_nistz256_point_doublex	ENDP

ALIGN	32
ecp_nistz256_point_addx	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_addx::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


$L$point_addx::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*18+8

	movdqu	xmm0,XMMWORD PTR[rsi]
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	movdqu	xmm4,XMMWORD PTR[64+rsi]
	movdqu	xmm5,XMMWORD PTR[80+rsi]
	mov	rbx,rsi
	mov	rsi,rdx
	movdqa	XMMWORD PTR[384+rsp],xmm0
	movdqa	XMMWORD PTR[(384+16)+rsp],xmm1
	movdqa	XMMWORD PTR[416+rsp],xmm2
	movdqa	XMMWORD PTR[(416+16)+rsp],xmm3
	movdqa	XMMWORD PTR[448+rsp],xmm4
	movdqa	XMMWORD PTR[(448+16)+rsp],xmm5
	por	xmm5,xmm4

	movdqu	xmm0,XMMWORD PTR[rsi]
	pshufd	xmm3,xmm5,1h
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	por	xmm5,xmm3
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	mov	rdx,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	movdqa	XMMWORD PTR[480+rsp],xmm0
	pshufd	xmm4,xmm5,01eh
	movdqa	XMMWORD PTR[(480+16)+rsp],xmm1
	movdqu	xmm0,XMMWORD PTR[64+rsi]
	movdqu	xmm1,XMMWORD PTR[80+rsi]
	movdqa	XMMWORD PTR[512+rsp],xmm2
	movdqa	XMMWORD PTR[(512+16)+rsp],xmm3
	por	xmm5,xmm4
	pxor	xmm4,xmm4
	por	xmm1,xmm0
DB	102,72,15,110,199

	lea	rsi,QWORD PTR[((64-128))+rsi]
	mov	QWORD PTR[((544+0))+rsp],rdx
	mov	QWORD PTR[((544+8))+rsp],r14
	mov	QWORD PTR[((544+16))+rsp],r15
	mov	QWORD PTR[((544+24))+rsp],r8
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sqr_montx

	pcmpeqd	xmm5,xmm4
	pshufd	xmm4,xmm1,1h
	por	xmm4,xmm1
	pshufd	xmm5,xmm5,0
	pshufd	xmm3,xmm4,01eh
	por	xmm4,xmm3
	pxor	xmm3,xmm3
	pcmpeqd	xmm4,xmm3
	pshufd	xmm4,xmm4,0
	mov	rdx,QWORD PTR[((64+0))+rbx]
	mov	r14,QWORD PTR[((64+8))+rbx]
	mov	r15,QWORD PTR[((64+16))+rbx]
	mov	r8,QWORD PTR[((64+24))+rbx]
DB	102,72,15,110,203

	lea	rsi,QWORD PTR[((64-128))+rbx]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[544+rsp]
	lea	rbx,QWORD PTR[544+rsp]
	mov	r9,QWORD PTR[((0+96))+rsp]
	mov	r10,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((-128+96))+rsp]
	mov	r11,QWORD PTR[((16+96))+rsp]
	mov	r12,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[416+rsp]
	lea	rbx,QWORD PTR[416+rsp]
	mov	r9,QWORD PTR[((0+224))+rsp]
	mov	r10,QWORD PTR[((8+224))+rsp]
	lea	rsi,QWORD PTR[((-128+224))+rsp]
	mov	r11,QWORD PTR[((16+224))+rsp]
	mov	r12,QWORD PTR[((24+224))+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[512+rsp]
	lea	rbx,QWORD PTR[512+rsp]
	mov	r9,QWORD PTR[((0+256))+rsp]
	mov	r10,QWORD PTR[((8+256))+rsp]
	lea	rsi,QWORD PTR[((-128+256))+rsp]
	mov	r11,QWORD PTR[((16+256))+rsp]
	mov	r12,QWORD PTR[((24+256))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[224+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromx

	or	r12,r13
	movdqa	xmm2,xmm4
	or	r12,r8
	or	r12,r9
	por	xmm2,xmm5
DB	102,73,15,110,220

	mov	rdx,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+96))+rsp]
	mov	r10,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((-128+96))+rsp]
	mov	r11,QWORD PTR[((16+96))+rsp]
	mov	r12,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[160+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[480+rsp]
	lea	rbx,QWORD PTR[480+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[160+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_sub_fromx

	or	r12,r13
	or	r12,r8
	or	r12,r9

DB	03eh
	jnz	$L$add_proceedx
DB	102,73,15,126,208
DB	102,73,15,126,217
	test	r8,r8
	jnz	$L$add_proceedx
	test	r9,r9
	jz	$L$add_doublex

DB	102,72,15,126,199
	pxor	xmm0,xmm0
	movdqu	XMMWORD PTR[rdi],xmm0
	movdqu	XMMWORD PTR[16+rdi],xmm0
	movdqu	XMMWORD PTR[32+rdi],xmm0
	movdqu	XMMWORD PTR[48+rdi],xmm0
	movdqu	XMMWORD PTR[64+rdi],xmm0
	movdqu	XMMWORD PTR[80+rdi],xmm0
	jmp	$L$add_donex

ALIGN	32
$L$add_doublex::
DB	102,72,15,126,206
DB	102,72,15,126,199
	add	rsp,416
	jmp	$L$point_double_shortcutx

ALIGN	32
$L$add_proceedx::
	mov	rdx,QWORD PTR[((0+64))+rsp]
	mov	r14,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r15,QWORD PTR[((16+64))+rsp]
	mov	r8,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+0))+rsp]
	mov	r10,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((-128+0))+rsp]
	mov	r11,QWORD PTR[((16+0))+rsp]
	mov	r12,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[352+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[((0+0))+rsp]
	mov	r14,QWORD PTR[((8+0))+rsp]
	lea	rsi,QWORD PTR[((-128+0))+rsp]
	mov	r15,QWORD PTR[((16+0))+rsp]
	mov	r8,QWORD PTR[((24+0))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[544+rsp]
	lea	rbx,QWORD PTR[544+rsp]
	mov	r9,QWORD PTR[((0+352))+rsp]
	mov	r10,QWORD PTR[((8+352))+rsp]
	lea	rsi,QWORD PTR[((-128+352))+rsp]
	mov	r11,QWORD PTR[((16+352))+rsp]
	mov	r12,QWORD PTR[((24+352))+rsp]
	lea	rdi,QWORD PTR[352+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[rsp]
	lea	rbx,QWORD PTR[rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[160+rsp]
	lea	rbx,QWORD PTR[160+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_mul_montx




	xor	r11,r11
	add	r12,r12
	lea	rsi,QWORD PTR[96+rsp]
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	mov	rax,QWORD PTR[rsi]
	cmovc	r13,rbp
	mov	rbp,QWORD PTR[8+rsi]
	cmovc	r8,rcx
	mov	rcx,QWORD PTR[16+rsi]
	cmovc	r9,r10
	mov	r10,QWORD PTR[24+rsi]

	call	__ecp_nistz256_subx

	lea	rbx,QWORD PTR[128+rsp]
	lea	rdi,QWORD PTR[288+rsp]
	call	__ecp_nistz256_sub_fromx

	mov	rax,QWORD PTR[((192+0))+rsp]
	mov	rbp,QWORD PTR[((192+8))+rsp]
	mov	rcx,QWORD PTR[((192+16))+rsp]
	mov	r10,QWORD PTR[((192+24))+rsp]
	lea	rdi,QWORD PTR[320+rsp]

	call	__ecp_nistz256_subx

	mov	QWORD PTR[rdi],r12
	mov	QWORD PTR[8+rdi],r13
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9
	mov	rdx,QWORD PTR[128+rsp]
	lea	rbx,QWORD PTR[128+rsp]
	mov	r9,QWORD PTR[((0+224))+rsp]
	mov	r10,QWORD PTR[((8+224))+rsp]
	lea	rsi,QWORD PTR[((-128+224))+rsp]
	mov	r11,QWORD PTR[((16+224))+rsp]
	mov	r12,QWORD PTR[((24+224))+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[320+rsp]
	lea	rbx,QWORD PTR[320+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[320+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[256+rsp]
	lea	rdi,QWORD PTR[320+rsp]
	call	__ecp_nistz256_sub_fromx

DB	102,72,15,126,199

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[352+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((352+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[544+rsp]
	pand	xmm3,XMMWORD PTR[((544+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[448+rsp]
	pand	xmm3,XMMWORD PTR[((448+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[64+rdi],xmm2
	movdqu	XMMWORD PTR[80+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[288+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((288+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[480+rsp]
	pand	xmm3,XMMWORD PTR[((480+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[384+rsp]
	pand	xmm3,XMMWORD PTR[((384+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[rdi],xmm2
	movdqu	XMMWORD PTR[16+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[320+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((320+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[512+rsp]
	pand	xmm3,XMMWORD PTR[((512+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[416+rsp]
	pand	xmm3,XMMWORD PTR[((416+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[32+rdi],xmm2
	movdqu	XMMWORD PTR[48+rdi],xmm3

$L$add_donex::
	add	rsp,32*18+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_addx::
ecp_nistz256_point_addx	ENDP

ALIGN	32
ecp_nistz256_point_add_affinex	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_point_add_affinex::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


$L$point_add_affinex::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,32*15+8

	movdqu	xmm0,XMMWORD PTR[rsi]
	mov	rbx,rdx
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	movdqu	xmm4,XMMWORD PTR[64+rsi]
	movdqu	xmm5,XMMWORD PTR[80+rsi]
	mov	rdx,QWORD PTR[((64+0))+rsi]
	mov	r14,QWORD PTR[((64+8))+rsi]
	mov	r15,QWORD PTR[((64+16))+rsi]
	mov	r8,QWORD PTR[((64+24))+rsi]
	movdqa	XMMWORD PTR[320+rsp],xmm0
	movdqa	XMMWORD PTR[(320+16)+rsp],xmm1
	movdqa	XMMWORD PTR[352+rsp],xmm2
	movdqa	XMMWORD PTR[(352+16)+rsp],xmm3
	movdqa	XMMWORD PTR[384+rsp],xmm4
	movdqa	XMMWORD PTR[(384+16)+rsp],xmm5
	por	xmm5,xmm4

	movdqu	xmm0,XMMWORD PTR[rbx]
	pshufd	xmm3,xmm5,1h
	movdqu	xmm1,XMMWORD PTR[16+rbx]
	movdqu	xmm2,XMMWORD PTR[32+rbx]
	por	xmm5,xmm3
	movdqu	xmm3,XMMWORD PTR[48+rbx]
	movdqa	XMMWORD PTR[416+rsp],xmm0
	pshufd	xmm4,xmm5,01eh
	movdqa	XMMWORD PTR[(416+16)+rsp],xmm1
	por	xmm1,xmm0
DB	102,72,15,110,199
	movdqa	XMMWORD PTR[448+rsp],xmm2
	movdqa	XMMWORD PTR[(448+16)+rsp],xmm3
	por	xmm3,xmm2
	por	xmm5,xmm4
	pxor	xmm4,xmm4
	por	xmm3,xmm1

	lea	rsi,QWORD PTR[((64-128))+rsi]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_sqr_montx

	pcmpeqd	xmm5,xmm4
	pshufd	xmm4,xmm3,1h
	mov	rdx,QWORD PTR[rbx]

	mov	r9,r12
	por	xmm4,xmm3
	pshufd	xmm5,xmm5,0
	pshufd	xmm3,xmm4,01eh
	mov	r10,r13
	por	xmm4,xmm3
	pxor	xmm3,xmm3
	mov	r11,r14
	pcmpeqd	xmm4,xmm3
	pshufd	xmm4,xmm4,0

	lea	rsi,QWORD PTR[((32-128))+rsp]
	mov	r12,r15
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[320+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_sub_fromx

	mov	rdx,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[384+rsp]
	lea	rbx,QWORD PTR[384+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[288+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[448+rsp]
	lea	rbx,QWORD PTR[448+rsp]
	mov	r9,QWORD PTR[((0+32))+rsp]
	mov	r10,QWORD PTR[((8+32))+rsp]
	lea	rsi,QWORD PTR[((-128+32))+rsp]
	mov	r11,QWORD PTR[((16+32))+rsp]
	mov	r12,QWORD PTR[((24+32))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[352+rsp]
	lea	rdi,QWORD PTR[96+rsp]
	call	__ecp_nistz256_sub_fromx

	mov	rdx,QWORD PTR[((0+64))+rsp]
	mov	r14,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r15,QWORD PTR[((16+64))+rsp]
	mov	r8,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[128+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[((0+96))+rsp]
	mov	r14,QWORD PTR[((8+96))+rsp]
	lea	rsi,QWORD PTR[((-128+96))+rsp]
	mov	r15,QWORD PTR[((16+96))+rsp]
	mov	r8,QWORD PTR[((24+96))+rsp]
	lea	rdi,QWORD PTR[192+rsp]
	call	__ecp_nistz256_sqr_montx

	mov	rdx,QWORD PTR[128+rsp]
	lea	rbx,QWORD PTR[128+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[160+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[320+rsp]
	lea	rbx,QWORD PTR[320+rsp]
	mov	r9,QWORD PTR[((0+128))+rsp]
	mov	r10,QWORD PTR[((8+128))+rsp]
	lea	rsi,QWORD PTR[((-128+128))+rsp]
	mov	r11,QWORD PTR[((16+128))+rsp]
	mov	r12,QWORD PTR[((24+128))+rsp]
	lea	rdi,QWORD PTR[rsp]
	call	__ecp_nistz256_mul_montx




	xor	r11,r11
	add	r12,r12
	lea	rsi,QWORD PTR[192+rsp]
	adc	r13,r13
	mov	rax,r12
	adc	r8,r8
	adc	r9,r9
	mov	rbp,r13
	adc	r11,0

	sub	r12,-1
	mov	rcx,r8
	sbb	r13,r14
	sbb	r8,0
	mov	r10,r9
	sbb	r9,r15
	sbb	r11,0

	cmovc	r12,rax
	mov	rax,QWORD PTR[rsi]
	cmovc	r13,rbp
	mov	rbp,QWORD PTR[8+rsi]
	cmovc	r8,rcx
	mov	rcx,QWORD PTR[16+rsi]
	cmovc	r9,r10
	mov	r10,QWORD PTR[24+rsi]

	call	__ecp_nistz256_subx

	lea	rbx,QWORD PTR[160+rsp]
	lea	rdi,QWORD PTR[224+rsp]
	call	__ecp_nistz256_sub_fromx

	mov	rax,QWORD PTR[((0+0))+rsp]
	mov	rbp,QWORD PTR[((0+8))+rsp]
	mov	rcx,QWORD PTR[((0+16))+rsp]
	mov	r10,QWORD PTR[((0+24))+rsp]
	lea	rdi,QWORD PTR[64+rsp]

	call	__ecp_nistz256_subx

	mov	QWORD PTR[rdi],r12
	mov	QWORD PTR[8+rdi],r13
	mov	QWORD PTR[16+rdi],r8
	mov	QWORD PTR[24+rdi],r9
	mov	rdx,QWORD PTR[352+rsp]
	lea	rbx,QWORD PTR[352+rsp]
	mov	r9,QWORD PTR[((0+160))+rsp]
	mov	r10,QWORD PTR[((8+160))+rsp]
	lea	rsi,QWORD PTR[((-128+160))+rsp]
	mov	r11,QWORD PTR[((16+160))+rsp]
	mov	r12,QWORD PTR[((24+160))+rsp]
	lea	rdi,QWORD PTR[32+rsp]
	call	__ecp_nistz256_mul_montx

	mov	rdx,QWORD PTR[96+rsp]
	lea	rbx,QWORD PTR[96+rsp]
	mov	r9,QWORD PTR[((0+64))+rsp]
	mov	r10,QWORD PTR[((8+64))+rsp]
	lea	rsi,QWORD PTR[((-128+64))+rsp]
	mov	r11,QWORD PTR[((16+64))+rsp]
	mov	r12,QWORD PTR[((24+64))+rsp]
	lea	rdi,QWORD PTR[64+rsp]
	call	__ecp_nistz256_mul_montx

	lea	rbx,QWORD PTR[32+rsp]
	lea	rdi,QWORD PTR[256+rsp]
	call	__ecp_nistz256_sub_fromx

DB	102,72,15,126,199

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[288+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((288+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[$L$ONE_mont]
	pand	xmm3,XMMWORD PTR[(($L$ONE_mont+16))]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[384+rsp]
	pand	xmm3,XMMWORD PTR[((384+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[64+rdi],xmm2
	movdqu	XMMWORD PTR[80+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[224+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((224+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[416+rsp]
	pand	xmm3,XMMWORD PTR[((416+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[320+rsp]
	pand	xmm3,XMMWORD PTR[((320+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[rdi],xmm2
	movdqu	XMMWORD PTR[16+rdi],xmm3

	movdqa	xmm0,xmm5
	movdqa	xmm1,xmm5
	pandn	xmm0,XMMWORD PTR[256+rsp]
	movdqa	xmm2,xmm5
	pandn	xmm1,XMMWORD PTR[((256+16))+rsp]
	movdqa	xmm3,xmm5
	pand	xmm2,XMMWORD PTR[448+rsp]
	pand	xmm3,XMMWORD PTR[((448+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1

	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	pandn	xmm0,xmm2
	movdqa	xmm2,xmm4
	pandn	xmm1,xmm3
	movdqa	xmm3,xmm4
	pand	xmm2,XMMWORD PTR[352+rsp]
	pand	xmm3,XMMWORD PTR[((352+16))+rsp]
	por	xmm2,xmm0
	por	xmm3,xmm1
	movdqu	XMMWORD PTR[32+rdi],xmm2
	movdqu	XMMWORD PTR[48+rdi],xmm3

	add	rsp,32*15+8
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbx
	pop	rbp
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ecp_nistz256_point_add_affinex::
ecp_nistz256_point_add_affinex	ENDP

.text$	ENDS
END
