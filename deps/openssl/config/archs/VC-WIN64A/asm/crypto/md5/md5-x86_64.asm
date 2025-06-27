default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64

ALIGN	16

global	ossl_md5_block_asm_data_order

ossl_md5_block_asm_data_order:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_md5_block_asm_data_order:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



	push	rbp

	push	rbx

	push	r12

	push	r14

	push	r15

$L$prologue:




	mov	rbp,rdi
	shl	rdx,6
	lea	rdi,[rdx*1+rsi]
	mov	eax,DWORD[rbp]
	mov	ebx,DWORD[4+rbp]
	mov	ecx,DWORD[8+rbp]
	mov	edx,DWORD[12+rbp]







	cmp	rsi,rdi
	je	NEAR $L$end


$L$loop:
	mov	r8d,eax
	mov	r9d,ebx
	mov	r14d,ecx
	mov	r15d,edx
	mov	r10d,DWORD[rsi]
	mov	r11d,edx
	xor	r11d,ecx
	lea	eax,[((-680876936))+r10*1+rax]
	and	r11d,ebx
	mov	r10d,DWORD[4+rsi]
	xor	r11d,edx
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,[((-389564586))+r10*1+rdx]
	and	r11d,eax
	mov	r10d,DWORD[8+rsi]
	xor	r11d,ecx
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,[606105819+r10*1+rcx]
	and	r11d,edx
	mov	r10d,DWORD[12+rsi]
	xor	r11d,ebx
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,[((-1044525330))+r10*1+rbx]
	and	r11d,ecx
	mov	r10d,DWORD[16+rsi]
	xor	r11d,eax
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,[((-176418897))+r10*1+rax]
	and	r11d,ebx
	mov	r10d,DWORD[20+rsi]
	xor	r11d,edx
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,[1200080426+r10*1+rdx]
	and	r11d,eax
	mov	r10d,DWORD[24+rsi]
	xor	r11d,ecx
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,[((-1473231341))+r10*1+rcx]
	and	r11d,edx
	mov	r10d,DWORD[28+rsi]
	xor	r11d,ebx
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,[((-45705983))+r10*1+rbx]
	and	r11d,ecx
	mov	r10d,DWORD[32+rsi]
	xor	r11d,eax
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,[1770035416+r10*1+rax]
	and	r11d,ebx
	mov	r10d,DWORD[36+rsi]
	xor	r11d,edx
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,[((-1958414417))+r10*1+rdx]
	and	r11d,eax
	mov	r10d,DWORD[40+rsi]
	xor	r11d,ecx
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,[((-42063))+r10*1+rcx]
	and	r11d,edx
	mov	r10d,DWORD[44+rsi]
	xor	r11d,ebx
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,[((-1990404162))+r10*1+rbx]
	and	r11d,ecx
	mov	r10d,DWORD[48+rsi]
	xor	r11d,eax
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,[1804603682+r10*1+rax]
	and	r11d,ebx
	mov	r10d,DWORD[52+rsi]
	xor	r11d,edx
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,[((-40341101))+r10*1+rdx]
	and	r11d,eax
	mov	r10d,DWORD[56+rsi]
	xor	r11d,ecx
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,[((-1502002290))+r10*1+rcx]
	and	r11d,edx
	mov	r10d,DWORD[60+rsi]
	xor	r11d,ebx
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,[1236535329+r10*1+rbx]
	and	r11d,ecx
	mov	r10d,DWORD[4+rsi]
	xor	r11d,eax
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	mov	r11d,edx
	mov	r12d,edx
	not	r11d
	and	r12d,ebx
	lea	eax,[((-165796510))+r10*1+rax]
	and	r11d,ecx
	mov	r10d,DWORD[24+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	and	r12d,eax
	lea	edx,[((-1069501632))+r10*1+rdx]
	and	r11d,ebx
	mov	r10d,DWORD[44+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	and	r12d,edx
	lea	ecx,[643717713+r10*1+rcx]
	and	r11d,eax
	mov	r10d,DWORD[rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	and	r12d,ecx
	lea	ebx,[((-373897302))+r10*1+rbx]
	and	r11d,edx
	mov	r10d,DWORD[20+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	and	r12d,ebx
	lea	eax,[((-701558691))+r10*1+rax]
	and	r11d,ecx
	mov	r10d,DWORD[40+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	and	r12d,eax
	lea	edx,[38016083+r10*1+rdx]
	and	r11d,ebx
	mov	r10d,DWORD[60+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	and	r12d,edx
	lea	ecx,[((-660478335))+r10*1+rcx]
	and	r11d,eax
	mov	r10d,DWORD[16+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	and	r12d,ecx
	lea	ebx,[((-405537848))+r10*1+rbx]
	and	r11d,edx
	mov	r10d,DWORD[36+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	and	r12d,ebx
	lea	eax,[568446438+r10*1+rax]
	and	r11d,ecx
	mov	r10d,DWORD[56+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	and	r12d,eax
	lea	edx,[((-1019803690))+r10*1+rdx]
	and	r11d,ebx
	mov	r10d,DWORD[12+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	and	r12d,edx
	lea	ecx,[((-187363961))+r10*1+rcx]
	and	r11d,eax
	mov	r10d,DWORD[32+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	and	r12d,ecx
	lea	ebx,[1163531501+r10*1+rbx]
	and	r11d,edx
	mov	r10d,DWORD[52+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	and	r12d,ebx
	lea	eax,[((-1444681467))+r10*1+rax]
	and	r11d,ecx
	mov	r10d,DWORD[8+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	and	r12d,eax
	lea	edx,[((-51403784))+r10*1+rdx]
	and	r11d,ebx
	mov	r10d,DWORD[28+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	and	r12d,edx
	lea	ecx,[1735328473+r10*1+rcx]
	and	r11d,eax
	mov	r10d,DWORD[48+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	and	r12d,ecx
	lea	ebx,[((-1926607734))+r10*1+rbx]
	and	r11d,edx
	mov	r10d,DWORD[20+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	mov	r11d,ecx
	lea	eax,[((-378558))+r10*1+rax]
	xor	r11d,edx
	mov	r10d,DWORD[32+rsi]
	xor	r11d,ebx
	add	eax,r11d
	mov	r11d,ebx
	rol	eax,4
	add	eax,ebx
	lea	edx,[((-2022574463))+r10*1+rdx]
	xor	r11d,ecx
	mov	r10d,DWORD[44+rsi]
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,[1839030562+r10*1+rcx]
	xor	r11d,ebx
	mov	r10d,DWORD[56+rsi]
	xor	r11d,edx
	add	ecx,r11d
	mov	r11d,edx
	rol	ecx,16
	add	ecx,edx
	lea	ebx,[((-35309556))+r10*1+rbx]
	xor	r11d,eax
	mov	r10d,DWORD[4+rsi]
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,[((-1530992060))+r10*1+rax]
	xor	r11d,edx
	mov	r10d,DWORD[16+rsi]
	xor	r11d,ebx
	add	eax,r11d
	mov	r11d,ebx
	rol	eax,4
	add	eax,ebx
	lea	edx,[1272893353+r10*1+rdx]
	xor	r11d,ecx
	mov	r10d,DWORD[28+rsi]
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,[((-155497632))+r10*1+rcx]
	xor	r11d,ebx
	mov	r10d,DWORD[40+rsi]
	xor	r11d,edx
	add	ecx,r11d
	mov	r11d,edx
	rol	ecx,16
	add	ecx,edx
	lea	ebx,[((-1094730640))+r10*1+rbx]
	xor	r11d,eax
	mov	r10d,DWORD[52+rsi]
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,[681279174+r10*1+rax]
	xor	r11d,edx
	mov	r10d,DWORD[rsi]
	xor	r11d,ebx
	add	eax,r11d
	mov	r11d,ebx
	rol	eax,4
	add	eax,ebx
	lea	edx,[((-358537222))+r10*1+rdx]
	xor	r11d,ecx
	mov	r10d,DWORD[12+rsi]
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,[((-722521979))+r10*1+rcx]
	xor	r11d,ebx
	mov	r10d,DWORD[24+rsi]
	xor	r11d,edx
	add	ecx,r11d
	mov	r11d,edx
	rol	ecx,16
	add	ecx,edx
	lea	ebx,[76029189+r10*1+rbx]
	xor	r11d,eax
	mov	r10d,DWORD[36+rsi]
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,[((-640364487))+r10*1+rax]
	xor	r11d,edx
	mov	r10d,DWORD[48+rsi]
	xor	r11d,ebx
	add	eax,r11d
	mov	r11d,ebx
	rol	eax,4
	add	eax,ebx
	lea	edx,[((-421815835))+r10*1+rdx]
	xor	r11d,ecx
	mov	r10d,DWORD[60+rsi]
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,[530742520+r10*1+rcx]
	xor	r11d,ebx
	mov	r10d,DWORD[8+rsi]
	xor	r11d,edx
	add	ecx,r11d
	mov	r11d,edx
	rol	ecx,16
	add	ecx,edx
	lea	ebx,[((-995338651))+r10*1+rbx]
	xor	r11d,eax
	mov	r10d,DWORD[rsi]
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	mov	r11d,0xffffffff
	xor	r11d,edx
	lea	eax,[((-198630844))+r10*1+rax]
	or	r11d,ebx
	mov	r10d,DWORD[28+rsi]
	xor	r11d,ecx
	add	eax,r11d
	mov	r11d,0xffffffff
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,[1126891415+r10*1+rdx]
	or	r11d,eax
	mov	r10d,DWORD[56+rsi]
	xor	r11d,ebx
	add	edx,r11d
	mov	r11d,0xffffffff
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,[((-1416354905))+r10*1+rcx]
	or	r11d,edx
	mov	r10d,DWORD[20+rsi]
	xor	r11d,eax
	add	ecx,r11d
	mov	r11d,0xffffffff
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,[((-57434055))+r10*1+rbx]
	or	r11d,ecx
	mov	r10d,DWORD[48+rsi]
	xor	r11d,edx
	add	ebx,r11d
	mov	r11d,0xffffffff
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,[1700485571+r10*1+rax]
	or	r11d,ebx
	mov	r10d,DWORD[12+rsi]
	xor	r11d,ecx
	add	eax,r11d
	mov	r11d,0xffffffff
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,[((-1894986606))+r10*1+rdx]
	or	r11d,eax
	mov	r10d,DWORD[40+rsi]
	xor	r11d,ebx
	add	edx,r11d
	mov	r11d,0xffffffff
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,[((-1051523))+r10*1+rcx]
	or	r11d,edx
	mov	r10d,DWORD[4+rsi]
	xor	r11d,eax
	add	ecx,r11d
	mov	r11d,0xffffffff
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,[((-2054922799))+r10*1+rbx]
	or	r11d,ecx
	mov	r10d,DWORD[32+rsi]
	xor	r11d,edx
	add	ebx,r11d
	mov	r11d,0xffffffff
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,[1873313359+r10*1+rax]
	or	r11d,ebx
	mov	r10d,DWORD[60+rsi]
	xor	r11d,ecx
	add	eax,r11d
	mov	r11d,0xffffffff
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,[((-30611744))+r10*1+rdx]
	or	r11d,eax
	mov	r10d,DWORD[24+rsi]
	xor	r11d,ebx
	add	edx,r11d
	mov	r11d,0xffffffff
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,[((-1560198380))+r10*1+rcx]
	or	r11d,edx
	mov	r10d,DWORD[52+rsi]
	xor	r11d,eax
	add	ecx,r11d
	mov	r11d,0xffffffff
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,[1309151649+r10*1+rbx]
	or	r11d,ecx
	mov	r10d,DWORD[16+rsi]
	xor	r11d,edx
	add	ebx,r11d
	mov	r11d,0xffffffff
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,[((-145523070))+r10*1+rax]
	or	r11d,ebx
	mov	r10d,DWORD[44+rsi]
	xor	r11d,ecx
	add	eax,r11d
	mov	r11d,0xffffffff
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,[((-1120210379))+r10*1+rdx]
	or	r11d,eax
	mov	r10d,DWORD[8+rsi]
	xor	r11d,ebx
	add	edx,r11d
	mov	r11d,0xffffffff
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,[718787259+r10*1+rcx]
	or	r11d,edx
	mov	r10d,DWORD[36+rsi]
	xor	r11d,eax
	add	ecx,r11d
	mov	r11d,0xffffffff
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,[((-343485551))+r10*1+rbx]
	or	r11d,ecx
	mov	r10d,DWORD[rsi]
	xor	r11d,edx
	add	ebx,r11d
	mov	r11d,0xffffffff
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx

	add	eax,r8d
	add	ebx,r9d
	add	ecx,r14d
	add	edx,r15d


	add	rsi,64
	cmp	rsi,rdi
	jb	NEAR $L$loop


$L$end:
	mov	DWORD[rbp],eax
	mov	DWORD[4+rbp],ebx
	mov	DWORD[8+rbp],ecx
	mov	DWORD[12+rbp],edx

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r12,QWORD[16+rsp]

	mov	rbx,QWORD[24+rsp]

	mov	rbp,QWORD[32+rsp]

	add	rsp,40

$L$epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_md5_block_asm_data_order:
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

	lea	r10,[$L$prologue]
	cmp	rbx,r10
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]

	lea	r10,[$L$epilogue]
	cmp	rbx,r10
	jae	NEAR $L$in_prologue

	lea	rax,[40+rax]

	mov	rbp,QWORD[((-8))+rax]
	mov	rbx,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r14,QWORD[((-32))+rax]
	mov	r15,QWORD[((-40))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[216+r8],r12
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

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
	DD	$L$SEH_begin_ossl_md5_block_asm_data_order wrt ..imagebase
	DD	$L$SEH_end_ossl_md5_block_asm_data_order wrt ..imagebase
	DD	$L$SEH_info_ossl_md5_block_asm_data_order wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_ossl_md5_block_asm_data_order:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
