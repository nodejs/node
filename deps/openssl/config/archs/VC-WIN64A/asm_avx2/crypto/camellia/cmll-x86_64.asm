default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64



global	Camellia_EncryptBlock

ALIGN	16
Camellia_EncryptBlock:
	mov	eax,128
	sub	eax,ecx
	mov	ecx,3
	adc	ecx,0
	jmp	NEAR $L$enc_rounds


global	Camellia_EncryptBlock_Rounds

ALIGN	16
$L$enc_rounds:
Camellia_EncryptBlock_Rounds:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_EncryptBlock_Rounds:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



	push	rbx

	push	rbp

	push	r13

	push	r14

	push	r15

$L$enc_prologue:


	mov	r13,rcx
	mov	r14,rdx

	shl	edi,6
	lea	rbp,[$L$Camellia_SBOX]
	lea	r15,[rdi*1+r14]

	mov	r8d,DWORD[rsi]
	mov	r9d,DWORD[4+rsi]
	mov	r10d,DWORD[8+rsi]
	bswap	r8d
	mov	r11d,DWORD[12+rsi]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_encrypt

	bswap	r8d
	bswap	r9d
	bswap	r10d
	mov	DWORD[r13],r8d
	bswap	r11d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	mov	DWORD[12+r13],r11d

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	rbp,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	lea	rsp,[40+rsp]

$L$enc_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_Camellia_EncryptBlock_Rounds:


ALIGN	16
_x86_64_Camellia_encrypt:
	xor	r9d,DWORD[r14]
	xor	r8d,DWORD[4+r14]
	xor	r11d,DWORD[8+r14]
	xor	r10d,DWORD[12+r14]
ALIGN	16
$L$eloop:
	mov	ebx,DWORD[16+r14]
	mov	eax,DWORD[20+r14]

	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[24+r14]
	mov	eax,DWORD[28+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[32+r14]
	mov	eax,DWORD[36+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[40+r14]
	mov	eax,DWORD[44+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[48+r14]
	mov	eax,DWORD[52+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[56+r14]
	mov	eax,DWORD[60+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[64+r14]
	mov	eax,DWORD[68+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	lea	r14,[64+r14]
	cmp	r14,r15
	mov	edx,DWORD[8+r14]
	mov	ecx,DWORD[12+r14]
	je	NEAR $L$edone

	and	eax,r8d
	or	edx,r11d
	rol	eax,1
	xor	r10d,edx
	xor	r9d,eax
	and	ecx,r10d
	or	ebx,r9d
	rol	ecx,1
	xor	r8d,ebx
	xor	r11d,ecx
	jmp	NEAR $L$eloop

ALIGN	16
$L$edone:
	xor	eax,r10d
	xor	ebx,r11d
	xor	ecx,r8d
	xor	edx,r9d

	mov	r8d,eax
	mov	r9d,ebx
	mov	r10d,ecx
	mov	r11d,edx

DB	0xf3,0xc3



global	Camellia_DecryptBlock

ALIGN	16
Camellia_DecryptBlock:
	mov	eax,128
	sub	eax,ecx
	mov	ecx,3
	adc	ecx,0
	jmp	NEAR $L$dec_rounds


global	Camellia_DecryptBlock_Rounds

ALIGN	16
$L$dec_rounds:
Camellia_DecryptBlock_Rounds:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_DecryptBlock_Rounds:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



	push	rbx

	push	rbp

	push	r13

	push	r14

	push	r15

$L$dec_prologue:


	mov	r13,rcx
	mov	r15,rdx

	shl	edi,6
	lea	rbp,[$L$Camellia_SBOX]
	lea	r14,[rdi*1+r15]

	mov	r8d,DWORD[rsi]
	mov	r9d,DWORD[4+rsi]
	mov	r10d,DWORD[8+rsi]
	bswap	r8d
	mov	r11d,DWORD[12+rsi]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_decrypt

	bswap	r8d
	bswap	r9d
	bswap	r10d
	mov	DWORD[r13],r8d
	bswap	r11d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	mov	DWORD[12+r13],r11d

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	rbp,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	lea	rsp,[40+rsp]

$L$dec_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_Camellia_DecryptBlock_Rounds:


ALIGN	16
_x86_64_Camellia_decrypt:
	xor	r9d,DWORD[r14]
	xor	r8d,DWORD[4+r14]
	xor	r11d,DWORD[8+r14]
	xor	r10d,DWORD[12+r14]
ALIGN	16
$L$dloop:
	mov	ebx,DWORD[((-8))+r14]
	mov	eax,DWORD[((-4))+r14]

	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-16))+r14]
	mov	eax,DWORD[((-12))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-24))+r14]
	mov	eax,DWORD[((-20))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-32))+r14]
	mov	eax,DWORD[((-28))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-40))+r14]
	mov	eax,DWORD[((-36))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-48))+r14]
	mov	eax,DWORD[((-44))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[((-56))+r14]
	mov	eax,DWORD[((-52))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	lea	r14,[((-64))+r14]
	cmp	r14,r15
	mov	edx,DWORD[r14]
	mov	ecx,DWORD[4+r14]
	je	NEAR $L$ddone

	and	eax,r8d
	or	edx,r11d
	rol	eax,1
	xor	r10d,edx
	xor	r9d,eax
	and	ecx,r10d
	or	ebx,r9d
	rol	ecx,1
	xor	r8d,ebx
	xor	r11d,ecx

	jmp	NEAR $L$dloop

ALIGN	16
$L$ddone:
	xor	ecx,r10d
	xor	edx,r11d
	xor	eax,r8d
	xor	ebx,r9d

	mov	r8d,ecx
	mov	r9d,edx
	mov	r10d,eax
	mov	r11d,ebx

DB	0xf3,0xc3

global	Camellia_Ekeygen

ALIGN	16
Camellia_Ekeygen:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_Ekeygen:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



	push	rbx

	push	rbp

	push	r13

	push	r14

	push	r15

$L$key_prologue:

	mov	r15d,edi
	mov	r13,rdx

	mov	r8d,DWORD[rsi]
	mov	r9d,DWORD[4+rsi]
	mov	r10d,DWORD[8+rsi]
	mov	r11d,DWORD[12+rsi]

	bswap	r8d
	bswap	r9d
	bswap	r10d
	bswap	r11d
	mov	DWORD[r13],r9d
	mov	DWORD[4+r13],r8d
	mov	DWORD[8+r13],r11d
	mov	DWORD[12+r13],r10d
	cmp	r15,128
	je	NEAR $L$1st128

	mov	r8d,DWORD[16+rsi]
	mov	r9d,DWORD[20+rsi]
	cmp	r15,192
	je	NEAR $L$1st192
	mov	r10d,DWORD[24+rsi]
	mov	r11d,DWORD[28+rsi]
	jmp	NEAR $L$1st256
$L$1st192:
	mov	r10d,r8d
	mov	r11d,r9d
	not	r10d
	not	r11d
$L$1st256:
	bswap	r8d
	bswap	r9d
	bswap	r10d
	bswap	r11d
	mov	DWORD[32+r13],r9d
	mov	DWORD[36+r13],r8d
	mov	DWORD[40+r13],r11d
	mov	DWORD[44+r13],r10d
	xor	r9d,DWORD[r13]
	xor	r8d,DWORD[4+r13]
	xor	r11d,DWORD[8+r13]
	xor	r10d,DWORD[12+r13]

$L$1st128:
	lea	r14,[$L$Camellia_SIGMA]
	lea	rbp,[$L$Camellia_SBOX]

	mov	ebx,DWORD[r14]
	mov	eax,DWORD[4+r14]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[8+r14]
	mov	eax,DWORD[12+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[16+r14]
	mov	eax,DWORD[20+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	r9d,DWORD[r13]
	xor	r8d,DWORD[4+r13]
	xor	r11d,DWORD[8+r13]
	xor	r10d,DWORD[12+r13]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[24+r14]
	mov	eax,DWORD[28+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[32+r14]
	mov	eax,DWORD[36+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	cmp	r15,128
	jne	NEAR $L$2nd256

	lea	r13,[128+r13]
	shl	r8,32
	shl	r10,32
	or	r8,r9
	or	r10,r11
	mov	rax,QWORD[((-128))+r13]
	mov	rbx,QWORD[((-120))+r13]
	mov	QWORD[((-112))+r13],r8
	mov	QWORD[((-104))+r13],r10
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD[((-96))+r13],rax
	mov	QWORD[((-88))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD[((-80))+r13],r8
	mov	QWORD[((-72))+r13],r10
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD[((-64))+r13],r8
	mov	QWORD[((-56))+r13],r10
	mov	r11,rax
	shl	rax,30
	mov	r9,rbx
	shr	r9,34
	shr	r11,34
	or	rax,r9
	shl	rbx,30
	or	rbx,r11
	mov	QWORD[((-48))+r13],rax
	mov	QWORD[((-40))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD[((-32))+r13],r8
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD[((-24))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD[((-16))+r13],r8
	mov	QWORD[((-8))+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD[r13],rax
	mov	QWORD[8+r13],rbx
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD[16+r13],rax
	mov	QWORD[24+r13],rbx
	mov	r11,r8
	shl	r8,34
	mov	r9,r10
	shr	r9,30
	shr	r11,30
	or	r8,r9
	shl	r10,34
	or	r10,r11
	mov	QWORD[32+r13],r8
	mov	QWORD[40+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD[48+r13],rax
	mov	QWORD[56+r13],rbx
	mov	r11,r8
	shl	r8,17
	mov	r9,r10
	shr	r9,47
	shr	r11,47
	or	r8,r9
	shl	r10,17
	or	r10,r11
	mov	QWORD[64+r13],r8
	mov	QWORD[72+r13],r10
	mov	eax,3
	jmp	NEAR $L$done
ALIGN	16
$L$2nd256:
	mov	DWORD[48+r13],r9d
	mov	DWORD[52+r13],r8d
	mov	DWORD[56+r13],r11d
	mov	DWORD[60+r13],r10d
	xor	r9d,DWORD[32+r13]
	xor	r8d,DWORD[36+r13]
	xor	r11d,DWORD[40+r13]
	xor	r10d,DWORD[44+r13]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[40+r14]
	mov	eax,DWORD[44+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD[2052+rsi*8+rbp]
	mov	ecx,DWORD[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD[rsi*8+rbp]
	xor	ecx,DWORD[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD[2048+rsi*8+rbp]
	xor	ecx,DWORD[2048+rdi*8+rbp]
	mov	ebx,DWORD[48+r14]
	mov	eax,DWORD[52+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	mov	rax,QWORD[r13]
	mov	rbx,QWORD[8+r13]
	mov	rcx,QWORD[32+r13]
	mov	rdx,QWORD[40+r13]
	mov	r14,QWORD[48+r13]
	mov	r15,QWORD[56+r13]
	lea	r13,[128+r13]
	shl	r8,32
	shl	r10,32
	or	r8,r9
	or	r10,r11
	mov	QWORD[((-112))+r13],r8
	mov	QWORD[((-104))+r13],r10
	mov	r11,rcx
	shl	rcx,15
	mov	r9,rdx
	shr	r9,49
	shr	r11,49
	or	rcx,r9
	shl	rdx,15
	or	rdx,r11
	mov	QWORD[((-96))+r13],rcx
	mov	QWORD[((-88))+r13],rdx
	mov	r11,r14
	shl	r14,15
	mov	r9,r15
	shr	r9,49
	shr	r11,49
	or	r14,r9
	shl	r15,15
	or	r15,r11
	mov	QWORD[((-80))+r13],r14
	mov	QWORD[((-72))+r13],r15
	mov	r11,rcx
	shl	rcx,15
	mov	r9,rdx
	shr	r9,49
	shr	r11,49
	or	rcx,r9
	shl	rdx,15
	or	rdx,r11
	mov	QWORD[((-64))+r13],rcx
	mov	QWORD[((-56))+r13],rdx
	mov	r11,r8
	shl	r8,30
	mov	r9,r10
	shr	r9,34
	shr	r11,34
	or	r8,r9
	shl	r10,30
	or	r10,r11
	mov	QWORD[((-48))+r13],r8
	mov	QWORD[((-40))+r13],r10
	mov	r11,rax
	shl	rax,45
	mov	r9,rbx
	shr	r9,19
	shr	r11,19
	or	rax,r9
	shl	rbx,45
	or	rbx,r11
	mov	QWORD[((-32))+r13],rax
	mov	QWORD[((-24))+r13],rbx
	mov	r11,r14
	shl	r14,30
	mov	r9,r15
	shr	r9,34
	shr	r11,34
	or	r14,r9
	shl	r15,30
	or	r15,r11
	mov	QWORD[((-16))+r13],r14
	mov	QWORD[((-8))+r13],r15
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD[r13],rax
	mov	QWORD[8+r13],rbx
	mov	r11,rcx
	shl	rcx,30
	mov	r9,rdx
	shr	r9,34
	shr	r11,34
	or	rcx,r9
	shl	rdx,30
	or	rdx,r11
	mov	QWORD[16+r13],rcx
	mov	QWORD[24+r13],rdx
	mov	r11,r8
	shl	r8,30
	mov	r9,r10
	shr	r9,34
	shr	r11,34
	or	r8,r9
	shl	r10,30
	or	r10,r11
	mov	QWORD[32+r13],r8
	mov	QWORD[40+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD[48+r13],rax
	mov	QWORD[56+r13],rbx
	mov	r11,r14
	shl	r14,32
	mov	r9,r15
	shr	r9,32
	shr	r11,32
	or	r14,r9
	shl	r15,32
	or	r15,r11
	mov	QWORD[64+r13],r14
	mov	QWORD[72+r13],r15
	mov	r11,rcx
	shl	rcx,34
	mov	r9,rdx
	shr	r9,30
	shr	r11,30
	or	rcx,r9
	shl	rdx,34
	or	rdx,r11
	mov	QWORD[80+r13],rcx
	mov	QWORD[88+r13],rdx
	mov	r11,r14
	shl	r14,17
	mov	r9,r15
	shr	r9,47
	shr	r11,47
	or	r14,r9
	shl	r15,17
	or	r15,r11
	mov	QWORD[96+r13],r14
	mov	QWORD[104+r13],r15
	mov	r11,rax
	shl	rax,34
	mov	r9,rbx
	shr	r9,30
	shr	r11,30
	or	rax,r9
	shl	rbx,34
	or	rbx,r11
	mov	QWORD[112+r13],rax
	mov	QWORD[120+r13],rbx
	mov	r11,r8
	shl	r8,51
	mov	r9,r10
	shr	r9,13
	shr	r11,13
	or	r8,r9
	shl	r10,51
	or	r10,r11
	mov	QWORD[128+r13],r8
	mov	QWORD[136+r13],r10
	mov	eax,4
$L$done:
	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	rbp,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	lea	rsp,[40+rsp]

$L$key_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_Camellia_Ekeygen:
ALIGN	64
$L$Camellia_SIGMA:
	DD	0x3bcc908b,0xa09e667f,0x4caa73b2,0xb67ae858
	DD	0xe94f82be,0xc6ef372f,0xf1d36f1c,0x54ff53a5
	DD	0xde682d1d,0x10e527fa,0xb3e6c1fd,0xb05688c2
	DD	0,0,0,0
$L$Camellia_SBOX:
	DD	0x70707000,0x70700070
	DD	0x82828200,0x2c2c002c
	DD	0x2c2c2c00,0xb3b300b3
	DD	0xececec00,0xc0c000c0
	DD	0xb3b3b300,0xe4e400e4
	DD	0x27272700,0x57570057
	DD	0xc0c0c000,0xeaea00ea
	DD	0xe5e5e500,0xaeae00ae
	DD	0xe4e4e400,0x23230023
	DD	0x85858500,0x6b6b006b
	DD	0x57575700,0x45450045
	DD	0x35353500,0xa5a500a5
	DD	0xeaeaea00,0xeded00ed
	DD	0x0c0c0c00,0x4f4f004f
	DD	0xaeaeae00,0x1d1d001d
	DD	0x41414100,0x92920092
	DD	0x23232300,0x86860086
	DD	0xefefef00,0xafaf00af
	DD	0x6b6b6b00,0x7c7c007c
	DD	0x93939300,0x1f1f001f
	DD	0x45454500,0x3e3e003e
	DD	0x19191900,0xdcdc00dc
	DD	0xa5a5a500,0x5e5e005e
	DD	0x21212100,0x0b0b000b
	DD	0xededed00,0xa6a600a6
	DD	0x0e0e0e00,0x39390039
	DD	0x4f4f4f00,0xd5d500d5
	DD	0x4e4e4e00,0x5d5d005d
	DD	0x1d1d1d00,0xd9d900d9
	DD	0x65656500,0x5a5a005a
	DD	0x92929200,0x51510051
	DD	0xbdbdbd00,0x6c6c006c
	DD	0x86868600,0x8b8b008b
	DD	0xb8b8b800,0x9a9a009a
	DD	0xafafaf00,0xfbfb00fb
	DD	0x8f8f8f00,0xb0b000b0
	DD	0x7c7c7c00,0x74740074
	DD	0xebebeb00,0x2b2b002b
	DD	0x1f1f1f00,0xf0f000f0
	DD	0xcecece00,0x84840084
	DD	0x3e3e3e00,0xdfdf00df
	DD	0x30303000,0xcbcb00cb
	DD	0xdcdcdc00,0x34340034
	DD	0x5f5f5f00,0x76760076
	DD	0x5e5e5e00,0x6d6d006d
	DD	0xc5c5c500,0xa9a900a9
	DD	0x0b0b0b00,0xd1d100d1
	DD	0x1a1a1a00,0x04040004
	DD	0xa6a6a600,0x14140014
	DD	0xe1e1e100,0x3a3a003a
	DD	0x39393900,0xdede00de
	DD	0xcacaca00,0x11110011
	DD	0xd5d5d500,0x32320032
	DD	0x47474700,0x9c9c009c
	DD	0x5d5d5d00,0x53530053
	DD	0x3d3d3d00,0xf2f200f2
	DD	0xd9d9d900,0xfefe00fe
	DD	0x01010100,0xcfcf00cf
	DD	0x5a5a5a00,0xc3c300c3
	DD	0xd6d6d600,0x7a7a007a
	DD	0x51515100,0x24240024
	DD	0x56565600,0xe8e800e8
	DD	0x6c6c6c00,0x60600060
	DD	0x4d4d4d00,0x69690069
	DD	0x8b8b8b00,0xaaaa00aa
	DD	0x0d0d0d00,0xa0a000a0
	DD	0x9a9a9a00,0xa1a100a1
	DD	0x66666600,0x62620062
	DD	0xfbfbfb00,0x54540054
	DD	0xcccccc00,0x1e1e001e
	DD	0xb0b0b000,0xe0e000e0
	DD	0x2d2d2d00,0x64640064
	DD	0x74747400,0x10100010
	DD	0x12121200,0x00000000
	DD	0x2b2b2b00,0xa3a300a3
	DD	0x20202000,0x75750075
	DD	0xf0f0f000,0x8a8a008a
	DD	0xb1b1b100,0xe6e600e6
	DD	0x84848400,0x09090009
	DD	0x99999900,0xdddd00dd
	DD	0xdfdfdf00,0x87870087
	DD	0x4c4c4c00,0x83830083
	DD	0xcbcbcb00,0xcdcd00cd
	DD	0xc2c2c200,0x90900090
	DD	0x34343400,0x73730073
	DD	0x7e7e7e00,0xf6f600f6
	DD	0x76767600,0x9d9d009d
	DD	0x05050500,0xbfbf00bf
	DD	0x6d6d6d00,0x52520052
	DD	0xb7b7b700,0xd8d800d8
	DD	0xa9a9a900,0xc8c800c8
	DD	0x31313100,0xc6c600c6
	DD	0xd1d1d100,0x81810081
	DD	0x17171700,0x6f6f006f
	DD	0x04040400,0x13130013
	DD	0xd7d7d700,0x63630063
	DD	0x14141400,0xe9e900e9
	DD	0x58585800,0xa7a700a7
	DD	0x3a3a3a00,0x9f9f009f
	DD	0x61616100,0xbcbc00bc
	DD	0xdedede00,0x29290029
	DD	0x1b1b1b00,0xf9f900f9
	DD	0x11111100,0x2f2f002f
	DD	0x1c1c1c00,0xb4b400b4
	DD	0x32323200,0x78780078
	DD	0x0f0f0f00,0x06060006
	DD	0x9c9c9c00,0xe7e700e7
	DD	0x16161600,0x71710071
	DD	0x53535300,0xd4d400d4
	DD	0x18181800,0xabab00ab
	DD	0xf2f2f200,0x88880088
	DD	0x22222200,0x8d8d008d
	DD	0xfefefe00,0x72720072
	DD	0x44444400,0xb9b900b9
	DD	0xcfcfcf00,0xf8f800f8
	DD	0xb2b2b200,0xacac00ac
	DD	0xc3c3c300,0x36360036
	DD	0xb5b5b500,0x2a2a002a
	DD	0x7a7a7a00,0x3c3c003c
	DD	0x91919100,0xf1f100f1
	DD	0x24242400,0x40400040
	DD	0x08080800,0xd3d300d3
	DD	0xe8e8e800,0xbbbb00bb
	DD	0xa8a8a800,0x43430043
	DD	0x60606000,0x15150015
	DD	0xfcfcfc00,0xadad00ad
	DD	0x69696900,0x77770077
	DD	0x50505000,0x80800080
	DD	0xaaaaaa00,0x82820082
	DD	0xd0d0d000,0xecec00ec
	DD	0xa0a0a000,0x27270027
	DD	0x7d7d7d00,0xe5e500e5
	DD	0xa1a1a100,0x85850085
	DD	0x89898900,0x35350035
	DD	0x62626200,0x0c0c000c
	DD	0x97979700,0x41410041
	DD	0x54545400,0xefef00ef
	DD	0x5b5b5b00,0x93930093
	DD	0x1e1e1e00,0x19190019
	DD	0x95959500,0x21210021
	DD	0xe0e0e000,0x0e0e000e
	DD	0xffffff00,0x4e4e004e
	DD	0x64646400,0x65650065
	DD	0xd2d2d200,0xbdbd00bd
	DD	0x10101000,0xb8b800b8
	DD	0xc4c4c400,0x8f8f008f
	DD	0x00000000,0xebeb00eb
	DD	0x48484800,0xcece00ce
	DD	0xa3a3a300,0x30300030
	DD	0xf7f7f700,0x5f5f005f
	DD	0x75757500,0xc5c500c5
	DD	0xdbdbdb00,0x1a1a001a
	DD	0x8a8a8a00,0xe1e100e1
	DD	0x03030300,0xcaca00ca
	DD	0xe6e6e600,0x47470047
	DD	0xdadada00,0x3d3d003d
	DD	0x09090900,0x01010001
	DD	0x3f3f3f00,0xd6d600d6
	DD	0xdddddd00,0x56560056
	DD	0x94949400,0x4d4d004d
	DD	0x87878700,0x0d0d000d
	DD	0x5c5c5c00,0x66660066
	DD	0x83838300,0xcccc00cc
	DD	0x02020200,0x2d2d002d
	DD	0xcdcdcd00,0x12120012
	DD	0x4a4a4a00,0x20200020
	DD	0x90909000,0xb1b100b1
	DD	0x33333300,0x99990099
	DD	0x73737300,0x4c4c004c
	DD	0x67676700,0xc2c200c2
	DD	0xf6f6f600,0x7e7e007e
	DD	0xf3f3f300,0x05050005
	DD	0x9d9d9d00,0xb7b700b7
	DD	0x7f7f7f00,0x31310031
	DD	0xbfbfbf00,0x17170017
	DD	0xe2e2e200,0xd7d700d7
	DD	0x52525200,0x58580058
	DD	0x9b9b9b00,0x61610061
	DD	0xd8d8d800,0x1b1b001b
	DD	0x26262600,0x1c1c001c
	DD	0xc8c8c800,0x0f0f000f
	DD	0x37373700,0x16160016
	DD	0xc6c6c600,0x18180018
	DD	0x3b3b3b00,0x22220022
	DD	0x81818100,0x44440044
	DD	0x96969600,0xb2b200b2
	DD	0x6f6f6f00,0xb5b500b5
	DD	0x4b4b4b00,0x91910091
	DD	0x13131300,0x08080008
	DD	0xbebebe00,0xa8a800a8
	DD	0x63636300,0xfcfc00fc
	DD	0x2e2e2e00,0x50500050
	DD	0xe9e9e900,0xd0d000d0
	DD	0x79797900,0x7d7d007d
	DD	0xa7a7a700,0x89890089
	DD	0x8c8c8c00,0x97970097
	DD	0x9f9f9f00,0x5b5b005b
	DD	0x6e6e6e00,0x95950095
	DD	0xbcbcbc00,0xffff00ff
	DD	0x8e8e8e00,0xd2d200d2
	DD	0x29292900,0xc4c400c4
	DD	0xf5f5f500,0x48480048
	DD	0xf9f9f900,0xf7f700f7
	DD	0xb6b6b600,0xdbdb00db
	DD	0x2f2f2f00,0x03030003
	DD	0xfdfdfd00,0xdada00da
	DD	0xb4b4b400,0x3f3f003f
	DD	0x59595900,0x94940094
	DD	0x78787800,0x5c5c005c
	DD	0x98989800,0x02020002
	DD	0x06060600,0x4a4a004a
	DD	0x6a6a6a00,0x33330033
	DD	0xe7e7e700,0x67670067
	DD	0x46464600,0xf3f300f3
	DD	0x71717100,0x7f7f007f
	DD	0xbababa00,0xe2e200e2
	DD	0xd4d4d400,0x9b9b009b
	DD	0x25252500,0x26260026
	DD	0xababab00,0x37370037
	DD	0x42424200,0x3b3b003b
	DD	0x88888800,0x96960096
	DD	0xa2a2a200,0x4b4b004b
	DD	0x8d8d8d00,0xbebe00be
	DD	0xfafafa00,0x2e2e002e
	DD	0x72727200,0x79790079
	DD	0x07070700,0x8c8c008c
	DD	0xb9b9b900,0x6e6e006e
	DD	0x55555500,0x8e8e008e
	DD	0xf8f8f800,0xf5f500f5
	DD	0xeeeeee00,0xb6b600b6
	DD	0xacacac00,0xfdfd00fd
	DD	0x0a0a0a00,0x59590059
	DD	0x36363600,0x98980098
	DD	0x49494900,0x6a6a006a
	DD	0x2a2a2a00,0x46460046
	DD	0x68686800,0xbaba00ba
	DD	0x3c3c3c00,0x25250025
	DD	0x38383800,0x42420042
	DD	0xf1f1f100,0xa2a200a2
	DD	0xa4a4a400,0xfafa00fa
	DD	0x40404000,0x07070007
	DD	0x28282800,0x55550055
	DD	0xd3d3d300,0xeeee00ee
	DD	0x7b7b7b00,0x0a0a000a
	DD	0xbbbbbb00,0x49490049
	DD	0xc9c9c900,0x68680068
	DD	0x43434300,0x38380038
	DD	0xc1c1c100,0xa4a400a4
	DD	0x15151500,0x28280028
	DD	0xe3e3e300,0x7b7b007b
	DD	0xadadad00,0xc9c900c9
	DD	0xf4f4f400,0xc1c100c1
	DD	0x77777700,0xe3e300e3
	DD	0xc7c7c700,0xf4f400f4
	DD	0x80808000,0xc7c700c7
	DD	0x9e9e9e00,0x9e9e009e
	DD	0x00e0e0e0,0x38003838
	DD	0x00050505,0x41004141
	DD	0x00585858,0x16001616
	DD	0x00d9d9d9,0x76007676
	DD	0x00676767,0xd900d9d9
	DD	0x004e4e4e,0x93009393
	DD	0x00818181,0x60006060
	DD	0x00cbcbcb,0xf200f2f2
	DD	0x00c9c9c9,0x72007272
	DD	0x000b0b0b,0xc200c2c2
	DD	0x00aeaeae,0xab00abab
	DD	0x006a6a6a,0x9a009a9a
	DD	0x00d5d5d5,0x75007575
	DD	0x00181818,0x06000606
	DD	0x005d5d5d,0x57005757
	DD	0x00828282,0xa000a0a0
	DD	0x00464646,0x91009191
	DD	0x00dfdfdf,0xf700f7f7
	DD	0x00d6d6d6,0xb500b5b5
	DD	0x00272727,0xc900c9c9
	DD	0x008a8a8a,0xa200a2a2
	DD	0x00323232,0x8c008c8c
	DD	0x004b4b4b,0xd200d2d2
	DD	0x00424242,0x90009090
	DD	0x00dbdbdb,0xf600f6f6
	DD	0x001c1c1c,0x07000707
	DD	0x009e9e9e,0xa700a7a7
	DD	0x009c9c9c,0x27002727
	DD	0x003a3a3a,0x8e008e8e
	DD	0x00cacaca,0xb200b2b2
	DD	0x00252525,0x49004949
	DD	0x007b7b7b,0xde00dede
	DD	0x000d0d0d,0x43004343
	DD	0x00717171,0x5c005c5c
	DD	0x005f5f5f,0xd700d7d7
	DD	0x001f1f1f,0xc700c7c7
	DD	0x00f8f8f8,0x3e003e3e
	DD	0x00d7d7d7,0xf500f5f5
	DD	0x003e3e3e,0x8f008f8f
	DD	0x009d9d9d,0x67006767
	DD	0x007c7c7c,0x1f001f1f
	DD	0x00606060,0x18001818
	DD	0x00b9b9b9,0x6e006e6e
	DD	0x00bebebe,0xaf00afaf
	DD	0x00bcbcbc,0x2f002f2f
	DD	0x008b8b8b,0xe200e2e2
	DD	0x00161616,0x85008585
	DD	0x00343434,0x0d000d0d
	DD	0x004d4d4d,0x53005353
	DD	0x00c3c3c3,0xf000f0f0
	DD	0x00727272,0x9c009c9c
	DD	0x00959595,0x65006565
	DD	0x00ababab,0xea00eaea
	DD	0x008e8e8e,0xa300a3a3
	DD	0x00bababa,0xae00aeae
	DD	0x007a7a7a,0x9e009e9e
	DD	0x00b3b3b3,0xec00ecec
	DD	0x00020202,0x80008080
	DD	0x00b4b4b4,0x2d002d2d
	DD	0x00adadad,0x6b006b6b
	DD	0x00a2a2a2,0xa800a8a8
	DD	0x00acacac,0x2b002b2b
	DD	0x00d8d8d8,0x36003636
	DD	0x009a9a9a,0xa600a6a6
	DD	0x00171717,0xc500c5c5
	DD	0x001a1a1a,0x86008686
	DD	0x00353535,0x4d004d4d
	DD	0x00cccccc,0x33003333
	DD	0x00f7f7f7,0xfd00fdfd
	DD	0x00999999,0x66006666
	DD	0x00616161,0x58005858
	DD	0x005a5a5a,0x96009696
	DD	0x00e8e8e8,0x3a003a3a
	DD	0x00242424,0x09000909
	DD	0x00565656,0x95009595
	DD	0x00404040,0x10001010
	DD	0x00e1e1e1,0x78007878
	DD	0x00636363,0xd800d8d8
	DD	0x00090909,0x42004242
	DD	0x00333333,0xcc00cccc
	DD	0x00bfbfbf,0xef00efef
	DD	0x00989898,0x26002626
	DD	0x00979797,0xe500e5e5
	DD	0x00858585,0x61006161
	DD	0x00686868,0x1a001a1a
	DD	0x00fcfcfc,0x3f003f3f
	DD	0x00ececec,0x3b003b3b
	DD	0x000a0a0a,0x82008282
	DD	0x00dadada,0xb600b6b6
	DD	0x006f6f6f,0xdb00dbdb
	DD	0x00535353,0xd400d4d4
	DD	0x00626262,0x98009898
	DD	0x00a3a3a3,0xe800e8e8
	DD	0x002e2e2e,0x8b008b8b
	DD	0x00080808,0x02000202
	DD	0x00afafaf,0xeb00ebeb
	DD	0x00282828,0x0a000a0a
	DD	0x00b0b0b0,0x2c002c2c
	DD	0x00747474,0x1d001d1d
	DD	0x00c2c2c2,0xb000b0b0
	DD	0x00bdbdbd,0x6f006f6f
	DD	0x00363636,0x8d008d8d
	DD	0x00222222,0x88008888
	DD	0x00383838,0x0e000e0e
	DD	0x00646464,0x19001919
	DD	0x001e1e1e,0x87008787
	DD	0x00393939,0x4e004e4e
	DD	0x002c2c2c,0x0b000b0b
	DD	0x00a6a6a6,0xa900a9a9
	DD	0x00303030,0x0c000c0c
	DD	0x00e5e5e5,0x79007979
	DD	0x00444444,0x11001111
	DD	0x00fdfdfd,0x7f007f7f
	DD	0x00888888,0x22002222
	DD	0x009f9f9f,0xe700e7e7
	DD	0x00656565,0x59005959
	DD	0x00878787,0xe100e1e1
	DD	0x006b6b6b,0xda00dada
	DD	0x00f4f4f4,0x3d003d3d
	DD	0x00232323,0xc800c8c8
	DD	0x00484848,0x12001212
	DD	0x00101010,0x04000404
	DD	0x00d1d1d1,0x74007474
	DD	0x00515151,0x54005454
	DD	0x00c0c0c0,0x30003030
	DD	0x00f9f9f9,0x7e007e7e
	DD	0x00d2d2d2,0xb400b4b4
	DD	0x00a0a0a0,0x28002828
	DD	0x00555555,0x55005555
	DD	0x00a1a1a1,0x68006868
	DD	0x00414141,0x50005050
	DD	0x00fafafa,0xbe00bebe
	DD	0x00434343,0xd000d0d0
	DD	0x00131313,0xc400c4c4
	DD	0x00c4c4c4,0x31003131
	DD	0x002f2f2f,0xcb00cbcb
	DD	0x00a8a8a8,0x2a002a2a
	DD	0x00b6b6b6,0xad00adad
	DD	0x003c3c3c,0x0f000f0f
	DD	0x002b2b2b,0xca00caca
	DD	0x00c1c1c1,0x70007070
	DD	0x00ffffff,0xff00ffff
	DD	0x00c8c8c8,0x32003232
	DD	0x00a5a5a5,0x69006969
	DD	0x00202020,0x08000808
	DD	0x00898989,0x62006262
	DD	0x00000000,0x00000000
	DD	0x00909090,0x24002424
	DD	0x00474747,0xd100d1d1
	DD	0x00efefef,0xfb00fbfb
	DD	0x00eaeaea,0xba00baba
	DD	0x00b7b7b7,0xed00eded
	DD	0x00151515,0x45004545
	DD	0x00060606,0x81008181
	DD	0x00cdcdcd,0x73007373
	DD	0x00b5b5b5,0x6d006d6d
	DD	0x00121212,0x84008484
	DD	0x007e7e7e,0x9f009f9f
	DD	0x00bbbbbb,0xee00eeee
	DD	0x00292929,0x4a004a4a
	DD	0x000f0f0f,0xc300c3c3
	DD	0x00b8b8b8,0x2e002e2e
	DD	0x00070707,0xc100c1c1
	DD	0x00040404,0x01000101
	DD	0x009b9b9b,0xe600e6e6
	DD	0x00949494,0x25002525
	DD	0x00212121,0x48004848
	DD	0x00666666,0x99009999
	DD	0x00e6e6e6,0xb900b9b9
	DD	0x00cecece,0xb300b3b3
	DD	0x00ededed,0x7b007b7b
	DD	0x00e7e7e7,0xf900f9f9
	DD	0x003b3b3b,0xce00cece
	DD	0x00fefefe,0xbf00bfbf
	DD	0x007f7f7f,0xdf00dfdf
	DD	0x00c5c5c5,0x71007171
	DD	0x00a4a4a4,0x29002929
	DD	0x00373737,0xcd00cdcd
	DD	0x00b1b1b1,0x6c006c6c
	DD	0x004c4c4c,0x13001313
	DD	0x00919191,0x64006464
	DD	0x006e6e6e,0x9b009b9b
	DD	0x008d8d8d,0x63006363
	DD	0x00767676,0x9d009d9d
	DD	0x00030303,0xc000c0c0
	DD	0x002d2d2d,0x4b004b4b
	DD	0x00dedede,0xb700b7b7
	DD	0x00969696,0xa500a5a5
	DD	0x00262626,0x89008989
	DD	0x007d7d7d,0x5f005f5f
	DD	0x00c6c6c6,0xb100b1b1
	DD	0x005c5c5c,0x17001717
	DD	0x00d3d3d3,0xf400f4f4
	DD	0x00f2f2f2,0xbc00bcbc
	DD	0x004f4f4f,0xd300d3d3
	DD	0x00191919,0x46004646
	DD	0x003f3f3f,0xcf00cfcf
	DD	0x00dcdcdc,0x37003737
	DD	0x00797979,0x5e005e5e
	DD	0x001d1d1d,0x47004747
	DD	0x00525252,0x94009494
	DD	0x00ebebeb,0xfa00fafa
	DD	0x00f3f3f3,0xfc00fcfc
	DD	0x006d6d6d,0x5b005b5b
	DD	0x005e5e5e,0x97009797
	DD	0x00fbfbfb,0xfe00fefe
	DD	0x00696969,0x5a005a5a
	DD	0x00b2b2b2,0xac00acac
	DD	0x00f0f0f0,0x3c003c3c
	DD	0x00313131,0x4c004c4c
	DD	0x000c0c0c,0x03000303
	DD	0x00d4d4d4,0x35003535
	DD	0x00cfcfcf,0xf300f3f3
	DD	0x008c8c8c,0x23002323
	DD	0x00e2e2e2,0xb800b8b8
	DD	0x00757575,0x5d005d5d
	DD	0x00a9a9a9,0x6a006a6a
	DD	0x004a4a4a,0x92009292
	DD	0x00575757,0xd500d5d5
	DD	0x00848484,0x21002121
	DD	0x00111111,0x44004444
	DD	0x00454545,0x51005151
	DD	0x001b1b1b,0xc600c6c6
	DD	0x00f5f5f5,0x7d007d7d
	DD	0x00e4e4e4,0x39003939
	DD	0x000e0e0e,0x83008383
	DD	0x00737373,0xdc00dcdc
	DD	0x00aaaaaa,0xaa00aaaa
	DD	0x00f1f1f1,0x7c007c7c
	DD	0x00dddddd,0x77007777
	DD	0x00595959,0x56005656
	DD	0x00141414,0x05000505
	DD	0x006c6c6c,0x1b001b1b
	DD	0x00929292,0xa400a4a4
	DD	0x00545454,0x15001515
	DD	0x00d0d0d0,0x34003434
	DD	0x00787878,0x1e001e1e
	DD	0x00707070,0x1c001c1c
	DD	0x00e3e3e3,0xf800f8f8
	DD	0x00494949,0x52005252
	DD	0x00808080,0x20002020
	DD	0x00505050,0x14001414
	DD	0x00a7a7a7,0xe900e9e9
	DD	0x00f6f6f6,0xbd00bdbd
	DD	0x00777777,0xdd00dddd
	DD	0x00939393,0xe400e4e4
	DD	0x00868686,0xa100a1a1
	DD	0x00838383,0xe000e0e0
	DD	0x002a2a2a,0x8a008a8a
	DD	0x00c7c7c7,0xf100f1f1
	DD	0x005b5b5b,0xd600d6d6
	DD	0x00e9e9e9,0x7a007a7a
	DD	0x00eeeeee,0xbb00bbbb
	DD	0x008f8f8f,0xe300e3e3
	DD	0x00010101,0x40004040
	DD	0x003d3d3d,0x4f004f4f
global	Camellia_cbc_encrypt

ALIGN	16
Camellia_cbc_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_cbc_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



	cmp	rdx,0
	je	NEAR $L$cbc_abort
	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$cbc_prologue:

	mov	rbp,rsp

	sub	rsp,64
	and	rsp,-64



	lea	r10,[((-64-63))+rcx]
	sub	r10,rsp
	neg	r10
	and	r10,0x3C0
	sub	rsp,r10


	mov	r12,rdi
	mov	r13,rsi
	mov	rbx,r8
	mov	r14,rcx
	mov	r15d,DWORD[272+rcx]

	mov	QWORD[40+rsp],r8
	mov	QWORD[48+rsp],rbp


$L$cbc_body:
	lea	rbp,[$L$Camellia_SBOX]

	mov	ecx,32
ALIGN	4
$L$cbc_prefetch_sbox:
	mov	rax,QWORD[rbp]
	mov	rsi,QWORD[32+rbp]
	mov	rdi,QWORD[64+rbp]
	mov	r11,QWORD[96+rbp]
	lea	rbp,[128+rbp]
	loop	$L$cbc_prefetch_sbox
	sub	rbp,4096
	shl	r15,6
	mov	rcx,rdx
	lea	r15,[r15*1+r14]

	cmp	r9d,0
	je	NEAR $L$CBC_DECRYPT

	and	rdx,-16
	and	rcx,15
	lea	rdx,[rdx*1+r12]
	mov	QWORD[rsp],r14
	mov	QWORD[8+rsp],rdx
	mov	QWORD[16+rsp],rcx

	cmp	rdx,r12
	mov	r8d,DWORD[rbx]
	mov	r9d,DWORD[4+rbx]
	mov	r10d,DWORD[8+rbx]
	mov	r11d,DWORD[12+rbx]
	je	NEAR $L$cbc_enc_tail
	jmp	NEAR $L$cbc_eloop

ALIGN	16
$L$cbc_eloop:
	xor	r8d,DWORD[r12]
	xor	r9d,DWORD[4+r12]
	xor	r10d,DWORD[8+r12]
	bswap	r8d
	xor	r11d,DWORD[12+r12]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_encrypt

	mov	r14,QWORD[rsp]
	bswap	r8d
	mov	rdx,QWORD[8+rsp]
	bswap	r9d
	mov	rcx,QWORD[16+rsp]
	bswap	r10d
	mov	DWORD[r13],r8d
	bswap	r11d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	lea	r12,[16+r12]
	mov	DWORD[12+r13],r11d
	cmp	r12,rdx
	lea	r13,[16+r13]
	jne	NEAR $L$cbc_eloop

	cmp	rcx,0
	jne	NEAR $L$cbc_enc_tail

	mov	r13,QWORD[40+rsp]
	mov	DWORD[r13],r8d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	mov	DWORD[12+r13],r11d
	jmp	NEAR $L$cbc_done

ALIGN	16
$L$cbc_enc_tail:
	xor	rax,rax
	mov	QWORD[((0+24))+rsp],rax
	mov	QWORD[((8+24))+rsp],rax
	mov	QWORD[16+rsp],rax

$L$cbc_enc_pushf:
	pushfq
	cld
	mov	rsi,r12
	lea	rdi,[((8+24))+rsp]
	DD	0x9066A4F3
	popfq
$L$cbc_enc_popf:

	lea	r12,[24+rsp]
	lea	rax,[((16+24))+rsp]
	mov	QWORD[8+rsp],rax
	jmp	NEAR $L$cbc_eloop

ALIGN	16
$L$CBC_DECRYPT:
	xchg	r15,r14
	add	rdx,15
	and	rcx,15
	and	rdx,-16
	mov	QWORD[rsp],r14
	lea	rdx,[rdx*1+r12]
	mov	QWORD[8+rsp],rdx
	mov	QWORD[16+rsp],rcx

	mov	rax,QWORD[rbx]
	mov	rbx,QWORD[8+rbx]
	jmp	NEAR $L$cbc_dloop
ALIGN	16
$L$cbc_dloop:
	mov	r8d,DWORD[r12]
	mov	r9d,DWORD[4+r12]
	mov	r10d,DWORD[8+r12]
	bswap	r8d
	mov	r11d,DWORD[12+r12]
	bswap	r9d
	mov	QWORD[((0+24))+rsp],rax
	bswap	r10d
	mov	QWORD[((8+24))+rsp],rbx
	bswap	r11d

	call	_x86_64_Camellia_decrypt

	mov	r14,QWORD[rsp]
	mov	rdx,QWORD[8+rsp]
	mov	rcx,QWORD[16+rsp]

	bswap	r8d
	mov	rax,QWORD[r12]
	bswap	r9d
	mov	rbx,QWORD[8+r12]
	bswap	r10d
	xor	r8d,DWORD[((0+24))+rsp]
	bswap	r11d
	xor	r9d,DWORD[((4+24))+rsp]
	xor	r10d,DWORD[((8+24))+rsp]
	lea	r12,[16+r12]
	xor	r11d,DWORD[((12+24))+rsp]
	cmp	r12,rdx
	je	NEAR $L$cbc_ddone

	mov	DWORD[r13],r8d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	mov	DWORD[12+r13],r11d

	lea	r13,[16+r13]
	jmp	NEAR $L$cbc_dloop

ALIGN	16
$L$cbc_ddone:
	mov	rdx,QWORD[40+rsp]
	cmp	rcx,0
	jne	NEAR $L$cbc_dec_tail

	mov	DWORD[r13],r8d
	mov	DWORD[4+r13],r9d
	mov	DWORD[8+r13],r10d
	mov	DWORD[12+r13],r11d

	mov	QWORD[rdx],rax
	mov	QWORD[8+rdx],rbx
	jmp	NEAR $L$cbc_done
ALIGN	16
$L$cbc_dec_tail:
	mov	DWORD[((0+24))+rsp],r8d
	mov	DWORD[((4+24))+rsp],r9d
	mov	DWORD[((8+24))+rsp],r10d
	mov	DWORD[((12+24))+rsp],r11d

$L$cbc_dec_pushf:
	pushfq
	cld
	lea	rsi,[((8+24))+rsp]
	lea	rdi,[r13]
	DD	0x9066A4F3
	popfq
$L$cbc_dec_popf:

	mov	QWORD[rdx],rax
	mov	QWORD[8+rdx],rbx
	jmp	NEAR $L$cbc_done

ALIGN	16
$L$cbc_done:
	mov	rcx,QWORD[48+rsp]

	mov	r15,QWORD[rcx]

	mov	r14,QWORD[8+rcx]

	mov	r13,QWORD[16+rcx]

	mov	r12,QWORD[24+rcx]

	mov	rbp,QWORD[32+rcx]

	mov	rbx,QWORD[40+rcx]

	lea	rsp,[48+rcx]

$L$cbc_abort:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_Camellia_cbc_encrypt:

DB	67,97,109,101,108,108,105,97,32,102,111,114,32,120,56,54
DB	95,54,52,32,98,121,32,60,97,112,112,114,111,64,111,112
DB	101,110,115,115,108,46,111,114,103,62,0
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
common_se_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	lea	rsp,[((-64))+rsp]

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

	lea	rax,[40+rax]
	mov	rbx,QWORD[((-8))+rax]
	mov	rbp,QWORD[((-16))+rax]
	mov	r13,QWORD[((-24))+rax]
	mov	r14,QWORD[((-32))+rax]
	mov	r15,QWORD[((-40))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

$L$in_prologue:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi

	jmp	NEAR $L$common_seh_exit



ALIGN	16
cbc_se_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	lea	rsp,[((-64))+rsp]

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	lea	r10,[$L$cbc_prologue]
	cmp	rbx,r10
	jb	NEAR $L$in_cbc_prologue

	lea	r10,[$L$cbc_body]
	cmp	rbx,r10
	jb	NEAR $L$in_cbc_frame_setup

	mov	rax,QWORD[152+r8]

	lea	r10,[$L$cbc_abort]
	cmp	rbx,r10
	jae	NEAR $L$in_cbc_prologue


	lea	r10,[$L$cbc_enc_pushf]
	cmp	rbx,r10
	jbe	NEAR $L$in_cbc_no_flag
	lea	rax,[8+rax]
	lea	r10,[$L$cbc_enc_popf]
	cmp	rbx,r10
	jb	NEAR $L$in_cbc_no_flag
	lea	rax,[((-8))+rax]
	lea	r10,[$L$cbc_dec_pushf]
	cmp	rbx,r10
	jbe	NEAR $L$in_cbc_no_flag
	lea	rax,[8+rax]
	lea	r10,[$L$cbc_dec_popf]
	cmp	rbx,r10
	jb	NEAR $L$in_cbc_no_flag
	lea	rax,[((-8))+rax]

$L$in_cbc_no_flag:
	mov	rax,QWORD[48+rax]
	lea	rax,[48+rax]

$L$in_cbc_frame_setup:
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

$L$in_cbc_prologue:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi

ALIGN	4
$L$common_seh_exit:

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
	lea	rsp,[64+rsp]
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
	DD	$L$SEH_begin_Camellia_EncryptBlock_Rounds wrt ..imagebase
	DD	$L$SEH_end_Camellia_EncryptBlock_Rounds wrt ..imagebase
	DD	$L$SEH_info_Camellia_EncryptBlock_Rounds wrt ..imagebase

	DD	$L$SEH_begin_Camellia_DecryptBlock_Rounds wrt ..imagebase
	DD	$L$SEH_end_Camellia_DecryptBlock_Rounds wrt ..imagebase
	DD	$L$SEH_info_Camellia_DecryptBlock_Rounds wrt ..imagebase

	DD	$L$SEH_begin_Camellia_Ekeygen wrt ..imagebase
	DD	$L$SEH_end_Camellia_Ekeygen wrt ..imagebase
	DD	$L$SEH_info_Camellia_Ekeygen wrt ..imagebase

	DD	$L$SEH_begin_Camellia_cbc_encrypt wrt ..imagebase
	DD	$L$SEH_end_Camellia_cbc_encrypt wrt ..imagebase
	DD	$L$SEH_info_Camellia_cbc_encrypt wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_Camellia_EncryptBlock_Rounds:
DB	9,0,0,0
	DD	common_se_handler wrt ..imagebase
	DD	$L$enc_prologue wrt ..imagebase,$L$enc_epilogue wrt ..imagebase
$L$SEH_info_Camellia_DecryptBlock_Rounds:
DB	9,0,0,0
	DD	common_se_handler wrt ..imagebase
	DD	$L$dec_prologue wrt ..imagebase,$L$dec_epilogue wrt ..imagebase
$L$SEH_info_Camellia_Ekeygen:
DB	9,0,0,0
	DD	common_se_handler wrt ..imagebase
	DD	$L$key_prologue wrt ..imagebase,$L$key_epilogue wrt ..imagebase
$L$SEH_info_Camellia_cbc_encrypt:
DB	9,0,0,0
	DD	cbc_se_handler wrt ..imagebase
