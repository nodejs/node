OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
ALIGN	16

PUBLIC	md5_block_asm_data_order

md5_block_asm_data_order	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_md5_block_asm_data_order::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbp
	push	rbx
	push	r12
	push	r14
	push	r15
$L$prologue::




	mov	rbp,rdi
	shl	rdx,6
	lea	rdi,QWORD PTR[rdx*1+rsi]
	mov	eax,DWORD PTR[rbp]
	mov	ebx,DWORD PTR[4+rbp]
	mov	ecx,DWORD PTR[8+rbp]
	mov	edx,DWORD PTR[12+rbp]







	cmp	rsi,rdi
	je	$L$end



$L$loop::
	mov	r8d,eax
	mov	r9d,ebx
	mov	r14d,ecx
	mov	r15d,edx
	mov	r10d,DWORD PTR[rsi]
	mov	r11d,edx
	xor	r11d,ecx
	lea	eax,DWORD PTR[((-680876936))+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[4+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[((-389564586))+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[8+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[606105819+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[12+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[((-1044525330))+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[16+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[((-176418897))+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[20+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[1200080426+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[24+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[((-1473231341))+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[28+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[((-45705983))+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[32+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[1770035416+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[36+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[((-1958414417))+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[40+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[((-42063))+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[44+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[((-1990404162))+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[48+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[1804603682+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[52+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[((-40341101))+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[56+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[((-1502002290))+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[60+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[1236535329+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	mov	r10d,DWORD PTR[4+rsi]
	mov	r11d,edx
	mov	r12d,edx
	not	r11d
	lea	eax,DWORD PTR[((-165796510))+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[24+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[((-1069501632))+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[44+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[643717713+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[((-373897302))+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[20+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[((-701558691))+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[40+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[38016083+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[60+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[((-660478335))+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[16+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[((-405537848))+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[36+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[568446438+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[56+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[((-1019803690))+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[12+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[((-187363961))+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[32+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[1163531501+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[52+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[((-1444681467))+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[8+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[((-51403784))+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[28+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[1735328473+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[48+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[((-1926607734))+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	mov	r10d,DWORD PTR[20+rsi]
	mov	r11d,ecx
	lea	eax,DWORD PTR[((-378558))+r10*1+rax]
	mov	r10d,DWORD PTR[32+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[((-2022574463))+r10*1+rdx]
	mov	r10d,DWORD PTR[44+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[1839030562+r10*1+rcx]
	mov	r10d,DWORD PTR[56+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[((-35309556))+r10*1+rbx]
	mov	r10d,DWORD PTR[4+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[((-1530992060))+r10*1+rax]
	mov	r10d,DWORD PTR[16+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[1272893353+r10*1+rdx]
	mov	r10d,DWORD PTR[28+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[((-155497632))+r10*1+rcx]
	mov	r10d,DWORD PTR[40+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[((-1094730640))+r10*1+rbx]
	mov	r10d,DWORD PTR[52+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[681279174+r10*1+rax]
	mov	r10d,DWORD PTR[rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[((-358537222))+r10*1+rdx]
	mov	r10d,DWORD PTR[12+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[((-722521979))+r10*1+rcx]
	mov	r10d,DWORD PTR[24+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[76029189+r10*1+rbx]
	mov	r10d,DWORD PTR[36+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[((-640364487))+r10*1+rax]
	mov	r10d,DWORD PTR[48+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[((-421815835))+r10*1+rdx]
	mov	r10d,DWORD PTR[60+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[530742520+r10*1+rcx]
	mov	r10d,DWORD PTR[8+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[((-995338651))+r10*1+rbx]
	mov	r10d,DWORD PTR[rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	mov	r10d,DWORD PTR[rsi]
	mov	r11d,0ffffffffh
	xor	r11d,edx
	lea	eax,DWORD PTR[((-198630844))+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[28+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[1126891415+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[56+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[((-1416354905))+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[20+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[((-57434055))+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[48+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[1700485571+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[12+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[((-1894986606))+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[40+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[((-1051523))+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[4+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[((-2054922799))+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[32+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[1873313359+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[60+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[((-30611744))+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[24+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[((-1560198380))+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[52+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[1309151649+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[16+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[((-145523070))+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[44+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[((-1120210379))+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[8+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[718787259+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[36+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[((-343485551))+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx

	add	eax,r8d
	add	ebx,r9d
	add	ecx,r14d
	add	edx,r15d


	add	rsi,64
	cmp	rsi,rdi
	jb	$L$loop



$L$end::
	mov	DWORD PTR[rbp],eax
	mov	DWORD PTR[4+rbp],ebx
	mov	DWORD PTR[8+rbp],ecx
	mov	DWORD PTR[12+rbp],edx

	mov	r15,QWORD PTR[rsp]
	mov	r14,QWORD PTR[8+rsp]
	mov	r12,QWORD PTR[16+rsp]
	mov	rbx,QWORD PTR[24+rsp]
	mov	rbp,QWORD PTR[32+rsp]
	add	rsp,40
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_md5_block_asm_data_order::
md5_block_asm_data_order	ENDP
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

	lea	rax,QWORD PTR[40+rax]

	mov	rbp,QWORD PTR[((-8))+rax]
	mov	rbx,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r14,QWORD PTR[((-32))+rax]
	mov	r15,QWORD PTR[((-40))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
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
	DD	imagerel $L$SEH_begin_md5_block_asm_data_order
	DD	imagerel $L$SEH_end_md5_block_asm_data_order
	DD	imagerel $L$SEH_info_md5_block_asm_data_order

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_md5_block_asm_data_order::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
