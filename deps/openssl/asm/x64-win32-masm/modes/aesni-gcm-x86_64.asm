OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'


ALIGN	32
_aesni_ctr32_ghash_6x	PROC PRIVATE
	vmovdqu	xmm2,XMMWORD PTR[32+r11]
	sub	rdx,6
	vpxor	xmm4,xmm4,xmm4
	vmovdqu	xmm15,XMMWORD PTR[((0-128))+rcx]
	vpaddb	xmm10,xmm1,xmm2
	vpaddb	xmm11,xmm10,xmm2
	vpaddb	xmm12,xmm11,xmm2
	vpaddb	xmm13,xmm12,xmm2
	vpaddb	xmm14,xmm13,xmm2
	vpxor	xmm9,xmm1,xmm15
	vmovdqu	XMMWORD PTR[(16+8)+rsp],xmm4
	jmp	$L$oop6x

ALIGN	32
$L$oop6x::
	add	ebx,100663296
	jc	$L$handle_ctr32
	vmovdqu	xmm3,XMMWORD PTR[((0-32))+r9]
	vpaddb	xmm1,xmm14,xmm2
	vpxor	xmm10,xmm10,xmm15
	vpxor	xmm11,xmm11,xmm15

$L$resume_ctr32::
	vmovdqu	XMMWORD PTR[r8],xmm1
	vpclmulqdq	xmm5,xmm7,xmm3,010h
	vpxor	xmm12,xmm12,xmm15
	vmovups	xmm2,XMMWORD PTR[((16-128))+rcx]
	vpclmulqdq	xmm6,xmm7,xmm3,001h
	xor	r12,r12
	cmp	r15,r14

	vaesenc	xmm9,xmm9,xmm2
	vmovdqu	xmm0,XMMWORD PTR[((48+8))+rsp]
	vpxor	xmm13,xmm13,xmm15
	vpclmulqdq	xmm1,xmm7,xmm3,000h
	vaesenc	xmm10,xmm10,xmm2
	vpxor	xmm14,xmm14,xmm15
	setnc	r12b
	vpclmulqdq	xmm7,xmm7,xmm3,011h
	vaesenc	xmm11,xmm11,xmm2
	vmovdqu	xmm3,XMMWORD PTR[((16-32))+r9]
	neg	r12
	vaesenc	xmm12,xmm12,xmm2
	vpxor	xmm6,xmm6,xmm5
	vpclmulqdq	xmm5,xmm0,xmm3,000h
	vpxor	xmm8,xmm8,xmm4
	vaesenc	xmm13,xmm13,xmm2
	vpxor	xmm4,xmm1,xmm5
	and	r12,060h
	vmovups	xmm15,XMMWORD PTR[((32-128))+rcx]
	vpclmulqdq	xmm1,xmm0,xmm3,010h
	vaesenc	xmm14,xmm14,xmm2

	vpclmulqdq	xmm2,xmm0,xmm3,001h
	lea	r14,QWORD PTR[r12*1+r14]
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm8,xmm8,XMMWORD PTR[((16+8))+rsp]
	vpclmulqdq	xmm3,xmm0,xmm3,011h
	vmovdqu	xmm0,XMMWORD PTR[((64+8))+rsp]
	vaesenc	xmm10,xmm10,xmm15
	movbe	r13,QWORD PTR[88+r14]
	vaesenc	xmm11,xmm11,xmm15
	movbe	r12,QWORD PTR[80+r14]
	vaesenc	xmm12,xmm12,xmm15
	mov	QWORD PTR[((32+8))+rsp],r13
	vaesenc	xmm13,xmm13,xmm15
	mov	QWORD PTR[((40+8))+rsp],r12
	vmovdqu	xmm5,XMMWORD PTR[((48-32))+r9]
	vaesenc	xmm14,xmm14,xmm15

	vmovups	xmm15,XMMWORD PTR[((48-128))+rcx]
	vpxor	xmm6,xmm6,xmm1
	vpclmulqdq	xmm1,xmm0,xmm5,000h
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm6,xmm6,xmm2
	vpclmulqdq	xmm2,xmm0,xmm5,010h
	vaesenc	xmm10,xmm10,xmm15
	vpxor	xmm7,xmm7,xmm3
	vpclmulqdq	xmm3,xmm0,xmm5,001h
	vaesenc	xmm11,xmm11,xmm15
	vpclmulqdq	xmm5,xmm0,xmm5,011h
	vmovdqu	xmm0,XMMWORD PTR[((80+8))+rsp]
	vaesenc	xmm12,xmm12,xmm15
	vaesenc	xmm13,xmm13,xmm15
	vpxor	xmm4,xmm4,xmm1
	vmovdqu	xmm1,XMMWORD PTR[((64-32))+r9]
	vaesenc	xmm14,xmm14,xmm15

	vmovups	xmm15,XMMWORD PTR[((64-128))+rcx]
	vpxor	xmm6,xmm6,xmm2
	vpclmulqdq	xmm2,xmm0,xmm1,000h
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm6,xmm6,xmm3
	vpclmulqdq	xmm3,xmm0,xmm1,010h
	vaesenc	xmm10,xmm10,xmm15
	movbe	r13,QWORD PTR[72+r14]
	vpxor	xmm7,xmm7,xmm5
	vpclmulqdq	xmm5,xmm0,xmm1,001h
	vaesenc	xmm11,xmm11,xmm15
	movbe	r12,QWORD PTR[64+r14]
	vpclmulqdq	xmm1,xmm0,xmm1,011h
	vmovdqu	xmm0,XMMWORD PTR[((96+8))+rsp]
	vaesenc	xmm12,xmm12,xmm15
	mov	QWORD PTR[((48+8))+rsp],r13
	vaesenc	xmm13,xmm13,xmm15
	mov	QWORD PTR[((56+8))+rsp],r12
	vpxor	xmm4,xmm4,xmm2
	vmovdqu	xmm2,XMMWORD PTR[((96-32))+r9]
	vaesenc	xmm14,xmm14,xmm15

	vmovups	xmm15,XMMWORD PTR[((80-128))+rcx]
	vpxor	xmm6,xmm6,xmm3
	vpclmulqdq	xmm3,xmm0,xmm2,000h
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm6,xmm6,xmm5
	vpclmulqdq	xmm5,xmm0,xmm2,010h
	vaesenc	xmm10,xmm10,xmm15
	movbe	r13,QWORD PTR[56+r14]
	vpxor	xmm7,xmm7,xmm1
	vpclmulqdq	xmm1,xmm0,xmm2,001h
	vpxor	xmm8,xmm8,XMMWORD PTR[((112+8))+rsp]
	vaesenc	xmm11,xmm11,xmm15
	movbe	r12,QWORD PTR[48+r14]
	vpclmulqdq	xmm2,xmm0,xmm2,011h
	vaesenc	xmm12,xmm12,xmm15
	mov	QWORD PTR[((64+8))+rsp],r13
	vaesenc	xmm13,xmm13,xmm15
	mov	QWORD PTR[((72+8))+rsp],r12
	vpxor	xmm4,xmm4,xmm3
	vmovdqu	xmm3,XMMWORD PTR[((112-32))+r9]
	vaesenc	xmm14,xmm14,xmm15

	vmovups	xmm15,XMMWORD PTR[((96-128))+rcx]
	vpxor	xmm6,xmm6,xmm5
	vpclmulqdq	xmm5,xmm8,xmm3,010h
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm6,xmm6,xmm1
	vpclmulqdq	xmm1,xmm8,xmm3,001h
	vaesenc	xmm10,xmm10,xmm15
	movbe	r13,QWORD PTR[40+r14]
	vpxor	xmm7,xmm7,xmm2
	vpclmulqdq	xmm2,xmm8,xmm3,000h
	vaesenc	xmm11,xmm11,xmm15
	movbe	r12,QWORD PTR[32+r14]
	vpclmulqdq	xmm8,xmm8,xmm3,011h
	vaesenc	xmm12,xmm12,xmm15
	mov	QWORD PTR[((80+8))+rsp],r13
	vaesenc	xmm13,xmm13,xmm15
	mov	QWORD PTR[((88+8))+rsp],r12
	vpxor	xmm6,xmm6,xmm5
	vaesenc	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm6,xmm1

	vmovups	xmm15,XMMWORD PTR[((112-128))+rcx]
	vpslldq	xmm5,xmm6,8
	vpxor	xmm4,xmm4,xmm2
	vmovdqu	xmm3,XMMWORD PTR[16+r11]

	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm7,xmm7,xmm8
	vaesenc	xmm10,xmm10,xmm15
	vpxor	xmm4,xmm4,xmm5
	movbe	r13,QWORD PTR[24+r14]
	vaesenc	xmm11,xmm11,xmm15
	movbe	r12,QWORD PTR[16+r14]
	vpalignr	xmm0,xmm4,xmm4,8
	vpclmulqdq	xmm4,xmm4,xmm3,010h
	mov	QWORD PTR[((96+8))+rsp],r13
	vaesenc	xmm12,xmm12,xmm15
	mov	QWORD PTR[((104+8))+rsp],r12
	vaesenc	xmm13,xmm13,xmm15
	vmovups	xmm1,XMMWORD PTR[((128-128))+rcx]
	vaesenc	xmm14,xmm14,xmm15

	vaesenc	xmm9,xmm9,xmm1
	vmovups	xmm15,XMMWORD PTR[((144-128))+rcx]
	vaesenc	xmm10,xmm10,xmm1
	vpsrldq	xmm6,xmm6,8
	vaesenc	xmm11,xmm11,xmm1
	vpxor	xmm7,xmm7,xmm6
	vaesenc	xmm12,xmm12,xmm1
	vpxor	xmm4,xmm4,xmm0
	movbe	r13,QWORD PTR[8+r14]
	vaesenc	xmm13,xmm13,xmm1
	movbe	r12,QWORD PTR[r14]
	vaesenc	xmm14,xmm14,xmm1
	vmovups	xmm1,XMMWORD PTR[((160-128))+rcx]
	cmp	ebp,11
	jb	$L$enc_tail

	vaesenc	xmm9,xmm9,xmm15
	vaesenc	xmm10,xmm10,xmm15
	vaesenc	xmm11,xmm11,xmm15
	vaesenc	xmm12,xmm12,xmm15
	vaesenc	xmm13,xmm13,xmm15
	vaesenc	xmm14,xmm14,xmm15

	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1
	vmovups	xmm15,XMMWORD PTR[((176-128))+rcx]
	vaesenc	xmm14,xmm14,xmm1
	vmovups	xmm1,XMMWORD PTR[((192-128))+rcx]
	je	$L$enc_tail

	vaesenc	xmm9,xmm9,xmm15
	vaesenc	xmm10,xmm10,xmm15
	vaesenc	xmm11,xmm11,xmm15
	vaesenc	xmm12,xmm12,xmm15
	vaesenc	xmm13,xmm13,xmm15
	vaesenc	xmm14,xmm14,xmm15

	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1
	vmovups	xmm15,XMMWORD PTR[((208-128))+rcx]
	vaesenc	xmm14,xmm14,xmm1
	vmovups	xmm1,XMMWORD PTR[((224-128))+rcx]
	jmp	$L$enc_tail

ALIGN	32
$L$handle_ctr32::
	vmovdqu	xmm0,XMMWORD PTR[r11]
	vpshufb	xmm6,xmm1,xmm0
	vmovdqu	xmm5,XMMWORD PTR[48+r11]
	vpaddd	xmm10,xmm6,XMMWORD PTR[64+r11]
	vpaddd	xmm11,xmm6,xmm5
	vmovdqu	xmm3,XMMWORD PTR[((0-32))+r9]
	vpaddd	xmm12,xmm10,xmm5
	vpshufb	xmm10,xmm10,xmm0
	vpaddd	xmm13,xmm11,xmm5
	vpshufb	xmm11,xmm11,xmm0
	vpxor	xmm10,xmm10,xmm15
	vpaddd	xmm14,xmm12,xmm5
	vpshufb	xmm12,xmm12,xmm0
	vpxor	xmm11,xmm11,xmm15
	vpaddd	xmm1,xmm13,xmm5
	vpshufb	xmm13,xmm13,xmm0
	vpshufb	xmm14,xmm14,xmm0
	vpshufb	xmm1,xmm1,xmm0
	jmp	$L$resume_ctr32

ALIGN	32
$L$enc_tail::
	vaesenc	xmm9,xmm9,xmm15
	vmovdqu	XMMWORD PTR[(16+8)+rsp],xmm7
	vpalignr	xmm8,xmm4,xmm4,8
	vaesenc	xmm10,xmm10,xmm15
	vpclmulqdq	xmm4,xmm4,xmm3,010h
	vpxor	xmm2,xmm1,XMMWORD PTR[rdi]
	vaesenc	xmm11,xmm11,xmm15
	vpxor	xmm0,xmm1,XMMWORD PTR[16+rdi]
	vaesenc	xmm12,xmm12,xmm15
	vpxor	xmm5,xmm1,XMMWORD PTR[32+rdi]
	vaesenc	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm1,XMMWORD PTR[48+rdi]
	vaesenc	xmm14,xmm14,xmm15
	vpxor	xmm7,xmm1,XMMWORD PTR[64+rdi]
	vpxor	xmm3,xmm1,XMMWORD PTR[80+rdi]
	vmovdqu	xmm1,XMMWORD PTR[r8]

	vaesenclast	xmm9,xmm9,xmm2
	vmovdqu	xmm2,XMMWORD PTR[32+r11]
	vaesenclast	xmm10,xmm10,xmm0
	vpaddb	xmm0,xmm1,xmm2
	mov	QWORD PTR[((112+8))+rsp],r13
	lea	rdi,QWORD PTR[96+rdi]
	vaesenclast	xmm11,xmm11,xmm5
	vpaddb	xmm5,xmm0,xmm2
	mov	QWORD PTR[((120+8))+rsp],r12
	lea	rsi,QWORD PTR[96+rsi]
	vmovdqu	xmm15,XMMWORD PTR[((0-128))+rcx]
	vaesenclast	xmm12,xmm12,xmm6
	vpaddb	xmm6,xmm5,xmm2
	vaesenclast	xmm13,xmm13,xmm7
	vpaddb	xmm7,xmm6,xmm2
	vaesenclast	xmm14,xmm14,xmm3
	vpaddb	xmm3,xmm7,xmm2

	add	r10,060h
	sub	rdx,06h
	jc	$L$6x_done

	vmovups	XMMWORD PTR[(-96)+rsi],xmm9
	vpxor	xmm9,xmm1,xmm15
	vmovups	XMMWORD PTR[(-80)+rsi],xmm10
	vmovdqa	xmm10,xmm0
	vmovups	XMMWORD PTR[(-64)+rsi],xmm11
	vmovdqa	xmm11,xmm5
	vmovups	XMMWORD PTR[(-48)+rsi],xmm12
	vmovdqa	xmm12,xmm6
	vmovups	XMMWORD PTR[(-32)+rsi],xmm13
	vmovdqa	xmm13,xmm7
	vmovups	XMMWORD PTR[(-16)+rsi],xmm14
	vmovdqa	xmm14,xmm3
	vmovdqu	xmm7,XMMWORD PTR[((32+8))+rsp]
	jmp	$L$oop6x

$L$6x_done::
	vpxor	xmm8,xmm8,XMMWORD PTR[((16+8))+rsp]
	vpxor	xmm8,xmm8,xmm4

	DB	0F3h,0C3h		;repret
_aesni_ctr32_ghash_6x	ENDP
PUBLIC	aesni_gcm_decrypt

ALIGN	32
aesni_gcm_decrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_gcm_decrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	xor	r10,r10
	cmp	rdx,060h
	jb	$L$gcm_dec_abort

	lea	rax,QWORD PTR[rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[(-216)+rax],xmm6
	movaps	XMMWORD PTR[(-200)+rax],xmm7
	movaps	XMMWORD PTR[(-184)+rax],xmm8
	movaps	XMMWORD PTR[(-168)+rax],xmm9
	movaps	XMMWORD PTR[(-152)+rax],xmm10
	movaps	XMMWORD PTR[(-136)+rax],xmm11
	movaps	XMMWORD PTR[(-120)+rax],xmm12
	movaps	XMMWORD PTR[(-104)+rax],xmm13
	movaps	XMMWORD PTR[(-88)+rax],xmm14
	movaps	XMMWORD PTR[(-72)+rax],xmm15
$L$gcm_dec_body::
	vzeroupper

	vmovdqu	xmm1,XMMWORD PTR[r8]
	add	rsp,-128
	mov	ebx,DWORD PTR[12+r8]
	lea	r11,QWORD PTR[$L$bswap_mask]
	lea	r14,QWORD PTR[((-128))+rcx]
	mov	r15,0f80h
	vmovdqu	xmm8,XMMWORD PTR[r9]
	and	rsp,-128
	vmovdqu	xmm0,XMMWORD PTR[r11]
	lea	rcx,QWORD PTR[128+rcx]
	lea	r9,QWORD PTR[((32+32))+r9]
	mov	ebp,DWORD PTR[((240-128))+rcx]
	vpshufb	xmm8,xmm8,xmm0

	and	r14,r15
	and	r15,rsp
	sub	r15,r14
	jc	$L$dec_no_key_aliasing
	cmp	r15,768
	jnc	$L$dec_no_key_aliasing
	sub	rsp,r15
$L$dec_no_key_aliasing::

	vmovdqu	xmm7,XMMWORD PTR[80+rdi]
	lea	r14,QWORD PTR[rdi]
	vmovdqu	xmm4,XMMWORD PTR[64+rdi]
	lea	r15,QWORD PTR[((-192))+rdx*1+rdi]
	vmovdqu	xmm5,XMMWORD PTR[48+rdi]
	shr	rdx,4
	xor	r10,r10
	vmovdqu	xmm6,XMMWORD PTR[32+rdi]
	vpshufb	xmm7,xmm7,xmm0
	vmovdqu	xmm2,XMMWORD PTR[16+rdi]
	vpshufb	xmm4,xmm4,xmm0
	vmovdqu	xmm3,XMMWORD PTR[rdi]
	vpshufb	xmm5,xmm5,xmm0
	vmovdqu	XMMWORD PTR[48+rsp],xmm4
	vpshufb	xmm6,xmm6,xmm0
	vmovdqu	XMMWORD PTR[64+rsp],xmm5
	vpshufb	xmm2,xmm2,xmm0
	vmovdqu	XMMWORD PTR[80+rsp],xmm6
	vpshufb	xmm3,xmm3,xmm0
	vmovdqu	XMMWORD PTR[96+rsp],xmm2
	vmovdqu	XMMWORD PTR[112+rsp],xmm3

	call	_aesni_ctr32_ghash_6x

	vmovups	XMMWORD PTR[(-96)+rsi],xmm9
	vmovups	XMMWORD PTR[(-80)+rsi],xmm10
	vmovups	XMMWORD PTR[(-64)+rsi],xmm11
	vmovups	XMMWORD PTR[(-48)+rsi],xmm12
	vmovups	XMMWORD PTR[(-32)+rsi],xmm13
	vmovups	XMMWORD PTR[(-16)+rsi],xmm14

	vpshufb	xmm8,xmm8,XMMWORD PTR[r11]
	vmovdqu	XMMWORD PTR[(-64)+r9],xmm8

	vzeroupper
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]
	movaps	xmm13,XMMWORD PTR[((-104))+rax]
	movaps	xmm14,XMMWORD PTR[((-88))+rax]
	movaps	xmm15,XMMWORD PTR[((-72))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$gcm_dec_abort::
	mov	rax,r10
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_gcm_decrypt::
aesni_gcm_decrypt	ENDP

ALIGN	32
_aesni_ctr32_6x	PROC PRIVATE
	vmovdqu	xmm4,XMMWORD PTR[((0-128))+rcx]
	vmovdqu	xmm2,XMMWORD PTR[32+r11]
	lea	r13,QWORD PTR[((-1))+rbp]
	vmovups	xmm15,XMMWORD PTR[((16-128))+rcx]
	lea	r12,QWORD PTR[((32-128))+rcx]
	vpxor	xmm9,xmm1,xmm4
	add	ebx,100663296
	jc	$L$handle_ctr32_2
	vpaddb	xmm10,xmm1,xmm2
	vpaddb	xmm11,xmm10,xmm2
	vpxor	xmm10,xmm10,xmm4
	vpaddb	xmm12,xmm11,xmm2
	vpxor	xmm11,xmm11,xmm4
	vpaddb	xmm13,xmm12,xmm2
	vpxor	xmm12,xmm12,xmm4
	vpaddb	xmm14,xmm13,xmm2
	vpxor	xmm13,xmm13,xmm4
	vpaddb	xmm1,xmm14,xmm2
	vpxor	xmm14,xmm14,xmm4
	jmp	$L$oop_ctr32

ALIGN	16
$L$oop_ctr32::
	vaesenc	xmm9,xmm9,xmm15
	vaesenc	xmm10,xmm10,xmm15
	vaesenc	xmm11,xmm11,xmm15
	vaesenc	xmm12,xmm12,xmm15
	vaesenc	xmm13,xmm13,xmm15
	vaesenc	xmm14,xmm14,xmm15
	vmovups	xmm15,XMMWORD PTR[r12]
	lea	r12,QWORD PTR[16+r12]
	dec	r13d
	jnz	$L$oop_ctr32

	vmovdqu	xmm3,XMMWORD PTR[r12]
	vaesenc	xmm9,xmm9,xmm15
	vpxor	xmm4,xmm3,XMMWORD PTR[rdi]
	vaesenc	xmm10,xmm10,xmm15
	vpxor	xmm5,xmm3,XMMWORD PTR[16+rdi]
	vaesenc	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm3,XMMWORD PTR[32+rdi]
	vaesenc	xmm12,xmm12,xmm15
	vpxor	xmm8,xmm3,XMMWORD PTR[48+rdi]
	vaesenc	xmm13,xmm13,xmm15
	vpxor	xmm2,xmm3,XMMWORD PTR[64+rdi]
	vaesenc	xmm14,xmm14,xmm15
	vpxor	xmm3,xmm3,XMMWORD PTR[80+rdi]
	lea	rdi,QWORD PTR[96+rdi]

	vaesenclast	xmm9,xmm9,xmm4
	vaesenclast	xmm10,xmm10,xmm5
	vaesenclast	xmm11,xmm11,xmm6
	vaesenclast	xmm12,xmm12,xmm8
	vaesenclast	xmm13,xmm13,xmm2
	vaesenclast	xmm14,xmm14,xmm3
	vmovups	XMMWORD PTR[rsi],xmm9
	vmovups	XMMWORD PTR[16+rsi],xmm10
	vmovups	XMMWORD PTR[32+rsi],xmm11
	vmovups	XMMWORD PTR[48+rsi],xmm12
	vmovups	XMMWORD PTR[64+rsi],xmm13
	vmovups	XMMWORD PTR[80+rsi],xmm14
	lea	rsi,QWORD PTR[96+rsi]

	DB	0F3h,0C3h		;repret
ALIGN	32
$L$handle_ctr32_2::
	vpshufb	xmm6,xmm1,xmm0
	vmovdqu	xmm5,XMMWORD PTR[48+r11]
	vpaddd	xmm10,xmm6,XMMWORD PTR[64+r11]
	vpaddd	xmm11,xmm6,xmm5
	vpaddd	xmm12,xmm10,xmm5
	vpshufb	xmm10,xmm10,xmm0
	vpaddd	xmm13,xmm11,xmm5
	vpshufb	xmm11,xmm11,xmm0
	vpxor	xmm10,xmm10,xmm4
	vpaddd	xmm14,xmm12,xmm5
	vpshufb	xmm12,xmm12,xmm0
	vpxor	xmm11,xmm11,xmm4
	vpaddd	xmm1,xmm13,xmm5
	vpshufb	xmm13,xmm13,xmm0
	vpxor	xmm12,xmm12,xmm4
	vpshufb	xmm14,xmm14,xmm0
	vpxor	xmm13,xmm13,xmm4
	vpshufb	xmm1,xmm1,xmm0
	vpxor	xmm14,xmm14,xmm4
	jmp	$L$oop_ctr32
_aesni_ctr32_6x	ENDP

PUBLIC	aesni_gcm_encrypt

ALIGN	32
aesni_gcm_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_gcm_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	xor	r10,r10
	cmp	rdx,060h*3
	jb	$L$gcm_enc_abort

	lea	rax,QWORD PTR[rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[(-216)+rax],xmm6
	movaps	XMMWORD PTR[(-200)+rax],xmm7
	movaps	XMMWORD PTR[(-184)+rax],xmm8
	movaps	XMMWORD PTR[(-168)+rax],xmm9
	movaps	XMMWORD PTR[(-152)+rax],xmm10
	movaps	XMMWORD PTR[(-136)+rax],xmm11
	movaps	XMMWORD PTR[(-120)+rax],xmm12
	movaps	XMMWORD PTR[(-104)+rax],xmm13
	movaps	XMMWORD PTR[(-88)+rax],xmm14
	movaps	XMMWORD PTR[(-72)+rax],xmm15
$L$gcm_enc_body::
	vzeroupper

	vmovdqu	xmm1,XMMWORD PTR[r8]
	add	rsp,-128
	mov	ebx,DWORD PTR[12+r8]
	lea	r11,QWORD PTR[$L$bswap_mask]
	lea	r14,QWORD PTR[((-128))+rcx]
	mov	r15,0f80h
	lea	rcx,QWORD PTR[128+rcx]
	vmovdqu	xmm0,XMMWORD PTR[r11]
	and	rsp,-128
	mov	ebp,DWORD PTR[((240-128))+rcx]

	and	r14,r15
	and	r15,rsp
	sub	r15,r14
	jc	$L$enc_no_key_aliasing
	cmp	r15,768
	jnc	$L$enc_no_key_aliasing
	sub	rsp,r15
$L$enc_no_key_aliasing::

	lea	r14,QWORD PTR[rsi]
	lea	r15,QWORD PTR[((-192))+rdx*1+rsi]
	shr	rdx,4

	call	_aesni_ctr32_6x
	vpshufb	xmm8,xmm9,xmm0
	vpshufb	xmm2,xmm10,xmm0
	vmovdqu	XMMWORD PTR[112+rsp],xmm8
	vpshufb	xmm4,xmm11,xmm0
	vmovdqu	XMMWORD PTR[96+rsp],xmm2
	vpshufb	xmm5,xmm12,xmm0
	vmovdqu	XMMWORD PTR[80+rsp],xmm4
	vpshufb	xmm6,xmm13,xmm0
	vmovdqu	XMMWORD PTR[64+rsp],xmm5
	vpshufb	xmm7,xmm14,xmm0
	vmovdqu	XMMWORD PTR[48+rsp],xmm6

	call	_aesni_ctr32_6x

	vmovdqu	xmm8,XMMWORD PTR[r9]
	lea	r9,QWORD PTR[((32+32))+r9]
	sub	rdx,12
	mov	r10,060h*2
	vpshufb	xmm8,xmm8,xmm0

	call	_aesni_ctr32_ghash_6x
	vmovdqu	xmm7,XMMWORD PTR[32+rsp]
	vmovdqu	xmm0,XMMWORD PTR[r11]
	vmovdqu	xmm3,XMMWORD PTR[((0-32))+r9]
	vpunpckhqdq	xmm1,xmm7,xmm7
	vmovdqu	xmm15,XMMWORD PTR[((32-32))+r9]
	vmovups	XMMWORD PTR[(-96)+rsi],xmm9
	vpshufb	xmm9,xmm9,xmm0
	vpxor	xmm1,xmm1,xmm7
	vmovups	XMMWORD PTR[(-80)+rsi],xmm10
	vpshufb	xmm10,xmm10,xmm0
	vmovups	XMMWORD PTR[(-64)+rsi],xmm11
	vpshufb	xmm11,xmm11,xmm0
	vmovups	XMMWORD PTR[(-48)+rsi],xmm12
	vpshufb	xmm12,xmm12,xmm0
	vmovups	XMMWORD PTR[(-32)+rsi],xmm13
	vpshufb	xmm13,xmm13,xmm0
	vmovups	XMMWORD PTR[(-16)+rsi],xmm14
	vpshufb	xmm14,xmm14,xmm0
	vmovdqu	XMMWORD PTR[16+rsp],xmm9
	vmovdqu	xmm6,XMMWORD PTR[48+rsp]
	vmovdqu	xmm0,XMMWORD PTR[((16-32))+r9]
	vpunpckhqdq	xmm2,xmm6,xmm6
	vpclmulqdq	xmm5,xmm7,xmm3,000h
	vpxor	xmm2,xmm2,xmm6
	vpclmulqdq	xmm7,xmm7,xmm3,011h
	vpclmulqdq	xmm1,xmm1,xmm15,000h

	vmovdqu	xmm9,XMMWORD PTR[64+rsp]
	vpclmulqdq	xmm4,xmm6,xmm0,000h
	vmovdqu	xmm3,XMMWORD PTR[((48-32))+r9]
	vpxor	xmm4,xmm4,xmm5
	vpunpckhqdq	xmm5,xmm9,xmm9
	vpclmulqdq	xmm6,xmm6,xmm0,011h
	vpxor	xmm5,xmm5,xmm9
	vpxor	xmm6,xmm6,xmm7
	vpclmulqdq	xmm2,xmm2,xmm15,010h
	vmovdqu	xmm15,XMMWORD PTR[((80-32))+r9]
	vpxor	xmm2,xmm2,xmm1

	vmovdqu	xmm1,XMMWORD PTR[80+rsp]
	vpclmulqdq	xmm7,xmm9,xmm3,000h
	vmovdqu	xmm0,XMMWORD PTR[((64-32))+r9]
	vpxor	xmm7,xmm7,xmm4
	vpunpckhqdq	xmm4,xmm1,xmm1
	vpclmulqdq	xmm9,xmm9,xmm3,011h
	vpxor	xmm4,xmm4,xmm1
	vpxor	xmm9,xmm9,xmm6
	vpclmulqdq	xmm5,xmm5,xmm15,000h
	vpxor	xmm5,xmm5,xmm2

	vmovdqu	xmm2,XMMWORD PTR[96+rsp]
	vpclmulqdq	xmm6,xmm1,xmm0,000h
	vmovdqu	xmm3,XMMWORD PTR[((96-32))+r9]
	vpxor	xmm6,xmm6,xmm7
	vpunpckhqdq	xmm7,xmm2,xmm2
	vpclmulqdq	xmm1,xmm1,xmm0,011h
	vpxor	xmm7,xmm7,xmm2
	vpxor	xmm1,xmm1,xmm9
	vpclmulqdq	xmm4,xmm4,xmm15,010h
	vmovdqu	xmm15,XMMWORD PTR[((128-32))+r9]
	vpxor	xmm4,xmm4,xmm5

	vpxor	xmm8,xmm8,XMMWORD PTR[112+rsp]
	vpclmulqdq	xmm5,xmm2,xmm3,000h
	vmovdqu	xmm0,XMMWORD PTR[((112-32))+r9]
	vpunpckhqdq	xmm9,xmm8,xmm8
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm2,xmm2,xmm3,011h
	vpxor	xmm9,xmm9,xmm8
	vpxor	xmm2,xmm2,xmm1
	vpclmulqdq	xmm7,xmm7,xmm15,000h
	vpxor	xmm4,xmm7,xmm4

	vpclmulqdq	xmm6,xmm8,xmm0,000h
	vmovdqu	xmm3,XMMWORD PTR[((0-32))+r9]
	vpunpckhqdq	xmm1,xmm14,xmm14
	vpclmulqdq	xmm8,xmm8,xmm0,011h
	vpxor	xmm1,xmm1,xmm14
	vpxor	xmm5,xmm6,xmm5
	vpclmulqdq	xmm9,xmm9,xmm15,010h
	vmovdqu	xmm15,XMMWORD PTR[((32-32))+r9]
	vpxor	xmm7,xmm8,xmm2
	vpxor	xmm6,xmm9,xmm4

	vmovdqu	xmm0,XMMWORD PTR[((16-32))+r9]
	vpxor	xmm9,xmm7,xmm5
	vpclmulqdq	xmm4,xmm14,xmm3,000h
	vpxor	xmm6,xmm6,xmm9
	vpunpckhqdq	xmm2,xmm13,xmm13
	vpclmulqdq	xmm14,xmm14,xmm3,011h
	vpxor	xmm2,xmm2,xmm13
	vpslldq	xmm9,xmm6,8
	vpclmulqdq	xmm1,xmm1,xmm15,000h
	vpxor	xmm8,xmm5,xmm9
	vpsrldq	xmm6,xmm6,8
	vpxor	xmm7,xmm7,xmm6

	vpclmulqdq	xmm5,xmm13,xmm0,000h
	vmovdqu	xmm3,XMMWORD PTR[((48-32))+r9]
	vpxor	xmm5,xmm5,xmm4
	vpunpckhqdq	xmm9,xmm12,xmm12
	vpclmulqdq	xmm13,xmm13,xmm0,011h
	vpxor	xmm9,xmm9,xmm12
	vpxor	xmm13,xmm13,xmm14
	vpalignr	xmm14,xmm8,xmm8,8
	vpclmulqdq	xmm2,xmm2,xmm15,010h
	vmovdqu	xmm15,XMMWORD PTR[((80-32))+r9]
	vpxor	xmm2,xmm2,xmm1

	vpclmulqdq	xmm4,xmm12,xmm3,000h
	vmovdqu	xmm0,XMMWORD PTR[((64-32))+r9]
	vpxor	xmm4,xmm4,xmm5
	vpunpckhqdq	xmm1,xmm11,xmm11
	vpclmulqdq	xmm12,xmm12,xmm3,011h
	vpxor	xmm1,xmm1,xmm11
	vpxor	xmm12,xmm12,xmm13
	vxorps	xmm7,xmm7,XMMWORD PTR[16+rsp]
	vpclmulqdq	xmm9,xmm9,xmm15,000h
	vpxor	xmm9,xmm9,xmm2

	vpclmulqdq	xmm8,xmm8,XMMWORD PTR[16+r11],010h
	vxorps	xmm8,xmm8,xmm14

	vpclmulqdq	xmm5,xmm11,xmm0,000h
	vmovdqu	xmm3,XMMWORD PTR[((96-32))+r9]
	vpxor	xmm5,xmm5,xmm4
	vpunpckhqdq	xmm2,xmm10,xmm10
	vpclmulqdq	xmm11,xmm11,xmm0,011h
	vpxor	xmm2,xmm2,xmm10
	vpalignr	xmm14,xmm8,xmm8,8
	vpxor	xmm11,xmm11,xmm12
	vpclmulqdq	xmm1,xmm1,xmm15,010h
	vmovdqu	xmm15,XMMWORD PTR[((128-32))+r9]
	vpxor	xmm1,xmm1,xmm9

	vxorps	xmm14,xmm14,xmm7
	vpclmulqdq	xmm8,xmm8,XMMWORD PTR[16+r11],010h
	vxorps	xmm8,xmm8,xmm14

	vpclmulqdq	xmm4,xmm10,xmm3,000h
	vmovdqu	xmm0,XMMWORD PTR[((112-32))+r9]
	vpxor	xmm4,xmm4,xmm5
	vpunpckhqdq	xmm9,xmm8,xmm8
	vpclmulqdq	xmm10,xmm10,xmm3,011h
	vpxor	xmm9,xmm9,xmm8
	vpxor	xmm10,xmm10,xmm11
	vpclmulqdq	xmm2,xmm2,xmm15,000h
	vpxor	xmm2,xmm2,xmm1

	vpclmulqdq	xmm5,xmm8,xmm0,000h
	vpclmulqdq	xmm7,xmm8,xmm0,011h
	vpxor	xmm5,xmm5,xmm4
	vpclmulqdq	xmm6,xmm9,xmm15,010h
	vpxor	xmm7,xmm7,xmm10
	vpxor	xmm6,xmm6,xmm2

	vpxor	xmm4,xmm7,xmm5
	vpxor	xmm6,xmm6,xmm4
	vpslldq	xmm1,xmm6,8
	vmovdqu	xmm3,XMMWORD PTR[16+r11]
	vpsrldq	xmm6,xmm6,8
	vpxor	xmm8,xmm5,xmm1
	vpxor	xmm7,xmm7,xmm6

	vpalignr	xmm2,xmm8,xmm8,8
	vpclmulqdq	xmm8,xmm8,xmm3,010h
	vpxor	xmm8,xmm8,xmm2

	vpalignr	xmm2,xmm8,xmm8,8
	vpclmulqdq	xmm8,xmm8,xmm3,010h
	vpxor	xmm2,xmm2,xmm7
	vpxor	xmm8,xmm8,xmm2
	vpshufb	xmm8,xmm8,XMMWORD PTR[r11]
	vmovdqu	XMMWORD PTR[(-64)+r9],xmm8

	vzeroupper
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]
	movaps	xmm13,XMMWORD PTR[((-104))+rax]
	movaps	xmm14,XMMWORD PTR[((-88))+rax]
	movaps	xmm15,XMMWORD PTR[((-72))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$gcm_enc_abort::
	mov	rax,r10
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_gcm_encrypt::
aesni_gcm_encrypt	ENDP
ALIGN	64
$L$bswap_mask::
DB	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
$L$poly::
DB	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0c2h
$L$one_msb::
DB	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
$L$two_lsb::
DB	2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
$L$one_lsb::
DB	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
DB	65,69,83,45,78,73,32,71,67,77,32,109,111,100,117,108
DB	101,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82
DB	89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112
DB	114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
ALIGN	64
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
gcm_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	rax,QWORD PTR[120+r8]

	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	mov	QWORD PTR[240+r8],r15
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[144+r8],rbx

	lea	rsi,QWORD PTR[((-216))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$common_seh_tail::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	mov	rdi,QWORD PTR[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0a548f3fch

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD PTR[8+rsi]
	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[16+rsi]
	mov	r10,QWORD PTR[40+rsi]
	lea	r11,QWORD PTR[56+rsi]
	lea	r12,QWORD PTR[24+rsi]
	mov	QWORD PTR[32+rsp],r10
	mov	QWORD PTR[40+rsp],r11
	mov	QWORD PTR[48+rsp],r12
	mov	QWORD PTR[56+rsp],rcx
	call	QWORD PTR[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret
gcm_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_aesni_gcm_decrypt
	DD	imagerel $L$SEH_end_aesni_gcm_decrypt
	DD	imagerel $L$SEH_gcm_dec_info

	DD	imagerel $L$SEH_begin_aesni_gcm_encrypt
	DD	imagerel $L$SEH_end_aesni_gcm_encrypt
	DD	imagerel $L$SEH_gcm_enc_info
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_gcm_dec_info::
DB	9,0,0,0
	DD	imagerel gcm_se_handler
	DD	imagerel $L$gcm_dec_body,imagerel $L$gcm_dec_abort
$L$SEH_gcm_enc_info::
DB	9,0,0,0
	DD	imagerel gcm_se_handler
	DD	imagerel $L$gcm_enc_body,imagerel $L$gcm_enc_abort

.xdata	ENDS
END
