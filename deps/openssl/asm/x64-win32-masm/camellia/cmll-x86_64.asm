OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'


PUBLIC	Camellia_EncryptBlock

ALIGN	16
Camellia_EncryptBlock	PROC PUBLIC
	mov	eax,128
	sub	eax,ecx
	mov	ecx,3
	adc	ecx,0
	jmp	$L$enc_rounds
Camellia_EncryptBlock	ENDP

PUBLIC	Camellia_EncryptBlock_Rounds

ALIGN	16
$L$enc_rounds::
Camellia_EncryptBlock_Rounds	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_EncryptBlock_Rounds::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbx
	push	rbp
	push	r13
	push	r14
	push	r15
$L$enc_prologue::


	mov	r13,rcx
	mov	r14,rdx

	shl	edi,6
	lea	rbp,QWORD PTR[$L$Camellia_SBOX]
	lea	r15,QWORD PTR[rdi*1+r14]

	mov	r8d,DWORD PTR[rsi]
	mov	r9d,DWORD PTR[4+rsi]
	mov	r10d,DWORD PTR[8+rsi]
	bswap	r8d
	mov	r11d,DWORD PTR[12+rsi]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_encrypt

	bswap	r8d
	bswap	r9d
	bswap	r10d
	mov	DWORD PTR[r13],r8d
	bswap	r11d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	mov	DWORD PTR[12+r13],r11d

	mov	r15,QWORD PTR[rsp]
	mov	r14,QWORD PTR[8+rsp]
	mov	r13,QWORD PTR[16+rsp]
	mov	rbp,QWORD PTR[24+rsp]
	mov	rbx,QWORD PTR[32+rsp]
	lea	rsp,QWORD PTR[40+rsp]
$L$enc_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_Camellia_EncryptBlock_Rounds::
Camellia_EncryptBlock_Rounds	ENDP


ALIGN	16
_x86_64_Camellia_encrypt	PROC PRIVATE
	xor	r9d,DWORD PTR[r14]
	xor	r8d,DWORD PTR[4+r14]
	xor	r11d,DWORD PTR[8+r14]
	xor	r10d,DWORD PTR[12+r14]
ALIGN	16
$L$eloop::
	mov	ebx,DWORD PTR[16+r14]
	mov	eax,DWORD PTR[20+r14]

	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[24+r14]
	mov	eax,DWORD PTR[28+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[32+r14]
	mov	eax,DWORD PTR[36+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[40+r14]
	mov	eax,DWORD PTR[44+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[48+r14]
	mov	eax,DWORD PTR[52+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[56+r14]
	mov	eax,DWORD PTR[60+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[64+r14]
	mov	eax,DWORD PTR[68+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	lea	r14,QWORD PTR[64+r14]
	cmp	r14,r15
	mov	edx,DWORD PTR[8+r14]
	mov	ecx,DWORD PTR[12+r14]
	je	$L$edone

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
	jmp	$L$eloop

ALIGN	16
$L$edone::
	xor	eax,r10d
	xor	ebx,r11d
	xor	ecx,r8d
	xor	edx,r9d

	mov	r8d,eax
	mov	r9d,ebx
	mov	r10d,ecx
	mov	r11d,edx

DB	0f3h,0c3h

_x86_64_Camellia_encrypt	ENDP


PUBLIC	Camellia_DecryptBlock

ALIGN	16
Camellia_DecryptBlock	PROC PUBLIC
	mov	eax,128
	sub	eax,ecx
	mov	ecx,3
	adc	ecx,0
	jmp	$L$dec_rounds
Camellia_DecryptBlock	ENDP

PUBLIC	Camellia_DecryptBlock_Rounds

ALIGN	16
$L$dec_rounds::
Camellia_DecryptBlock_Rounds	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_DecryptBlock_Rounds::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbx
	push	rbp
	push	r13
	push	r14
	push	r15
$L$dec_prologue::


	mov	r13,rcx
	mov	r15,rdx

	shl	edi,6
	lea	rbp,QWORD PTR[$L$Camellia_SBOX]
	lea	r14,QWORD PTR[rdi*1+r15]

	mov	r8d,DWORD PTR[rsi]
	mov	r9d,DWORD PTR[4+rsi]
	mov	r10d,DWORD PTR[8+rsi]
	bswap	r8d
	mov	r11d,DWORD PTR[12+rsi]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_decrypt

	bswap	r8d
	bswap	r9d
	bswap	r10d
	mov	DWORD PTR[r13],r8d
	bswap	r11d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	mov	DWORD PTR[12+r13],r11d

	mov	r15,QWORD PTR[rsp]
	mov	r14,QWORD PTR[8+rsp]
	mov	r13,QWORD PTR[16+rsp]
	mov	rbp,QWORD PTR[24+rsp]
	mov	rbx,QWORD PTR[32+rsp]
	lea	rsp,QWORD PTR[40+rsp]
$L$dec_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_Camellia_DecryptBlock_Rounds::
Camellia_DecryptBlock_Rounds	ENDP


ALIGN	16
_x86_64_Camellia_decrypt	PROC PRIVATE
	xor	r9d,DWORD PTR[r14]
	xor	r8d,DWORD PTR[4+r14]
	xor	r11d,DWORD PTR[8+r14]
	xor	r10d,DWORD PTR[12+r14]
ALIGN	16
$L$dloop::
	mov	ebx,DWORD PTR[((-8))+r14]
	mov	eax,DWORD PTR[((-4))+r14]

	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-16))+r14]
	mov	eax,DWORD PTR[((-12))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-24))+r14]
	mov	eax,DWORD PTR[((-20))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-32))+r14]
	mov	eax,DWORD PTR[((-28))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-40))+r14]
	mov	eax,DWORD PTR[((-36))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-48))+r14]
	mov	eax,DWORD PTR[((-44))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[((-56))+r14]
	mov	eax,DWORD PTR[((-52))+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	lea	r14,QWORD PTR[((-64))+r14]
	cmp	r14,r15
	mov	edx,DWORD PTR[r14]
	mov	ecx,DWORD PTR[4+r14]
	je	$L$ddone

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

	jmp	$L$dloop

ALIGN	16
$L$ddone::
	xor	ecx,r10d
	xor	edx,r11d
	xor	eax,r8d
	xor	ebx,r9d

	mov	r8d,ecx
	mov	r9d,edx
	mov	r10d,eax
	mov	r11d,ebx

DB	0f3h,0c3h

_x86_64_Camellia_decrypt	ENDP
PUBLIC	Camellia_Ekeygen

ALIGN	16
Camellia_Ekeygen	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_Ekeygen::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r13
	push	r14
	push	r15
$L$key_prologue::

	mov	r15,rdi
	mov	r13,rdx

	mov	r8d,DWORD PTR[rsi]
	mov	r9d,DWORD PTR[4+rsi]
	mov	r10d,DWORD PTR[8+rsi]
	mov	r11d,DWORD PTR[12+rsi]

	bswap	r8d
	bswap	r9d
	bswap	r10d
	bswap	r11d
	mov	DWORD PTR[r13],r9d
	mov	DWORD PTR[4+r13],r8d
	mov	DWORD PTR[8+r13],r11d
	mov	DWORD PTR[12+r13],r10d
	cmp	r15,128
	je	$L$1st128

	mov	r8d,DWORD PTR[16+rsi]
	mov	r9d,DWORD PTR[20+rsi]
	cmp	r15,192
	je	$L$1st192
	mov	r10d,DWORD PTR[24+rsi]
	mov	r11d,DWORD PTR[28+rsi]
	jmp	$L$1st256
$L$1st192::
	mov	r10d,r8d
	mov	r11d,r9d
	not	r10d
	not	r11d
$L$1st256::
	bswap	r8d
	bswap	r9d
	bswap	r10d
	bswap	r11d
	mov	DWORD PTR[32+r13],r9d
	mov	DWORD PTR[36+r13],r8d
	mov	DWORD PTR[40+r13],r11d
	mov	DWORD PTR[44+r13],r10d
	xor	r9d,DWORD PTR[r13]
	xor	r8d,DWORD PTR[4+r13]
	xor	r11d,DWORD PTR[8+r13]
	xor	r10d,DWORD PTR[12+r13]

$L$1st128::
	lea	r14,QWORD PTR[$L$Camellia_SIGMA]
	lea	rbp,QWORD PTR[$L$Camellia_SBOX]

	mov	ebx,DWORD PTR[r14]
	mov	eax,DWORD PTR[4+r14]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[8+r14]
	mov	eax,DWORD PTR[12+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[16+r14]
	mov	eax,DWORD PTR[20+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	xor	r9d,DWORD PTR[r13]
	xor	r8d,DWORD PTR[4+r13]
	xor	r11d,DWORD PTR[8+r13]
	xor	r10d,DWORD PTR[12+r13]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[24+r14]
	mov	eax,DWORD PTR[28+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[32+r14]
	mov	eax,DWORD PTR[36+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	cmp	r15,128
	jne	$L$2nd256

	lea	r13,QWORD PTR[128+r13]
	shl	r8,32
	shl	r10,32
	or	r8,r9
	or	r10,r11
	mov	rax,QWORD PTR[((-128))+r13]
	mov	rbx,QWORD PTR[((-120))+r13]
	mov	QWORD PTR[((-112))+r13],r8
	mov	QWORD PTR[((-104))+r13],r10
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD PTR[((-96))+r13],rax
	mov	QWORD PTR[((-88))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD PTR[((-80))+r13],r8
	mov	QWORD PTR[((-72))+r13],r10
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD PTR[((-64))+r13],r8
	mov	QWORD PTR[((-56))+r13],r10
	mov	r11,rax
	shl	rax,30
	mov	r9,rbx
	shr	r9,34
	shr	r11,34
	or	rax,r9
	shl	rbx,30
	or	rbx,r11
	mov	QWORD PTR[((-48))+r13],rax
	mov	QWORD PTR[((-40))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD PTR[((-32))+r13],r8
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD PTR[((-24))+r13],rbx
	mov	r11,r8
	shl	r8,15
	mov	r9,r10
	shr	r9,49
	shr	r11,49
	or	r8,r9
	shl	r10,15
	or	r10,r11
	mov	QWORD PTR[((-16))+r13],r8
	mov	QWORD PTR[((-8))+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD PTR[r13],rax
	mov	QWORD PTR[8+r13],rbx
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD PTR[16+r13],rax
	mov	QWORD PTR[24+r13],rbx
	mov	r11,r8
	shl	r8,34
	mov	r9,r10
	shr	r9,30
	shr	r11,30
	or	r8,r9
	shl	r10,34
	or	r10,r11
	mov	QWORD PTR[32+r13],r8
	mov	QWORD PTR[40+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD PTR[48+r13],rax
	mov	QWORD PTR[56+r13],rbx
	mov	r11,r8
	shl	r8,17
	mov	r9,r10
	shr	r9,47
	shr	r11,47
	or	r8,r9
	shl	r10,17
	or	r10,r11
	mov	QWORD PTR[64+r13],r8
	mov	QWORD PTR[72+r13],r10
	mov	eax,3
	jmp	$L$done
ALIGN	16
$L$2nd256::
	mov	DWORD PTR[48+r13],r9d
	mov	DWORD PTR[52+r13],r8d
	mov	DWORD PTR[56+r13],r11d
	mov	DWORD PTR[60+r13],r10d
	xor	r9d,DWORD PTR[32+r13]
	xor	r8d,DWORD PTR[36+r13]
	xor	r11d,DWORD PTR[40+r13]
	xor	r10d,DWORD PTR[44+r13]
	xor	eax,r8d
	xor	ebx,r9d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[40+r14]
	mov	eax,DWORD PTR[44+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r10d,ecx
	xor	r11d,ecx
	xor	r11d,edx
	xor	eax,r10d
	xor	ebx,r11d
	movzx	esi,ah
	movzx	edi,bl
	mov	edx,DWORD PTR[2052+rsi*8+rbp]
	mov	ecx,DWORD PTR[rdi*8+rbp]
	movzx	esi,al
	shr	eax,16
	movzx	edi,bh
	xor	edx,DWORD PTR[4+rsi*8+rbp]
	shr	ebx,16
	xor	ecx,DWORD PTR[4+rdi*8+rbp]
	movzx	esi,ah
	movzx	edi,bl
	xor	edx,DWORD PTR[rsi*8+rbp]
	xor	ecx,DWORD PTR[2052+rdi*8+rbp]
	movzx	esi,al
	movzx	edi,bh
	xor	edx,DWORD PTR[2048+rsi*8+rbp]
	xor	ecx,DWORD PTR[2048+rdi*8+rbp]
	mov	ebx,DWORD PTR[48+r14]
	mov	eax,DWORD PTR[52+r14]
	xor	ecx,edx
	ror	edx,8
	xor	r8d,ecx
	xor	r9d,ecx
	xor	r9d,edx
	mov	rax,QWORD PTR[r13]
	mov	rbx,QWORD PTR[8+r13]
	mov	rcx,QWORD PTR[32+r13]
	mov	rdx,QWORD PTR[40+r13]
	mov	r14,QWORD PTR[48+r13]
	mov	r15,QWORD PTR[56+r13]
	lea	r13,QWORD PTR[128+r13]
	shl	r8,32
	shl	r10,32
	or	r8,r9
	or	r10,r11
	mov	QWORD PTR[((-112))+r13],r8
	mov	QWORD PTR[((-104))+r13],r10
	mov	r11,rcx
	shl	rcx,15
	mov	r9,rdx
	shr	r9,49
	shr	r11,49
	or	rcx,r9
	shl	rdx,15
	or	rdx,r11
	mov	QWORD PTR[((-96))+r13],rcx
	mov	QWORD PTR[((-88))+r13],rdx
	mov	r11,r14
	shl	r14,15
	mov	r9,r15
	shr	r9,49
	shr	r11,49
	or	r14,r9
	shl	r15,15
	or	r15,r11
	mov	QWORD PTR[((-80))+r13],r14
	mov	QWORD PTR[((-72))+r13],r15
	mov	r11,rcx
	shl	rcx,15
	mov	r9,rdx
	shr	r9,49
	shr	r11,49
	or	rcx,r9
	shl	rdx,15
	or	rdx,r11
	mov	QWORD PTR[((-64))+r13],rcx
	mov	QWORD PTR[((-56))+r13],rdx
	mov	r11,r8
	shl	r8,30
	mov	r9,r10
	shr	r9,34
	shr	r11,34
	or	r8,r9
	shl	r10,30
	or	r10,r11
	mov	QWORD PTR[((-48))+r13],r8
	mov	QWORD PTR[((-40))+r13],r10
	mov	r11,rax
	shl	rax,45
	mov	r9,rbx
	shr	r9,19
	shr	r11,19
	or	rax,r9
	shl	rbx,45
	or	rbx,r11
	mov	QWORD PTR[((-32))+r13],rax
	mov	QWORD PTR[((-24))+r13],rbx
	mov	r11,r14
	shl	r14,30
	mov	r9,r15
	shr	r9,34
	shr	r11,34
	or	r14,r9
	shl	r15,30
	or	r15,r11
	mov	QWORD PTR[((-16))+r13],r14
	mov	QWORD PTR[((-8))+r13],r15
	mov	r11,rax
	shl	rax,15
	mov	r9,rbx
	shr	r9,49
	shr	r11,49
	or	rax,r9
	shl	rbx,15
	or	rbx,r11
	mov	QWORD PTR[r13],rax
	mov	QWORD PTR[8+r13],rbx
	mov	r11,rcx
	shl	rcx,30
	mov	r9,rdx
	shr	r9,34
	shr	r11,34
	or	rcx,r9
	shl	rdx,30
	or	rdx,r11
	mov	QWORD PTR[16+r13],rcx
	mov	QWORD PTR[24+r13],rdx
	mov	r11,r8
	shl	r8,30
	mov	r9,r10
	shr	r9,34
	shr	r11,34
	or	r8,r9
	shl	r10,30
	or	r10,r11
	mov	QWORD PTR[32+r13],r8
	mov	QWORD PTR[40+r13],r10
	mov	r11,rax
	shl	rax,17
	mov	r9,rbx
	shr	r9,47
	shr	r11,47
	or	rax,r9
	shl	rbx,17
	or	rbx,r11
	mov	QWORD PTR[48+r13],rax
	mov	QWORD PTR[56+r13],rbx
	mov	r11,r14
	shl	r14,32
	mov	r9,r15
	shr	r9,32
	shr	r11,32
	or	r14,r9
	shl	r15,32
	or	r15,r11
	mov	QWORD PTR[64+r13],r14
	mov	QWORD PTR[72+r13],r15
	mov	r11,rcx
	shl	rcx,34
	mov	r9,rdx
	shr	r9,30
	shr	r11,30
	or	rcx,r9
	shl	rdx,34
	or	rdx,r11
	mov	QWORD PTR[80+r13],rcx
	mov	QWORD PTR[88+r13],rdx
	mov	r11,r14
	shl	r14,17
	mov	r9,r15
	shr	r9,47
	shr	r11,47
	or	r14,r9
	shl	r15,17
	or	r15,r11
	mov	QWORD PTR[96+r13],r14
	mov	QWORD PTR[104+r13],r15
	mov	r11,rax
	shl	rax,34
	mov	r9,rbx
	shr	r9,30
	shr	r11,30
	or	rax,r9
	shl	rbx,34
	or	rbx,r11
	mov	QWORD PTR[112+r13],rax
	mov	QWORD PTR[120+r13],rbx
	mov	r11,r8
	shl	r8,51
	mov	r9,r10
	shr	r9,13
	shr	r11,13
	or	r8,r9
	shl	r10,51
	or	r10,r11
	mov	QWORD PTR[128+r13],r8
	mov	QWORD PTR[136+r13],r10
	mov	eax,4
$L$done::
	mov	r15,QWORD PTR[rsp]
	mov	r14,QWORD PTR[8+rsp]
	mov	r13,QWORD PTR[16+rsp]
	mov	rbp,QWORD PTR[24+rsp]
	mov	rbx,QWORD PTR[32+rsp]
	lea	rsp,QWORD PTR[40+rsp]
$L$key_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_Camellia_Ekeygen::
Camellia_Ekeygen	ENDP
ALIGN	64
$L$Camellia_SIGMA::
	DD	03bcc908bh,0a09e667fh,04caa73b2h,0b67ae858h
	DD	0e94f82beh,0c6ef372fh,0f1d36f1ch,054ff53a5h
	DD	0de682d1dh,010e527fah,0b3e6c1fdh,0b05688c2h
	DD	0,0,0,0
$L$Camellia_SBOX::
	DD	070707000h,070700070h
	DD	082828200h,02c2c002ch
	DD	02c2c2c00h,0b3b300b3h
	DD	0ececec00h,0c0c000c0h
	DD	0b3b3b300h,0e4e400e4h
	DD	027272700h,057570057h
	DD	0c0c0c000h,0eaea00eah
	DD	0e5e5e500h,0aeae00aeh
	DD	0e4e4e400h,023230023h
	DD	085858500h,06b6b006bh
	DD	057575700h,045450045h
	DD	035353500h,0a5a500a5h
	DD	0eaeaea00h,0eded00edh
	DD	00c0c0c00h,04f4f004fh
	DD	0aeaeae00h,01d1d001dh
	DD	041414100h,092920092h
	DD	023232300h,086860086h
	DD	0efefef00h,0afaf00afh
	DD	06b6b6b00h,07c7c007ch
	DD	093939300h,01f1f001fh
	DD	045454500h,03e3e003eh
	DD	019191900h,0dcdc00dch
	DD	0a5a5a500h,05e5e005eh
	DD	021212100h,00b0b000bh
	DD	0ededed00h,0a6a600a6h
	DD	00e0e0e00h,039390039h
	DD	04f4f4f00h,0d5d500d5h
	DD	04e4e4e00h,05d5d005dh
	DD	01d1d1d00h,0d9d900d9h
	DD	065656500h,05a5a005ah
	DD	092929200h,051510051h
	DD	0bdbdbd00h,06c6c006ch
	DD	086868600h,08b8b008bh
	DD	0b8b8b800h,09a9a009ah
	DD	0afafaf00h,0fbfb00fbh
	DD	08f8f8f00h,0b0b000b0h
	DD	07c7c7c00h,074740074h
	DD	0ebebeb00h,02b2b002bh
	DD	01f1f1f00h,0f0f000f0h
	DD	0cecece00h,084840084h
	DD	03e3e3e00h,0dfdf00dfh
	DD	030303000h,0cbcb00cbh
	DD	0dcdcdc00h,034340034h
	DD	05f5f5f00h,076760076h
	DD	05e5e5e00h,06d6d006dh
	DD	0c5c5c500h,0a9a900a9h
	DD	00b0b0b00h,0d1d100d1h
	DD	01a1a1a00h,004040004h
	DD	0a6a6a600h,014140014h
	DD	0e1e1e100h,03a3a003ah
	DD	039393900h,0dede00deh
	DD	0cacaca00h,011110011h
	DD	0d5d5d500h,032320032h
	DD	047474700h,09c9c009ch
	DD	05d5d5d00h,053530053h
	DD	03d3d3d00h,0f2f200f2h
	DD	0d9d9d900h,0fefe00feh
	DD	001010100h,0cfcf00cfh
	DD	05a5a5a00h,0c3c300c3h
	DD	0d6d6d600h,07a7a007ah
	DD	051515100h,024240024h
	DD	056565600h,0e8e800e8h
	DD	06c6c6c00h,060600060h
	DD	04d4d4d00h,069690069h
	DD	08b8b8b00h,0aaaa00aah
	DD	00d0d0d00h,0a0a000a0h
	DD	09a9a9a00h,0a1a100a1h
	DD	066666600h,062620062h
	DD	0fbfbfb00h,054540054h
	DD	0cccccc00h,01e1e001eh
	DD	0b0b0b000h,0e0e000e0h
	DD	02d2d2d00h,064640064h
	DD	074747400h,010100010h
	DD	012121200h,000000000h
	DD	02b2b2b00h,0a3a300a3h
	DD	020202000h,075750075h
	DD	0f0f0f000h,08a8a008ah
	DD	0b1b1b100h,0e6e600e6h
	DD	084848400h,009090009h
	DD	099999900h,0dddd00ddh
	DD	0dfdfdf00h,087870087h
	DD	04c4c4c00h,083830083h
	DD	0cbcbcb00h,0cdcd00cdh
	DD	0c2c2c200h,090900090h
	DD	034343400h,073730073h
	DD	07e7e7e00h,0f6f600f6h
	DD	076767600h,09d9d009dh
	DD	005050500h,0bfbf00bfh
	DD	06d6d6d00h,052520052h
	DD	0b7b7b700h,0d8d800d8h
	DD	0a9a9a900h,0c8c800c8h
	DD	031313100h,0c6c600c6h
	DD	0d1d1d100h,081810081h
	DD	017171700h,06f6f006fh
	DD	004040400h,013130013h
	DD	0d7d7d700h,063630063h
	DD	014141400h,0e9e900e9h
	DD	058585800h,0a7a700a7h
	DD	03a3a3a00h,09f9f009fh
	DD	061616100h,0bcbc00bch
	DD	0dedede00h,029290029h
	DD	01b1b1b00h,0f9f900f9h
	DD	011111100h,02f2f002fh
	DD	01c1c1c00h,0b4b400b4h
	DD	032323200h,078780078h
	DD	00f0f0f00h,006060006h
	DD	09c9c9c00h,0e7e700e7h
	DD	016161600h,071710071h
	DD	053535300h,0d4d400d4h
	DD	018181800h,0abab00abh
	DD	0f2f2f200h,088880088h
	DD	022222200h,08d8d008dh
	DD	0fefefe00h,072720072h
	DD	044444400h,0b9b900b9h
	DD	0cfcfcf00h,0f8f800f8h
	DD	0b2b2b200h,0acac00ach
	DD	0c3c3c300h,036360036h
	DD	0b5b5b500h,02a2a002ah
	DD	07a7a7a00h,03c3c003ch
	DD	091919100h,0f1f100f1h
	DD	024242400h,040400040h
	DD	008080800h,0d3d300d3h
	DD	0e8e8e800h,0bbbb00bbh
	DD	0a8a8a800h,043430043h
	DD	060606000h,015150015h
	DD	0fcfcfc00h,0adad00adh
	DD	069696900h,077770077h
	DD	050505000h,080800080h
	DD	0aaaaaa00h,082820082h
	DD	0d0d0d000h,0ecec00ech
	DD	0a0a0a000h,027270027h
	DD	07d7d7d00h,0e5e500e5h
	DD	0a1a1a100h,085850085h
	DD	089898900h,035350035h
	DD	062626200h,00c0c000ch
	DD	097979700h,041410041h
	DD	054545400h,0efef00efh
	DD	05b5b5b00h,093930093h
	DD	01e1e1e00h,019190019h
	DD	095959500h,021210021h
	DD	0e0e0e000h,00e0e000eh
	DD	0ffffff00h,04e4e004eh
	DD	064646400h,065650065h
	DD	0d2d2d200h,0bdbd00bdh
	DD	010101000h,0b8b800b8h
	DD	0c4c4c400h,08f8f008fh
	DD	000000000h,0ebeb00ebh
	DD	048484800h,0cece00ceh
	DD	0a3a3a300h,030300030h
	DD	0f7f7f700h,05f5f005fh
	DD	075757500h,0c5c500c5h
	DD	0dbdbdb00h,01a1a001ah
	DD	08a8a8a00h,0e1e100e1h
	DD	003030300h,0caca00cah
	DD	0e6e6e600h,047470047h
	DD	0dadada00h,03d3d003dh
	DD	009090900h,001010001h
	DD	03f3f3f00h,0d6d600d6h
	DD	0dddddd00h,056560056h
	DD	094949400h,04d4d004dh
	DD	087878700h,00d0d000dh
	DD	05c5c5c00h,066660066h
	DD	083838300h,0cccc00cch
	DD	002020200h,02d2d002dh
	DD	0cdcdcd00h,012120012h
	DD	04a4a4a00h,020200020h
	DD	090909000h,0b1b100b1h
	DD	033333300h,099990099h
	DD	073737300h,04c4c004ch
	DD	067676700h,0c2c200c2h
	DD	0f6f6f600h,07e7e007eh
	DD	0f3f3f300h,005050005h
	DD	09d9d9d00h,0b7b700b7h
	DD	07f7f7f00h,031310031h
	DD	0bfbfbf00h,017170017h
	DD	0e2e2e200h,0d7d700d7h
	DD	052525200h,058580058h
	DD	09b9b9b00h,061610061h
	DD	0d8d8d800h,01b1b001bh
	DD	026262600h,01c1c001ch
	DD	0c8c8c800h,00f0f000fh
	DD	037373700h,016160016h
	DD	0c6c6c600h,018180018h
	DD	03b3b3b00h,022220022h
	DD	081818100h,044440044h
	DD	096969600h,0b2b200b2h
	DD	06f6f6f00h,0b5b500b5h
	DD	04b4b4b00h,091910091h
	DD	013131300h,008080008h
	DD	0bebebe00h,0a8a800a8h
	DD	063636300h,0fcfc00fch
	DD	02e2e2e00h,050500050h
	DD	0e9e9e900h,0d0d000d0h
	DD	079797900h,07d7d007dh
	DD	0a7a7a700h,089890089h
	DD	08c8c8c00h,097970097h
	DD	09f9f9f00h,05b5b005bh
	DD	06e6e6e00h,095950095h
	DD	0bcbcbc00h,0ffff00ffh
	DD	08e8e8e00h,0d2d200d2h
	DD	029292900h,0c4c400c4h
	DD	0f5f5f500h,048480048h
	DD	0f9f9f900h,0f7f700f7h
	DD	0b6b6b600h,0dbdb00dbh
	DD	02f2f2f00h,003030003h
	DD	0fdfdfd00h,0dada00dah
	DD	0b4b4b400h,03f3f003fh
	DD	059595900h,094940094h
	DD	078787800h,05c5c005ch
	DD	098989800h,002020002h
	DD	006060600h,04a4a004ah
	DD	06a6a6a00h,033330033h
	DD	0e7e7e700h,067670067h
	DD	046464600h,0f3f300f3h
	DD	071717100h,07f7f007fh
	DD	0bababa00h,0e2e200e2h
	DD	0d4d4d400h,09b9b009bh
	DD	025252500h,026260026h
	DD	0ababab00h,037370037h
	DD	042424200h,03b3b003bh
	DD	088888800h,096960096h
	DD	0a2a2a200h,04b4b004bh
	DD	08d8d8d00h,0bebe00beh
	DD	0fafafa00h,02e2e002eh
	DD	072727200h,079790079h
	DD	007070700h,08c8c008ch
	DD	0b9b9b900h,06e6e006eh
	DD	055555500h,08e8e008eh
	DD	0f8f8f800h,0f5f500f5h
	DD	0eeeeee00h,0b6b600b6h
	DD	0acacac00h,0fdfd00fdh
	DD	00a0a0a00h,059590059h
	DD	036363600h,098980098h
	DD	049494900h,06a6a006ah
	DD	02a2a2a00h,046460046h
	DD	068686800h,0baba00bah
	DD	03c3c3c00h,025250025h
	DD	038383800h,042420042h
	DD	0f1f1f100h,0a2a200a2h
	DD	0a4a4a400h,0fafa00fah
	DD	040404000h,007070007h
	DD	028282800h,055550055h
	DD	0d3d3d300h,0eeee00eeh
	DD	07b7b7b00h,00a0a000ah
	DD	0bbbbbb00h,049490049h
	DD	0c9c9c900h,068680068h
	DD	043434300h,038380038h
	DD	0c1c1c100h,0a4a400a4h
	DD	015151500h,028280028h
	DD	0e3e3e300h,07b7b007bh
	DD	0adadad00h,0c9c900c9h
	DD	0f4f4f400h,0c1c100c1h
	DD	077777700h,0e3e300e3h
	DD	0c7c7c700h,0f4f400f4h
	DD	080808000h,0c7c700c7h
	DD	09e9e9e00h,09e9e009eh
	DD	000e0e0e0h,038003838h
	DD	000050505h,041004141h
	DD	000585858h,016001616h
	DD	000d9d9d9h,076007676h
	DD	000676767h,0d900d9d9h
	DD	0004e4e4eh,093009393h
	DD	000818181h,060006060h
	DD	000cbcbcbh,0f200f2f2h
	DD	000c9c9c9h,072007272h
	DD	0000b0b0bh,0c200c2c2h
	DD	000aeaeaeh,0ab00ababh
	DD	0006a6a6ah,09a009a9ah
	DD	000d5d5d5h,075007575h
	DD	000181818h,006000606h
	DD	0005d5d5dh,057005757h
	DD	000828282h,0a000a0a0h
	DD	000464646h,091009191h
	DD	000dfdfdfh,0f700f7f7h
	DD	000d6d6d6h,0b500b5b5h
	DD	000272727h,0c900c9c9h
	DD	0008a8a8ah,0a200a2a2h
	DD	000323232h,08c008c8ch
	DD	0004b4b4bh,0d200d2d2h
	DD	000424242h,090009090h
	DD	000dbdbdbh,0f600f6f6h
	DD	0001c1c1ch,007000707h
	DD	0009e9e9eh,0a700a7a7h
	DD	0009c9c9ch,027002727h
	DD	0003a3a3ah,08e008e8eh
	DD	000cacacah,0b200b2b2h
	DD	000252525h,049004949h
	DD	0007b7b7bh,0de00dedeh
	DD	0000d0d0dh,043004343h
	DD	000717171h,05c005c5ch
	DD	0005f5f5fh,0d700d7d7h
	DD	0001f1f1fh,0c700c7c7h
	DD	000f8f8f8h,03e003e3eh
	DD	000d7d7d7h,0f500f5f5h
	DD	0003e3e3eh,08f008f8fh
	DD	0009d9d9dh,067006767h
	DD	0007c7c7ch,01f001f1fh
	DD	000606060h,018001818h
	DD	000b9b9b9h,06e006e6eh
	DD	000bebebeh,0af00afafh
	DD	000bcbcbch,02f002f2fh
	DD	0008b8b8bh,0e200e2e2h
	DD	000161616h,085008585h
	DD	000343434h,00d000d0dh
	DD	0004d4d4dh,053005353h
	DD	000c3c3c3h,0f000f0f0h
	DD	000727272h,09c009c9ch
	DD	000959595h,065006565h
	DD	000abababh,0ea00eaeah
	DD	0008e8e8eh,0a300a3a3h
	DD	000bababah,0ae00aeaeh
	DD	0007a7a7ah,09e009e9eh
	DD	000b3b3b3h,0ec00ecech
	DD	000020202h,080008080h
	DD	000b4b4b4h,02d002d2dh
	DD	000adadadh,06b006b6bh
	DD	000a2a2a2h,0a800a8a8h
	DD	000acacach,02b002b2bh
	DD	000d8d8d8h,036003636h
	DD	0009a9a9ah,0a600a6a6h
	DD	000171717h,0c500c5c5h
	DD	0001a1a1ah,086008686h
	DD	000353535h,04d004d4dh
	DD	000cccccch,033003333h
	DD	000f7f7f7h,0fd00fdfdh
	DD	000999999h,066006666h
	DD	000616161h,058005858h
	DD	0005a5a5ah,096009696h
	DD	000e8e8e8h,03a003a3ah
	DD	000242424h,009000909h
	DD	000565656h,095009595h
	DD	000404040h,010001010h
	DD	000e1e1e1h,078007878h
	DD	000636363h,0d800d8d8h
	DD	000090909h,042004242h
	DD	000333333h,0cc00cccch
	DD	000bfbfbfh,0ef00efefh
	DD	000989898h,026002626h
	DD	000979797h,0e500e5e5h
	DD	000858585h,061006161h
	DD	000686868h,01a001a1ah
	DD	000fcfcfch,03f003f3fh
	DD	000ececech,03b003b3bh
	DD	0000a0a0ah,082008282h
	DD	000dadadah,0b600b6b6h
	DD	0006f6f6fh,0db00dbdbh
	DD	000535353h,0d400d4d4h
	DD	000626262h,098009898h
	DD	000a3a3a3h,0e800e8e8h
	DD	0002e2e2eh,08b008b8bh
	DD	000080808h,002000202h
	DD	000afafafh,0eb00ebebh
	DD	000282828h,00a000a0ah
	DD	000b0b0b0h,02c002c2ch
	DD	000747474h,01d001d1dh
	DD	000c2c2c2h,0b000b0b0h
	DD	000bdbdbdh,06f006f6fh
	DD	000363636h,08d008d8dh
	DD	000222222h,088008888h
	DD	000383838h,00e000e0eh
	DD	000646464h,019001919h
	DD	0001e1e1eh,087008787h
	DD	000393939h,04e004e4eh
	DD	0002c2c2ch,00b000b0bh
	DD	000a6a6a6h,0a900a9a9h
	DD	000303030h,00c000c0ch
	DD	000e5e5e5h,079007979h
	DD	000444444h,011001111h
	DD	000fdfdfdh,07f007f7fh
	DD	000888888h,022002222h
	DD	0009f9f9fh,0e700e7e7h
	DD	000656565h,059005959h
	DD	000878787h,0e100e1e1h
	DD	0006b6b6bh,0da00dadah
	DD	000f4f4f4h,03d003d3dh
	DD	000232323h,0c800c8c8h
	DD	000484848h,012001212h
	DD	000101010h,004000404h
	DD	000d1d1d1h,074007474h
	DD	000515151h,054005454h
	DD	000c0c0c0h,030003030h
	DD	000f9f9f9h,07e007e7eh
	DD	000d2d2d2h,0b400b4b4h
	DD	000a0a0a0h,028002828h
	DD	000555555h,055005555h
	DD	000a1a1a1h,068006868h
	DD	000414141h,050005050h
	DD	000fafafah,0be00bebeh
	DD	000434343h,0d000d0d0h
	DD	000131313h,0c400c4c4h
	DD	000c4c4c4h,031003131h
	DD	0002f2f2fh,0cb00cbcbh
	DD	000a8a8a8h,02a002a2ah
	DD	000b6b6b6h,0ad00adadh
	DD	0003c3c3ch,00f000f0fh
	DD	0002b2b2bh,0ca00cacah
	DD	000c1c1c1h,070007070h
	DD	000ffffffh,0ff00ffffh
	DD	000c8c8c8h,032003232h
	DD	000a5a5a5h,069006969h
	DD	000202020h,008000808h
	DD	000898989h,062006262h
	DD	000000000h,000000000h
	DD	000909090h,024002424h
	DD	000474747h,0d100d1d1h
	DD	000efefefh,0fb00fbfbh
	DD	000eaeaeah,0ba00babah
	DD	000b7b7b7h,0ed00ededh
	DD	000151515h,045004545h
	DD	000060606h,081008181h
	DD	000cdcdcdh,073007373h
	DD	000b5b5b5h,06d006d6dh
	DD	000121212h,084008484h
	DD	0007e7e7eh,09f009f9fh
	DD	000bbbbbbh,0ee00eeeeh
	DD	000292929h,04a004a4ah
	DD	0000f0f0fh,0c300c3c3h
	DD	000b8b8b8h,02e002e2eh
	DD	000070707h,0c100c1c1h
	DD	000040404h,001000101h
	DD	0009b9b9bh,0e600e6e6h
	DD	000949494h,025002525h
	DD	000212121h,048004848h
	DD	000666666h,099009999h
	DD	000e6e6e6h,0b900b9b9h
	DD	000cececeh,0b300b3b3h
	DD	000edededh,07b007b7bh
	DD	000e7e7e7h,0f900f9f9h
	DD	0003b3b3bh,0ce00ceceh
	DD	000fefefeh,0bf00bfbfh
	DD	0007f7f7fh,0df00dfdfh
	DD	000c5c5c5h,071007171h
	DD	000a4a4a4h,029002929h
	DD	000373737h,0cd00cdcdh
	DD	000b1b1b1h,06c006c6ch
	DD	0004c4c4ch,013001313h
	DD	000919191h,064006464h
	DD	0006e6e6eh,09b009b9bh
	DD	0008d8d8dh,063006363h
	DD	000767676h,09d009d9dh
	DD	000030303h,0c000c0c0h
	DD	0002d2d2dh,04b004b4bh
	DD	000dededeh,0b700b7b7h
	DD	000969696h,0a500a5a5h
	DD	000262626h,089008989h
	DD	0007d7d7dh,05f005f5fh
	DD	000c6c6c6h,0b100b1b1h
	DD	0005c5c5ch,017001717h
	DD	000d3d3d3h,0f400f4f4h
	DD	000f2f2f2h,0bc00bcbch
	DD	0004f4f4fh,0d300d3d3h
	DD	000191919h,046004646h
	DD	0003f3f3fh,0cf00cfcfh
	DD	000dcdcdch,037003737h
	DD	000797979h,05e005e5eh
	DD	0001d1d1dh,047004747h
	DD	000525252h,094009494h
	DD	000ebebebh,0fa00fafah
	DD	000f3f3f3h,0fc00fcfch
	DD	0006d6d6dh,05b005b5bh
	DD	0005e5e5eh,097009797h
	DD	000fbfbfbh,0fe00fefeh
	DD	000696969h,05a005a5ah
	DD	000b2b2b2h,0ac00acach
	DD	000f0f0f0h,03c003c3ch
	DD	000313131h,04c004c4ch
	DD	0000c0c0ch,003000303h
	DD	000d4d4d4h,035003535h
	DD	000cfcfcfh,0f300f3f3h
	DD	0008c8c8ch,023002323h
	DD	000e2e2e2h,0b800b8b8h
	DD	000757575h,05d005d5dh
	DD	000a9a9a9h,06a006a6ah
	DD	0004a4a4ah,092009292h
	DD	000575757h,0d500d5d5h
	DD	000848484h,021002121h
	DD	000111111h,044004444h
	DD	000454545h,051005151h
	DD	0001b1b1bh,0c600c6c6h
	DD	000f5f5f5h,07d007d7dh
	DD	000e4e4e4h,039003939h
	DD	0000e0e0eh,083008383h
	DD	000737373h,0dc00dcdch
	DD	000aaaaaah,0aa00aaaah
	DD	000f1f1f1h,07c007c7ch
	DD	000ddddddh,077007777h
	DD	000595959h,056005656h
	DD	000141414h,005000505h
	DD	0006c6c6ch,01b001b1bh
	DD	000929292h,0a400a4a4h
	DD	000545454h,015001515h
	DD	000d0d0d0h,034003434h
	DD	000787878h,01e001e1eh
	DD	000707070h,01c001c1ch
	DD	000e3e3e3h,0f800f8f8h
	DD	000494949h,052005252h
	DD	000808080h,020002020h
	DD	000505050h,014001414h
	DD	000a7a7a7h,0e900e9e9h
	DD	000f6f6f6h,0bd00bdbdh
	DD	000777777h,0dd00ddddh
	DD	000939393h,0e400e4e4h
	DD	000868686h,0a100a1a1h
	DD	000838383h,0e000e0e0h
	DD	0002a2a2ah,08a008a8ah
	DD	000c7c7c7h,0f100f1f1h
	DD	0005b5b5bh,0d600d6d6h
	DD	000e9e9e9h,07a007a7ah
	DD	000eeeeeeh,0bb00bbbbh
	DD	0008f8f8fh,0e300e3e3h
	DD	000010101h,040004040h
	DD	0003d3d3dh,04f004f4fh
PUBLIC	Camellia_cbc_encrypt

ALIGN	16
Camellia_cbc_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_Camellia_cbc_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	cmp	rdx,0
	je	$L$cbc_abort
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$cbc_prologue::

	mov	rbp,rsp
	sub	rsp,64
	and	rsp,-64



	lea	r10,QWORD PTR[((-64-63))+rcx]
	sub	r10,rsp
	neg	r10
	and	r10,03C0h
	sub	rsp,r10


	mov	r12,rdi
	mov	r13,rsi
	mov	rbx,r8
	mov	r14,rcx
	mov	r15d,DWORD PTR[272+rcx]

	mov	QWORD PTR[40+rsp],r8
	mov	QWORD PTR[48+rsp],rbp

$L$cbc_body::
	lea	rbp,QWORD PTR[$L$Camellia_SBOX]

	mov	ecx,32
ALIGN	4
$L$cbc_prefetch_sbox::
	mov	rax,QWORD PTR[rbp]
	mov	rsi,QWORD PTR[32+rbp]
	mov	rdi,QWORD PTR[64+rbp]
	mov	r11,QWORD PTR[96+rbp]
	lea	rbp,QWORD PTR[128+rbp]
	loop	$L$cbc_prefetch_sbox
	sub	rbp,4096
	shl	r15,6
	mov	rcx,rdx
	lea	r15,QWORD PTR[r15*1+r14]

	cmp	r9d,0
	je	$L$CBC_DECRYPT

	and	rdx,-16
	and	rcx,15
	lea	rdx,QWORD PTR[rdx*1+r12]
	mov	QWORD PTR[rsp],r14
	mov	QWORD PTR[8+rsp],rdx
	mov	QWORD PTR[16+rsp],rcx

	cmp	rdx,r12
	mov	r8d,DWORD PTR[rbx]
	mov	r9d,DWORD PTR[4+rbx]
	mov	r10d,DWORD PTR[8+rbx]
	mov	r11d,DWORD PTR[12+rbx]
	je	$L$cbc_enc_tail
	jmp	$L$cbc_eloop

ALIGN	16
$L$cbc_eloop::
	xor	r8d,DWORD PTR[r12]
	xor	r9d,DWORD PTR[4+r12]
	xor	r10d,DWORD PTR[8+r12]
	bswap	r8d
	xor	r11d,DWORD PTR[12+r12]
	bswap	r9d
	bswap	r10d
	bswap	r11d

	call	_x86_64_Camellia_encrypt

	mov	r14,QWORD PTR[rsp]
	bswap	r8d
	mov	rdx,QWORD PTR[8+rsp]
	bswap	r9d
	mov	rcx,QWORD PTR[16+rsp]
	bswap	r10d
	mov	DWORD PTR[r13],r8d
	bswap	r11d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	lea	r12,QWORD PTR[16+r12]
	mov	DWORD PTR[12+r13],r11d
	cmp	r12,rdx
	lea	r13,QWORD PTR[16+r13]
	jne	$L$cbc_eloop

	cmp	rcx,0
	jne	$L$cbc_enc_tail

	mov	r13,QWORD PTR[40+rsp]
	mov	DWORD PTR[r13],r8d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	mov	DWORD PTR[12+r13],r11d
	jmp	$L$cbc_done

ALIGN	16
$L$cbc_enc_tail::
	xor	rax,rax
	mov	QWORD PTR[((0+24))+rsp],rax
	mov	QWORD PTR[((8+24))+rsp],rax
	mov	QWORD PTR[16+rsp],rax

$L$cbc_enc_pushf::
	pushfq
	cld
	mov	rsi,r12
	lea	rdi,QWORD PTR[((8+24))+rsp]
	DD	09066A4F3h

	popfq
$L$cbc_enc_popf::

	lea	r12,QWORD PTR[24+rsp]
	lea	rax,QWORD PTR[((16+24))+rsp]
	mov	QWORD PTR[8+rsp],rax
	jmp	$L$cbc_eloop


ALIGN	16
$L$CBC_DECRYPT::
	xchg	r15,r14
	add	rdx,15
	and	rcx,15
	and	rdx,-16
	mov	QWORD PTR[rsp],r14
	lea	rdx,QWORD PTR[rdx*1+r12]
	mov	QWORD PTR[8+rsp],rdx
	mov	QWORD PTR[16+rsp],rcx

	mov	rax,QWORD PTR[rbx]
	mov	rbx,QWORD PTR[8+rbx]
	jmp	$L$cbc_dloop
ALIGN	16
$L$cbc_dloop::
	mov	r8d,DWORD PTR[r12]
	mov	r9d,DWORD PTR[4+r12]
	mov	r10d,DWORD PTR[8+r12]
	bswap	r8d
	mov	r11d,DWORD PTR[12+r12]
	bswap	r9d
	mov	QWORD PTR[((0+24))+rsp],rax
	bswap	r10d
	mov	QWORD PTR[((8+24))+rsp],rbx
	bswap	r11d

	call	_x86_64_Camellia_decrypt

	mov	r14,QWORD PTR[rsp]
	mov	rdx,QWORD PTR[8+rsp]
	mov	rcx,QWORD PTR[16+rsp]

	bswap	r8d
	mov	rax,QWORD PTR[r12]
	bswap	r9d
	mov	rbx,QWORD PTR[8+r12]
	bswap	r10d
	xor	r8d,DWORD PTR[((0+24))+rsp]
	bswap	r11d
	xor	r9d,DWORD PTR[((4+24))+rsp]
	xor	r10d,DWORD PTR[((8+24))+rsp]
	lea	r12,QWORD PTR[16+r12]
	xor	r11d,DWORD PTR[((12+24))+rsp]
	cmp	r12,rdx
	je	$L$cbc_ddone

	mov	DWORD PTR[r13],r8d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	mov	DWORD PTR[12+r13],r11d

	lea	r13,QWORD PTR[16+r13]
	jmp	$L$cbc_dloop

ALIGN	16
$L$cbc_ddone::
	mov	rdx,QWORD PTR[40+rsp]
	cmp	rcx,0
	jne	$L$cbc_dec_tail

	mov	DWORD PTR[r13],r8d
	mov	DWORD PTR[4+r13],r9d
	mov	DWORD PTR[8+r13],r10d
	mov	DWORD PTR[12+r13],r11d

	mov	QWORD PTR[rdx],rax
	mov	QWORD PTR[8+rdx],rbx
	jmp	$L$cbc_done
ALIGN	16
$L$cbc_dec_tail::
	mov	DWORD PTR[((0+24))+rsp],r8d
	mov	DWORD PTR[((4+24))+rsp],r9d
	mov	DWORD PTR[((8+24))+rsp],r10d
	mov	DWORD PTR[((12+24))+rsp],r11d

$L$cbc_dec_pushf::
	pushfq
	cld
	lea	rsi,QWORD PTR[((8+24))+rsp]
	lea	rdi,QWORD PTR[r13]
	DD	09066A4F3h

	popfq
$L$cbc_dec_popf::

	mov	QWORD PTR[rdx],rax
	mov	QWORD PTR[8+rdx],rbx
	jmp	$L$cbc_done

ALIGN	16
$L$cbc_done::
	mov	rcx,QWORD PTR[48+rsp]
	mov	r15,QWORD PTR[rcx]
	mov	r14,QWORD PTR[8+rcx]
	mov	r13,QWORD PTR[16+rcx]
	mov	r12,QWORD PTR[24+rcx]
	mov	rbp,QWORD PTR[32+rcx]
	mov	rbx,QWORD PTR[40+rcx]
	lea	rsp,QWORD PTR[48+rcx]
$L$cbc_abort::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_Camellia_cbc_encrypt::
Camellia_cbc_encrypt	ENDP

DB	67,97,109,101,108,108,105,97,32,102,111,114,32,120,56,54
DB	95,54,52,32,98,121,32,60,97,112,112,114,111,64,111,112
DB	101,110,115,115,108,46,111,114,103,62,0
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
common_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	lea	rsp,QWORD PTR[((-64))+rsp]

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

	lea	rax,QWORD PTR[40+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r13,QWORD PTR[((-24))+rax]
	mov	r14,QWORD PTR[((-32))+rax]
	mov	r15,QWORD PTR[((-40))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15

$L$in_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	jmp	$L$common_seh_exit
common_se_handler	ENDP


ALIGN	16
cbc_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	lea	rsp,QWORD PTR[((-64))+rsp]

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	lea	r10,QWORD PTR[$L$cbc_prologue]
	cmp	rbx,r10
	jb	$L$in_cbc_prologue

	lea	r10,QWORD PTR[$L$cbc_body]
	cmp	rbx,r10
	jb	$L$in_cbc_frame_setup

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$cbc_abort]
	cmp	rbx,r10
	jae	$L$in_cbc_prologue


	lea	r10,QWORD PTR[$L$cbc_enc_pushf]
	cmp	rbx,r10
	jbe	$L$in_cbc_no_flag
	lea	rax,QWORD PTR[8+rax]
	lea	r10,QWORD PTR[$L$cbc_enc_popf]
	cmp	rbx,r10
	jb	$L$in_cbc_no_flag
	lea	rax,QWORD PTR[((-8))+rax]
	lea	r10,QWORD PTR[$L$cbc_dec_pushf]
	cmp	rbx,r10
	jbe	$L$in_cbc_no_flag
	lea	rax,QWORD PTR[8+rax]
	lea	r10,QWORD PTR[$L$cbc_dec_popf]
	cmp	rbx,r10
	jb	$L$in_cbc_no_flag
	lea	rax,QWORD PTR[((-8))+rax]

$L$in_cbc_no_flag::
	mov	rax,QWORD PTR[48+rax]
	lea	rax,QWORD PTR[48+rax]

$L$in_cbc_frame_setup::
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

$L$in_cbc_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

ALIGN	4
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
	lea	rsp,QWORD PTR[64+rsp]
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
cbc_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_Camellia_EncryptBlock_Rounds
	DD	imagerel $L$SEH_end_Camellia_EncryptBlock_Rounds
	DD	imagerel $L$SEH_info_Camellia_EncryptBlock_Rounds

	DD	imagerel $L$SEH_begin_Camellia_DecryptBlock_Rounds
	DD	imagerel $L$SEH_end_Camellia_DecryptBlock_Rounds
	DD	imagerel $L$SEH_info_Camellia_DecryptBlock_Rounds

	DD	imagerel $L$SEH_begin_Camellia_Ekeygen
	DD	imagerel $L$SEH_end_Camellia_Ekeygen
	DD	imagerel $L$SEH_info_Camellia_Ekeygen

	DD	imagerel $L$SEH_begin_Camellia_cbc_encrypt
	DD	imagerel $L$SEH_end_Camellia_cbc_encrypt
	DD	imagerel $L$SEH_info_Camellia_cbc_encrypt

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_Camellia_EncryptBlock_Rounds::
DB	9,0,0,0
	DD	imagerel common_se_handler
	DD	imagerel $L$enc_prologue,imagerel $L$enc_epilogue

$L$SEH_info_Camellia_DecryptBlock_Rounds::
DB	9,0,0,0
	DD	imagerel common_se_handler
	DD	imagerel $L$dec_prologue,imagerel $L$dec_epilogue

$L$SEH_info_Camellia_Ekeygen::
DB	9,0,0,0
	DD	imagerel common_se_handler
	DD	imagerel $L$key_prologue,imagerel $L$key_epilogue

$L$SEH_info_Camellia_cbc_encrypt::
DB	9,0,0,0
	DD	imagerel cbc_se_handler

.xdata	ENDS
END
