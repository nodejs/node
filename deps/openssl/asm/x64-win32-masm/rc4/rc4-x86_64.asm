OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	RC4

ALIGN	16
RC4	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_RC4::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9

	or	rsi,rsi
	jne	$L$entry
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$entry::
	push	rbx
	push	r12
	push	r13
$L$prologue::
	mov	r11,rsi
	mov	r12,rdx
	mov	r13,rcx
	xor	r10,r10
	xor	rcx,rcx

	lea	rdi,QWORD PTR[8+rdi]
	mov	r10b,BYTE PTR[((-8))+rdi]
	mov	cl,BYTE PTR[((-4))+rdi]
	cmp	DWORD PTR[256+rdi],-1
	je	$L$RC4_CHAR
	mov	r8d,DWORD PTR[OPENSSL_ia32cap_P]
	xor	rbx,rbx
	inc	r10b
	sub	rbx,r10
	sub	r13,r12
	mov	eax,DWORD PTR[r10*4+rdi]
	test	r11,-16
	jz	$L$loop1
	bt	r8d,30
	jc	$L$intel
	and	rbx,7
	lea	rsi,QWORD PTR[1+r10]
	jz	$L$oop8
	sub	r11,rbx
$L$oop8_warmup::
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	DWORD PTR[r10*4+rdi],edx
	add	al,dl
	inc	r10b
	mov	edx,DWORD PTR[rax*4+rdi]
	mov	eax,DWORD PTR[r10*4+rdi]
	xor	dl,BYTE PTR[r12]
	mov	BYTE PTR[r12*1+r13],dl
	lea	r12,QWORD PTR[1+r12]
	dec	rbx
	jnz	$L$oop8_warmup

	lea	rsi,QWORD PTR[1+r10]
	jmp	$L$oop8
ALIGN	16
$L$oop8::
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	ebx,DWORD PTR[rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[r10*4+rdi],edx
	add	dl,al
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,bl
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	mov	eax,DWORD PTR[4+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[4+r10*4+rdi],edx
	add	dl,bl
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	ebx,DWORD PTR[8+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[8+r10*4+rdi],edx
	add	dl,al
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,bl
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	mov	eax,DWORD PTR[12+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[12+r10*4+rdi],edx
	add	dl,bl
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	ebx,DWORD PTR[16+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[16+r10*4+rdi],edx
	add	dl,al
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,bl
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	mov	eax,DWORD PTR[20+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[20+r10*4+rdi],edx
	add	dl,bl
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	ebx,DWORD PTR[24+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[24+r10*4+rdi],edx
	add	dl,al
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	sil,8
	add	cl,bl
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	mov	eax,DWORD PTR[((-4))+rsi*4+rdi]
	ror	r8,8
	mov	DWORD PTR[28+r10*4+rdi],edx
	add	dl,bl
	mov	r8b,BYTE PTR[rdx*4+rdi]
	add	r10b,8
	ror	r8,8
	sub	r11,8

	xor	r8,QWORD PTR[r12]
	mov	QWORD PTR[r12*1+r13],r8
	lea	r12,QWORD PTR[8+r12]

	test	r11,-8
	jnz	$L$oop8
	cmp	r11,0
	jne	$L$loop1
	jmp	$L$exit

ALIGN	16
$L$intel::
	test	r11,-32
	jz	$L$loop1
	and	rbx,15
	jz	$L$oop16_is_hot
	sub	r11,rbx
$L$oop16_warmup::
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	DWORD PTR[r10*4+rdi],edx
	add	al,dl
	inc	r10b
	mov	edx,DWORD PTR[rax*4+rdi]
	mov	eax,DWORD PTR[r10*4+rdi]
	xor	dl,BYTE PTR[r12]
	mov	BYTE PTR[r12*1+r13],dl
	lea	r12,QWORD PTR[1+r12]
	dec	rbx
	jnz	$L$oop16_warmup

	mov	rbx,rcx
	xor	rcx,rcx
	mov	cl,bl

$L$oop16_is_hot::
	lea	rsi,QWORD PTR[r10*4+rdi]
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	pxor	xmm0,xmm0
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[4+rsi]
	movzx	eax,al
	mov	DWORD PTR[rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],0
	jmp	$L$oop16_enter
ALIGN	16
$L$oop16::
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	pxor	xmm2,xmm0
	psllq	xmm1,8
	pxor	xmm0,xmm0
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[4+rsi]
	movzx	eax,al
	mov	DWORD PTR[rsi],edx
	pxor	xmm2,xmm1
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],0
	movdqu	XMMWORD PTR[r12*1+r13],xmm2
	lea	r12,QWORD PTR[16+r12]
$L$oop16_enter::
	mov	edx,DWORD PTR[rcx*4+rdi]
	pxor	xmm1,xmm1
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[8+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[4+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],0
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[12+rsi]
	movzx	eax,al
	mov	DWORD PTR[8+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],1
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[16+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[12+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],1
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[20+rsi]
	movzx	eax,al
	mov	DWORD PTR[16+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],2
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[24+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[20+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],2
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[28+rsi]
	movzx	eax,al
	mov	DWORD PTR[24+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],3
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[32+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[28+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],3
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[36+rsi]
	movzx	eax,al
	mov	DWORD PTR[32+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],4
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[40+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[36+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],4
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[44+rsi]
	movzx	eax,al
	mov	DWORD PTR[40+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],5
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[48+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[44+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],5
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[52+rsi]
	movzx	eax,al
	mov	DWORD PTR[48+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],6
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	mov	eax,DWORD PTR[56+rsi]
	movzx	ebx,bl
	mov	DWORD PTR[52+rsi],edx
	add	cl,al
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],6
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	add	al,dl
	mov	ebx,DWORD PTR[60+rsi]
	movzx	eax,al
	mov	DWORD PTR[56+rsi],edx
	add	cl,bl
	pinsrw	xmm0,WORD PTR[rax*4+rdi],7
	add	r10b,16
	movdqu	xmm2,XMMWORD PTR[r12]
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],ebx
	add	bl,dl
	movzx	ebx,bl
	mov	DWORD PTR[60+rsi],edx
	lea	rsi,QWORD PTR[r10*4+rdi]
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],7
	mov	eax,DWORD PTR[rsi]
	mov	rbx,rcx
	xor	rcx,rcx
	sub	r11,16
	mov	cl,bl
	test	r11,-16
	jnz	$L$oop16

	psllq	xmm1,8
	pxor	xmm2,xmm0
	pxor	xmm2,xmm1
	movdqu	XMMWORD PTR[r12*1+r13],xmm2
	lea	r12,QWORD PTR[16+r12]

	cmp	r11,0
	jne	$L$loop1
	jmp	$L$exit

ALIGN	16
$L$loop1::
	add	cl,al
	mov	edx,DWORD PTR[rcx*4+rdi]
	mov	DWORD PTR[rcx*4+rdi],eax
	mov	DWORD PTR[r10*4+rdi],edx
	add	al,dl
	inc	r10b
	mov	edx,DWORD PTR[rax*4+rdi]
	mov	eax,DWORD PTR[r10*4+rdi]
	xor	dl,BYTE PTR[r12]
	mov	BYTE PTR[r12*1+r13],dl
	lea	r12,QWORD PTR[1+r12]
	dec	r11
	jnz	$L$loop1
	jmp	$L$exit

ALIGN	16
$L$RC4_CHAR::
	add	r10b,1
	movzx	eax,BYTE PTR[r10*1+rdi]
	test	r11,-8
	jz	$L$cloop1
	jmp	$L$cloop8
ALIGN	16
$L$cloop8::
	mov	r8d,DWORD PTR[r12]
	mov	r9d,DWORD PTR[4+r12]
	add	cl,al
	lea	rsi,QWORD PTR[1+r10]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	esi,sil
	movzx	ebx,BYTE PTR[rsi*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],al
	cmp	rcx,rsi
	mov	BYTE PTR[r10*1+rdi],dl
	jne	$L$cmov0

	mov	rbx,rax
$L$cmov0::
	add	dl,al
	xor	r8b,BYTE PTR[rdx*1+rdi]
	ror	r8d,8
	add	cl,bl
	lea	r10,QWORD PTR[1+rsi]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	r10d,r10b
	movzx	eax,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],bl
	cmp	rcx,r10
	mov	BYTE PTR[rsi*1+rdi],dl
	jne	$L$cmov1

	mov	rax,rbx
$L$cmov1::
	add	dl,bl
	xor	r8b,BYTE PTR[rdx*1+rdi]
	ror	r8d,8
	add	cl,al
	lea	rsi,QWORD PTR[1+r10]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	esi,sil
	movzx	ebx,BYTE PTR[rsi*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],al
	cmp	rcx,rsi
	mov	BYTE PTR[r10*1+rdi],dl
	jne	$L$cmov2

	mov	rbx,rax
$L$cmov2::
	add	dl,al
	xor	r8b,BYTE PTR[rdx*1+rdi]
	ror	r8d,8
	add	cl,bl
	lea	r10,QWORD PTR[1+rsi]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	r10d,r10b
	movzx	eax,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],bl
	cmp	rcx,r10
	mov	BYTE PTR[rsi*1+rdi],dl
	jne	$L$cmov3

	mov	rax,rbx
$L$cmov3::
	add	dl,bl
	xor	r8b,BYTE PTR[rdx*1+rdi]
	ror	r8d,8
	add	cl,al
	lea	rsi,QWORD PTR[1+r10]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	esi,sil
	movzx	ebx,BYTE PTR[rsi*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],al
	cmp	rcx,rsi
	mov	BYTE PTR[r10*1+rdi],dl
	jne	$L$cmov4

	mov	rbx,rax
$L$cmov4::
	add	dl,al
	xor	r9b,BYTE PTR[rdx*1+rdi]
	ror	r9d,8
	add	cl,bl
	lea	r10,QWORD PTR[1+rsi]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	r10d,r10b
	movzx	eax,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],bl
	cmp	rcx,r10
	mov	BYTE PTR[rsi*1+rdi],dl
	jne	$L$cmov5

	mov	rax,rbx
$L$cmov5::
	add	dl,bl
	xor	r9b,BYTE PTR[rdx*1+rdi]
	ror	r9d,8
	add	cl,al
	lea	rsi,QWORD PTR[1+r10]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	esi,sil
	movzx	ebx,BYTE PTR[rsi*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],al
	cmp	rcx,rsi
	mov	BYTE PTR[r10*1+rdi],dl
	jne	$L$cmov6

	mov	rbx,rax
$L$cmov6::
	add	dl,al
	xor	r9b,BYTE PTR[rdx*1+rdi]
	ror	r9d,8
	add	cl,bl
	lea	r10,QWORD PTR[1+rsi]
	movzx	edx,BYTE PTR[rcx*1+rdi]
	movzx	r10d,r10b
	movzx	eax,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],bl
	cmp	rcx,r10
	mov	BYTE PTR[rsi*1+rdi],dl
	jne	$L$cmov7

	mov	rax,rbx
$L$cmov7::
	add	dl,bl
	xor	r9b,BYTE PTR[rdx*1+rdi]
	ror	r9d,8
	lea	r11,QWORD PTR[((-8))+r11]
	mov	DWORD PTR[r13],r8d
	lea	r12,QWORD PTR[8+r12]
	mov	DWORD PTR[4+r13],r9d
	lea	r13,QWORD PTR[8+r13]

	test	r11,-8
	jnz	$L$cloop8
	cmp	r11,0
	jne	$L$cloop1
	jmp	$L$exit
ALIGN	16
$L$cloop1::
	add	cl,al
	movzx	ecx,cl
	movzx	edx,BYTE PTR[rcx*1+rdi]
	mov	BYTE PTR[rcx*1+rdi],al
	mov	BYTE PTR[r10*1+rdi],dl
	add	dl,al
	add	r10b,1
	movzx	edx,dl
	movzx	r10d,r10b
	movzx	edx,BYTE PTR[rdx*1+rdi]
	movzx	eax,BYTE PTR[r10*1+rdi]
	xor	dl,BYTE PTR[r12]
	lea	r12,QWORD PTR[1+r12]
	mov	BYTE PTR[r13],dl
	lea	r13,QWORD PTR[1+r13]
	sub	r11,1
	jnz	$L$cloop1
	jmp	$L$exit

ALIGN	16
$L$exit::
	sub	r10b,1
	mov	DWORD PTR[((-8))+rdi],r10d
	mov	DWORD PTR[((-4))+rdi],ecx

	mov	r13,QWORD PTR[rsp]
	mov	r12,QWORD PTR[8+rsp]
	mov	rbx,QWORD PTR[16+rsp]
	add	rsp,24
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_RC4::
RC4	ENDP
PUBLIC	private_RC4_set_key

ALIGN	16
private_RC4_set_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_private_RC4_set_key::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	rdi,QWORD PTR[8+rdi]
	lea	rdx,QWORD PTR[rsi*1+rdx]
	neg	rsi
	mov	rcx,rsi
	xor	eax,eax
	xor	r9,r9
	xor	r10,r10
	xor	r11,r11

	mov	r8d,DWORD PTR[OPENSSL_ia32cap_P]
	bt	r8d,20
	jc	$L$c1stloop
	jmp	$L$w1stloop

ALIGN	16
$L$w1stloop::
	mov	DWORD PTR[rax*4+rdi],eax
	add	al,1
	jnc	$L$w1stloop

	xor	r9,r9
	xor	r8,r8
ALIGN	16
$L$w2ndloop::
	mov	r10d,DWORD PTR[r9*4+rdi]
	add	r8b,BYTE PTR[rsi*1+rdx]
	add	r8b,r10b
	add	rsi,1
	mov	r11d,DWORD PTR[r8*4+rdi]
	cmovz	rsi,rcx
	mov	DWORD PTR[r8*4+rdi],r10d
	mov	DWORD PTR[r9*4+rdi],r11d
	add	r9b,1
	jnc	$L$w2ndloop
	jmp	$L$exit_key

ALIGN	16
$L$c1stloop::
	mov	BYTE PTR[rax*1+rdi],al
	add	al,1
	jnc	$L$c1stloop

	xor	r9,r9
	xor	r8,r8
ALIGN	16
$L$c2ndloop::
	mov	r10b,BYTE PTR[r9*1+rdi]
	add	r8b,BYTE PTR[rsi*1+rdx]
	add	r8b,r10b
	add	rsi,1
	mov	r11b,BYTE PTR[r8*1+rdi]
	jnz	$L$cnowrap
	mov	rsi,rcx
$L$cnowrap::
	mov	BYTE PTR[r8*1+rdi],r10b
	mov	BYTE PTR[r9*1+rdi],r11b
	add	r9b,1
	jnc	$L$c2ndloop
	mov	DWORD PTR[256+rdi],-1

ALIGN	16
$L$exit_key::
	xor	eax,eax
	mov	DWORD PTR[((-8))+rdi],eax
	mov	DWORD PTR[((-4))+rdi],eax
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_private_RC4_set_key::
private_RC4_set_key	ENDP

PUBLIC	RC4_options

ALIGN	16
RC4_options	PROC PUBLIC
	lea	rax,QWORD PTR[$L$opts]
	mov	edx,DWORD PTR[OPENSSL_ia32cap_P]
	bt	edx,20
	jc	$L$8xchar
	bt	edx,30
	jnc	$L$done
	add	rax,25
	DB	0F3h,0C3h		;repret
$L$8xchar::
	add	rax,12
$L$done::
	DB	0F3h,0C3h		;repret
ALIGN	64
$L$opts::
DB	114,99,52,40,56,120,44,105,110,116,41,0
DB	114,99,52,40,56,120,44,99,104,97,114,41,0
DB	114,99,52,40,49,54,120,44,105,110,116,41,0
DB	82,67,52,32,102,111,114,32,120,56,54,95,54,52,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
ALIGN	64
RC4_options	ENDP
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
stream_se_handler	PROC PRIVATE
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

	lea	r10,QWORD PTR[$L$prologue]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

	lea	rax,QWORD PTR[24+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	r12,QWORD PTR[((-16))+rax]
	mov	r13,QWORD PTR[((-24))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13

$L$in_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	jmp	$L$common_seh_exit
stream_se_handler	ENDP


ALIGN	16
key_se_handler	PROC PRIVATE
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

	mov	rax,QWORD PTR[152+r8]
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

$L$common_seh_exit::

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
key_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_RC4
	DD	imagerel $L$SEH_end_RC4
	DD	imagerel $L$SEH_info_RC4

	DD	imagerel $L$SEH_begin_private_RC4_set_key
	DD	imagerel $L$SEH_end_private_RC4_set_key
	DD	imagerel $L$SEH_info_private_RC4_set_key

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_RC4::
DB	9,0,0,0
	DD	imagerel stream_se_handler
$L$SEH_info_private_RC4_set_key::
DB	9,0,0,0
	DD	imagerel key_se_handler

.xdata	ENDS
END
