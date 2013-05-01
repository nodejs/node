OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

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
	mov	rcx,r9


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

	lea	rbp,QWORD PTR[K256]

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
	xor	rdi,rdi
	mov	r12d,DWORD PTR[rsi]
	mov	r13d,r8d
	mov	r14d,eax
	bswap	r12d
	ror	r13d,14
	mov	r15d,r9d
	mov	DWORD PTR[rsp],r12d

	ror	r14d,9
	xor	r13d,r8d
	xor	r15d,r10d

	ror	r13d,5
	add	r12d,r11d
	xor	r14d,eax

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r8d
	mov	r11d,ebx

	ror	r14d,11
	xor	r13d,r8d
	xor	r15d,r10d

	xor	r11d,ecx
	xor	r14d,eax
	add	r12d,r15d
	mov	r15d,ebx

	ror	r13d,6
	and	r11d,eax
	and	r15d,ecx

	ror	r14d,2
	add	r12d,r13d
	add	r11d,r15d

	add	edx,r12d
	add	r11d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r11d,r14d

	mov	r12d,DWORD PTR[4+rsi]
	mov	r13d,edx
	mov	r14d,r11d
	bswap	r12d
	ror	r13d,14
	mov	r15d,r8d
	mov	DWORD PTR[4+rsp],r12d

	ror	r14d,9
	xor	r13d,edx
	xor	r15d,r9d

	ror	r13d,5
	add	r12d,r10d
	xor	r14d,r11d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,edx
	mov	r10d,eax

	ror	r14d,11
	xor	r13d,edx
	xor	r15d,r9d

	xor	r10d,ebx
	xor	r14d,r11d
	add	r12d,r15d
	mov	r15d,eax

	ror	r13d,6
	and	r10d,r11d
	and	r15d,ebx

	ror	r14d,2
	add	r12d,r13d
	add	r10d,r15d

	add	ecx,r12d
	add	r10d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r10d,r14d

	mov	r12d,DWORD PTR[8+rsi]
	mov	r13d,ecx
	mov	r14d,r10d
	bswap	r12d
	ror	r13d,14
	mov	r15d,edx
	mov	DWORD PTR[8+rsp],r12d

	ror	r14d,9
	xor	r13d,ecx
	xor	r15d,r8d

	ror	r13d,5
	add	r12d,r9d
	xor	r14d,r10d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ecx
	mov	r9d,r11d

	ror	r14d,11
	xor	r13d,ecx
	xor	r15d,r8d

	xor	r9d,eax
	xor	r14d,r10d
	add	r12d,r15d
	mov	r15d,r11d

	ror	r13d,6
	and	r9d,r10d
	and	r15d,eax

	ror	r14d,2
	add	r12d,r13d
	add	r9d,r15d

	add	ebx,r12d
	add	r9d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r9d,r14d

	mov	r12d,DWORD PTR[12+rsi]
	mov	r13d,ebx
	mov	r14d,r9d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ecx
	mov	DWORD PTR[12+rsp],r12d

	ror	r14d,9
	xor	r13d,ebx
	xor	r15d,edx

	ror	r13d,5
	add	r12d,r8d
	xor	r14d,r9d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ebx
	mov	r8d,r10d

	ror	r14d,11
	xor	r13d,ebx
	xor	r15d,edx

	xor	r8d,r11d
	xor	r14d,r9d
	add	r12d,r15d
	mov	r15d,r10d

	ror	r13d,6
	and	r8d,r9d
	and	r15d,r11d

	ror	r14d,2
	add	r12d,r13d
	add	r8d,r15d

	add	eax,r12d
	add	r8d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r8d,r14d

	mov	r12d,DWORD PTR[16+rsi]
	mov	r13d,eax
	mov	r14d,r8d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ebx
	mov	DWORD PTR[16+rsp],r12d

	ror	r14d,9
	xor	r13d,eax
	xor	r15d,ecx

	ror	r13d,5
	add	r12d,edx
	xor	r14d,r8d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,eax
	mov	edx,r9d

	ror	r14d,11
	xor	r13d,eax
	xor	r15d,ecx

	xor	edx,r10d
	xor	r14d,r8d
	add	r12d,r15d
	mov	r15d,r9d

	ror	r13d,6
	and	edx,r8d
	and	r15d,r10d

	ror	r14d,2
	add	r12d,r13d
	add	edx,r15d

	add	r11d,r12d
	add	edx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	edx,r14d

	mov	r12d,DWORD PTR[20+rsi]
	mov	r13d,r11d
	mov	r14d,edx
	bswap	r12d
	ror	r13d,14
	mov	r15d,eax
	mov	DWORD PTR[20+rsp],r12d

	ror	r14d,9
	xor	r13d,r11d
	xor	r15d,ebx

	ror	r13d,5
	add	r12d,ecx
	xor	r14d,edx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r11d
	mov	ecx,r8d

	ror	r14d,11
	xor	r13d,r11d
	xor	r15d,ebx

	xor	ecx,r9d
	xor	r14d,edx
	add	r12d,r15d
	mov	r15d,r8d

	ror	r13d,6
	and	ecx,edx
	and	r15d,r9d

	ror	r14d,2
	add	r12d,r13d
	add	ecx,r15d

	add	r10d,r12d
	add	ecx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ecx,r14d

	mov	r12d,DWORD PTR[24+rsi]
	mov	r13d,r10d
	mov	r14d,ecx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r11d
	mov	DWORD PTR[24+rsp],r12d

	ror	r14d,9
	xor	r13d,r10d
	xor	r15d,eax

	ror	r13d,5
	add	r12d,ebx
	xor	r14d,ecx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r10d
	mov	ebx,edx

	ror	r14d,11
	xor	r13d,r10d
	xor	r15d,eax

	xor	ebx,r8d
	xor	r14d,ecx
	add	r12d,r15d
	mov	r15d,edx

	ror	r13d,6
	and	ebx,ecx
	and	r15d,r8d

	ror	r14d,2
	add	r12d,r13d
	add	ebx,r15d

	add	r9d,r12d
	add	ebx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ebx,r14d

	mov	r12d,DWORD PTR[28+rsi]
	mov	r13d,r9d
	mov	r14d,ebx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r10d
	mov	DWORD PTR[28+rsp],r12d

	ror	r14d,9
	xor	r13d,r9d
	xor	r15d,r11d

	ror	r13d,5
	add	r12d,eax
	xor	r14d,ebx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r9d
	mov	eax,ecx

	ror	r14d,11
	xor	r13d,r9d
	xor	r15d,r11d

	xor	eax,edx
	xor	r14d,ebx
	add	r12d,r15d
	mov	r15d,ecx

	ror	r13d,6
	and	eax,ebx
	and	r15d,edx

	ror	r14d,2
	add	r12d,r13d
	add	eax,r15d

	add	r8d,r12d
	add	eax,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	eax,r14d

	mov	r12d,DWORD PTR[32+rsi]
	mov	r13d,r8d
	mov	r14d,eax
	bswap	r12d
	ror	r13d,14
	mov	r15d,r9d
	mov	DWORD PTR[32+rsp],r12d

	ror	r14d,9
	xor	r13d,r8d
	xor	r15d,r10d

	ror	r13d,5
	add	r12d,r11d
	xor	r14d,eax

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r8d
	mov	r11d,ebx

	ror	r14d,11
	xor	r13d,r8d
	xor	r15d,r10d

	xor	r11d,ecx
	xor	r14d,eax
	add	r12d,r15d
	mov	r15d,ebx

	ror	r13d,6
	and	r11d,eax
	and	r15d,ecx

	ror	r14d,2
	add	r12d,r13d
	add	r11d,r15d

	add	edx,r12d
	add	r11d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r11d,r14d

	mov	r12d,DWORD PTR[36+rsi]
	mov	r13d,edx
	mov	r14d,r11d
	bswap	r12d
	ror	r13d,14
	mov	r15d,r8d
	mov	DWORD PTR[36+rsp],r12d

	ror	r14d,9
	xor	r13d,edx
	xor	r15d,r9d

	ror	r13d,5
	add	r12d,r10d
	xor	r14d,r11d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,edx
	mov	r10d,eax

	ror	r14d,11
	xor	r13d,edx
	xor	r15d,r9d

	xor	r10d,ebx
	xor	r14d,r11d
	add	r12d,r15d
	mov	r15d,eax

	ror	r13d,6
	and	r10d,r11d
	and	r15d,ebx

	ror	r14d,2
	add	r12d,r13d
	add	r10d,r15d

	add	ecx,r12d
	add	r10d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r10d,r14d

	mov	r12d,DWORD PTR[40+rsi]
	mov	r13d,ecx
	mov	r14d,r10d
	bswap	r12d
	ror	r13d,14
	mov	r15d,edx
	mov	DWORD PTR[40+rsp],r12d

	ror	r14d,9
	xor	r13d,ecx
	xor	r15d,r8d

	ror	r13d,5
	add	r12d,r9d
	xor	r14d,r10d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ecx
	mov	r9d,r11d

	ror	r14d,11
	xor	r13d,ecx
	xor	r15d,r8d

	xor	r9d,eax
	xor	r14d,r10d
	add	r12d,r15d
	mov	r15d,r11d

	ror	r13d,6
	and	r9d,r10d
	and	r15d,eax

	ror	r14d,2
	add	r12d,r13d
	add	r9d,r15d

	add	ebx,r12d
	add	r9d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r9d,r14d

	mov	r12d,DWORD PTR[44+rsi]
	mov	r13d,ebx
	mov	r14d,r9d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ecx
	mov	DWORD PTR[44+rsp],r12d

	ror	r14d,9
	xor	r13d,ebx
	xor	r15d,edx

	ror	r13d,5
	add	r12d,r8d
	xor	r14d,r9d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ebx
	mov	r8d,r10d

	ror	r14d,11
	xor	r13d,ebx
	xor	r15d,edx

	xor	r8d,r11d
	xor	r14d,r9d
	add	r12d,r15d
	mov	r15d,r10d

	ror	r13d,6
	and	r8d,r9d
	and	r15d,r11d

	ror	r14d,2
	add	r12d,r13d
	add	r8d,r15d

	add	eax,r12d
	add	r8d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r8d,r14d

	mov	r12d,DWORD PTR[48+rsi]
	mov	r13d,eax
	mov	r14d,r8d
	bswap	r12d
	ror	r13d,14
	mov	r15d,ebx
	mov	DWORD PTR[48+rsp],r12d

	ror	r14d,9
	xor	r13d,eax
	xor	r15d,ecx

	ror	r13d,5
	add	r12d,edx
	xor	r14d,r8d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,eax
	mov	edx,r9d

	ror	r14d,11
	xor	r13d,eax
	xor	r15d,ecx

	xor	edx,r10d
	xor	r14d,r8d
	add	r12d,r15d
	mov	r15d,r9d

	ror	r13d,6
	and	edx,r8d
	and	r15d,r10d

	ror	r14d,2
	add	r12d,r13d
	add	edx,r15d

	add	r11d,r12d
	add	edx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	edx,r14d

	mov	r12d,DWORD PTR[52+rsi]
	mov	r13d,r11d
	mov	r14d,edx
	bswap	r12d
	ror	r13d,14
	mov	r15d,eax
	mov	DWORD PTR[52+rsp],r12d

	ror	r14d,9
	xor	r13d,r11d
	xor	r15d,ebx

	ror	r13d,5
	add	r12d,ecx
	xor	r14d,edx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r11d
	mov	ecx,r8d

	ror	r14d,11
	xor	r13d,r11d
	xor	r15d,ebx

	xor	ecx,r9d
	xor	r14d,edx
	add	r12d,r15d
	mov	r15d,r8d

	ror	r13d,6
	and	ecx,edx
	and	r15d,r9d

	ror	r14d,2
	add	r12d,r13d
	add	ecx,r15d

	add	r10d,r12d
	add	ecx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ecx,r14d

	mov	r12d,DWORD PTR[56+rsi]
	mov	r13d,r10d
	mov	r14d,ecx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r11d
	mov	DWORD PTR[56+rsp],r12d

	ror	r14d,9
	xor	r13d,r10d
	xor	r15d,eax

	ror	r13d,5
	add	r12d,ebx
	xor	r14d,ecx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r10d
	mov	ebx,edx

	ror	r14d,11
	xor	r13d,r10d
	xor	r15d,eax

	xor	ebx,r8d
	xor	r14d,ecx
	add	r12d,r15d
	mov	r15d,edx

	ror	r13d,6
	and	ebx,ecx
	and	r15d,r8d

	ror	r14d,2
	add	r12d,r13d
	add	ebx,r15d

	add	r9d,r12d
	add	ebx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ebx,r14d

	mov	r12d,DWORD PTR[60+rsi]
	mov	r13d,r9d
	mov	r14d,ebx
	bswap	r12d
	ror	r13d,14
	mov	r15d,r10d
	mov	DWORD PTR[60+rsp],r12d

	ror	r14d,9
	xor	r13d,r9d
	xor	r15d,r11d

	ror	r13d,5
	add	r12d,eax
	xor	r14d,ebx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r9d
	mov	eax,ecx

	ror	r14d,11
	xor	r13d,r9d
	xor	r15d,r11d

	xor	eax,edx
	xor	r14d,ebx
	add	r12d,r15d
	mov	r15d,ecx

	ror	r13d,6
	and	eax,ebx
	and	r15d,edx

	ror	r14d,2
	add	r12d,r13d
	add	eax,r15d

	add	r8d,r12d
	add	eax,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	eax,r14d

	jmp	$L$rounds_16_xx
ALIGN	16
$L$rounds_16_xx::
	mov	r13d,DWORD PTR[4+rsp]
	mov	r14d,DWORD PTR[56+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[36+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[rsp]
	mov	r13d,r8d
	add	r12d,r14d
	mov	r14d,eax
	ror	r13d,14
	mov	r15d,r9d
	mov	DWORD PTR[rsp],r12d

	ror	r14d,9
	xor	r13d,r8d
	xor	r15d,r10d

	ror	r13d,5
	add	r12d,r11d
	xor	r14d,eax

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r8d
	mov	r11d,ebx

	ror	r14d,11
	xor	r13d,r8d
	xor	r15d,r10d

	xor	r11d,ecx
	xor	r14d,eax
	add	r12d,r15d
	mov	r15d,ebx

	ror	r13d,6
	and	r11d,eax
	and	r15d,ecx

	ror	r14d,2
	add	r12d,r13d
	add	r11d,r15d

	add	edx,r12d
	add	r11d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r11d,r14d

	mov	r13d,DWORD PTR[8+rsp]
	mov	r14d,DWORD PTR[60+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[40+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[4+rsp]
	mov	r13d,edx
	add	r12d,r14d
	mov	r14d,r11d
	ror	r13d,14
	mov	r15d,r8d
	mov	DWORD PTR[4+rsp],r12d

	ror	r14d,9
	xor	r13d,edx
	xor	r15d,r9d

	ror	r13d,5
	add	r12d,r10d
	xor	r14d,r11d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,edx
	mov	r10d,eax

	ror	r14d,11
	xor	r13d,edx
	xor	r15d,r9d

	xor	r10d,ebx
	xor	r14d,r11d
	add	r12d,r15d
	mov	r15d,eax

	ror	r13d,6
	and	r10d,r11d
	and	r15d,ebx

	ror	r14d,2
	add	r12d,r13d
	add	r10d,r15d

	add	ecx,r12d
	add	r10d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r10d,r14d

	mov	r13d,DWORD PTR[12+rsp]
	mov	r14d,DWORD PTR[rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[44+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[8+rsp]
	mov	r13d,ecx
	add	r12d,r14d
	mov	r14d,r10d
	ror	r13d,14
	mov	r15d,edx
	mov	DWORD PTR[8+rsp],r12d

	ror	r14d,9
	xor	r13d,ecx
	xor	r15d,r8d

	ror	r13d,5
	add	r12d,r9d
	xor	r14d,r10d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ecx
	mov	r9d,r11d

	ror	r14d,11
	xor	r13d,ecx
	xor	r15d,r8d

	xor	r9d,eax
	xor	r14d,r10d
	add	r12d,r15d
	mov	r15d,r11d

	ror	r13d,6
	and	r9d,r10d
	and	r15d,eax

	ror	r14d,2
	add	r12d,r13d
	add	r9d,r15d

	add	ebx,r12d
	add	r9d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r9d,r14d

	mov	r13d,DWORD PTR[16+rsp]
	mov	r14d,DWORD PTR[4+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[48+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[12+rsp]
	mov	r13d,ebx
	add	r12d,r14d
	mov	r14d,r9d
	ror	r13d,14
	mov	r15d,ecx
	mov	DWORD PTR[12+rsp],r12d

	ror	r14d,9
	xor	r13d,ebx
	xor	r15d,edx

	ror	r13d,5
	add	r12d,r8d
	xor	r14d,r9d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ebx
	mov	r8d,r10d

	ror	r14d,11
	xor	r13d,ebx
	xor	r15d,edx

	xor	r8d,r11d
	xor	r14d,r9d
	add	r12d,r15d
	mov	r15d,r10d

	ror	r13d,6
	and	r8d,r9d
	and	r15d,r11d

	ror	r14d,2
	add	r12d,r13d
	add	r8d,r15d

	add	eax,r12d
	add	r8d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r8d,r14d

	mov	r13d,DWORD PTR[20+rsp]
	mov	r14d,DWORD PTR[8+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[52+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[16+rsp]
	mov	r13d,eax
	add	r12d,r14d
	mov	r14d,r8d
	ror	r13d,14
	mov	r15d,ebx
	mov	DWORD PTR[16+rsp],r12d

	ror	r14d,9
	xor	r13d,eax
	xor	r15d,ecx

	ror	r13d,5
	add	r12d,edx
	xor	r14d,r8d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,eax
	mov	edx,r9d

	ror	r14d,11
	xor	r13d,eax
	xor	r15d,ecx

	xor	edx,r10d
	xor	r14d,r8d
	add	r12d,r15d
	mov	r15d,r9d

	ror	r13d,6
	and	edx,r8d
	and	r15d,r10d

	ror	r14d,2
	add	r12d,r13d
	add	edx,r15d

	add	r11d,r12d
	add	edx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	edx,r14d

	mov	r13d,DWORD PTR[24+rsp]
	mov	r14d,DWORD PTR[12+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[56+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[20+rsp]
	mov	r13d,r11d
	add	r12d,r14d
	mov	r14d,edx
	ror	r13d,14
	mov	r15d,eax
	mov	DWORD PTR[20+rsp],r12d

	ror	r14d,9
	xor	r13d,r11d
	xor	r15d,ebx

	ror	r13d,5
	add	r12d,ecx
	xor	r14d,edx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r11d
	mov	ecx,r8d

	ror	r14d,11
	xor	r13d,r11d
	xor	r15d,ebx

	xor	ecx,r9d
	xor	r14d,edx
	add	r12d,r15d
	mov	r15d,r8d

	ror	r13d,6
	and	ecx,edx
	and	r15d,r9d

	ror	r14d,2
	add	r12d,r13d
	add	ecx,r15d

	add	r10d,r12d
	add	ecx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ecx,r14d

	mov	r13d,DWORD PTR[28+rsp]
	mov	r14d,DWORD PTR[16+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[60+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[24+rsp]
	mov	r13d,r10d
	add	r12d,r14d
	mov	r14d,ecx
	ror	r13d,14
	mov	r15d,r11d
	mov	DWORD PTR[24+rsp],r12d

	ror	r14d,9
	xor	r13d,r10d
	xor	r15d,eax

	ror	r13d,5
	add	r12d,ebx
	xor	r14d,ecx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r10d
	mov	ebx,edx

	ror	r14d,11
	xor	r13d,r10d
	xor	r15d,eax

	xor	ebx,r8d
	xor	r14d,ecx
	add	r12d,r15d
	mov	r15d,edx

	ror	r13d,6
	and	ebx,ecx
	and	r15d,r8d

	ror	r14d,2
	add	r12d,r13d
	add	ebx,r15d

	add	r9d,r12d
	add	ebx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ebx,r14d

	mov	r13d,DWORD PTR[32+rsp]
	mov	r14d,DWORD PTR[20+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[28+rsp]
	mov	r13d,r9d
	add	r12d,r14d
	mov	r14d,ebx
	ror	r13d,14
	mov	r15d,r10d
	mov	DWORD PTR[28+rsp],r12d

	ror	r14d,9
	xor	r13d,r9d
	xor	r15d,r11d

	ror	r13d,5
	add	r12d,eax
	xor	r14d,ebx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r9d
	mov	eax,ecx

	ror	r14d,11
	xor	r13d,r9d
	xor	r15d,r11d

	xor	eax,edx
	xor	r14d,ebx
	add	r12d,r15d
	mov	r15d,ecx

	ror	r13d,6
	and	eax,ebx
	and	r15d,edx

	ror	r14d,2
	add	r12d,r13d
	add	eax,r15d

	add	r8d,r12d
	add	eax,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	eax,r14d

	mov	r13d,DWORD PTR[36+rsp]
	mov	r14d,DWORD PTR[24+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[4+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[32+rsp]
	mov	r13d,r8d
	add	r12d,r14d
	mov	r14d,eax
	ror	r13d,14
	mov	r15d,r9d
	mov	DWORD PTR[32+rsp],r12d

	ror	r14d,9
	xor	r13d,r8d
	xor	r15d,r10d

	ror	r13d,5
	add	r12d,r11d
	xor	r14d,eax

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r8d
	mov	r11d,ebx

	ror	r14d,11
	xor	r13d,r8d
	xor	r15d,r10d

	xor	r11d,ecx
	xor	r14d,eax
	add	r12d,r15d
	mov	r15d,ebx

	ror	r13d,6
	and	r11d,eax
	and	r15d,ecx

	ror	r14d,2
	add	r12d,r13d
	add	r11d,r15d

	add	edx,r12d
	add	r11d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r11d,r14d

	mov	r13d,DWORD PTR[40+rsp]
	mov	r14d,DWORD PTR[28+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[8+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[36+rsp]
	mov	r13d,edx
	add	r12d,r14d
	mov	r14d,r11d
	ror	r13d,14
	mov	r15d,r8d
	mov	DWORD PTR[36+rsp],r12d

	ror	r14d,9
	xor	r13d,edx
	xor	r15d,r9d

	ror	r13d,5
	add	r12d,r10d
	xor	r14d,r11d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,edx
	mov	r10d,eax

	ror	r14d,11
	xor	r13d,edx
	xor	r15d,r9d

	xor	r10d,ebx
	xor	r14d,r11d
	add	r12d,r15d
	mov	r15d,eax

	ror	r13d,6
	and	r10d,r11d
	and	r15d,ebx

	ror	r14d,2
	add	r12d,r13d
	add	r10d,r15d

	add	ecx,r12d
	add	r10d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r10d,r14d

	mov	r13d,DWORD PTR[44+rsp]
	mov	r14d,DWORD PTR[32+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[12+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[40+rsp]
	mov	r13d,ecx
	add	r12d,r14d
	mov	r14d,r10d
	ror	r13d,14
	mov	r15d,edx
	mov	DWORD PTR[40+rsp],r12d

	ror	r14d,9
	xor	r13d,ecx
	xor	r15d,r8d

	ror	r13d,5
	add	r12d,r9d
	xor	r14d,r10d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ecx
	mov	r9d,r11d

	ror	r14d,11
	xor	r13d,ecx
	xor	r15d,r8d

	xor	r9d,eax
	xor	r14d,r10d
	add	r12d,r15d
	mov	r15d,r11d

	ror	r13d,6
	and	r9d,r10d
	and	r15d,eax

	ror	r14d,2
	add	r12d,r13d
	add	r9d,r15d

	add	ebx,r12d
	add	r9d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r9d,r14d

	mov	r13d,DWORD PTR[48+rsp]
	mov	r14d,DWORD PTR[36+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[16+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[44+rsp]
	mov	r13d,ebx
	add	r12d,r14d
	mov	r14d,r9d
	ror	r13d,14
	mov	r15d,ecx
	mov	DWORD PTR[44+rsp],r12d

	ror	r14d,9
	xor	r13d,ebx
	xor	r15d,edx

	ror	r13d,5
	add	r12d,r8d
	xor	r14d,r9d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,ebx
	mov	r8d,r10d

	ror	r14d,11
	xor	r13d,ebx
	xor	r15d,edx

	xor	r8d,r11d
	xor	r14d,r9d
	add	r12d,r15d
	mov	r15d,r10d

	ror	r13d,6
	and	r8d,r9d
	and	r15d,r11d

	ror	r14d,2
	add	r12d,r13d
	add	r8d,r15d

	add	eax,r12d
	add	r8d,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	r8d,r14d

	mov	r13d,DWORD PTR[52+rsp]
	mov	r14d,DWORD PTR[40+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[20+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[48+rsp]
	mov	r13d,eax
	add	r12d,r14d
	mov	r14d,r8d
	ror	r13d,14
	mov	r15d,ebx
	mov	DWORD PTR[48+rsp],r12d

	ror	r14d,9
	xor	r13d,eax
	xor	r15d,ecx

	ror	r13d,5
	add	r12d,edx
	xor	r14d,r8d

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,eax
	mov	edx,r9d

	ror	r14d,11
	xor	r13d,eax
	xor	r15d,ecx

	xor	edx,r10d
	xor	r14d,r8d
	add	r12d,r15d
	mov	r15d,r9d

	ror	r13d,6
	and	edx,r8d
	and	r15d,r10d

	ror	r14d,2
	add	r12d,r13d
	add	edx,r15d

	add	r11d,r12d
	add	edx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	edx,r14d

	mov	r13d,DWORD PTR[56+rsp]
	mov	r14d,DWORD PTR[44+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[24+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[52+rsp]
	mov	r13d,r11d
	add	r12d,r14d
	mov	r14d,edx
	ror	r13d,14
	mov	r15d,eax
	mov	DWORD PTR[52+rsp],r12d

	ror	r14d,9
	xor	r13d,r11d
	xor	r15d,ebx

	ror	r13d,5
	add	r12d,ecx
	xor	r14d,edx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r11d
	mov	ecx,r8d

	ror	r14d,11
	xor	r13d,r11d
	xor	r15d,ebx

	xor	ecx,r9d
	xor	r14d,edx
	add	r12d,r15d
	mov	r15d,r8d

	ror	r13d,6
	and	ecx,edx
	and	r15d,r9d

	ror	r14d,2
	add	r12d,r13d
	add	ecx,r15d

	add	r10d,r12d
	add	ecx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ecx,r14d

	mov	r13d,DWORD PTR[60+rsp]
	mov	r14d,DWORD PTR[48+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[28+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[56+rsp]
	mov	r13d,r10d
	add	r12d,r14d
	mov	r14d,ecx
	ror	r13d,14
	mov	r15d,r11d
	mov	DWORD PTR[56+rsp],r12d

	ror	r14d,9
	xor	r13d,r10d
	xor	r15d,eax

	ror	r13d,5
	add	r12d,ebx
	xor	r14d,ecx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r10d
	mov	ebx,edx

	ror	r14d,11
	xor	r13d,r10d
	xor	r15d,eax

	xor	ebx,r8d
	xor	r14d,ecx
	add	r12d,r15d
	mov	r15d,edx

	ror	r13d,6
	and	ebx,ecx
	and	r15d,r8d

	ror	r14d,2
	add	r12d,r13d
	add	ebx,r15d

	add	r9d,r12d
	add	ebx,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	ebx,r14d

	mov	r13d,DWORD PTR[rsp]
	mov	r14d,DWORD PTR[52+rsp]
	mov	r12d,r13d
	mov	r15d,r14d

	ror	r12d,11
	xor	r12d,r13d
	shr	r13d,3

	ror	r12d,7
	xor	r13d,r12d
	mov	r12d,DWORD PTR[32+rsp]

	ror	r15d,2
	xor	r15d,r14d
	shr	r14d,10

	ror	r15d,17
	add	r12d,r13d
	xor	r14d,r15d

	add	r12d,DWORD PTR[60+rsp]
	mov	r13d,r9d
	add	r12d,r14d
	mov	r14d,ebx
	ror	r13d,14
	mov	r15d,r10d
	mov	DWORD PTR[60+rsp],r12d

	ror	r14d,9
	xor	r13d,r9d
	xor	r15d,r11d

	ror	r13d,5
	add	r12d,eax
	xor	r14d,ebx

	add	r12d,DWORD PTR[rdi*4+rbp]
	and	r15d,r9d
	mov	eax,ecx

	ror	r14d,11
	xor	r13d,r9d
	xor	r15d,r11d

	xor	eax,edx
	xor	r14d,ebx
	add	r12d,r15d
	mov	r15d,ecx

	ror	r13d,6
	and	eax,ebx
	and	r15d,edx

	ror	r14d,2
	add	r12d,r13d
	add	eax,r15d

	add	r8d,r12d
	add	eax,r12d
	lea	rdi,QWORD PTR[1+rdi]
	add	eax,r14d

	cmp	rdi,64
	jb	$L$rounds_16_xx

	mov	rdi,QWORD PTR[((64+0))+rsp]
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
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h
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

	lea	r10,QWORD PTR[$L$prologue]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

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
	DD	imagerel $L$SEH_begin_sha256_block_data_order
	DD	imagerel $L$SEH_end_sha256_block_data_order
	DD	imagerel $L$SEH_info_sha256_block_data_order

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha256_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
