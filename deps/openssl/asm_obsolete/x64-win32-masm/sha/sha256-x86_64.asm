OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	sha256_block_data_order

ALIGN	16
sha256_block_data_order	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_block_data_order::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	r11,QWORD PTR[OPENSSL_ia32cap_P]
	mov	r9d,DWORD PTR[r11]
	mov	r10d,DWORD PTR[4+r11]
	mov	r11d,DWORD PTR[8+r11]
	test	r11d,536870912
	jnz	_shaext_shortcut
	test	r10d,512
	jnz	$L$ssse3_shortcut
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	shl	rdx,4
	sub	rsp,16*4+4*8
	lea	rdx,QWORD PTR[rdx*4+rsi]
	and	rsp,-64
	mov	QWORD PTR[((64+0))+rsp],rdi
	mov	QWORD PTR[((64+8))+rsp],rsi
	mov	QWORD PTR[((64+16))+rsp],rdx
	mov	QWORD PTR[((64+24))+rsp],r11
$L$prologue::

	mov	eax,DWORD PTR[rdi]
	mov	ebx,DWORD PTR[4+rdi]
	mov	ecx,DWORD PTR[8+rdi]
	mov	edx,DWORD PTR[12+rdi]
	mov	r8d,DWORD PTR[16+rdi]
	mov	r9d,DWORD PTR[20+rdi]
	mov	r10d,DWORD PTR[24+rdi]
	mov	r11d,DWORD PTR[28+rdi]
	jmp	$L$loop

ALIGN	16
$L$loop::
	mov	edi,ebx
	lea	rbp,QWORD PTR[K256]
	xor	edi,ecx
	mov	r12d,DWORD PTR[rsi]
	mov	r13d,r8d
	mov	r14d,eax
	bswap	r12d
	ror	r13d,14
	mov	r15d,r9d

	xor	r13d,r8d
	ror	r14d,9
	xor	r15d,r10d

	mov	DWORD PTR[rsp],r12d
	xor	r14d,eax
	and	r15d,r8d

	ror	r13d,5
	add	r12d,r11d
	xor	r15d,r10d

	ror	r14d,11
	xor	r13d,r8d
	add	r12d,r15d

	mov	r15d,eax
	add	r12d,DWORD PTR[rbp]
	xor	r14d,eax

	xor	r15d,ebx
	ror	r13d,6
	mov	r11d,ebx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r11d,edi
	add	edx,r12d
	add	r11d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r11d,r14d
	mov	r12d,DWORD PTR[4+rsi]
	mov	r13d,edx
	mov	r14d,r11d
	bswap	r12d
	ror	r13d,14
	mov	edi,r8d

	xor	r13d,edx
	ror	r14d,9
	xor	edi,r9d

	mov	DWORD PTR[4+rsp],r12d
	xor	r14d,r11d
	and	edi,edx

	ror	r13d,5
	add	r12d,r10d
	xor	edi,r9d

	ror	r14d,11
	xor	r13d,edx
	add	r12d,edi

	mov	edi,r11d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r11d

	xor	edi,eax
	ror	r13d,6
	mov	r10d,eax

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r10d,r15d
	add	ecx,r12d
	add	r10d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r10d,r14d
	mov	r12d,DWORD PTR[8+rsi]
	mov	r13d,ecx
	mov	r14d,r10d
	bswap	r12d
	ror	r13d,14
	mov	r15d,edx

	xor	r13d,ecx
	ror	r14d,9
	xor	r15d,r8d

	mov	DWORD PTR[8+rsp],r12d
	xor	r14d,r10d
	and	r15d,ecx

	ror	r13d,5
	add	r12d,r9d
	xor	r15d,r8d

	ror	r14d,11
	xor	r13d,ecx
	add	r12d,r15d

	mov	r15d,r10d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r10d

	xor	r15d,r11d
	ror	r13d,6
	mov	r9d,r11d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r9d,edi
	add	ebx,r12d
	add	r9d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r9d,r14d
	mov	r12d,DWORD PTR[12+rsi]
	mov	r13d,ebx
	mov	r14d,r9d
	bswap	r12d
	ror	r13d,14
	mov	edi,ecx

	xor	r13d,ebx
	ror	r14d,9
	xor	edi,edx

	mov	DWORD PTR[12+rsp],r12d
	xor	r14d,r9d
	and	edi,ebx

	ror	r13d,5
	add	r12d,r8d
	xor	edi,edx

	ror	r14d,11
	xor	r13d,ebx
	add	r12d,edi

	mov	edi,r9d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r9d

	xor	edi,r10d
	ror	r13d,6
	mov	r8d,r10d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r8d,r15d
	add	eax,r12d
	add	r8d,r12d

	lea	rbp,QWORD PTR[20+rbp]
	add	r8d,r14d
	mov	r12d,DWORD PTR[16+rsi]
	mov	r13d,eax
	mov	r14d,r8d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ebx

	xor	r13d,eax
	ror	r14d,9
	xor	r15d,ecx

	mov	DWORD PTR[16+rsp],r12d
	xor	r14d,r8d
	and	r15d,eax

	ror	r13d,5
	add	r12d,edx
	xor	r15d,ecx

	ror	r14d,11
	xor	r13d,eax
	add	r12d,r15d

	mov	r15d,r8d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r8d

	xor	r15d,r9d
	ror	r13d,6
	mov	edx,r9d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	edx,edi
	add	r11d,r12d
	add	edx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	edx,r14d
	mov	r12d,DWORD PTR[20+rsi]
	mov	r13d,r11d
	mov	r14d,edx
	bswap	r12d
	ror	r13d,14
	mov	edi,eax

	xor	r13d,r11d
	ror	r14d,9
	xor	edi,ebx

	mov	DWORD PTR[20+rsp],r12d
	xor	r14d,edx
	and	edi,r11d

	ror	r13d,5
	add	r12d,ecx
	xor	edi,ebx

	ror	r14d,11
	xor	r13d,r11d
	add	r12d,edi

	mov	edi,edx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,edx

	xor	edi,r8d
	ror	r13d,6
	mov	ecx,r8d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	ecx,r15d
	add	r10d,r12d
	add	ecx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	ecx,r14d
	mov	r12d,DWORD PTR[24+rsi]
	mov	r13d,r10d
	mov	r14d,ecx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r11d

	xor	r13d,r10d
	ror	r14d,9
	xor	r15d,eax

	mov	DWORD PTR[24+rsp],r12d
	xor	r14d,ecx
	and	r15d,r10d

	ror	r13d,5
	add	r12d,ebx
	xor	r15d,eax

	ror	r14d,11
	xor	r13d,r10d
	add	r12d,r15d

	mov	r15d,ecx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ecx

	xor	r15d,edx
	ror	r13d,6
	mov	ebx,edx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	ebx,edi
	add	r9d,r12d
	add	ebx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	ebx,r14d
	mov	r12d,DWORD PTR[28+rsi]
	mov	r13d,r9d
	mov	r14d,ebx
	bswap	r12d
	ror	r13d,14
	mov	edi,r10d

	xor	r13d,r9d
	ror	r14d,9
	xor	edi,r11d

	mov	DWORD PTR[28+rsp],r12d
	xor	r14d,ebx
	and	edi,r9d

	ror	r13d,5
	add	r12d,eax
	xor	edi,r11d

	ror	r14d,11
	xor	r13d,r9d
	add	r12d,edi

	mov	edi,ebx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ebx

	xor	edi,ecx
	ror	r13d,6
	mov	eax,ecx

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	eax,r15d
	add	r8d,r12d
	add	eax,r12d

	lea	rbp,QWORD PTR[20+rbp]
	add	eax,r14d
	mov	r12d,DWORD PTR[32+rsi]
	mov	r13d,r8d
	mov	r14d,eax
	bswap	r12d
	ror	r13d,14
	mov	r15d,r9d

	xor	r13d,r8d
	ror	r14d,9
	xor	r15d,r10d

	mov	DWORD PTR[32+rsp],r12d
	xor	r14d,eax
	and	r15d,r8d

	ror	r13d,5
	add	r12d,r11d
	xor	r15d,r10d

	ror	r14d,11
	xor	r13d,r8d
	add	r12d,r15d

	mov	r15d,eax
	add	r12d,DWORD PTR[rbp]
	xor	r14d,eax

	xor	r15d,ebx
	ror	r13d,6
	mov	r11d,ebx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r11d,edi
	add	edx,r12d
	add	r11d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r11d,r14d
	mov	r12d,DWORD PTR[36+rsi]
	mov	r13d,edx
	mov	r14d,r11d
	bswap	r12d
	ror	r13d,14
	mov	edi,r8d

	xor	r13d,edx
	ror	r14d,9
	xor	edi,r9d

	mov	DWORD PTR[36+rsp],r12d
	xor	r14d,r11d
	and	edi,edx

	ror	r13d,5
	add	r12d,r10d
	xor	edi,r9d

	ror	r14d,11
	xor	r13d,edx
	add	r12d,edi

	mov	edi,r11d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r11d

	xor	edi,eax
	ror	r13d,6
	mov	r10d,eax

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r10d,r15d
	add	ecx,r12d
	add	r10d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r10d,r14d
	mov	r12d,DWORD PTR[40+rsi]
	mov	r13d,ecx
	mov	r14d,r10d
	bswap	r12d
	ror	r13d,14
	mov	r15d,edx

	xor	r13d,ecx
	ror	r14d,9
	xor	r15d,r8d

	mov	DWORD PTR[40+rsp],r12d
	xor	r14d,r10d
	and	r15d,ecx

	ror	r13d,5
	add	r12d,r9d
	xor	r15d,r8d

	ror	r14d,11
	xor	r13d,ecx
	add	r12d,r15d

	mov	r15d,r10d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r10d

	xor	r15d,r11d
	ror	r13d,6
	mov	r9d,r11d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r9d,edi
	add	ebx,r12d
	add	r9d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	r9d,r14d
	mov	r12d,DWORD PTR[44+rsi]
	mov	r13d,ebx
	mov	r14d,r9d
	bswap	r12d
	ror	r13d,14
	mov	edi,ecx

	xor	r13d,ebx
	ror	r14d,9
	xor	edi,edx

	mov	DWORD PTR[44+rsp],r12d
	xor	r14d,r9d
	and	edi,ebx

	ror	r13d,5
	add	r12d,r8d
	xor	edi,edx

	ror	r14d,11
	xor	r13d,ebx
	add	r12d,edi

	mov	edi,r9d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r9d

	xor	edi,r10d
	ror	r13d,6
	mov	r8d,r10d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r8d,r15d
	add	eax,r12d
	add	r8d,r12d

	lea	rbp,QWORD PTR[20+rbp]
	add	r8d,r14d
	mov	r12d,DWORD PTR[48+rsi]
	mov	r13d,eax
	mov	r14d,r8d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ebx

	xor	r13d,eax
	ror	r14d,9
	xor	r15d,ecx

	mov	DWORD PTR[48+rsp],r12d
	xor	r14d,r8d
	and	r15d,eax

	ror	r13d,5
	add	r12d,edx
	xor	r15d,ecx

	ror	r14d,11
	xor	r13d,eax
	add	r12d,r15d

	mov	r15d,r8d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r8d

	xor	r15d,r9d
	ror	r13d,6
	mov	edx,r9d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	edx,edi
	add	r11d,r12d
	add	edx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	edx,r14d
	mov	r12d,DWORD PTR[52+rsi]
	mov	r13d,r11d
	mov	r14d,edx
	bswap	r12d
	ror	r13d,14
	mov	edi,eax

	xor	r13d,r11d
	ror	r14d,9
	xor	edi,ebx

	mov	DWORD PTR[52+rsp],r12d
	xor	r14d,edx
	and	edi,r11d

	ror	r13d,5
	add	r12d,ecx
	xor	edi,ebx

	ror	r14d,11
	xor	r13d,r11d
	add	r12d,edi

	mov	edi,edx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,edx

	xor	edi,r8d
	ror	r13d,6
	mov	ecx,r8d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	ecx,r15d
	add	r10d,r12d
	add	ecx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	ecx,r14d
	mov	r12d,DWORD PTR[56+rsi]
	mov	r13d,r10d
	mov	r14d,ecx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r11d

	xor	r13d,r10d
	ror	r14d,9
	xor	r15d,eax

	mov	DWORD PTR[56+rsp],r12d
	xor	r14d,ecx
	and	r15d,r10d

	ror	r13d,5
	add	r12d,ebx
	xor	r15d,eax

	ror	r14d,11
	xor	r13d,r10d
	add	r12d,r15d

	mov	r15d,ecx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ecx

	xor	r15d,edx
	ror	r13d,6
	mov	ebx,edx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	ebx,edi
	add	r9d,r12d
	add	ebx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	add	ebx,r14d
	mov	r12d,DWORD PTR[60+rsi]
	mov	r13d,r9d
	mov	r14d,ebx
	bswap	r12d
	ror	r13d,14
	mov	edi,r10d

	xor	r13d,r9d
	ror	r14d,9
	xor	edi,r11d

	mov	DWORD PTR[60+rsp],r12d
	xor	r14d,ebx
	and	edi,r9d

	ror	r13d,5
	add	r12d,eax
	xor	edi,r11d

	ror	r14d,11
	xor	r13d,r9d
	add	r12d,edi

	mov	edi,ebx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ebx

	xor	edi,ecx
	ror	r13d,6
	mov	eax,ecx

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	eax,r15d
	add	r8d,r12d
	add	eax,r12d

	lea	rbp,QWORD PTR[20+rbp]
	jmp	$L$rounds_16_xx
ALIGN	16
$L$rounds_16_xx::
	mov	r13d,DWORD PTR[4+rsp]
	mov	r15d,DWORD PTR[56+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	eax,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[36+rsp]

	add	r12d,DWORD PTR[rsp]
	mov	r13d,r8d
	add	r12d,r15d
	mov	r14d,eax
	ror	r13d,14
	mov	r15d,r9d

	xor	r13d,r8d
	ror	r14d,9
	xor	r15d,r10d

	mov	DWORD PTR[rsp],r12d
	xor	r14d,eax
	and	r15d,r8d

	ror	r13d,5
	add	r12d,r11d
	xor	r15d,r10d

	ror	r14d,11
	xor	r13d,r8d
	add	r12d,r15d

	mov	r15d,eax
	add	r12d,DWORD PTR[rbp]
	xor	r14d,eax

	xor	r15d,ebx
	ror	r13d,6
	mov	r11d,ebx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r11d,edi
	add	edx,r12d
	add	r11d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[8+rsp]
	mov	edi,DWORD PTR[60+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r11d,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[40+rsp]

	add	r12d,DWORD PTR[4+rsp]
	mov	r13d,edx
	add	r12d,edi
	mov	r14d,r11d
	ror	r13d,14
	mov	edi,r8d

	xor	r13d,edx
	ror	r14d,9
	xor	edi,r9d

	mov	DWORD PTR[4+rsp],r12d
	xor	r14d,r11d
	and	edi,edx

	ror	r13d,5
	add	r12d,r10d
	xor	edi,r9d

	ror	r14d,11
	xor	r13d,edx
	add	r12d,edi

	mov	edi,r11d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r11d

	xor	edi,eax
	ror	r13d,6
	mov	r10d,eax

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r10d,r15d
	add	ecx,r12d
	add	r10d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[12+rsp]
	mov	r15d,DWORD PTR[rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r10d,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[44+rsp]

	add	r12d,DWORD PTR[8+rsp]
	mov	r13d,ecx
	add	r12d,r15d
	mov	r14d,r10d
	ror	r13d,14
	mov	r15d,edx

	xor	r13d,ecx
	ror	r14d,9
	xor	r15d,r8d

	mov	DWORD PTR[8+rsp],r12d
	xor	r14d,r10d
	and	r15d,ecx

	ror	r13d,5
	add	r12d,r9d
	xor	r15d,r8d

	ror	r14d,11
	xor	r13d,ecx
	add	r12d,r15d

	mov	r15d,r10d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r10d

	xor	r15d,r11d
	ror	r13d,6
	mov	r9d,r11d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r9d,edi
	add	ebx,r12d
	add	r9d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[16+rsp]
	mov	edi,DWORD PTR[4+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r9d,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[48+rsp]

	add	r12d,DWORD PTR[12+rsp]
	mov	r13d,ebx
	add	r12d,edi
	mov	r14d,r9d
	ror	r13d,14
	mov	edi,ecx

	xor	r13d,ebx
	ror	r14d,9
	xor	edi,edx

	mov	DWORD PTR[12+rsp],r12d
	xor	r14d,r9d
	and	edi,ebx

	ror	r13d,5
	add	r12d,r8d
	xor	edi,edx

	ror	r14d,11
	xor	r13d,ebx
	add	r12d,edi

	mov	edi,r9d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r9d

	xor	edi,r10d
	ror	r13d,6
	mov	r8d,r10d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r8d,r15d
	add	eax,r12d
	add	r8d,r12d

	lea	rbp,QWORD PTR[20+rbp]
	mov	r13d,DWORD PTR[20+rsp]
	mov	r15d,DWORD PTR[8+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r8d,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[52+rsp]

	add	r12d,DWORD PTR[16+rsp]
	mov	r13d,eax
	add	r12d,r15d
	mov	r14d,r8d
	ror	r13d,14
	mov	r15d,ebx

	xor	r13d,eax
	ror	r14d,9
	xor	r15d,ecx

	mov	DWORD PTR[16+rsp],r12d
	xor	r14d,r8d
	and	r15d,eax

	ror	r13d,5
	add	r12d,edx
	xor	r15d,ecx

	ror	r14d,11
	xor	r13d,eax
	add	r12d,r15d

	mov	r15d,r8d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r8d

	xor	r15d,r9d
	ror	r13d,6
	mov	edx,r9d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	edx,edi
	add	r11d,r12d
	add	edx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[24+rsp]
	mov	edi,DWORD PTR[12+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	edx,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[56+rsp]

	add	r12d,DWORD PTR[20+rsp]
	mov	r13d,r11d
	add	r12d,edi
	mov	r14d,edx
	ror	r13d,14
	mov	edi,eax

	xor	r13d,r11d
	ror	r14d,9
	xor	edi,ebx

	mov	DWORD PTR[20+rsp],r12d
	xor	r14d,edx
	and	edi,r11d

	ror	r13d,5
	add	r12d,ecx
	xor	edi,ebx

	ror	r14d,11
	xor	r13d,r11d
	add	r12d,edi

	mov	edi,edx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,edx

	xor	edi,r8d
	ror	r13d,6
	mov	ecx,r8d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	ecx,r15d
	add	r10d,r12d
	add	ecx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[28+rsp]
	mov	r15d,DWORD PTR[16+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	ecx,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[60+rsp]

	add	r12d,DWORD PTR[24+rsp]
	mov	r13d,r10d
	add	r12d,r15d
	mov	r14d,ecx
	ror	r13d,14
	mov	r15d,r11d

	xor	r13d,r10d
	ror	r14d,9
	xor	r15d,eax

	mov	DWORD PTR[24+rsp],r12d
	xor	r14d,ecx
	and	r15d,r10d

	ror	r13d,5
	add	r12d,ebx
	xor	r15d,eax

	ror	r14d,11
	xor	r13d,r10d
	add	r12d,r15d

	mov	r15d,ecx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ecx

	xor	r15d,edx
	ror	r13d,6
	mov	ebx,edx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	ebx,edi
	add	r9d,r12d
	add	ebx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[32+rsp]
	mov	edi,DWORD PTR[20+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	ebx,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[rsp]

	add	r12d,DWORD PTR[28+rsp]
	mov	r13d,r9d
	add	r12d,edi
	mov	r14d,ebx
	ror	r13d,14
	mov	edi,r10d

	xor	r13d,r9d
	ror	r14d,9
	xor	edi,r11d

	mov	DWORD PTR[28+rsp],r12d
	xor	r14d,ebx
	and	edi,r9d

	ror	r13d,5
	add	r12d,eax
	xor	edi,r11d

	ror	r14d,11
	xor	r13d,r9d
	add	r12d,edi

	mov	edi,ebx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ebx

	xor	edi,ecx
	ror	r13d,6
	mov	eax,ecx

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	eax,r15d
	add	r8d,r12d
	add	eax,r12d

	lea	rbp,QWORD PTR[20+rbp]
	mov	r13d,DWORD PTR[36+rsp]
	mov	r15d,DWORD PTR[24+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	eax,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[4+rsp]

	add	r12d,DWORD PTR[32+rsp]
	mov	r13d,r8d
	add	r12d,r15d
	mov	r14d,eax
	ror	r13d,14
	mov	r15d,r9d

	xor	r13d,r8d
	ror	r14d,9
	xor	r15d,r10d

	mov	DWORD PTR[32+rsp],r12d
	xor	r14d,eax
	and	r15d,r8d

	ror	r13d,5
	add	r12d,r11d
	xor	r15d,r10d

	ror	r14d,11
	xor	r13d,r8d
	add	r12d,r15d

	mov	r15d,eax
	add	r12d,DWORD PTR[rbp]
	xor	r14d,eax

	xor	r15d,ebx
	ror	r13d,6
	mov	r11d,ebx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r11d,edi
	add	edx,r12d
	add	r11d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[40+rsp]
	mov	edi,DWORD PTR[28+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r11d,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[8+rsp]

	add	r12d,DWORD PTR[36+rsp]
	mov	r13d,edx
	add	r12d,edi
	mov	r14d,r11d
	ror	r13d,14
	mov	edi,r8d

	xor	r13d,edx
	ror	r14d,9
	xor	edi,r9d

	mov	DWORD PTR[36+rsp],r12d
	xor	r14d,r11d
	and	edi,edx

	ror	r13d,5
	add	r12d,r10d
	xor	edi,r9d

	ror	r14d,11
	xor	r13d,edx
	add	r12d,edi

	mov	edi,r11d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r11d

	xor	edi,eax
	ror	r13d,6
	mov	r10d,eax

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r10d,r15d
	add	ecx,r12d
	add	r10d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[44+rsp]
	mov	r15d,DWORD PTR[32+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r10d,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[12+rsp]

	add	r12d,DWORD PTR[40+rsp]
	mov	r13d,ecx
	add	r12d,r15d
	mov	r14d,r10d
	ror	r13d,14
	mov	r15d,edx

	xor	r13d,ecx
	ror	r14d,9
	xor	r15d,r8d

	mov	DWORD PTR[40+rsp],r12d
	xor	r14d,r10d
	and	r15d,ecx

	ror	r13d,5
	add	r12d,r9d
	xor	r15d,r8d

	ror	r14d,11
	xor	r13d,ecx
	add	r12d,r15d

	mov	r15d,r10d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r10d

	xor	r15d,r11d
	ror	r13d,6
	mov	r9d,r11d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	r9d,edi
	add	ebx,r12d
	add	r9d,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[48+rsp]
	mov	edi,DWORD PTR[36+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r9d,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[16+rsp]

	add	r12d,DWORD PTR[44+rsp]
	mov	r13d,ebx
	add	r12d,edi
	mov	r14d,r9d
	ror	r13d,14
	mov	edi,ecx

	xor	r13d,ebx
	ror	r14d,9
	xor	edi,edx

	mov	DWORD PTR[44+rsp],r12d
	xor	r14d,r9d
	and	edi,ebx

	ror	r13d,5
	add	r12d,r8d
	xor	edi,edx

	ror	r14d,11
	xor	r13d,ebx
	add	r12d,edi

	mov	edi,r9d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r9d

	xor	edi,r10d
	ror	r13d,6
	mov	r8d,r10d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	r8d,r15d
	add	eax,r12d
	add	r8d,r12d

	lea	rbp,QWORD PTR[20+rbp]
	mov	r13d,DWORD PTR[52+rsp]
	mov	r15d,DWORD PTR[40+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	r8d,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[20+rsp]

	add	r12d,DWORD PTR[48+rsp]
	mov	r13d,eax
	add	r12d,r15d
	mov	r14d,r8d
	ror	r13d,14
	mov	r15d,ebx

	xor	r13d,eax
	ror	r14d,9
	xor	r15d,ecx

	mov	DWORD PTR[48+rsp],r12d
	xor	r14d,r8d
	and	r15d,eax

	ror	r13d,5
	add	r12d,edx
	xor	r15d,ecx

	ror	r14d,11
	xor	r13d,eax
	add	r12d,r15d

	mov	r15d,r8d
	add	r12d,DWORD PTR[rbp]
	xor	r14d,r8d

	xor	r15d,r9d
	ror	r13d,6
	mov	edx,r9d

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	edx,edi
	add	r11d,r12d
	add	edx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[56+rsp]
	mov	edi,DWORD PTR[44+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	edx,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[24+rsp]

	add	r12d,DWORD PTR[52+rsp]
	mov	r13d,r11d
	add	r12d,edi
	mov	r14d,edx
	ror	r13d,14
	mov	edi,eax

	xor	r13d,r11d
	ror	r14d,9
	xor	edi,ebx

	mov	DWORD PTR[52+rsp],r12d
	xor	r14d,edx
	and	edi,r11d

	ror	r13d,5
	add	r12d,ecx
	xor	edi,ebx

	ror	r14d,11
	xor	r13d,r11d
	add	r12d,edi

	mov	edi,edx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,edx

	xor	edi,r8d
	ror	r13d,6
	mov	ecx,r8d

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	ecx,r15d
	add	r10d,r12d
	add	ecx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[60+rsp]
	mov	r15d,DWORD PTR[48+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	ecx,r14d
	mov	r14d,r15d
	ror	r15d,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	xor	r12d,r13d
	xor	r15d,r14d
	add	r12d,DWORD PTR[28+rsp]

	add	r12d,DWORD PTR[56+rsp]
	mov	r13d,r10d
	add	r12d,r15d
	mov	r14d,ecx
	ror	r13d,14
	mov	r15d,r11d

	xor	r13d,r10d
	ror	r14d,9
	xor	r15d,eax

	mov	DWORD PTR[56+rsp],r12d
	xor	r14d,ecx
	and	r15d,r10d

	ror	r13d,5
	add	r12d,ebx
	xor	r15d,eax

	ror	r14d,11
	xor	r13d,r10d
	add	r12d,r15d

	mov	r15d,ecx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ecx

	xor	r15d,edx
	ror	r13d,6
	mov	ebx,edx

	and	edi,r15d
	ror	r14d,2
	add	r12d,r13d

	xor	ebx,edi
	add	r9d,r12d
	add	ebx,r12d

	lea	rbp,QWORD PTR[4+rbp]
	mov	r13d,DWORD PTR[rsp]
	mov	edi,DWORD PTR[52+rsp]

	mov	r12d,r13d
	ror	r13d,11
	add	ebx,r14d
	mov	r14d,edi
	ror	edi,2

	xor	r13d,r12d
	shr	r12d,3
	ror	r13d,7
	xor	edi,r14d
	shr	r14d,10

	ror	edi,17
	xor	r12d,r13d
	xor	edi,r14d
	add	r12d,DWORD PTR[32+rsp]

	add	r12d,DWORD PTR[60+rsp]
	mov	r13d,r9d
	add	r12d,edi
	mov	r14d,ebx
	ror	r13d,14
	mov	edi,r10d

	xor	r13d,r9d
	ror	r14d,9
	xor	edi,r11d

	mov	DWORD PTR[60+rsp],r12d
	xor	r14d,ebx
	and	edi,r9d

	ror	r13d,5
	add	r12d,eax
	xor	edi,r11d

	ror	r14d,11
	xor	r13d,r9d
	add	r12d,edi

	mov	edi,ebx
	add	r12d,DWORD PTR[rbp]
	xor	r14d,ebx

	xor	edi,ecx
	ror	r13d,6
	mov	eax,ecx

	and	r15d,edi
	ror	r14d,2
	add	r12d,r13d

	xor	eax,r15d
	add	r8d,r12d
	add	eax,r12d

	lea	rbp,QWORD PTR[20+rbp]
	cmp	BYTE PTR[3+rbp],0
	jnz	$L$rounds_16_xx

	mov	rdi,QWORD PTR[((64+0))+rsp]
	add	eax,r14d
	lea	rsi,QWORD PTR[64+rsi]

	add	eax,DWORD PTR[rdi]
	add	ebx,DWORD PTR[4+rdi]
	add	ecx,DWORD PTR[8+rdi]
	add	edx,DWORD PTR[12+rdi]
	add	r8d,DWORD PTR[16+rdi]
	add	r9d,DWORD PTR[20+rdi]
	add	r10d,DWORD PTR[24+rdi]
	add	r11d,DWORD PTR[28+rdi]

	cmp	rsi,QWORD PTR[((64+16))+rsp]

	mov	DWORD PTR[rdi],eax
	mov	DWORD PTR[4+rdi],ebx
	mov	DWORD PTR[8+rdi],ecx
	mov	DWORD PTR[12+rdi],edx
	mov	DWORD PTR[16+rdi],r8d
	mov	DWORD PTR[20+rdi],r9d
	mov	DWORD PTR[24+rdi],r10d
	mov	DWORD PTR[28+rdi],r11d
	jb	$L$loop

	mov	rsi,QWORD PTR[((64+24))+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha256_block_data_order::
sha256_block_data_order	ENDP
ALIGN	64

K256::
	DD	0428a2f98h,071374491h,0b5c0fbcfh,0e9b5dba5h
	DD	0428a2f98h,071374491h,0b5c0fbcfh,0e9b5dba5h
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h

	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	003020100h,00b0a0908h,0ffffffffh,0ffffffffh
	DD	003020100h,00b0a0908h,0ffffffffh,0ffffffffh
	DD	0ffffffffh,0ffffffffh,003020100h,00b0a0908h
	DD	0ffffffffh,0ffffffffh,003020100h,00b0a0908h
DB	83,72,65,50,53,54,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,95,54
DB	52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121
DB	32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46
DB	111,114,103,62,0

ALIGN	64
sha256_block_data_order_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_block_data_order_shaext::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_shaext_shortcut::
	lea	rsp,QWORD PTR[((-88))+rsp]
	movaps	XMMWORD PTR[(-8-80)+rax],xmm6
	movaps	XMMWORD PTR[(-8-64)+rax],xmm7
	movaps	XMMWORD PTR[(-8-48)+rax],xmm8
	movaps	XMMWORD PTR[(-8-32)+rax],xmm9
	movaps	XMMWORD PTR[(-8-16)+rax],xmm10
$L$prologue_shaext::
	lea	rcx,QWORD PTR[((K256+128))]
	movdqu	xmm1,XMMWORD PTR[rdi]
	movdqu	xmm2,XMMWORD PTR[16+rdi]
	movdqa	xmm7,XMMWORD PTR[((512-128))+rcx]

	pshufd	xmm0,xmm1,01bh
	pshufd	xmm1,xmm1,0b1h
	pshufd	xmm2,xmm2,01bh
	movdqa	xmm8,xmm7
DB	102,15,58,15,202,8
	punpcklqdq	xmm2,xmm0
	jmp	$L$oop_shaext

ALIGN	16
$L$oop_shaext::
	movdqu	xmm3,XMMWORD PTR[rsi]
	movdqu	xmm4,XMMWORD PTR[16+rsi]
	movdqu	xmm5,XMMWORD PTR[32+rsi]
DB	102,15,56,0,223
	movdqu	xmm6,XMMWORD PTR[48+rsi]

	movdqa	xmm0,XMMWORD PTR[((0-128))+rcx]
	paddd	xmm0,xmm3
DB	102,15,56,0,231
	movdqa	xmm10,xmm2
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	nop
	movdqa	xmm9,xmm1
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((32-128))+rcx]
	paddd	xmm0,xmm4
DB	102,15,56,0,239
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	lea	rsi,QWORD PTR[64+rsi]
DB	15,56,204,220
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((64-128))+rcx]
	paddd	xmm0,xmm5
DB	102,15,56,0,247
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm6
DB	102,15,58,15,253,4
	nop
	paddd	xmm3,xmm7
DB	15,56,204,229
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((96-128))+rcx]
	paddd	xmm0,xmm6
DB	15,56,205,222
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm3
DB	102,15,58,15,254,4
	nop
	paddd	xmm4,xmm7
DB	15,56,204,238
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((128-128))+rcx]
	paddd	xmm0,xmm3
DB	15,56,205,227
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm4
DB	102,15,58,15,251,4
	nop
	paddd	xmm5,xmm7
DB	15,56,204,243
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((160-128))+rcx]
	paddd	xmm0,xmm4
DB	15,56,205,236
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm5
DB	102,15,58,15,252,4
	nop
	paddd	xmm6,xmm7
DB	15,56,204,220
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((192-128))+rcx]
	paddd	xmm0,xmm5
DB	15,56,205,245
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm6
DB	102,15,58,15,253,4
	nop
	paddd	xmm3,xmm7
DB	15,56,204,229
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((224-128))+rcx]
	paddd	xmm0,xmm6
DB	15,56,205,222
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm3
DB	102,15,58,15,254,4
	nop
	paddd	xmm4,xmm7
DB	15,56,204,238
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((256-128))+rcx]
	paddd	xmm0,xmm3
DB	15,56,205,227
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm4
DB	102,15,58,15,251,4
	nop
	paddd	xmm5,xmm7
DB	15,56,204,243
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((288-128))+rcx]
	paddd	xmm0,xmm4
DB	15,56,205,236
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm5
DB	102,15,58,15,252,4
	nop
	paddd	xmm6,xmm7
DB	15,56,204,220
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((320-128))+rcx]
	paddd	xmm0,xmm5
DB	15,56,205,245
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm6
DB	102,15,58,15,253,4
	nop
	paddd	xmm3,xmm7
DB	15,56,204,229
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((352-128))+rcx]
	paddd	xmm0,xmm6
DB	15,56,205,222
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm3
DB	102,15,58,15,254,4
	nop
	paddd	xmm4,xmm7
DB	15,56,204,238
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((384-128))+rcx]
	paddd	xmm0,xmm3
DB	15,56,205,227
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm4
DB	102,15,58,15,251,4
	nop
	paddd	xmm5,xmm7
DB	15,56,204,243
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((416-128))+rcx]
	paddd	xmm0,xmm4
DB	15,56,205,236
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm7,xmm5
DB	102,15,58,15,252,4
DB	15,56,203,202
	paddd	xmm6,xmm7

	movdqa	xmm0,XMMWORD PTR[((448-128))+rcx]
	paddd	xmm0,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
DB	15,56,205,245
	movdqa	xmm7,xmm8
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((480-128))+rcx]
	paddd	xmm0,xmm6
	nop
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	dec	rdx
	nop
DB	15,56,203,202

	paddd	xmm2,xmm10
	paddd	xmm1,xmm9
	jnz	$L$oop_shaext

	pshufd	xmm2,xmm2,0b1h
	pshufd	xmm7,xmm1,01bh
	pshufd	xmm1,xmm1,0b1h
	punpckhqdq	xmm1,xmm2
DB	102,15,58,15,215,8

	movdqu	XMMWORD PTR[rdi],xmm1
	movdqu	XMMWORD PTR[16+rdi],xmm2
	movaps	xmm6,XMMWORD PTR[((-8-80))+rax]
	movaps	xmm7,XMMWORD PTR[((-8-64))+rax]
	movaps	xmm8,XMMWORD PTR[((-8-48))+rax]
	movaps	xmm9,XMMWORD PTR[((-8-32))+rax]
	movaps	xmm10,XMMWORD PTR[((-8-16))+rax]
	mov	rsp,rax
$L$epilogue_shaext::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha256_block_data_order_shaext::
sha256_block_data_order_shaext	ENDP

ALIGN	64
sha256_block_data_order_ssse3	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_block_data_order_ssse3::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


$L$ssse3_shortcut::
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	shl	rdx,4
	sub	rsp,160
	lea	rdx,QWORD PTR[rdx*4+rsi]
	and	rsp,-64
	mov	QWORD PTR[((64+0))+rsp],rdi
	mov	QWORD PTR[((64+8))+rsp],rsi
	mov	QWORD PTR[((64+16))+rsp],rdx
	mov	QWORD PTR[((64+24))+rsp],r11
	movaps	XMMWORD PTR[(64+32)+rsp],xmm6
	movaps	XMMWORD PTR[(64+48)+rsp],xmm7
	movaps	XMMWORD PTR[(64+64)+rsp],xmm8
	movaps	XMMWORD PTR[(64+80)+rsp],xmm9
$L$prologue_ssse3::

	mov	eax,DWORD PTR[rdi]
	mov	ebx,DWORD PTR[4+rdi]
	mov	ecx,DWORD PTR[8+rdi]
	mov	edx,DWORD PTR[12+rdi]
	mov	r8d,DWORD PTR[16+rdi]
	mov	r9d,DWORD PTR[20+rdi]
	mov	r10d,DWORD PTR[24+rdi]
	mov	r11d,DWORD PTR[28+rdi]


	jmp	$L$loop_ssse3
ALIGN	16
$L$loop_ssse3::
	movdqa	xmm7,XMMWORD PTR[((K256+512))]
	movdqu	xmm0,XMMWORD PTR[rsi]
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
DB	102,15,56,0,199
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	lea	rbp,QWORD PTR[K256]
DB	102,15,56,0,207
	movdqa	xmm4,XMMWORD PTR[rbp]
	movdqa	xmm5,XMMWORD PTR[32+rbp]
DB	102,15,56,0,215
	paddd	xmm4,xmm0
	movdqa	xmm6,XMMWORD PTR[64+rbp]
DB	102,15,56,0,223
	movdqa	xmm7,XMMWORD PTR[96+rbp]
	paddd	xmm5,xmm1
	paddd	xmm6,xmm2
	paddd	xmm7,xmm3
	movdqa	XMMWORD PTR[rsp],xmm4
	mov	r14d,eax
	movdqa	XMMWORD PTR[16+rsp],xmm5
	mov	edi,ebx
	movdqa	XMMWORD PTR[32+rsp],xmm6
	xor	edi,ecx
	movdqa	XMMWORD PTR[48+rsp],xmm7
	mov	r13d,r8d
	jmp	$L$ssse3_00_47

ALIGN	16
$L$ssse3_00_47::
	sub	rbp,-128
	ror	r13d,14
	movdqa	xmm4,xmm1
	mov	eax,r14d
	mov	r12d,r9d
	movdqa	xmm7,xmm3
	ror	r14d,9
	xor	r13d,r8d
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
DB	102,15,58,15,224,4
	and	r12d,r8d
	xor	r13d,r8d
DB	102,15,58,15,250,4
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
	xor	r12d,r10d
	ror	r14d,11
	movdqa	xmm5,xmm4
	xor	r15d,ebx
	add	r11d,r12d
	movdqa	xmm6,xmm4
	ror	r13d,6
	and	edi,r15d
	psrld	xmm4,3
	xor	r14d,eax
	add	r11d,r13d
	xor	edi,ebx
	paddd	xmm0,xmm7
	ror	r14d,2
	add	edx,r11d
	psrld	xmm6,7
	add	r11d,edi
	mov	r13d,edx
	pshufd	xmm7,xmm3,250
	add	r14d,r11d
	ror	r13d,14
	pslld	xmm5,14
	mov	r11d,r14d
	mov	r12d,r8d
	pxor	xmm4,xmm6
	ror	r14d,9
	xor	r13d,edx
	xor	r12d,r9d
	ror	r13d,5
	psrld	xmm6,11
	xor	r14d,r11d
	pxor	xmm4,xmm5
	and	r12d,edx
	xor	r13d,edx
	pslld	xmm5,11
	add	r10d,DWORD PTR[4+rsp]
	mov	edi,r11d
	pxor	xmm4,xmm6
	xor	r12d,r9d
	ror	r14d,11
	movdqa	xmm6,xmm7
	xor	edi,eax
	add	r10d,r12d
	pxor	xmm4,xmm5
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r11d
	psrld	xmm7,10
	add	r10d,r13d
	xor	r15d,eax
	paddd	xmm0,xmm4
	ror	r14d,2
	add	ecx,r10d
	psrlq	xmm6,17
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	pxor	xmm7,xmm6
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	ror	r14d,9
	psrlq	xmm6,2
	xor	r13d,ecx
	xor	r12d,r8d
	pxor	xmm7,xmm6
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	pshufd	xmm7,xmm7,128
	xor	r13d,ecx
	add	r9d,DWORD PTR[8+rsp]
	mov	r15d,r10d
	psrldq	xmm7,8
	xor	r12d,r8d
	ror	r14d,11
	xor	r15d,r11d
	add	r9d,r12d
	ror	r13d,6
	paddd	xmm0,xmm7
	and	edi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	pshufd	xmm7,xmm0,80
	xor	edi,r11d
	ror	r14d,2
	add	ebx,r9d
	movdqa	xmm6,xmm7
	add	r9d,edi
	mov	r13d,ebx
	psrld	xmm7,10
	add	r14d,r9d
	ror	r13d,14
	psrlq	xmm6,17
	mov	r9d,r14d
	mov	r12d,ecx
	pxor	xmm7,xmm6
	ror	r14d,9
	xor	r13d,ebx
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	psrlq	xmm6,2
	and	r12d,ebx
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	pxor	xmm7,xmm6
	mov	edi,r9d
	xor	r12d,edx
	ror	r14d,11
	pshufd	xmm7,xmm7,8
	xor	edi,r10d
	add	r8d,r12d
	movdqa	xmm6,XMMWORD PTR[rbp]
	ror	r13d,6
	and	r15d,edi
	pslldq	xmm7,8
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	paddd	xmm0,xmm7
	ror	r14d,2
	add	eax,r8d
	add	r8d,r15d
	paddd	xmm6,xmm0
	mov	r13d,eax
	add	r14d,r8d
	movdqa	XMMWORD PTR[rsp],xmm6
	ror	r13d,14
	movdqa	xmm4,xmm2
	mov	r8d,r14d
	mov	r12d,ebx
	movdqa	xmm7,xmm0
	ror	r14d,9
	xor	r13d,eax
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
DB	102,15,58,15,225,4
	and	r12d,eax
	xor	r13d,eax
DB	102,15,58,15,251,4
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
	xor	r12d,ecx
	ror	r14d,11
	movdqa	xmm5,xmm4
	xor	r15d,r9d
	add	edx,r12d
	movdqa	xmm6,xmm4
	ror	r13d,6
	and	edi,r15d
	psrld	xmm4,3
	xor	r14d,r8d
	add	edx,r13d
	xor	edi,r9d
	paddd	xmm1,xmm7
	ror	r14d,2
	add	r11d,edx
	psrld	xmm6,7
	add	edx,edi
	mov	r13d,r11d
	pshufd	xmm7,xmm0,250
	add	r14d,edx
	ror	r13d,14
	pslld	xmm5,14
	mov	edx,r14d
	mov	r12d,eax
	pxor	xmm4,xmm6
	ror	r14d,9
	xor	r13d,r11d
	xor	r12d,ebx
	ror	r13d,5
	psrld	xmm6,11
	xor	r14d,edx
	pxor	xmm4,xmm5
	and	r12d,r11d
	xor	r13d,r11d
	pslld	xmm5,11
	add	ecx,DWORD PTR[20+rsp]
	mov	edi,edx
	pxor	xmm4,xmm6
	xor	r12d,ebx
	ror	r14d,11
	movdqa	xmm6,xmm7
	xor	edi,r8d
	add	ecx,r12d
	pxor	xmm4,xmm5
	ror	r13d,6
	and	r15d,edi
	xor	r14d,edx
	psrld	xmm7,10
	add	ecx,r13d
	xor	r15d,r8d
	paddd	xmm1,xmm4
	ror	r14d,2
	add	r10d,ecx
	psrlq	xmm6,17
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	pxor	xmm7,xmm6
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	ror	r14d,9
	psrlq	xmm6,2
	xor	r13d,r10d
	xor	r12d,eax
	pxor	xmm7,xmm6
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	pshufd	xmm7,xmm7,128
	xor	r13d,r10d
	add	ebx,DWORD PTR[24+rsp]
	mov	r15d,ecx
	psrldq	xmm7,8
	xor	r12d,eax
	ror	r14d,11
	xor	r15d,edx
	add	ebx,r12d
	ror	r13d,6
	paddd	xmm1,xmm7
	and	edi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	pshufd	xmm7,xmm1,80
	xor	edi,edx
	ror	r14d,2
	add	r9d,ebx
	movdqa	xmm6,xmm7
	add	ebx,edi
	mov	r13d,r9d
	psrld	xmm7,10
	add	r14d,ebx
	ror	r13d,14
	psrlq	xmm6,17
	mov	ebx,r14d
	mov	r12d,r10d
	pxor	xmm7,xmm6
	ror	r14d,9
	xor	r13d,r9d
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	psrlq	xmm6,2
	and	r12d,r9d
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	pxor	xmm7,xmm6
	mov	edi,ebx
	xor	r12d,r11d
	ror	r14d,11
	pshufd	xmm7,xmm7,8
	xor	edi,ecx
	add	eax,r12d
	movdqa	xmm6,XMMWORD PTR[32+rbp]
	ror	r13d,6
	and	r15d,edi
	pslldq	xmm7,8
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	paddd	xmm1,xmm7
	ror	r14d,2
	add	r8d,eax
	add	eax,r15d
	paddd	xmm6,xmm1
	mov	r13d,r8d
	add	r14d,eax
	movdqa	XMMWORD PTR[16+rsp],xmm6
	ror	r13d,14
	movdqa	xmm4,xmm3
	mov	eax,r14d
	mov	r12d,r9d
	movdqa	xmm7,xmm1
	ror	r14d,9
	xor	r13d,r8d
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
DB	102,15,58,15,226,4
	and	r12d,r8d
	xor	r13d,r8d
DB	102,15,58,15,248,4
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
	xor	r12d,r10d
	ror	r14d,11
	movdqa	xmm5,xmm4
	xor	r15d,ebx
	add	r11d,r12d
	movdqa	xmm6,xmm4
	ror	r13d,6
	and	edi,r15d
	psrld	xmm4,3
	xor	r14d,eax
	add	r11d,r13d
	xor	edi,ebx
	paddd	xmm2,xmm7
	ror	r14d,2
	add	edx,r11d
	psrld	xmm6,7
	add	r11d,edi
	mov	r13d,edx
	pshufd	xmm7,xmm1,250
	add	r14d,r11d
	ror	r13d,14
	pslld	xmm5,14
	mov	r11d,r14d
	mov	r12d,r8d
	pxor	xmm4,xmm6
	ror	r14d,9
	xor	r13d,edx
	xor	r12d,r9d
	ror	r13d,5
	psrld	xmm6,11
	xor	r14d,r11d
	pxor	xmm4,xmm5
	and	r12d,edx
	xor	r13d,edx
	pslld	xmm5,11
	add	r10d,DWORD PTR[36+rsp]
	mov	edi,r11d
	pxor	xmm4,xmm6
	xor	r12d,r9d
	ror	r14d,11
	movdqa	xmm6,xmm7
	xor	edi,eax
	add	r10d,r12d
	pxor	xmm4,xmm5
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r11d
	psrld	xmm7,10
	add	r10d,r13d
	xor	r15d,eax
	paddd	xmm2,xmm4
	ror	r14d,2
	add	ecx,r10d
	psrlq	xmm6,17
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	pxor	xmm7,xmm6
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	ror	r14d,9
	psrlq	xmm6,2
	xor	r13d,ecx
	xor	r12d,r8d
	pxor	xmm7,xmm6
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	pshufd	xmm7,xmm7,128
	xor	r13d,ecx
	add	r9d,DWORD PTR[40+rsp]
	mov	r15d,r10d
	psrldq	xmm7,8
	xor	r12d,r8d
	ror	r14d,11
	xor	r15d,r11d
	add	r9d,r12d
	ror	r13d,6
	paddd	xmm2,xmm7
	and	edi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	pshufd	xmm7,xmm2,80
	xor	edi,r11d
	ror	r14d,2
	add	ebx,r9d
	movdqa	xmm6,xmm7
	add	r9d,edi
	mov	r13d,ebx
	psrld	xmm7,10
	add	r14d,r9d
	ror	r13d,14
	psrlq	xmm6,17
	mov	r9d,r14d
	mov	r12d,ecx
	pxor	xmm7,xmm6
	ror	r14d,9
	xor	r13d,ebx
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	psrlq	xmm6,2
	and	r12d,ebx
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	pxor	xmm7,xmm6
	mov	edi,r9d
	xor	r12d,edx
	ror	r14d,11
	pshufd	xmm7,xmm7,8
	xor	edi,r10d
	add	r8d,r12d
	movdqa	xmm6,XMMWORD PTR[64+rbp]
	ror	r13d,6
	and	r15d,edi
	pslldq	xmm7,8
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	paddd	xmm2,xmm7
	ror	r14d,2
	add	eax,r8d
	add	r8d,r15d
	paddd	xmm6,xmm2
	mov	r13d,eax
	add	r14d,r8d
	movdqa	XMMWORD PTR[32+rsp],xmm6
	ror	r13d,14
	movdqa	xmm4,xmm0
	mov	r8d,r14d
	mov	r12d,ebx
	movdqa	xmm7,xmm2
	ror	r14d,9
	xor	r13d,eax
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
DB	102,15,58,15,227,4
	and	r12d,eax
	xor	r13d,eax
DB	102,15,58,15,249,4
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
	xor	r12d,ecx
	ror	r14d,11
	movdqa	xmm5,xmm4
	xor	r15d,r9d
	add	edx,r12d
	movdqa	xmm6,xmm4
	ror	r13d,6
	and	edi,r15d
	psrld	xmm4,3
	xor	r14d,r8d
	add	edx,r13d
	xor	edi,r9d
	paddd	xmm3,xmm7
	ror	r14d,2
	add	r11d,edx
	psrld	xmm6,7
	add	edx,edi
	mov	r13d,r11d
	pshufd	xmm7,xmm2,250
	add	r14d,edx
	ror	r13d,14
	pslld	xmm5,14
	mov	edx,r14d
	mov	r12d,eax
	pxor	xmm4,xmm6
	ror	r14d,9
	xor	r13d,r11d
	xor	r12d,ebx
	ror	r13d,5
	psrld	xmm6,11
	xor	r14d,edx
	pxor	xmm4,xmm5
	and	r12d,r11d
	xor	r13d,r11d
	pslld	xmm5,11
	add	ecx,DWORD PTR[52+rsp]
	mov	edi,edx
	pxor	xmm4,xmm6
	xor	r12d,ebx
	ror	r14d,11
	movdqa	xmm6,xmm7
	xor	edi,r8d
	add	ecx,r12d
	pxor	xmm4,xmm5
	ror	r13d,6
	and	r15d,edi
	xor	r14d,edx
	psrld	xmm7,10
	add	ecx,r13d
	xor	r15d,r8d
	paddd	xmm3,xmm4
	ror	r14d,2
	add	r10d,ecx
	psrlq	xmm6,17
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	pxor	xmm7,xmm6
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	ror	r14d,9
	psrlq	xmm6,2
	xor	r13d,r10d
	xor	r12d,eax
	pxor	xmm7,xmm6
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	pshufd	xmm7,xmm7,128
	xor	r13d,r10d
	add	ebx,DWORD PTR[56+rsp]
	mov	r15d,ecx
	psrldq	xmm7,8
	xor	r12d,eax
	ror	r14d,11
	xor	r15d,edx
	add	ebx,r12d
	ror	r13d,6
	paddd	xmm3,xmm7
	and	edi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	pshufd	xmm7,xmm3,80
	xor	edi,edx
	ror	r14d,2
	add	r9d,ebx
	movdqa	xmm6,xmm7
	add	ebx,edi
	mov	r13d,r9d
	psrld	xmm7,10
	add	r14d,ebx
	ror	r13d,14
	psrlq	xmm6,17
	mov	ebx,r14d
	mov	r12d,r10d
	pxor	xmm7,xmm6
	ror	r14d,9
	xor	r13d,r9d
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	psrlq	xmm6,2
	and	r12d,r9d
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	pxor	xmm7,xmm6
	mov	edi,ebx
	xor	r12d,r11d
	ror	r14d,11
	pshufd	xmm7,xmm7,8
	xor	edi,ecx
	add	eax,r12d
	movdqa	xmm6,XMMWORD PTR[96+rbp]
	ror	r13d,6
	and	r15d,edi
	pslldq	xmm7,8
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	paddd	xmm3,xmm7
	ror	r14d,2
	add	r8d,eax
	add	eax,r15d
	paddd	xmm6,xmm3
	mov	r13d,r8d
	add	r14d,eax
	movdqa	XMMWORD PTR[48+rsp],xmm6
	cmp	BYTE PTR[131+rbp],0
	jne	$L$ssse3_00_47
	ror	r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	ror	r14d,9
	xor	r13d,r8d
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
	and	r12d,r8d
	xor	r13d,r8d
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
	xor	r12d,r10d
	ror	r14d,11
	xor	r15d,ebx
	add	r11d,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	edi,ebx
	ror	r14d,2
	add	edx,r11d
	add	r11d,edi
	mov	r13d,edx
	add	r14d,r11d
	ror	r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	ror	r14d,9
	xor	r13d,edx
	xor	r12d,r9d
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	xor	r13d,edx
	add	r10d,DWORD PTR[4+rsp]
	mov	edi,r11d
	xor	r12d,r9d
	ror	r14d,11
	xor	edi,eax
	add	r10d,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	ror	r14d,2
	add	ecx,r10d
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	ror	r14d,9
	xor	r13d,ecx
	xor	r12d,r8d
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	xor	r13d,ecx
	add	r9d,DWORD PTR[8+rsp]
	mov	r15d,r10d
	xor	r12d,r8d
	ror	r14d,11
	xor	r15d,r11d
	add	r9d,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	edi,r11d
	ror	r14d,2
	add	ebx,r9d
	add	r9d,edi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	ror	r14d,9
	xor	r13d,ebx
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	mov	edi,r9d
	xor	r12d,edx
	ror	r14d,11
	xor	edi,r10d
	add	r8d,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	ror	r14d,2
	add	eax,r8d
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	ror	r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	ror	r14d,9
	xor	r13d,eax
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
	and	r12d,eax
	xor	r13d,eax
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
	xor	r12d,ecx
	ror	r14d,11
	xor	r15d,r9d
	add	edx,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	edi,r9d
	ror	r14d,2
	add	r11d,edx
	add	edx,edi
	mov	r13d,r11d
	add	r14d,edx
	ror	r13d,14
	mov	edx,r14d
	mov	r12d,eax
	ror	r14d,9
	xor	r13d,r11d
	xor	r12d,ebx
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	xor	r13d,r11d
	add	ecx,DWORD PTR[20+rsp]
	mov	edi,edx
	xor	r12d,ebx
	ror	r14d,11
	xor	edi,r8d
	add	ecx,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	ror	r14d,2
	add	r10d,ecx
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	ror	r14d,9
	xor	r13d,r10d
	xor	r12d,eax
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	xor	r13d,r10d
	add	ebx,DWORD PTR[24+rsp]
	mov	r15d,ecx
	xor	r12d,eax
	ror	r14d,11
	xor	r15d,edx
	add	ebx,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	edi,edx
	ror	r14d,2
	add	r9d,ebx
	add	ebx,edi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	ror	r14d,9
	xor	r13d,r9d
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	mov	edi,ebx
	xor	r12d,r11d
	ror	r14d,11
	xor	edi,ecx
	add	eax,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	ror	r14d,2
	add	r8d,eax
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	ror	r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	ror	r14d,9
	xor	r13d,r8d
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
	and	r12d,r8d
	xor	r13d,r8d
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
	xor	r12d,r10d
	ror	r14d,11
	xor	r15d,ebx
	add	r11d,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	edi,ebx
	ror	r14d,2
	add	edx,r11d
	add	r11d,edi
	mov	r13d,edx
	add	r14d,r11d
	ror	r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	ror	r14d,9
	xor	r13d,edx
	xor	r12d,r9d
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	xor	r13d,edx
	add	r10d,DWORD PTR[36+rsp]
	mov	edi,r11d
	xor	r12d,r9d
	ror	r14d,11
	xor	edi,eax
	add	r10d,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	ror	r14d,2
	add	ecx,r10d
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	ror	r14d,9
	xor	r13d,ecx
	xor	r12d,r8d
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	xor	r13d,ecx
	add	r9d,DWORD PTR[40+rsp]
	mov	r15d,r10d
	xor	r12d,r8d
	ror	r14d,11
	xor	r15d,r11d
	add	r9d,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	edi,r11d
	ror	r14d,2
	add	ebx,r9d
	add	r9d,edi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	ror	r14d,9
	xor	r13d,ebx
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	mov	edi,r9d
	xor	r12d,edx
	ror	r14d,11
	xor	edi,r10d
	add	r8d,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	ror	r14d,2
	add	eax,r8d
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	ror	r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	ror	r14d,9
	xor	r13d,eax
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
	and	r12d,eax
	xor	r13d,eax
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
	xor	r12d,ecx
	ror	r14d,11
	xor	r15d,r9d
	add	edx,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	edi,r9d
	ror	r14d,2
	add	r11d,edx
	add	edx,edi
	mov	r13d,r11d
	add	r14d,edx
	ror	r13d,14
	mov	edx,r14d
	mov	r12d,eax
	ror	r14d,9
	xor	r13d,r11d
	xor	r12d,ebx
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	xor	r13d,r11d
	add	ecx,DWORD PTR[52+rsp]
	mov	edi,edx
	xor	r12d,ebx
	ror	r14d,11
	xor	edi,r8d
	add	ecx,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	ror	r14d,2
	add	r10d,ecx
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	ror	r14d,9
	xor	r13d,r10d
	xor	r12d,eax
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	xor	r13d,r10d
	add	ebx,DWORD PTR[56+rsp]
	mov	r15d,ecx
	xor	r12d,eax
	ror	r14d,11
	xor	r15d,edx
	add	ebx,r12d
	ror	r13d,6
	and	edi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	edi,edx
	ror	r14d,2
	add	r9d,ebx
	add	ebx,edi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	ror	r14d,9
	xor	r13d,r9d
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	mov	edi,ebx
	xor	r12d,r11d
	ror	r14d,11
	xor	edi,ecx
	add	eax,r12d
	ror	r13d,6
	and	r15d,edi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	ror	r14d,2
	add	r8d,eax
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	mov	rdi,QWORD PTR[((64+0))+rsp]
	mov	eax,r14d

	add	eax,DWORD PTR[rdi]
	lea	rsi,QWORD PTR[64+rsi]
	add	ebx,DWORD PTR[4+rdi]
	add	ecx,DWORD PTR[8+rdi]
	add	edx,DWORD PTR[12+rdi]
	add	r8d,DWORD PTR[16+rdi]
	add	r9d,DWORD PTR[20+rdi]
	add	r10d,DWORD PTR[24+rdi]
	add	r11d,DWORD PTR[28+rdi]

	cmp	rsi,QWORD PTR[((64+16))+rsp]

	mov	DWORD PTR[rdi],eax
	mov	DWORD PTR[4+rdi],ebx
	mov	DWORD PTR[8+rdi],ecx
	mov	DWORD PTR[12+rdi],edx
	mov	DWORD PTR[16+rdi],r8d
	mov	DWORD PTR[20+rdi],r9d
	mov	DWORD PTR[24+rdi],r10d
	mov	DWORD PTR[28+rdi],r11d
	jb	$L$loop_ssse3

	mov	rsi,QWORD PTR[((64+24))+rsp]
	movaps	xmm6,XMMWORD PTR[((64+32))+rsp]
	movaps	xmm7,XMMWORD PTR[((64+48))+rsp]
	movaps	xmm8,XMMWORD PTR[((64+64))+rsp]
	movaps	xmm9,XMMWORD PTR[((64+80))+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue_ssse3::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha256_block_data_order_ssse3::
sha256_block_data_order_ssse3	ENDP
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
	mov	rsi,rax
	mov	rax,QWORD PTR[((64+24))+rax]
	lea	rax,QWORD PTR[48+rax]

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

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jb	$L$in_prologue

	lea	rsi,QWORD PTR[((64+32))+rsi]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,8
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

ALIGN	16
shaext_handler	PROC PRIVATE
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

	lea	r10,QWORD PTR[$L$prologue_shaext]
	cmp	rbx,r10
	jb	$L$in_prologue

	lea	r10,QWORD PTR[$L$epilogue_shaext]
	cmp	rbx,r10
	jae	$L$in_prologue

	lea	rsi,QWORD PTR[((-8-80))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,10
	DD	0a548f3fch

	jmp	$L$in_prologue
shaext_handler	ENDP
.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_sha256_block_data_order
	DD	imagerel $L$SEH_end_sha256_block_data_order
	DD	imagerel $L$SEH_info_sha256_block_data_order
	DD	imagerel $L$SEH_begin_sha256_block_data_order_shaext
	DD	imagerel $L$SEH_end_sha256_block_data_order_shaext
	DD	imagerel $L$SEH_info_sha256_block_data_order_shaext
	DD	imagerel $L$SEH_begin_sha256_block_data_order_ssse3
	DD	imagerel $L$SEH_end_sha256_block_data_order_ssse3
	DD	imagerel $L$SEH_info_sha256_block_data_order_ssse3
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha256_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue,imagerel $L$epilogue
$L$SEH_info_sha256_block_data_order_shaext::
DB	9,0,0,0
	DD	imagerel shaext_handler
$L$SEH_info_sha256_block_data_order_ssse3::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue_ssse3,imagerel $L$epilogue_ssse3

.xdata	ENDS
END
