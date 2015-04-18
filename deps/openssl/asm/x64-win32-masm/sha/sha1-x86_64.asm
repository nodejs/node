OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
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
	test	r8d,512
	jz	$L$ialu
	jmp	_ssse3_shortcut

ALIGN	16
$L$ialu::
	push	rbx
	push	rbp
	push	r12
	push	r13
	mov	r11,rsp
	mov	r8,rdi
	sub	rsp,72
	mov	r9,rsi
	and	rsp,-64
	mov	r10,rdx
	mov	QWORD PTR[64+rsp],r11
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
	mov	DWORD PTR[rsp],edx
	mov	eax,r11d
	mov	ebp,DWORD PTR[4+r9]
	mov	ecx,esi
	xor	eax,r12d
	bswap	ebp
	rol	ecx,5
	lea	r13d,DWORD PTR[1518500249+r13*1+rdx]
	and	eax,edi
	mov	DWORD PTR[4+rsp],ebp
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	eax,edi
	mov	edx,DWORD PTR[8+r9]
	mov	ecx,r13d
	xor	eax,r11d
	bswap	edx
	rol	ecx,5
	lea	r12d,DWORD PTR[1518500249+r12*1+rbp]
	and	eax,esi
	mov	DWORD PTR[8+rsp],edx
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	eax,esi
	mov	ebp,DWORD PTR[12+r9]
	mov	ecx,r12d
	xor	eax,edi
	bswap	ebp
	rol	ecx,5
	lea	r11d,DWORD PTR[1518500249+r11*1+rdx]
	and	eax,r13d
	mov	DWORD PTR[12+rsp],ebp
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	eax,r13d
	mov	edx,DWORD PTR[16+r9]
	mov	ecx,r11d
	xor	eax,esi
	bswap	edx
	rol	ecx,5
	lea	edi,DWORD PTR[1518500249+rdi*1+rbp]
	and	eax,r12d
	mov	DWORD PTR[16+rsp],edx
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	eax,r12d
	mov	ebp,DWORD PTR[20+r9]
	mov	ecx,edi
	xor	eax,r13d
	bswap	ebp
	rol	ecx,5
	lea	esi,DWORD PTR[1518500249+rsi*1+rdx]
	and	eax,r11d
	mov	DWORD PTR[20+rsp],ebp
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	mov	eax,r11d
	mov	edx,DWORD PTR[24+r9]
	mov	ecx,esi
	xor	eax,r12d
	bswap	edx
	rol	ecx,5
	lea	r13d,DWORD PTR[1518500249+r13*1+rbp]
	and	eax,edi
	mov	DWORD PTR[24+rsp],edx
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	eax,edi
	mov	ebp,DWORD PTR[28+r9]
	mov	ecx,r13d
	xor	eax,r11d
	bswap	ebp
	rol	ecx,5
	lea	r12d,DWORD PTR[1518500249+r12*1+rdx]
	and	eax,esi
	mov	DWORD PTR[28+rsp],ebp
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	eax,esi
	mov	edx,DWORD PTR[32+r9]
	mov	ecx,r12d
	xor	eax,edi
	bswap	edx
	rol	ecx,5
	lea	r11d,DWORD PTR[1518500249+r11*1+rbp]
	and	eax,r13d
	mov	DWORD PTR[32+rsp],edx
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	eax,r13d
	mov	ebp,DWORD PTR[36+r9]
	mov	ecx,r11d
	xor	eax,esi
	bswap	ebp
	rol	ecx,5
	lea	edi,DWORD PTR[1518500249+rdi*1+rdx]
	and	eax,r12d
	mov	DWORD PTR[36+rsp],ebp
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	eax,r12d
	mov	edx,DWORD PTR[40+r9]
	mov	ecx,edi
	xor	eax,r13d
	bswap	edx
	rol	ecx,5
	lea	esi,DWORD PTR[1518500249+rsi*1+rbp]
	and	eax,r11d
	mov	DWORD PTR[40+rsp],edx
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	mov	eax,r11d
	mov	ebp,DWORD PTR[44+r9]
	mov	ecx,esi
	xor	eax,r12d
	bswap	ebp
	rol	ecx,5
	lea	r13d,DWORD PTR[1518500249+r13*1+rdx]
	and	eax,edi
	mov	DWORD PTR[44+rsp],ebp
	add	r13d,ecx
	xor	eax,r12d
	rol	edi,30
	add	r13d,eax
	mov	eax,edi
	mov	edx,DWORD PTR[48+r9]
	mov	ecx,r13d
	xor	eax,r11d
	bswap	edx
	rol	ecx,5
	lea	r12d,DWORD PTR[1518500249+r12*1+rbp]
	and	eax,esi
	mov	DWORD PTR[48+rsp],edx
	add	r12d,ecx
	xor	eax,r11d
	rol	esi,30
	add	r12d,eax
	mov	eax,esi
	mov	ebp,DWORD PTR[52+r9]
	mov	ecx,r12d
	xor	eax,edi
	bswap	ebp
	rol	ecx,5
	lea	r11d,DWORD PTR[1518500249+r11*1+rdx]
	and	eax,r13d
	mov	DWORD PTR[52+rsp],ebp
	add	r11d,ecx
	xor	eax,edi
	rol	r13d,30
	add	r11d,eax
	mov	eax,r13d
	mov	edx,DWORD PTR[56+r9]
	mov	ecx,r11d
	xor	eax,esi
	bswap	edx
	rol	ecx,5
	lea	edi,DWORD PTR[1518500249+rdi*1+rbp]
	and	eax,r12d
	mov	DWORD PTR[56+rsp],edx
	add	edi,ecx
	xor	eax,esi
	rol	r12d,30
	add	edi,eax
	mov	eax,r12d
	mov	ebp,DWORD PTR[60+r9]
	mov	ecx,edi
	xor	eax,r13d
	bswap	ebp
	rol	ecx,5
	lea	esi,DWORD PTR[1518500249+rsi*1+rdx]
	and	eax,r11d
	mov	DWORD PTR[60+rsp],ebp
	add	esi,ecx
	xor	eax,r13d
	rol	r11d,30
	add	esi,eax
	mov	edx,DWORD PTR[rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[8+rsp]
	xor	eax,r12d
	rol	ecx,5
	xor	edx,DWORD PTR[32+rsp]
	and	eax,edi
	lea	r13d,DWORD PTR[1518500249+r13*1+rbp]
	xor	edx,DWORD PTR[52+rsp]
	xor	eax,r12d
	rol	edx,1
	add	r13d,ecx
	rol	edi,30
	mov	DWORD PTR[rsp],edx
	add	r13d,eax
	mov	ebp,DWORD PTR[4+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[12+rsp]
	xor	eax,r11d
	rol	ecx,5
	xor	ebp,DWORD PTR[36+rsp]
	and	eax,esi
	lea	r12d,DWORD PTR[1518500249+r12*1+rdx]
	xor	ebp,DWORD PTR[56+rsp]
	xor	eax,r11d
	rol	ebp,1
	add	r12d,ecx
	rol	esi,30
	mov	DWORD PTR[4+rsp],ebp
	add	r12d,eax
	mov	edx,DWORD PTR[8+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,edi
	rol	ecx,5
	xor	edx,DWORD PTR[40+rsp]
	and	eax,r13d
	lea	r11d,DWORD PTR[1518500249+r11*1+rbp]
	xor	edx,DWORD PTR[60+rsp]
	xor	eax,edi
	rol	edx,1
	add	r11d,ecx
	rol	r13d,30
	mov	DWORD PTR[8+rsp],edx
	add	r11d,eax
	mov	ebp,DWORD PTR[12+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,esi
	rol	ecx,5
	xor	ebp,DWORD PTR[44+rsp]
	and	eax,r12d
	lea	edi,DWORD PTR[1518500249+rdi*1+rdx]
	xor	ebp,DWORD PTR[rsp]
	xor	eax,esi
	rol	ebp,1
	add	edi,ecx
	rol	r12d,30
	mov	DWORD PTR[12+rsp],ebp
	add	edi,eax
	mov	edx,DWORD PTR[16+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,r13d
	rol	ecx,5
	xor	edx,DWORD PTR[48+rsp]
	and	eax,r11d
	lea	esi,DWORD PTR[1518500249+rsi*1+rbp]
	xor	edx,DWORD PTR[4+rsp]
	xor	eax,r13d
	rol	edx,1
	add	esi,ecx
	rol	r11d,30
	mov	DWORD PTR[16+rsp],edx
	add	esi,eax
	mov	ebp,DWORD PTR[20+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[1859775393+r13*1+rdx]
	xor	ebp,DWORD PTR[52+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	ebp,DWORD PTR[8+rsp]
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	mov	DWORD PTR[20+rsp],ebp
	mov	edx,DWORD PTR[24+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[32+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[1859775393+r12*1+rbp]
	xor	edx,DWORD PTR[56+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	edx,DWORD PTR[12+rsp]
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	mov	DWORD PTR[24+rsp],edx
	mov	ebp,DWORD PTR[28+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[36+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[1859775393+r11*1+rdx]
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	ebp,DWORD PTR[16+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	mov	DWORD PTR[28+rsp],ebp
	mov	edx,DWORD PTR[32+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[1859775393+rdi*1+rbp]
	xor	edx,DWORD PTR[rsp]
	xor	eax,esi
	add	edi,ecx
	xor	edx,DWORD PTR[20+rsp]
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	mov	DWORD PTR[32+rsp],edx
	mov	ebp,DWORD PTR[36+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[1859775393+rsi*1+rdx]
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	ebp,DWORD PTR[24+rsp]
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	mov	DWORD PTR[36+rsp],ebp
	mov	edx,DWORD PTR[40+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[48+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[1859775393+r13*1+rbp]
	xor	edx,DWORD PTR[8+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	edx,DWORD PTR[28+rsp]
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	mov	DWORD PTR[40+rsp],edx
	mov	ebp,DWORD PTR[44+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[52+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[1859775393+r12*1+rdx]
	xor	ebp,DWORD PTR[12+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	ebp,DWORD PTR[32+rsp]
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	mov	DWORD PTR[44+rsp],ebp
	mov	edx,DWORD PTR[48+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[56+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[1859775393+r11*1+rbp]
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	edx,DWORD PTR[36+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	mov	DWORD PTR[48+rsp],edx
	mov	ebp,DWORD PTR[52+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[1859775393+rdi*1+rdx]
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	ebp,DWORD PTR[40+rsp]
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	mov	DWORD PTR[52+rsp],ebp
	mov	edx,DWORD PTR[56+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[1859775393+rsi*1+rbp]
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	edx,DWORD PTR[44+rsp]
	rol	r11d,30
	add	esi,eax
	rol	edx,1
	mov	DWORD PTR[56+rsp],edx
	mov	ebp,DWORD PTR[60+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[1859775393+r13*1+rdx]
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	ebp,DWORD PTR[48+rsp]
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	mov	DWORD PTR[60+rsp],ebp
	mov	edx,DWORD PTR[rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[8+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[1859775393+r12*1+rbp]
	xor	edx,DWORD PTR[32+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	edx,DWORD PTR[52+rsp]
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	mov	DWORD PTR[rsp],edx
	mov	ebp,DWORD PTR[4+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[12+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[1859775393+r11*1+rdx]
	xor	ebp,DWORD PTR[36+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	ebp,DWORD PTR[56+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	mov	DWORD PTR[4+rsp],ebp
	mov	edx,DWORD PTR[8+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[1859775393+rdi*1+rbp]
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	edx,DWORD PTR[60+rsp]
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	mov	DWORD PTR[8+rsp],edx
	mov	ebp,DWORD PTR[12+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[1859775393+rsi*1+rdx]
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	ebp,DWORD PTR[rsp]
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	mov	DWORD PTR[12+rsp],ebp
	mov	edx,DWORD PTR[16+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[1859775393+r13*1+rbp]
	xor	edx,DWORD PTR[48+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	edx,DWORD PTR[4+rsp]
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	mov	DWORD PTR[16+rsp],edx
	mov	ebp,DWORD PTR[20+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[1859775393+r12*1+rdx]
	xor	ebp,DWORD PTR[52+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	ebp,DWORD PTR[8+rsp]
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	mov	DWORD PTR[20+rsp],ebp
	mov	edx,DWORD PTR[24+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[32+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[1859775393+r11*1+rbp]
	xor	edx,DWORD PTR[56+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	edx,DWORD PTR[12+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	mov	DWORD PTR[24+rsp],edx
	mov	ebp,DWORD PTR[28+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[36+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[1859775393+rdi*1+rdx]
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	ebp,DWORD PTR[16+rsp]
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	mov	DWORD PTR[28+rsp],ebp
	mov	edx,DWORD PTR[32+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[1859775393+rsi*1+rbp]
	xor	edx,DWORD PTR[rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	edx,DWORD PTR[20+rsp]
	rol	r11d,30
	add	esi,eax
	rol	edx,1
	mov	DWORD PTR[32+rsp],edx
	mov	ebp,DWORD PTR[36+rsp]
	mov	eax,r11d
	mov	ebx,r11d
	xor	ebp,DWORD PTR[44+rsp]
	and	eax,r12d
	mov	ecx,esi
	xor	ebp,DWORD PTR[4+rsp]
	xor	ebx,r12d
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[24+rsp]
	add	r13d,eax
	and	ebx,edi
	rol	ebp,1
	add	r13d,ebx
	rol	edi,30
	mov	DWORD PTR[36+rsp],ebp
	add	r13d,ecx
	mov	edx,DWORD PTR[40+rsp]
	mov	eax,edi
	mov	ebx,edi
	xor	edx,DWORD PTR[48+rsp]
	and	eax,r11d
	mov	ecx,r13d
	xor	edx,DWORD PTR[8+rsp]
	xor	ebx,r11d
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[28+rsp]
	add	r12d,eax
	and	ebx,esi
	rol	edx,1
	add	r12d,ebx
	rol	esi,30
	mov	DWORD PTR[40+rsp],edx
	add	r12d,ecx
	mov	ebp,DWORD PTR[44+rsp]
	mov	eax,esi
	mov	ebx,esi
	xor	ebp,DWORD PTR[52+rsp]
	and	eax,edi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[12+rsp]
	xor	ebx,edi
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[32+rsp]
	add	r11d,eax
	and	ebx,r13d
	rol	ebp,1
	add	r11d,ebx
	rol	r13d,30
	mov	DWORD PTR[44+rsp],ebp
	add	r11d,ecx
	mov	edx,DWORD PTR[48+rsp]
	mov	eax,r13d
	mov	ebx,r13d
	xor	edx,DWORD PTR[56+rsp]
	and	eax,esi
	mov	ecx,r11d
	xor	edx,DWORD PTR[16+rsp]
	xor	ebx,esi
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[36+rsp]
	add	edi,eax
	and	ebx,r12d
	rol	edx,1
	add	edi,ebx
	rol	r12d,30
	mov	DWORD PTR[48+rsp],edx
	add	edi,ecx
	mov	ebp,DWORD PTR[52+rsp]
	mov	eax,r12d
	mov	ebx,r12d
	xor	ebp,DWORD PTR[60+rsp]
	and	eax,r13d
	mov	ecx,edi
	xor	ebp,DWORD PTR[20+rsp]
	xor	ebx,r13d
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[40+rsp]
	add	esi,eax
	and	ebx,r11d
	rol	ebp,1
	add	esi,ebx
	rol	r11d,30
	mov	DWORD PTR[52+rsp],ebp
	add	esi,ecx
	mov	edx,DWORD PTR[56+rsp]
	mov	eax,r11d
	mov	ebx,r11d
	xor	edx,DWORD PTR[rsp]
	and	eax,r12d
	mov	ecx,esi
	xor	edx,DWORD PTR[24+rsp]
	xor	ebx,r12d
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[44+rsp]
	add	r13d,eax
	and	ebx,edi
	rol	edx,1
	add	r13d,ebx
	rol	edi,30
	mov	DWORD PTR[56+rsp],edx
	add	r13d,ecx
	mov	ebp,DWORD PTR[60+rsp]
	mov	eax,edi
	mov	ebx,edi
	xor	ebp,DWORD PTR[4+rsp]
	and	eax,r11d
	mov	ecx,r13d
	xor	ebp,DWORD PTR[28+rsp]
	xor	ebx,r11d
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[48+rsp]
	add	r12d,eax
	and	ebx,esi
	rol	ebp,1
	add	r12d,ebx
	rol	esi,30
	mov	DWORD PTR[60+rsp],ebp
	add	r12d,ecx
	mov	edx,DWORD PTR[rsp]
	mov	eax,esi
	mov	ebx,esi
	xor	edx,DWORD PTR[8+rsp]
	and	eax,edi
	mov	ecx,r12d
	xor	edx,DWORD PTR[32+rsp]
	xor	ebx,edi
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[52+rsp]
	add	r11d,eax
	and	ebx,r13d
	rol	edx,1
	add	r11d,ebx
	rol	r13d,30
	mov	DWORD PTR[rsp],edx
	add	r11d,ecx
	mov	ebp,DWORD PTR[4+rsp]
	mov	eax,r13d
	mov	ebx,r13d
	xor	ebp,DWORD PTR[12+rsp]
	and	eax,esi
	mov	ecx,r11d
	xor	ebp,DWORD PTR[36+rsp]
	xor	ebx,esi
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[56+rsp]
	add	edi,eax
	and	ebx,r12d
	rol	ebp,1
	add	edi,ebx
	rol	r12d,30
	mov	DWORD PTR[4+rsp],ebp
	add	edi,ecx
	mov	edx,DWORD PTR[8+rsp]
	mov	eax,r12d
	mov	ebx,r12d
	xor	edx,DWORD PTR[16+rsp]
	and	eax,r13d
	mov	ecx,edi
	xor	edx,DWORD PTR[40+rsp]
	xor	ebx,r13d
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[60+rsp]
	add	esi,eax
	and	ebx,r11d
	rol	edx,1
	add	esi,ebx
	rol	r11d,30
	mov	DWORD PTR[8+rsp],edx
	add	esi,ecx
	mov	ebp,DWORD PTR[12+rsp]
	mov	eax,r11d
	mov	ebx,r11d
	xor	ebp,DWORD PTR[20+rsp]
	and	eax,r12d
	mov	ecx,esi
	xor	ebp,DWORD PTR[44+rsp]
	xor	ebx,r12d
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[rsp]
	add	r13d,eax
	and	ebx,edi
	rol	ebp,1
	add	r13d,ebx
	rol	edi,30
	mov	DWORD PTR[12+rsp],ebp
	add	r13d,ecx
	mov	edx,DWORD PTR[16+rsp]
	mov	eax,edi
	mov	ebx,edi
	xor	edx,DWORD PTR[24+rsp]
	and	eax,r11d
	mov	ecx,r13d
	xor	edx,DWORD PTR[48+rsp]
	xor	ebx,r11d
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[4+rsp]
	add	r12d,eax
	and	ebx,esi
	rol	edx,1
	add	r12d,ebx
	rol	esi,30
	mov	DWORD PTR[16+rsp],edx
	add	r12d,ecx
	mov	ebp,DWORD PTR[20+rsp]
	mov	eax,esi
	mov	ebx,esi
	xor	ebp,DWORD PTR[28+rsp]
	and	eax,edi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[52+rsp]
	xor	ebx,edi
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[8+rsp]
	add	r11d,eax
	and	ebx,r13d
	rol	ebp,1
	add	r11d,ebx
	rol	r13d,30
	mov	DWORD PTR[20+rsp],ebp
	add	r11d,ecx
	mov	edx,DWORD PTR[24+rsp]
	mov	eax,r13d
	mov	ebx,r13d
	xor	edx,DWORD PTR[32+rsp]
	and	eax,esi
	mov	ecx,r11d
	xor	edx,DWORD PTR[56+rsp]
	xor	ebx,esi
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[12+rsp]
	add	edi,eax
	and	ebx,r12d
	rol	edx,1
	add	edi,ebx
	rol	r12d,30
	mov	DWORD PTR[24+rsp],edx
	add	edi,ecx
	mov	ebp,DWORD PTR[28+rsp]
	mov	eax,r12d
	mov	ebx,r12d
	xor	ebp,DWORD PTR[36+rsp]
	and	eax,r13d
	mov	ecx,edi
	xor	ebp,DWORD PTR[60+rsp]
	xor	ebx,r13d
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[16+rsp]
	add	esi,eax
	and	ebx,r11d
	rol	ebp,1
	add	esi,ebx
	rol	r11d,30
	mov	DWORD PTR[28+rsp],ebp
	add	esi,ecx
	mov	edx,DWORD PTR[32+rsp]
	mov	eax,r11d
	mov	ebx,r11d
	xor	edx,DWORD PTR[40+rsp]
	and	eax,r12d
	mov	ecx,esi
	xor	edx,DWORD PTR[rsp]
	xor	ebx,r12d
	lea	r13d,DWORD PTR[((-1894007588))+r13*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[20+rsp]
	add	r13d,eax
	and	ebx,edi
	rol	edx,1
	add	r13d,ebx
	rol	edi,30
	mov	DWORD PTR[32+rsp],edx
	add	r13d,ecx
	mov	ebp,DWORD PTR[36+rsp]
	mov	eax,edi
	mov	ebx,edi
	xor	ebp,DWORD PTR[44+rsp]
	and	eax,r11d
	mov	ecx,r13d
	xor	ebp,DWORD PTR[4+rsp]
	xor	ebx,r11d
	lea	r12d,DWORD PTR[((-1894007588))+r12*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[24+rsp]
	add	r12d,eax
	and	ebx,esi
	rol	ebp,1
	add	r12d,ebx
	rol	esi,30
	mov	DWORD PTR[36+rsp],ebp
	add	r12d,ecx
	mov	edx,DWORD PTR[40+rsp]
	mov	eax,esi
	mov	ebx,esi
	xor	edx,DWORD PTR[48+rsp]
	and	eax,edi
	mov	ecx,r12d
	xor	edx,DWORD PTR[8+rsp]
	xor	ebx,edi
	lea	r11d,DWORD PTR[((-1894007588))+r11*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[28+rsp]
	add	r11d,eax
	and	ebx,r13d
	rol	edx,1
	add	r11d,ebx
	rol	r13d,30
	mov	DWORD PTR[40+rsp],edx
	add	r11d,ecx
	mov	ebp,DWORD PTR[44+rsp]
	mov	eax,r13d
	mov	ebx,r13d
	xor	ebp,DWORD PTR[52+rsp]
	and	eax,esi
	mov	ecx,r11d
	xor	ebp,DWORD PTR[12+rsp]
	xor	ebx,esi
	lea	edi,DWORD PTR[((-1894007588))+rdi*1+rdx]
	rol	ecx,5
	xor	ebp,DWORD PTR[32+rsp]
	add	edi,eax
	and	ebx,r12d
	rol	ebp,1
	add	edi,ebx
	rol	r12d,30
	mov	DWORD PTR[44+rsp],ebp
	add	edi,ecx
	mov	edx,DWORD PTR[48+rsp]
	mov	eax,r12d
	mov	ebx,r12d
	xor	edx,DWORD PTR[56+rsp]
	and	eax,r13d
	mov	ecx,edi
	xor	edx,DWORD PTR[16+rsp]
	xor	ebx,r13d
	lea	esi,DWORD PTR[((-1894007588))+rsi*1+rbp]
	rol	ecx,5
	xor	edx,DWORD PTR[36+rsp]
	add	esi,eax
	and	ebx,r11d
	rol	edx,1
	add	esi,ebx
	rol	r11d,30
	mov	DWORD PTR[48+rsp],edx
	add	esi,ecx
	mov	ebp,DWORD PTR[52+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rdx]
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	ebp,DWORD PTR[40+rsp]
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	mov	DWORD PTR[52+rsp],ebp
	mov	edx,DWORD PTR[56+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rbp]
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	edx,DWORD PTR[44+rsp]
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	mov	DWORD PTR[56+rsp],edx
	mov	ebp,DWORD PTR[60+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rdx]
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	ebp,DWORD PTR[48+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	mov	DWORD PTR[60+rsp],ebp
	mov	edx,DWORD PTR[rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	edx,DWORD PTR[8+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rbp]
	xor	edx,DWORD PTR[32+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	edx,DWORD PTR[52+rsp]
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	mov	DWORD PTR[rsp],edx
	mov	ebp,DWORD PTR[4+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	ebp,DWORD PTR[12+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rdx]
	xor	ebp,DWORD PTR[36+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	ebp,DWORD PTR[56+rsp]
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	mov	DWORD PTR[4+rsp],ebp
	mov	edx,DWORD PTR[8+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rbp]
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	edx,DWORD PTR[60+rsp]
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	mov	DWORD PTR[8+rsp],edx
	mov	ebp,DWORD PTR[12+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rdx]
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	ebp,DWORD PTR[rsp]
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	mov	DWORD PTR[12+rsp],ebp
	mov	edx,DWORD PTR[16+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rbp]
	xor	edx,DWORD PTR[48+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	edx,DWORD PTR[4+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	mov	DWORD PTR[16+rsp],edx
	mov	ebp,DWORD PTR[20+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rdx]
	xor	ebp,DWORD PTR[52+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	ebp,DWORD PTR[8+rsp]
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	mov	DWORD PTR[20+rsp],ebp
	mov	edx,DWORD PTR[24+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	edx,DWORD PTR[32+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rbp]
	xor	edx,DWORD PTR[56+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	edx,DWORD PTR[12+rsp]
	rol	r11d,30
	add	esi,eax
	rol	edx,1
	mov	DWORD PTR[24+rsp],edx
	mov	ebp,DWORD PTR[28+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	ebp,DWORD PTR[36+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rdx]
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	ebp,DWORD PTR[16+rsp]
	rol	edi,30
	add	r13d,eax
	rol	ebp,1
	mov	DWORD PTR[28+rsp],ebp
	mov	edx,DWORD PTR[32+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	edx,DWORD PTR[40+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rbp]
	xor	edx,DWORD PTR[rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	edx,DWORD PTR[20+rsp]
	rol	esi,30
	add	r12d,eax
	rol	edx,1
	mov	DWORD PTR[32+rsp],edx
	mov	ebp,DWORD PTR[36+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	ebp,DWORD PTR[44+rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rdx]
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	ebp,DWORD PTR[24+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	ebp,1
	mov	DWORD PTR[36+rsp],ebp
	mov	edx,DWORD PTR[40+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	edx,DWORD PTR[48+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rbp]
	xor	edx,DWORD PTR[8+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	edx,DWORD PTR[28+rsp]
	rol	r12d,30
	add	edi,eax
	rol	edx,1
	mov	DWORD PTR[40+rsp],edx
	mov	ebp,DWORD PTR[44+rsp]
	mov	eax,r12d
	mov	ecx,edi
	xor	ebp,DWORD PTR[52+rsp]
	xor	eax,r11d
	rol	ecx,5
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rdx]
	xor	ebp,DWORD PTR[12+rsp]
	xor	eax,r13d
	add	esi,ecx
	xor	ebp,DWORD PTR[32+rsp]
	rol	r11d,30
	add	esi,eax
	rol	ebp,1
	mov	DWORD PTR[44+rsp],ebp
	mov	edx,DWORD PTR[48+rsp]
	mov	eax,r11d
	mov	ecx,esi
	xor	edx,DWORD PTR[56+rsp]
	xor	eax,edi
	rol	ecx,5
	lea	r13d,DWORD PTR[((-899497514))+r13*1+rbp]
	xor	edx,DWORD PTR[16+rsp]
	xor	eax,r12d
	add	r13d,ecx
	xor	edx,DWORD PTR[36+rsp]
	rol	edi,30
	add	r13d,eax
	rol	edx,1
	mov	DWORD PTR[48+rsp],edx
	mov	ebp,DWORD PTR[52+rsp]
	mov	eax,edi
	mov	ecx,r13d
	xor	ebp,DWORD PTR[60+rsp]
	xor	eax,esi
	rol	ecx,5
	lea	r12d,DWORD PTR[((-899497514))+r12*1+rdx]
	xor	ebp,DWORD PTR[20+rsp]
	xor	eax,r11d
	add	r12d,ecx
	xor	ebp,DWORD PTR[40+rsp]
	rol	esi,30
	add	r12d,eax
	rol	ebp,1
	mov	edx,DWORD PTR[56+rsp]
	mov	eax,esi
	mov	ecx,r12d
	xor	edx,DWORD PTR[rsp]
	xor	eax,r13d
	rol	ecx,5
	lea	r11d,DWORD PTR[((-899497514))+r11*1+rbp]
	xor	edx,DWORD PTR[24+rsp]
	xor	eax,edi
	add	r11d,ecx
	xor	edx,DWORD PTR[44+rsp]
	rol	r13d,30
	add	r11d,eax
	rol	edx,1
	mov	ebp,DWORD PTR[60+rsp]
	mov	eax,r13d
	mov	ecx,r11d
	xor	ebp,DWORD PTR[4+rsp]
	xor	eax,r12d
	rol	ecx,5
	lea	edi,DWORD PTR[((-899497514))+rdi*1+rdx]
	xor	ebp,DWORD PTR[28+rsp]
	xor	eax,esi
	add	edi,ecx
	xor	ebp,DWORD PTR[48+rsp]
	rol	r12d,30
	add	edi,eax
	rol	ebp,1
	mov	eax,r12d
	mov	ecx,edi
	xor	eax,r11d
	lea	esi,DWORD PTR[((-899497514))+rsi*1+rbp]
	rol	ecx,5
	xor	eax,r13d
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
	mov	r13,QWORD PTR[rsi]
	mov	r12,QWORD PTR[8+rsi]
	mov	rbp,QWORD PTR[16+rsi]
	mov	rbx,QWORD PTR[24+rsi]
	lea	rsp,QWORD PTR[32+rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order::
sha1_block_data_order	ENDP

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
	push	rbx
	push	rbp
	push	r12
	lea	rsp,QWORD PTR[((-144))+rsp]
	movaps	XMMWORD PTR[(64+0)+rsp],xmm6
	movaps	XMMWORD PTR[(64+16)+rsp],xmm7
	movaps	XMMWORD PTR[(64+32)+rsp],xmm8
	movaps	XMMWORD PTR[(64+48)+rsp],xmm9
	movaps	XMMWORD PTR[(64+64)+rsp],xmm10
$L$prologue_ssse3::
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rdx

	shl	r10,6
	add	r10,r9
	lea	r11,QWORD PTR[K_XX_XX]

	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	esi,ebx
	mov	ebp,DWORD PTR[16+r8]

	movdqa	xmm6,XMMWORD PTR[64+r11]
	movdqa	xmm9,XMMWORD PTR[r11]
	movdqu	xmm0,XMMWORD PTR[r9]
	movdqu	xmm1,XMMWORD PTR[16+r9]
	movdqu	xmm2,XMMWORD PTR[32+r9]
	movdqu	xmm3,XMMWORD PTR[48+r9]
DB	102,15,56,0,198
	add	r9,64
DB	102,15,56,0,206
DB	102,15,56,0,214
DB	102,15,56,0,222
	paddd	xmm0,xmm9
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
	movdqa	xmm4,xmm1
	add	ebp,DWORD PTR[rsp]
	xor	ecx,edx
	movdqa	xmm8,xmm3
DB	102,15,58,15,224,8
	mov	edi,eax
	rol	eax,5
	paddd	xmm9,xmm3
	and	esi,ecx
	xor	ecx,edx
	psrldq	xmm8,4
	xor	esi,edx
	add	ebp,eax
	pxor	xmm4,xmm0
	ror	ebx,2
	add	ebp,esi
	pxor	xmm8,xmm2
	add	edx,DWORD PTR[4+rsp]
	xor	ebx,ecx
	mov	esi,ebp
	rol	ebp,5
	pxor	xmm4,xmm8
	and	edi,ebx
	xor	ebx,ecx
	movdqa	XMMWORD PTR[48+rsp],xmm9
	xor	edi,ecx
	add	edx,ebp
	movdqa	xmm10,xmm4
	movdqa	xmm8,xmm4
	ror	eax,7
	add	edx,edi
	add	ecx,DWORD PTR[8+rsp]
	xor	eax,ebx
	pslldq	xmm10,12
	paddd	xmm4,xmm4
	mov	edi,edx
	rol	edx,5
	and	esi,eax
	xor	eax,ebx
	psrld	xmm8,31
	xor	esi,ebx
	add	ecx,edx
	movdqa	xmm9,xmm10
	ror	ebp,7
	add	ecx,esi
	psrld	xmm10,30
	por	xmm4,xmm8
	add	ebx,DWORD PTR[12+rsp]
	xor	ebp,eax
	mov	esi,ecx
	rol	ecx,5
	pslld	xmm9,2
	pxor	xmm4,xmm10
	and	edi,ebp
	xor	ebp,eax
	movdqa	xmm10,XMMWORD PTR[r11]
	xor	edi,eax
	add	ebx,ecx
	pxor	xmm4,xmm9
	ror	edx,7
	add	ebx,edi
	movdqa	xmm5,xmm2
	add	eax,DWORD PTR[16+rsp]
	xor	edx,ebp
	movdqa	xmm9,xmm4
DB	102,15,58,15,233,8
	mov	edi,ebx
	rol	ebx,5
	paddd	xmm10,xmm4
	and	esi,edx
	xor	edx,ebp
	psrldq	xmm9,4
	xor	esi,ebp
	add	eax,ebx
	pxor	xmm5,xmm1
	ror	ecx,7
	add	eax,esi
	pxor	xmm9,xmm3
	add	ebp,DWORD PTR[20+rsp]
	xor	ecx,edx
	mov	esi,eax
	rol	eax,5
	pxor	xmm5,xmm9
	and	edi,ecx
	xor	ecx,edx
	movdqa	XMMWORD PTR[rsp],xmm10
	xor	edi,edx
	add	ebp,eax
	movdqa	xmm8,xmm5
	movdqa	xmm9,xmm5
	ror	ebx,7
	add	ebp,edi
	add	edx,DWORD PTR[24+rsp]
	xor	ebx,ecx
	pslldq	xmm8,12
	paddd	xmm5,xmm5
	mov	edi,ebp
	rol	ebp,5
	and	esi,ebx
	xor	ebx,ecx
	psrld	xmm9,31
	xor	esi,ecx
	add	edx,ebp
	movdqa	xmm10,xmm8
	ror	eax,7
	add	edx,esi
	psrld	xmm8,30
	por	xmm5,xmm9
	add	ecx,DWORD PTR[28+rsp]
	xor	eax,ebx
	mov	esi,edx
	rol	edx,5
	pslld	xmm10,2
	pxor	xmm5,xmm8
	and	edi,eax
	xor	eax,ebx
	movdqa	xmm8,XMMWORD PTR[16+r11]
	xor	edi,ebx
	add	ecx,edx
	pxor	xmm5,xmm10
	ror	ebp,7
	add	ecx,edi
	movdqa	xmm6,xmm3
	add	ebx,DWORD PTR[32+rsp]
	xor	ebp,eax
	movdqa	xmm10,xmm5
DB	102,15,58,15,242,8
	mov	edi,ecx
	rol	ecx,5
	paddd	xmm8,xmm5
	and	esi,ebp
	xor	ebp,eax
	psrldq	xmm10,4
	xor	esi,eax
	add	ebx,ecx
	pxor	xmm6,xmm2
	ror	edx,7
	add	ebx,esi
	pxor	xmm10,xmm4
	add	eax,DWORD PTR[36+rsp]
	xor	edx,ebp
	mov	esi,ebx
	rol	ebx,5
	pxor	xmm6,xmm10
	and	edi,edx
	xor	edx,ebp
	movdqa	XMMWORD PTR[16+rsp],xmm8
	xor	edi,ebp
	add	eax,ebx
	movdqa	xmm9,xmm6
	movdqa	xmm10,xmm6
	ror	ecx,7
	add	eax,edi
	add	ebp,DWORD PTR[40+rsp]
	xor	ecx,edx
	pslldq	xmm9,12
	paddd	xmm6,xmm6
	mov	edi,eax
	rol	eax,5
	and	esi,ecx
	xor	ecx,edx
	psrld	xmm10,31
	xor	esi,edx
	add	ebp,eax
	movdqa	xmm8,xmm9
	ror	ebx,7
	add	ebp,esi
	psrld	xmm9,30
	por	xmm6,xmm10
	add	edx,DWORD PTR[44+rsp]
	xor	ebx,ecx
	mov	esi,ebp
	rol	ebp,5
	pslld	xmm8,2
	pxor	xmm6,xmm9
	and	edi,ebx
	xor	ebx,ecx
	movdqa	xmm9,XMMWORD PTR[16+r11]
	xor	edi,ecx
	add	edx,ebp
	pxor	xmm6,xmm8
	ror	eax,7
	add	edx,edi
	movdqa	xmm7,xmm4
	add	ecx,DWORD PTR[48+rsp]
	xor	eax,ebx
	movdqa	xmm8,xmm6
DB	102,15,58,15,251,8
	mov	edi,edx
	rol	edx,5
	paddd	xmm9,xmm6
	and	esi,eax
	xor	eax,ebx
	psrldq	xmm8,4
	xor	esi,ebx
	add	ecx,edx
	pxor	xmm7,xmm3
	ror	ebp,7
	add	ecx,esi
	pxor	xmm8,xmm5
	add	ebx,DWORD PTR[52+rsp]
	xor	ebp,eax
	mov	esi,ecx
	rol	ecx,5
	pxor	xmm7,xmm8
	and	edi,ebp
	xor	ebp,eax
	movdqa	XMMWORD PTR[32+rsp],xmm9
	xor	edi,eax
	add	ebx,ecx
	movdqa	xmm10,xmm7
	movdqa	xmm8,xmm7
	ror	edx,7
	add	ebx,edi
	add	eax,DWORD PTR[56+rsp]
	xor	edx,ebp
	pslldq	xmm10,12
	paddd	xmm7,xmm7
	mov	edi,ebx
	rol	ebx,5
	and	esi,edx
	xor	edx,ebp
	psrld	xmm8,31
	xor	esi,ebp
	add	eax,ebx
	movdqa	xmm9,xmm10
	ror	ecx,7
	add	eax,esi
	psrld	xmm10,30
	por	xmm7,xmm8
	add	ebp,DWORD PTR[60+rsp]
	xor	ecx,edx
	mov	esi,eax
	rol	eax,5
	pslld	xmm9,2
	pxor	xmm7,xmm10
	and	edi,ecx
	xor	ecx,edx
	movdqa	xmm10,XMMWORD PTR[16+r11]
	xor	edi,edx
	add	ebp,eax
	pxor	xmm7,xmm9
	ror	ebx,7
	add	ebp,edi
	movdqa	xmm9,xmm7
	add	edx,DWORD PTR[rsp]
	pxor	xmm0,xmm4
DB	102,68,15,58,15,206,8
	xor	ebx,ecx
	mov	edi,ebp
	rol	ebp,5
	pxor	xmm0,xmm1
	and	esi,ebx
	xor	ebx,ecx
	movdqa	xmm8,xmm10
	paddd	xmm10,xmm7
	xor	esi,ecx
	add	edx,ebp
	pxor	xmm0,xmm9
	ror	eax,7
	add	edx,esi
	add	ecx,DWORD PTR[4+rsp]
	xor	eax,ebx
	movdqa	xmm9,xmm0
	movdqa	XMMWORD PTR[48+rsp],xmm10
	mov	esi,edx
	rol	edx,5
	and	edi,eax
	xor	eax,ebx
	pslld	xmm0,2
	xor	edi,ebx
	add	ecx,edx
	psrld	xmm9,30
	ror	ebp,7
	add	ecx,edi
	add	ebx,DWORD PTR[8+rsp]
	xor	ebp,eax
	mov	edi,ecx
	rol	ecx,5
	por	xmm0,xmm9
	and	esi,ebp
	xor	ebp,eax
	movdqa	xmm10,xmm0
	xor	esi,eax
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	add	eax,DWORD PTR[12+rsp]
	xor	edx,ebp
	mov	esi,ebx
	rol	ebx,5
	and	edi,edx
	xor	edx,ebp
	xor	edi,ebp
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	add	ebp,DWORD PTR[16+rsp]
	pxor	xmm1,xmm5
DB	102,68,15,58,15,215,8
	xor	esi,edx
	mov	edi,eax
	rol	eax,5
	pxor	xmm1,xmm2
	xor	esi,ecx
	add	ebp,eax
	movdqa	xmm9,xmm8
	paddd	xmm8,xmm0
	ror	ebx,7
	add	ebp,esi
	pxor	xmm1,xmm10
	add	edx,DWORD PTR[20+rsp]
	xor	edi,ecx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm10,xmm1
	movdqa	XMMWORD PTR[rsp],xmm8
	xor	edi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,edi
	pslld	xmm1,2
	add	ecx,DWORD PTR[24+rsp]
	xor	esi,ebx
	psrld	xmm10,30
	mov	edi,edx
	rol	edx,5
	xor	esi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,esi
	por	xmm1,xmm10
	add	ebx,DWORD PTR[28+rsp]
	xor	edi,eax
	movdqa	xmm8,xmm1
	mov	esi,ecx
	rol	ecx,5
	xor	edi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,edi
	add	eax,DWORD PTR[32+rsp]
	pxor	xmm2,xmm6
DB	102,68,15,58,15,192,8
	xor	esi,ebp
	mov	edi,ebx
	rol	ebx,5
	pxor	xmm2,xmm3
	xor	esi,edx
	add	eax,ebx
	movdqa	xmm10,XMMWORD PTR[32+r11]
	paddd	xmm9,xmm1
	ror	ecx,7
	add	eax,esi
	pxor	xmm2,xmm8
	add	ebp,DWORD PTR[36+rsp]
	xor	edi,edx
	mov	esi,eax
	rol	eax,5
	movdqa	xmm8,xmm2
	movdqa	XMMWORD PTR[16+rsp],xmm9
	xor	edi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,edi
	pslld	xmm2,2
	add	edx,DWORD PTR[40+rsp]
	xor	esi,ecx
	psrld	xmm8,30
	mov	edi,ebp
	rol	ebp,5
	xor	esi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,esi
	por	xmm2,xmm8
	add	ecx,DWORD PTR[44+rsp]
	xor	edi,ebx
	movdqa	xmm9,xmm2
	mov	esi,edx
	rol	edx,5
	xor	edi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,edi
	add	ebx,DWORD PTR[48+rsp]
	pxor	xmm3,xmm7
DB	102,68,15,58,15,201,8
	xor	esi,eax
	mov	edi,ecx
	rol	ecx,5
	pxor	xmm3,xmm4
	xor	esi,ebp
	add	ebx,ecx
	movdqa	xmm8,xmm10
	paddd	xmm10,xmm2
	ror	edx,7
	add	ebx,esi
	pxor	xmm3,xmm9
	add	eax,DWORD PTR[52+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	movdqa	xmm9,xmm3
	movdqa	XMMWORD PTR[32+rsp],xmm10
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	pslld	xmm3,2
	add	ebp,DWORD PTR[56+rsp]
	xor	esi,edx
	psrld	xmm9,30
	mov	edi,eax
	rol	eax,5
	xor	esi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,esi
	por	xmm3,xmm9
	add	edx,DWORD PTR[60+rsp]
	xor	edi,ecx
	movdqa	xmm10,xmm3
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,edi
	add	ecx,DWORD PTR[rsp]
	pxor	xmm4,xmm0
DB	102,68,15,58,15,210,8
	xor	esi,ebx
	mov	edi,edx
	rol	edx,5
	pxor	xmm4,xmm5
	xor	esi,eax
	add	ecx,edx
	movdqa	xmm9,xmm8
	paddd	xmm8,xmm3
	ror	ebp,7
	add	ecx,esi
	pxor	xmm4,xmm10
	add	ebx,DWORD PTR[4+rsp]
	xor	edi,eax
	mov	esi,ecx
	rol	ecx,5
	movdqa	xmm10,xmm4
	movdqa	XMMWORD PTR[48+rsp],xmm8
	xor	edi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,edi
	pslld	xmm4,2
	add	eax,DWORD PTR[8+rsp]
	xor	esi,ebp
	psrld	xmm10,30
	mov	edi,ebx
	rol	ebx,5
	xor	esi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,esi
	por	xmm4,xmm10
	add	ebp,DWORD PTR[12+rsp]
	xor	edi,edx
	movdqa	xmm8,xmm4
	mov	esi,eax
	rol	eax,5
	xor	edi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,edi
	add	edx,DWORD PTR[16+rsp]
	pxor	xmm5,xmm1
DB	102,68,15,58,15,195,8
	xor	esi,ecx
	mov	edi,ebp
	rol	ebp,5
	pxor	xmm5,xmm6
	xor	esi,ebx
	add	edx,ebp
	movdqa	xmm10,xmm9
	paddd	xmm9,xmm4
	ror	eax,7
	add	edx,esi
	pxor	xmm5,xmm8
	add	ecx,DWORD PTR[20+rsp]
	xor	edi,ebx
	mov	esi,edx
	rol	edx,5
	movdqa	xmm8,xmm5
	movdqa	XMMWORD PTR[rsp],xmm9
	xor	edi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,edi
	pslld	xmm5,2
	add	ebx,DWORD PTR[24+rsp]
	xor	esi,eax
	psrld	xmm8,30
	mov	edi,ecx
	rol	ecx,5
	xor	esi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	por	xmm5,xmm8
	add	eax,DWORD PTR[28+rsp]
	xor	edi,ebp
	movdqa	xmm9,xmm5
	mov	esi,ebx
	rol	ebx,5
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	mov	edi,ecx
	pxor	xmm6,xmm2
DB	102,68,15,58,15,204,8
	xor	ecx,edx
	add	ebp,DWORD PTR[32+rsp]
	and	edi,edx
	pxor	xmm6,xmm7
	and	esi,ecx
	ror	ebx,7
	movdqa	xmm8,xmm10
	paddd	xmm10,xmm5
	add	ebp,edi
	mov	edi,eax
	pxor	xmm6,xmm9
	rol	eax,5
	add	ebp,esi
	xor	ecx,edx
	add	ebp,eax
	movdqa	xmm9,xmm6
	movdqa	XMMWORD PTR[16+rsp],xmm10
	mov	esi,ebx
	xor	ebx,ecx
	add	edx,DWORD PTR[36+rsp]
	and	esi,ecx
	pslld	xmm6,2
	and	edi,ebx
	ror	eax,7
	psrld	xmm9,30
	add	edx,esi
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	ebx,ecx
	add	edx,ebp
	por	xmm6,xmm9
	mov	edi,eax
	xor	eax,ebx
	movdqa	xmm10,xmm6
	add	ecx,DWORD PTR[40+rsp]
	and	edi,ebx
	and	esi,eax
	ror	ebp,7
	add	ecx,edi
	mov	edi,edx
	rol	edx,5
	add	ecx,esi
	xor	eax,ebx
	add	ecx,edx
	mov	esi,ebp
	xor	ebp,eax
	add	ebx,DWORD PTR[44+rsp]
	and	esi,eax
	and	edi,ebp
	ror	edx,7
	add	ebx,esi
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	ebp,eax
	add	ebx,ecx
	mov	edi,edx
	pxor	xmm7,xmm3
DB	102,68,15,58,15,213,8
	xor	edx,ebp
	add	eax,DWORD PTR[48+rsp]
	and	edi,ebp
	pxor	xmm7,xmm0
	and	esi,edx
	ror	ecx,7
	movdqa	xmm9,XMMWORD PTR[48+r11]
	paddd	xmm8,xmm6
	add	eax,edi
	mov	edi,ebx
	pxor	xmm7,xmm10
	rol	ebx,5
	add	eax,esi
	xor	edx,ebp
	add	eax,ebx
	movdqa	xmm10,xmm7
	movdqa	XMMWORD PTR[32+rsp],xmm8
	mov	esi,ecx
	xor	ecx,edx
	add	ebp,DWORD PTR[52+rsp]
	and	esi,edx
	pslld	xmm7,2
	and	edi,ecx
	ror	ebx,7
	psrld	xmm10,30
	add	ebp,esi
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	ecx,edx
	add	ebp,eax
	por	xmm7,xmm10
	mov	edi,ebx
	xor	ebx,ecx
	movdqa	xmm8,xmm7
	add	edx,DWORD PTR[56+rsp]
	and	edi,ecx
	and	esi,ebx
	ror	eax,7
	add	edx,edi
	mov	edi,ebp
	rol	ebp,5
	add	edx,esi
	xor	ebx,ecx
	add	edx,ebp
	mov	esi,eax
	xor	eax,ebx
	add	ecx,DWORD PTR[60+rsp]
	and	esi,ebx
	and	edi,eax
	ror	ebp,7
	add	ecx,esi
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	xor	eax,ebx
	add	ecx,edx
	mov	edi,ebp
	pxor	xmm0,xmm4
DB	102,68,15,58,15,198,8
	xor	ebp,eax
	add	ebx,DWORD PTR[rsp]
	and	edi,eax
	pxor	xmm0,xmm1
	and	esi,ebp
	ror	edx,7
	movdqa	xmm10,xmm9
	paddd	xmm9,xmm7
	add	ebx,edi
	mov	edi,ecx
	pxor	xmm0,xmm8
	rol	ecx,5
	add	ebx,esi
	xor	ebp,eax
	add	ebx,ecx
	movdqa	xmm8,xmm0
	movdqa	XMMWORD PTR[48+rsp],xmm9
	mov	esi,edx
	xor	edx,ebp
	add	eax,DWORD PTR[4+rsp]
	and	esi,ebp
	pslld	xmm0,2
	and	edi,edx
	ror	ecx,7
	psrld	xmm8,30
	add	eax,esi
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	edx,ebp
	add	eax,ebx
	por	xmm0,xmm8
	mov	edi,ecx
	xor	ecx,edx
	movdqa	xmm9,xmm0
	add	ebp,DWORD PTR[8+rsp]
	and	edi,edx
	and	esi,ecx
	ror	ebx,7
	add	ebp,edi
	mov	edi,eax
	rol	eax,5
	add	ebp,esi
	xor	ecx,edx
	add	ebp,eax
	mov	esi,ebx
	xor	ebx,ecx
	add	edx,DWORD PTR[12+rsp]
	and	esi,ecx
	and	edi,ebx
	ror	eax,7
	add	edx,esi
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	ebx,ecx
	add	edx,ebp
	mov	edi,eax
	pxor	xmm1,xmm5
DB	102,68,15,58,15,207,8
	xor	eax,ebx
	add	ecx,DWORD PTR[16+rsp]
	and	edi,ebx
	pxor	xmm1,xmm2
	and	esi,eax
	ror	ebp,7
	movdqa	xmm8,xmm10
	paddd	xmm10,xmm0
	add	ecx,edi
	mov	edi,edx
	pxor	xmm1,xmm9
	rol	edx,5
	add	ecx,esi
	xor	eax,ebx
	add	ecx,edx
	movdqa	xmm9,xmm1
	movdqa	XMMWORD PTR[rsp],xmm10
	mov	esi,ebp
	xor	ebp,eax
	add	ebx,DWORD PTR[20+rsp]
	and	esi,eax
	pslld	xmm1,2
	and	edi,ebp
	ror	edx,7
	psrld	xmm9,30
	add	ebx,esi
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	ebp,eax
	add	ebx,ecx
	por	xmm1,xmm9
	mov	edi,edx
	xor	edx,ebp
	movdqa	xmm10,xmm1
	add	eax,DWORD PTR[24+rsp]
	and	edi,ebp
	and	esi,edx
	ror	ecx,7
	add	eax,edi
	mov	edi,ebx
	rol	ebx,5
	add	eax,esi
	xor	edx,ebp
	add	eax,ebx
	mov	esi,ecx
	xor	ecx,edx
	add	ebp,DWORD PTR[28+rsp]
	and	esi,edx
	and	edi,ecx
	ror	ebx,7
	add	ebp,esi
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	ecx,edx
	add	ebp,eax
	mov	edi,ebx
	pxor	xmm2,xmm6
DB	102,68,15,58,15,208,8
	xor	ebx,ecx
	add	edx,DWORD PTR[32+rsp]
	and	edi,ecx
	pxor	xmm2,xmm3
	and	esi,ebx
	ror	eax,7
	movdqa	xmm9,xmm8
	paddd	xmm8,xmm1
	add	edx,edi
	mov	edi,ebp
	pxor	xmm2,xmm10
	rol	ebp,5
	add	edx,esi
	xor	ebx,ecx
	add	edx,ebp
	movdqa	xmm10,xmm2
	movdqa	XMMWORD PTR[16+rsp],xmm8
	mov	esi,eax
	xor	eax,ebx
	add	ecx,DWORD PTR[36+rsp]
	and	esi,ebx
	pslld	xmm2,2
	and	edi,eax
	ror	ebp,7
	psrld	xmm10,30
	add	ecx,esi
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	xor	eax,ebx
	add	ecx,edx
	por	xmm2,xmm10
	mov	edi,ebp
	xor	ebp,eax
	movdqa	xmm8,xmm2
	add	ebx,DWORD PTR[40+rsp]
	and	edi,eax
	and	esi,ebp
	ror	edx,7
	add	ebx,edi
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	ebp,eax
	add	ebx,ecx
	mov	esi,edx
	xor	edx,ebp
	add	eax,DWORD PTR[44+rsp]
	and	esi,ebp
	and	edi,edx
	ror	ecx,7
	add	eax,esi
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	edx,ebp
	add	eax,ebx
	add	ebp,DWORD PTR[48+rsp]
	pxor	xmm3,xmm7
DB	102,68,15,58,15,193,8
	xor	esi,edx
	mov	edi,eax
	rol	eax,5
	pxor	xmm3,xmm4
	xor	esi,ecx
	add	ebp,eax
	movdqa	xmm10,xmm9
	paddd	xmm9,xmm2
	ror	ebx,7
	add	ebp,esi
	pxor	xmm3,xmm8
	add	edx,DWORD PTR[52+rsp]
	xor	edi,ecx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm8,xmm3
	movdqa	XMMWORD PTR[32+rsp],xmm9
	xor	edi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,edi
	pslld	xmm3,2
	add	ecx,DWORD PTR[56+rsp]
	xor	esi,ebx
	psrld	xmm8,30
	mov	edi,edx
	rol	edx,5
	xor	esi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,esi
	por	xmm3,xmm8
	add	ebx,DWORD PTR[60+rsp]
	xor	edi,eax
	mov	esi,ecx
	rol	ecx,5
	xor	edi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,edi
	add	eax,DWORD PTR[rsp]
	paddd	xmm10,xmm3
	xor	esi,ebp
	mov	edi,ebx
	rol	ebx,5
	xor	esi,edx
	movdqa	XMMWORD PTR[48+rsp],xmm10
	add	eax,ebx
	ror	ecx,7
	add	eax,esi
	add	ebp,DWORD PTR[4+rsp]
	xor	edi,edx
	mov	esi,eax
	rol	eax,5
	xor	edi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,edi
	add	edx,DWORD PTR[8+rsp]
	xor	esi,ecx
	mov	edi,ebp
	rol	ebp,5
	xor	esi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,esi
	add	ecx,DWORD PTR[12+rsp]
	xor	edi,ebx
	mov	esi,edx
	rol	edx,5
	xor	edi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,edi
	cmp	r9,r10
	je	$L$done_ssse3
	movdqa	xmm6,XMMWORD PTR[64+r11]
	movdqa	xmm9,XMMWORD PTR[r11]
	movdqu	xmm0,XMMWORD PTR[r9]
	movdqu	xmm1,XMMWORD PTR[16+r9]
	movdqu	xmm2,XMMWORD PTR[32+r9]
	movdqu	xmm3,XMMWORD PTR[48+r9]
DB	102,15,56,0,198
	add	r9,64
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,eax
DB	102,15,56,0,206
	mov	edi,ecx
	rol	ecx,5
	paddd	xmm0,xmm9
	xor	esi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	movdqa	XMMWORD PTR[rsp],xmm0
	add	eax,DWORD PTR[20+rsp]
	xor	edi,ebp
	psubd	xmm0,xmm9
	mov	esi,ebx
	rol	ebx,5
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,edx
	mov	edi,eax
	rol	eax,5
	xor	esi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,esi
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ecx
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,edi
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,ebx
DB	102,15,56,0,214
	mov	edi,edx
	rol	edx,5
	paddd	xmm1,xmm9
	xor	esi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,esi
	movdqa	XMMWORD PTR[16+rsp],xmm1
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,eax
	psubd	xmm1,xmm9
	mov	esi,ecx
	rol	ecx,5
	xor	edi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,edi
	add	eax,DWORD PTR[40+rsp]
	xor	esi,ebp
	mov	edi,ebx
	rol	ebx,5
	xor	esi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,esi
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,edx
	mov	esi,eax
	rol	eax,5
	xor	edi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,edi
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ecx
DB	102,15,56,0,222
	mov	edi,ebp
	rol	ebp,5
	paddd	xmm2,xmm9
	xor	esi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,esi
	movdqa	XMMWORD PTR[32+rsp],xmm2
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,ebx
	psubd	xmm2,xmm9
	mov	esi,edx
	rol	edx,5
	xor	edi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,edi
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,eax
	mov	edi,ecx
	rol	ecx,5
	xor	esi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	add	eax,DWORD PTR[60+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	add	eax,DWORD PTR[r8]
	add	esi,DWORD PTR[4+r8]
	add	ecx,DWORD PTR[8+r8]
	add	edx,DWORD PTR[12+r8]
	mov	DWORD PTR[r8],eax
	add	ebp,DWORD PTR[16+r8]
	mov	DWORD PTR[4+r8],esi
	mov	ebx,esi
	mov	DWORD PTR[8+r8],ecx
	mov	DWORD PTR[12+r8],edx
	mov	DWORD PTR[16+r8],ebp
	jmp	$L$oop_ssse3

ALIGN	16
$L$done_ssse3::
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,eax
	mov	edi,ecx
	rol	ecx,5
	xor	esi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	add	eax,DWORD PTR[20+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
	add	ebp,DWORD PTR[24+rsp]
	xor	esi,edx
	mov	edi,eax
	rol	eax,5
	xor	esi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,esi
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ecx
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,edi
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,ebx
	mov	edi,edx
	rol	edx,5
	xor	esi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,esi
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,eax
	mov	esi,ecx
	rol	ecx,5
	xor	edi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,edi
	add	eax,DWORD PTR[40+rsp]
	xor	esi,ebp
	mov	edi,ebx
	rol	ebx,5
	xor	esi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,esi
	add	ebp,DWORD PTR[44+rsp]
	xor	edi,edx
	mov	esi,eax
	rol	eax,5
	xor	edi,ecx
	add	ebp,eax
	ror	ebx,7
	add	ebp,edi
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ecx
	mov	edi,ebp
	rol	ebp,5
	xor	esi,ebx
	add	edx,ebp
	ror	eax,7
	add	edx,esi
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,ebx
	mov	esi,edx
	rol	edx,5
	xor	edi,eax
	add	ecx,edx
	ror	ebp,7
	add	ecx,edi
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,eax
	mov	edi,ecx
	rol	ecx,5
	xor	esi,ebp
	add	ebx,ecx
	ror	edx,7
	add	ebx,esi
	add	eax,DWORD PTR[60+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	xor	edi,edx
	add	eax,ebx
	ror	ecx,7
	add	eax,edi
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
	movaps	xmm6,XMMWORD PTR[((64+0))+rsp]
	movaps	xmm7,XMMWORD PTR[((64+16))+rsp]
	movaps	xmm8,XMMWORD PTR[((64+32))+rsp]
	movaps	xmm9,XMMWORD PTR[((64+48))+rsp]
	movaps	xmm10,XMMWORD PTR[((64+64))+rsp]
	lea	rsi,QWORD PTR[144+rsp]
	mov	r12,QWORD PTR[rsi]
	mov	rbp,QWORD PTR[8+rsi]
	mov	rbx,QWORD PTR[16+rsi]
	lea	rsp,QWORD PTR[24+rsi]
$L$epilogue_ssse3::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order_ssse3::
sha1_block_data_order_ssse3	ENDP
ALIGN	64
K_XX_XX::
	DD	05a827999h,05a827999h,05a827999h,05a827999h

	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h

	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch

	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h

	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh

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
	lea	rax,QWORD PTR[32+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13

	jmp	$L$common_seh_tail
se_handler	ENDP


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

	lea	rsi,QWORD PTR[64+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,10
	DD	0a548f3fch

	lea	rax,QWORD PTR[168+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12

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
	DD	imagerel $L$SEH_begin_sha1_block_data_order_ssse3
	DD	imagerel $L$SEH_end_sha1_block_data_order_ssse3
	DD	imagerel $L$SEH_info_sha1_block_data_order_ssse3
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha1_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler
$L$SEH_info_sha1_block_data_order_ssse3::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_ssse3,imagerel $L$epilogue_ssse3


.xdata	ENDS
END
