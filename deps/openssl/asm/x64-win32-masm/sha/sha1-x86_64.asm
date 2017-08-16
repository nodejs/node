OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	sha1_block_data_order

ALIGN	16
sha1_block_data_order	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_block_data_order::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	r9d,DWORD PTR[((OPENSSL_ia32cap_P+0))]
	mov	r8d,DWORD PTR[((OPENSSL_ia32cap_P+4))]
	mov	r10d,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	test	r8d,512
	jz	$L$ialu
	test	r10d,536870912
	jnz	_shaext_shortcut
	and	r10d,296
	cmp	r10d,296
	je	_avx2_shortcut
	and	r8d,268435456
	and	r9d,1073741824
	or	r8d,r9d
	cmp	r8d,1342177280
	je	_avx_shortcut
	jmp	_ssse3_shortcut

ALIGN	16
$L$ialu::
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	mov	r8,rdi
	sub	rsp,72
	mov	r9,rsi
	and	rsp,-64
	mov	r10,rdx
	mov	QWORD PTR[64+rsp],rax
$L$prologue::

	mov	esi,DWORD PTR[r8]
	mov	edi,DWORD PTR[4+r8]
	mov	r11d,DWORD PTR[8+r8]
	mov	r12d,DWORD PTR[12+r8]
	mov	r13d,DWORD PTR[16+r8]
	jmp	$L$loop

ALIGN	16
$L$loop::
	mov	edx,DWORD PTR[r9]
	bswap	edx
	mov	ebp,DWORD PTR[4+r9]
	mov	eax,r12d
	mov	DWORD PTR[rsp],edx
	mov	ecx,esi
	bswap	ebp
	xor	eax,r11d
	rol	ecx,5
	and	eax,edi
	lea	r13d,DWORD PTR[1518500249+r13*1+rdx]
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	r14d,DWORD PTR[8+r9]
	mov	eax,r11d
	mov	DWORD PTR[4+rsp],ebp
	mov	ecx,r13d
	bswap	r14d
	xor	eax,edi
	rol	ecx,5
	and	eax,esi
	lea	r12d,DWORD PTR[1518500249+r12*1+rbp]
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	edx,DWORD PTR[12+r9]
	mov	eax,edi
	mov	DWORD PTR[8+rsp],r14d
	mov	ecx,r12d
	bswap	edx
	xor	eax,esi
	rol	ecx,5
	and	eax,r13d
	lea	r11d,DWORD PTR[1518500249+r11*1+r14]
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	ebp,DWORD PTR[16+r9]
	mov	eax,esi
	mov	DWORD PTR[12+rsp],edx
	mov	ecx,r11d
	bswap	ebp
	xor	eax,r13d
	rol	ecx,5
	and	eax,r12d
	lea	edi,DWORD PTR[1518500249+rdi*1+rdx]
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	r14d,DWORD PTR[20+r9]
	mov	eax,r13d
	mov	DWORD PTR[16+rsp],ebp
	mov	ecx,edi
	bswap	r14d
	xor	eax,r12d
	rol	ecx,5
	and	eax,r11d
	lea	esi,DWORD PTR[1518500249+rsi*1+rbp]
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	mov	edx,DWORD PTR[24+r9]
	mov	eax,r12d
	mov	DWORD PTR[20+rsp],r14d
	mov	ecx,esi
	bswap	edx
	xor	eax,r11d
	rol	ecx,5
	and	eax,edi
	lea	r13d,DWORD PTR[1518500249+r13*1+r14]
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	ebp,DWORD PTR[28+r9]
	mov	eax,r11d
	mov	DWORD PTR[24+rsp],edx
	mov	ecx,r13d
	bswap	ebp
	xor	eax,edi
	rol	ecx,5
	and	eax,esi
	lea	r12d,DWORD PTR[1518500249+r12*1+rdx]
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	r14d,DWORD PTR[32+r9]
	mov	eax,edi
	mov	DWORD PTR[28+rsp],ebp
	mov	ecx,r12d
	bswap	r14d
	xor	eax,esi
	rol	ecx,5
	and	eax,r13d
	lea	r11d,DWORD PTR[1518500249+r11*1+rbp]
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	edx,DWORD PTR[36+r9]
	mov	eax,esi
	mov	DWORD PTR[32+rsp],r14d
	mov	ecx,r11d
	bswap	edx
	xor	eax,r13d
	rol	ecx,5
	and	eax,r12d
	lea	edi,DWORD PTR[1518500249+rdi*1+r14]
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	ebp,DWORD PTR[40+r9]
	mov	eax,r13d
	mov	DWORD PTR[36+rsp],edx
	mov	ecx,edi
	bswap	ebp
	xor	eax,r12d
	rol	ecx,5
	and	eax,r11d
	lea	esi,DWORD PTR[1518500249+rsi*1+rdx]
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	mov	r14d,DWORD PTR[44+r9]
	mov	eax,r12d
	mov	DWORD PTR[40+rsp],ebp
	mov	ecx,esi
	bswap	r14d
	xor	eax,r11d
	rol	ecx,5
	and	eax,edi
	lea	r13d,DWORD PTR[1518500249+r13*1+rbp]
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	edx,DWORD PTR[48+r9]
	mov	eax,r11d
	mov	DWORD PTR[44+rsp],r14d
	mov	ecx,r13d
	bswap	edx
	xor	eax,edi
	rol	ecx,5
	and	eax,esi
	lea	r12d,DWORD PTR[1518500249+r12*1+r14]
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	ebp,DWORD PTR[52+r9]
	mov	eax,edi
	mov	DWORD PTR[48+rsp],edx
	mov	ecx,r12d
	bswap	ebp
	xor	eax,esi
	rol	ecx,5
	and	eax,r13d
	lea	r11d,DWORD PTR[1518500249+r11*1+rdx]
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	r14d,DWORD PTR[56+r9]
	mov	eax,esi
	mov	DWORD PTR[52+rsp],ebp
	mov	ecx,r11d
	bswap	r14d
	xor	eax,r13d
	rol	ecx,5
	and	eax,r12d
	lea	edi,DWORD PTR[1518500249+rdi*1+rbp]
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	edx,DWORD PTR[60+r9]
	mov	eax,r13d
	mov	DWORD PTR[56+rsp],r14d
	mov	ecx,edi
	bswap	edx
	xor	eax,r12d
	rol	ecx,5
	and	eax,r11d
	lea	esi,DWORD PTR[1518500249+rsi*1+r14]
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	xor	ebp,DWORD PTR[rsp]
	mov	eax,r12d
	mov	DWORD PTR[60+rsp],edx
	mov	ecx,esi
	xor	ebp,DWORD PTR[8+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	ebp,DWORD PTR[32+rsp]
	and	eax,edi
	lea	r13d,DWORD PTR[1518500249+r13*1+rdx]
	rol	edi,30
	xor	eax,r12d
	add	r13d,ecx
	rol	ebp,1
	add	r13d,eax
	xor	r14d,DWORD PTR[4+rsp]
	mov	eax,r11d
	mov	DWORD PTR[rsp],ebp
	mov	ecx,r13d
	xor	r14d,DWORD PTR[12+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	r14d,DWORD PTR[36+rsp]
	and	eax,esi
	lea	r12d,DWORD PTR[1518500249+r12*1+rbp]
	rol	esi,30
	xor	eax,r11d
	add	r12d,ecx
	rol	r14d,1
	add	r12d,eax
	xor	edx,DWORD PTR[8+rsp]
	mov	eax,edi
	mov	DWORD PTR[4+rsp],r14d
	mov	ecx,r12d
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	edx,DWORD PTR[40+rsp]
	and	eax,r13d
	lea	r11d,DWORD PTR[1518500249+r11*1+r14]
	rol	r13d,30
	xor	eax,edi
	add	r11d,ecx
	rol	edx,1
	add	r11d,eax
	xor	ebp,DWORD PTR[12+rsp]
	mov	eax,esi
	mov	DWORD PTR[8+rsp],edx
	mov	ecx,r11d
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	ebp,DWORD PTR[44+rsp]
	and	eax,r12d
	lea	edi,DWORD PTR[1518500249+rdi*1+rdx]
	rol	r12d,30
	xor	eax,esi
	add	edi,ecx
	rol	ebp,1
	add	edi,eax
	xor	r14d,DWORD PTR[16+rsp]
	mov	eax,r13d
	mov	DWORD PTR[12+rsp],ebp
	mov	ecx,edi
	xor	r14d,DWORD PTR[24+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	r14d,DWORD PTR[48+rsp]
	and	eax,r11d
	lea	esi,DWORD PTR[1518500249+rsi*1+rbp]
	rol	r11d,30
	xor	eax,r13d
	add	esi,ecx
	rol	r14d,1
	add	esi,eax
	xor	edx,DWORD PTR[20+rsp]
	mov	eax,edi
	mov	DWORD PTR[16+rsp],r14d
	mov	ecx,esi
	xor	edx,DWORD PTR[28+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	edx,DWORD PTR[52+rsp]
	lea	r13d,DWORD PTR[1859775393+r13*1+r14]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[24+rsp]
	mov	eax,esi
	mov	DWORD PTR[20+rsp],edx
	mov	ecx,r13d
	xor	ebp,DWORD PTR[32+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	ebp,DWORD PTR[56+rsp]
	lea	r12d,DWORD PTR[1859775393+r12*1+rdx]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[28+rsp]
	mov	eax,r13d
	mov	DWORD PTR[24+rsp],ebp
	mov	ecx,r12d
	xor	r14d,DWORD PTR[36+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	r14d,DWORD PTR[60+rsp]
	lea	r11d,DWORD PTR[1859775393+r11*1+rbp]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[32+rsp]
	mov	eax,r12d
	mov	DWORD PTR[28+rsp],r14d
	mov	ecx,r11d
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	edx,DWORD PTR[rsp]
	lea	edi,DWORD PTR[1859775393+rdi*1+r14]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	xor	ebp,DWORD PTR[36+rsp]
	mov	eax,r11d
	mov	DWORD PTR[32+rsp],edx
	mov	ecx,edi
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	ebp,DWORD PTR[4+rsp]
	lea	esi,DWORD PTR[1859775393+rsi*1+rdx]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[40+rsp]
	mov	eax,edi
	mov	DWORD PTR[36+rsp],ebp
	mov	ecx,esi
	xor	r14d,DWORD PTR[48+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	r14d,DWORD PTR[8+rsp]
	lea	r13d,DWORD PTR[1859775393+r13*1+rbp]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[44+rsp]
	mov	eax,esi
	mov	DWORD PTR[40+rsp],r14d
	mov	ecx,r13d
	xor	edx,DWORD PTR[52+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	edx,DWORD PTR[12+rsp]
	lea	r12d,DWORD PTR[1859775393+r12*1+r14]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[48+rsp]
	mov	eax,r13d
	mov	DWORD PTR[44+rsp],edx
	mov	ecx,r12d
	xor	ebp,DWORD PTR[56+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	ebp,DWORD PTR[16+rsp]
	lea	r11d,DWORD PTR[1859775393+r11*1+rdx]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[52+rsp]
	mov	eax,r12d
	mov	DWORD PTR[48+rsp],ebp
	mov	ecx,r11d
	xor	r14d,DWORD PTR[60+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	r14d,DWORD PTR[20+rsp]
	lea	edi,DWORD PTR[1859775393+rdi*1+rbp]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	r14d,1
	xor	edx,DWORD PTR[56+rsp]
	mov	eax,r11d
	mov	DWORD PTR[52+rsp],r14d
	mov	ecx,edi
	xor	edx,DWORD PTR[rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	edx,DWORD PTR[24+rsp]
	lea	esi,DWORD PTR[1859775393+rsi*1+r14]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	edx,1
	xor	ebp,DWORD PTR[60+rsp]
	mov	eax,edi
	mov	DWORD PTR[56+rsp],edx
	mov	ecx,esi
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	ebp,DWORD PTR[28+rsp]
	lea	r13d,DWORD PTR[1859775393+r13*1+rdx]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[rsp]
	mov	eax,esi
	mov	DWORD PTR[60+rsp],ebp
	mov	ecx,r13d
	xor	r14d,DWORD PTR[8+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	r14d,DWORD PTR[32+rsp]
	lea	r12d,DWORD PTR[1859775393+r12*1+rbp]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[4+rsp]
	mov	eax,r13d
	mov	DWORD PTR[rsp],r14d
	mov	ecx,r12d
	xor	edx,DWORD PTR[12+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	edx,DWORD PTR[36+rsp]
	lea	r11d,DWORD PTR[1859775393+r11*1+r14]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[8+rsp]
	mov	eax,r12d
	mov	DWORD PTR[4+rsp],edx
	mov	ecx,r11d
	xor	ebp,DWORD PTR[16+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	ebp,DWORD PTR[40+rsp]
	lea	edi,DWORD PTR[1859775393+rdi*1+rdx]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[12+rsp]
	mov	eax,r11d
	mov	DWORD PTR[8+rsp],ebp
	mov	ecx,edi
	xor	r14d,DWORD PTR[20+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	r14d,DWORD PTR[44+rsp]
	lea	esi,DWORD PTR[1859775393+rsi*1+rbp]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	r14d,1
	xor	edx,DWORD PTR[16+rsp]
	mov	eax,edi
	mov	DWORD PTR[12+rsp],r14d
	mov	ecx,esi
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	edx,DWORD PTR[48+rsp]
	lea	r13d,DWORD PTR[1859775393+r13*1+r14]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[20+rsp]
	mov	eax,esi
	mov	DWORD PTR[16+rsp],edx
	mov	ecx,r13d
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	ebp,DWORD PTR[52+rsp]
	lea	r12d,DWORD PTR[1859775393+r12*1+rdx]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[24+rsp]
	mov	eax,r13d
	mov	DWORD PTR[20+rsp],ebp
	mov	ecx,r12d
	xor	r14d,DWORD PTR[32+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	r14d,DWORD PTR[56+rsp]
	lea	r11d,DWORD PTR[1859775393+r11*1+rbp]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[28+rsp]
	mov	eax,r12d
	mov	DWORD PTR[24+rsp],r14d
	mov	ecx,r11d
	xor	edx,DWORD PTR[36+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	edx,DWORD PTR[60+rsp]
	lea	edi,DWORD PTR[1859775393+rdi*1+r14]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	xor	ebp,DWORD PTR[32+rsp]
	mov	eax,r11d
	mov	DWORD PTR[28+rsp],edx
	mov	ecx,edi
	xor	ebp,DWORD PTR[40+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	ebp,DWORD PTR[rsp]
	lea	esi,DWORD PTR[1859775393+rsi*1+rdx]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[36+rsp]
	mov	eax,r12d
	mov	DWORD PTR[32+rsp],ebp
	mov	ebx,r12d
	xor	r14d,DWORD PTR[44+rsp]
	and	eax,r11d
	mov	ecx,esi
	xor	r14d,DWORD PTR[4+rsp]
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rbp]
	xor	ebx,r11d
	rol	ecx,5
	add	r13d,eax
	rol	r14d,1
	and	ebx,edi
	add	r13d,ecx
	rol	edi,30
	add	r13d,ebx
	xor	edx,DWORD PTR[40+rsp]
	mov	eax,r11d
	mov	DWORD PTR[36+rsp],r14d
	mov	ebx,r11d
	xor	edx,DWORD PTR[48+rsp]
	and	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[8+rsp]
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+r14]
	xor	ebx,edi
	rol	ecx,5
	add	r12d,eax
	rol	edx,1
	and	ebx,esi
	add	r12d,ecx
	rol	esi,30
	add	r12d,ebx
	xor	ebp,DWORD PTR[44+rsp]
	mov	eax,edi
	mov	DWORD PTR[40+rsp],edx
	mov	ebx,edi
	xor	ebp,DWORD PTR[52+rsp]
	and	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[12+rsp]
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rdx]
	xor	ebx,esi
	rol	ecx,5
	add	r11d,eax
	rol	ebp,1
	and	ebx,r13d
	add	r11d,ecx
	rol	r13d,30
	add	r11d,ebx
	xor	r14d,DWORD PTR[48+rsp]
	mov	eax,esi
	mov	DWORD PTR[44+rsp],ebp
	mov	ebx,esi
	xor	r14d,DWORD PTR[56+rsp]
	and	eax,r13d
	mov	ecx,r11d
	xor	r14d,DWORD PTR[16+rsp]
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rbp]
	xor	ebx,r13d
	rol	ecx,5
	add	edi,eax
	rol	r14d,1
	and	ebx,r12d
	add	edi,ecx
	rol	r12d,30
	add	edi,ebx
	xor	edx,DWORD PTR[52+rsp]
	mov	eax,r13d
	mov	DWORD PTR[48+rsp],r14d
	mov	ebx,r13d
	xor	edx,DWORD PTR[60+rsp]
	and	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[20+rsp]
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+r14]
	xor	ebx,r12d
	rol	ecx,5
	add	esi,eax
	rol	edx,1
	and	ebx,r11d
	add	esi,ecx
	rol	r11d,30
	add	esi,ebx
	xor	ebp,DWORD PTR[56+rsp]
	mov	eax,r12d
	mov	DWORD PTR[52+rsp],edx
	mov	ebx,r12d
	xor	ebp,DWORD PTR[rsp]
	and	eax,r11d
	mov	ecx,esi
	xor	ebp,DWORD PTR[24+rsp]
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rdx]
	xor	ebx,r11d
	rol	ecx,5
	add	r13d,eax
	rol	ebp,1
	and	ebx,edi
	add	r13d,ecx
	rol	edi,30
	add	r13d,ebx
	xor	r14d,DWORD PTR[60+rsp]
	mov	eax,r11d
	mov	DWORD PTR[56+rsp],ebp
	mov	ebx,r11d
	xor	r14d,DWORD PTR[4+rsp]
	and	eax,edi
	mov	ecx,r13d
	xor	r14d,DWORD PTR[28+rsp]
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rbp]
	xor	ebx,edi
	rol	ecx,5
	add	r12d,eax
	rol	r14d,1
	and	ebx,esi
	add	r12d,ecx
	rol	esi,30
	add	r12d,ebx
	xor	edx,DWORD PTR[rsp]
	mov	eax,edi
	mov	DWORD PTR[60+rsp],r14d
	mov	ebx,edi
	xor	edx,DWORD PTR[8+rsp]
	and	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[32+rsp]
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+r14]
	xor	ebx,esi
	rol	ecx,5
	add	r11d,eax
	rol	edx,1
	and	ebx,r13d
	add	r11d,ecx
	rol	r13d,30
	add	r11d,ebx
	xor	ebp,DWORD PTR[4+rsp]
	mov	eax,esi
	mov	DWORD PTR[rsp],edx
	mov	ebx,esi
	xor	ebp,DWORD PTR[12+rsp]
	and	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[36+rsp]
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rdx]
	xor	ebx,r13d
	rol	ecx,5
	add	edi,eax
	rol	ebp,1
	and	ebx,r12d
	add	edi,ecx
	rol	r12d,30
	add	edi,ebx
	xor	r14d,DWORD PTR[8+rsp]
	mov	eax,r13d
	mov	DWORD PTR[4+rsp],ebp
	mov	ebx,r13d
	xor	r14d,DWORD PTR[16+rsp]
	and	eax,r12d
	mov	ecx,edi
	xor	r14d,DWORD PTR[40+rsp]
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rbp]
	xor	ebx,r12d
	rol	ecx,5
	add	esi,eax
	rol	r14d,1
	and	ebx,r11d
	add	esi,ecx
	rol	r11d,30
	add	esi,ebx
	xor	edx,DWORD PTR[12+rsp]
	mov	eax,r12d
	mov	DWORD PTR[8+rsp],r14d
	mov	ebx,r12d
	xor	edx,DWORD PTR[20+rsp]
	and	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[44+rsp]
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+r14]
	xor	ebx,r11d
	rol	ecx,5
	add	r13d,eax
	rol	edx,1
	and	ebx,edi
	add	r13d,ecx
	rol	edi,30
	add	r13d,ebx
	xor	ebp,DWORD PTR[16+rsp]
	mov	eax,r11d
	mov	DWORD PTR[12+rsp],edx
	mov	ebx,r11d
	xor	ebp,DWORD PTR[24+rsp]
	and	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[48+rsp]
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rdx]
	xor	ebx,edi
	rol	ecx,5
	add	r12d,eax
	rol	ebp,1
	and	ebx,esi
	add	r12d,ecx
	rol	esi,30
	add	r12d,ebx
	xor	r14d,DWORD PTR[20+rsp]
	mov	eax,edi
	mov	DWORD PTR[16+rsp],ebp
	mov	ebx,edi
	xor	r14d,DWORD PTR[28+rsp]
	and	eax,esi
	mov	ecx,r12d
	xor	r14d,DWORD PTR[52+rsp]
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rbp]
	xor	ebx,esi
	rol	ecx,5
	add	r11d,eax
	rol	r14d,1
	and	ebx,r13d
	add	r11d,ecx
	rol	r13d,30
	add	r11d,ebx
	xor	edx,DWORD PTR[24+rsp]
	mov	eax,esi
	mov	DWORD PTR[20+rsp],r14d
	mov	ebx,esi
	xor	edx,DWORD PTR[32+rsp]
	and	eax,r13d
	mov	ecx,r11d
	xor	edx,DWORD PTR[56+rsp]
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+r14]
	xor	ebx,r13d
	rol	ecx,5
	add	edi,eax
	rol	edx,1
	and	ebx,r12d
	add	edi,ecx
	rol	r12d,30
	add	edi,ebx
	xor	ebp,DWORD PTR[28+rsp]
	mov	eax,r13d
	mov	DWORD PTR[24+rsp],edx
	mov	ebx,r13d
	xor	ebp,DWORD PTR[36+rsp]
	and	eax,r12d
	mov	ecx,edi
	xor	ebp,DWORD PTR[60+rsp]
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rdx]
	xor	ebx,r12d
	rol	ecx,5
	add	esi,eax
	rol	ebp,1
	and	ebx,r11d
	add	esi,ecx
	rol	r11d,30
	add	esi,ebx
	xor	r14d,DWORD PTR[32+rsp]
	mov	eax,r12d
	mov	DWORD PTR[28+rsp],ebp
	mov	ebx,r12d
	xor	r14d,DWORD PTR[40+rsp]
	and	eax,r11d
	mov	ecx,esi
	xor	r14d,DWORD PTR[rsp]
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rbp]
	xor	ebx,r11d
	rol	ecx,5
	add	r13d,eax
	rol	r14d,1
	and	ebx,edi
	add	r13d,ecx
	rol	edi,30
	add	r13d,ebx
	xor	edx,DWORD PTR[36+rsp]
	mov	eax,r11d
	mov	DWORD PTR[32+rsp],r14d
	mov	ebx,r11d
	xor	edx,DWORD PTR[44+rsp]
	and	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[4+rsp]
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+r14]
	xor	ebx,edi
	rol	ecx,5
	add	r12d,eax
	rol	edx,1
	and	ebx,esi
	add	r12d,ecx
	rol	esi,30
	add	r12d,ebx
	xor	ebp,DWORD PTR[40+rsp]
	mov	eax,edi
	mov	DWORD PTR[36+rsp],edx
	mov	ebx,edi
	xor	ebp,DWORD PTR[48+rsp]
	and	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[8+rsp]
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rdx]
	xor	ebx,esi
	rol	ecx,5
	add	r11d,eax
	rol	ebp,1
	and	ebx,r13d
	add	r11d,ecx
	rol	r13d,30
	add	r11d,ebx
	xor	r14d,DWORD PTR[44+rsp]
	mov	eax,esi
	mov	DWORD PTR[40+rsp],ebp
	mov	ebx,esi
	xor	r14d,DWORD PTR[52+rsp]
	and	eax,r13d
	mov	ecx,r11d
	xor	r14d,DWORD PTR[12+rsp]
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rbp]
	xor	ebx,r13d
	rol	ecx,5
	add	edi,eax
	rol	r14d,1
	and	ebx,r12d
	add	edi,ecx
	rol	r12d,30
	add	edi,ebx
	xor	edx,DWORD PTR[48+rsp]
	mov	eax,r13d
	mov	DWORD PTR[44+rsp],r14d
	mov	ebx,r13d
	xor	edx,DWORD PTR[56+rsp]
	and	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[16+rsp]
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+r14]
	xor	ebx,r12d
	rol	ecx,5
	add	esi,eax
	rol	edx,1
	and	ebx,r11d
	add	esi,ecx
	rol	r11d,30
	add	esi,ebx
	xor	ebp,DWORD PTR[52+rsp]
	mov	eax,edi
	mov	DWORD PTR[48+rsp],edx
	mov	ecx,esi
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	ebp,DWORD PTR[20+rsp]
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rdx]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[56+rsp]
	mov	eax,esi
	mov	DWORD PTR[52+rsp],ebp
	mov	ecx,r13d
	xor	r14d,DWORD PTR[rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	r14d,DWORD PTR[24+rsp]
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rbp]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[60+rsp]
	mov	eax,r13d
	mov	DWORD PTR[56+rsp],r14d
	mov	ecx,r12d
	xor	edx,DWORD PTR[4+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	edx,DWORD PTR[28+rsp]
	lea	r11d,DWORD PTR[((-899497514))+r11*1+r14]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[rsp]
	mov	eax,r12d
	mov	DWORD PTR[60+rsp],edx
	mov	ecx,r11d
	xor	ebp,DWORD PTR[8+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	ebp,DWORD PTR[32+rsp]
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rdx]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[4+rsp]
	mov	eax,r11d
	mov	DWORD PTR[rsp],ebp
	mov	ecx,edi
	xor	r14d,DWORD PTR[12+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	r14d,DWORD PTR[36+rsp]
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rbp]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	r14d,1
	xor	edx,DWORD PTR[8+rsp]
	mov	eax,edi
	mov	DWORD PTR[4+rsp],r14d
	mov	ecx,esi
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	edx,DWORD PTR[40+rsp]
	lea	r13d,DWORD PTR[((-899497514))+r13*1+r14]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[12+rsp]
	mov	eax,esi
	mov	DWORD PTR[8+rsp],edx
	mov	ecx,r13d
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	ebp,DWORD PTR[44+rsp]
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rdx]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[16+rsp]
	mov	eax,r13d
	mov	DWORD PTR[12+rsp],ebp
	mov	ecx,r12d
	xor	r14d,DWORD PTR[24+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	r14d,DWORD PTR[48+rsp]
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rbp]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[20+rsp]
	mov	eax,r12d
	mov	DWORD PTR[16+rsp],r14d
	mov	ecx,r11d
	xor	edx,DWORD PTR[28+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	edx,DWORD PTR[52+rsp]
	lea	edi,DWORD PTR[((-899497514))+rdi*1+r14]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	xor	ebp,DWORD PTR[24+rsp]
	mov	eax,r11d
	mov	DWORD PTR[20+rsp],edx
	mov	ecx,edi
	xor	ebp,DWORD PTR[32+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	ebp,DWORD PTR[56+rsp]
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rdx]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[28+rsp]
	mov	eax,edi
	mov	DWORD PTR[24+rsp],ebp
	mov	ecx,esi
	xor	r14d,DWORD PTR[36+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	r14d,DWORD PTR[60+rsp]
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rbp]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[32+rsp]
	mov	eax,esi
	mov	DWORD PTR[28+rsp],r14d
	mov	ecx,r13d
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	edx,DWORD PTR[rsp]
	lea	r12d,DWORD PTR[((-899497514))+r12*1+r14]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[36+rsp]
	mov	eax,r13d

	mov	ecx,r12d
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	ebp,DWORD PTR[4+rsp]
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rdx]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[40+rsp]
	mov	eax,r12d

	mov	ecx,r11d
	xor	r14d,DWORD PTR[48+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	r14d,DWORD PTR[8+rsp]
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rbp]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	r14d,1
	xor	edx,DWORD PTR[44+rsp]
	mov	eax,r11d

	mov	ecx,edi
	xor	edx,DWORD PTR[52+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	edx,DWORD PTR[12+rsp]
	lea	esi,DWORD PTR[((-899497514))+rsi*1+r14]
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	rol	edx,1
	xor	ebp,DWORD PTR[48+rsp]
	mov	eax,edi

	mov	ecx,esi
	xor	ebp,DWORD PTR[56+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	ebp,DWORD PTR[16+rsp]
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rdx]
	xor	eax,r11d
	add	r13d,ecx
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	xor	r14d,DWORD PTR[52+rsp]
	mov	eax,esi

	mov	ecx,r13d
	xor	r14d,DWORD PTR[60+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	r14d,DWORD PTR[20+rsp]
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rbp]
	xor	eax,edi
	add	r12d,ecx
	rol	esi,30
	add	r12d,eax
	rol	r14d,1
	xor	edx,DWORD PTR[56+rsp]
	mov	eax,r13d

	mov	ecx,r12d
	xor	edx,DWORD PTR[rsp]
	xor	eax,edi
	rol	ecx,5
	xor	edx,DWORD PTR[24+rsp]
	lea	r11d,DWORD PTR[((-899497514))+r11*1+r14]
	xor	eax,esi
	add	r11d,ecx
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	xor	ebp,DWORD PTR[60+rsp]
	mov	eax,r12d

	mov	ecx,r11d
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	ebp,DWORD PTR[28+rsp]
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rdx]
	xor	eax,r13d
	add	edi,ecx
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	mov	eax,r11d
	mov	ecx,edi
	xor	eax,r13d
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rbp]
	rol	ecx,5
	xor	eax,r12d
	add	esi,ecx
	rol	r11d,30
	add	esi,eax
	add	esi,DWORD PTR[r8]
	add	edi,DWORD PTR[4+r8]
	add	r11d,DWORD PTR[8+r8]
	add	r12d,DWORD PTR[12+r8]
	add	r13d,DWORD PTR[16+r8]
	mov	DWORD PTR[r8],esi
	mov	DWORD PTR[4+r8],edi
	mov	DWORD PTR[8+r8],r11d
	mov	DWORD PTR[12+r8],r12d
	mov	DWORD PTR[16+r8],r13d

	sub	r10,1
	lea	r9,QWORD PTR[64+r9]
	jnz	$L$loop

	mov	rsi,QWORD PTR[64+rsp]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order::
sha1_block_data_order	ENDP

ALIGN	32
sha1_block_data_order_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_block_data_order_shaext::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_shaext_shortcut::
	lea	rsp,QWORD PTR[((-72))+rsp]
	movaps	XMMWORD PTR[(-8-64)+rax],xmm6
	movaps	XMMWORD PTR[(-8-48)+rax],xmm7
	movaps	XMMWORD PTR[(-8-32)+rax],xmm8
	movaps	XMMWORD PTR[(-8-16)+rax],xmm9
$L$prologue_shaext::
	movdqu	xmm0,XMMWORD PTR[rdi]
	movd	xmm1,DWORD PTR[16+rdi]
	movdqa	xmm3,XMMWORD PTR[((K_XX_XX+160))]

	movdqu	xmm4,XMMWORD PTR[rsi]
	pshufd	xmm0,xmm0,27
	movdqu	xmm5,XMMWORD PTR[16+rsi]
	pshufd	xmm1,xmm1,27
	movdqu	xmm6,XMMWORD PTR[32+rsi]
DB	102,15,56,0,227
	movdqu	xmm7,XMMWORD PTR[48+rsi]
DB	102,15,56,0,235
DB	102,15,56,0,243
	movdqa	xmm9,xmm1
DB	102,15,56,0,251
	jmp	$L$oop_shaext

ALIGN	16
$L$oop_shaext::
	dec	rdx
	lea	r8,QWORD PTR[64+rsi]
	paddd	xmm1,xmm4
	cmovne	rsi,r8
	movdqa	xmm8,xmm0
DB	15,56,201,229
	movdqa	xmm2,xmm0
DB	15,58,204,193,0
DB	15,56,200,213
	pxor	xmm4,xmm6
DB	15,56,201,238
DB	15,56,202,231

	movdqa	xmm1,xmm0
DB	15,58,204,194,0
DB	15,56,200,206
	pxor	xmm5,xmm7
DB	15,56,202,236
DB	15,56,201,247
	movdqa	xmm2,xmm0
DB	15,58,204,193,0
DB	15,56,200,215
	pxor	xmm6,xmm4
DB	15,56,201,252
DB	15,56,202,245

	movdqa	xmm1,xmm0
DB	15,58,204,194,0
DB	15,56,200,204
	pxor	xmm7,xmm5
DB	15,56,202,254
DB	15,56,201,229
	movdqa	xmm2,xmm0
DB	15,58,204,193,0
DB	15,56,200,213
	pxor	xmm4,xmm6
DB	15,56,201,238
DB	15,56,202,231

	movdqa	xmm1,xmm0
DB	15,58,204,194,1
DB	15,56,200,206
	pxor	xmm5,xmm7
DB	15,56,202,236
DB	15,56,201,247
	movdqa	xmm2,xmm0
DB	15,58,204,193,1
DB	15,56,200,215
	pxor	xmm6,xmm4
DB	15,56,201,252
DB	15,56,202,245

	movdqa	xmm1,xmm0
DB	15,58,204,194,1
DB	15,56,200,204
	pxor	xmm7,xmm5
DB	15,56,202,254
DB	15,56,201,229
	movdqa	xmm2,xmm0
DB	15,58,204,193,1
DB	15,56,200,213
	pxor	xmm4,xmm6
DB	15,56,201,238
DB	15,56,202,231

	movdqa	xmm1,xmm0
DB	15,58,204,194,1
DB	15,56,200,206
	pxor	xmm5,xmm7
DB	15,56,202,236
DB	15,56,201,247
	movdqa	xmm2,xmm0
DB	15,58,204,193,2
DB	15,56,200,215
	pxor	xmm6,xmm4
DB	15,56,201,252
DB	15,56,202,245

	movdqa	xmm1,xmm0
DB	15,58,204,194,2
DB	15,56,200,204
	pxor	xmm7,xmm5
DB	15,56,202,254
DB	15,56,201,229
	movdqa	xmm2,xmm0
DB	15,58,204,193,2
DB	15,56,200,213
	pxor	xmm4,xmm6
DB	15,56,201,238
DB	15,56,202,231

	movdqa	xmm1,xmm0
DB	15,58,204,194,2
DB	15,56,200,206
	pxor	xmm5,xmm7
DB	15,56,202,236
DB	15,56,201,247
	movdqa	xmm2,xmm0
DB	15,58,204,193,2
DB	15,56,200,215
	pxor	xmm6,xmm4
DB	15,56,201,252
DB	15,56,202,245

	movdqa	xmm1,xmm0
DB	15,58,204,194,3
DB	15,56,200,204
	pxor	xmm7,xmm5
DB	15,56,202,254
	movdqu	xmm4,XMMWORD PTR[rsi]
	movdqa	xmm2,xmm0
DB	15,58,204,193,3
DB	15,56,200,213
	movdqu	xmm5,XMMWORD PTR[16+rsi]
DB	102,15,56,0,227

	movdqa	xmm1,xmm0
DB	15,58,204,194,3
DB	15,56,200,206
	movdqu	xmm6,XMMWORD PTR[32+rsi]
DB	102,15,56,0,235

	movdqa	xmm2,xmm0
DB	15,58,204,193,3
DB	15,56,200,215
	movdqu	xmm7,XMMWORD PTR[48+rsi]
DB	102,15,56,0,243

	movdqa	xmm1,xmm0
DB	15,58,204,194,3
DB	65,15,56,200,201
DB	102,15,56,0,251

	paddd	xmm0,xmm8
	movdqa	xmm9,xmm1

	jnz	$L$oop_shaext

	pshufd	xmm0,xmm0,27
	pshufd	xmm1,xmm1,27
	movdqu	XMMWORD PTR[rdi],xmm0
	movd	DWORD PTR[16+rdi],xmm1
	movaps	xmm6,XMMWORD PTR[((-8-64))+rax]
	movaps	xmm7,XMMWORD PTR[((-8-48))+rax]
	movaps	xmm8,XMMWORD PTR[((-8-32))+rax]
	movaps	xmm9,XMMWORD PTR[((-8-16))+rax]
	mov	rsp,rax
$L$epilogue_shaext::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order_shaext::
sha1_block_data_order_shaext	ENDP

ALIGN	16
sha1_block_data_order_ssse3	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_block_data_order_ssse3::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_ssse3_shortcut::
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	lea	rsp,QWORD PTR[((-160))+rsp]
	movaps	XMMWORD PTR[(-40-96)+rax],xmm6
	movaps	XMMWORD PTR[(-40-80)+rax],xmm7
	movaps	XMMWORD PTR[(-40-64)+rax],xmm8
	movaps	XMMWORD PTR[(-40-48)+rax],xmm9
	movaps	XMMWORD PTR[(-40-32)+rax],xmm10
	movaps	XMMWORD PTR[(-40-16)+rax],xmm11
$L$prologue_ssse3::
	mov	r14,rax
	and	rsp,-64
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rdx

	shl	r10,6
	add	r10,r9
	lea	r11,QWORD PTR[((K_XX_XX+64))]

	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	esi,ebx
	mov	ebp,DWORD PTR[16+r8]
	mov	edi,ecx
	xor	edi,edx
	and	esi,edi

	movdqa	xmm6,XMMWORD PTR[64+r11]
	movdqa	xmm9,XMMWORD PTR[((-64))+r11]
	movdqu	xmm0,XMMWORD PTR[r9]
	movdqu	xmm1,XMMWORD PTR[16+r9]
	movdqu	xmm2,XMMWORD PTR[32+r9]
	movdqu	xmm3,XMMWORD PTR[48+r9]
DB	102,15,56,0,198
DB	102,15,56,0,206
DB	102,15,56,0,214
	add	r9,64
	paddd	xmm0,xmm9
DB	102,15,56,0,222
	paddd	xmm1,xmm9
	paddd	xmm2,xmm9
	movdqa	XMMWORD PTR[rsp],xmm0
	psubd	xmm0,xmm9
	movdqa	XMMWORD PTR[16+rsp],xmm1
	psubd	xmm1,xmm9
	movdqa	XMMWORD PTR[32+rsp],xmm2
	psubd	xmm2,xmm9
	jmp	$L$oop_ssse3
ALIGN	16
$L$oop_ssse3::
	ror	ebx,2
	pshufd	xmm4,xmm0,238
	xor	esi,edx
	movdqa	xmm8,xmm3
	paddd	xmm9,xmm3
	mov	edi,eax
	add	ebp,DWORD PTR[rsp]
	punpcklqdq	xmm4,xmm1
	xor	ebx,ecx
	rol	eax,5
	add	ebp,esi
	psrldq	xmm8,4
	and	edi,ebx
	xor	ebx,ecx
	pxor	xmm4,xmm0
	add	ebp,eax
	ror	eax,7
	pxor	xmm8,xmm2
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[4+rsp]
	pxor	xmm4,xmm8
	xor	eax,ebx
	rol	ebp,5
	movdqa	XMMWORD PTR[48+rsp],xmm9
	add	edx,edi
	and	esi,eax
	movdqa	xmm10,xmm4
	xor	eax,ebx
	add	edx,ebp
	ror	ebp,7
	movdqa	xmm8,xmm4
	xor	esi,ebx
	pslldq	xmm10,12
	paddd	xmm4,xmm4
	mov	edi,edx
	add	ecx,DWORD PTR[8+rsp]
	psrld	xmm8,31
	xor	ebp,eax
	rol	edx,5
	add	ecx,esi
	movdqa	xmm9,xmm10
	and	edi,ebp
	xor	ebp,eax
	psrld	xmm10,30
	add	ecx,edx
	ror	edx,7
	por	xmm4,xmm8
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[12+rsp]
	pslld	xmm9,2
	pxor	xmm4,xmm10
	xor	edx,ebp
	movdqa	xmm10,XMMWORD PTR[((-64))+r11]
	rol	ecx,5
	add	ebx,edi
	and	esi,edx
	pxor	xmm4,xmm9
	xor	edx,ebp
	add	ebx,ecx
	ror	ecx,7
	pshufd	xmm5,xmm1,238
	xor	esi,ebp
	movdqa	xmm9,xmm4
	paddd	xmm10,xmm4
	mov	edi,ebx
	add	eax,DWORD PTR[16+rsp]
	punpcklqdq	xmm5,xmm2
	xor	ecx,edx
	rol	ebx,5
	add	eax,esi
	psrldq	xmm9,4
	and	edi,ecx
	xor	ecx,edx
	pxor	xmm5,xmm1
	add	eax,ebx
	ror	ebx,7
	pxor	xmm9,xmm3
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[20+rsp]
	pxor	xmm5,xmm9
	xor	ebx,ecx
	rol	eax,5
	movdqa	XMMWORD PTR[rsp],xmm10
	add	ebp,edi
	and	esi,ebx
	movdqa	xmm8,xmm5
	xor	ebx,ecx
	add	ebp,eax
	ror	eax,7
	movdqa	xmm9,xmm5
	xor	esi,ecx
	pslldq	xmm8,12
	paddd	xmm5,xmm5
	mov	edi,ebp
	add	edx,DWORD PTR[24+rsp]
	psrld	xmm9,31
	xor	eax,ebx
	rol	ebp,5
	add	edx,esi
	movdqa	xmm10,xmm8
	and	edi,eax
	xor	eax,ebx
	psrld	xmm8,30
	add	edx,ebp
	ror	ebp,7
	por	xmm5,xmm9
	xor	edi,ebx
	mov	esi,edx
	add	ecx,DWORD PTR[28+rsp]
	pslld	xmm10,2
	pxor	xmm5,xmm8
	xor	ebp,eax
	movdqa	xmm8,XMMWORD PTR[((-32))+r11]
	rol	edx,5
	add	ecx,edi
	and	esi,ebp
	pxor	xmm5,xmm10
	xor	ebp,eax
	add	ecx,edx
	ror	edx,7
	pshufd	xmm6,xmm2,238
	xor	esi,eax
	movdqa	xmm10,xmm5
	paddd	xmm8,xmm5
	mov	edi,ecx
	add	ebx,DWORD PTR[32+rsp]
	punpcklqdq	xmm6,xmm3
	xor	edx,ebp
	rol	ecx,5
	add	ebx,esi
	psrldq	xmm10,4
	and	edi,edx
	xor	edx,ebp
	pxor	xmm6,xmm2
	add	ebx,ecx
	ror	ecx,7
	pxor	xmm10,xmm4
	xor	edi,ebp
	mov	esi,ebx
	add	eax,DWORD PTR[36+rsp]
	pxor	xmm6,xmm10
	xor	ecx,edx
	rol	ebx,5
	movdqa	XMMWORD PTR[16+rsp],xmm8
	add	eax,edi
	and	esi,ecx
	movdqa	xmm9,xmm6
	xor	ecx,edx
	add	eax,ebx
	ror	ebx,7
	movdqa	xmm10,xmm6
	xor	esi,edx
	pslldq	xmm9,12
	paddd	xmm6,xmm6
	mov	edi,eax
	add	ebp,DWORD PTR[40+rsp]
	psrld	xmm10,31
	xor	ebx,ecx
	rol	eax,5
	add	ebp,esi
	movdqa	xmm8,xmm9
	and	edi,ebx
	xor	ebx,ecx
	psrld	xmm9,30
	add	ebp,eax
	ror	eax,7
	por	xmm6,xmm10
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[44+rsp]
	pslld	xmm8,2
	pxor	xmm6,xmm9
	xor	eax,ebx
	movdqa	xmm9,XMMWORD PTR[((-32))+r11]
	rol	ebp,5
	add	edx,edi
	and	esi,eax
	pxor	xmm6,xmm8
	xor	eax,ebx
	add	edx,ebp
	ror	ebp,7
	pshufd	xmm7,xmm3,238
	xor	esi,ebx
	movdqa	xmm8,xmm6
	paddd	xmm9,xmm6
	mov	edi,edx
	add	ecx,DWORD PTR[48+rsp]
	punpcklqdq	xmm7,xmm4
	xor	ebp,eax
	rol	edx,5
	add	ecx,esi
	psrldq	xmm8,4
	and	edi,ebp
	xor	ebp,eax
	pxor	xmm7,xmm3
	add	ecx,edx
	ror	edx,7
	pxor	xmm8,xmm5
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[52+rsp]
	pxor	xmm7,xmm8
	xor	edx,ebp
	rol	ecx,5
	movdqa	XMMWORD PTR[32+rsp],xmm9
	add	ebx,edi
	and	esi,edx
	movdqa	xmm10,xmm7
	xor	edx,ebp
	add	ebx,ecx
	ror	ecx,7
	movdqa	xmm8,xmm7
	xor	esi,ebp
	pslldq	xmm10,12
	paddd	xmm7,xmm7
	mov	edi,ebx
	add	eax,DWORD PTR[56+rsp]
	psrld	xmm8,31
	xor	ecx,edx
	rol	ebx,5
	add	eax,esi
	movdqa	xmm9,xmm10
	and	edi,ecx
	xor	ecx,edx
	psrld	xmm10,30
	add	eax,ebx
	ror	ebx,7
	por	xmm7,xmm8
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[60+rsp]
	pslld	xmm9,2
	pxor	xmm7,xmm10
	xor	ebx,ecx
	movdqa	xmm10,XMMWORD PTR[((-32))+r11]
	rol	eax,5
	add	ebp,edi
	and	esi,ebx
	pxor	xmm7,xmm9
	pshufd	xmm9,xmm6,238
	xor	ebx,ecx
	add	ebp,eax
	ror	eax,7
	pxor	xmm0,xmm4
	xor	esi,ecx
	mov	edi,ebp
	add	edx,DWORD PTR[rsp]
	punpcklqdq	xmm9,xmm7
	xor	eax,ebx
	rol	ebp,5
	pxor	xmm0,xmm1
	add	edx,esi
	and	edi,eax
	movdqa	xmm8,xmm10
	xor	eax,ebx
	paddd	xmm10,xmm7
	add	edx,ebp
	pxor	xmm0,xmm9
	ror	ebp,7
	xor	edi,ebx
	mov	esi,edx
	add	ecx,DWORD PTR[4+rsp]
	movdqa	xmm9,xmm0
	xor	ebp,eax
	rol	edx,5
	movdqa	XMMWORD PTR[48+rsp],xmm10
	add	ecx,edi
	and	esi,ebp
	xor	ebp,eax
	pslld	xmm0,2
	add	ecx,edx
	ror	edx,7
	psrld	xmm9,30
	xor	esi,eax
	mov	edi,ecx
	add	ebx,DWORD PTR[8+rsp]
	por	xmm0,xmm9
	xor	edx,ebp
	rol	ecx,5
	pshufd	xmm10,xmm7,238
	add	ebx,esi
	and	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[12+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	add	eax,ebx
	pxor	xmm1,xmm5
	add	ebp,DWORD PTR[16+rsp]
	xor	esi,ecx
	punpcklqdq	xmm10,xmm0
	mov	edi,eax
	rol	eax,5
	pxor	xmm1,xmm2
	add	ebp,esi
	xor	edi,ecx
	movdqa	xmm9,xmm8
	ror	ebx,7
	paddd	xmm8,xmm0
	add	ebp,eax
	pxor	xmm1,xmm10
	add	edx,DWORD PTR[20+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm10,xmm1
	add	edx,edi
	xor	esi,ebx
	movdqa	XMMWORD PTR[rsp],xmm8
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[24+rsp]
	pslld	xmm1,2
	xor	esi,eax
	mov	edi,edx
	psrld	xmm10,30
	rol	edx,5
	add	ecx,esi
	xor	edi,eax
	ror	ebp,7
	por	xmm1,xmm10
	add	ecx,edx
	add	ebx,DWORD PTR[28+rsp]
	pshufd	xmm8,xmm0,238
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	pxor	xmm2,xmm6
	add	eax,DWORD PTR[32+rsp]
	xor	esi,edx
	punpcklqdq	xmm8,xmm1
	mov	edi,ebx
	rol	ebx,5
	pxor	xmm2,xmm3
	add	eax,esi
	xor	edi,edx
	movdqa	xmm10,XMMWORD PTR[r11]
	ror	ecx,7
	paddd	xmm9,xmm1
	add	eax,ebx
	pxor	xmm2,xmm8
	add	ebp,DWORD PTR[36+rsp]
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	movdqa	xmm8,xmm2
	add	ebp,edi
	xor	esi,ecx
	movdqa	XMMWORD PTR[16+rsp],xmm9
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[40+rsp]
	pslld	xmm2,2
	xor	esi,ebx
	mov	edi,ebp
	psrld	xmm8,30
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	por	xmm2,xmm8
	add	edx,ebp
	add	ecx,DWORD PTR[44+rsp]
	pshufd	xmm9,xmm1,238
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	pxor	xmm3,xmm7
	add	ebx,DWORD PTR[48+rsp]
	xor	esi,ebp
	punpcklqdq	xmm9,xmm2
	mov	edi,ecx
	rol	ecx,5
	pxor	xmm3,xmm4
	add	ebx,esi
	xor	edi,ebp
	movdqa	xmm8,xmm10
	ror	edx,7
	paddd	xmm10,xmm2
	add	ebx,ecx
	pxor	xmm3,xmm9
	add	eax,DWORD PTR[52+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	movdqa	xmm9,xmm3
	add	eax,edi
	xor	esi,edx
	movdqa	XMMWORD PTR[32+rsp],xmm10
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[56+rsp]
	pslld	xmm3,2
	xor	esi,ecx
	mov	edi,eax
	psrld	xmm9,30
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	por	xmm3,xmm9
	add	ebp,eax
	add	edx,DWORD PTR[60+rsp]
	pshufd	xmm10,xmm2,238
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	pxor	xmm4,xmm0
	add	ecx,DWORD PTR[rsp]
	xor	esi,eax
	punpcklqdq	xmm10,xmm3
	mov	edi,edx
	rol	edx,5
	pxor	xmm4,xmm5
	add	ecx,esi
	xor	edi,eax
	movdqa	xmm9,xmm8
	ror	ebp,7
	paddd	xmm8,xmm3
	add	ecx,edx
	pxor	xmm4,xmm10
	add	ebx,DWORD PTR[4+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	movdqa	xmm10,xmm4
	add	ebx,edi
	xor	esi,ebp
	movdqa	XMMWORD PTR[48+rsp],xmm8
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[8+rsp]
	pslld	xmm4,2
	xor	esi,edx
	mov	edi,ebx
	psrld	xmm10,30
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	por	xmm4,xmm10
	add	eax,ebx
	add	ebp,DWORD PTR[12+rsp]
	pshufd	xmm8,xmm3,238
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	pxor	xmm5,xmm1
	add	edx,DWORD PTR[16+rsp]
	xor	esi,ebx
	punpcklqdq	xmm8,xmm4
	mov	edi,ebp
	rol	ebp,5
	pxor	xmm5,xmm6
	add	edx,esi
	xor	edi,ebx
	movdqa	xmm10,xmm9
	ror	eax,7
	paddd	xmm9,xmm4
	add	edx,ebp
	pxor	xmm5,xmm8
	add	ecx,DWORD PTR[20+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	movdqa	xmm8,xmm5
	add	ecx,edi
	xor	esi,eax
	movdqa	XMMWORD PTR[rsp],xmm9
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[24+rsp]
	pslld	xmm5,2
	xor	esi,ebp
	mov	edi,ecx
	psrld	xmm8,30
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	por	xmm5,xmm8
	add	ebx,ecx
	add	eax,DWORD PTR[28+rsp]
	pshufd	xmm9,xmm4,238
	ror	ecx,7
	mov	esi,ebx
	xor	edi,edx
	rol	ebx,5
	add	eax,edi
	xor	esi,ecx
	xor	ecx,edx
	add	eax,ebx
	pxor	xmm6,xmm2
	add	ebp,DWORD PTR[32+rsp]
	and	esi,ecx
	xor	ecx,edx
	ror	ebx,7
	punpcklqdq	xmm9,xmm5
	mov	edi,eax
	xor	esi,ecx
	pxor	xmm6,xmm7
	rol	eax,5
	add	ebp,esi
	movdqa	xmm8,xmm10
	xor	edi,ebx
	paddd	xmm10,xmm5
	xor	ebx,ecx
	pxor	xmm6,xmm9
	add	ebp,eax
	add	edx,DWORD PTR[36+rsp]
	and	edi,ebx
	xor	ebx,ecx
	ror	eax,7
	movdqa	xmm9,xmm6
	mov	esi,ebp
	xor	edi,ebx
	movdqa	XMMWORD PTR[16+rsp],xmm10
	rol	ebp,5
	add	edx,edi
	xor	esi,eax
	pslld	xmm6,2
	xor	eax,ebx
	add	edx,ebp
	psrld	xmm9,30
	add	ecx,DWORD PTR[40+rsp]
	and	esi,eax
	xor	eax,ebx
	por	xmm6,xmm9
	ror	ebp,7
	mov	edi,edx
	xor	esi,eax
	rol	edx,5
	pshufd	xmm10,xmm5,238
	add	ecx,esi
	xor	edi,ebp
	xor	ebp,eax
	add	ecx,edx
	add	ebx,DWORD PTR[44+rsp]
	and	edi,ebp
	xor	ebp,eax
	ror	edx,7
	mov	esi,ecx
	xor	edi,ebp
	rol	ecx,5
	add	ebx,edi
	xor	esi,edx
	xor	edx,ebp
	add	ebx,ecx
	pxor	xmm7,xmm3
	add	eax,DWORD PTR[48+rsp]
	and	esi,edx
	xor	edx,ebp
	ror	ecx,7
	punpcklqdq	xmm10,xmm6
	mov	edi,ebx
	xor	esi,edx
	pxor	xmm7,xmm0
	rol	ebx,5
	add	eax,esi
	movdqa	xmm9,XMMWORD PTR[32+r11]
	xor	edi,ecx
	paddd	xmm8,xmm6
	xor	ecx,edx
	pxor	xmm7,xmm10
	add	eax,ebx
	add	ebp,DWORD PTR[52+rsp]
	and	edi,ecx
	xor	ecx,edx
	ror	ebx,7
	movdqa	xmm10,xmm7
	mov	esi,eax
	xor	edi,ecx
	movdqa	XMMWORD PTR[32+rsp],xmm8
	rol	eax,5
	add	ebp,edi
	xor	esi,ebx
	pslld	xmm7,2
	xor	ebx,ecx
	add	ebp,eax
	psrld	xmm10,30
	add	edx,DWORD PTR[56+rsp]
	and	esi,ebx
	xor	ebx,ecx
	por	xmm7,xmm10
	ror	eax,7
	mov	edi,ebp
	xor	esi,ebx
	rol	ebp,5
	pshufd	xmm8,xmm6,238
	add	edx,esi
	xor	edi,eax
	xor	eax,ebx
	add	edx,ebp
	add	ecx,DWORD PTR[60+rsp]
	and	edi,eax
	xor	eax,ebx
	ror	ebp,7
	mov	esi,edx
	xor	edi,eax
	rol	edx,5
	add	ecx,edi
	xor	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	pxor	xmm0,xmm4
	add	ebx,DWORD PTR[rsp]
	and	esi,ebp
	xor	ebp,eax
	ror	edx,7
	punpcklqdq	xmm8,xmm7
	mov	edi,ecx
	xor	esi,ebp
	pxor	xmm0,xmm1
	rol	ecx,5
	add	ebx,esi
	movdqa	xmm10,xmm9
	xor	edi,edx
	paddd	xmm9,xmm7
	xor	edx,ebp
	pxor	xmm0,xmm8
	add	ebx,ecx
	add	eax,DWORD PTR[4+rsp]
	and	edi,edx
	xor	edx,ebp
	ror	ecx,7
	movdqa	xmm8,xmm0
	mov	esi,ebx
	xor	edi,edx
	movdqa	XMMWORD PTR[48+rsp],xmm9
	rol	ebx,5
	add	eax,edi
	xor	esi,ecx
	pslld	xmm0,2
	xor	ecx,edx
	add	eax,ebx
	psrld	xmm8,30
	add	ebp,DWORD PTR[8+rsp]
	and	esi,ecx
	xor	ecx,edx
	por	xmm0,xmm8
	ror	ebx,7
	mov	edi,eax
	xor	esi,ecx
	rol	eax,5
	pshufd	xmm9,xmm7,238
	add	ebp,esi
	xor	edi,ebx
	xor	ebx,ecx
	add	ebp,eax
	add	edx,DWORD PTR[12+rsp]
	and	edi,ebx
	xor	ebx,ecx
	ror	eax,7
	mov	esi,ebp
	xor	edi,ebx
	rol	ebp,5
	add	edx,edi
	xor	esi,eax
	xor	eax,ebx
	add	edx,ebp
	pxor	xmm1,xmm5
	add	ecx,DWORD PTR[16+rsp]
	and	esi,eax
	xor	eax,ebx
	ror	ebp,7
	punpcklqdq	xmm9,xmm0
	mov	edi,edx
	xor	esi,eax
	pxor	xmm1,xmm2
	rol	edx,5
	add	ecx,esi
	movdqa	xmm8,xmm10
	xor	edi,ebp
	paddd	xmm10,xmm0
	xor	ebp,eax
	pxor	xmm1,xmm9
	add	ecx,edx
	add	ebx,DWORD PTR[20+rsp]
	and	edi,ebp
	xor	ebp,eax
	ror	edx,7
	movdqa	xmm9,xmm1
	mov	esi,ecx
	xor	edi,ebp
	movdqa	XMMWORD PTR[rsp],xmm10
	rol	ecx,5
	add	ebx,edi
	xor	esi,edx
	pslld	xmm1,2
	xor	edx,ebp
	add	ebx,ecx
	psrld	xmm9,30
	add	eax,DWORD PTR[24+rsp]
	and	esi,edx
	xor	edx,ebp
	por	xmm1,xmm9
	ror	ecx,7
	mov	edi,ebx
	xor	esi,edx
	rol	ebx,5
	pshufd	xmm10,xmm0,238
	add	eax,esi
	xor	edi,ecx
	xor	ecx,edx
	add	eax,ebx
	add	ebp,DWORD PTR[28+rsp]
	and	edi,ecx
	xor	ecx,edx
	ror	ebx,7
	mov	esi,eax
	xor	edi,ecx
	rol	eax,5
	add	ebp,edi
	xor	esi,ebx
	xor	ebx,ecx
	add	ebp,eax
	pxor	xmm2,xmm6
	add	edx,DWORD PTR[32+rsp]
	and	esi,ebx
	xor	ebx,ecx
	ror	eax,7
	punpcklqdq	xmm10,xmm1
	mov	edi,ebp
	xor	esi,ebx
	pxor	xmm2,xmm3
	rol	ebp,5
	add	edx,esi
	movdqa	xmm9,xmm8
	xor	edi,eax
	paddd	xmm8,xmm1
	xor	eax,ebx
	pxor	xmm2,xmm10
	add	edx,ebp
	add	ecx,DWORD PTR[36+rsp]
	and	edi,eax
	xor	eax,ebx
	ror	ebp,7
	movdqa	xmm10,xmm2
	mov	esi,edx
	xor	edi,eax
	movdqa	XMMWORD PTR[16+rsp],xmm8
	rol	edx,5
	add	ecx,edi
	xor	esi,ebp
	pslld	xmm2,2
	xor	ebp,eax
	add	ecx,edx
	psrld	xmm10,30
	add	ebx,DWORD PTR[40+rsp]
	and	esi,ebp
	xor	ebp,eax
	por	xmm2,xmm10
	ror	edx,7
	mov	edi,ecx
	xor	esi,ebp
	rol	ecx,5
	pshufd	xmm8,xmm1,238
	add	ebx,esi
	xor	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[44+rsp]
	and	edi,edx
	xor	edx,ebp
	ror	ecx,7
	mov	esi,ebx
	xor	edi,edx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	add	eax,ebx
	pxor	xmm3,xmm7
	add	ebp,DWORD PTR[48+rsp]
	xor	esi,ecx
	punpcklqdq	xmm8,xmm2
	mov	edi,eax
	rol	eax,5
	pxor	xmm3,xmm4
	add	ebp,esi
	xor	edi,ecx
	movdqa	xmm10,xmm9
	ror	ebx,7
	paddd	xmm9,xmm2
	add	ebp,eax
	pxor	xmm3,xmm8
	add	edx,DWORD PTR[52+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm8,xmm3
	add	edx,edi
	xor	esi,ebx
	movdqa	XMMWORD PTR[32+rsp],xmm9
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[56+rsp]
	pslld	xmm3,2
	xor	esi,eax
	mov	edi,edx
	psrld	xmm8,30
	rol	edx,5
	add	ecx,esi
	xor	edi,eax
	ror	ebp,7
	por	xmm3,xmm8
	add	ecx,edx
	add	ebx,DWORD PTR[60+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	paddd	xmm10,xmm3
	add	eax,esi
	xor	edi,edx
	movdqa	XMMWORD PTR[48+rsp],xmm10
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[4+rsp]
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[8+rsp]
	xor	esi,ebx
	mov	edi,ebp
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[12+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	cmp	r9,r10
	je	$L$done_ssse3
	movdqa	xmm6,XMMWORD PTR[64+r11]
	movdqa	xmm9,XMMWORD PTR[((-64))+r11]
	movdqu	xmm0,XMMWORD PTR[r9]
	movdqu	xmm1,XMMWORD PTR[16+r9]
	movdqu	xmm2,XMMWORD PTR[32+r9]
	movdqu	xmm3,XMMWORD PTR[48+r9]
DB	102,15,56,0,198
	add	r9,64
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	mov	edi,ecx
DB	102,15,56,0,206
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	paddd	xmm0,xmm9
	add	ebx,ecx
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	movdqa	XMMWORD PTR[rsp],xmm0
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	psubd	xmm0,xmm9
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,ecx
	mov	edi,eax
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	mov	edi,edx
DB	102,15,56,0,214
	rol	edx,5
	add	ecx,esi
	xor	edi,eax
	ror	ebp,7
	paddd	xmm1,xmm9
	add	ecx,edx
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	movdqa	XMMWORD PTR[16+rsp],xmm1
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	psubd	xmm1,xmm9
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	mov	edi,ebp
DB	102,15,56,0,222
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	paddd	xmm2,xmm9
	add	edx,ebp
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	movdqa	XMMWORD PTR[32+rsp],xmm2
	rol	edx,5
	add	ecx,edi
	xor	esi,eax
	ror	ebp,7
	psubd	xmm2,xmm9
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	ror	ecx,7
	add	eax,ebx
	add	eax,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ecx,DWORD PTR[8+r8]
	add	edx,DWORD PTR[12+r8]
	mov	DWORD PTR[r8],eax
	add	ebp,DWORD PTR[16+r8]
	mov	DWORD PTR[4+r8],esi
	mov	ebx,esi
	mov	DWORD PTR[8+r8],ecx
	mov	edi,ecx
	mov	DWORD PTR[12+r8],edx
	xor	edi,edx
	mov	DWORD PTR[16+r8],ebp
	and	esi,edi
	jmp	$L$oop_ssse3

ALIGN	16
$L$done_ssse3::
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,ecx
	mov	edi,eax
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	mov	edi,edx
	rol	edx,5
	add	ecx,esi
	xor	edi,eax
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	mov	edi,ebp
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	ror	ecx,7
	add	eax,ebx
	add	eax,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ecx,DWORD PTR[8+r8]
	mov	DWORD PTR[r8],eax
	add	edx,DWORD PTR[12+r8]
	mov	DWORD PTR[4+r8],esi
	add	ebp,DWORD PTR[16+r8]
	mov	DWORD PTR[8+r8],ecx
	mov	DWORD PTR[12+r8],edx
	mov	DWORD PTR[16+r8],ebp
	movaps	xmm6,XMMWORD PTR[((-40-96))+r14]
	movaps	xmm7,XMMWORD PTR[((-40-80))+r14]
	movaps	xmm8,XMMWORD PTR[((-40-64))+r14]
	movaps	xmm9,XMMWORD PTR[((-40-48))+r14]
	movaps	xmm10,XMMWORD PTR[((-40-32))+r14]
	movaps	xmm11,XMMWORD PTR[((-40-16))+r14]
	lea	rsi,QWORD PTR[r14]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$epilogue_ssse3::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order_ssse3::
sha1_block_data_order_ssse3	ENDP

ALIGN	16
sha1_block_data_order_avx	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_block_data_order_avx::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_avx_shortcut::
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	lea	rsp,QWORD PTR[((-160))+rsp]
	vzeroupper
	vmovaps	XMMWORD PTR[(-40-96)+rax],xmm6
	vmovaps	XMMWORD PTR[(-40-80)+rax],xmm7
	vmovaps	XMMWORD PTR[(-40-64)+rax],xmm8
	vmovaps	XMMWORD PTR[(-40-48)+rax],xmm9
	vmovaps	XMMWORD PTR[(-40-32)+rax],xmm10
	vmovaps	XMMWORD PTR[(-40-16)+rax],xmm11
$L$prologue_avx::
	mov	r14,rax
	and	rsp,-64
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rdx

	shl	r10,6
	add	r10,r9
	lea	r11,QWORD PTR[((K_XX_XX+64))]

	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	esi,ebx
	mov	ebp,DWORD PTR[16+r8]
	mov	edi,ecx
	xor	edi,edx
	and	esi,edi

	vmovdqa	xmm6,XMMWORD PTR[64+r11]
	vmovdqa	xmm11,XMMWORD PTR[((-64))+r11]
	vmovdqu	xmm0,XMMWORD PTR[r9]
	vmovdqu	xmm1,XMMWORD PTR[16+r9]
	vmovdqu	xmm2,XMMWORD PTR[32+r9]
	vmovdqu	xmm3,XMMWORD PTR[48+r9]
	vpshufb	xmm0,xmm0,xmm6
	add	r9,64
	vpshufb	xmm1,xmm1,xmm6
	vpshufb	xmm2,xmm2,xmm6
	vpshufb	xmm3,xmm3,xmm6
	vpaddd	xmm4,xmm0,xmm11
	vpaddd	xmm5,xmm1,xmm11
	vpaddd	xmm6,xmm2,xmm11
	vmovdqa	XMMWORD PTR[rsp],xmm4
	vmovdqa	XMMWORD PTR[16+rsp],xmm5
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	jmp	$L$oop_avx
ALIGN	16
$L$oop_avx::
	shrd	ebx,ebx,2
	xor	esi,edx
	vpalignr	xmm4,xmm1,xmm0,8
	mov	edi,eax
	add	ebp,DWORD PTR[rsp]
	vpaddd	xmm9,xmm11,xmm3
	xor	ebx,ecx
	shld	eax,eax,5
	vpsrldq	xmm8,xmm3,4
	add	ebp,esi
	and	edi,ebx
	vpxor	xmm4,xmm4,xmm0
	xor	ebx,ecx
	add	ebp,eax
	vpxor	xmm8,xmm8,xmm2
	shrd	eax,eax,7
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[4+rsp]
	vpxor	xmm4,xmm4,xmm8
	xor	eax,ebx
	shld	ebp,ebp,5
	vmovdqa	XMMWORD PTR[48+rsp],xmm9
	add	edx,edi
	and	esi,eax
	vpsrld	xmm8,xmm4,31
	xor	eax,ebx
	add	edx,ebp
	shrd	ebp,ebp,7
	xor	esi,ebx
	vpslldq	xmm10,xmm4,12
	vpaddd	xmm4,xmm4,xmm4
	mov	edi,edx
	add	ecx,DWORD PTR[8+rsp]
	xor	ebp,eax
	shld	edx,edx,5
	vpsrld	xmm9,xmm10,30
	vpor	xmm4,xmm4,xmm8
	add	ecx,esi
	and	edi,ebp
	xor	ebp,eax
	add	ecx,edx
	vpslld	xmm10,xmm10,2
	vpxor	xmm4,xmm4,xmm9
	shrd	edx,edx,7
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[12+rsp]
	vpxor	xmm4,xmm4,xmm10
	xor	edx,ebp
	shld	ecx,ecx,5
	add	ebx,edi
	and	esi,edx
	xor	edx,ebp
	add	ebx,ecx
	shrd	ecx,ecx,7
	xor	esi,ebp
	vpalignr	xmm5,xmm2,xmm1,8
	mov	edi,ebx
	add	eax,DWORD PTR[16+rsp]
	vpaddd	xmm9,xmm11,xmm4
	xor	ecx,edx
	shld	ebx,ebx,5
	vpsrldq	xmm8,xmm4,4
	add	eax,esi
	and	edi,ecx
	vpxor	xmm5,xmm5,xmm1
	xor	ecx,edx
	add	eax,ebx
	vpxor	xmm8,xmm8,xmm3
	shrd	ebx,ebx,7
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[20+rsp]
	vpxor	xmm5,xmm5,xmm8
	xor	ebx,ecx
	shld	eax,eax,5
	vmovdqa	XMMWORD PTR[rsp],xmm9
	add	ebp,edi
	and	esi,ebx
	vpsrld	xmm8,xmm5,31
	xor	ebx,ecx
	add	ebp,eax
	shrd	eax,eax,7
	xor	esi,ecx
	vpslldq	xmm10,xmm5,12
	vpaddd	xmm5,xmm5,xmm5
	mov	edi,ebp
	add	edx,DWORD PTR[24+rsp]
	xor	eax,ebx
	shld	ebp,ebp,5
	vpsrld	xmm9,xmm10,30
	vpor	xmm5,xmm5,xmm8
	add	edx,esi
	and	edi,eax
	xor	eax,ebx
	add	edx,ebp
	vpslld	xmm10,xmm10,2
	vpxor	xmm5,xmm5,xmm9
	shrd	ebp,ebp,7
	xor	edi,ebx
	mov	esi,edx
	add	ecx,DWORD PTR[28+rsp]
	vpxor	xmm5,xmm5,xmm10
	xor	ebp,eax
	shld	edx,edx,5
	vmovdqa	xmm11,XMMWORD PTR[((-32))+r11]
	add	ecx,edi
	and	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	shrd	edx,edx,7
	xor	esi,eax
	vpalignr	xmm6,xmm3,xmm2,8
	mov	edi,ecx
	add	ebx,DWORD PTR[32+rsp]
	vpaddd	xmm9,xmm11,xmm5
	xor	edx,ebp
	shld	ecx,ecx,5
	vpsrldq	xmm8,xmm5,4
	add	ebx,esi
	and	edi,edx
	vpxor	xmm6,xmm6,xmm2
	xor	edx,ebp
	add	ebx,ecx
	vpxor	xmm8,xmm8,xmm4
	shrd	ecx,ecx,7
	xor	edi,ebp
	mov	esi,ebx
	add	eax,DWORD PTR[36+rsp]
	vpxor	xmm6,xmm6,xmm8
	xor	ecx,edx
	shld	ebx,ebx,5
	vmovdqa	XMMWORD PTR[16+rsp],xmm9
	add	eax,edi
	and	esi,ecx
	vpsrld	xmm8,xmm6,31
	xor	ecx,edx
	add	eax,ebx
	shrd	ebx,ebx,7
	xor	esi,edx
	vpslldq	xmm10,xmm6,12
	vpaddd	xmm6,xmm6,xmm6
	mov	edi,eax
	add	ebp,DWORD PTR[40+rsp]
	xor	ebx,ecx
	shld	eax,eax,5
	vpsrld	xmm9,xmm10,30
	vpor	xmm6,xmm6,xmm8
	add	ebp,esi
	and	edi,ebx
	xor	ebx,ecx
	add	ebp,eax
	vpslld	xmm10,xmm10,2
	vpxor	xmm6,xmm6,xmm9
	shrd	eax,eax,7
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[44+rsp]
	vpxor	xmm6,xmm6,xmm10
	xor	eax,ebx
	shld	ebp,ebp,5
	add	edx,edi
	and	esi,eax
	xor	eax,ebx
	add	edx,ebp
	shrd	ebp,ebp,7
	xor	esi,ebx
	vpalignr	xmm7,xmm4,xmm3,8
	mov	edi,edx
	add	ecx,DWORD PTR[48+rsp]
	vpaddd	xmm9,xmm11,xmm6
	xor	ebp,eax
	shld	edx,edx,5
	vpsrldq	xmm8,xmm6,4
	add	ecx,esi
	and	edi,ebp
	vpxor	xmm7,xmm7,xmm3
	xor	ebp,eax
	add	ecx,edx
	vpxor	xmm8,xmm8,xmm5
	shrd	edx,edx,7
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[52+rsp]
	vpxor	xmm7,xmm7,xmm8
	xor	edx,ebp
	shld	ecx,ecx,5
	vmovdqa	XMMWORD PTR[32+rsp],xmm9
	add	ebx,edi
	and	esi,edx
	vpsrld	xmm8,xmm7,31
	xor	edx,ebp
	add	ebx,ecx
	shrd	ecx,ecx,7
	xor	esi,ebp
	vpslldq	xmm10,xmm7,12
	vpaddd	xmm7,xmm7,xmm7
	mov	edi,ebx
	add	eax,DWORD PTR[56+rsp]
	xor	ecx,edx
	shld	ebx,ebx,5
	vpsrld	xmm9,xmm10,30
	vpor	xmm7,xmm7,xmm8
	add	eax,esi
	and	edi,ecx
	xor	ecx,edx
	add	eax,ebx
	vpslld	xmm10,xmm10,2
	vpxor	xmm7,xmm7,xmm9
	shrd	ebx,ebx,7
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[60+rsp]
	vpxor	xmm7,xmm7,xmm10
	xor	ebx,ecx
	shld	eax,eax,5
	add	ebp,edi
	and	esi,ebx
	xor	ebx,ecx
	add	ebp,eax
	vpalignr	xmm8,xmm7,xmm6,8
	vpxor	xmm0,xmm0,xmm4
	shrd	eax,eax,7
	xor	esi,ecx
	mov	edi,ebp
	add	edx,DWORD PTR[rsp]
	vpxor	xmm0,xmm0,xmm1
	xor	eax,ebx
	shld	ebp,ebp,5
	vpaddd	xmm9,xmm11,xmm7
	add	edx,esi
	and	edi,eax
	vpxor	xmm0,xmm0,xmm8
	xor	eax,ebx
	add	edx,ebp
	shrd	ebp,ebp,7
	xor	edi,ebx
	vpsrld	xmm8,xmm0,30
	vmovdqa	XMMWORD PTR[48+rsp],xmm9
	mov	esi,edx
	add	ecx,DWORD PTR[4+rsp]
	xor	ebp,eax
	shld	edx,edx,5
	vpslld	xmm0,xmm0,2
	add	ecx,edi
	and	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	shrd	edx,edx,7
	xor	esi,eax
	mov	edi,ecx
	add	ebx,DWORD PTR[8+rsp]
	vpor	xmm0,xmm0,xmm8
	xor	edx,ebp
	shld	ecx,ecx,5
	add	ebx,esi
	and	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[12+rsp]
	xor	edi,ebp
	mov	esi,ebx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	vpalignr	xmm8,xmm0,xmm7,8
	vpxor	xmm1,xmm1,xmm5
	add	ebp,DWORD PTR[16+rsp]
	xor	esi,ecx
	mov	edi,eax
	shld	eax,eax,5
	vpxor	xmm1,xmm1,xmm2
	add	ebp,esi
	xor	edi,ecx
	vpaddd	xmm9,xmm11,xmm0
	shrd	ebx,ebx,7
	add	ebp,eax
	vpxor	xmm1,xmm1,xmm8
	add	edx,DWORD PTR[20+rsp]
	xor	edi,ebx
	mov	esi,ebp
	shld	ebp,ebp,5
	vpsrld	xmm8,xmm1,30
	vmovdqa	XMMWORD PTR[rsp],xmm9
	add	edx,edi
	xor	esi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	vpslld	xmm1,xmm1,2
	add	ecx,DWORD PTR[24+rsp]
	xor	esi,eax
	mov	edi,edx
	shld	edx,edx,5
	add	ecx,esi
	xor	edi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	vpor	xmm1,xmm1,xmm8
	add	ebx,DWORD PTR[28+rsp]
	xor	edi,ebp
	mov	esi,ecx
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	vpalignr	xmm8,xmm1,xmm0,8
	vpxor	xmm2,xmm2,xmm6
	add	eax,DWORD PTR[32+rsp]
	xor	esi,edx
	mov	edi,ebx
	shld	ebx,ebx,5
	vpxor	xmm2,xmm2,xmm3
	add	eax,esi
	xor	edi,edx
	vpaddd	xmm9,xmm11,xmm1
	vmovdqa	xmm11,XMMWORD PTR[r11]
	shrd	ecx,ecx,7
	add	eax,ebx
	vpxor	xmm2,xmm2,xmm8
	add	ebp,DWORD PTR[36+rsp]
	xor	edi,ecx
	mov	esi,eax
	shld	eax,eax,5
	vpsrld	xmm8,xmm2,30
	vmovdqa	XMMWORD PTR[16+rsp],xmm9
	add	ebp,edi
	xor	esi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	vpslld	xmm2,xmm2,2
	add	edx,DWORD PTR[40+rsp]
	xor	esi,ebx
	mov	edi,ebp
	shld	ebp,ebp,5
	add	edx,esi
	xor	edi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	vpor	xmm2,xmm2,xmm8
	add	ecx,DWORD PTR[44+rsp]
	xor	edi,eax
	mov	esi,edx
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	vpalignr	xmm8,xmm2,xmm1,8
	vpxor	xmm3,xmm3,xmm7
	add	ebx,DWORD PTR[48+rsp]
	xor	esi,ebp
	mov	edi,ecx
	shld	ecx,ecx,5
	vpxor	xmm3,xmm3,xmm4
	add	ebx,esi
	xor	edi,ebp
	vpaddd	xmm9,xmm11,xmm2
	shrd	edx,edx,7
	add	ebx,ecx
	vpxor	xmm3,xmm3,xmm8
	add	eax,DWORD PTR[52+rsp]
	xor	edi,edx
	mov	esi,ebx
	shld	ebx,ebx,5
	vpsrld	xmm8,xmm3,30
	vmovdqa	XMMWORD PTR[32+rsp],xmm9
	add	eax,edi
	xor	esi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	vpslld	xmm3,xmm3,2
	add	ebp,DWORD PTR[56+rsp]
	xor	esi,ecx
	mov	edi,eax
	shld	eax,eax,5
	add	ebp,esi
	xor	edi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	vpor	xmm3,xmm3,xmm8
	add	edx,DWORD PTR[60+rsp]
	xor	edi,ebx
	mov	esi,ebp
	shld	ebp,ebp,5
	add	edx,edi
	xor	esi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	vpalignr	xmm8,xmm3,xmm2,8
	vpxor	xmm4,xmm4,xmm0
	add	ecx,DWORD PTR[rsp]
	xor	esi,eax
	mov	edi,edx
	shld	edx,edx,5
	vpxor	xmm4,xmm4,xmm5
	add	ecx,esi
	xor	edi,eax
	vpaddd	xmm9,xmm11,xmm3
	shrd	ebp,ebp,7
	add	ecx,edx
	vpxor	xmm4,xmm4,xmm8
	add	ebx,DWORD PTR[4+rsp]
	xor	edi,ebp
	mov	esi,ecx
	shld	ecx,ecx,5
	vpsrld	xmm8,xmm4,30
	vmovdqa	XMMWORD PTR[48+rsp],xmm9
	add	ebx,edi
	xor	esi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	vpslld	xmm4,xmm4,2
	add	eax,DWORD PTR[8+rsp]
	xor	esi,edx
	mov	edi,ebx
	shld	ebx,ebx,5
	add	eax,esi
	xor	edi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	vpor	xmm4,xmm4,xmm8
	add	ebp,DWORD PTR[12+rsp]
	xor	edi,ecx
	mov	esi,eax
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	vpalignr	xmm8,xmm4,xmm3,8
	vpxor	xmm5,xmm5,xmm1
	add	edx,DWORD PTR[16+rsp]
	xor	esi,ebx
	mov	edi,ebp
	shld	ebp,ebp,5
	vpxor	xmm5,xmm5,xmm6
	add	edx,esi
	xor	edi,ebx
	vpaddd	xmm9,xmm11,xmm4
	shrd	eax,eax,7
	add	edx,ebp
	vpxor	xmm5,xmm5,xmm8
	add	ecx,DWORD PTR[20+rsp]
	xor	edi,eax
	mov	esi,edx
	shld	edx,edx,5
	vpsrld	xmm8,xmm5,30
	vmovdqa	XMMWORD PTR[rsp],xmm9
	add	ecx,edi
	xor	esi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	vpslld	xmm5,xmm5,2
	add	ebx,DWORD PTR[24+rsp]
	xor	esi,ebp
	mov	edi,ecx
	shld	ecx,ecx,5
	add	ebx,esi
	xor	edi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	vpor	xmm5,xmm5,xmm8
	add	eax,DWORD PTR[28+rsp]
	shrd	ecx,ecx,7
	mov	esi,ebx
	xor	edi,edx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,ecx
	xor	ecx,edx
	add	eax,ebx
	vpalignr	xmm8,xmm5,xmm4,8
	vpxor	xmm6,xmm6,xmm2
	add	ebp,DWORD PTR[32+rsp]
	and	esi,ecx
	xor	ecx,edx
	shrd	ebx,ebx,7
	vpxor	xmm6,xmm6,xmm7
	mov	edi,eax
	xor	esi,ecx
	vpaddd	xmm9,xmm11,xmm5
	shld	eax,eax,5
	add	ebp,esi
	vpxor	xmm6,xmm6,xmm8
	xor	edi,ebx
	xor	ebx,ecx
	add	ebp,eax
	add	edx,DWORD PTR[36+rsp]
	vpsrld	xmm8,xmm6,30
	vmovdqa	XMMWORD PTR[16+rsp],xmm9
	and	edi,ebx
	xor	ebx,ecx
	shrd	eax,eax,7
	mov	esi,ebp
	vpslld	xmm6,xmm6,2
	xor	edi,ebx
	shld	ebp,ebp,5
	add	edx,edi
	xor	esi,eax
	xor	eax,ebx
	add	edx,ebp
	add	ecx,DWORD PTR[40+rsp]
	and	esi,eax
	vpor	xmm6,xmm6,xmm8
	xor	eax,ebx
	shrd	ebp,ebp,7
	mov	edi,edx
	xor	esi,eax
	shld	edx,edx,5
	add	ecx,esi
	xor	edi,ebp
	xor	ebp,eax
	add	ecx,edx
	add	ebx,DWORD PTR[44+rsp]
	and	edi,ebp
	xor	ebp,eax
	shrd	edx,edx,7
	mov	esi,ecx
	xor	edi,ebp
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,edx
	xor	edx,ebp
	add	ebx,ecx
	vpalignr	xmm8,xmm6,xmm5,8
	vpxor	xmm7,xmm7,xmm3
	add	eax,DWORD PTR[48+rsp]
	and	esi,edx
	xor	edx,ebp
	shrd	ecx,ecx,7
	vpxor	xmm7,xmm7,xmm0
	mov	edi,ebx
	xor	esi,edx
	vpaddd	xmm9,xmm11,xmm6
	vmovdqa	xmm11,XMMWORD PTR[32+r11]
	shld	ebx,ebx,5
	add	eax,esi
	vpxor	xmm7,xmm7,xmm8
	xor	edi,ecx
	xor	ecx,edx
	add	eax,ebx
	add	ebp,DWORD PTR[52+rsp]
	vpsrld	xmm8,xmm7,30
	vmovdqa	XMMWORD PTR[32+rsp],xmm9
	and	edi,ecx
	xor	ecx,edx
	shrd	ebx,ebx,7
	mov	esi,eax
	vpslld	xmm7,xmm7,2
	xor	edi,ecx
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ebx
	xor	ebx,ecx
	add	ebp,eax
	add	edx,DWORD PTR[56+rsp]
	and	esi,ebx
	vpor	xmm7,xmm7,xmm8
	xor	ebx,ecx
	shrd	eax,eax,7
	mov	edi,ebp
	xor	esi,ebx
	shld	ebp,ebp,5
	add	edx,esi
	xor	edi,eax
	xor	eax,ebx
	add	edx,ebp
	add	ecx,DWORD PTR[60+rsp]
	and	edi,eax
	xor	eax,ebx
	shrd	ebp,ebp,7
	mov	esi,edx
	xor	edi,eax
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	vpalignr	xmm8,xmm7,xmm6,8
	vpxor	xmm0,xmm0,xmm4
	add	ebx,DWORD PTR[rsp]
	and	esi,ebp
	xor	ebp,eax
	shrd	edx,edx,7
	vpxor	xmm0,xmm0,xmm1
	mov	edi,ecx
	xor	esi,ebp
	vpaddd	xmm9,xmm11,xmm7
	shld	ecx,ecx,5
	add	ebx,esi
	vpxor	xmm0,xmm0,xmm8
	xor	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[4+rsp]
	vpsrld	xmm8,xmm0,30
	vmovdqa	XMMWORD PTR[48+rsp],xmm9
	and	edi,edx
	xor	edx,ebp
	shrd	ecx,ecx,7
	mov	esi,ebx
	vpslld	xmm0,xmm0,2
	xor	edi,edx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,ecx
	xor	ecx,edx
	add	eax,ebx
	add	ebp,DWORD PTR[8+rsp]
	and	esi,ecx
	vpor	xmm0,xmm0,xmm8
	xor	ecx,edx
	shrd	ebx,ebx,7
	mov	edi,eax
	xor	esi,ecx
	shld	eax,eax,5
	add	ebp,esi
	xor	edi,ebx
	xor	ebx,ecx
	add	ebp,eax
	add	edx,DWORD PTR[12+rsp]
	and	edi,ebx
	xor	ebx,ecx
	shrd	eax,eax,7
	mov	esi,ebp
	xor	edi,ebx
	shld	ebp,ebp,5
	add	edx,edi
	xor	esi,eax
	xor	eax,ebx
	add	edx,ebp
	vpalignr	xmm8,xmm0,xmm7,8
	vpxor	xmm1,xmm1,xmm5
	add	ecx,DWORD PTR[16+rsp]
	and	esi,eax
	xor	eax,ebx
	shrd	ebp,ebp,7
	vpxor	xmm1,xmm1,xmm2
	mov	edi,edx
	xor	esi,eax
	vpaddd	xmm9,xmm11,xmm0
	shld	edx,edx,5
	add	ecx,esi
	vpxor	xmm1,xmm1,xmm8
	xor	edi,ebp
	xor	ebp,eax
	add	ecx,edx
	add	ebx,DWORD PTR[20+rsp]
	vpsrld	xmm8,xmm1,30
	vmovdqa	XMMWORD PTR[rsp],xmm9
	and	edi,ebp
	xor	ebp,eax
	shrd	edx,edx,7
	mov	esi,ecx
	vpslld	xmm1,xmm1,2
	xor	edi,ebp
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[24+rsp]
	and	esi,edx
	vpor	xmm1,xmm1,xmm8
	xor	edx,ebp
	shrd	ecx,ecx,7
	mov	edi,ebx
	xor	esi,edx
	shld	ebx,ebx,5
	add	eax,esi
	xor	edi,ecx
	xor	ecx,edx
	add	eax,ebx
	add	ebp,DWORD PTR[28+rsp]
	and	edi,ecx
	xor	ecx,edx
	shrd	ebx,ebx,7
	mov	esi,eax
	xor	edi,ecx
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ebx
	xor	ebx,ecx
	add	ebp,eax
	vpalignr	xmm8,xmm1,xmm0,8
	vpxor	xmm2,xmm2,xmm6
	add	edx,DWORD PTR[32+rsp]
	and	esi,ebx
	xor	ebx,ecx
	shrd	eax,eax,7
	vpxor	xmm2,xmm2,xmm3
	mov	edi,ebp
	xor	esi,ebx
	vpaddd	xmm9,xmm11,xmm1
	shld	ebp,ebp,5
	add	edx,esi
	vpxor	xmm2,xmm2,xmm8
	xor	edi,eax
	xor	eax,ebx
	add	edx,ebp
	add	ecx,DWORD PTR[36+rsp]
	vpsrld	xmm8,xmm2,30
	vmovdqa	XMMWORD PTR[16+rsp],xmm9
	and	edi,eax
	xor	eax,ebx
	shrd	ebp,ebp,7
	mov	esi,edx
	vpslld	xmm2,xmm2,2
	xor	edi,eax
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	add	ebx,DWORD PTR[40+rsp]
	and	esi,ebp
	vpor	xmm2,xmm2,xmm8
	xor	ebp,eax
	shrd	edx,edx,7
	mov	edi,ecx
	xor	esi,ebp
	shld	ecx,ecx,5
	add	ebx,esi
	xor	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[44+rsp]
	and	edi,edx
	xor	edx,ebp
	shrd	ecx,ecx,7
	mov	esi,ebx
	xor	edi,edx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,edx
	add	eax,ebx
	vpalignr	xmm8,xmm2,xmm1,8
	vpxor	xmm3,xmm3,xmm7
	add	ebp,DWORD PTR[48+rsp]
	xor	esi,ecx
	mov	edi,eax
	shld	eax,eax,5
	vpxor	xmm3,xmm3,xmm4
	add	ebp,esi
	xor	edi,ecx
	vpaddd	xmm9,xmm11,xmm2
	shrd	ebx,ebx,7
	add	ebp,eax
	vpxor	xmm3,xmm3,xmm8
	add	edx,DWORD PTR[52+rsp]
	xor	edi,ebx
	mov	esi,ebp
	shld	ebp,ebp,5
	vpsrld	xmm8,xmm3,30
	vmovdqa	XMMWORD PTR[32+rsp],xmm9
	add	edx,edi
	xor	esi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	vpslld	xmm3,xmm3,2
	add	ecx,DWORD PTR[56+rsp]
	xor	esi,eax
	mov	edi,edx
	shld	edx,edx,5
	add	ecx,esi
	xor	edi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	vpor	xmm3,xmm3,xmm8
	add	ebx,DWORD PTR[60+rsp]
	xor	edi,ebp
	mov	esi,ecx
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[rsp]
	vpaddd	xmm9,xmm11,xmm3
	xor	esi,edx
	mov	edi,ebx
	shld	ebx,ebx,5
	add	eax,esi
	vmovdqa	XMMWORD PTR[48+rsp],xmm9
	xor	edi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[4+rsp]
	xor	edi,ecx
	mov	esi,eax
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[8+rsp]
	xor	esi,ebx
	mov	edi,ebp
	shld	ebp,ebp,5
	add	edx,esi
	xor	edi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[12+rsp]
	xor	edi,eax
	mov	esi,edx
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	cmp	r9,r10
	je	$L$done_avx
	vmovdqa	xmm6,XMMWORD PTR[64+r11]
	vmovdqa	xmm11,XMMWORD PTR[((-64))+r11]
	vmovdqu	xmm0,XMMWORD PTR[r9]
	vmovdqu	xmm1,XMMWORD PTR[16+r9]
	vmovdqu	xmm2,XMMWORD PTR[32+r9]
	vmovdqu	xmm3,XMMWORD PTR[48+r9]
	vpshufb	xmm0,xmm0,xmm6
	add	r9,64
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	vpshufb	xmm1,xmm1,xmm6
	mov	edi,ecx
	shld	ecx,ecx,5
	vpaddd	xmm4,xmm0,xmm11
	add	ebx,esi
	xor	edi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	vmovdqa	XMMWORD PTR[rsp],xmm4
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,ecx
	mov	edi,eax
	shld	eax,eax,5
	add	ebp,esi
	xor	edi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	shld	ebp,ebp,5
	add	edx,edi
	xor	esi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	vpshufb	xmm2,xmm2,xmm6
	mov	edi,edx
	shld	edx,edx,5
	vpaddd	xmm5,xmm1,xmm11
	add	ecx,esi
	xor	edi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	vmovdqa	XMMWORD PTR[16+rsp],xmm5
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	shld	ebx,ebx,5
	add	eax,esi
	xor	edi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,ecx
	mov	esi,eax
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	vpshufb	xmm3,xmm3,xmm6
	mov	edi,ebp
	shld	ebp,ebp,5
	vpaddd	xmm6,xmm2,xmm11
	add	edx,esi
	xor	edi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	shld	ecx,ecx,5
	add	ebx,esi
	xor	edi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	shld	ebx,ebx,5
	add	eax,edi
	shrd	ecx,ecx,7
	add	eax,ebx
	add	eax,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ecx,DWORD PTR[8+r8]
	add	edx,DWORD PTR[12+r8]
	mov	DWORD PTR[r8],eax
	add	ebp,DWORD PTR[16+r8]
	mov	DWORD PTR[4+r8],esi
	mov	ebx,esi
	mov	DWORD PTR[8+r8],ecx
	mov	edi,ecx
	mov	DWORD PTR[12+r8],edx
	xor	edi,edx
	mov	DWORD PTR[16+r8],ebp
	and	esi,edi
	jmp	$L$oop_avx

ALIGN	16
$L$done_avx::
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	mov	edi,ecx
	shld	ecx,ecx,5
	add	ebx,esi
	xor	edi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	shld	ebx,ebx,5
	add	eax,edi
	xor	esi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,ecx
	mov	edi,eax
	shld	eax,eax,5
	add	ebp,esi
	xor	edi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	shld	ebp,ebp,5
	add	edx,edi
	xor	esi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	mov	edi,edx
	shld	edx,edx,5
	add	ecx,esi
	xor	edi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	shld	ecx,ecx,5
	add	ebx,edi
	xor	esi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	shld	ebx,ebx,5
	add	eax,esi
	xor	edi,edx
	shrd	ecx,ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,ecx
	mov	esi,eax
	shld	eax,eax,5
	add	ebp,edi
	xor	esi,ecx
	shrd	ebx,ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	mov	edi,ebp
	shld	ebp,ebp,5
	add	edx,esi
	xor	edi,ebx
	shrd	eax,eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	shld	edx,edx,5
	add	ecx,edi
	xor	esi,eax
	shrd	ebp,ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	shld	ecx,ecx,5
	add	ebx,esi
	xor	edi,ebp
	shrd	edx,edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	shld	ebx,ebx,5
	add	eax,edi
	shrd	ecx,ecx,7
	add	eax,ebx
	vzeroupper

	add	eax,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ecx,DWORD PTR[8+r8]
	mov	DWORD PTR[r8],eax
	add	edx,DWORD PTR[12+r8]
	mov	DWORD PTR[4+r8],esi
	add	ebp,DWORD PTR[16+r8]
	mov	DWORD PTR[8+r8],ecx
	mov	DWORD PTR[12+r8],edx
	mov	DWORD PTR[16+r8],ebp
	movaps	xmm6,XMMWORD PTR[((-40-96))+r14]
	movaps	xmm7,XMMWORD PTR[((-40-80))+r14]
	movaps	xmm8,XMMWORD PTR[((-40-64))+r14]
	movaps	xmm9,XMMWORD PTR[((-40-48))+r14]
	movaps	xmm10,XMMWORD PTR[((-40-32))+r14]
	movaps	xmm11,XMMWORD PTR[((-40-16))+r14]
	lea	rsi,QWORD PTR[r14]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$epilogue_avx::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order_avx::
sha1_block_data_order_avx	ENDP

ALIGN	16
sha1_block_data_order_avx2	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_block_data_order_avx2::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_avx2_shortcut::
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	vzeroupper
	lea	rsp,QWORD PTR[((-96))+rsp]
	vmovaps	XMMWORD PTR[(-40-96)+rax],xmm6
	vmovaps	XMMWORD PTR[(-40-80)+rax],xmm7
	vmovaps	XMMWORD PTR[(-40-64)+rax],xmm8
	vmovaps	XMMWORD PTR[(-40-48)+rax],xmm9
	vmovaps	XMMWORD PTR[(-40-32)+rax],xmm10
	vmovaps	XMMWORD PTR[(-40-16)+rax],xmm11
$L$prologue_avx2::
	mov	r14,rax
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rdx

	lea	rsp,QWORD PTR[((-640))+rsp]
	shl	r10,6
	lea	r13,QWORD PTR[64+r9]
	and	rsp,-128
	add	r10,r9
	lea	r11,QWORD PTR[((K_XX_XX+64))]

	mov	eax,DWORD PTR[r8]
	cmp	r13,r10
	cmovae	r13,r9
	mov	ebp,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	esi,DWORD PTR[16+r8]
	vmovdqu	ymm6,YMMWORD PTR[64+r11]

	vmovdqu	xmm0,XMMWORD PTR[r9]
	vmovdqu	xmm1,XMMWORD PTR[16+r9]
	vmovdqu	xmm2,XMMWORD PTR[32+r9]
	vmovdqu	xmm3,XMMWORD PTR[48+r9]
	lea	r9,QWORD PTR[64+r9]
	vinserti128	ymm0,ymm0,XMMWORD PTR[r13],1
	vinserti128	ymm1,ymm1,XMMWORD PTR[16+r13],1
	vpshufb	ymm0,ymm0,ymm6
	vinserti128	ymm2,ymm2,XMMWORD PTR[32+r13],1
	vpshufb	ymm1,ymm1,ymm6
	vinserti128	ymm3,ymm3,XMMWORD PTR[48+r13],1
	vpshufb	ymm2,ymm2,ymm6
	vmovdqu	ymm11,YMMWORD PTR[((-64))+r11]
	vpshufb	ymm3,ymm3,ymm6

	vpaddd	ymm4,ymm0,ymm11
	vpaddd	ymm5,ymm1,ymm11
	vmovdqu	YMMWORD PTR[rsp],ymm4
	vpaddd	ymm6,ymm2,ymm11
	vmovdqu	YMMWORD PTR[32+rsp],ymm5
	vpaddd	ymm7,ymm3,ymm11
	vmovdqu	YMMWORD PTR[64+rsp],ymm6
	vmovdqu	YMMWORD PTR[96+rsp],ymm7
	vpalignr	ymm4,ymm1,ymm0,8
	vpsrldq	ymm8,ymm3,4
	vpxor	ymm4,ymm4,ymm0
	vpxor	ymm8,ymm8,ymm2
	vpxor	ymm4,ymm4,ymm8
	vpsrld	ymm8,ymm4,31
	vpslldq	ymm10,ymm4,12
	vpaddd	ymm4,ymm4,ymm4
	vpsrld	ymm9,ymm10,30
	vpor	ymm4,ymm4,ymm8
	vpslld	ymm10,ymm10,2
	vpxor	ymm4,ymm4,ymm9
	vpxor	ymm4,ymm4,ymm10
	vpaddd	ymm9,ymm4,ymm11
	vmovdqu	YMMWORD PTR[128+rsp],ymm9
	vpalignr	ymm5,ymm2,ymm1,8
	vpsrldq	ymm8,ymm4,4
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm8,ymm8,ymm3
	vpxor	ymm5,ymm5,ymm8
	vpsrld	ymm8,ymm5,31
	vmovdqu	ymm11,YMMWORD PTR[((-32))+r11]
	vpslldq	ymm10,ymm5,12
	vpaddd	ymm5,ymm5,ymm5
	vpsrld	ymm9,ymm10,30
	vpor	ymm5,ymm5,ymm8
	vpslld	ymm10,ymm10,2
	vpxor	ymm5,ymm5,ymm9
	vpxor	ymm5,ymm5,ymm10
	vpaddd	ymm9,ymm5,ymm11
	vmovdqu	YMMWORD PTR[160+rsp],ymm9
	vpalignr	ymm6,ymm3,ymm2,8
	vpsrldq	ymm8,ymm5,4
	vpxor	ymm6,ymm6,ymm2
	vpxor	ymm8,ymm8,ymm4
	vpxor	ymm6,ymm6,ymm8
	vpsrld	ymm8,ymm6,31
	vpslldq	ymm10,ymm6,12
	vpaddd	ymm6,ymm6,ymm6
	vpsrld	ymm9,ymm10,30
	vpor	ymm6,ymm6,ymm8
	vpslld	ymm10,ymm10,2
	vpxor	ymm6,ymm6,ymm9
	vpxor	ymm6,ymm6,ymm10
	vpaddd	ymm9,ymm6,ymm11
	vmovdqu	YMMWORD PTR[192+rsp],ymm9
	vpalignr	ymm7,ymm4,ymm3,8
	vpsrldq	ymm8,ymm6,4
	vpxor	ymm7,ymm7,ymm3
	vpxor	ymm8,ymm8,ymm5
	vpxor	ymm7,ymm7,ymm8
	vpsrld	ymm8,ymm7,31
	vpslldq	ymm10,ymm7,12
	vpaddd	ymm7,ymm7,ymm7
	vpsrld	ymm9,ymm10,30
	vpor	ymm7,ymm7,ymm8
	vpslld	ymm10,ymm10,2
	vpxor	ymm7,ymm7,ymm9
	vpxor	ymm7,ymm7,ymm10
	vpaddd	ymm9,ymm7,ymm11
	vmovdqu	YMMWORD PTR[224+rsp],ymm9
	lea	r13,QWORD PTR[128+rsp]
	jmp	$L$oop_avx2
ALIGN	32
$L$oop_avx2::
	rorx	ebx,ebp,2
	andn	edi,ebp,edx
	and	ebp,ecx
	xor	ebp,edi
	jmp	$L$align32_1
ALIGN	32
$L$align32_1::
	vpalignr	ymm8,ymm7,ymm6,8
	vpxor	ymm0,ymm0,ymm4
	add	esi,DWORD PTR[((-128))+r13]
	andn	edi,eax,ecx
	vpxor	ymm0,ymm0,ymm1
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	vpxor	ymm0,ymm0,ymm8
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	vpsrld	ymm8,ymm0,30
	vpslld	ymm0,ymm0,2
	add	edx,DWORD PTR[((-124))+r13]
	andn	edi,esi,ebx
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	vpor	ymm0,ymm0,ymm8
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-120))+r13]
	andn	edi,edx,ebp
	vpaddd	ymm9,ymm0,ymm11
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	vmovdqu	YMMWORD PTR[256+rsp],ymm9
	add	ecx,r12d
	xor	edx,edi
	add	ebx,DWORD PTR[((-116))+r13]
	andn	edi,ecx,eax
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	add	ebp,DWORD PTR[((-96))+r13]
	andn	edi,ebx,esi
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	and	ebx,edx
	add	ebp,r12d
	xor	ebx,edi
	vpalignr	ymm8,ymm0,ymm7,8
	vpxor	ymm1,ymm1,ymm5
	add	eax,DWORD PTR[((-92))+r13]
	andn	edi,ebp,edx
	vpxor	ymm1,ymm1,ymm2
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	vpxor	ymm1,ymm1,ymm8
	and	ebp,ecx
	add	eax,r12d
	xor	ebp,edi
	vpsrld	ymm8,ymm1,30
	vpslld	ymm1,ymm1,2
	add	esi,DWORD PTR[((-88))+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	vpor	ymm1,ymm1,ymm8
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[((-84))+r13]
	andn	edi,esi,ebx
	vpaddd	ymm9,ymm1,ymm11
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	vmovdqu	YMMWORD PTR[288+rsp],ymm9
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-64))+r13]
	andn	edi,edx,ebp
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	add	ecx,r12d
	xor	edx,edi
	add	ebx,DWORD PTR[((-60))+r13]
	andn	edi,ecx,eax
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	vpalignr	ymm8,ymm1,ymm0,8
	vpxor	ymm2,ymm2,ymm6
	add	ebp,DWORD PTR[((-56))+r13]
	andn	edi,ebx,esi
	vpxor	ymm2,ymm2,ymm3
	vmovdqu	ymm11,YMMWORD PTR[r11]
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	vpxor	ymm2,ymm2,ymm8
	and	ebx,edx
	add	ebp,r12d
	xor	ebx,edi
	vpsrld	ymm8,ymm2,30
	vpslld	ymm2,ymm2,2
	add	eax,DWORD PTR[((-52))+r13]
	andn	edi,ebp,edx
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	and	ebp,ecx
	vpor	ymm2,ymm2,ymm8
	add	eax,r12d
	xor	ebp,edi
	add	esi,DWORD PTR[((-32))+r13]
	andn	edi,eax,ecx
	vpaddd	ymm9,ymm2,ymm11
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	vmovdqu	YMMWORD PTR[320+rsp],ymm9
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[((-28))+r13]
	andn	edi,esi,ebx
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-24))+r13]
	andn	edi,edx,ebp
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	add	ecx,r12d
	xor	edx,edi
	vpalignr	ymm8,ymm2,ymm1,8
	vpxor	ymm3,ymm3,ymm7
	add	ebx,DWORD PTR[((-20))+r13]
	andn	edi,ecx,eax
	vpxor	ymm3,ymm3,ymm4
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	vpxor	ymm3,ymm3,ymm8
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	vpsrld	ymm8,ymm3,30
	vpslld	ymm3,ymm3,2
	add	ebp,DWORD PTR[r13]
	andn	edi,ebx,esi
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	and	ebx,edx
	vpor	ymm3,ymm3,ymm8
	add	ebp,r12d
	xor	ebx,edi
	add	eax,DWORD PTR[4+r13]
	andn	edi,ebp,edx
	vpaddd	ymm9,ymm3,ymm11
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	and	ebp,ecx
	vmovdqu	YMMWORD PTR[352+rsp],ymm9
	add	eax,r12d
	xor	ebp,edi
	add	esi,DWORD PTR[8+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[12+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	vpalignr	ymm8,ymm3,ymm2,8
	vpxor	ymm4,ymm4,ymm0
	add	ecx,DWORD PTR[32+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	vpxor	ymm4,ymm4,ymm5
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	vpxor	ymm4,ymm4,ymm8
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[36+r13]
	vpsrld	ymm8,ymm4,30
	vpslld	ymm4,ymm4,2
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	vpor	ymm4,ymm4,ymm8
	add	ebp,DWORD PTR[40+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	vpaddd	ymm9,ymm4,ymm11
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[44+r13]
	vmovdqu	YMMWORD PTR[384+rsp],ymm9
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[64+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	vpalignr	ymm8,ymm4,ymm3,8
	vpxor	ymm5,ymm5,ymm1
	add	edx,DWORD PTR[68+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	vpxor	ymm5,ymm5,ymm6
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	vpxor	ymm5,ymm5,ymm8
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[72+r13]
	vpsrld	ymm8,ymm5,30
	vpslld	ymm5,ymm5,2
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	vpor	ymm5,ymm5,ymm8
	add	ebx,DWORD PTR[76+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	vpaddd	ymm9,ymm5,ymm11
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[96+r13]
	vmovdqu	YMMWORD PTR[416+rsp],ymm9
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[100+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	vpalignr	ymm8,ymm5,ymm4,8
	vpxor	ymm6,ymm6,ymm2
	add	esi,DWORD PTR[104+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	vpxor	ymm6,ymm6,ymm7
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	vpxor	ymm6,ymm6,ymm8
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[108+r13]
	lea	r13,QWORD PTR[256+r13]
	vpsrld	ymm8,ymm6,30
	vpslld	ymm6,ymm6,2
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	vpor	ymm6,ymm6,ymm8
	add	ecx,DWORD PTR[((-128))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	vpaddd	ymm9,ymm6,ymm11
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-124))+r13]
	vmovdqu	YMMWORD PTR[448+rsp],ymm9
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-120))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	vpalignr	ymm8,ymm6,ymm5,8
	vpxor	ymm7,ymm7,ymm3
	add	eax,DWORD PTR[((-116))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	vpxor	ymm7,ymm7,ymm0
	vmovdqu	ymm11,YMMWORD PTR[32+r11]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	vpxor	ymm7,ymm7,ymm8
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-96))+r13]
	vpsrld	ymm8,ymm7,30
	vpslld	ymm7,ymm7,2
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	vpor	ymm7,ymm7,ymm8
	add	edx,DWORD PTR[((-92))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	vpaddd	ymm9,ymm7,ymm11
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[((-88))+r13]
	vmovdqu	YMMWORD PTR[480+rsp],ymm9
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-84))+r13]
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	and	ecx,edi
	jmp	$L$align32_2
ALIGN	32
$L$align32_2::
	vpalignr	ymm8,ymm7,ymm6,8
	vpxor	ymm0,ymm0,ymm4
	add	ebp,DWORD PTR[((-64))+r13]
	xor	ecx,esi
	vpxor	ymm0,ymm0,ymm1
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	vpxor	ymm0,ymm0,ymm8
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	vpsrld	ymm8,ymm0,30
	vpslld	ymm0,ymm0,2
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[((-60))+r13]
	xor	ebx,edx
	mov	edi,ecx
	xor	edi,edx
	vpor	ymm0,ymm0,ymm8
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	vpaddd	ymm9,ymm0,ymm11
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[((-56))+r13]
	xor	ebp,ecx
	vmovdqu	YMMWORD PTR[512+rsp],ymm9
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	add	edx,DWORD PTR[((-52))+r13]
	xor	eax,ebx
	mov	edi,ebp
	xor	edi,ebx
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	and	esi,edi
	add	ecx,DWORD PTR[((-32))+r13]
	xor	esi,ebp
	mov	edi,eax
	xor	edi,ebp
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	and	edx,edi
	vpalignr	ymm8,ymm0,ymm7,8
	vpxor	ymm1,ymm1,ymm5
	add	ebx,DWORD PTR[((-28))+r13]
	xor	edx,eax
	vpxor	ymm1,ymm1,ymm2
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	vpxor	ymm1,ymm1,ymm8
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	vpsrld	ymm8,ymm1,30
	vpslld	ymm1,ymm1,2
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[((-24))+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	vpor	ymm1,ymm1,ymm8
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	vpaddd	ymm9,ymm1,ymm11
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[((-20))+r13]
	xor	ebx,edx
	vmovdqu	YMMWORD PTR[544+rsp],ymm9
	mov	edi,ecx
	xor	edi,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[r13]
	xor	ebp,ecx
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	add	edx,DWORD PTR[4+r13]
	xor	eax,ebx
	mov	edi,ebp
	xor	edi,ebx
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	and	esi,edi
	vpalignr	ymm8,ymm1,ymm0,8
	vpxor	ymm2,ymm2,ymm6
	add	ecx,DWORD PTR[8+r13]
	xor	esi,ebp
	vpxor	ymm2,ymm2,ymm3
	mov	edi,eax
	xor	edi,ebp
	lea	ecx,DWORD PTR[rsi*1+rcx]
	vpxor	ymm2,ymm2,ymm8
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	vpsrld	ymm8,ymm2,30
	vpslld	ymm2,ymm2,2
	add	ecx,r12d
	and	edx,edi
	add	ebx,DWORD PTR[12+r13]
	xor	edx,eax
	mov	edi,esi
	xor	edi,eax
	vpor	ymm2,ymm2,ymm8
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	vpaddd	ymm9,ymm2,ymm11
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[32+r13]
	xor	ecx,esi
	vmovdqu	YMMWORD PTR[576+rsp],ymm9
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[36+r13]
	xor	ebx,edx
	mov	edi,ecx
	xor	edi,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[40+r13]
	xor	ebp,ecx
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	vpalignr	ymm8,ymm2,ymm1,8
	vpxor	ymm3,ymm3,ymm7
	add	edx,DWORD PTR[44+r13]
	xor	eax,ebx
	vpxor	ymm3,ymm3,ymm4
	mov	edi,ebp
	xor	edi,ebx
	lea	edx,DWORD PTR[rax*1+rdx]
	vpxor	ymm3,ymm3,ymm8
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	vpsrld	ymm8,ymm3,30
	vpslld	ymm3,ymm3,2
	add	edx,r12d
	and	esi,edi
	add	ecx,DWORD PTR[64+r13]
	xor	esi,ebp
	mov	edi,eax
	xor	edi,ebp
	vpor	ymm3,ymm3,ymm8
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	vpaddd	ymm9,ymm3,ymm11
	add	ecx,r12d
	and	edx,edi
	add	ebx,DWORD PTR[68+r13]
	xor	edx,eax
	vmovdqu	YMMWORD PTR[608+rsp],ymm9
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[72+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[76+r13]
	xor	ebx,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[96+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[100+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[104+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[108+r13]
	lea	r13,QWORD PTR[256+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-128))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[((-124))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-120))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[((-116))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[((-96))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-92))+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-88))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[((-84))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-64))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[((-60))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[((-56))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-52))+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-32))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[((-28))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-24))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[((-20))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	add	edx,r12d
	lea	r13,QWORD PTR[128+r9]
	lea	rdi,QWORD PTR[128+r9]
	cmp	r13,r10
	cmovae	r13,r9


	add	edx,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ebp,DWORD PTR[8+r8]
	mov	DWORD PTR[r8],edx
	add	ebx,DWORD PTR[12+r8]
	mov	DWORD PTR[4+r8],esi
	mov	eax,edx
	add	ecx,DWORD PTR[16+r8]
	mov	r12d,ebp
	mov	DWORD PTR[8+r8],ebp
	mov	edx,ebx

	mov	DWORD PTR[12+r8],ebx
	mov	ebp,esi
	mov	DWORD PTR[16+r8],ecx

	mov	esi,ecx
	mov	ecx,r12d


	cmp	r9,r10
	je	$L$done_avx2
	vmovdqu	ymm6,YMMWORD PTR[64+r11]
	cmp	rdi,r10
	ja	$L$ast_avx2

	vmovdqu	xmm0,XMMWORD PTR[((-64))+rdi]
	vmovdqu	xmm1,XMMWORD PTR[((-48))+rdi]
	vmovdqu	xmm2,XMMWORD PTR[((-32))+rdi]
	vmovdqu	xmm3,XMMWORD PTR[((-16))+rdi]
	vinserti128	ymm0,ymm0,XMMWORD PTR[r13],1
	vinserti128	ymm1,ymm1,XMMWORD PTR[16+r13],1
	vinserti128	ymm2,ymm2,XMMWORD PTR[32+r13],1
	vinserti128	ymm3,ymm3,XMMWORD PTR[48+r13],1
	jmp	$L$ast_avx2

ALIGN	32
$L$ast_avx2::
	lea	r13,QWORD PTR[((128+16))+rsp]
	rorx	ebx,ebp,2
	andn	edi,ebp,edx
	and	ebp,ecx
	xor	ebp,edi
	sub	r9,-128
	add	esi,DWORD PTR[((-128))+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[((-124))+r13]
	andn	edi,esi,ebx
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-120))+r13]
	andn	edi,edx,ebp
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	add	ecx,r12d
	xor	edx,edi
	add	ebx,DWORD PTR[((-116))+r13]
	andn	edi,ecx,eax
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	add	ebp,DWORD PTR[((-96))+r13]
	andn	edi,ebx,esi
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	and	ebx,edx
	add	ebp,r12d
	xor	ebx,edi
	add	eax,DWORD PTR[((-92))+r13]
	andn	edi,ebp,edx
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	and	ebp,ecx
	add	eax,r12d
	xor	ebp,edi
	add	esi,DWORD PTR[((-88))+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[((-84))+r13]
	andn	edi,esi,ebx
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-64))+r13]
	andn	edi,edx,ebp
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	add	ecx,r12d
	xor	edx,edi
	add	ebx,DWORD PTR[((-60))+r13]
	andn	edi,ecx,eax
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	add	ebp,DWORD PTR[((-56))+r13]
	andn	edi,ebx,esi
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	and	ebx,edx
	add	ebp,r12d
	xor	ebx,edi
	add	eax,DWORD PTR[((-52))+r13]
	andn	edi,ebp,edx
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	and	ebp,ecx
	add	eax,r12d
	xor	ebp,edi
	add	esi,DWORD PTR[((-32))+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[((-28))+r13]
	andn	edi,esi,ebx
	add	edx,eax
	rorx	r12d,esi,27
	rorx	eax,esi,2
	and	esi,ebp
	add	edx,r12d
	xor	esi,edi
	add	ecx,DWORD PTR[((-24))+r13]
	andn	edi,edx,ebp
	add	ecx,esi
	rorx	r12d,edx,27
	rorx	esi,edx,2
	and	edx,eax
	add	ecx,r12d
	xor	edx,edi
	add	ebx,DWORD PTR[((-20))+r13]
	andn	edi,ecx,eax
	add	ebx,edx
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	and	ecx,esi
	add	ebx,r12d
	xor	ecx,edi
	add	ebp,DWORD PTR[r13]
	andn	edi,ebx,esi
	add	ebp,ecx
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	and	ebx,edx
	add	ebp,r12d
	xor	ebx,edi
	add	eax,DWORD PTR[4+r13]
	andn	edi,ebp,edx
	add	eax,ebx
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	and	ebp,ecx
	add	eax,r12d
	xor	ebp,edi
	add	esi,DWORD PTR[8+r13]
	andn	edi,eax,ecx
	add	esi,ebp
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	and	eax,ebx
	add	esi,r12d
	xor	eax,edi
	add	edx,DWORD PTR[12+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[32+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[36+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[40+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[44+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[64+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	vmovdqu	ymm11,YMMWORD PTR[((-64))+r11]
	vpshufb	ymm0,ymm0,ymm6
	add	edx,DWORD PTR[68+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[72+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[76+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[96+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[100+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	vpshufb	ymm1,ymm1,ymm6
	vpaddd	ymm8,ymm0,ymm11
	add	esi,DWORD PTR[104+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[108+r13]
	lea	r13,QWORD PTR[256+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[((-128))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-124))+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-120))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	vmovdqu	YMMWORD PTR[rsp],ymm8
	vpshufb	ymm2,ymm2,ymm6
	vpaddd	ymm9,ymm1,ymm11
	add	eax,DWORD PTR[((-116))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-96))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[((-92))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	add	ecx,DWORD PTR[((-88))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-84))+r13]
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	and	ecx,edi
	vmovdqu	YMMWORD PTR[32+rsp],ymm9
	vpshufb	ymm3,ymm3,ymm6
	vpaddd	ymm6,ymm2,ymm11
	add	ebp,DWORD PTR[((-64))+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[((-60))+r13]
	xor	ebx,edx
	mov	edi,ecx
	xor	edi,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[((-56))+r13]
	xor	ebp,ecx
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	add	edx,DWORD PTR[((-52))+r13]
	xor	eax,ebx
	mov	edi,ebp
	xor	edi,ebx
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	and	esi,edi
	add	ecx,DWORD PTR[((-32))+r13]
	xor	esi,ebp
	mov	edi,eax
	xor	edi,ebp
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	and	edx,edi
	jmp	$L$align32_3
ALIGN	32
$L$align32_3::
	vmovdqu	YMMWORD PTR[64+rsp],ymm6
	vpaddd	ymm7,ymm3,ymm11
	add	ebx,DWORD PTR[((-28))+r13]
	xor	edx,eax
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[((-24))+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[((-20))+r13]
	xor	ebx,edx
	mov	edi,ecx
	xor	edi,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[r13]
	xor	ebp,ecx
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	add	edx,DWORD PTR[4+r13]
	xor	eax,ebx
	mov	edi,ebp
	xor	edi,ebx
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	and	esi,edi
	vmovdqu	YMMWORD PTR[96+rsp],ymm7
	add	ecx,DWORD PTR[8+r13]
	xor	esi,ebp
	mov	edi,eax
	xor	edi,ebp
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	and	edx,edi
	add	ebx,DWORD PTR[12+r13]
	xor	edx,eax
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[32+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[36+r13]
	xor	ebx,edx
	mov	edi,ecx
	xor	edi,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	and	ebp,edi
	add	esi,DWORD PTR[40+r13]
	xor	ebp,ecx
	mov	edi,ebx
	xor	edi,ecx
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	and	eax,edi
	vpalignr	ymm4,ymm1,ymm0,8
	add	edx,DWORD PTR[44+r13]
	xor	eax,ebx
	mov	edi,ebp
	xor	edi,ebx
	vpsrldq	ymm8,ymm3,4
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	vpxor	ymm4,ymm4,ymm0
	vpxor	ymm8,ymm8,ymm2
	xor	esi,ebp
	add	edx,r12d
	vpxor	ymm4,ymm4,ymm8
	and	esi,edi
	add	ecx,DWORD PTR[64+r13]
	xor	esi,ebp
	mov	edi,eax
	vpsrld	ymm8,ymm4,31
	xor	edi,ebp
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	vpslldq	ymm10,ymm4,12
	vpaddd	ymm4,ymm4,ymm4
	rorx	esi,edx,2
	xor	edx,eax
	vpsrld	ymm9,ymm10,30
	vpor	ymm4,ymm4,ymm8
	add	ecx,r12d
	and	edx,edi
	vpslld	ymm10,ymm10,2
	vpxor	ymm4,ymm4,ymm9
	add	ebx,DWORD PTR[68+r13]
	xor	edx,eax
	vpxor	ymm4,ymm4,ymm10
	mov	edi,esi
	xor	edi,eax
	lea	ebx,DWORD PTR[rdx*1+rbx]
	vpaddd	ymm9,ymm4,ymm11
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	vmovdqu	YMMWORD PTR[128+rsp],ymm9
	add	ebx,r12d
	and	ecx,edi
	add	ebp,DWORD PTR[72+r13]
	xor	ecx,esi
	mov	edi,edx
	xor	edi,esi
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	and	ebx,edi
	add	eax,DWORD PTR[76+r13]
	xor	ebx,edx
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	vpalignr	ymm5,ymm2,ymm1,8
	add	esi,DWORD PTR[96+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	vpsrldq	ymm8,ymm4,4
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm8,ymm8,ymm3
	add	edx,DWORD PTR[100+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	vpxor	ymm5,ymm5,ymm8
	rorx	r12d,esi,27
	rorx	eax,esi,2
	xor	esi,ebp
	add	edx,r12d
	vpsrld	ymm8,ymm5,31
	vmovdqu	ymm11,YMMWORD PTR[((-32))+r11]
	xor	esi,ebx
	add	ecx,DWORD PTR[104+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	vpslldq	ymm10,ymm5,12
	vpaddd	ymm5,ymm5,ymm5
	rorx	r12d,edx,27
	rorx	esi,edx,2
	vpsrld	ymm9,ymm10,30
	vpor	ymm5,ymm5,ymm8
	xor	edx,eax
	add	ecx,r12d
	vpslld	ymm10,ymm10,2
	vpxor	ymm5,ymm5,ymm9
	xor	edx,ebp
	add	ebx,DWORD PTR[108+r13]
	lea	r13,QWORD PTR[256+r13]
	vpxor	ymm5,ymm5,ymm10
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	vpaddd	ymm9,ymm5,ymm11
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	vmovdqu	YMMWORD PTR[160+rsp],ymm9
	add	ebp,DWORD PTR[((-128))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	vpalignr	ymm6,ymm3,ymm2,8
	add	eax,DWORD PTR[((-124))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	vpsrldq	ymm8,ymm5,4
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	vpxor	ymm6,ymm6,ymm2
	vpxor	ymm8,ymm8,ymm4
	add	esi,DWORD PTR[((-120))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	vpxor	ymm6,ymm6,ymm8
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	vpsrld	ymm8,ymm6,31
	xor	eax,ecx
	add	edx,DWORD PTR[((-116))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	vpslldq	ymm10,ymm6,12
	vpaddd	ymm6,ymm6,ymm6
	rorx	r12d,esi,27
	rorx	eax,esi,2
	vpsrld	ymm9,ymm10,30
	vpor	ymm6,ymm6,ymm8
	xor	esi,ebp
	add	edx,r12d
	vpslld	ymm10,ymm10,2
	vpxor	ymm6,ymm6,ymm9
	xor	esi,ebx
	add	ecx,DWORD PTR[((-96))+r13]
	vpxor	ymm6,ymm6,ymm10
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	vpaddd	ymm9,ymm6,ymm11
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	vmovdqu	YMMWORD PTR[192+rsp],ymm9
	add	ebx,DWORD PTR[((-92))+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	vpalignr	ymm7,ymm4,ymm3,8
	add	ebp,DWORD PTR[((-88))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	vpsrldq	ymm8,ymm6,4
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	vpxor	ymm7,ymm7,ymm3
	vpxor	ymm8,ymm8,ymm5
	add	eax,DWORD PTR[((-84))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	vpxor	ymm7,ymm7,ymm8
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	vpsrld	ymm8,ymm7,31
	xor	ebp,edx
	add	esi,DWORD PTR[((-64))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	vpslldq	ymm10,ymm7,12
	vpaddd	ymm7,ymm7,ymm7
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	vpsrld	ymm9,ymm10,30
	vpor	ymm7,ymm7,ymm8
	xor	eax,ebx
	add	esi,r12d
	vpslld	ymm10,ymm10,2
	vpxor	ymm7,ymm7,ymm9
	xor	eax,ecx
	add	edx,DWORD PTR[((-60))+r13]
	vpxor	ymm7,ymm7,ymm10
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	rorx	eax,esi,2
	vpaddd	ymm9,ymm7,ymm11
	xor	esi,ebp
	add	edx,r12d
	xor	esi,ebx
	vmovdqu	YMMWORD PTR[224+rsp],ymm9
	add	ecx,DWORD PTR[((-56))+r13]
	lea	ecx,DWORD PTR[rsi*1+rcx]
	rorx	r12d,edx,27
	rorx	esi,edx,2
	xor	edx,eax
	add	ecx,r12d
	xor	edx,ebp
	add	ebx,DWORD PTR[((-52))+r13]
	lea	ebx,DWORD PTR[rdx*1+rbx]
	rorx	r12d,ecx,27
	rorx	edx,ecx,2
	xor	ecx,esi
	add	ebx,r12d
	xor	ecx,eax
	add	ebp,DWORD PTR[((-32))+r13]
	lea	ebp,DWORD PTR[rbp*1+rcx]
	rorx	r12d,ebx,27
	rorx	ecx,ebx,2
	xor	ebx,edx
	add	ebp,r12d
	xor	ebx,esi
	add	eax,DWORD PTR[((-28))+r13]
	lea	eax,DWORD PTR[rbx*1+rax]
	rorx	r12d,ebp,27
	rorx	ebx,ebp,2
	xor	ebp,ecx
	add	eax,r12d
	xor	ebp,edx
	add	esi,DWORD PTR[((-24))+r13]
	lea	esi,DWORD PTR[rbp*1+rsi]
	rorx	r12d,eax,27
	rorx	ebp,eax,2
	xor	eax,ebx
	add	esi,r12d
	xor	eax,ecx
	add	edx,DWORD PTR[((-20))+r13]
	lea	edx,DWORD PTR[rax*1+rdx]
	rorx	r12d,esi,27
	add	edx,r12d
	lea	r13,QWORD PTR[128+rsp]


	add	edx,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ebp,DWORD PTR[8+r8]
	mov	DWORD PTR[r8],edx
	add	ebx,DWORD PTR[12+r8]
	mov	DWORD PTR[4+r8],esi
	mov	eax,edx
	add	ecx,DWORD PTR[16+r8]
	mov	r12d,ebp
	mov	DWORD PTR[8+r8],ebp
	mov	edx,ebx

	mov	DWORD PTR[12+r8],ebx
	mov	ebp,esi
	mov	DWORD PTR[16+r8],ecx

	mov	esi,ecx
	mov	ecx,r12d


	cmp	r9,r10
	jbe	$L$oop_avx2

$L$done_avx2::
	vzeroupper
	movaps	xmm6,XMMWORD PTR[((-40-96))+r14]
	movaps	xmm7,XMMWORD PTR[((-40-80))+r14]
	movaps	xmm8,XMMWORD PTR[((-40-64))+r14]
	movaps	xmm9,XMMWORD PTR[((-40-48))+r14]
	movaps	xmm10,XMMWORD PTR[((-40-32))+r14]
	movaps	xmm11,XMMWORD PTR[((-40-16))+r14]
	lea	rsi,QWORD PTR[r14]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$epilogue_avx2::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order_avx2::
sha1_block_data_order_avx2	ENDP
ALIGN	64
K_XX_XX::
	DD	05a827999h,05a827999h,05a827999h,05a827999h
	DD	05a827999h,05a827999h,05a827999h,05a827999h
	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h
	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h
	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch
	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch
	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h
	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
DB	0fh,0eh,0dh,0ch,0bh,0ah,09h,08h,07h,06h,05h,04h,03h,02h,01h,00h
DB	83,72,65,49,32,98,108,111,99,107,32,116,114,97,110,115
DB	102,111,114,109,32,102,111,114,32,120,56,54,95,54,52,44
DB	32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60
DB	97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114
DB	103,62,0
ALIGN	64
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
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	rax,QWORD PTR[64+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14

	jmp	$L$common_seh_tail
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
	jb	$L$common_seh_tail

	lea	r10,QWORD PTR[$L$epilogue_shaext]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[((-8-64))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,8
	DD	0a548f3fch

	jmp	$L$common_seh_tail
shaext_handler	ENDP

ALIGN	16
ssse3_handler	PROC PRIVATE
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
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	rax,QWORD PTR[232+r8]

	lea	rsi,QWORD PTR[((-40-96))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,12
	DD	0a548f3fch

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14

$L$common_seh_tail::
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
ssse3_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_sha1_block_data_order
	DD	imagerel $L$SEH_end_sha1_block_data_order
	DD	imagerel $L$SEH_info_sha1_block_data_order
	DD	imagerel $L$SEH_begin_sha1_block_data_order_shaext
	DD	imagerel $L$SEH_end_sha1_block_data_order_shaext
	DD	imagerel $L$SEH_info_sha1_block_data_order_shaext
	DD	imagerel $L$SEH_begin_sha1_block_data_order_ssse3
	DD	imagerel $L$SEH_end_sha1_block_data_order_ssse3
	DD	imagerel $L$SEH_info_sha1_block_data_order_ssse3
	DD	imagerel $L$SEH_begin_sha1_block_data_order_avx
	DD	imagerel $L$SEH_end_sha1_block_data_order_avx
	DD	imagerel $L$SEH_info_sha1_block_data_order_avx
	DD	imagerel $L$SEH_begin_sha1_block_data_order_avx2
	DD	imagerel $L$SEH_end_sha1_block_data_order_avx2
	DD	imagerel $L$SEH_info_sha1_block_data_order_avx2
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha1_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler
$L$SEH_info_sha1_block_data_order_shaext::
DB	9,0,0,0
	DD	imagerel shaext_handler
$L$SEH_info_sha1_block_data_order_ssse3::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_ssse3,imagerel $L$epilogue_ssse3
$L$SEH_info_sha1_block_data_order_avx::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_avx,imagerel $L$epilogue_avx
$L$SEH_info_sha1_block_data_order_avx2::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_avx2,imagerel $L$epilogue_avx2

.xdata	ENDS
END
