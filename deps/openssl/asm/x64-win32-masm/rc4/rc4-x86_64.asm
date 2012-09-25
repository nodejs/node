OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

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

	add	rdi,8
	mov	r8d,DWORD PTR[((-8))+rdi]
	mov	r12d,DWORD PTR[((-4))+rdi]
	cmp	DWORD PTR[256+rdi],-1
	je	$L$RC4_CHAR
	inc	r8b
	mov	r9d,DWORD PTR[r8*4+rdi]
	test	rsi,-8
	jz	$L$loop1
	jmp	$L$loop8
ALIGN	16
$L$loop8::
	add	r12b,r9b
	mov	r10,r8
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r10b
	mov	r11d,DWORD PTR[r10*4+rdi]
	cmp	r12,r10
	mov	DWORD PTR[r12*4+rdi],r9d
	cmove	r11,r9
	mov	DWORD PTR[r8*4+rdi],r13d
	add	r13b,r9b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r11b
	mov	r8,r10
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r8b
	mov	r9d,DWORD PTR[r8*4+rdi]
	cmp	r12,r8
	mov	DWORD PTR[r12*4+rdi],r11d
	cmove	r9,r11
	mov	DWORD PTR[r10*4+rdi],r13d
	add	r13b,r11b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r9b
	mov	r10,r8
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r10b
	mov	r11d,DWORD PTR[r10*4+rdi]
	cmp	r12,r10
	mov	DWORD PTR[r12*4+rdi],r9d
	cmove	r11,r9
	mov	DWORD PTR[r8*4+rdi],r13d
	add	r13b,r9b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r11b
	mov	r8,r10
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r8b
	mov	r9d,DWORD PTR[r8*4+rdi]
	cmp	r12,r8
	mov	DWORD PTR[r12*4+rdi],r11d
	cmove	r9,r11
	mov	DWORD PTR[r10*4+rdi],r13d
	add	r13b,r11b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r9b
	mov	r10,r8
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r10b
	mov	r11d,DWORD PTR[r10*4+rdi]
	cmp	r12,r10
	mov	DWORD PTR[r12*4+rdi],r9d
	cmove	r11,r9
	mov	DWORD PTR[r8*4+rdi],r13d
	add	r13b,r9b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r11b
	mov	r8,r10
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r8b
	mov	r9d,DWORD PTR[r8*4+rdi]
	cmp	r12,r8
	mov	DWORD PTR[r12*4+rdi],r11d
	cmove	r9,r11
	mov	DWORD PTR[r10*4+rdi],r13d
	add	r13b,r11b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r9b
	mov	r10,r8
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r10b
	mov	r11d,DWORD PTR[r10*4+rdi]
	cmp	r12,r10
	mov	DWORD PTR[r12*4+rdi],r9d
	cmove	r11,r9
	mov	DWORD PTR[r8*4+rdi],r13d
	add	r13b,r9b
	mov	al,BYTE PTR[r13*4+rdi]
	add	r12b,r11b
	mov	r8,r10
	mov	r13d,DWORD PTR[r12*4+rdi]
	ror	rax,8
	inc	r8b
	mov	r9d,DWORD PTR[r8*4+rdi]
	cmp	r12,r8
	mov	DWORD PTR[r12*4+rdi],r11d
	cmove	r9,r11
	mov	DWORD PTR[r10*4+rdi],r13d
	add	r13b,r11b
	mov	al,BYTE PTR[r13*4+rdi]
	ror	rax,8
	sub	rsi,8

	xor	rax,QWORD PTR[rdx]
	add	rdx,8
	mov	QWORD PTR[rcx],rax
	add	rcx,8

	test	rsi,-8
	jnz	$L$loop8
	cmp	rsi,0
	jne	$L$loop1
	jmp	$L$exit

ALIGN	16
$L$loop1::
	add	r12b,r9b
	mov	r13d,DWORD PTR[r12*4+rdi]
	mov	DWORD PTR[r12*4+rdi],r9d
	mov	DWORD PTR[r8*4+rdi],r13d
	add	r9b,r13b
	inc	r8b
	mov	r13d,DWORD PTR[r9*4+rdi]
	mov	r9d,DWORD PTR[r8*4+rdi]
	xor	r13b,BYTE PTR[rdx]
	inc	rdx
	mov	BYTE PTR[rcx],r13b
	inc	rcx
	dec	rsi
	jnz	$L$loop1
	jmp	$L$exit

ALIGN	16
$L$RC4_CHAR::
	add	r8b,1
	movzx	r9d,BYTE PTR[r8*1+rdi]
	test	rsi,-8
	jz	$L$cloop1
	cmp	DWORD PTR[260+rdi],0
	jnz	$L$cloop1
	jmp	$L$cloop8
ALIGN	16
$L$cloop8::
	mov	eax,DWORD PTR[rdx]
	mov	ebx,DWORD PTR[4+rdx]
	add	r12b,r9b
	lea	r10,QWORD PTR[1+r8]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r10d,r10b
	movzx	r11d,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r9b
	cmp	r12,r10
	mov	BYTE PTR[r8*1+rdi],r13b
	jne	$L$cmov0

	mov	r11,r9
$L$cmov0::
	add	r13b,r9b
	xor	al,BYTE PTR[r13*1+rdi]
	ror	eax,8
	add	r12b,r11b
	lea	r8,QWORD PTR[1+r10]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r8d,r8b
	movzx	r9d,BYTE PTR[r8*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r11b
	cmp	r12,r8
	mov	BYTE PTR[r10*1+rdi],r13b
	jne	$L$cmov1

	mov	r9,r11
$L$cmov1::
	add	r13b,r11b
	xor	al,BYTE PTR[r13*1+rdi]
	ror	eax,8
	add	r12b,r9b
	lea	r10,QWORD PTR[1+r8]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r10d,r10b
	movzx	r11d,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r9b
	cmp	r12,r10
	mov	BYTE PTR[r8*1+rdi],r13b
	jne	$L$cmov2

	mov	r11,r9
$L$cmov2::
	add	r13b,r9b
	xor	al,BYTE PTR[r13*1+rdi]
	ror	eax,8
	add	r12b,r11b
	lea	r8,QWORD PTR[1+r10]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r8d,r8b
	movzx	r9d,BYTE PTR[r8*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r11b
	cmp	r12,r8
	mov	BYTE PTR[r10*1+rdi],r13b
	jne	$L$cmov3

	mov	r9,r11
$L$cmov3::
	add	r13b,r11b
	xor	al,BYTE PTR[r13*1+rdi]
	ror	eax,8
	add	r12b,r9b
	lea	r10,QWORD PTR[1+r8]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r10d,r10b
	movzx	r11d,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r9b
	cmp	r12,r10
	mov	BYTE PTR[r8*1+rdi],r13b
	jne	$L$cmov4

	mov	r11,r9
$L$cmov4::
	add	r13b,r9b
	xor	bl,BYTE PTR[r13*1+rdi]
	ror	ebx,8
	add	r12b,r11b
	lea	r8,QWORD PTR[1+r10]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r8d,r8b
	movzx	r9d,BYTE PTR[r8*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r11b
	cmp	r12,r8
	mov	BYTE PTR[r10*1+rdi],r13b
	jne	$L$cmov5

	mov	r9,r11
$L$cmov5::
	add	r13b,r11b
	xor	bl,BYTE PTR[r13*1+rdi]
	ror	ebx,8
	add	r12b,r9b
	lea	r10,QWORD PTR[1+r8]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r10d,r10b
	movzx	r11d,BYTE PTR[r10*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r9b
	cmp	r12,r10
	mov	BYTE PTR[r8*1+rdi],r13b
	jne	$L$cmov6

	mov	r11,r9
$L$cmov6::
	add	r13b,r9b
	xor	bl,BYTE PTR[r13*1+rdi]
	ror	ebx,8
	add	r12b,r11b
	lea	r8,QWORD PTR[1+r10]
	movzx	r13d,BYTE PTR[r12*1+rdi]
	movzx	r8d,r8b
	movzx	r9d,BYTE PTR[r8*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r11b
	cmp	r12,r8
	mov	BYTE PTR[r10*1+rdi],r13b
	jne	$L$cmov7

	mov	r9,r11
$L$cmov7::
	add	r13b,r11b
	xor	bl,BYTE PTR[r13*1+rdi]
	ror	ebx,8
	lea	rsi,QWORD PTR[((-8))+rsi]
	mov	DWORD PTR[rcx],eax
	lea	rdx,QWORD PTR[8+rdx]
	mov	DWORD PTR[4+rcx],ebx
	lea	rcx,QWORD PTR[8+rcx]

	test	rsi,-8
	jnz	$L$cloop8
	cmp	rsi,0
	jne	$L$cloop1
	jmp	$L$exit
ALIGN	16
$L$cloop1::
	add	r12b,r9b
	movzx	r13d,BYTE PTR[r12*1+rdi]
	mov	BYTE PTR[r12*1+rdi],r9b
	mov	BYTE PTR[r8*1+rdi],r13b
	add	r13b,r9b
	add	r8b,1
	movzx	r13d,r13b
	movzx	r8d,r8b
	movzx	r13d,BYTE PTR[r13*1+rdi]
	movzx	r9d,BYTE PTR[r8*1+rdi]
	xor	r13b,BYTE PTR[rdx]
	lea	rdx,QWORD PTR[1+rdx]
	mov	BYTE PTR[rcx],r13b
	lea	rcx,QWORD PTR[1+rcx]
	sub	rsi,1
	jnz	$L$cloop1
	jmp	$L$exit

ALIGN	16
$L$exit::
	sub	r8b,1
	mov	DWORD PTR[((-8))+rdi],r8d
	mov	DWORD PTR[((-4))+rdi],r12d

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
EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	RC4_set_key

ALIGN	16
RC4_set_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_RC4_set_key::
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
	jnc	$L$w1stloop
	bt	r8d,30
	setc	r9b
	mov	DWORD PTR[260+rdi],r9d
	jmp	$L$c1stloop

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
$L$SEH_end_RC4_set_key::
RC4_set_key	ENDP

PUBLIC	RC4_options

ALIGN	16
RC4_options	PROC PUBLIC
	lea	rax,QWORD PTR[$L$opts]
	mov	edx,DWORD PTR[OPENSSL_ia32cap_P]
	bt	edx,20
	jnc	$L$done
	add	rax,12
	bt	edx,30
	jnc	$L$done
	add	rax,13
$L$done::
	DB	0F3h,0C3h		;repret
ALIGN	64
$L$opts::
DB	114,99,52,40,56,120,44,105,110,116,41,0
DB	114,99,52,40,56,120,44,99,104,97,114,41,0
DB	114,99,52,40,49,120,44,99,104,97,114,41,0
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

	DD	imagerel $L$SEH_begin_RC4_set_key
	DD	imagerel $L$SEH_end_RC4_set_key
	DD	imagerel $L$SEH_info_RC4_set_key

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_RC4::
DB	9,0,0,0
	DD	imagerel stream_se_handler
$L$SEH_info_RC4_set_key::
DB	9,0,0,0
	DD	imagerel key_se_handler

.xdata	ENDS
END
