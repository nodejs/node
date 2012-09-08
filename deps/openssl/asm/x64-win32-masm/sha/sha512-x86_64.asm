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
	mov	QWORD PTR[((16*4+0*8))+rsp],rdi
	mov	QWORD PTR[((16*4+1*8))+rsp],rsi
	mov	QWORD PTR[((16*4+2*8))+rsp],rdx
	mov	QWORD PTR[((16*4+3*8))+rsp],r11
$L$prologue::

	lea	rbp,QWORD PTR[K256]

	mov	eax,DWORD PTR[((4*0))+rdi]
	mov	ebx,DWORD PTR[((4*1))+rdi]
	mov	ecx,DWORD PTR[((4*2))+rdi]
	mov	edx,DWORD PTR[((4*3))+rdi]
	mov	r8d,DWORD PTR[((4*4))+rdi]
	mov	r9d,DWORD PTR[((4*5))+rdi]
	mov	r10d,DWORD PTR[((4*6))+rdi]
	mov	r11d,DWORD PTR[((4*7))+rdi]
	jmp	$L$loop

ALIGN	16
$L$loop::
	xor	rdi,rdi
	mov	r12d,DWORD PTR[((4*0))+rsi]
	bswap	r12d
	mov	r13d,r8d
	mov	r14d,r8d
	mov	r15d,r9d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r10d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r8d
	mov	DWORD PTR[rsp],r12d

	xor	r13d,r14d
	xor	r15d,r10d
	add	r12d,r11d

	mov	r11d,eax
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,eax
	mov	r14d,eax

	ror	r11d,2
	ror	r13d,13
	mov	r15d,eax
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r11d,r13d
	ror	r13d,9
	or	r14d,ecx

	xor	r11d,r13d
	and	r15d,ecx
	add	edx,r12d

	and	r14d,ebx
	add	r11d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r11d,r14d
	mov	r12d,DWORD PTR[((4*1))+rsi]
	bswap	r12d
	mov	r13d,edx
	mov	r14d,edx
	mov	r15d,r8d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r9d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,edx
	mov	DWORD PTR[4+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r9d
	add	r12d,r10d

	mov	r10d,r11d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r11d
	mov	r14d,r11d

	ror	r10d,2
	ror	r13d,13
	mov	r15d,r11d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r10d,r13d
	ror	r13d,9
	or	r14d,ebx

	xor	r10d,r13d
	and	r15d,ebx
	add	ecx,r12d

	and	r14d,eax
	add	r10d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r10d,r14d
	mov	r12d,DWORD PTR[((4*2))+rsi]
	bswap	r12d
	mov	r13d,ecx
	mov	r14d,ecx
	mov	r15d,edx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r8d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ecx
	mov	DWORD PTR[8+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r8d
	add	r12d,r9d

	mov	r9d,r10d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r10d
	mov	r14d,r10d

	ror	r9d,2
	ror	r13d,13
	mov	r15d,r10d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r9d,r13d
	ror	r13d,9
	or	r14d,eax

	xor	r9d,r13d
	and	r15d,eax
	add	ebx,r12d

	and	r14d,r11d
	add	r9d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r9d,r14d
	mov	r12d,DWORD PTR[((4*3))+rsi]
	bswap	r12d
	mov	r13d,ebx
	mov	r14d,ebx
	mov	r15d,ecx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,edx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ebx
	mov	DWORD PTR[12+rsp],r12d

	xor	r13d,r14d
	xor	r15d,edx
	add	r12d,r8d

	mov	r8d,r9d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r9d
	mov	r14d,r9d

	ror	r8d,2
	ror	r13d,13
	mov	r15d,r9d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r8d,r13d
	ror	r13d,9
	or	r14d,r11d

	xor	r8d,r13d
	and	r15d,r11d
	add	eax,r12d

	and	r14d,r10d
	add	r8d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r8d,r14d
	mov	r12d,DWORD PTR[((4*4))+rsi]
	bswap	r12d
	mov	r13d,eax
	mov	r14d,eax
	mov	r15d,ebx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ecx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,eax
	mov	DWORD PTR[16+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ecx
	add	r12d,edx

	mov	edx,r8d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r8d
	mov	r14d,r8d

	ror	edx,2
	ror	r13d,13
	mov	r15d,r8d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	edx,r13d
	ror	r13d,9
	or	r14d,r10d

	xor	edx,r13d
	and	r15d,r10d
	add	r11d,r12d

	and	r14d,r9d
	add	edx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	edx,r14d
	mov	r12d,DWORD PTR[((4*5))+rsi]
	bswap	r12d
	mov	r13d,r11d
	mov	r14d,r11d
	mov	r15d,eax

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ebx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r11d
	mov	DWORD PTR[20+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ebx
	add	r12d,ecx

	mov	ecx,edx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,edx
	mov	r14d,edx

	ror	ecx,2
	ror	r13d,13
	mov	r15d,edx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ecx,r13d
	ror	r13d,9
	or	r14d,r9d

	xor	ecx,r13d
	and	r15d,r9d
	add	r10d,r12d

	and	r14d,r8d
	add	ecx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ecx,r14d
	mov	r12d,DWORD PTR[((4*6))+rsi]
	bswap	r12d
	mov	r13d,r10d
	mov	r14d,r10d
	mov	r15d,r11d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,eax

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r10d
	mov	DWORD PTR[24+rsp],r12d

	xor	r13d,r14d
	xor	r15d,eax
	add	r12d,ebx

	mov	ebx,ecx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ecx
	mov	r14d,ecx

	ror	ebx,2
	ror	r13d,13
	mov	r15d,ecx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ebx,r13d
	ror	r13d,9
	or	r14d,r8d

	xor	ebx,r13d
	and	r15d,r8d
	add	r9d,r12d

	and	r14d,edx
	add	ebx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ebx,r14d
	mov	r12d,DWORD PTR[((4*7))+rsi]
	bswap	r12d
	mov	r13d,r9d
	mov	r14d,r9d
	mov	r15d,r10d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r11d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r9d
	mov	DWORD PTR[28+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r11d
	add	r12d,eax

	mov	eax,ebx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ebx
	mov	r14d,ebx

	ror	eax,2
	ror	r13d,13
	mov	r15d,ebx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	eax,r13d
	ror	r13d,9
	or	r14d,edx

	xor	eax,r13d
	and	r15d,edx
	add	r8d,r12d

	and	r14d,ecx
	add	eax,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	eax,r14d
	mov	r12d,DWORD PTR[((4*8))+rsi]
	bswap	r12d
	mov	r13d,r8d
	mov	r14d,r8d
	mov	r15d,r9d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r10d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r8d
	mov	DWORD PTR[32+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r10d
	add	r12d,r11d

	mov	r11d,eax
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,eax
	mov	r14d,eax

	ror	r11d,2
	ror	r13d,13
	mov	r15d,eax
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r11d,r13d
	ror	r13d,9
	or	r14d,ecx

	xor	r11d,r13d
	and	r15d,ecx
	add	edx,r12d

	and	r14d,ebx
	add	r11d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r11d,r14d
	mov	r12d,DWORD PTR[((4*9))+rsi]
	bswap	r12d
	mov	r13d,edx
	mov	r14d,edx
	mov	r15d,r8d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r9d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,edx
	mov	DWORD PTR[36+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r9d
	add	r12d,r10d

	mov	r10d,r11d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r11d
	mov	r14d,r11d

	ror	r10d,2
	ror	r13d,13
	mov	r15d,r11d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r10d,r13d
	ror	r13d,9
	or	r14d,ebx

	xor	r10d,r13d
	and	r15d,ebx
	add	ecx,r12d

	and	r14d,eax
	add	r10d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r10d,r14d
	mov	r12d,DWORD PTR[((4*10))+rsi]
	bswap	r12d
	mov	r13d,ecx
	mov	r14d,ecx
	mov	r15d,edx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r8d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ecx
	mov	DWORD PTR[40+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r8d
	add	r12d,r9d

	mov	r9d,r10d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r10d
	mov	r14d,r10d

	ror	r9d,2
	ror	r13d,13
	mov	r15d,r10d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r9d,r13d
	ror	r13d,9
	or	r14d,eax

	xor	r9d,r13d
	and	r15d,eax
	add	ebx,r12d

	and	r14d,r11d
	add	r9d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r9d,r14d
	mov	r12d,DWORD PTR[((4*11))+rsi]
	bswap	r12d
	mov	r13d,ebx
	mov	r14d,ebx
	mov	r15d,ecx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,edx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ebx
	mov	DWORD PTR[44+rsp],r12d

	xor	r13d,r14d
	xor	r15d,edx
	add	r12d,r8d

	mov	r8d,r9d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r9d
	mov	r14d,r9d

	ror	r8d,2
	ror	r13d,13
	mov	r15d,r9d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r8d,r13d
	ror	r13d,9
	or	r14d,r11d

	xor	r8d,r13d
	and	r15d,r11d
	add	eax,r12d

	and	r14d,r10d
	add	r8d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r8d,r14d
	mov	r12d,DWORD PTR[((4*12))+rsi]
	bswap	r12d
	mov	r13d,eax
	mov	r14d,eax
	mov	r15d,ebx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ecx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,eax
	mov	DWORD PTR[48+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ecx
	add	r12d,edx

	mov	edx,r8d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r8d
	mov	r14d,r8d

	ror	edx,2
	ror	r13d,13
	mov	r15d,r8d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	edx,r13d
	ror	r13d,9
	or	r14d,r10d

	xor	edx,r13d
	and	r15d,r10d
	add	r11d,r12d

	and	r14d,r9d
	add	edx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	edx,r14d
	mov	r12d,DWORD PTR[((4*13))+rsi]
	bswap	r12d
	mov	r13d,r11d
	mov	r14d,r11d
	mov	r15d,eax

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ebx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r11d
	mov	DWORD PTR[52+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ebx
	add	r12d,ecx

	mov	ecx,edx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,edx
	mov	r14d,edx

	ror	ecx,2
	ror	r13d,13
	mov	r15d,edx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ecx,r13d
	ror	r13d,9
	or	r14d,r9d

	xor	ecx,r13d
	and	r15d,r9d
	add	r10d,r12d

	and	r14d,r8d
	add	ecx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ecx,r14d
	mov	r12d,DWORD PTR[((4*14))+rsi]
	bswap	r12d
	mov	r13d,r10d
	mov	r14d,r10d
	mov	r15d,r11d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,eax

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r10d
	mov	DWORD PTR[56+rsp],r12d

	xor	r13d,r14d
	xor	r15d,eax
	add	r12d,ebx

	mov	ebx,ecx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ecx
	mov	r14d,ecx

	ror	ebx,2
	ror	r13d,13
	mov	r15d,ecx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ebx,r13d
	ror	r13d,9
	or	r14d,r8d

	xor	ebx,r13d
	and	r15d,r8d
	add	r9d,r12d

	and	r14d,edx
	add	ebx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ebx,r14d
	mov	r12d,DWORD PTR[((4*15))+rsi]
	bswap	r12d
	mov	r13d,r9d
	mov	r14d,r9d
	mov	r15d,r10d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r11d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r9d
	mov	DWORD PTR[60+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r11d
	add	r12d,eax

	mov	eax,ebx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ebx
	mov	r14d,ebx

	ror	eax,2
	ror	r13d,13
	mov	r15d,ebx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	eax,r13d
	ror	r13d,9
	or	r14d,edx

	xor	eax,r13d
	and	r15d,edx
	add	r8d,r12d

	and	r14d,ecx
	add	eax,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	eax,r14d
	jmp	$L$rounds_16_xx
ALIGN	16
$L$rounds_16_xx::
	mov	r13d,DWORD PTR[4+rsp]
	mov	r12d,DWORD PTR[56+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[36+rsp]

	add	r12d,DWORD PTR[rsp]
	mov	r13d,r8d
	mov	r14d,r8d
	mov	r15d,r9d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r10d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r8d
	mov	DWORD PTR[rsp],r12d

	xor	r13d,r14d
	xor	r15d,r10d
	add	r12d,r11d

	mov	r11d,eax
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,eax
	mov	r14d,eax

	ror	r11d,2
	ror	r13d,13
	mov	r15d,eax
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r11d,r13d
	ror	r13d,9
	or	r14d,ecx

	xor	r11d,r13d
	and	r15d,ecx
	add	edx,r12d

	and	r14d,ebx
	add	r11d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r11d,r14d
	mov	r13d,DWORD PTR[8+rsp]
	mov	r12d,DWORD PTR[60+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[40+rsp]

	add	r12d,DWORD PTR[4+rsp]
	mov	r13d,edx
	mov	r14d,edx
	mov	r15d,r8d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r9d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,edx
	mov	DWORD PTR[4+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r9d
	add	r12d,r10d

	mov	r10d,r11d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r11d
	mov	r14d,r11d

	ror	r10d,2
	ror	r13d,13
	mov	r15d,r11d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r10d,r13d
	ror	r13d,9
	or	r14d,ebx

	xor	r10d,r13d
	and	r15d,ebx
	add	ecx,r12d

	and	r14d,eax
	add	r10d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r10d,r14d
	mov	r13d,DWORD PTR[12+rsp]
	mov	r12d,DWORD PTR[rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[44+rsp]

	add	r12d,DWORD PTR[8+rsp]
	mov	r13d,ecx
	mov	r14d,ecx
	mov	r15d,edx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r8d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ecx
	mov	DWORD PTR[8+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r8d
	add	r12d,r9d

	mov	r9d,r10d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r10d
	mov	r14d,r10d

	ror	r9d,2
	ror	r13d,13
	mov	r15d,r10d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r9d,r13d
	ror	r13d,9
	or	r14d,eax

	xor	r9d,r13d
	and	r15d,eax
	add	ebx,r12d

	and	r14d,r11d
	add	r9d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r9d,r14d
	mov	r13d,DWORD PTR[16+rsp]
	mov	r12d,DWORD PTR[4+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[48+rsp]

	add	r12d,DWORD PTR[12+rsp]
	mov	r13d,ebx
	mov	r14d,ebx
	mov	r15d,ecx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,edx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ebx
	mov	DWORD PTR[12+rsp],r12d

	xor	r13d,r14d
	xor	r15d,edx
	add	r12d,r8d

	mov	r8d,r9d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r9d
	mov	r14d,r9d

	ror	r8d,2
	ror	r13d,13
	mov	r15d,r9d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r8d,r13d
	ror	r13d,9
	or	r14d,r11d

	xor	r8d,r13d
	and	r15d,r11d
	add	eax,r12d

	and	r14d,r10d
	add	r8d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r8d,r14d
	mov	r13d,DWORD PTR[20+rsp]
	mov	r12d,DWORD PTR[8+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[52+rsp]

	add	r12d,DWORD PTR[16+rsp]
	mov	r13d,eax
	mov	r14d,eax
	mov	r15d,ebx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ecx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,eax
	mov	DWORD PTR[16+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ecx
	add	r12d,edx

	mov	edx,r8d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r8d
	mov	r14d,r8d

	ror	edx,2
	ror	r13d,13
	mov	r15d,r8d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	edx,r13d
	ror	r13d,9
	or	r14d,r10d

	xor	edx,r13d
	and	r15d,r10d
	add	r11d,r12d

	and	r14d,r9d
	add	edx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	edx,r14d
	mov	r13d,DWORD PTR[24+rsp]
	mov	r12d,DWORD PTR[12+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[56+rsp]

	add	r12d,DWORD PTR[20+rsp]
	mov	r13d,r11d
	mov	r14d,r11d
	mov	r15d,eax

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ebx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r11d
	mov	DWORD PTR[20+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ebx
	add	r12d,ecx

	mov	ecx,edx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,edx
	mov	r14d,edx

	ror	ecx,2
	ror	r13d,13
	mov	r15d,edx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ecx,r13d
	ror	r13d,9
	or	r14d,r9d

	xor	ecx,r13d
	and	r15d,r9d
	add	r10d,r12d

	and	r14d,r8d
	add	ecx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ecx,r14d
	mov	r13d,DWORD PTR[28+rsp]
	mov	r12d,DWORD PTR[16+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[60+rsp]

	add	r12d,DWORD PTR[24+rsp]
	mov	r13d,r10d
	mov	r14d,r10d
	mov	r15d,r11d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,eax

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r10d
	mov	DWORD PTR[24+rsp],r12d

	xor	r13d,r14d
	xor	r15d,eax
	add	r12d,ebx

	mov	ebx,ecx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ecx
	mov	r14d,ecx

	ror	ebx,2
	ror	r13d,13
	mov	r15d,ecx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ebx,r13d
	ror	r13d,9
	or	r14d,r8d

	xor	ebx,r13d
	and	r15d,r8d
	add	r9d,r12d

	and	r14d,edx
	add	ebx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ebx,r14d
	mov	r13d,DWORD PTR[32+rsp]
	mov	r12d,DWORD PTR[20+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[rsp]

	add	r12d,DWORD PTR[28+rsp]
	mov	r13d,r9d
	mov	r14d,r9d
	mov	r15d,r10d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r11d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r9d
	mov	DWORD PTR[28+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r11d
	add	r12d,eax

	mov	eax,ebx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ebx
	mov	r14d,ebx

	ror	eax,2
	ror	r13d,13
	mov	r15d,ebx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	eax,r13d
	ror	r13d,9
	or	r14d,edx

	xor	eax,r13d
	and	r15d,edx
	add	r8d,r12d

	and	r14d,ecx
	add	eax,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	eax,r14d
	mov	r13d,DWORD PTR[36+rsp]
	mov	r12d,DWORD PTR[24+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[4+rsp]

	add	r12d,DWORD PTR[32+rsp]
	mov	r13d,r8d
	mov	r14d,r8d
	mov	r15d,r9d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r10d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r8d
	mov	DWORD PTR[32+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r10d
	add	r12d,r11d

	mov	r11d,eax
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,eax
	mov	r14d,eax

	ror	r11d,2
	ror	r13d,13
	mov	r15d,eax
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r11d,r13d
	ror	r13d,9
	or	r14d,ecx

	xor	r11d,r13d
	and	r15d,ecx
	add	edx,r12d

	and	r14d,ebx
	add	r11d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r11d,r14d
	mov	r13d,DWORD PTR[40+rsp]
	mov	r12d,DWORD PTR[28+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[8+rsp]

	add	r12d,DWORD PTR[36+rsp]
	mov	r13d,edx
	mov	r14d,edx
	mov	r15d,r8d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r9d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,edx
	mov	DWORD PTR[36+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r9d
	add	r12d,r10d

	mov	r10d,r11d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r11d
	mov	r14d,r11d

	ror	r10d,2
	ror	r13d,13
	mov	r15d,r11d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r10d,r13d
	ror	r13d,9
	or	r14d,ebx

	xor	r10d,r13d
	and	r15d,ebx
	add	ecx,r12d

	and	r14d,eax
	add	r10d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r10d,r14d
	mov	r13d,DWORD PTR[44+rsp]
	mov	r12d,DWORD PTR[32+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[12+rsp]

	add	r12d,DWORD PTR[40+rsp]
	mov	r13d,ecx
	mov	r14d,ecx
	mov	r15d,edx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r8d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ecx
	mov	DWORD PTR[40+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r8d
	add	r12d,r9d

	mov	r9d,r10d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r10d
	mov	r14d,r10d

	ror	r9d,2
	ror	r13d,13
	mov	r15d,r10d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r9d,r13d
	ror	r13d,9
	or	r14d,eax

	xor	r9d,r13d
	and	r15d,eax
	add	ebx,r12d

	and	r14d,r11d
	add	r9d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r9d,r14d
	mov	r13d,DWORD PTR[48+rsp]
	mov	r12d,DWORD PTR[36+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[16+rsp]

	add	r12d,DWORD PTR[44+rsp]
	mov	r13d,ebx
	mov	r14d,ebx
	mov	r15d,ecx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,edx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,ebx
	mov	DWORD PTR[44+rsp],r12d

	xor	r13d,r14d
	xor	r15d,edx
	add	r12d,r8d

	mov	r8d,r9d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r9d
	mov	r14d,r9d

	ror	r8d,2
	ror	r13d,13
	mov	r15d,r9d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	r8d,r13d
	ror	r13d,9
	or	r14d,r11d

	xor	r8d,r13d
	and	r15d,r11d
	add	eax,r12d

	and	r14d,r10d
	add	r8d,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	r8d,r14d
	mov	r13d,DWORD PTR[52+rsp]
	mov	r12d,DWORD PTR[40+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[20+rsp]

	add	r12d,DWORD PTR[48+rsp]
	mov	r13d,eax
	mov	r14d,eax
	mov	r15d,ebx

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ecx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,eax
	mov	DWORD PTR[48+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ecx
	add	r12d,edx

	mov	edx,r8d
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,r8d
	mov	r14d,r8d

	ror	edx,2
	ror	r13d,13
	mov	r15d,r8d
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	edx,r13d
	ror	r13d,9
	or	r14d,r10d

	xor	edx,r13d
	and	r15d,r10d
	add	r11d,r12d

	and	r14d,r9d
	add	edx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	edx,r14d
	mov	r13d,DWORD PTR[56+rsp]
	mov	r12d,DWORD PTR[44+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[24+rsp]

	add	r12d,DWORD PTR[52+rsp]
	mov	r13d,r11d
	mov	r14d,r11d
	mov	r15d,eax

	ror	r13d,6
	ror	r14d,11
	xor	r15d,ebx

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r11d
	mov	DWORD PTR[52+rsp],r12d

	xor	r13d,r14d
	xor	r15d,ebx
	add	r12d,ecx

	mov	ecx,edx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,edx
	mov	r14d,edx

	ror	ecx,2
	ror	r13d,13
	mov	r15d,edx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ecx,r13d
	ror	r13d,9
	or	r14d,r9d

	xor	ecx,r13d
	and	r15d,r9d
	add	r10d,r12d

	and	r14d,r8d
	add	ecx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ecx,r14d
	mov	r13d,DWORD PTR[60+rsp]
	mov	r12d,DWORD PTR[48+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[28+rsp]

	add	r12d,DWORD PTR[56+rsp]
	mov	r13d,r10d
	mov	r14d,r10d
	mov	r15d,r11d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,eax

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r10d
	mov	DWORD PTR[56+rsp],r12d

	xor	r13d,r14d
	xor	r15d,eax
	add	r12d,ebx

	mov	ebx,ecx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ecx
	mov	r14d,ecx

	ror	ebx,2
	ror	r13d,13
	mov	r15d,ecx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	ebx,r13d
	ror	r13d,9
	or	r14d,r8d

	xor	ebx,r13d
	and	r15d,r8d
	add	r9d,r12d

	and	r14d,edx
	add	ebx,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	ebx,r14d
	mov	r13d,DWORD PTR[rsp]
	mov	r12d,DWORD PTR[52+rsp]

	mov	r15d,r13d

	shr	r13d,3
	ror	r15d,7

	xor	r13d,r15d
	ror	r15d,11

	xor	r13d,r15d
	mov	r14d,r12d

	shr	r12d,10
	ror	r14d,17

	xor	r12d,r14d
	ror	r14d,2

	xor	r12d,r14d

	add	r12d,r13d

	add	r12d,DWORD PTR[32+rsp]

	add	r12d,DWORD PTR[60+rsp]
	mov	r13d,r9d
	mov	r14d,r9d
	mov	r15d,r10d

	ror	r13d,6
	ror	r14d,11
	xor	r15d,r11d

	xor	r13d,r14d
	ror	r14d,14
	and	r15d,r9d
	mov	DWORD PTR[60+rsp],r12d

	xor	r13d,r14d
	xor	r15d,r11d
	add	r12d,eax

	mov	eax,ebx
	add	r12d,r13d

	add	r12d,r15d
	mov	r13d,ebx
	mov	r14d,ebx

	ror	eax,2
	ror	r13d,13
	mov	r15d,ebx
	add	r12d,DWORD PTR[rdi*4+rbp]

	xor	eax,r13d
	ror	r13d,9
	or	r14d,edx

	xor	eax,r13d
	and	r15d,edx
	add	r8d,r12d

	and	r14d,ecx
	add	eax,r12d

	or	r14d,r15d
	lea	rdi,QWORD PTR[1+rdi]

	add	eax,r14d
	cmp	rdi,64
	jb	$L$rounds_16_xx

	mov	rdi,QWORD PTR[((16*4+0*8))+rsp]
	lea	rsi,QWORD PTR[((16*4))+rsi]

	add	eax,DWORD PTR[((4*0))+rdi]
	add	ebx,DWORD PTR[((4*1))+rdi]
	add	ecx,DWORD PTR[((4*2))+rdi]
	add	edx,DWORD PTR[((4*3))+rdi]
	add	r8d,DWORD PTR[((4*4))+rdi]
	add	r9d,DWORD PTR[((4*5))+rdi]
	add	r10d,DWORD PTR[((4*6))+rdi]
	add	r11d,DWORD PTR[((4*7))+rdi]

	cmp	rsi,QWORD PTR[((16*4+2*8))+rsp]

	mov	DWORD PTR[((4*0))+rdi],eax
	mov	DWORD PTR[((4*1))+rdi],ebx
	mov	DWORD PTR[((4*2))+rdi],ecx
	mov	DWORD PTR[((4*3))+rdi],edx
	mov	DWORD PTR[((4*4))+rdi],r8d
	mov	DWORD PTR[((4*5))+rdi],r9d
	mov	DWORD PTR[((4*6))+rdi],r10d
	mov	DWORD PTR[((4*7))+rdi],r11d
	jb	$L$loop

	mov	rsi,QWORD PTR[((16*4+3*8))+rsp]
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

	mov	rax,QWORD PTR[((16*4+3*8))+rax]
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
