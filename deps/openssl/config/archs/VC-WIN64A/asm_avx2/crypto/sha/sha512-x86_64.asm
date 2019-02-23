default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P
global	sha512_block_data_order

ALIGN	16
sha512_block_data_order:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha512_block_data_order:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



	lea	r11,[OPENSSL_ia32cap_P]
	mov	r9d,DWORD[r11]
	mov	r10d,DWORD[4+r11]
	mov	r11d,DWORD[8+r11]
	test	r10d,2048
	jnz	NEAR $L$xop_shortcut
	and	r11d,296
	cmp	r11d,296
	je	NEAR $L$avx2_shortcut
	and	r9d,1073741824
	and	r10d,268435968
	or	r10d,r9d
	cmp	r10d,1342177792
	je	NEAR $L$avx_shortcut
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

	shl	rdx,4
	sub	rsp,16*8+4*8
	lea	rdx,[rdx*8+rsi]
	and	rsp,-64
	mov	QWORD[((128+0))+rsp],rdi
	mov	QWORD[((128+8))+rsp],rsi
	mov	QWORD[((128+16))+rsp],rdx
	mov	QWORD[152+rsp],rax

$L$prologue:

	mov	rax,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	rcx,QWORD[16+rdi]
	mov	rdx,QWORD[24+rdi]
	mov	r8,QWORD[32+rdi]
	mov	r9,QWORD[40+rdi]
	mov	r10,QWORD[48+rdi]
	mov	r11,QWORD[56+rdi]
	jmp	NEAR $L$loop

ALIGN	16
$L$loop:
	mov	rdi,rbx
	lea	rbp,[K512]
	xor	rdi,rcx
	mov	r12,QWORD[rsi]
	mov	r13,r8
	mov	r14,rax
	bswap	r12
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD[rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	r11,r14
	mov	r12,QWORD[8+rsi]
	mov	r13,rdx
	mov	r14,r11
	bswap	r12
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD[8+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	r10,r14
	mov	r12,QWORD[16+rsi]
	mov	r13,rcx
	mov	r14,r10
	bswap	r12
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD[16+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	r9,r14
	mov	r12,QWORD[24+rsi]
	mov	r13,rbx
	mov	r14,r9
	bswap	r12
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD[24+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	r8,r14
	mov	r12,QWORD[32+rsi]
	mov	r13,rax
	mov	r14,r8
	bswap	r12
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD[32+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	rdx,r14
	mov	r12,QWORD[40+rsi]
	mov	r13,r11
	mov	r14,rdx
	bswap	r12
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD[40+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	rcx,r14
	mov	r12,QWORD[48+rsi]
	mov	r13,r10
	mov	r14,rcx
	bswap	r12
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD[48+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	rbx,r14
	mov	r12,QWORD[56+rsi]
	mov	r13,r9
	mov	r14,rbx
	bswap	r12
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD[56+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	rax,r14
	mov	r12,QWORD[64+rsi]
	mov	r13,r8
	mov	r14,rax
	bswap	r12
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD[64+rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	r11,r14
	mov	r12,QWORD[72+rsi]
	mov	r13,rdx
	mov	r14,r11
	bswap	r12
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD[72+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	r10,r14
	mov	r12,QWORD[80+rsi]
	mov	r13,rcx
	mov	r14,r10
	bswap	r12
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD[80+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	r9,r14
	mov	r12,QWORD[88+rsi]
	mov	r13,rbx
	mov	r14,r9
	bswap	r12
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD[88+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	r8,r14
	mov	r12,QWORD[96+rsi]
	mov	r13,rax
	mov	r14,r8
	bswap	r12
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD[96+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	rdx,r14
	mov	r12,QWORD[104+rsi]
	mov	r13,r11
	mov	r14,rdx
	bswap	r12
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD[104+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	add	rcx,r14
	mov	r12,QWORD[112+rsi]
	mov	r13,r10
	mov	r14,rcx
	bswap	r12
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD[112+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	add	rbx,r14
	mov	r12,QWORD[120+rsi]
	mov	r13,r9
	mov	r14,rbx
	bswap	r12
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD[120+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	jmp	NEAR $L$rounds_16_xx
ALIGN	16
$L$rounds_16_xx:
	mov	r13,QWORD[8+rsp]
	mov	r15,QWORD[112+rsp]

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
	add	r12,QWORD[72+rsp]

	add	r12,QWORD[rsp]
	mov	r13,r8
	add	r12,r15
	mov	r14,rax
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD[rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[16+rsp]
	mov	rdi,QWORD[120+rsp]

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
	add	r12,QWORD[80+rsp]

	add	r12,QWORD[8+rsp]
	mov	r13,rdx
	add	r12,rdi
	mov	r14,r11
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD[8+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[24+rsp]
	mov	r15,QWORD[rsp]

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
	add	r12,QWORD[88+rsp]

	add	r12,QWORD[16+rsp]
	mov	r13,rcx
	add	r12,r15
	mov	r14,r10
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD[16+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[32+rsp]
	mov	rdi,QWORD[8+rsp]

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
	add	r12,QWORD[96+rsp]

	add	r12,QWORD[24+rsp]
	mov	r13,rbx
	add	r12,rdi
	mov	r14,r9
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD[24+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[40+rsp]
	mov	r15,QWORD[16+rsp]

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
	add	r12,QWORD[104+rsp]

	add	r12,QWORD[32+rsp]
	mov	r13,rax
	add	r12,r15
	mov	r14,r8
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD[32+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[48+rsp]
	mov	rdi,QWORD[24+rsp]

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
	add	r12,QWORD[112+rsp]

	add	r12,QWORD[40+rsp]
	mov	r13,r11
	add	r12,rdi
	mov	r14,rdx
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD[40+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[56+rsp]
	mov	r15,QWORD[32+rsp]

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
	add	r12,QWORD[120+rsp]

	add	r12,QWORD[48+rsp]
	mov	r13,r10
	add	r12,r15
	mov	r14,rcx
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD[48+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[64+rsp]
	mov	rdi,QWORD[40+rsp]

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
	add	r12,QWORD[rsp]

	add	r12,QWORD[56+rsp]
	mov	r13,r9
	add	r12,rdi
	mov	r14,rbx
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD[56+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[72+rsp]
	mov	r15,QWORD[48+rsp]

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
	add	r12,QWORD[8+rsp]

	add	r12,QWORD[64+rsp]
	mov	r13,r8
	add	r12,r15
	mov	r14,rax
	ror	r13,23
	mov	r15,r9

	xor	r13,r8
	ror	r14,5
	xor	r15,r10

	mov	QWORD[64+rsp],r12
	xor	r14,rax
	and	r15,r8

	ror	r13,4
	add	r12,r11
	xor	r15,r10

	ror	r14,6
	xor	r13,r8
	add	r12,r15

	mov	r15,rax
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[80+rsp]
	mov	rdi,QWORD[56+rsp]

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
	add	r12,QWORD[16+rsp]

	add	r12,QWORD[72+rsp]
	mov	r13,rdx
	add	r12,rdi
	mov	r14,r11
	ror	r13,23
	mov	rdi,r8

	xor	r13,rdx
	ror	r14,5
	xor	rdi,r9

	mov	QWORD[72+rsp],r12
	xor	r14,r11
	and	rdi,rdx

	ror	r13,4
	add	r12,r10
	xor	rdi,r9

	ror	r14,6
	xor	r13,rdx
	add	r12,rdi

	mov	rdi,r11
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[88+rsp]
	mov	r15,QWORD[64+rsp]

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
	add	r12,QWORD[24+rsp]

	add	r12,QWORD[80+rsp]
	mov	r13,rcx
	add	r12,r15
	mov	r14,r10
	ror	r13,23
	mov	r15,rdx

	xor	r13,rcx
	ror	r14,5
	xor	r15,r8

	mov	QWORD[80+rsp],r12
	xor	r14,r10
	and	r15,rcx

	ror	r13,4
	add	r12,r9
	xor	r15,r8

	ror	r14,6
	xor	r13,rcx
	add	r12,r15

	mov	r15,r10
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[96+rsp]
	mov	rdi,QWORD[72+rsp]

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
	add	r12,QWORD[32+rsp]

	add	r12,QWORD[88+rsp]
	mov	r13,rbx
	add	r12,rdi
	mov	r14,r9
	ror	r13,23
	mov	rdi,rcx

	xor	r13,rbx
	ror	r14,5
	xor	rdi,rdx

	mov	QWORD[88+rsp],r12
	xor	r14,r9
	and	rdi,rbx

	ror	r13,4
	add	r12,r8
	xor	rdi,rdx

	ror	r14,6
	xor	r13,rbx
	add	r12,rdi

	mov	rdi,r9
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[104+rsp]
	mov	r15,QWORD[80+rsp]

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
	add	r12,QWORD[40+rsp]

	add	r12,QWORD[96+rsp]
	mov	r13,rax
	add	r12,r15
	mov	r14,r8
	ror	r13,23
	mov	r15,rbx

	xor	r13,rax
	ror	r14,5
	xor	r15,rcx

	mov	QWORD[96+rsp],r12
	xor	r14,r8
	and	r15,rax

	ror	r13,4
	add	r12,rdx
	xor	r15,rcx

	ror	r14,6
	xor	r13,rax
	add	r12,r15

	mov	r15,r8
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[112+rsp]
	mov	rdi,QWORD[88+rsp]

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
	add	r12,QWORD[48+rsp]

	add	r12,QWORD[104+rsp]
	mov	r13,r11
	add	r12,rdi
	mov	r14,rdx
	ror	r13,23
	mov	rdi,rax

	xor	r13,r11
	ror	r14,5
	xor	rdi,rbx

	mov	QWORD[104+rsp],r12
	xor	r14,rdx
	and	rdi,r11

	ror	r13,4
	add	r12,rcx
	xor	rdi,rbx

	ror	r14,6
	xor	r13,r11
	add	r12,rdi

	mov	rdi,rdx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	mov	r13,QWORD[120+rsp]
	mov	r15,QWORD[96+rsp]

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
	add	r12,QWORD[56+rsp]

	add	r12,QWORD[112+rsp]
	mov	r13,r10
	add	r12,r15
	mov	r14,rcx
	ror	r13,23
	mov	r15,r11

	xor	r13,r10
	ror	r14,5
	xor	r15,rax

	mov	QWORD[112+rsp],r12
	xor	r14,rcx
	and	r15,r10

	ror	r13,4
	add	r12,rbx
	xor	r15,rax

	ror	r14,6
	xor	r13,r10
	add	r12,r15

	mov	r15,rcx
	add	r12,QWORD[rbp]
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

	lea	rbp,[8+rbp]
	mov	r13,QWORD[rsp]
	mov	rdi,QWORD[104+rsp]

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
	add	r12,QWORD[64+rsp]

	add	r12,QWORD[120+rsp]
	mov	r13,r9
	add	r12,rdi
	mov	r14,rbx
	ror	r13,23
	mov	rdi,r10

	xor	r13,r9
	ror	r14,5
	xor	rdi,r11

	mov	QWORD[120+rsp],r12
	xor	r14,rbx
	and	rdi,r9

	ror	r13,4
	add	r12,rax
	xor	rdi,r11

	ror	r14,6
	xor	r13,r9
	add	r12,rdi

	mov	rdi,rbx
	add	r12,QWORD[rbp]
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

	lea	rbp,[24+rbp]
	cmp	BYTE[7+rbp],0
	jnz	NEAR $L$rounds_16_xx

	mov	rdi,QWORD[((128+0))+rsp]
	add	rax,r14
	lea	rsi,[128+rsi]

	add	rax,QWORD[rdi]
	add	rbx,QWORD[8+rdi]
	add	rcx,QWORD[16+rdi]
	add	rdx,QWORD[24+rdi]
	add	r8,QWORD[32+rdi]
	add	r9,QWORD[40+rdi]
	add	r10,QWORD[48+rdi]
	add	r11,QWORD[56+rdi]

	cmp	rsi,QWORD[((128+16))+rsp]

	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rcx
	mov	QWORD[24+rdi],rdx
	mov	QWORD[32+rdi],r8
	mov	QWORD[40+rdi],r9
	mov	QWORD[48+rdi],r10
	mov	QWORD[56+rdi],r11
	jb	NEAR $L$loop

	mov	rsi,QWORD[152+rsp]

	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha512_block_data_order:
ALIGN	64

K512:
	DQ	0x428a2f98d728ae22,0x7137449123ef65cd
	DQ	0x428a2f98d728ae22,0x7137449123ef65cd
	DQ	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	DQ	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	DQ	0x3956c25bf348b538,0x59f111f1b605d019
	DQ	0x3956c25bf348b538,0x59f111f1b605d019
	DQ	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	DQ	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	DQ	0xd807aa98a3030242,0x12835b0145706fbe
	DQ	0xd807aa98a3030242,0x12835b0145706fbe
	DQ	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	DQ	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	DQ	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	DQ	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	DQ	0x9bdc06a725c71235,0xc19bf174cf692694
	DQ	0x9bdc06a725c71235,0xc19bf174cf692694
	DQ	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	DQ	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	DQ	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	DQ	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	DQ	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	DQ	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	DQ	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	DQ	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	DQ	0x983e5152ee66dfab,0xa831c66d2db43210
	DQ	0x983e5152ee66dfab,0xa831c66d2db43210
	DQ	0xb00327c898fb213f,0xbf597fc7beef0ee4
	DQ	0xb00327c898fb213f,0xbf597fc7beef0ee4
	DQ	0xc6e00bf33da88fc2,0xd5a79147930aa725
	DQ	0xc6e00bf33da88fc2,0xd5a79147930aa725
	DQ	0x06ca6351e003826f,0x142929670a0e6e70
	DQ	0x06ca6351e003826f,0x142929670a0e6e70
	DQ	0x27b70a8546d22ffc,0x2e1b21385c26c926
	DQ	0x27b70a8546d22ffc,0x2e1b21385c26c926
	DQ	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	DQ	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	DQ	0x650a73548baf63de,0x766a0abb3c77b2a8
	DQ	0x650a73548baf63de,0x766a0abb3c77b2a8
	DQ	0x81c2c92e47edaee6,0x92722c851482353b
	DQ	0x81c2c92e47edaee6,0x92722c851482353b
	DQ	0xa2bfe8a14cf10364,0xa81a664bbc423001
	DQ	0xa2bfe8a14cf10364,0xa81a664bbc423001
	DQ	0xc24b8b70d0f89791,0xc76c51a30654be30
	DQ	0xc24b8b70d0f89791,0xc76c51a30654be30
	DQ	0xd192e819d6ef5218,0xd69906245565a910
	DQ	0xd192e819d6ef5218,0xd69906245565a910
	DQ	0xf40e35855771202a,0x106aa07032bbd1b8
	DQ	0xf40e35855771202a,0x106aa07032bbd1b8
	DQ	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	DQ	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	DQ	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	DQ	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	DQ	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	DQ	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	DQ	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	DQ	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	DQ	0x748f82ee5defb2fc,0x78a5636f43172f60
	DQ	0x748f82ee5defb2fc,0x78a5636f43172f60
	DQ	0x84c87814a1f0ab72,0x8cc702081a6439ec
	DQ	0x84c87814a1f0ab72,0x8cc702081a6439ec
	DQ	0x90befffa23631e28,0xa4506cebde82bde9
	DQ	0x90befffa23631e28,0xa4506cebde82bde9
	DQ	0xbef9a3f7b2c67915,0xc67178f2e372532b
	DQ	0xbef9a3f7b2c67915,0xc67178f2e372532b
	DQ	0xca273eceea26619c,0xd186b8c721c0c207
	DQ	0xca273eceea26619c,0xd186b8c721c0c207
	DQ	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	DQ	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	DQ	0x06f067aa72176fba,0x0a637dc5a2c898a6
	DQ	0x06f067aa72176fba,0x0a637dc5a2c898a6
	DQ	0x113f9804bef90dae,0x1b710b35131c471b
	DQ	0x113f9804bef90dae,0x1b710b35131c471b
	DQ	0x28db77f523047d84,0x32caab7b40c72493
	DQ	0x28db77f523047d84,0x32caab7b40c72493
	DQ	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	DQ	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	DQ	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	DQ	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	DQ	0x5fcb6fab3ad6faec,0x6c44198c4a475817
	DQ	0x5fcb6fab3ad6faec,0x6c44198c4a475817

	DQ	0x0001020304050607,0x08090a0b0c0d0e0f
	DQ	0x0001020304050607,0x08090a0b0c0d0e0f
DB	83,72,65,53,49,50,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,95,54
DB	52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121
DB	32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46
DB	111,114,103,62,0

ALIGN	64
sha512_block_data_order_xop:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha512_block_data_order_xop:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



$L$xop_shortcut:
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

	shl	rdx,4
	sub	rsp,256
	lea	rdx,[rdx*8+rsi]
	and	rsp,-64
	mov	QWORD[((128+0))+rsp],rdi
	mov	QWORD[((128+8))+rsp],rsi
	mov	QWORD[((128+16))+rsp],rdx
	mov	QWORD[152+rsp],rax

	movaps	XMMWORD[(128+32)+rsp],xmm6
	movaps	XMMWORD[(128+48)+rsp],xmm7
	movaps	XMMWORD[(128+64)+rsp],xmm8
	movaps	XMMWORD[(128+80)+rsp],xmm9
	movaps	XMMWORD[(128+96)+rsp],xmm10
	movaps	XMMWORD[(128+112)+rsp],xmm11
$L$prologue_xop:

	vzeroupper
	mov	rax,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	rcx,QWORD[16+rdi]
	mov	rdx,QWORD[24+rdi]
	mov	r8,QWORD[32+rdi]
	mov	r9,QWORD[40+rdi]
	mov	r10,QWORD[48+rdi]
	mov	r11,QWORD[56+rdi]
	jmp	NEAR $L$loop_xop
ALIGN	16
$L$loop_xop:
	vmovdqa	xmm11,XMMWORD[((K512+1280))]
	vmovdqu	xmm0,XMMWORD[rsi]
	lea	rbp,[((K512+128))]
	vmovdqu	xmm1,XMMWORD[16+rsi]
	vmovdqu	xmm2,XMMWORD[32+rsi]
	vpshufb	xmm0,xmm0,xmm11
	vmovdqu	xmm3,XMMWORD[48+rsi]
	vpshufb	xmm1,xmm1,xmm11
	vmovdqu	xmm4,XMMWORD[64+rsi]
	vpshufb	xmm2,xmm2,xmm11
	vmovdqu	xmm5,XMMWORD[80+rsi]
	vpshufb	xmm3,xmm3,xmm11
	vmovdqu	xmm6,XMMWORD[96+rsi]
	vpshufb	xmm4,xmm4,xmm11
	vmovdqu	xmm7,XMMWORD[112+rsi]
	vpshufb	xmm5,xmm5,xmm11
	vpaddq	xmm8,xmm0,XMMWORD[((-128))+rbp]
	vpshufb	xmm6,xmm6,xmm11
	vpaddq	xmm9,xmm1,XMMWORD[((-96))+rbp]
	vpshufb	xmm7,xmm7,xmm11
	vpaddq	xmm10,xmm2,XMMWORD[((-64))+rbp]
	vpaddq	xmm11,xmm3,XMMWORD[((-32))+rbp]
	vmovdqa	XMMWORD[rsp],xmm8
	vpaddq	xmm8,xmm4,XMMWORD[rbp]
	vmovdqa	XMMWORD[16+rsp],xmm9
	vpaddq	xmm9,xmm5,XMMWORD[32+rbp]
	vmovdqa	XMMWORD[32+rsp],xmm10
	vpaddq	xmm10,xmm6,XMMWORD[64+rbp]
	vmovdqa	XMMWORD[48+rsp],xmm11
	vpaddq	xmm11,xmm7,XMMWORD[96+rbp]
	vmovdqa	XMMWORD[64+rsp],xmm8
	mov	r14,rax
	vmovdqa	XMMWORD[80+rsp],xmm9
	mov	rdi,rbx
	vmovdqa	XMMWORD[96+rsp],xmm10
	xor	rdi,rcx
	vmovdqa	XMMWORD[112+rsp],xmm11
	mov	r13,r8
	jmp	NEAR $L$xop_00_47

ALIGN	16
$L$xop_00_47:
	add	rbp,256
	vpalignr	xmm8,xmm1,xmm0,8
	ror	r13,23
	mov	rax,r14
	vpalignr	xmm11,xmm5,xmm4,8
	mov	r12,r9
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,r8
	xor	r12,r10
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,rax
	vpaddq	xmm0,xmm0,xmm11
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[rsp]
	mov	r15,rax
DB	143,72,120,195,209,7
	xor	r12,r10
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,rbx
	add	r11,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,223,3
	xor	r14,rax
	add	r11,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rbx
	ror	r14,28
	vpsrlq	xmm10,xmm7,6
	add	rdx,r11
	add	r11,rdi
	vpaddq	xmm0,xmm0,xmm8
	mov	r13,rdx
	add	r14,r11
DB	143,72,120,195,203,42
	ror	r13,23
	mov	r11,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,r8
	ror	r14,5
	xor	r13,rdx
	xor	r12,r9
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	vpaddq	xmm0,xmm0,xmm11
	add	r10,QWORD[8+rsp]
	mov	rdi,r11
	xor	r12,r9
	ror	r14,6
	vpaddq	xmm10,xmm0,XMMWORD[((-128))+rbp]
	xor	rdi,rax
	add	r10,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	ror	r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	vmovdqa	XMMWORD[rsp],xmm10
	vpalignr	xmm8,xmm2,xmm1,8
	ror	r13,23
	mov	r10,r14
	vpalignr	xmm11,xmm6,xmm5,8
	mov	r12,rdx
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,rcx
	xor	r12,r8
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,r10
	vpaddq	xmm1,xmm1,xmm11
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[16+rsp]
	mov	r15,r10
DB	143,72,120,195,209,7
	xor	r12,r8
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,r11
	add	r9,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,216,3
	xor	r14,r10
	add	r9,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r11
	ror	r14,28
	vpsrlq	xmm10,xmm0,6
	add	rbx,r9
	add	r9,rdi
	vpaddq	xmm1,xmm1,xmm8
	mov	r13,rbx
	add	r14,r9
DB	143,72,120,195,203,42
	ror	r13,23
	mov	r9,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,rcx
	ror	r14,5
	xor	r13,rbx
	xor	r12,rdx
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	vpaddq	xmm1,xmm1,xmm11
	add	r8,QWORD[24+rsp]
	mov	rdi,r9
	xor	r12,rdx
	ror	r14,6
	vpaddq	xmm10,xmm1,XMMWORD[((-96))+rbp]
	xor	rdi,r10
	add	r8,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	ror	r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	vmovdqa	XMMWORD[16+rsp],xmm10
	vpalignr	xmm8,xmm3,xmm2,8
	ror	r13,23
	mov	r8,r14
	vpalignr	xmm11,xmm7,xmm6,8
	mov	r12,rbx
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,rax
	xor	r12,rcx
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,r8
	vpaddq	xmm2,xmm2,xmm11
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[32+rsp]
	mov	r15,r8
DB	143,72,120,195,209,7
	xor	r12,rcx
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,r9
	add	rdx,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,217,3
	xor	r14,r8
	add	rdx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r9
	ror	r14,28
	vpsrlq	xmm10,xmm1,6
	add	r11,rdx
	add	rdx,rdi
	vpaddq	xmm2,xmm2,xmm8
	mov	r13,r11
	add	r14,rdx
DB	143,72,120,195,203,42
	ror	r13,23
	mov	rdx,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,rax
	ror	r14,5
	xor	r13,r11
	xor	r12,rbx
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	vpaddq	xmm2,xmm2,xmm11
	add	rcx,QWORD[40+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	ror	r14,6
	vpaddq	xmm10,xmm2,XMMWORD[((-64))+rbp]
	xor	rdi,r8
	add	rcx,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	ror	r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	vmovdqa	XMMWORD[32+rsp],xmm10
	vpalignr	xmm8,xmm4,xmm3,8
	ror	r13,23
	mov	rcx,r14
	vpalignr	xmm11,xmm0,xmm7,8
	mov	r12,r11
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,r10
	xor	r12,rax
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,rcx
	vpaddq	xmm3,xmm3,xmm11
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[48+rsp]
	mov	r15,rcx
DB	143,72,120,195,209,7
	xor	r12,rax
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,rdx
	add	rbx,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,218,3
	xor	r14,rcx
	add	rbx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rdx
	ror	r14,28
	vpsrlq	xmm10,xmm2,6
	add	r9,rbx
	add	rbx,rdi
	vpaddq	xmm3,xmm3,xmm8
	mov	r13,r9
	add	r14,rbx
DB	143,72,120,195,203,42
	ror	r13,23
	mov	rbx,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,r10
	ror	r14,5
	xor	r13,r9
	xor	r12,r11
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	vpaddq	xmm3,xmm3,xmm11
	add	rax,QWORD[56+rsp]
	mov	rdi,rbx
	xor	r12,r11
	ror	r14,6
	vpaddq	xmm10,xmm3,XMMWORD[((-32))+rbp]
	xor	rdi,rcx
	add	rax,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	ror	r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	vmovdqa	XMMWORD[48+rsp],xmm10
	vpalignr	xmm8,xmm5,xmm4,8
	ror	r13,23
	mov	rax,r14
	vpalignr	xmm11,xmm1,xmm0,8
	mov	r12,r9
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,r8
	xor	r12,r10
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,rax
	vpaddq	xmm4,xmm4,xmm11
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[64+rsp]
	mov	r15,rax
DB	143,72,120,195,209,7
	xor	r12,r10
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,rbx
	add	r11,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,219,3
	xor	r14,rax
	add	r11,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rbx
	ror	r14,28
	vpsrlq	xmm10,xmm3,6
	add	rdx,r11
	add	r11,rdi
	vpaddq	xmm4,xmm4,xmm8
	mov	r13,rdx
	add	r14,r11
DB	143,72,120,195,203,42
	ror	r13,23
	mov	r11,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,r8
	ror	r14,5
	xor	r13,rdx
	xor	r12,r9
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	vpaddq	xmm4,xmm4,xmm11
	add	r10,QWORD[72+rsp]
	mov	rdi,r11
	xor	r12,r9
	ror	r14,6
	vpaddq	xmm10,xmm4,XMMWORD[rbp]
	xor	rdi,rax
	add	r10,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	ror	r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	vmovdqa	XMMWORD[64+rsp],xmm10
	vpalignr	xmm8,xmm6,xmm5,8
	ror	r13,23
	mov	r10,r14
	vpalignr	xmm11,xmm2,xmm1,8
	mov	r12,rdx
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,rcx
	xor	r12,r8
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,r10
	vpaddq	xmm5,xmm5,xmm11
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[80+rsp]
	mov	r15,r10
DB	143,72,120,195,209,7
	xor	r12,r8
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,r11
	add	r9,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,220,3
	xor	r14,r10
	add	r9,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r11
	ror	r14,28
	vpsrlq	xmm10,xmm4,6
	add	rbx,r9
	add	r9,rdi
	vpaddq	xmm5,xmm5,xmm8
	mov	r13,rbx
	add	r14,r9
DB	143,72,120,195,203,42
	ror	r13,23
	mov	r9,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,rcx
	ror	r14,5
	xor	r13,rbx
	xor	r12,rdx
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	vpaddq	xmm5,xmm5,xmm11
	add	r8,QWORD[88+rsp]
	mov	rdi,r9
	xor	r12,rdx
	ror	r14,6
	vpaddq	xmm10,xmm5,XMMWORD[32+rbp]
	xor	rdi,r10
	add	r8,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	ror	r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	vmovdqa	XMMWORD[80+rsp],xmm10
	vpalignr	xmm8,xmm7,xmm6,8
	ror	r13,23
	mov	r8,r14
	vpalignr	xmm11,xmm3,xmm2,8
	mov	r12,rbx
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,rax
	xor	r12,rcx
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,r8
	vpaddq	xmm6,xmm6,xmm11
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[96+rsp]
	mov	r15,r8
DB	143,72,120,195,209,7
	xor	r12,rcx
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,r9
	add	rdx,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,221,3
	xor	r14,r8
	add	rdx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r9
	ror	r14,28
	vpsrlq	xmm10,xmm5,6
	add	r11,rdx
	add	rdx,rdi
	vpaddq	xmm6,xmm6,xmm8
	mov	r13,r11
	add	r14,rdx
DB	143,72,120,195,203,42
	ror	r13,23
	mov	rdx,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,rax
	ror	r14,5
	xor	r13,r11
	xor	r12,rbx
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	vpaddq	xmm6,xmm6,xmm11
	add	rcx,QWORD[104+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	ror	r14,6
	vpaddq	xmm10,xmm6,XMMWORD[64+rbp]
	xor	rdi,r8
	add	rcx,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	ror	r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	vmovdqa	XMMWORD[96+rsp],xmm10
	vpalignr	xmm8,xmm0,xmm7,8
	ror	r13,23
	mov	rcx,r14
	vpalignr	xmm11,xmm4,xmm3,8
	mov	r12,r11
	ror	r14,5
DB	143,72,120,195,200,56
	xor	r13,r10
	xor	r12,rax
	vpsrlq	xmm8,xmm8,7
	ror	r13,4
	xor	r14,rcx
	vpaddq	xmm7,xmm7,xmm11
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[112+rsp]
	mov	r15,rcx
DB	143,72,120,195,209,7
	xor	r12,rax
	ror	r14,6
	vpxor	xmm8,xmm8,xmm9
	xor	r15,rdx
	add	rbx,r12
	ror	r13,14
	and	rdi,r15
DB	143,104,120,195,222,3
	xor	r14,rcx
	add	rbx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rdx
	ror	r14,28
	vpsrlq	xmm10,xmm6,6
	add	r9,rbx
	add	rbx,rdi
	vpaddq	xmm7,xmm7,xmm8
	mov	r13,r9
	add	r14,rbx
DB	143,72,120,195,203,42
	ror	r13,23
	mov	rbx,r14
	vpxor	xmm11,xmm11,xmm10
	mov	r12,r10
	ror	r14,5
	xor	r13,r9
	xor	r12,r11
	vpxor	xmm11,xmm11,xmm9
	ror	r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	vpaddq	xmm7,xmm7,xmm11
	add	rax,QWORD[120+rsp]
	mov	rdi,rbx
	xor	r12,r11
	ror	r14,6
	vpaddq	xmm10,xmm7,XMMWORD[96+rbp]
	xor	rdi,rcx
	add	rax,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	ror	r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	vmovdqa	XMMWORD[112+rsp],xmm10
	cmp	BYTE[135+rbp],0
	jne	NEAR $L$xop_00_47
	ror	r13,23
	mov	rax,r14
	mov	r12,r9
	ror	r14,5
	xor	r13,r8
	xor	r12,r10
	ror	r13,4
	xor	r14,rax
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[rsp]
	mov	r15,rax
	xor	r12,r10
	ror	r14,6
	xor	r15,rbx
	add	r11,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,rax
	add	r11,r13
	xor	rdi,rbx
	ror	r14,28
	add	rdx,r11
	add	r11,rdi
	mov	r13,rdx
	add	r14,r11
	ror	r13,23
	mov	r11,r14
	mov	r12,r8
	ror	r14,5
	xor	r13,rdx
	xor	r12,r9
	ror	r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	add	r10,QWORD[8+rsp]
	mov	rdi,r11
	xor	r12,r9
	ror	r14,6
	xor	rdi,rax
	add	r10,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	ror	r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	ror	r13,23
	mov	r10,r14
	mov	r12,rdx
	ror	r14,5
	xor	r13,rcx
	xor	r12,r8
	ror	r13,4
	xor	r14,r10
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[16+rsp]
	mov	r15,r10
	xor	r12,r8
	ror	r14,6
	xor	r15,r11
	add	r9,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,r10
	add	r9,r13
	xor	rdi,r11
	ror	r14,28
	add	rbx,r9
	add	r9,rdi
	mov	r13,rbx
	add	r14,r9
	ror	r13,23
	mov	r9,r14
	mov	r12,rcx
	ror	r14,5
	xor	r13,rbx
	xor	r12,rdx
	ror	r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	add	r8,QWORD[24+rsp]
	mov	rdi,r9
	xor	r12,rdx
	ror	r14,6
	xor	rdi,r10
	add	r8,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	ror	r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	ror	r13,23
	mov	r8,r14
	mov	r12,rbx
	ror	r14,5
	xor	r13,rax
	xor	r12,rcx
	ror	r13,4
	xor	r14,r8
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[32+rsp]
	mov	r15,r8
	xor	r12,rcx
	ror	r14,6
	xor	r15,r9
	add	rdx,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,r8
	add	rdx,r13
	xor	rdi,r9
	ror	r14,28
	add	r11,rdx
	add	rdx,rdi
	mov	r13,r11
	add	r14,rdx
	ror	r13,23
	mov	rdx,r14
	mov	r12,rax
	ror	r14,5
	xor	r13,r11
	xor	r12,rbx
	ror	r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	add	rcx,QWORD[40+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	ror	r14,6
	xor	rdi,r8
	add	rcx,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	ror	r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	ror	r13,23
	mov	rcx,r14
	mov	r12,r11
	ror	r14,5
	xor	r13,r10
	xor	r12,rax
	ror	r13,4
	xor	r14,rcx
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[48+rsp]
	mov	r15,rcx
	xor	r12,rax
	ror	r14,6
	xor	r15,rdx
	add	rbx,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,rcx
	add	rbx,r13
	xor	rdi,rdx
	ror	r14,28
	add	r9,rbx
	add	rbx,rdi
	mov	r13,r9
	add	r14,rbx
	ror	r13,23
	mov	rbx,r14
	mov	r12,r10
	ror	r14,5
	xor	r13,r9
	xor	r12,r11
	ror	r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	add	rax,QWORD[56+rsp]
	mov	rdi,rbx
	xor	r12,r11
	ror	r14,6
	xor	rdi,rcx
	add	rax,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	ror	r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	ror	r13,23
	mov	rax,r14
	mov	r12,r9
	ror	r14,5
	xor	r13,r8
	xor	r12,r10
	ror	r13,4
	xor	r14,rax
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[64+rsp]
	mov	r15,rax
	xor	r12,r10
	ror	r14,6
	xor	r15,rbx
	add	r11,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,rax
	add	r11,r13
	xor	rdi,rbx
	ror	r14,28
	add	rdx,r11
	add	r11,rdi
	mov	r13,rdx
	add	r14,r11
	ror	r13,23
	mov	r11,r14
	mov	r12,r8
	ror	r14,5
	xor	r13,rdx
	xor	r12,r9
	ror	r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	add	r10,QWORD[72+rsp]
	mov	rdi,r11
	xor	r12,r9
	ror	r14,6
	xor	rdi,rax
	add	r10,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	ror	r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	ror	r13,23
	mov	r10,r14
	mov	r12,rdx
	ror	r14,5
	xor	r13,rcx
	xor	r12,r8
	ror	r13,4
	xor	r14,r10
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[80+rsp]
	mov	r15,r10
	xor	r12,r8
	ror	r14,6
	xor	r15,r11
	add	r9,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,r10
	add	r9,r13
	xor	rdi,r11
	ror	r14,28
	add	rbx,r9
	add	r9,rdi
	mov	r13,rbx
	add	r14,r9
	ror	r13,23
	mov	r9,r14
	mov	r12,rcx
	ror	r14,5
	xor	r13,rbx
	xor	r12,rdx
	ror	r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	add	r8,QWORD[88+rsp]
	mov	rdi,r9
	xor	r12,rdx
	ror	r14,6
	xor	rdi,r10
	add	r8,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	ror	r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	ror	r13,23
	mov	r8,r14
	mov	r12,rbx
	ror	r14,5
	xor	r13,rax
	xor	r12,rcx
	ror	r13,4
	xor	r14,r8
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[96+rsp]
	mov	r15,r8
	xor	r12,rcx
	ror	r14,6
	xor	r15,r9
	add	rdx,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,r8
	add	rdx,r13
	xor	rdi,r9
	ror	r14,28
	add	r11,rdx
	add	rdx,rdi
	mov	r13,r11
	add	r14,rdx
	ror	r13,23
	mov	rdx,r14
	mov	r12,rax
	ror	r14,5
	xor	r13,r11
	xor	r12,rbx
	ror	r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	add	rcx,QWORD[104+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	ror	r14,6
	xor	rdi,r8
	add	rcx,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	ror	r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	ror	r13,23
	mov	rcx,r14
	mov	r12,r11
	ror	r14,5
	xor	r13,r10
	xor	r12,rax
	ror	r13,4
	xor	r14,rcx
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[112+rsp]
	mov	r15,rcx
	xor	r12,rax
	ror	r14,6
	xor	r15,rdx
	add	rbx,r12
	ror	r13,14
	and	rdi,r15
	xor	r14,rcx
	add	rbx,r13
	xor	rdi,rdx
	ror	r14,28
	add	r9,rbx
	add	rbx,rdi
	mov	r13,r9
	add	r14,rbx
	ror	r13,23
	mov	rbx,r14
	mov	r12,r10
	ror	r14,5
	xor	r13,r9
	xor	r12,r11
	ror	r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	add	rax,QWORD[120+rsp]
	mov	rdi,rbx
	xor	r12,r11
	ror	r14,6
	xor	rdi,rcx
	add	rax,r12
	ror	r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	ror	r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	mov	rdi,QWORD[((128+0))+rsp]
	mov	rax,r14

	add	rax,QWORD[rdi]
	lea	rsi,[128+rsi]
	add	rbx,QWORD[8+rdi]
	add	rcx,QWORD[16+rdi]
	add	rdx,QWORD[24+rdi]
	add	r8,QWORD[32+rdi]
	add	r9,QWORD[40+rdi]
	add	r10,QWORD[48+rdi]
	add	r11,QWORD[56+rdi]

	cmp	rsi,QWORD[((128+16))+rsp]

	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rcx
	mov	QWORD[24+rdi],rdx
	mov	QWORD[32+rdi],r8
	mov	QWORD[40+rdi],r9
	mov	QWORD[48+rdi],r10
	mov	QWORD[56+rdi],r11
	jb	NEAR $L$loop_xop

	mov	rsi,QWORD[152+rsp]

	vzeroupper
	movaps	xmm6,XMMWORD[((128+32))+rsp]
	movaps	xmm7,XMMWORD[((128+48))+rsp]
	movaps	xmm8,XMMWORD[((128+64))+rsp]
	movaps	xmm9,XMMWORD[((128+80))+rsp]
	movaps	xmm10,XMMWORD[((128+96))+rsp]
	movaps	xmm11,XMMWORD[((128+112))+rsp]
	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$epilogue_xop:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha512_block_data_order_xop:

ALIGN	64
sha512_block_data_order_avx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha512_block_data_order_avx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



$L$avx_shortcut:
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

	shl	rdx,4
	sub	rsp,256
	lea	rdx,[rdx*8+rsi]
	and	rsp,-64
	mov	QWORD[((128+0))+rsp],rdi
	mov	QWORD[((128+8))+rsp],rsi
	mov	QWORD[((128+16))+rsp],rdx
	mov	QWORD[152+rsp],rax

	movaps	XMMWORD[(128+32)+rsp],xmm6
	movaps	XMMWORD[(128+48)+rsp],xmm7
	movaps	XMMWORD[(128+64)+rsp],xmm8
	movaps	XMMWORD[(128+80)+rsp],xmm9
	movaps	XMMWORD[(128+96)+rsp],xmm10
	movaps	XMMWORD[(128+112)+rsp],xmm11
$L$prologue_avx:

	vzeroupper
	mov	rax,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	rcx,QWORD[16+rdi]
	mov	rdx,QWORD[24+rdi]
	mov	r8,QWORD[32+rdi]
	mov	r9,QWORD[40+rdi]
	mov	r10,QWORD[48+rdi]
	mov	r11,QWORD[56+rdi]
	jmp	NEAR $L$loop_avx
ALIGN	16
$L$loop_avx:
	vmovdqa	xmm11,XMMWORD[((K512+1280))]
	vmovdqu	xmm0,XMMWORD[rsi]
	lea	rbp,[((K512+128))]
	vmovdqu	xmm1,XMMWORD[16+rsi]
	vmovdqu	xmm2,XMMWORD[32+rsi]
	vpshufb	xmm0,xmm0,xmm11
	vmovdqu	xmm3,XMMWORD[48+rsi]
	vpshufb	xmm1,xmm1,xmm11
	vmovdqu	xmm4,XMMWORD[64+rsi]
	vpshufb	xmm2,xmm2,xmm11
	vmovdqu	xmm5,XMMWORD[80+rsi]
	vpshufb	xmm3,xmm3,xmm11
	vmovdqu	xmm6,XMMWORD[96+rsi]
	vpshufb	xmm4,xmm4,xmm11
	vmovdqu	xmm7,XMMWORD[112+rsi]
	vpshufb	xmm5,xmm5,xmm11
	vpaddq	xmm8,xmm0,XMMWORD[((-128))+rbp]
	vpshufb	xmm6,xmm6,xmm11
	vpaddq	xmm9,xmm1,XMMWORD[((-96))+rbp]
	vpshufb	xmm7,xmm7,xmm11
	vpaddq	xmm10,xmm2,XMMWORD[((-64))+rbp]
	vpaddq	xmm11,xmm3,XMMWORD[((-32))+rbp]
	vmovdqa	XMMWORD[rsp],xmm8
	vpaddq	xmm8,xmm4,XMMWORD[rbp]
	vmovdqa	XMMWORD[16+rsp],xmm9
	vpaddq	xmm9,xmm5,XMMWORD[32+rbp]
	vmovdqa	XMMWORD[32+rsp],xmm10
	vpaddq	xmm10,xmm6,XMMWORD[64+rbp]
	vmovdqa	XMMWORD[48+rsp],xmm11
	vpaddq	xmm11,xmm7,XMMWORD[96+rbp]
	vmovdqa	XMMWORD[64+rsp],xmm8
	mov	r14,rax
	vmovdqa	XMMWORD[80+rsp],xmm9
	mov	rdi,rbx
	vmovdqa	XMMWORD[96+rsp],xmm10
	xor	rdi,rcx
	vmovdqa	XMMWORD[112+rsp],xmm11
	mov	r13,r8
	jmp	NEAR $L$avx_00_47

ALIGN	16
$L$avx_00_47:
	add	rbp,256
	vpalignr	xmm8,xmm1,xmm0,8
	shrd	r13,r13,23
	mov	rax,r14
	vpalignr	xmm11,xmm5,xmm4,8
	mov	r12,r9
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,r8
	xor	r12,r10
	vpaddq	xmm0,xmm0,xmm11
	shrd	r13,r13,4
	xor	r14,rax
	vpsrlq	xmm11,xmm8,7
	and	r12,r8
	xor	r13,r8
	vpsllq	xmm9,xmm8,56
	add	r11,QWORD[rsp]
	mov	r15,rax
	vpxor	xmm8,xmm11,xmm10
	xor	r12,r10
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,rbx
	add	r11,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,rax
	add	r11,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rbx
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm7,6
	add	rdx,r11
	add	r11,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,rdx
	add	r14,r11
	vpsllq	xmm10,xmm7,3
	shrd	r13,r13,23
	mov	r11,r14
	vpaddq	xmm0,xmm0,xmm8
	mov	r12,r8
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm7,19
	xor	r13,rdx
	xor	r12,r9
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,r11
	vpsllq	xmm10,xmm10,42
	and	r12,rdx
	xor	r13,rdx
	vpxor	xmm11,xmm11,xmm9
	add	r10,QWORD[8+rsp]
	mov	rdi,r11
	vpsrlq	xmm9,xmm9,42
	xor	r12,r9
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,rax
	add	r10,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm0,xmm0,xmm11
	xor	r14,r11
	add	r10,r13
	vpaddq	xmm10,xmm0,XMMWORD[((-128))+rbp]
	xor	r15,rax
	shrd	r14,r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	vmovdqa	XMMWORD[rsp],xmm10
	vpalignr	xmm8,xmm2,xmm1,8
	shrd	r13,r13,23
	mov	r10,r14
	vpalignr	xmm11,xmm6,xmm5,8
	mov	r12,rdx
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,rcx
	xor	r12,r8
	vpaddq	xmm1,xmm1,xmm11
	shrd	r13,r13,4
	xor	r14,r10
	vpsrlq	xmm11,xmm8,7
	and	r12,rcx
	xor	r13,rcx
	vpsllq	xmm9,xmm8,56
	add	r9,QWORD[16+rsp]
	mov	r15,r10
	vpxor	xmm8,xmm11,xmm10
	xor	r12,r8
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,r11
	add	r9,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,r10
	add	r9,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r11
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm0,6
	add	rbx,r9
	add	r9,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,rbx
	add	r14,r9
	vpsllq	xmm10,xmm0,3
	shrd	r13,r13,23
	mov	r9,r14
	vpaddq	xmm1,xmm1,xmm8
	mov	r12,rcx
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm0,19
	xor	r13,rbx
	xor	r12,rdx
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,r9
	vpsllq	xmm10,xmm10,42
	and	r12,rbx
	xor	r13,rbx
	vpxor	xmm11,xmm11,xmm9
	add	r8,QWORD[24+rsp]
	mov	rdi,r9
	vpsrlq	xmm9,xmm9,42
	xor	r12,rdx
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,r10
	add	r8,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm1,xmm1,xmm11
	xor	r14,r9
	add	r8,r13
	vpaddq	xmm10,xmm1,XMMWORD[((-96))+rbp]
	xor	r15,r10
	shrd	r14,r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	vmovdqa	XMMWORD[16+rsp],xmm10
	vpalignr	xmm8,xmm3,xmm2,8
	shrd	r13,r13,23
	mov	r8,r14
	vpalignr	xmm11,xmm7,xmm6,8
	mov	r12,rbx
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,rax
	xor	r12,rcx
	vpaddq	xmm2,xmm2,xmm11
	shrd	r13,r13,4
	xor	r14,r8
	vpsrlq	xmm11,xmm8,7
	and	r12,rax
	xor	r13,rax
	vpsllq	xmm9,xmm8,56
	add	rdx,QWORD[32+rsp]
	mov	r15,r8
	vpxor	xmm8,xmm11,xmm10
	xor	r12,rcx
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,r9
	add	rdx,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,r8
	add	rdx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r9
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm1,6
	add	r11,rdx
	add	rdx,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,r11
	add	r14,rdx
	vpsllq	xmm10,xmm1,3
	shrd	r13,r13,23
	mov	rdx,r14
	vpaddq	xmm2,xmm2,xmm8
	mov	r12,rax
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm1,19
	xor	r13,r11
	xor	r12,rbx
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,rdx
	vpsllq	xmm10,xmm10,42
	and	r12,r11
	xor	r13,r11
	vpxor	xmm11,xmm11,xmm9
	add	rcx,QWORD[40+rsp]
	mov	rdi,rdx
	vpsrlq	xmm9,xmm9,42
	xor	r12,rbx
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,r8
	add	rcx,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm2,xmm2,xmm11
	xor	r14,rdx
	add	rcx,r13
	vpaddq	xmm10,xmm2,XMMWORD[((-64))+rbp]
	xor	r15,r8
	shrd	r14,r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	vmovdqa	XMMWORD[32+rsp],xmm10
	vpalignr	xmm8,xmm4,xmm3,8
	shrd	r13,r13,23
	mov	rcx,r14
	vpalignr	xmm11,xmm0,xmm7,8
	mov	r12,r11
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,r10
	xor	r12,rax
	vpaddq	xmm3,xmm3,xmm11
	shrd	r13,r13,4
	xor	r14,rcx
	vpsrlq	xmm11,xmm8,7
	and	r12,r10
	xor	r13,r10
	vpsllq	xmm9,xmm8,56
	add	rbx,QWORD[48+rsp]
	mov	r15,rcx
	vpxor	xmm8,xmm11,xmm10
	xor	r12,rax
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,rdx
	add	rbx,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,rcx
	add	rbx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rdx
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm2,6
	add	r9,rbx
	add	rbx,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,r9
	add	r14,rbx
	vpsllq	xmm10,xmm2,3
	shrd	r13,r13,23
	mov	rbx,r14
	vpaddq	xmm3,xmm3,xmm8
	mov	r12,r10
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm2,19
	xor	r13,r9
	xor	r12,r11
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,rbx
	vpsllq	xmm10,xmm10,42
	and	r12,r9
	xor	r13,r9
	vpxor	xmm11,xmm11,xmm9
	add	rax,QWORD[56+rsp]
	mov	rdi,rbx
	vpsrlq	xmm9,xmm9,42
	xor	r12,r11
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,rcx
	add	rax,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm3,xmm3,xmm11
	xor	r14,rbx
	add	rax,r13
	vpaddq	xmm10,xmm3,XMMWORD[((-32))+rbp]
	xor	r15,rcx
	shrd	r14,r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	vmovdqa	XMMWORD[48+rsp],xmm10
	vpalignr	xmm8,xmm5,xmm4,8
	shrd	r13,r13,23
	mov	rax,r14
	vpalignr	xmm11,xmm1,xmm0,8
	mov	r12,r9
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,r8
	xor	r12,r10
	vpaddq	xmm4,xmm4,xmm11
	shrd	r13,r13,4
	xor	r14,rax
	vpsrlq	xmm11,xmm8,7
	and	r12,r8
	xor	r13,r8
	vpsllq	xmm9,xmm8,56
	add	r11,QWORD[64+rsp]
	mov	r15,rax
	vpxor	xmm8,xmm11,xmm10
	xor	r12,r10
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,rbx
	add	r11,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,rax
	add	r11,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rbx
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm3,6
	add	rdx,r11
	add	r11,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,rdx
	add	r14,r11
	vpsllq	xmm10,xmm3,3
	shrd	r13,r13,23
	mov	r11,r14
	vpaddq	xmm4,xmm4,xmm8
	mov	r12,r8
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm3,19
	xor	r13,rdx
	xor	r12,r9
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,r11
	vpsllq	xmm10,xmm10,42
	and	r12,rdx
	xor	r13,rdx
	vpxor	xmm11,xmm11,xmm9
	add	r10,QWORD[72+rsp]
	mov	rdi,r11
	vpsrlq	xmm9,xmm9,42
	xor	r12,r9
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,rax
	add	r10,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm4,xmm4,xmm11
	xor	r14,r11
	add	r10,r13
	vpaddq	xmm10,xmm4,XMMWORD[rbp]
	xor	r15,rax
	shrd	r14,r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	vmovdqa	XMMWORD[64+rsp],xmm10
	vpalignr	xmm8,xmm6,xmm5,8
	shrd	r13,r13,23
	mov	r10,r14
	vpalignr	xmm11,xmm2,xmm1,8
	mov	r12,rdx
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,rcx
	xor	r12,r8
	vpaddq	xmm5,xmm5,xmm11
	shrd	r13,r13,4
	xor	r14,r10
	vpsrlq	xmm11,xmm8,7
	and	r12,rcx
	xor	r13,rcx
	vpsllq	xmm9,xmm8,56
	add	r9,QWORD[80+rsp]
	mov	r15,r10
	vpxor	xmm8,xmm11,xmm10
	xor	r12,r8
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,r11
	add	r9,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,r10
	add	r9,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r11
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm4,6
	add	rbx,r9
	add	r9,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,rbx
	add	r14,r9
	vpsllq	xmm10,xmm4,3
	shrd	r13,r13,23
	mov	r9,r14
	vpaddq	xmm5,xmm5,xmm8
	mov	r12,rcx
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm4,19
	xor	r13,rbx
	xor	r12,rdx
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,r9
	vpsllq	xmm10,xmm10,42
	and	r12,rbx
	xor	r13,rbx
	vpxor	xmm11,xmm11,xmm9
	add	r8,QWORD[88+rsp]
	mov	rdi,r9
	vpsrlq	xmm9,xmm9,42
	xor	r12,rdx
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,r10
	add	r8,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm5,xmm5,xmm11
	xor	r14,r9
	add	r8,r13
	vpaddq	xmm10,xmm5,XMMWORD[32+rbp]
	xor	r15,r10
	shrd	r14,r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	vmovdqa	XMMWORD[80+rsp],xmm10
	vpalignr	xmm8,xmm7,xmm6,8
	shrd	r13,r13,23
	mov	r8,r14
	vpalignr	xmm11,xmm3,xmm2,8
	mov	r12,rbx
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,rax
	xor	r12,rcx
	vpaddq	xmm6,xmm6,xmm11
	shrd	r13,r13,4
	xor	r14,r8
	vpsrlq	xmm11,xmm8,7
	and	r12,rax
	xor	r13,rax
	vpsllq	xmm9,xmm8,56
	add	rdx,QWORD[96+rsp]
	mov	r15,r8
	vpxor	xmm8,xmm11,xmm10
	xor	r12,rcx
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,r9
	add	rdx,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,r8
	add	rdx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,r9
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm5,6
	add	r11,rdx
	add	rdx,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,r11
	add	r14,rdx
	vpsllq	xmm10,xmm5,3
	shrd	r13,r13,23
	mov	rdx,r14
	vpaddq	xmm6,xmm6,xmm8
	mov	r12,rax
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm5,19
	xor	r13,r11
	xor	r12,rbx
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,rdx
	vpsllq	xmm10,xmm10,42
	and	r12,r11
	xor	r13,r11
	vpxor	xmm11,xmm11,xmm9
	add	rcx,QWORD[104+rsp]
	mov	rdi,rdx
	vpsrlq	xmm9,xmm9,42
	xor	r12,rbx
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,r8
	add	rcx,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm6,xmm6,xmm11
	xor	r14,rdx
	add	rcx,r13
	vpaddq	xmm10,xmm6,XMMWORD[64+rbp]
	xor	r15,r8
	shrd	r14,r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	vmovdqa	XMMWORD[96+rsp],xmm10
	vpalignr	xmm8,xmm0,xmm7,8
	shrd	r13,r13,23
	mov	rcx,r14
	vpalignr	xmm11,xmm4,xmm3,8
	mov	r12,r11
	shrd	r14,r14,5
	vpsrlq	xmm10,xmm8,1
	xor	r13,r10
	xor	r12,rax
	vpaddq	xmm7,xmm7,xmm11
	shrd	r13,r13,4
	xor	r14,rcx
	vpsrlq	xmm11,xmm8,7
	and	r12,r10
	xor	r13,r10
	vpsllq	xmm9,xmm8,56
	add	rbx,QWORD[112+rsp]
	mov	r15,rcx
	vpxor	xmm8,xmm11,xmm10
	xor	r12,rax
	shrd	r14,r14,6
	vpsrlq	xmm10,xmm10,7
	xor	r15,rdx
	add	rbx,r12
	vpxor	xmm8,xmm8,xmm9
	shrd	r13,r13,14
	and	rdi,r15
	vpsllq	xmm9,xmm9,7
	xor	r14,rcx
	add	rbx,r13
	vpxor	xmm8,xmm8,xmm10
	xor	rdi,rdx
	shrd	r14,r14,28
	vpsrlq	xmm11,xmm6,6
	add	r9,rbx
	add	rbx,rdi
	vpxor	xmm8,xmm8,xmm9
	mov	r13,r9
	add	r14,rbx
	vpsllq	xmm10,xmm6,3
	shrd	r13,r13,23
	mov	rbx,r14
	vpaddq	xmm7,xmm7,xmm8
	mov	r12,r10
	shrd	r14,r14,5
	vpsrlq	xmm9,xmm6,19
	xor	r13,r9
	xor	r12,r11
	vpxor	xmm11,xmm11,xmm10
	shrd	r13,r13,4
	xor	r14,rbx
	vpsllq	xmm10,xmm10,42
	and	r12,r9
	xor	r13,r9
	vpxor	xmm11,xmm11,xmm9
	add	rax,QWORD[120+rsp]
	mov	rdi,rbx
	vpsrlq	xmm9,xmm9,42
	xor	r12,r11
	shrd	r14,r14,6
	vpxor	xmm11,xmm11,xmm10
	xor	rdi,rcx
	add	rax,r12
	vpxor	xmm11,xmm11,xmm9
	shrd	r13,r13,14
	and	r15,rdi
	vpaddq	xmm7,xmm7,xmm11
	xor	r14,rbx
	add	rax,r13
	vpaddq	xmm10,xmm7,XMMWORD[96+rbp]
	xor	r15,rcx
	shrd	r14,r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	vmovdqa	XMMWORD[112+rsp],xmm10
	cmp	BYTE[135+rbp],0
	jne	NEAR $L$avx_00_47
	shrd	r13,r13,23
	mov	rax,r14
	mov	r12,r9
	shrd	r14,r14,5
	xor	r13,r8
	xor	r12,r10
	shrd	r13,r13,4
	xor	r14,rax
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[rsp]
	mov	r15,rax
	xor	r12,r10
	shrd	r14,r14,6
	xor	r15,rbx
	add	r11,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,rax
	add	r11,r13
	xor	rdi,rbx
	shrd	r14,r14,28
	add	rdx,r11
	add	r11,rdi
	mov	r13,rdx
	add	r14,r11
	shrd	r13,r13,23
	mov	r11,r14
	mov	r12,r8
	shrd	r14,r14,5
	xor	r13,rdx
	xor	r12,r9
	shrd	r13,r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	add	r10,QWORD[8+rsp]
	mov	rdi,r11
	xor	r12,r9
	shrd	r14,r14,6
	xor	rdi,rax
	add	r10,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	shrd	r14,r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	shrd	r13,r13,23
	mov	r10,r14
	mov	r12,rdx
	shrd	r14,r14,5
	xor	r13,rcx
	xor	r12,r8
	shrd	r13,r13,4
	xor	r14,r10
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[16+rsp]
	mov	r15,r10
	xor	r12,r8
	shrd	r14,r14,6
	xor	r15,r11
	add	r9,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,r10
	add	r9,r13
	xor	rdi,r11
	shrd	r14,r14,28
	add	rbx,r9
	add	r9,rdi
	mov	r13,rbx
	add	r14,r9
	shrd	r13,r13,23
	mov	r9,r14
	mov	r12,rcx
	shrd	r14,r14,5
	xor	r13,rbx
	xor	r12,rdx
	shrd	r13,r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	add	r8,QWORD[24+rsp]
	mov	rdi,r9
	xor	r12,rdx
	shrd	r14,r14,6
	xor	rdi,r10
	add	r8,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	shrd	r14,r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	shrd	r13,r13,23
	mov	r8,r14
	mov	r12,rbx
	shrd	r14,r14,5
	xor	r13,rax
	xor	r12,rcx
	shrd	r13,r13,4
	xor	r14,r8
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[32+rsp]
	mov	r15,r8
	xor	r12,rcx
	shrd	r14,r14,6
	xor	r15,r9
	add	rdx,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,r8
	add	rdx,r13
	xor	rdi,r9
	shrd	r14,r14,28
	add	r11,rdx
	add	rdx,rdi
	mov	r13,r11
	add	r14,rdx
	shrd	r13,r13,23
	mov	rdx,r14
	mov	r12,rax
	shrd	r14,r14,5
	xor	r13,r11
	xor	r12,rbx
	shrd	r13,r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	add	rcx,QWORD[40+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	shrd	r14,r14,6
	xor	rdi,r8
	add	rcx,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	shrd	r14,r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	shrd	r13,r13,23
	mov	rcx,r14
	mov	r12,r11
	shrd	r14,r14,5
	xor	r13,r10
	xor	r12,rax
	shrd	r13,r13,4
	xor	r14,rcx
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[48+rsp]
	mov	r15,rcx
	xor	r12,rax
	shrd	r14,r14,6
	xor	r15,rdx
	add	rbx,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,rcx
	add	rbx,r13
	xor	rdi,rdx
	shrd	r14,r14,28
	add	r9,rbx
	add	rbx,rdi
	mov	r13,r9
	add	r14,rbx
	shrd	r13,r13,23
	mov	rbx,r14
	mov	r12,r10
	shrd	r14,r14,5
	xor	r13,r9
	xor	r12,r11
	shrd	r13,r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	add	rax,QWORD[56+rsp]
	mov	rdi,rbx
	xor	r12,r11
	shrd	r14,r14,6
	xor	rdi,rcx
	add	rax,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	shrd	r14,r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	shrd	r13,r13,23
	mov	rax,r14
	mov	r12,r9
	shrd	r14,r14,5
	xor	r13,r8
	xor	r12,r10
	shrd	r13,r13,4
	xor	r14,rax
	and	r12,r8
	xor	r13,r8
	add	r11,QWORD[64+rsp]
	mov	r15,rax
	xor	r12,r10
	shrd	r14,r14,6
	xor	r15,rbx
	add	r11,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,rax
	add	r11,r13
	xor	rdi,rbx
	shrd	r14,r14,28
	add	rdx,r11
	add	r11,rdi
	mov	r13,rdx
	add	r14,r11
	shrd	r13,r13,23
	mov	r11,r14
	mov	r12,r8
	shrd	r14,r14,5
	xor	r13,rdx
	xor	r12,r9
	shrd	r13,r13,4
	xor	r14,r11
	and	r12,rdx
	xor	r13,rdx
	add	r10,QWORD[72+rsp]
	mov	rdi,r11
	xor	r12,r9
	shrd	r14,r14,6
	xor	rdi,rax
	add	r10,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,r11
	add	r10,r13
	xor	r15,rax
	shrd	r14,r14,28
	add	rcx,r10
	add	r10,r15
	mov	r13,rcx
	add	r14,r10
	shrd	r13,r13,23
	mov	r10,r14
	mov	r12,rdx
	shrd	r14,r14,5
	xor	r13,rcx
	xor	r12,r8
	shrd	r13,r13,4
	xor	r14,r10
	and	r12,rcx
	xor	r13,rcx
	add	r9,QWORD[80+rsp]
	mov	r15,r10
	xor	r12,r8
	shrd	r14,r14,6
	xor	r15,r11
	add	r9,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,r10
	add	r9,r13
	xor	rdi,r11
	shrd	r14,r14,28
	add	rbx,r9
	add	r9,rdi
	mov	r13,rbx
	add	r14,r9
	shrd	r13,r13,23
	mov	r9,r14
	mov	r12,rcx
	shrd	r14,r14,5
	xor	r13,rbx
	xor	r12,rdx
	shrd	r13,r13,4
	xor	r14,r9
	and	r12,rbx
	xor	r13,rbx
	add	r8,QWORD[88+rsp]
	mov	rdi,r9
	xor	r12,rdx
	shrd	r14,r14,6
	xor	rdi,r10
	add	r8,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,r9
	add	r8,r13
	xor	r15,r10
	shrd	r14,r14,28
	add	rax,r8
	add	r8,r15
	mov	r13,rax
	add	r14,r8
	shrd	r13,r13,23
	mov	r8,r14
	mov	r12,rbx
	shrd	r14,r14,5
	xor	r13,rax
	xor	r12,rcx
	shrd	r13,r13,4
	xor	r14,r8
	and	r12,rax
	xor	r13,rax
	add	rdx,QWORD[96+rsp]
	mov	r15,r8
	xor	r12,rcx
	shrd	r14,r14,6
	xor	r15,r9
	add	rdx,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,r8
	add	rdx,r13
	xor	rdi,r9
	shrd	r14,r14,28
	add	r11,rdx
	add	rdx,rdi
	mov	r13,r11
	add	r14,rdx
	shrd	r13,r13,23
	mov	rdx,r14
	mov	r12,rax
	shrd	r14,r14,5
	xor	r13,r11
	xor	r12,rbx
	shrd	r13,r13,4
	xor	r14,rdx
	and	r12,r11
	xor	r13,r11
	add	rcx,QWORD[104+rsp]
	mov	rdi,rdx
	xor	r12,rbx
	shrd	r14,r14,6
	xor	rdi,r8
	add	rcx,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,rdx
	add	rcx,r13
	xor	r15,r8
	shrd	r14,r14,28
	add	r10,rcx
	add	rcx,r15
	mov	r13,r10
	add	r14,rcx
	shrd	r13,r13,23
	mov	rcx,r14
	mov	r12,r11
	shrd	r14,r14,5
	xor	r13,r10
	xor	r12,rax
	shrd	r13,r13,4
	xor	r14,rcx
	and	r12,r10
	xor	r13,r10
	add	rbx,QWORD[112+rsp]
	mov	r15,rcx
	xor	r12,rax
	shrd	r14,r14,6
	xor	r15,rdx
	add	rbx,r12
	shrd	r13,r13,14
	and	rdi,r15
	xor	r14,rcx
	add	rbx,r13
	xor	rdi,rdx
	shrd	r14,r14,28
	add	r9,rbx
	add	rbx,rdi
	mov	r13,r9
	add	r14,rbx
	shrd	r13,r13,23
	mov	rbx,r14
	mov	r12,r10
	shrd	r14,r14,5
	xor	r13,r9
	xor	r12,r11
	shrd	r13,r13,4
	xor	r14,rbx
	and	r12,r9
	xor	r13,r9
	add	rax,QWORD[120+rsp]
	mov	rdi,rbx
	xor	r12,r11
	shrd	r14,r14,6
	xor	rdi,rcx
	add	rax,r12
	shrd	r13,r13,14
	and	r15,rdi
	xor	r14,rbx
	add	rax,r13
	xor	r15,rcx
	shrd	r14,r14,28
	add	r8,rax
	add	rax,r15
	mov	r13,r8
	add	r14,rax
	mov	rdi,QWORD[((128+0))+rsp]
	mov	rax,r14

	add	rax,QWORD[rdi]
	lea	rsi,[128+rsi]
	add	rbx,QWORD[8+rdi]
	add	rcx,QWORD[16+rdi]
	add	rdx,QWORD[24+rdi]
	add	r8,QWORD[32+rdi]
	add	r9,QWORD[40+rdi]
	add	r10,QWORD[48+rdi]
	add	r11,QWORD[56+rdi]

	cmp	rsi,QWORD[((128+16))+rsp]

	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rcx
	mov	QWORD[24+rdi],rdx
	mov	QWORD[32+rdi],r8
	mov	QWORD[40+rdi],r9
	mov	QWORD[48+rdi],r10
	mov	QWORD[56+rdi],r11
	jb	NEAR $L$loop_avx

	mov	rsi,QWORD[152+rsp]

	vzeroupper
	movaps	xmm6,XMMWORD[((128+32))+rsp]
	movaps	xmm7,XMMWORD[((128+48))+rsp]
	movaps	xmm8,XMMWORD[((128+64))+rsp]
	movaps	xmm9,XMMWORD[((128+80))+rsp]
	movaps	xmm10,XMMWORD[((128+96))+rsp]
	movaps	xmm11,XMMWORD[((128+112))+rsp]
	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$epilogue_avx:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha512_block_data_order_avx:

ALIGN	64
sha512_block_data_order_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha512_block_data_order_avx2:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



$L$avx2_shortcut:
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

	sub	rsp,1408
	shl	rdx,4
	and	rsp,-256*8
	lea	rdx,[rdx*8+rsi]
	add	rsp,1152
	mov	QWORD[((128+0))+rsp],rdi
	mov	QWORD[((128+8))+rsp],rsi
	mov	QWORD[((128+16))+rsp],rdx
	mov	QWORD[152+rsp],rax

	movaps	XMMWORD[(128+32)+rsp],xmm6
	movaps	XMMWORD[(128+48)+rsp],xmm7
	movaps	XMMWORD[(128+64)+rsp],xmm8
	movaps	XMMWORD[(128+80)+rsp],xmm9
	movaps	XMMWORD[(128+96)+rsp],xmm10
	movaps	XMMWORD[(128+112)+rsp],xmm11
$L$prologue_avx2:

	vzeroupper
	sub	rsi,-16*8
	mov	rax,QWORD[rdi]
	mov	r12,rsi
	mov	rbx,QWORD[8+rdi]
	cmp	rsi,rdx
	mov	rcx,QWORD[16+rdi]
	cmove	r12,rsp
	mov	rdx,QWORD[24+rdi]
	mov	r8,QWORD[32+rdi]
	mov	r9,QWORD[40+rdi]
	mov	r10,QWORD[48+rdi]
	mov	r11,QWORD[56+rdi]
	jmp	NEAR $L$oop_avx2
ALIGN	16
$L$oop_avx2:
	vmovdqu	xmm0,XMMWORD[((-128))+rsi]
	vmovdqu	xmm1,XMMWORD[((-128+16))+rsi]
	vmovdqu	xmm2,XMMWORD[((-128+32))+rsi]
	lea	rbp,[((K512+128))]
	vmovdqu	xmm3,XMMWORD[((-128+48))+rsi]
	vmovdqu	xmm4,XMMWORD[((-128+64))+rsi]
	vmovdqu	xmm5,XMMWORD[((-128+80))+rsi]
	vmovdqu	xmm6,XMMWORD[((-128+96))+rsi]
	vmovdqu	xmm7,XMMWORD[((-128+112))+rsi]

	vmovdqa	ymm10,YMMWORD[1152+rbp]
	vinserti128	ymm0,ymm0,XMMWORD[r12],1
	vinserti128	ymm1,ymm1,XMMWORD[16+r12],1
	vpshufb	ymm0,ymm0,ymm10
	vinserti128	ymm2,ymm2,XMMWORD[32+r12],1
	vpshufb	ymm1,ymm1,ymm10
	vinserti128	ymm3,ymm3,XMMWORD[48+r12],1
	vpshufb	ymm2,ymm2,ymm10
	vinserti128	ymm4,ymm4,XMMWORD[64+r12],1
	vpshufb	ymm3,ymm3,ymm10
	vinserti128	ymm5,ymm5,XMMWORD[80+r12],1
	vpshufb	ymm4,ymm4,ymm10
	vinserti128	ymm6,ymm6,XMMWORD[96+r12],1
	vpshufb	ymm5,ymm5,ymm10
	vinserti128	ymm7,ymm7,XMMWORD[112+r12],1

	vpaddq	ymm8,ymm0,YMMWORD[((-128))+rbp]
	vpshufb	ymm6,ymm6,ymm10
	vpaddq	ymm9,ymm1,YMMWORD[((-96))+rbp]
	vpshufb	ymm7,ymm7,ymm10
	vpaddq	ymm10,ymm2,YMMWORD[((-64))+rbp]
	vpaddq	ymm11,ymm3,YMMWORD[((-32))+rbp]
	vmovdqa	YMMWORD[rsp],ymm8
	vpaddq	ymm8,ymm4,YMMWORD[rbp]
	vmovdqa	YMMWORD[32+rsp],ymm9
	vpaddq	ymm9,ymm5,YMMWORD[32+rbp]
	vmovdqa	YMMWORD[64+rsp],ymm10
	vpaddq	ymm10,ymm6,YMMWORD[64+rbp]
	vmovdqa	YMMWORD[96+rsp],ymm11
	lea	rsp,[((-128))+rsp]
	vpaddq	ymm11,ymm7,YMMWORD[96+rbp]
	vmovdqa	YMMWORD[rsp],ymm8
	xor	r14,r14
	vmovdqa	YMMWORD[32+rsp],ymm9
	mov	rdi,rbx
	vmovdqa	YMMWORD[64+rsp],ymm10
	xor	rdi,rcx
	vmovdqa	YMMWORD[96+rsp],ymm11
	mov	r12,r9
	add	rbp,16*2*8
	jmp	NEAR $L$avx2_00_47

ALIGN	16
$L$avx2_00_47:
	lea	rsp,[((-128))+rsp]
	vpalignr	ymm8,ymm1,ymm0,8
	add	r11,QWORD[((0+256))+rsp]
	and	r12,r8
	rorx	r13,r8,41
	vpalignr	ymm11,ymm5,ymm4,8
	rorx	r15,r8,18
	lea	rax,[r14*1+rax]
	lea	r11,[r12*1+r11]
	vpsrlq	ymm10,ymm8,1
	andn	r12,r8,r10
	xor	r13,r15
	rorx	r14,r8,14
	vpaddq	ymm0,ymm0,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	r11,[r12*1+r11]
	xor	r13,r14
	mov	r15,rax
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,rax,39
	lea	r11,[r13*1+r11]
	xor	r15,rbx
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,rax,34
	rorx	r13,rax,28
	lea	rdx,[r11*1+rdx]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rbx
	vpsrlq	ymm11,ymm7,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	r11,[rdi*1+r11]
	mov	r12,r8
	vpsllq	ymm10,ymm7,3
	vpaddq	ymm0,ymm0,ymm8
	add	r10,QWORD[((8+256))+rsp]
	and	r12,rdx
	rorx	r13,rdx,41
	vpsrlq	ymm9,ymm7,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,rdx,18
	lea	r11,[r14*1+r11]
	lea	r10,[r12*1+r10]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,rdx,r9
	xor	r13,rdi
	rorx	r14,rdx,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	r10,[r12*1+r10]
	xor	r13,r14
	mov	rdi,r11
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,r11,39
	lea	r10,[r13*1+r10]
	xor	rdi,rax
	vpaddq	ymm0,ymm0,ymm11
	rorx	r14,r11,34
	rorx	r13,r11,28
	lea	rcx,[r10*1+rcx]
	vpaddq	ymm10,ymm0,YMMWORD[((-128))+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rax
	xor	r14,r13
	lea	r10,[r15*1+r10]
	mov	r12,rdx
	vmovdqa	YMMWORD[rsp],ymm10
	vpalignr	ymm8,ymm2,ymm1,8
	add	r9,QWORD[((32+256))+rsp]
	and	r12,rcx
	rorx	r13,rcx,41
	vpalignr	ymm11,ymm6,ymm5,8
	rorx	r15,rcx,18
	lea	r10,[r14*1+r10]
	lea	r9,[r12*1+r9]
	vpsrlq	ymm10,ymm8,1
	andn	r12,rcx,r8
	xor	r13,r15
	rorx	r14,rcx,14
	vpaddq	ymm1,ymm1,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	r9,[r12*1+r9]
	xor	r13,r14
	mov	r15,r10
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,r10,39
	lea	r9,[r13*1+r9]
	xor	r15,r11
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,r10,34
	rorx	r13,r10,28
	lea	rbx,[r9*1+rbx]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r11
	vpsrlq	ymm11,ymm0,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	r9,[rdi*1+r9]
	mov	r12,rcx
	vpsllq	ymm10,ymm0,3
	vpaddq	ymm1,ymm1,ymm8
	add	r8,QWORD[((40+256))+rsp]
	and	r12,rbx
	rorx	r13,rbx,41
	vpsrlq	ymm9,ymm0,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,rbx,18
	lea	r9,[r14*1+r9]
	lea	r8,[r12*1+r8]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,rbx,rdx
	xor	r13,rdi
	rorx	r14,rbx,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	r8,[r12*1+r8]
	xor	r13,r14
	mov	rdi,r9
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,r9,39
	lea	r8,[r13*1+r8]
	xor	rdi,r10
	vpaddq	ymm1,ymm1,ymm11
	rorx	r14,r9,34
	rorx	r13,r9,28
	lea	rax,[r8*1+rax]
	vpaddq	ymm10,ymm1,YMMWORD[((-96))+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r10
	xor	r14,r13
	lea	r8,[r15*1+r8]
	mov	r12,rbx
	vmovdqa	YMMWORD[32+rsp],ymm10
	vpalignr	ymm8,ymm3,ymm2,8
	add	rdx,QWORD[((64+256))+rsp]
	and	r12,rax
	rorx	r13,rax,41
	vpalignr	ymm11,ymm7,ymm6,8
	rorx	r15,rax,18
	lea	r8,[r14*1+r8]
	lea	rdx,[r12*1+rdx]
	vpsrlq	ymm10,ymm8,1
	andn	r12,rax,rcx
	xor	r13,r15
	rorx	r14,rax,14
	vpaddq	ymm2,ymm2,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	rdx,[r12*1+rdx]
	xor	r13,r14
	mov	r15,r8
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,r8,39
	lea	rdx,[r13*1+rdx]
	xor	r15,r9
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,r8,34
	rorx	r13,r8,28
	lea	r11,[rdx*1+r11]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r9
	vpsrlq	ymm11,ymm1,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	rdx,[rdi*1+rdx]
	mov	r12,rax
	vpsllq	ymm10,ymm1,3
	vpaddq	ymm2,ymm2,ymm8
	add	rcx,QWORD[((72+256))+rsp]
	and	r12,r11
	rorx	r13,r11,41
	vpsrlq	ymm9,ymm1,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,r11,18
	lea	rdx,[r14*1+rdx]
	lea	rcx,[r12*1+rcx]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,r11,rbx
	xor	r13,rdi
	rorx	r14,r11,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	rcx,[r12*1+rcx]
	xor	r13,r14
	mov	rdi,rdx
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,rdx,39
	lea	rcx,[r13*1+rcx]
	xor	rdi,r8
	vpaddq	ymm2,ymm2,ymm11
	rorx	r14,rdx,34
	rorx	r13,rdx,28
	lea	r10,[rcx*1+r10]
	vpaddq	ymm10,ymm2,YMMWORD[((-64))+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r8
	xor	r14,r13
	lea	rcx,[r15*1+rcx]
	mov	r12,r11
	vmovdqa	YMMWORD[64+rsp],ymm10
	vpalignr	ymm8,ymm4,ymm3,8
	add	rbx,QWORD[((96+256))+rsp]
	and	r12,r10
	rorx	r13,r10,41
	vpalignr	ymm11,ymm0,ymm7,8
	rorx	r15,r10,18
	lea	rcx,[r14*1+rcx]
	lea	rbx,[r12*1+rbx]
	vpsrlq	ymm10,ymm8,1
	andn	r12,r10,rax
	xor	r13,r15
	rorx	r14,r10,14
	vpaddq	ymm3,ymm3,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	rbx,[r12*1+rbx]
	xor	r13,r14
	mov	r15,rcx
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,rcx,39
	lea	rbx,[r13*1+rbx]
	xor	r15,rdx
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,rcx,34
	rorx	r13,rcx,28
	lea	r9,[rbx*1+r9]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rdx
	vpsrlq	ymm11,ymm2,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	rbx,[rdi*1+rbx]
	mov	r12,r10
	vpsllq	ymm10,ymm2,3
	vpaddq	ymm3,ymm3,ymm8
	add	rax,QWORD[((104+256))+rsp]
	and	r12,r9
	rorx	r13,r9,41
	vpsrlq	ymm9,ymm2,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,r9,18
	lea	rbx,[r14*1+rbx]
	lea	rax,[r12*1+rax]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,r9,r11
	xor	r13,rdi
	rorx	r14,r9,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	rax,[r12*1+rax]
	xor	r13,r14
	mov	rdi,rbx
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,rbx,39
	lea	rax,[r13*1+rax]
	xor	rdi,rcx
	vpaddq	ymm3,ymm3,ymm11
	rorx	r14,rbx,34
	rorx	r13,rbx,28
	lea	r8,[rax*1+r8]
	vpaddq	ymm10,ymm3,YMMWORD[((-32))+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rcx
	xor	r14,r13
	lea	rax,[r15*1+rax]
	mov	r12,r9
	vmovdqa	YMMWORD[96+rsp],ymm10
	lea	rsp,[((-128))+rsp]
	vpalignr	ymm8,ymm5,ymm4,8
	add	r11,QWORD[((0+256))+rsp]
	and	r12,r8
	rorx	r13,r8,41
	vpalignr	ymm11,ymm1,ymm0,8
	rorx	r15,r8,18
	lea	rax,[r14*1+rax]
	lea	r11,[r12*1+r11]
	vpsrlq	ymm10,ymm8,1
	andn	r12,r8,r10
	xor	r13,r15
	rorx	r14,r8,14
	vpaddq	ymm4,ymm4,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	r11,[r12*1+r11]
	xor	r13,r14
	mov	r15,rax
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,rax,39
	lea	r11,[r13*1+r11]
	xor	r15,rbx
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,rax,34
	rorx	r13,rax,28
	lea	rdx,[r11*1+rdx]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rbx
	vpsrlq	ymm11,ymm3,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	r11,[rdi*1+r11]
	mov	r12,r8
	vpsllq	ymm10,ymm3,3
	vpaddq	ymm4,ymm4,ymm8
	add	r10,QWORD[((8+256))+rsp]
	and	r12,rdx
	rorx	r13,rdx,41
	vpsrlq	ymm9,ymm3,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,rdx,18
	lea	r11,[r14*1+r11]
	lea	r10,[r12*1+r10]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,rdx,r9
	xor	r13,rdi
	rorx	r14,rdx,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	r10,[r12*1+r10]
	xor	r13,r14
	mov	rdi,r11
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,r11,39
	lea	r10,[r13*1+r10]
	xor	rdi,rax
	vpaddq	ymm4,ymm4,ymm11
	rorx	r14,r11,34
	rorx	r13,r11,28
	lea	rcx,[r10*1+rcx]
	vpaddq	ymm10,ymm4,YMMWORD[rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rax
	xor	r14,r13
	lea	r10,[r15*1+r10]
	mov	r12,rdx
	vmovdqa	YMMWORD[rsp],ymm10
	vpalignr	ymm8,ymm6,ymm5,8
	add	r9,QWORD[((32+256))+rsp]
	and	r12,rcx
	rorx	r13,rcx,41
	vpalignr	ymm11,ymm2,ymm1,8
	rorx	r15,rcx,18
	lea	r10,[r14*1+r10]
	lea	r9,[r12*1+r9]
	vpsrlq	ymm10,ymm8,1
	andn	r12,rcx,r8
	xor	r13,r15
	rorx	r14,rcx,14
	vpaddq	ymm5,ymm5,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	r9,[r12*1+r9]
	xor	r13,r14
	mov	r15,r10
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,r10,39
	lea	r9,[r13*1+r9]
	xor	r15,r11
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,r10,34
	rorx	r13,r10,28
	lea	rbx,[r9*1+rbx]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r11
	vpsrlq	ymm11,ymm4,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	r9,[rdi*1+r9]
	mov	r12,rcx
	vpsllq	ymm10,ymm4,3
	vpaddq	ymm5,ymm5,ymm8
	add	r8,QWORD[((40+256))+rsp]
	and	r12,rbx
	rorx	r13,rbx,41
	vpsrlq	ymm9,ymm4,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,rbx,18
	lea	r9,[r14*1+r9]
	lea	r8,[r12*1+r8]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,rbx,rdx
	xor	r13,rdi
	rorx	r14,rbx,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	r8,[r12*1+r8]
	xor	r13,r14
	mov	rdi,r9
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,r9,39
	lea	r8,[r13*1+r8]
	xor	rdi,r10
	vpaddq	ymm5,ymm5,ymm11
	rorx	r14,r9,34
	rorx	r13,r9,28
	lea	rax,[r8*1+rax]
	vpaddq	ymm10,ymm5,YMMWORD[32+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r10
	xor	r14,r13
	lea	r8,[r15*1+r8]
	mov	r12,rbx
	vmovdqa	YMMWORD[32+rsp],ymm10
	vpalignr	ymm8,ymm7,ymm6,8
	add	rdx,QWORD[((64+256))+rsp]
	and	r12,rax
	rorx	r13,rax,41
	vpalignr	ymm11,ymm3,ymm2,8
	rorx	r15,rax,18
	lea	r8,[r14*1+r8]
	lea	rdx,[r12*1+rdx]
	vpsrlq	ymm10,ymm8,1
	andn	r12,rax,rcx
	xor	r13,r15
	rorx	r14,rax,14
	vpaddq	ymm6,ymm6,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	rdx,[r12*1+rdx]
	xor	r13,r14
	mov	r15,r8
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,r8,39
	lea	rdx,[r13*1+rdx]
	xor	r15,r9
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,r8,34
	rorx	r13,r8,28
	lea	r11,[rdx*1+r11]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r9
	vpsrlq	ymm11,ymm5,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	rdx,[rdi*1+rdx]
	mov	r12,rax
	vpsllq	ymm10,ymm5,3
	vpaddq	ymm6,ymm6,ymm8
	add	rcx,QWORD[((72+256))+rsp]
	and	r12,r11
	rorx	r13,r11,41
	vpsrlq	ymm9,ymm5,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,r11,18
	lea	rdx,[r14*1+rdx]
	lea	rcx,[r12*1+rcx]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,r11,rbx
	xor	r13,rdi
	rorx	r14,r11,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	rcx,[r12*1+rcx]
	xor	r13,r14
	mov	rdi,rdx
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,rdx,39
	lea	rcx,[r13*1+rcx]
	xor	rdi,r8
	vpaddq	ymm6,ymm6,ymm11
	rorx	r14,rdx,34
	rorx	r13,rdx,28
	lea	r10,[rcx*1+r10]
	vpaddq	ymm10,ymm6,YMMWORD[64+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r8
	xor	r14,r13
	lea	rcx,[r15*1+rcx]
	mov	r12,r11
	vmovdqa	YMMWORD[64+rsp],ymm10
	vpalignr	ymm8,ymm0,ymm7,8
	add	rbx,QWORD[((96+256))+rsp]
	and	r12,r10
	rorx	r13,r10,41
	vpalignr	ymm11,ymm4,ymm3,8
	rorx	r15,r10,18
	lea	rcx,[r14*1+rcx]
	lea	rbx,[r12*1+rbx]
	vpsrlq	ymm10,ymm8,1
	andn	r12,r10,rax
	xor	r13,r15
	rorx	r14,r10,14
	vpaddq	ymm7,ymm7,ymm11
	vpsrlq	ymm11,ymm8,7
	lea	rbx,[r12*1+rbx]
	xor	r13,r14
	mov	r15,rcx
	vpsllq	ymm9,ymm8,56
	vpxor	ymm8,ymm11,ymm10
	rorx	r12,rcx,39
	lea	rbx,[r13*1+rbx]
	xor	r15,rdx
	vpsrlq	ymm10,ymm10,7
	vpxor	ymm8,ymm8,ymm9
	rorx	r14,rcx,34
	rorx	r13,rcx,28
	lea	r9,[rbx*1+r9]
	vpsllq	ymm9,ymm9,7
	vpxor	ymm8,ymm8,ymm10
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rdx
	vpsrlq	ymm11,ymm6,6
	vpxor	ymm8,ymm8,ymm9
	xor	r14,r13
	lea	rbx,[rdi*1+rbx]
	mov	r12,r10
	vpsllq	ymm10,ymm6,3
	vpaddq	ymm7,ymm7,ymm8
	add	rax,QWORD[((104+256))+rsp]
	and	r12,r9
	rorx	r13,r9,41
	vpsrlq	ymm9,ymm6,19
	vpxor	ymm11,ymm11,ymm10
	rorx	rdi,r9,18
	lea	rbx,[r14*1+rbx]
	lea	rax,[r12*1+rax]
	vpsllq	ymm10,ymm10,42
	vpxor	ymm11,ymm11,ymm9
	andn	r12,r9,r11
	xor	r13,rdi
	rorx	r14,r9,14
	vpsrlq	ymm9,ymm9,42
	vpxor	ymm11,ymm11,ymm10
	lea	rax,[r12*1+rax]
	xor	r13,r14
	mov	rdi,rbx
	vpxor	ymm11,ymm11,ymm9
	rorx	r12,rbx,39
	lea	rax,[r13*1+rax]
	xor	rdi,rcx
	vpaddq	ymm7,ymm7,ymm11
	rorx	r14,rbx,34
	rorx	r13,rbx,28
	lea	r8,[rax*1+r8]
	vpaddq	ymm10,ymm7,YMMWORD[96+rbp]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rcx
	xor	r14,r13
	lea	rax,[r15*1+rax]
	mov	r12,r9
	vmovdqa	YMMWORD[96+rsp],ymm10
	lea	rbp,[256+rbp]
	cmp	BYTE[((-121))+rbp],0
	jne	NEAR $L$avx2_00_47
	add	r11,QWORD[((0+128))+rsp]
	and	r12,r8
	rorx	r13,r8,41
	rorx	r15,r8,18
	lea	rax,[r14*1+rax]
	lea	r11,[r12*1+r11]
	andn	r12,r8,r10
	xor	r13,r15
	rorx	r14,r8,14
	lea	r11,[r12*1+r11]
	xor	r13,r14
	mov	r15,rax
	rorx	r12,rax,39
	lea	r11,[r13*1+r11]
	xor	r15,rbx
	rorx	r14,rax,34
	rorx	r13,rax,28
	lea	rdx,[r11*1+rdx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rbx
	xor	r14,r13
	lea	r11,[rdi*1+r11]
	mov	r12,r8
	add	r10,QWORD[((8+128))+rsp]
	and	r12,rdx
	rorx	r13,rdx,41
	rorx	rdi,rdx,18
	lea	r11,[r14*1+r11]
	lea	r10,[r12*1+r10]
	andn	r12,rdx,r9
	xor	r13,rdi
	rorx	r14,rdx,14
	lea	r10,[r12*1+r10]
	xor	r13,r14
	mov	rdi,r11
	rorx	r12,r11,39
	lea	r10,[r13*1+r10]
	xor	rdi,rax
	rorx	r14,r11,34
	rorx	r13,r11,28
	lea	rcx,[r10*1+rcx]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rax
	xor	r14,r13
	lea	r10,[r15*1+r10]
	mov	r12,rdx
	add	r9,QWORD[((32+128))+rsp]
	and	r12,rcx
	rorx	r13,rcx,41
	rorx	r15,rcx,18
	lea	r10,[r14*1+r10]
	lea	r9,[r12*1+r9]
	andn	r12,rcx,r8
	xor	r13,r15
	rorx	r14,rcx,14
	lea	r9,[r12*1+r9]
	xor	r13,r14
	mov	r15,r10
	rorx	r12,r10,39
	lea	r9,[r13*1+r9]
	xor	r15,r11
	rorx	r14,r10,34
	rorx	r13,r10,28
	lea	rbx,[r9*1+rbx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r11
	xor	r14,r13
	lea	r9,[rdi*1+r9]
	mov	r12,rcx
	add	r8,QWORD[((40+128))+rsp]
	and	r12,rbx
	rorx	r13,rbx,41
	rorx	rdi,rbx,18
	lea	r9,[r14*1+r9]
	lea	r8,[r12*1+r8]
	andn	r12,rbx,rdx
	xor	r13,rdi
	rorx	r14,rbx,14
	lea	r8,[r12*1+r8]
	xor	r13,r14
	mov	rdi,r9
	rorx	r12,r9,39
	lea	r8,[r13*1+r8]
	xor	rdi,r10
	rorx	r14,r9,34
	rorx	r13,r9,28
	lea	rax,[r8*1+rax]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r10
	xor	r14,r13
	lea	r8,[r15*1+r8]
	mov	r12,rbx
	add	rdx,QWORD[((64+128))+rsp]
	and	r12,rax
	rorx	r13,rax,41
	rorx	r15,rax,18
	lea	r8,[r14*1+r8]
	lea	rdx,[r12*1+rdx]
	andn	r12,rax,rcx
	xor	r13,r15
	rorx	r14,rax,14
	lea	rdx,[r12*1+rdx]
	xor	r13,r14
	mov	r15,r8
	rorx	r12,r8,39
	lea	rdx,[r13*1+rdx]
	xor	r15,r9
	rorx	r14,r8,34
	rorx	r13,r8,28
	lea	r11,[rdx*1+r11]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r9
	xor	r14,r13
	lea	rdx,[rdi*1+rdx]
	mov	r12,rax
	add	rcx,QWORD[((72+128))+rsp]
	and	r12,r11
	rorx	r13,r11,41
	rorx	rdi,r11,18
	lea	rdx,[r14*1+rdx]
	lea	rcx,[r12*1+rcx]
	andn	r12,r11,rbx
	xor	r13,rdi
	rorx	r14,r11,14
	lea	rcx,[r12*1+rcx]
	xor	r13,r14
	mov	rdi,rdx
	rorx	r12,rdx,39
	lea	rcx,[r13*1+rcx]
	xor	rdi,r8
	rorx	r14,rdx,34
	rorx	r13,rdx,28
	lea	r10,[rcx*1+r10]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r8
	xor	r14,r13
	lea	rcx,[r15*1+rcx]
	mov	r12,r11
	add	rbx,QWORD[((96+128))+rsp]
	and	r12,r10
	rorx	r13,r10,41
	rorx	r15,r10,18
	lea	rcx,[r14*1+rcx]
	lea	rbx,[r12*1+rbx]
	andn	r12,r10,rax
	xor	r13,r15
	rorx	r14,r10,14
	lea	rbx,[r12*1+rbx]
	xor	r13,r14
	mov	r15,rcx
	rorx	r12,rcx,39
	lea	rbx,[r13*1+rbx]
	xor	r15,rdx
	rorx	r14,rcx,34
	rorx	r13,rcx,28
	lea	r9,[rbx*1+r9]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rdx
	xor	r14,r13
	lea	rbx,[rdi*1+rbx]
	mov	r12,r10
	add	rax,QWORD[((104+128))+rsp]
	and	r12,r9
	rorx	r13,r9,41
	rorx	rdi,r9,18
	lea	rbx,[r14*1+rbx]
	lea	rax,[r12*1+rax]
	andn	r12,r9,r11
	xor	r13,rdi
	rorx	r14,r9,14
	lea	rax,[r12*1+rax]
	xor	r13,r14
	mov	rdi,rbx
	rorx	r12,rbx,39
	lea	rax,[r13*1+rax]
	xor	rdi,rcx
	rorx	r14,rbx,34
	rorx	r13,rbx,28
	lea	r8,[rax*1+r8]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rcx
	xor	r14,r13
	lea	rax,[r15*1+rax]
	mov	r12,r9
	add	r11,QWORD[rsp]
	and	r12,r8
	rorx	r13,r8,41
	rorx	r15,r8,18
	lea	rax,[r14*1+rax]
	lea	r11,[r12*1+r11]
	andn	r12,r8,r10
	xor	r13,r15
	rorx	r14,r8,14
	lea	r11,[r12*1+r11]
	xor	r13,r14
	mov	r15,rax
	rorx	r12,rax,39
	lea	r11,[r13*1+r11]
	xor	r15,rbx
	rorx	r14,rax,34
	rorx	r13,rax,28
	lea	rdx,[r11*1+rdx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rbx
	xor	r14,r13
	lea	r11,[rdi*1+r11]
	mov	r12,r8
	add	r10,QWORD[8+rsp]
	and	r12,rdx
	rorx	r13,rdx,41
	rorx	rdi,rdx,18
	lea	r11,[r14*1+r11]
	lea	r10,[r12*1+r10]
	andn	r12,rdx,r9
	xor	r13,rdi
	rorx	r14,rdx,14
	lea	r10,[r12*1+r10]
	xor	r13,r14
	mov	rdi,r11
	rorx	r12,r11,39
	lea	r10,[r13*1+r10]
	xor	rdi,rax
	rorx	r14,r11,34
	rorx	r13,r11,28
	lea	rcx,[r10*1+rcx]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rax
	xor	r14,r13
	lea	r10,[r15*1+r10]
	mov	r12,rdx
	add	r9,QWORD[32+rsp]
	and	r12,rcx
	rorx	r13,rcx,41
	rorx	r15,rcx,18
	lea	r10,[r14*1+r10]
	lea	r9,[r12*1+r9]
	andn	r12,rcx,r8
	xor	r13,r15
	rorx	r14,rcx,14
	lea	r9,[r12*1+r9]
	xor	r13,r14
	mov	r15,r10
	rorx	r12,r10,39
	lea	r9,[r13*1+r9]
	xor	r15,r11
	rorx	r14,r10,34
	rorx	r13,r10,28
	lea	rbx,[r9*1+rbx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r11
	xor	r14,r13
	lea	r9,[rdi*1+r9]
	mov	r12,rcx
	add	r8,QWORD[40+rsp]
	and	r12,rbx
	rorx	r13,rbx,41
	rorx	rdi,rbx,18
	lea	r9,[r14*1+r9]
	lea	r8,[r12*1+r8]
	andn	r12,rbx,rdx
	xor	r13,rdi
	rorx	r14,rbx,14
	lea	r8,[r12*1+r8]
	xor	r13,r14
	mov	rdi,r9
	rorx	r12,r9,39
	lea	r8,[r13*1+r8]
	xor	rdi,r10
	rorx	r14,r9,34
	rorx	r13,r9,28
	lea	rax,[r8*1+rax]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r10
	xor	r14,r13
	lea	r8,[r15*1+r8]
	mov	r12,rbx
	add	rdx,QWORD[64+rsp]
	and	r12,rax
	rorx	r13,rax,41
	rorx	r15,rax,18
	lea	r8,[r14*1+r8]
	lea	rdx,[r12*1+rdx]
	andn	r12,rax,rcx
	xor	r13,r15
	rorx	r14,rax,14
	lea	rdx,[r12*1+rdx]
	xor	r13,r14
	mov	r15,r8
	rorx	r12,r8,39
	lea	rdx,[r13*1+rdx]
	xor	r15,r9
	rorx	r14,r8,34
	rorx	r13,r8,28
	lea	r11,[rdx*1+r11]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r9
	xor	r14,r13
	lea	rdx,[rdi*1+rdx]
	mov	r12,rax
	add	rcx,QWORD[72+rsp]
	and	r12,r11
	rorx	r13,r11,41
	rorx	rdi,r11,18
	lea	rdx,[r14*1+rdx]
	lea	rcx,[r12*1+rcx]
	andn	r12,r11,rbx
	xor	r13,rdi
	rorx	r14,r11,14
	lea	rcx,[r12*1+rcx]
	xor	r13,r14
	mov	rdi,rdx
	rorx	r12,rdx,39
	lea	rcx,[r13*1+rcx]
	xor	rdi,r8
	rorx	r14,rdx,34
	rorx	r13,rdx,28
	lea	r10,[rcx*1+r10]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r8
	xor	r14,r13
	lea	rcx,[r15*1+rcx]
	mov	r12,r11
	add	rbx,QWORD[96+rsp]
	and	r12,r10
	rorx	r13,r10,41
	rorx	r15,r10,18
	lea	rcx,[r14*1+rcx]
	lea	rbx,[r12*1+rbx]
	andn	r12,r10,rax
	xor	r13,r15
	rorx	r14,r10,14
	lea	rbx,[r12*1+rbx]
	xor	r13,r14
	mov	r15,rcx
	rorx	r12,rcx,39
	lea	rbx,[r13*1+rbx]
	xor	r15,rdx
	rorx	r14,rcx,34
	rorx	r13,rcx,28
	lea	r9,[rbx*1+r9]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rdx
	xor	r14,r13
	lea	rbx,[rdi*1+rbx]
	mov	r12,r10
	add	rax,QWORD[104+rsp]
	and	r12,r9
	rorx	r13,r9,41
	rorx	rdi,r9,18
	lea	rbx,[r14*1+rbx]
	lea	rax,[r12*1+rax]
	andn	r12,r9,r11
	xor	r13,rdi
	rorx	r14,r9,14
	lea	rax,[r12*1+rax]
	xor	r13,r14
	mov	rdi,rbx
	rorx	r12,rbx,39
	lea	rax,[r13*1+rax]
	xor	rdi,rcx
	rorx	r14,rbx,34
	rorx	r13,rbx,28
	lea	r8,[rax*1+r8]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rcx
	xor	r14,r13
	lea	rax,[r15*1+rax]
	mov	r12,r9
	mov	rdi,QWORD[1280+rsp]
	add	rax,r14

	lea	rbp,[1152+rsp]

	add	rax,QWORD[rdi]
	add	rbx,QWORD[8+rdi]
	add	rcx,QWORD[16+rdi]
	add	rdx,QWORD[24+rdi]
	add	r8,QWORD[32+rdi]
	add	r9,QWORD[40+rdi]
	add	r10,QWORD[48+rdi]
	add	r11,QWORD[56+rdi]

	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rcx
	mov	QWORD[24+rdi],rdx
	mov	QWORD[32+rdi],r8
	mov	QWORD[40+rdi],r9
	mov	QWORD[48+rdi],r10
	mov	QWORD[56+rdi],r11

	cmp	rsi,QWORD[144+rbp]
	je	NEAR $L$done_avx2

	xor	r14,r14
	mov	rdi,rbx
	xor	rdi,rcx
	mov	r12,r9
	jmp	NEAR $L$ower_avx2
ALIGN	16
$L$ower_avx2:
	add	r11,QWORD[((0+16))+rbp]
	and	r12,r8
	rorx	r13,r8,41
	rorx	r15,r8,18
	lea	rax,[r14*1+rax]
	lea	r11,[r12*1+r11]
	andn	r12,r8,r10
	xor	r13,r15
	rorx	r14,r8,14
	lea	r11,[r12*1+r11]
	xor	r13,r14
	mov	r15,rax
	rorx	r12,rax,39
	lea	r11,[r13*1+r11]
	xor	r15,rbx
	rorx	r14,rax,34
	rorx	r13,rax,28
	lea	rdx,[r11*1+rdx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rbx
	xor	r14,r13
	lea	r11,[rdi*1+r11]
	mov	r12,r8
	add	r10,QWORD[((8+16))+rbp]
	and	r12,rdx
	rorx	r13,rdx,41
	rorx	rdi,rdx,18
	lea	r11,[r14*1+r11]
	lea	r10,[r12*1+r10]
	andn	r12,rdx,r9
	xor	r13,rdi
	rorx	r14,rdx,14
	lea	r10,[r12*1+r10]
	xor	r13,r14
	mov	rdi,r11
	rorx	r12,r11,39
	lea	r10,[r13*1+r10]
	xor	rdi,rax
	rorx	r14,r11,34
	rorx	r13,r11,28
	lea	rcx,[r10*1+rcx]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rax
	xor	r14,r13
	lea	r10,[r15*1+r10]
	mov	r12,rdx
	add	r9,QWORD[((32+16))+rbp]
	and	r12,rcx
	rorx	r13,rcx,41
	rorx	r15,rcx,18
	lea	r10,[r14*1+r10]
	lea	r9,[r12*1+r9]
	andn	r12,rcx,r8
	xor	r13,r15
	rorx	r14,rcx,14
	lea	r9,[r12*1+r9]
	xor	r13,r14
	mov	r15,r10
	rorx	r12,r10,39
	lea	r9,[r13*1+r9]
	xor	r15,r11
	rorx	r14,r10,34
	rorx	r13,r10,28
	lea	rbx,[r9*1+rbx]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r11
	xor	r14,r13
	lea	r9,[rdi*1+r9]
	mov	r12,rcx
	add	r8,QWORD[((40+16))+rbp]
	and	r12,rbx
	rorx	r13,rbx,41
	rorx	rdi,rbx,18
	lea	r9,[r14*1+r9]
	lea	r8,[r12*1+r8]
	andn	r12,rbx,rdx
	xor	r13,rdi
	rorx	r14,rbx,14
	lea	r8,[r12*1+r8]
	xor	r13,r14
	mov	rdi,r9
	rorx	r12,r9,39
	lea	r8,[r13*1+r8]
	xor	rdi,r10
	rorx	r14,r9,34
	rorx	r13,r9,28
	lea	rax,[r8*1+rax]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r10
	xor	r14,r13
	lea	r8,[r15*1+r8]
	mov	r12,rbx
	add	rdx,QWORD[((64+16))+rbp]
	and	r12,rax
	rorx	r13,rax,41
	rorx	r15,rax,18
	lea	r8,[r14*1+r8]
	lea	rdx,[r12*1+rdx]
	andn	r12,rax,rcx
	xor	r13,r15
	rorx	r14,rax,14
	lea	rdx,[r12*1+rdx]
	xor	r13,r14
	mov	r15,r8
	rorx	r12,r8,39
	lea	rdx,[r13*1+rdx]
	xor	r15,r9
	rorx	r14,r8,34
	rorx	r13,r8,28
	lea	r11,[rdx*1+r11]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,r9
	xor	r14,r13
	lea	rdx,[rdi*1+rdx]
	mov	r12,rax
	add	rcx,QWORD[((72+16))+rbp]
	and	r12,r11
	rorx	r13,r11,41
	rorx	rdi,r11,18
	lea	rdx,[r14*1+rdx]
	lea	rcx,[r12*1+rcx]
	andn	r12,r11,rbx
	xor	r13,rdi
	rorx	r14,r11,14
	lea	rcx,[r12*1+rcx]
	xor	r13,r14
	mov	rdi,rdx
	rorx	r12,rdx,39
	lea	rcx,[r13*1+rcx]
	xor	rdi,r8
	rorx	r14,rdx,34
	rorx	r13,rdx,28
	lea	r10,[rcx*1+r10]
	and	r15,rdi
	xor	r14,r12
	xor	r15,r8
	xor	r14,r13
	lea	rcx,[r15*1+rcx]
	mov	r12,r11
	add	rbx,QWORD[((96+16))+rbp]
	and	r12,r10
	rorx	r13,r10,41
	rorx	r15,r10,18
	lea	rcx,[r14*1+rcx]
	lea	rbx,[r12*1+rbx]
	andn	r12,r10,rax
	xor	r13,r15
	rorx	r14,r10,14
	lea	rbx,[r12*1+rbx]
	xor	r13,r14
	mov	r15,rcx
	rorx	r12,rcx,39
	lea	rbx,[r13*1+rbx]
	xor	r15,rdx
	rorx	r14,rcx,34
	rorx	r13,rcx,28
	lea	r9,[rbx*1+r9]
	and	rdi,r15
	xor	r14,r12
	xor	rdi,rdx
	xor	r14,r13
	lea	rbx,[rdi*1+rbx]
	mov	r12,r10
	add	rax,QWORD[((104+16))+rbp]
	and	r12,r9
	rorx	r13,r9,41
	rorx	rdi,r9,18
	lea	rbx,[r14*1+rbx]
	lea	rax,[r12*1+rax]
	andn	r12,r9,r11
	xor	r13,rdi
	rorx	r14,r9,14
	lea	rax,[r12*1+rax]
	xor	r13,r14
	mov	rdi,rbx
	rorx	r12,rbx,39
	lea	rax,[r13*1+rax]
	xor	rdi,rcx
	rorx	r14,rbx,34
	rorx	r13,rbx,28
	lea	r8,[rax*1+r8]
	and	r15,rdi
	xor	r14,r12
	xor	r15,rcx
	xor	r14,r13
	lea	rax,[r15*1+rax]
	mov	r12,r9
	lea	rbp,[((-128))+rbp]
	cmp	rbp,rsp
	jae	NEAR $L$ower_avx2

	mov	rdi,QWORD[1280+rsp]
	add	rax,r14

	lea	rsp,[1152+rsp]

	add	rax,QWORD[rdi]
	add	rbx,QWORD[8+rdi]
	add	rcx,QWORD[16+rdi]
	add	rdx,QWORD[24+rdi]
	add	r8,QWORD[32+rdi]
	add	r9,QWORD[40+rdi]
	lea	rsi,[256+rsi]
	add	r10,QWORD[48+rdi]
	mov	r12,rsi
	add	r11,QWORD[56+rdi]
	cmp	rsi,QWORD[((128+16))+rsp]

	mov	QWORD[rdi],rax
	cmove	r12,rsp
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rcx
	mov	QWORD[24+rdi],rdx
	mov	QWORD[32+rdi],r8
	mov	QWORD[40+rdi],r9
	mov	QWORD[48+rdi],r10
	mov	QWORD[56+rdi],r11

	jbe	NEAR $L$oop_avx2
	lea	rbp,[rsp]

$L$done_avx2:
	lea	rsp,[rbp]
	mov	rsi,QWORD[152+rsp]

	vzeroupper
	movaps	xmm6,XMMWORD[((128+32))+rsp]
	movaps	xmm7,XMMWORD[((128+48))+rsp]
	movaps	xmm8,XMMWORD[((128+64))+rsp]
	movaps	xmm9,XMMWORD[((128+80))+rsp]
	movaps	xmm10,XMMWORD[((128+96))+rsp]
	movaps	xmm11,XMMWORD[((128+112))+rsp]
	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$epilogue_avx2:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha512_block_data_order_avx2:
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
se_handler:
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

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	mov	rsi,QWORD[8+r9]
	mov	r11,QWORD[56+r9]

	mov	r10d,DWORD[r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$in_prologue
	lea	r10,[$L$avx2_shortcut]
	cmp	rbx,r10
	jb	NEAR $L$not_in_avx2

	and	rax,-256*8
	add	rax,1152
$L$not_in_avx2:
	mov	rsi,rax
	mov	rax,QWORD[((128+24))+rax]

	mov	rbx,QWORD[((-8))+rax]
	mov	rbp,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r13,QWORD[((-32))+rax]
	mov	r14,QWORD[((-40))+rax]
	mov	r15,QWORD[((-48))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

	lea	r10,[$L$epilogue]
	cmp	rbx,r10
	jb	NEAR $L$in_prologue

	lea	rsi,[((128+32))+rsi]
	lea	rdi,[512+r8]
	mov	ecx,12
	DD	0xa548f3fc

$L$in_prologue:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi

	mov	rdi,QWORD[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0xa548f3fc

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD[8+rsi]
	mov	r8,QWORD[rsi]
	mov	r9,QWORD[16+rsi]
	mov	r10,QWORD[40+rsi]
	lea	r11,[56+rsi]
	lea	r12,[24+rsi]
	mov	QWORD[32+rsp],r10
	mov	QWORD[40+rsp],r11
	mov	QWORD[48+rsp],r12
	mov	QWORD[56+rsp],rcx
	call	QWORD[__imp_RtlVirtualUnwind]

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

section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_sha512_block_data_order wrt ..imagebase
	DD	$L$SEH_end_sha512_block_data_order wrt ..imagebase
	DD	$L$SEH_info_sha512_block_data_order wrt ..imagebase
	DD	$L$SEH_begin_sha512_block_data_order_xop wrt ..imagebase
	DD	$L$SEH_end_sha512_block_data_order_xop wrt ..imagebase
	DD	$L$SEH_info_sha512_block_data_order_xop wrt ..imagebase
	DD	$L$SEH_begin_sha512_block_data_order_avx wrt ..imagebase
	DD	$L$SEH_end_sha512_block_data_order_avx wrt ..imagebase
	DD	$L$SEH_info_sha512_block_data_order_avx wrt ..imagebase
	DD	$L$SEH_begin_sha512_block_data_order_avx2 wrt ..imagebase
	DD	$L$SEH_end_sha512_block_data_order_avx2 wrt ..imagebase
	DD	$L$SEH_info_sha512_block_data_order_avx2 wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_sha512_block_data_order:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$prologue wrt ..imagebase,$L$epilogue wrt ..imagebase
$L$SEH_info_sha512_block_data_order_xop:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$prologue_xop wrt ..imagebase,$L$epilogue_xop wrt ..imagebase
$L$SEH_info_sha512_block_data_order_avx:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$prologue_avx wrt ..imagebase,$L$epilogue_avx wrt ..imagebase
$L$SEH_info_sha512_block_data_order_avx2:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$prologue_avx2 wrt ..imagebase,$L$epilogue_avx2 wrt ..imagebase
