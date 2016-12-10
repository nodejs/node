OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	aesni_multi_cbc_encrypt

ALIGN	32
aesni_multi_cbc_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_multi_cbc_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
	movaps	XMMWORD PTR[64+rsp],xmm10
	movaps	XMMWORD PTR[80+rsp],xmm11
	movaps	XMMWORD PTR[96+rsp],xmm12
	movaps	XMMWORD PTR[(-104)+rax],xmm13
	movaps	XMMWORD PTR[(-88)+rax],xmm14
	movaps	XMMWORD PTR[(-72)+rax],xmm15






	sub	rsp,48
	and	rsp,-64
	mov	QWORD PTR[16+rsp],rax

$L$enc4x_body::
	movdqu	xmm12,XMMWORD PTR[rsi]
	lea	rsi,QWORD PTR[120+rsi]
	lea	rdi,QWORD PTR[80+rdi]

$L$enc4x_loop_grande::
	mov	DWORD PTR[24+rsp],edx
	xor	edx,edx
	mov	ecx,DWORD PTR[((-64))+rdi]
	mov	r8,QWORD PTR[((-80))+rdi]
	cmp	ecx,edx
	mov	r12,QWORD PTR[((-72))+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm2,XMMWORD PTR[((-56))+rdi]
	mov	DWORD PTR[32+rsp],ecx
	cmovle	r8,rsp
	mov	ecx,DWORD PTR[((-24))+rdi]
	mov	r9,QWORD PTR[((-40))+rdi]
	cmp	ecx,edx
	mov	r13,QWORD PTR[((-32))+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm3,XMMWORD PTR[((-16))+rdi]
	mov	DWORD PTR[36+rsp],ecx
	cmovle	r9,rsp
	mov	ecx,DWORD PTR[16+rdi]
	mov	r10,QWORD PTR[rdi]
	cmp	ecx,edx
	mov	r14,QWORD PTR[8+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm4,XMMWORD PTR[24+rdi]
	mov	DWORD PTR[40+rsp],ecx
	cmovle	r10,rsp
	mov	ecx,DWORD PTR[56+rdi]
	mov	r11,QWORD PTR[40+rdi]
	cmp	ecx,edx
	mov	r15,QWORD PTR[48+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm5,XMMWORD PTR[64+rdi]
	mov	DWORD PTR[44+rsp],ecx
	cmovle	r11,rsp
	test	edx,edx
	jz	$L$enc4x_done

	movups	xmm1,XMMWORD PTR[((16-120))+rsi]
	pxor	xmm2,xmm12
	movups	xmm0,XMMWORD PTR[((32-120))+rsi]
	pxor	xmm3,xmm12
	mov	eax,DWORD PTR[((240-120))+rsi]
	pxor	xmm4,xmm12
	movdqu	xmm6,XMMWORD PTR[r8]
	pxor	xmm5,xmm12
	movdqu	xmm7,XMMWORD PTR[r9]
	pxor	xmm2,xmm6
	movdqu	xmm8,XMMWORD PTR[r10]
	pxor	xmm3,xmm7
	movdqu	xmm9,XMMWORD PTR[r11]
	pxor	xmm4,xmm8
	pxor	xmm5,xmm9
	movdqa	xmm10,XMMWORD PTR[32+rsp]
	xor	rbx,rbx
	jmp	$L$oop_enc4x

ALIGN	32
$L$oop_enc4x::
	add	rbx,16
	lea	rbp,QWORD PTR[16+rsp]
	mov	ecx,1
	sub	rbp,rbx

DB	102,15,56,220,209
	prefetcht0	[31+rbx*1+r8]
	prefetcht0	[31+rbx*1+r9]
DB	102,15,56,220,217
	prefetcht0	[31+rbx*1+r10]
	prefetcht0	[31+rbx*1+r10]
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((48-120))+rsi]
	cmp	ecx,DWORD PTR[32+rsp]
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
	cmovge	r8,rbp
	cmovg	r12,rbp
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((-56))+rsi]
	cmp	ecx,DWORD PTR[36+rsp]
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
	cmovge	r9,rbp
	cmovg	r13,rbp
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((-40))+rsi]
	cmp	ecx,DWORD PTR[40+rsp]
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
	cmovge	r10,rbp
	cmovg	r14,rbp
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((-24))+rsi]
	cmp	ecx,DWORD PTR[44+rsp]
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
	cmovge	r11,rbp
	cmovg	r15,rbp
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((-8))+rsi]
	movdqa	xmm11,xmm10
DB	102,15,56,220,208
	prefetcht0	[15+rbx*1+r12]
	prefetcht0	[15+rbx*1+r13]
DB	102,15,56,220,216
	prefetcht0	[15+rbx*1+r14]
	prefetcht0	[15+rbx*1+r15]
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((128-120))+rsi]
	pxor	xmm12,xmm12

DB	102,15,56,220,209
	pcmpgtd	xmm11,xmm12
	movdqu	xmm12,XMMWORD PTR[((-120))+rsi]
DB	102,15,56,220,217
	paddd	xmm10,xmm11
	movdqa	XMMWORD PTR[32+rsp],xmm10
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((144-120))+rsi]

	cmp	eax,11

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((160-120))+rsi]

	jb	$L$enc4x_tail

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((176-120))+rsi]

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((192-120))+rsi]

	je	$L$enc4x_tail

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[((208-120))+rsi]

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((224-120))+rsi]
	jmp	$L$enc4x_tail

ALIGN	32
$L$enc4x_tail::
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movdqu	xmm6,XMMWORD PTR[rbx*1+r8]
	movdqu	xmm1,XMMWORD PTR[((16-120))+rsi]

DB	102,15,56,221,208
	movdqu	xmm7,XMMWORD PTR[rbx*1+r9]
	pxor	xmm6,xmm12
DB	102,15,56,221,216
	movdqu	xmm8,XMMWORD PTR[rbx*1+r10]
	pxor	xmm7,xmm12
DB	102,15,56,221,224
	movdqu	xmm9,XMMWORD PTR[rbx*1+r11]
	pxor	xmm8,xmm12
DB	102,15,56,221,232
	movdqu	xmm0,XMMWORD PTR[((32-120))+rsi]
	pxor	xmm9,xmm12

	movups	XMMWORD PTR[(-16)+rbx*1+r12],xmm2
	pxor	xmm2,xmm6
	movups	XMMWORD PTR[(-16)+rbx*1+r13],xmm3
	pxor	xmm3,xmm7
	movups	XMMWORD PTR[(-16)+rbx*1+r14],xmm4
	pxor	xmm4,xmm8
	movups	XMMWORD PTR[(-16)+rbx*1+r15],xmm5
	pxor	xmm5,xmm9

	dec	edx
	jnz	$L$oop_enc4x

	mov	rax,QWORD PTR[16+rsp]
	mov	edx,DWORD PTR[24+rsp]










	lea	rdi,QWORD PTR[160+rdi]
	dec	edx
	jnz	$L$enc4x_loop_grande

$L$enc4x_done::
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]



	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$enc4x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_multi_cbc_encrypt::
aesni_multi_cbc_encrypt	ENDP

PUBLIC	aesni_multi_cbc_decrypt

ALIGN	32
aesni_multi_cbc_decrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_multi_cbc_decrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
	movaps	XMMWORD PTR[64+rsp],xmm10
	movaps	XMMWORD PTR[80+rsp],xmm11
	movaps	XMMWORD PTR[96+rsp],xmm12
	movaps	XMMWORD PTR[(-104)+rax],xmm13
	movaps	XMMWORD PTR[(-88)+rax],xmm14
	movaps	XMMWORD PTR[(-72)+rax],xmm15






	sub	rsp,48
	and	rsp,-64
	mov	QWORD PTR[16+rsp],rax

$L$dec4x_body::
	movdqu	xmm12,XMMWORD PTR[rsi]
	lea	rsi,QWORD PTR[120+rsi]
	lea	rdi,QWORD PTR[80+rdi]

$L$dec4x_loop_grande::
	mov	DWORD PTR[24+rsp],edx
	xor	edx,edx
	mov	ecx,DWORD PTR[((-64))+rdi]
	mov	r8,QWORD PTR[((-80))+rdi]
	cmp	ecx,edx
	mov	r12,QWORD PTR[((-72))+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm6,XMMWORD PTR[((-56))+rdi]
	mov	DWORD PTR[32+rsp],ecx
	cmovle	r8,rsp
	mov	ecx,DWORD PTR[((-24))+rdi]
	mov	r9,QWORD PTR[((-40))+rdi]
	cmp	ecx,edx
	mov	r13,QWORD PTR[((-32))+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm7,XMMWORD PTR[((-16))+rdi]
	mov	DWORD PTR[36+rsp],ecx
	cmovle	r9,rsp
	mov	ecx,DWORD PTR[16+rdi]
	mov	r10,QWORD PTR[rdi]
	cmp	ecx,edx
	mov	r14,QWORD PTR[8+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm8,XMMWORD PTR[24+rdi]
	mov	DWORD PTR[40+rsp],ecx
	cmovle	r10,rsp
	mov	ecx,DWORD PTR[56+rdi]
	mov	r11,QWORD PTR[40+rdi]
	cmp	ecx,edx
	mov	r15,QWORD PTR[48+rdi]
	cmovg	edx,ecx
	test	ecx,ecx
	movdqu	xmm9,XMMWORD PTR[64+rdi]
	mov	DWORD PTR[44+rsp],ecx
	cmovle	r11,rsp
	test	edx,edx
	jz	$L$dec4x_done

	movups	xmm1,XMMWORD PTR[((16-120))+rsi]
	movups	xmm0,XMMWORD PTR[((32-120))+rsi]
	mov	eax,DWORD PTR[((240-120))+rsi]
	movdqu	xmm2,XMMWORD PTR[r8]
	movdqu	xmm3,XMMWORD PTR[r9]
	pxor	xmm2,xmm12
	movdqu	xmm4,XMMWORD PTR[r10]
	pxor	xmm3,xmm12
	movdqu	xmm5,XMMWORD PTR[r11]
	pxor	xmm4,xmm12
	pxor	xmm5,xmm12
	movdqa	xmm10,XMMWORD PTR[32+rsp]
	xor	rbx,rbx
	jmp	$L$oop_dec4x

ALIGN	32
$L$oop_dec4x::
	add	rbx,16
	lea	rbp,QWORD PTR[16+rsp]
	mov	ecx,1
	sub	rbp,rbx

DB	102,15,56,222,209
	prefetcht0	[31+rbx*1+r8]
	prefetcht0	[31+rbx*1+r9]
DB	102,15,56,222,217
	prefetcht0	[31+rbx*1+r10]
	prefetcht0	[31+rbx*1+r11]
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((48-120))+rsi]
	cmp	ecx,DWORD PTR[32+rsp]
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
	cmovge	r8,rbp
	cmovg	r12,rbp
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((-56))+rsi]
	cmp	ecx,DWORD PTR[36+rsp]
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
	cmovge	r9,rbp
	cmovg	r13,rbp
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((-40))+rsi]
	cmp	ecx,DWORD PTR[40+rsp]
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
	cmovge	r10,rbp
	cmovg	r14,rbp
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((-24))+rsi]
	cmp	ecx,DWORD PTR[44+rsp]
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
	cmovge	r11,rbp
	cmovg	r15,rbp
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((-8))+rsi]
	movdqa	xmm11,xmm10
DB	102,15,56,222,208
	prefetcht0	[15+rbx*1+r12]
	prefetcht0	[15+rbx*1+r13]
DB	102,15,56,222,216
	prefetcht0	[15+rbx*1+r14]
	prefetcht0	[15+rbx*1+r15]
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((128-120))+rsi]
	pxor	xmm12,xmm12

DB	102,15,56,222,209
	pcmpgtd	xmm11,xmm12
	movdqu	xmm12,XMMWORD PTR[((-120))+rsi]
DB	102,15,56,222,217
	paddd	xmm10,xmm11
	movdqa	XMMWORD PTR[32+rsp],xmm10
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((144-120))+rsi]

	cmp	eax,11

DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((160-120))+rsi]

	jb	$L$dec4x_tail

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((176-120))+rsi]

DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((192-120))+rsi]

	je	$L$dec4x_tail

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[((208-120))+rsi]

DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((224-120))+rsi]
	jmp	$L$dec4x_tail

ALIGN	32
$L$dec4x_tail::
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
	pxor	xmm6,xmm0
	pxor	xmm7,xmm0
DB	102,15,56,222,233
	movdqu	xmm1,XMMWORD PTR[((16-120))+rsi]
	pxor	xmm8,xmm0
	pxor	xmm9,xmm0
	movdqu	xmm0,XMMWORD PTR[((32-120))+rsi]

DB	102,15,56,223,214
DB	102,15,56,223,223
	movdqu	xmm6,XMMWORD PTR[((-16))+rbx*1+r8]
	movdqu	xmm7,XMMWORD PTR[((-16))+rbx*1+r9]
DB	102,65,15,56,223,224
DB	102,65,15,56,223,233
	movdqu	xmm8,XMMWORD PTR[((-16))+rbx*1+r10]
	movdqu	xmm9,XMMWORD PTR[((-16))+rbx*1+r11]

	movups	XMMWORD PTR[(-16)+rbx*1+r12],xmm2
	movdqu	xmm2,XMMWORD PTR[rbx*1+r8]
	movups	XMMWORD PTR[(-16)+rbx*1+r13],xmm3
	movdqu	xmm3,XMMWORD PTR[rbx*1+r9]
	pxor	xmm2,xmm12
	movups	XMMWORD PTR[(-16)+rbx*1+r14],xmm4
	movdqu	xmm4,XMMWORD PTR[rbx*1+r10]
	pxor	xmm3,xmm12
	movups	XMMWORD PTR[(-16)+rbx*1+r15],xmm5
	movdqu	xmm5,XMMWORD PTR[rbx*1+r11]
	pxor	xmm4,xmm12
	pxor	xmm5,xmm12

	dec	edx
	jnz	$L$oop_dec4x

	mov	rax,QWORD PTR[16+rsp]
	mov	edx,DWORD PTR[24+rsp]

	lea	rdi,QWORD PTR[160+rdi]
	dec	edx
	jnz	$L$dec4x_loop_grande

$L$dec4x_done::
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]



	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$dec4x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_multi_cbc_decrypt::
aesni_multi_cbc_decrypt	ENDP
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
se_handler	PROC PRIVATE
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
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_prologue

	mov	rax,QWORD PTR[16+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15

	lea	rsi,QWORD PTR[((-56-160))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$in_prologue::
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
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_aesni_multi_cbc_encrypt
	DD	imagerel $L$SEH_end_aesni_multi_cbc_encrypt
	DD	imagerel $L$SEH_info_aesni_multi_cbc_encrypt
	DD	imagerel $L$SEH_begin_aesni_multi_cbc_decrypt
	DD	imagerel $L$SEH_end_aesni_multi_cbc_decrypt
	DD	imagerel $L$SEH_info_aesni_multi_cbc_decrypt
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_aesni_multi_cbc_encrypt::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$enc4x_body,imagerel $L$enc4x_epilogue
$L$SEH_info_aesni_multi_cbc_decrypt::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$dec4x_body,imagerel $L$dec4x_epilogue

.xdata	ENDS
END
