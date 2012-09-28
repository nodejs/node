OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
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


	push	rbx
	push	rbp
	push	r12
	mov	r11,rsp
	mov	r8,rdi
	sub	rsp,72
	mov	r9,rsi
	and	rsp,-64
	mov	r10,rdx
	mov	QWORD PTR[64+rsp],r11
$L$prologue::

	mov	edx,DWORD PTR[r8]
	mov	esi,DWORD PTR[4+r8]
	mov	edi,DWORD PTR[8+r8]
	mov	ebp,DWORD PTR[12+r8]
	mov	r11d,DWORD PTR[16+r8]
ALIGN	4
$L$loop::
	mov	eax,DWORD PTR[r9]
	bswap	eax
	mov	DWORD PTR[rsp],eax
	lea	r12d,DWORD PTR[05a827999h+r11*1+rax]
	mov	ebx,edi
	mov	eax,DWORD PTR[4+r9]
	mov	r11d,edx
	xor	ebx,ebp
	bswap	eax
	rol	r11d,5
	and	ebx,esi
	mov	DWORD PTR[4+rsp],eax
	add	r12d,r11d
	xor	ebx,ebp
	rol	esi,30
	add	r12d,ebx
	lea	r11d,DWORD PTR[05a827999h+rbp*1+rax]
	mov	ebx,esi
	mov	eax,DWORD PTR[8+r9]
	mov	ebp,r12d
	xor	ebx,edi
	bswap	eax
	rol	ebp,5
	and	ebx,edx
	mov	DWORD PTR[8+rsp],eax
	add	r11d,ebp
	xor	ebx,edi
	rol	edx,30
	add	r11d,ebx
	lea	ebp,DWORD PTR[05a827999h+rdi*1+rax]
	mov	ebx,edx
	mov	eax,DWORD PTR[12+r9]
	mov	edi,r11d
	xor	ebx,esi
	bswap	eax
	rol	edi,5
	and	ebx,r12d
	mov	DWORD PTR[12+rsp],eax
	add	ebp,edi
	xor	ebx,esi
	rol	r12d,30
	add	ebp,ebx
	lea	edi,DWORD PTR[05a827999h+rsi*1+rax]
	mov	ebx,r12d
	mov	eax,DWORD PTR[16+r9]
	mov	esi,ebp
	xor	ebx,edx
	bswap	eax
	rol	esi,5
	and	ebx,r11d
	mov	DWORD PTR[16+rsp],eax
	add	edi,esi
	xor	ebx,edx
	rol	r11d,30
	add	edi,ebx
	lea	esi,DWORD PTR[05a827999h+rdx*1+rax]
	mov	ebx,r11d
	mov	eax,DWORD PTR[20+r9]
	mov	edx,edi
	xor	ebx,r12d
	bswap	eax
	rol	edx,5
	and	ebx,ebp
	mov	DWORD PTR[20+rsp],eax
	add	esi,edx
	xor	ebx,r12d
	rol	ebp,30
	add	esi,ebx
	lea	edx,DWORD PTR[05a827999h+r12*1+rax]
	mov	ebx,ebp
	mov	eax,DWORD PTR[24+r9]
	mov	r12d,esi
	xor	ebx,r11d
	bswap	eax
	rol	r12d,5
	and	ebx,edi
	mov	DWORD PTR[24+rsp],eax
	add	edx,r12d
	xor	ebx,r11d
	rol	edi,30
	add	edx,ebx
	lea	r12d,DWORD PTR[05a827999h+r11*1+rax]
	mov	ebx,edi
	mov	eax,DWORD PTR[28+r9]
	mov	r11d,edx
	xor	ebx,ebp
	bswap	eax
	rol	r11d,5
	and	ebx,esi
	mov	DWORD PTR[28+rsp],eax
	add	r12d,r11d
	xor	ebx,ebp
	rol	esi,30
	add	r12d,ebx
	lea	r11d,DWORD PTR[05a827999h+rbp*1+rax]
	mov	ebx,esi
	mov	eax,DWORD PTR[32+r9]
	mov	ebp,r12d
	xor	ebx,edi
	bswap	eax
	rol	ebp,5
	and	ebx,edx
	mov	DWORD PTR[32+rsp],eax
	add	r11d,ebp
	xor	ebx,edi
	rol	edx,30
	add	r11d,ebx
	lea	ebp,DWORD PTR[05a827999h+rdi*1+rax]
	mov	ebx,edx
	mov	eax,DWORD PTR[36+r9]
	mov	edi,r11d
	xor	ebx,esi
	bswap	eax
	rol	edi,5
	and	ebx,r12d
	mov	DWORD PTR[36+rsp],eax
	add	ebp,edi
	xor	ebx,esi
	rol	r12d,30
	add	ebp,ebx
	lea	edi,DWORD PTR[05a827999h+rsi*1+rax]
	mov	ebx,r12d
	mov	eax,DWORD PTR[40+r9]
	mov	esi,ebp
	xor	ebx,edx
	bswap	eax
	rol	esi,5
	and	ebx,r11d
	mov	DWORD PTR[40+rsp],eax
	add	edi,esi
	xor	ebx,edx
	rol	r11d,30
	add	edi,ebx
	lea	esi,DWORD PTR[05a827999h+rdx*1+rax]
	mov	ebx,r11d
	mov	eax,DWORD PTR[44+r9]
	mov	edx,edi
	xor	ebx,r12d
	bswap	eax
	rol	edx,5
	and	ebx,ebp
	mov	DWORD PTR[44+rsp],eax
	add	esi,edx
	xor	ebx,r12d
	rol	ebp,30
	add	esi,ebx
	lea	edx,DWORD PTR[05a827999h+r12*1+rax]
	mov	ebx,ebp
	mov	eax,DWORD PTR[48+r9]
	mov	r12d,esi
	xor	ebx,r11d
	bswap	eax
	rol	r12d,5
	and	ebx,edi
	mov	DWORD PTR[48+rsp],eax
	add	edx,r12d
	xor	ebx,r11d
	rol	edi,30
	add	edx,ebx
	lea	r12d,DWORD PTR[05a827999h+r11*1+rax]
	mov	ebx,edi
	mov	eax,DWORD PTR[52+r9]
	mov	r11d,edx
	xor	ebx,ebp
	bswap	eax
	rol	r11d,5
	and	ebx,esi
	mov	DWORD PTR[52+rsp],eax
	add	r12d,r11d
	xor	ebx,ebp
	rol	esi,30
	add	r12d,ebx
	lea	r11d,DWORD PTR[05a827999h+rbp*1+rax]
	mov	ebx,esi
	mov	eax,DWORD PTR[56+r9]
	mov	ebp,r12d
	xor	ebx,edi
	bswap	eax
	rol	ebp,5
	and	ebx,edx
	mov	DWORD PTR[56+rsp],eax
	add	r11d,ebp
	xor	ebx,edi
	rol	edx,30
	add	r11d,ebx
	lea	ebp,DWORD PTR[05a827999h+rdi*1+rax]
	mov	ebx,edx
	mov	eax,DWORD PTR[60+r9]
	mov	edi,r11d
	xor	ebx,esi
	bswap	eax
	rol	edi,5
	and	ebx,r12d
	mov	DWORD PTR[60+rsp],eax
	add	ebp,edi
	xor	ebx,esi
	rol	r12d,30
	add	ebp,ebx
	lea	edi,DWORD PTR[05a827999h+rsi*1+rax]
	mov	eax,DWORD PTR[rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[8+rsp]
	xor	ebx,edx
	rol	esi,5
	xor	eax,DWORD PTR[32+rsp]
	and	ebx,r11d
	add	edi,esi
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,edx
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[rsp],eax
	lea	esi,DWORD PTR[05a827999h+rdx*1+rax]
	mov	eax,DWORD PTR[4+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[12+rsp]
	xor	ebx,r12d
	rol	edx,5
	xor	eax,DWORD PTR[36+rsp]
	and	ebx,ebp
	add	esi,edx
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,r12d
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[4+rsp],eax
	lea	edx,DWORD PTR[05a827999h+r12*1+rax]
	mov	eax,DWORD PTR[8+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[16+rsp]
	xor	ebx,r11d
	rol	r12d,5
	xor	eax,DWORD PTR[40+rsp]
	and	ebx,edi
	add	edx,r12d
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,r11d
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[8+rsp],eax
	lea	r12d,DWORD PTR[05a827999h+r11*1+rax]
	mov	eax,DWORD PTR[12+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,ebp
	rol	r11d,5
	xor	eax,DWORD PTR[44+rsp]
	and	ebx,esi
	add	r12d,r11d
	xor	eax,DWORD PTR[rsp]
	xor	ebx,ebp
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[12+rsp],eax
	lea	r11d,DWORD PTR[05a827999h+rbp*1+rax]
	mov	eax,DWORD PTR[16+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,edi
	rol	ebp,5
	xor	eax,DWORD PTR[48+rsp]
	and	ebx,edx
	add	r11d,ebp
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,edi
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[16+rsp],eax
	lea	ebp,DWORD PTR[1859775393+rdi*1+rax]
	mov	eax,DWORD PTR[20+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[8+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[20+rsp],eax
	lea	edi,DWORD PTR[1859775393+rsi*1+rax]
	mov	eax,DWORD PTR[24+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[32+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[12+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[24+rsp],eax
	lea	esi,DWORD PTR[1859775393+rdx*1+rax]
	mov	eax,DWORD PTR[28+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[36+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[16+rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[28+rsp],eax
	lea	edx,DWORD PTR[1859775393+r12*1+rax]
	mov	eax,DWORD PTR[32+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[40+rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[20+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[32+rsp],eax
	lea	r12d,DWORD PTR[1859775393+r11*1+rax]
	mov	eax,DWORD PTR[36+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[44+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[24+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[36+rsp],eax
	lea	r11d,DWORD PTR[1859775393+rbp*1+rax]
	mov	eax,DWORD PTR[40+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[48+rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[8+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[28+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[40+rsp],eax
	lea	ebp,DWORD PTR[1859775393+rdi*1+rax]
	mov	eax,DWORD PTR[44+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[12+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[32+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[44+rsp],eax
	lea	edi,DWORD PTR[1859775393+rsi*1+rax]
	mov	eax,DWORD PTR[48+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[16+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[36+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[48+rsp],eax
	lea	esi,DWORD PTR[1859775393+rdx*1+rax]
	mov	eax,DWORD PTR[52+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[40+rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[52+rsp],eax
	lea	edx,DWORD PTR[1859775393+r12*1+rax]
	mov	eax,DWORD PTR[56+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[44+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[56+rsp],eax
	lea	r12d,DWORD PTR[1859775393+r11*1+rax]
	mov	eax,DWORD PTR[60+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[48+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[60+rsp],eax
	lea	r11d,DWORD PTR[1859775393+rbp*1+rax]
	mov	eax,DWORD PTR[rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[8+rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[32+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[52+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[rsp],eax
	lea	ebp,DWORD PTR[1859775393+rdi*1+rax]
	mov	eax,DWORD PTR[4+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[12+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[36+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[56+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[4+rsp],eax
	lea	edi,DWORD PTR[1859775393+rsi*1+rax]
	mov	eax,DWORD PTR[8+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[16+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[40+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[60+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[8+rsp],eax
	lea	esi,DWORD PTR[1859775393+rdx*1+rax]
	mov	eax,DWORD PTR[12+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[44+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[12+rsp],eax
	lea	edx,DWORD PTR[1859775393+r12*1+rax]
	mov	eax,DWORD PTR[16+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[48+rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[4+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[16+rsp],eax
	lea	r12d,DWORD PTR[1859775393+r11*1+rax]
	mov	eax,DWORD PTR[20+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[8+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[20+rsp],eax
	lea	r11d,DWORD PTR[1859775393+rbp*1+rax]
	mov	eax,DWORD PTR[24+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[32+rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[12+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[24+rsp],eax
	lea	ebp,DWORD PTR[1859775393+rdi*1+rax]
	mov	eax,DWORD PTR[28+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[36+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[16+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[28+rsp],eax
	lea	edi,DWORD PTR[1859775393+rsi*1+rax]
	mov	eax,DWORD PTR[32+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[40+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[20+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[32+rsp],eax
	lea	esi,DWORD PTR[08f1bbcdch+rdx*1+rax]
	mov	eax,DWORD PTR[36+rsp]
	mov	ebx,ebp
	mov	ecx,ebp
	xor	eax,DWORD PTR[44+rsp]
	mov	edx,edi
	and	ebx,r11d
	xor	eax,DWORD PTR[4+rsp]
	or	ecx,r11d
	rol	edx,5
	xor	eax,DWORD PTR[24+rsp]
	and	ecx,r12d
	add	esi,edx
	rol	eax,1
	or	ebx,ecx
	rol	ebp,30
	mov	DWORD PTR[36+rsp],eax
	add	esi,ebx
	lea	edx,DWORD PTR[08f1bbcdch+r12*1+rax]
	mov	eax,DWORD PTR[40+rsp]
	mov	ebx,edi
	mov	ecx,edi
	xor	eax,DWORD PTR[48+rsp]
	mov	r12d,esi
	and	ebx,ebp
	xor	eax,DWORD PTR[8+rsp]
	or	ecx,ebp
	rol	r12d,5
	xor	eax,DWORD PTR[28+rsp]
	and	ecx,r11d
	add	edx,r12d
	rol	eax,1
	or	ebx,ecx
	rol	edi,30
	mov	DWORD PTR[40+rsp],eax
	add	edx,ebx
	lea	r12d,DWORD PTR[08f1bbcdch+r11*1+rax]
	mov	eax,DWORD PTR[44+rsp]
	mov	ebx,esi
	mov	ecx,esi
	xor	eax,DWORD PTR[52+rsp]
	mov	r11d,edx
	and	ebx,edi
	xor	eax,DWORD PTR[12+rsp]
	or	ecx,edi
	rol	r11d,5
	xor	eax,DWORD PTR[32+rsp]
	and	ecx,ebp
	add	r12d,r11d
	rol	eax,1
	or	ebx,ecx
	rol	esi,30
	mov	DWORD PTR[44+rsp],eax
	add	r12d,ebx
	lea	r11d,DWORD PTR[08f1bbcdch+rbp*1+rax]
	mov	eax,DWORD PTR[48+rsp]
	mov	ebx,edx
	mov	ecx,edx
	xor	eax,DWORD PTR[56+rsp]
	mov	ebp,r12d
	and	ebx,esi
	xor	eax,DWORD PTR[16+rsp]
	or	ecx,esi
	rol	ebp,5
	xor	eax,DWORD PTR[36+rsp]
	and	ecx,edi
	add	r11d,ebp
	rol	eax,1
	or	ebx,ecx
	rol	edx,30
	mov	DWORD PTR[48+rsp],eax
	add	r11d,ebx
	lea	ebp,DWORD PTR[08f1bbcdch+rdi*1+rax]
	mov	eax,DWORD PTR[52+rsp]
	mov	ebx,r12d
	mov	ecx,r12d
	xor	eax,DWORD PTR[60+rsp]
	mov	edi,r11d
	and	ebx,edx
	xor	eax,DWORD PTR[20+rsp]
	or	ecx,edx
	rol	edi,5
	xor	eax,DWORD PTR[40+rsp]
	and	ecx,esi
	add	ebp,edi
	rol	eax,1
	or	ebx,ecx
	rol	r12d,30
	mov	DWORD PTR[52+rsp],eax
	add	ebp,ebx
	lea	edi,DWORD PTR[08f1bbcdch+rsi*1+rax]
	mov	eax,DWORD PTR[56+rsp]
	mov	ebx,r11d
	mov	ecx,r11d
	xor	eax,DWORD PTR[rsp]
	mov	esi,ebp
	and	ebx,r12d
	xor	eax,DWORD PTR[24+rsp]
	or	ecx,r12d
	rol	esi,5
	xor	eax,DWORD PTR[44+rsp]
	and	ecx,edx
	add	edi,esi
	rol	eax,1
	or	ebx,ecx
	rol	r11d,30
	mov	DWORD PTR[56+rsp],eax
	add	edi,ebx
	lea	esi,DWORD PTR[08f1bbcdch+rdx*1+rax]
	mov	eax,DWORD PTR[60+rsp]
	mov	ebx,ebp
	mov	ecx,ebp
	xor	eax,DWORD PTR[4+rsp]
	mov	edx,edi
	and	ebx,r11d
	xor	eax,DWORD PTR[28+rsp]
	or	ecx,r11d
	rol	edx,5
	xor	eax,DWORD PTR[48+rsp]
	and	ecx,r12d
	add	esi,edx
	rol	eax,1
	or	ebx,ecx
	rol	ebp,30
	mov	DWORD PTR[60+rsp],eax
	add	esi,ebx
	lea	edx,DWORD PTR[08f1bbcdch+r12*1+rax]
	mov	eax,DWORD PTR[rsp]
	mov	ebx,edi
	mov	ecx,edi
	xor	eax,DWORD PTR[8+rsp]
	mov	r12d,esi
	and	ebx,ebp
	xor	eax,DWORD PTR[32+rsp]
	or	ecx,ebp
	rol	r12d,5
	xor	eax,DWORD PTR[52+rsp]
	and	ecx,r11d
	add	edx,r12d
	rol	eax,1
	or	ebx,ecx
	rol	edi,30
	mov	DWORD PTR[rsp],eax
	add	edx,ebx
	lea	r12d,DWORD PTR[08f1bbcdch+r11*1+rax]
	mov	eax,DWORD PTR[4+rsp]
	mov	ebx,esi
	mov	ecx,esi
	xor	eax,DWORD PTR[12+rsp]
	mov	r11d,edx
	and	ebx,edi
	xor	eax,DWORD PTR[36+rsp]
	or	ecx,edi
	rol	r11d,5
	xor	eax,DWORD PTR[56+rsp]
	and	ecx,ebp
	add	r12d,r11d
	rol	eax,1
	or	ebx,ecx
	rol	esi,30
	mov	DWORD PTR[4+rsp],eax
	add	r12d,ebx
	lea	r11d,DWORD PTR[08f1bbcdch+rbp*1+rax]
	mov	eax,DWORD PTR[8+rsp]
	mov	ebx,edx
	mov	ecx,edx
	xor	eax,DWORD PTR[16+rsp]
	mov	ebp,r12d
	and	ebx,esi
	xor	eax,DWORD PTR[40+rsp]
	or	ecx,esi
	rol	ebp,5
	xor	eax,DWORD PTR[60+rsp]
	and	ecx,edi
	add	r11d,ebp
	rol	eax,1
	or	ebx,ecx
	rol	edx,30
	mov	DWORD PTR[8+rsp],eax
	add	r11d,ebx
	lea	ebp,DWORD PTR[08f1bbcdch+rdi*1+rax]
	mov	eax,DWORD PTR[12+rsp]
	mov	ebx,r12d
	mov	ecx,r12d
	xor	eax,DWORD PTR[20+rsp]
	mov	edi,r11d
	and	ebx,edx
	xor	eax,DWORD PTR[44+rsp]
	or	ecx,edx
	rol	edi,5
	xor	eax,DWORD PTR[rsp]
	and	ecx,esi
	add	ebp,edi
	rol	eax,1
	or	ebx,ecx
	rol	r12d,30
	mov	DWORD PTR[12+rsp],eax
	add	ebp,ebx
	lea	edi,DWORD PTR[08f1bbcdch+rsi*1+rax]
	mov	eax,DWORD PTR[16+rsp]
	mov	ebx,r11d
	mov	ecx,r11d
	xor	eax,DWORD PTR[24+rsp]
	mov	esi,ebp
	and	ebx,r12d
	xor	eax,DWORD PTR[48+rsp]
	or	ecx,r12d
	rol	esi,5
	xor	eax,DWORD PTR[4+rsp]
	and	ecx,edx
	add	edi,esi
	rol	eax,1
	or	ebx,ecx
	rol	r11d,30
	mov	DWORD PTR[16+rsp],eax
	add	edi,ebx
	lea	esi,DWORD PTR[08f1bbcdch+rdx*1+rax]
	mov	eax,DWORD PTR[20+rsp]
	mov	ebx,ebp
	mov	ecx,ebp
	xor	eax,DWORD PTR[28+rsp]
	mov	edx,edi
	and	ebx,r11d
	xor	eax,DWORD PTR[52+rsp]
	or	ecx,r11d
	rol	edx,5
	xor	eax,DWORD PTR[8+rsp]
	and	ecx,r12d
	add	esi,edx
	rol	eax,1
	or	ebx,ecx
	rol	ebp,30
	mov	DWORD PTR[20+rsp],eax
	add	esi,ebx
	lea	edx,DWORD PTR[08f1bbcdch+r12*1+rax]
	mov	eax,DWORD PTR[24+rsp]
	mov	ebx,edi
	mov	ecx,edi
	xor	eax,DWORD PTR[32+rsp]
	mov	r12d,esi
	and	ebx,ebp
	xor	eax,DWORD PTR[56+rsp]
	or	ecx,ebp
	rol	r12d,5
	xor	eax,DWORD PTR[12+rsp]
	and	ecx,r11d
	add	edx,r12d
	rol	eax,1
	or	ebx,ecx
	rol	edi,30
	mov	DWORD PTR[24+rsp],eax
	add	edx,ebx
	lea	r12d,DWORD PTR[08f1bbcdch+r11*1+rax]
	mov	eax,DWORD PTR[28+rsp]
	mov	ebx,esi
	mov	ecx,esi
	xor	eax,DWORD PTR[36+rsp]
	mov	r11d,edx
	and	ebx,edi
	xor	eax,DWORD PTR[60+rsp]
	or	ecx,edi
	rol	r11d,5
	xor	eax,DWORD PTR[16+rsp]
	and	ecx,ebp
	add	r12d,r11d
	rol	eax,1
	or	ebx,ecx
	rol	esi,30
	mov	DWORD PTR[28+rsp],eax
	add	r12d,ebx
	lea	r11d,DWORD PTR[08f1bbcdch+rbp*1+rax]
	mov	eax,DWORD PTR[32+rsp]
	mov	ebx,edx
	mov	ecx,edx
	xor	eax,DWORD PTR[40+rsp]
	mov	ebp,r12d
	and	ebx,esi
	xor	eax,DWORD PTR[rsp]
	or	ecx,esi
	rol	ebp,5
	xor	eax,DWORD PTR[20+rsp]
	and	ecx,edi
	add	r11d,ebp
	rol	eax,1
	or	ebx,ecx
	rol	edx,30
	mov	DWORD PTR[32+rsp],eax
	add	r11d,ebx
	lea	ebp,DWORD PTR[08f1bbcdch+rdi*1+rax]
	mov	eax,DWORD PTR[36+rsp]
	mov	ebx,r12d
	mov	ecx,r12d
	xor	eax,DWORD PTR[44+rsp]
	mov	edi,r11d
	and	ebx,edx
	xor	eax,DWORD PTR[4+rsp]
	or	ecx,edx
	rol	edi,5
	xor	eax,DWORD PTR[24+rsp]
	and	ecx,esi
	add	ebp,edi
	rol	eax,1
	or	ebx,ecx
	rol	r12d,30
	mov	DWORD PTR[36+rsp],eax
	add	ebp,ebx
	lea	edi,DWORD PTR[08f1bbcdch+rsi*1+rax]
	mov	eax,DWORD PTR[40+rsp]
	mov	ebx,r11d
	mov	ecx,r11d
	xor	eax,DWORD PTR[48+rsp]
	mov	esi,ebp
	and	ebx,r12d
	xor	eax,DWORD PTR[8+rsp]
	or	ecx,r12d
	rol	esi,5
	xor	eax,DWORD PTR[28+rsp]
	and	ecx,edx
	add	edi,esi
	rol	eax,1
	or	ebx,ecx
	rol	r11d,30
	mov	DWORD PTR[40+rsp],eax
	add	edi,ebx
	lea	esi,DWORD PTR[08f1bbcdch+rdx*1+rax]
	mov	eax,DWORD PTR[44+rsp]
	mov	ebx,ebp
	mov	ecx,ebp
	xor	eax,DWORD PTR[52+rsp]
	mov	edx,edi
	and	ebx,r11d
	xor	eax,DWORD PTR[12+rsp]
	or	ecx,r11d
	rol	edx,5
	xor	eax,DWORD PTR[32+rsp]
	and	ecx,r12d
	add	esi,edx
	rol	eax,1
	or	ebx,ecx
	rol	ebp,30
	mov	DWORD PTR[44+rsp],eax
	add	esi,ebx
	lea	edx,DWORD PTR[08f1bbcdch+r12*1+rax]
	mov	eax,DWORD PTR[48+rsp]
	mov	ebx,edi
	mov	ecx,edi
	xor	eax,DWORD PTR[56+rsp]
	mov	r12d,esi
	and	ebx,ebp
	xor	eax,DWORD PTR[16+rsp]
	or	ecx,ebp
	rol	r12d,5
	xor	eax,DWORD PTR[36+rsp]
	and	ecx,r11d
	add	edx,r12d
	rol	eax,1
	or	ebx,ecx
	rol	edi,30
	mov	DWORD PTR[48+rsp],eax
	add	edx,ebx
	lea	r12d,DWORD PTR[3395469782+r11*1+rax]
	mov	eax,DWORD PTR[52+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[40+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[52+rsp],eax
	lea	r11d,DWORD PTR[3395469782+rbp*1+rax]
	mov	eax,DWORD PTR[56+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[44+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[56+rsp],eax
	lea	ebp,DWORD PTR[3395469782+rdi*1+rax]
	mov	eax,DWORD PTR[60+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[48+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[60+rsp],eax
	lea	edi,DWORD PTR[3395469782+rsi*1+rax]
	mov	eax,DWORD PTR[rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[8+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[32+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[52+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[rsp],eax
	lea	esi,DWORD PTR[3395469782+rdx*1+rax]
	mov	eax,DWORD PTR[4+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[12+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[36+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[56+rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[4+rsp],eax
	lea	edx,DWORD PTR[3395469782+r12*1+rax]
	mov	eax,DWORD PTR[8+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[16+rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[40+rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[60+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[8+rsp],eax
	lea	r12d,DWORD PTR[3395469782+r11*1+rax]
	mov	eax,DWORD PTR[12+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[44+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[12+rsp],eax
	lea	r11d,DWORD PTR[3395469782+rbp*1+rax]
	mov	eax,DWORD PTR[16+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[48+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[4+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[16+rsp],eax
	lea	ebp,DWORD PTR[3395469782+rdi*1+rax]
	mov	eax,DWORD PTR[20+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[8+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[20+rsp],eax
	lea	edi,DWORD PTR[3395469782+rsi*1+rax]
	mov	eax,DWORD PTR[24+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[32+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[12+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[24+rsp],eax
	lea	esi,DWORD PTR[3395469782+rdx*1+rax]
	mov	eax,DWORD PTR[28+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[36+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[16+rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	mov	DWORD PTR[28+rsp],eax
	lea	edx,DWORD PTR[3395469782+r12*1+rax]
	mov	eax,DWORD PTR[32+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[40+rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[20+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	mov	DWORD PTR[32+rsp],eax
	lea	r12d,DWORD PTR[3395469782+r11*1+rax]
	mov	eax,DWORD PTR[36+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[44+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[24+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	mov	DWORD PTR[36+rsp],eax
	lea	r11d,DWORD PTR[3395469782+rbp*1+rax]
	mov	eax,DWORD PTR[40+rsp]
	mov	ebx,esi
	mov	ebp,r12d
	xor	eax,DWORD PTR[48+rsp]
	xor	ebx,edx
	rol	ebp,5
	xor	eax,DWORD PTR[8+rsp]
	xor	ebx,edi
	add	r11d,ebp
	xor	eax,DWORD PTR[28+rsp]
	rol	edx,30
	add	r11d,ebx
	rol	eax,1
	mov	DWORD PTR[40+rsp],eax
	lea	ebp,DWORD PTR[3395469782+rdi*1+rax]
	mov	eax,DWORD PTR[44+rsp]
	mov	ebx,edx
	mov	edi,r11d
	xor	eax,DWORD PTR[52+rsp]
	xor	ebx,r12d
	rol	edi,5
	xor	eax,DWORD PTR[12+rsp]
	xor	ebx,esi
	add	ebp,edi
	xor	eax,DWORD PTR[32+rsp]
	rol	r12d,30
	add	ebp,ebx
	rol	eax,1
	mov	DWORD PTR[44+rsp],eax
	lea	edi,DWORD PTR[3395469782+rsi*1+rax]
	mov	eax,DWORD PTR[48+rsp]
	mov	ebx,r12d
	mov	esi,ebp
	xor	eax,DWORD PTR[56+rsp]
	xor	ebx,r11d
	rol	esi,5
	xor	eax,DWORD PTR[16+rsp]
	xor	ebx,edx
	add	edi,esi
	xor	eax,DWORD PTR[36+rsp]
	rol	r11d,30
	add	edi,ebx
	rol	eax,1
	mov	DWORD PTR[48+rsp],eax
	lea	esi,DWORD PTR[3395469782+rdx*1+rax]
	mov	eax,DWORD PTR[52+rsp]
	mov	ebx,r11d
	mov	edx,edi
	xor	eax,DWORD PTR[60+rsp]
	xor	ebx,ebp
	rol	edx,5
	xor	eax,DWORD PTR[20+rsp]
	xor	ebx,r12d
	add	esi,edx
	xor	eax,DWORD PTR[40+rsp]
	rol	ebp,30
	add	esi,ebx
	rol	eax,1
	lea	edx,DWORD PTR[3395469782+r12*1+rax]
	mov	eax,DWORD PTR[56+rsp]
	mov	ebx,ebp
	mov	r12d,esi
	xor	eax,DWORD PTR[rsp]
	xor	ebx,edi
	rol	r12d,5
	xor	eax,DWORD PTR[24+rsp]
	xor	ebx,r11d
	add	edx,r12d
	xor	eax,DWORD PTR[44+rsp]
	rol	edi,30
	add	edx,ebx
	rol	eax,1
	lea	r12d,DWORD PTR[3395469782+r11*1+rax]
	mov	eax,DWORD PTR[60+rsp]
	mov	ebx,edi
	mov	r11d,edx
	xor	eax,DWORD PTR[4+rsp]
	xor	ebx,esi
	rol	r11d,5
	xor	eax,DWORD PTR[28+rsp]
	xor	ebx,ebp
	add	r12d,r11d
	xor	eax,DWORD PTR[48+rsp]
	rol	esi,30
	add	r12d,ebx
	rol	eax,1
	lea	r11d,DWORD PTR[3395469782+rbp*1+rax]
	mov	ebx,esi
	mov	ebp,r12d
	xor	ebx,edx
	rol	ebp,5
	xor	ebx,edi
	add	r11d,ebp
	rol	edx,30
	add	r11d,ebx
	add	r11d,DWORD PTR[r8]
	add	r12d,DWORD PTR[4+r8]
	add	edx,DWORD PTR[8+r8]
	add	esi,DWORD PTR[12+r8]
	add	edi,DWORD PTR[16+r8]
	mov	DWORD PTR[r8],r11d
	mov	DWORD PTR[4+r8],r12d
	mov	DWORD PTR[8+r8],edx
	mov	DWORD PTR[12+r8],esi
	mov	DWORD PTR[16+r8],edi

	xchg	edx,r11d
	xchg	esi,r12d
	xchg	edi,r11d
	xchg	ebp,r12d

	lea	r9,QWORD PTR[64+r9]
	sub	r10,1
	jnz	$L$loop
	mov	rsi,QWORD PTR[64+rsp]
	mov	r12,QWORD PTR[rsi]
	mov	rbp,QWORD PTR[8+rsi]
	mov	rbx,QWORD PTR[16+rsi]
	lea	rsp,QWORD PTR[24+rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_block_data_order::
sha1_block_data_order	ENDP
DB	83,72,65,49,32,98,108,111,99,107,32,116,114,97,110,115
DB	102,111,114,109,32,102,111,114,32,120,56,54,95,54,52,44
DB	32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60
DB	97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114
DB	103,62,0
ALIGN	16
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

	mov	rax,QWORD PTR[64+rax]
	lea	rax,QWORD PTR[24+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12

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
	DD	imagerel $L$SEH_begin_sha1_block_data_order
	DD	imagerel $L$SEH_end_sha1_block_data_order
	DD	imagerel $L$SEH_info_sha1_block_data_order

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha1_block_data_order::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
