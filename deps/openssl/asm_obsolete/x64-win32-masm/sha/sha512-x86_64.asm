OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	sha512_block_data_order

ALIGN	16
sha512_block_data_order	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha512_block_data_order::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	shl	rdx,4
	sub	rsp,16*8+4*8
	lea	rdx,QWORD PTR[rdx*8+rsi]
	and	rsp,-64
	mov	QWORD PTR[((128+0))+rsp],rdi
	mov	QWORD PTR[((128+8))+rsp],rsi
	mov	QWORD PTR[((128+16))+rsp],rdx
	mov	QWORD PTR[((128+24))+rsp],r11
$L$prologue::

	mov	rax,QWORD PTR[rdi]
	mov	rbx,QWORD PTR[8+rdi]
	mov	rcx,QWORD PTR[16+rdi]
	mov	rdx,QWORD PTR[24+rdi]
	mov	r8,QWORD PTR[32+rdi]
	mov	r9,QWORD PTR[40+rdi]
	mov	r10,QWORD PTR[48+rdi]
	mov	r11,QWORD PTR[56+rdi]
	jmp	$L$loop

ALIGN	16
$L$loop::
	mov	rdi,rbx
	lea	rbp,QWORD PTR[K512]
	xor	rdi,rcx
	mov	r12,QWORD PTR[rsi]
	mov	r13,r8
	mov	r14,rax
	bswap	r12
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD PTR[rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD PTR[rbp]
	xor	r14,rax

	xor	r15,rbx
	ror	r13,14
	mov	r11,rbx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r11,rdi
	add	rdx,r12
	add	r11,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	r11,r14
	mov	r12,QWORD PTR[8+rsi]
	mov	r13,rdx
	mov	r14,r11
	bswap	r12
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD PTR[8+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD PTR[rbp]
	xor	r14,r11

	xor	rdi,rax
	ror	r13,14
	mov	r10,rax

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r10,r15
	add	rcx,r12
	add	r10,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	r10,r14
	mov	r12,QWORD PTR[16+rsi]
	mov	r13,rcx
	mov	r14,r10
	bswap	r12
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD PTR[16+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD PTR[rbp]
	xor	r14,r10

	xor	r15,r11
	ror	r13,14
	mov	r9,r11

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r9,rdi
	add	rbx,r12
	add	r9,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	r9,r14
	mov	r12,QWORD PTR[24+rsi]
	mov	r13,rbx
	mov	r14,r9
	bswap	r12
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD PTR[24+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD PTR[rbp]
	xor	r14,r9

	xor	rdi,r10
	ror	r13,14
	mov	r8,r10

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r8,r15
	add	rax,r12
	add	r8,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	r8,r14
	mov	r12,QWORD PTR[32+rsi]
	mov	r13,rax
	mov	r14,r8
	bswap	r12
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD PTR[32+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD PTR[rbp]
	xor	r14,r8

	xor	r15,r9
	ror	r13,14
	mov	rdx,r9

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rdx,rdi
	add	r11,r12
	add	rdx,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	rdx,r14
	mov	r12,QWORD PTR[40+rsi]
	mov	r13,r11
	mov	r14,rdx
	bswap	r12
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD PTR[40+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD PTR[rbp]
	xor	r14,rdx

	xor	rdi,r8
	ror	r13,14
	mov	rcx,r8

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rcx,r15
	add	r10,r12
	add	rcx,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	rcx,r14
	mov	r12,QWORD PTR[48+rsi]
	mov	r13,r10
	mov	r14,rcx
	bswap	r12
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD PTR[48+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD PTR[rbp]
	xor	r14,rcx

	xor	r15,rdx
	ror	r13,14
	mov	rbx,rdx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rbx,rdi
	add	r9,r12
	add	rbx,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	rbx,r14
	mov	r12,QWORD PTR[56+rsi]
	mov	r13,r9
	mov	r14,rbx
	bswap	r12
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD PTR[56+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD PTR[rbp]
	xor	r14,rbx

	xor	rdi,rcx
	ror	r13,14
	mov	rax,rcx

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rax,r15
	add	r8,r12
	add	rax,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	rax,r14
	mov	r12,QWORD PTR[64+rsi]
	mov	r13,r8
	mov	r14,rax
	bswap	r12
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD PTR[64+rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD PTR[rbp]
	xor	r14,rax

	xor	r15,rbx
	ror	r13,14
	mov	r11,rbx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r11,rdi
	add	rdx,r12
	add	r11,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	r11,r14
	mov	r12,QWORD PTR[72+rsi]
	mov	r13,rdx
	mov	r14,r11
	bswap	r12
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD PTR[72+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD PTR[rbp]
	xor	r14,r11

	xor	rdi,rax
	ror	r13,14
	mov	r10,rax

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r10,r15
	add	rcx,r12
	add	r10,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	r10,r14
	mov	r12,QWORD PTR[80+rsi]
	mov	r13,rcx
	mov	r14,r10
	bswap	r12
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD PTR[80+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD PTR[rbp]
	xor	r14,r10

	xor	r15,r11
	ror	r13,14
	mov	r9,r11

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r9,rdi
	add	rbx,r12
	add	r9,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	r9,r14
	mov	r12,QWORD PTR[88+rsi]
	mov	r13,rbx
	mov	r14,r9
	bswap	r12
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD PTR[88+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD PTR[rbp]
	xor	r14,r9

	xor	rdi,r10
	ror	r13,14
	mov	r8,r10

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r8,r15
	add	rax,r12
	add	r8,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	r8,r14
	mov	r12,QWORD PTR[96+rsi]
	mov	r13,rax
	mov	r14,r8
	bswap	r12
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD PTR[96+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD PTR[rbp]
	xor	r14,r8

	xor	r15,r9
	ror	r13,14
	mov	rdx,r9

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rdx,rdi
	add	r11,r12
	add	rdx,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	rdx,r14
	mov	r12,QWORD PTR[104+rsi]
	mov	r13,r11
	mov	r14,rdx
	bswap	r12
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD PTR[104+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD PTR[rbp]
	xor	r14,rdx

	xor	rdi,r8
	ror	r13,14
	mov	rcx,r8

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rcx,r15
	add	r10,r12
	add	rcx,r12

	lea	rbp,QWORD PTR[24+rbp]
	add	rcx,r14
	mov	r12,QWORD PTR[112+rsi]
	mov	r13,r10
	mov	r14,rcx
	bswap	r12
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD PTR[112+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD PTR[rbp]
	xor	r14,rcx

	xor	r15,rdx
	ror	r13,14
	mov	rbx,rdx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rbx,rdi
	add	r9,r12
	add	rbx,r12

	lea	rbp,QWORD PTR[8+rbp]
	add	rbx,r14
	mov	r12,QWORD PTR[120+rsi]
	mov	r13,r9
	mov	r14,rbx
	bswap	r12
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD PTR[120+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD PTR[rbp]
	xor	r14,rbx

	xor	rdi,rcx
	ror	r13,14
	mov	rax,rcx

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rax,r15
	add	r8,r12
	add	rax,r12

	lea	rbp,QWORD PTR[24+rbp]
	jmp	$L$rounds_16_xx
ALIGN	16
$L$rounds_16_xx::
	mov	r13,QWORD PTR[8+rsp]
	mov	r15,QWORD PTR[112+rsp]

	mov	r12,r13
	ror	r13,7
	add	rax,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[72+rsp]

	add	r12,QWORD PTR[rsp]
	mov	r13,r8
	add	r12,r15
	mov	r14,rax
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD PTR[rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD PTR[rbp]
	xor	r14,rax

	xor	r15,rbx
	ror	r13,14
	mov	r11,rbx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r11,rdi
	add	rdx,r12
	add	r11,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[16+rsp]
	mov	rdi,QWORD PTR[120+rsp]

	mov	r12,r13
	ror	r13,7
	add	r11,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[80+rsp]

	add	r12,QWORD PTR[8+rsp]
	mov	r13,rdx
	add	r12,rdi
	mov	r14,r11
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD PTR[8+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD PTR[rbp]
	xor	r14,r11

	xor	rdi,rax
	ror	r13,14
	mov	r10,rax

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r10,r15
	add	rcx,r12
	add	r10,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[24+rsp]
	mov	r15,QWORD PTR[rsp]

	mov	r12,r13
	ror	r13,7
	add	r10,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[88+rsp]

	add	r12,QWORD PTR[16+rsp]
	mov	r13,rcx
	add	r12,r15
	mov	r14,r10
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD PTR[16+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD PTR[rbp]
	xor	r14,r10

	xor	r15,r11
	ror	r13,14
	mov	r9,r11

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r9,rdi
	add	rbx,r12
	add	r9,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[32+rsp]
	mov	rdi,QWORD PTR[8+rsp]

	mov	r12,r13
	ror	r13,7
	add	r9,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[96+rsp]

	add	r12,QWORD PTR[24+rsp]
	mov	r13,rbx
	add	r12,rdi
	mov	r14,r9
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD PTR[24+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD PTR[rbp]
	xor	r14,r9

	xor	rdi,r10
	ror	r13,14
	mov	r8,r10

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r8,r15
	add	rax,r12
	add	r8,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[40+rsp]
	mov	r15,QWORD PTR[16+rsp]

	mov	r12,r13
	ror	r13,7
	add	r8,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[104+rsp]

	add	r12,QWORD PTR[32+rsp]
	mov	r13,rax
	add	r12,r15
	mov	r14,r8
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD PTR[32+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD PTR[rbp]
	xor	r14,r8

	xor	r15,r9
	ror	r13,14
	mov	rdx,r9

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rdx,rdi
	add	r11,r12
	add	rdx,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[48+rsp]
	mov	rdi,QWORD PTR[24+rsp]

	mov	r12,r13
	ror	r13,7
	add	rdx,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[112+rsp]

	add	r12,QWORD PTR[40+rsp]
	mov	r13,r11
	add	r12,rdi
	mov	r14,rdx
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD PTR[40+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD PTR[rbp]
	xor	r14,rdx

	xor	rdi,r8
	ror	r13,14
	mov	rcx,r8

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rcx,r15
	add	r10,r12
	add	rcx,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[56+rsp]
	mov	r15,QWORD PTR[32+rsp]

	mov	r12,r13
	ror	r13,7
	add	rcx,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[120+rsp]

	add	r12,QWORD PTR[48+rsp]
	mov	r13,r10
	add	r12,r15
	mov	r14,rcx
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD PTR[48+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD PTR[rbp]
	xor	r14,rcx

	xor	r15,rdx
	ror	r13,14
	mov	rbx,rdx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rbx,rdi
	add	r9,r12
	add	rbx,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[64+rsp]
	mov	rdi,QWORD PTR[40+rsp]

	mov	r12,r13
	ror	r13,7
	add	rbx,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[rsp]

	add	r12,QWORD PTR[56+rsp]
	mov	r13,r9
	add	r12,rdi
	mov	r14,rbx
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD PTR[56+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD PTR[rbp]
	xor	r14,rbx

	xor	rdi,rcx
	ror	r13,14
	mov	rax,rcx

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rax,r15
	add	r8,r12
	add	rax,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[72+rsp]
	mov	r15,QWORD PTR[48+rsp]

	mov	r12,r13
	ror	r13,7
	add	rax,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[8+rsp]

	add	r12,QWORD PTR[64+rsp]
	mov	r13,r8
	add	r12,r15
	mov	r14,rax
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD PTR[64+rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD PTR[rbp]
	xor	r14,rax

	xor	r15,rbx
	ror	r13,14
	mov	r11,rbx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r11,rdi
	add	rdx,r12
	add	r11,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[80+rsp]
	mov	rdi,QWORD PTR[56+rsp]

	mov	r12,r13
	ror	r13,7
	add	r11,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[16+rsp]

	add	r12,QWORD PTR[72+rsp]
	mov	r13,rdx
	add	r12,rdi
	mov	r14,r11
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD PTR[72+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD PTR[rbp]
	xor	r14,r11

	xor	rdi,rax
	ror	r13,14
	mov	r10,rax

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r10,r15
	add	rcx,r12
	add	r10,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[88+rsp]
	mov	r15,QWORD PTR[64+rsp]

	mov	r12,r13
	ror	r13,7
	add	r10,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[24+rsp]

	add	r12,QWORD PTR[80+rsp]
	mov	r13,rcx
	add	r12,r15
	mov	r14,r10
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD PTR[80+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD PTR[rbp]
	xor	r14,r10

	xor	r15,r11
	ror	r13,14
	mov	r9,r11

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	r9,rdi
	add	rbx,r12
	add	r9,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[96+rsp]
	mov	rdi,QWORD PTR[72+rsp]

	mov	r12,r13
	ror	r13,7
	add	r9,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[32+rsp]

	add	r12,QWORD PTR[88+rsp]
	mov	r13,rbx
	add	r12,rdi
	mov	r14,r9
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD PTR[88+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD PTR[rbp]
	xor	r14,r9

	xor	rdi,r10
	ror	r13,14
	mov	r8,r10

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	r8,r15
	add	rax,r12
	add	r8,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[104+rsp]
	mov	r15,QWORD PTR[80+rsp]

	mov	r12,r13
	ror	r13,7
	add	r8,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[40+rsp]

	add	r12,QWORD PTR[96+rsp]
	mov	r13,rax
	add	r12,r15
	mov	r14,r8
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD PTR[96+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD PTR[rbp]
	xor	r14,r8

	xor	r15,r9
	ror	r13,14
	mov	rdx,r9

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rdx,rdi
	add	r11,r12
	add	rdx,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[112+rsp]
	mov	rdi,QWORD PTR[88+rsp]

	mov	r12,r13
	ror	r13,7
	add	rdx,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[48+rsp]

	add	r12,QWORD PTR[104+rsp]
	mov	r13,r11
	add	r12,rdi
	mov	r14,rdx
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD PTR[104+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD PTR[rbp]
	xor	r14,rdx

	xor	rdi,r8
	ror	r13,14
	mov	rcx,r8

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rcx,r15
	add	r10,r12
	add	rcx,r12

	lea	rbp,QWORD PTR[24+rbp]
	mov	r13,QWORD PTR[120+rsp]
	mov	r15,QWORD PTR[96+rsp]

	mov	r12,r13
	ror	r13,7
	add	rcx,r14
	mov	r14,r15
	ror	r15,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	r15,r14
	shr	r14,6

	ror	r15,19
	xor	r12,r13
	xor	r15,r14
	add	r12,QWORD PTR[56+rsp]

	add	r12,QWORD PTR[112+rsp]
	mov	r13,r10
	add	r12,r15
	mov	r14,rcx
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD PTR[112+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD PTR[rbp]
	xor	r14,rcx

	xor	r15,rdx
	ror	r13,14
	mov	rbx,rdx

	and	rdi,r15
	ror	r14,28
	add	r12,r13

	xor	rbx,rdi
	add	r9,r12
	add	rbx,r12

	lea	rbp,QWORD PTR[8+rbp]
	mov	r13,QWORD PTR[rsp]
	mov	rdi,QWORD PTR[104+rsp]

	mov	r12,r13
	ror	r13,7
	add	rbx,r14
	mov	r14,rdi
	ror	rdi,42

	xor	r13,r12
	shr	r12,7
	ror	r13,1
	xor	rdi,r14
	shr	r14,6

	ror	rdi,19
	xor	r12,r13
	xor	rdi,r14
	add	r12,QWORD PTR[64+rsp]

	add	r12,QWORD PTR[120+rsp]
	mov	r13,r9
	add	r12,rdi
	mov	r14,rbx
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD PTR[120+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD PTR[rbp]
	xor	r14,rbx

	xor	rdi,rcx
	ror	r13,14
	mov	rax,rcx

	and	r15,rdi
	ror	r14,28
	add	r12,r13

	xor	rax,r15
	add	r8,r12
	add	rax,r12

	lea	rbp,QWORD PTR[24+rbp]
	cmp	BYTE PTR[7+rbp],0
	jnz	$L$rounds_16_xx

	mov	rdi,QWORD PTR[((128+0))+rsp]
	add	rax,r14
	lea	rsi,QWORD PTR[128+rsi]

	add	rax,QWORD PTR[rdi]
	add	rbx,QWORD PTR[8+rdi]
	add	rcx,QWORD PTR[16+rdi]
	add	rdx,QWORD PTR[24+rdi]
	add	r8,QWORD PTR[32+rdi]
	add	r9,QWORD PTR[40+rdi]
	add	r10,QWORD PTR[48+rdi]
	add	r11,QWORD PTR[56+rdi]

	cmp	rsi,QWORD PTR[((128+16))+rsp]

	mov	QWORD PTR[rdi],rax
	mov	QWORD PTR[8+rdi],rbx
	mov	QWORD PTR[16+rdi],rcx
	mov	QWORD PTR[24+rdi],rdx
	mov	QWORD PTR[32+rdi],r8
	mov	QWORD PTR[40+rdi],r9
	mov	QWORD PTR[48+rdi],r10
	mov	QWORD PTR[56+rdi],r11
	jb	$L$loop

	mov	rsi,QWORD PTR[((128+24))+rsp]
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
$L$SEH_end_sha512_block_data_order::
sha512_block_data_order	ENDP
ALIGN	64

K512::
	DQ	0428a2f98d728ae22h,07137449123ef65cdh
	DQ	0428a2f98d728ae22h,07137449123ef65cdh
	DQ	0b5c0fbcfec4d3b2fh,0e9b5dba58189dbbch
	DQ	0b5c0fbcfec4d3b2fh,0e9b5dba58189dbbch
	DQ	03956c25bf348b538h,059f111f1b605d019h
	DQ	03956c25bf348b538h,059f111f1b605d019h
	DQ	0923f82a4af194f9bh,0ab1c5ed5da6d8118h
	DQ	0923f82a4af194f9bh,0ab1c5ed5da6d8118h
	DQ	0d807aa98a3030242h,012835b0145706fbeh
	DQ	0d807aa98a3030242h,012835b0145706fbeh
	DQ	0243185be4ee4b28ch,0550c7dc3d5ffb4e2h
	DQ	0243185be4ee4b28ch,0550c7dc3d5ffb4e2h
	DQ	072be5d74f27b896fh,080deb1fe3b1696b1h
	DQ	072be5d74f27b896fh,080deb1fe3b1696b1h
	DQ	09bdc06a725c71235h,0c19bf174cf692694h
	DQ	09bdc06a725c71235h,0c19bf174cf692694h
	DQ	0e49b69c19ef14ad2h,0efbe4786384f25e3h
	DQ	0e49b69c19ef14ad2h,0efbe4786384f25e3h
	DQ	00fc19dc68b8cd5b5h,0240ca1cc77ac9c65h
	DQ	00fc19dc68b8cd5b5h,0240ca1cc77ac9c65h
	DQ	02de92c6f592b0275h,04a7484aa6ea6e483h
	DQ	02de92c6f592b0275h,04a7484aa6ea6e483h
	DQ	05cb0a9dcbd41fbd4h,076f988da831153b5h
	DQ	05cb0a9dcbd41fbd4h,076f988da831153b5h
	DQ	0983e5152ee66dfabh,0a831c66d2db43210h
	DQ	0983e5152ee66dfabh,0a831c66d2db43210h
	DQ	0b00327c898fb213fh,0bf597fc7beef0ee4h
	DQ	0b00327c898fb213fh,0bf597fc7beef0ee4h
	DQ	0c6e00bf33da88fc2h,0d5a79147930aa725h
	DQ	0c6e00bf33da88fc2h,0d5a79147930aa725h
	DQ	006ca6351e003826fh,0142929670a0e6e70h
	DQ	006ca6351e003826fh,0142929670a0e6e70h
	DQ	027b70a8546d22ffch,02e1b21385c26c926h
	DQ	027b70a8546d22ffch,02e1b21385c26c926h
	DQ	04d2c6dfc5ac42aedh,053380d139d95b3dfh
	DQ	04d2c6dfc5ac42aedh,053380d139d95b3dfh
	DQ	0650a73548baf63deh,0766a0abb3c77b2a8h
	DQ	0650a73548baf63deh,0766a0abb3c77b2a8h
	DQ	081c2c92e47edaee6h,092722c851482353bh
	DQ	081c2c92e47edaee6h,092722c851482353bh
	DQ	0a2bfe8a14cf10364h,0a81a664bbc423001h
	DQ	0a2bfe8a14cf10364h,0a81a664bbc423001h
	DQ	0c24b8b70d0f89791h,0c76c51a30654be30h
	DQ	0c24b8b70d0f89791h,0c76c51a30654be30h
	DQ	0d192e819d6ef5218h,0d69906245565a910h
	DQ	0d192e819d6ef5218h,0d69906245565a910h
	DQ	0f40e35855771202ah,0106aa07032bbd1b8h
	DQ	0f40e35855771202ah,0106aa07032bbd1b8h
	DQ	019a4c116b8d2d0c8h,01e376c085141ab53h
	DQ	019a4c116b8d2d0c8h,01e376c085141ab53h
	DQ	02748774cdf8eeb99h,034b0bcb5e19b48a8h
	DQ	02748774cdf8eeb99h,034b0bcb5e19b48a8h
	DQ	0391c0cb3c5c95a63h,04ed8aa4ae3418acbh
	DQ	0391c0cb3c5c95a63h,04ed8aa4ae3418acbh
	DQ	05b9cca4f7763e373h,0682e6ff3d6b2b8a3h
	DQ	05b9cca4f7763e373h,0682e6ff3d6b2b8a3h
	DQ	0748f82ee5defb2fch,078a5636f43172f60h
	DQ	0748f82ee5defb2fch,078a5636f43172f60h
	DQ	084c87814a1f0ab72h,08cc702081a6439ech
	DQ	084c87814a1f0ab72h,08cc702081a6439ech
	DQ	090befffa23631e28h,0a4506cebde82bde9h
	DQ	090befffa23631e28h,0a4506cebde82bde9h
	DQ	0bef9a3f7b2c67915h,0c67178f2e372532bh
	DQ	0bef9a3f7b2c67915h,0c67178f2e372532bh
	DQ	0ca273eceea26619ch,0d186b8c721c0c207h
	DQ	0ca273eceea26619ch,0d186b8c721c0c207h
	DQ	0eada7dd6cde0eb1eh,0f57d4f7fee6ed178h
	DQ	0eada7dd6cde0eb1eh,0f57d4f7fee6ed178h
	DQ	006f067aa72176fbah,00a637dc5a2c898a6h
	DQ	006f067aa72176fbah,00a637dc5a2c898a6h
	DQ	0113f9804bef90daeh,01b710b35131c471bh
	DQ	0113f9804bef90daeh,01b710b35131c471bh
	DQ	028db77f523047d84h,032caab7b40c72493h
	DQ	028db77f523047d84h,032caab7b40c72493h
	DQ	03c9ebe0a15c9bebch,0431d67c49c100d4ch
	DQ	03c9ebe0a15c9bebch,0431d67c49c100d4ch
	DQ	04cc5d4becb3e42b6h,0597f299cfc657e2ah
	DQ	04cc5d4becb3e42b6h,0597f299cfc657e2ah
	DQ	05fcb6fab3ad6faech,06c44198c4a475817h
	DQ	05fcb6fab3ad6faech,06c44198c4a475817h

	DQ	00001020304050607h,008090a0b0c0d0e0fh
	DQ	00001020304050607h,008090a0b0c0d0e0fh
DB	83,72,65,53,49,50,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,95,54
DB	52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121
DB	32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46
DB	111,114,103,62,0
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
	mov	rax,QWORD PTR[((128+24))+rax]
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

	lea	rsi,QWORD PTR[((128+32))+rsi]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,12
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
	DD	imagerel $L$SEH_begin_sha512_block_data_order
	DD	imagerel $L$SEH_end_sha512_block_data_order
	DD	imagerel $L$SEH_info_sha512_block_data_order
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha512_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue,imagerel $L$epilogue

.xdata	ENDS
END
