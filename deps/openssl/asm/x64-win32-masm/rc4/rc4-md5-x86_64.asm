OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
ALIGN	16

PUBLIC	rc4_md5_enc

rc4_md5_enc	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rc4_md5_enc::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	cmp	r9,0
	je	$L$abort
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,40
$L$body::
	mov	r11,rcx
	mov	r12,r9
	mov	r13,rsi
	mov	r14,rdx
	mov	r15,r8
	xor	rbp,rbp
	xor	rcx,rcx

	lea	rdi,QWORD PTR[8+rdi]
	mov	bpl,BYTE PTR[((-8))+rdi]
	mov	cl,BYTE PTR[((-4))+rdi]

	inc	bpl
	sub	r14,r13
	mov	eax,DWORD PTR[rbp*4+rdi]
	add	cl,al
	lea	rsi,QWORD PTR[rbp*4+rdi]
	shl	r12,6
	add	r12,r15
	mov	QWORD PTR[16+rsp],r12

	mov	QWORD PTR[24+rsp],r11
	mov	r8d,DWORD PTR[r11]
	mov	r9d,DWORD PTR[4+r11]
	mov	r10d,DWORD PTR[8+r11]
	mov	r11d,DWORD PTR[12+r11]
	jmp	$L$oop

ALIGN	16
$L$oop::
	mov	DWORD PTR[rsp],r8d
	mov	DWORD PTR[4+rsp],r9d
	mov	DWORD PTR[8+rsp],r10d
	mov	r12d,r11d
	mov	DWORD PTR[12+rsp],r11d
	pxor	xmm0,xmm0
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r8d,DWORD PTR[r15]
	add	al,dl
	mov	ebx,DWORD PTR[4+rsi]
	add	r8d,3614090360
	xor	r12d,r11d
	movzx	eax,al
	mov	DWORD PTR[rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,7
	mov	r12d,r10d
	movd	xmm0,DWORD PTR[rax*4+rdi]

	add	r8d,r9d
	pxor	xmm1,xmm1
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r11d,DWORD PTR[4+r15]
	add	bl,dl
	mov	eax,DWORD PTR[8+rsi]
	add	r11d,3905402710
	xor	r12d,r10d
	movzx	ebx,bl
	mov	DWORD PTR[4+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,12
	mov	r12d,r9d
	movd	xmm1,DWORD PTR[rbx*4+rdi]

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r10d,DWORD PTR[8+r15]
	add	al,dl
	mov	ebx,DWORD PTR[12+rsi]
	add	r10d,606105819
	xor	r12d,r9d
	movzx	eax,al
	mov	DWORD PTR[8+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,17
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],1

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r9d,DWORD PTR[12+r15]
	add	bl,dl
	mov	eax,DWORD PTR[16+rsi]
	add	r9d,3250441966
	xor	r12d,r8d
	movzx	ebx,bl
	mov	DWORD PTR[12+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,22
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],1

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r8d,DWORD PTR[16+r15]
	add	al,dl
	mov	ebx,DWORD PTR[20+rsi]
	add	r8d,4118548399
	xor	r12d,r11d
	movzx	eax,al
	mov	DWORD PTR[16+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,7
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],2

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r11d,DWORD PTR[20+r15]
	add	bl,dl
	mov	eax,DWORD PTR[24+rsi]
	add	r11d,1200080426
	xor	r12d,r10d
	movzx	ebx,bl
	mov	DWORD PTR[20+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,12
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],2

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r10d,DWORD PTR[24+r15]
	add	al,dl
	mov	ebx,DWORD PTR[28+rsi]
	add	r10d,2821735955
	xor	r12d,r9d
	movzx	eax,al
	mov	DWORD PTR[24+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,17
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],3

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r9d,DWORD PTR[28+r15]
	add	bl,dl
	mov	eax,DWORD PTR[32+rsi]
	add	r9d,4249261313
	xor	r12d,r8d
	movzx	ebx,bl
	mov	DWORD PTR[28+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,22
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],3

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r8d,DWORD PTR[32+r15]
	add	al,dl
	mov	ebx,DWORD PTR[36+rsi]
	add	r8d,1770035416
	xor	r12d,r11d
	movzx	eax,al
	mov	DWORD PTR[32+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,7
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],4

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r11d,DWORD PTR[36+r15]
	add	bl,dl
	mov	eax,DWORD PTR[40+rsi]
	add	r11d,2336552879
	xor	r12d,r10d
	movzx	ebx,bl
	mov	DWORD PTR[36+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,12
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],4

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r10d,DWORD PTR[40+r15]
	add	al,dl
	mov	ebx,DWORD PTR[44+rsi]
	add	r10d,4294925233
	xor	r12d,r9d
	movzx	eax,al
	mov	DWORD PTR[40+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,17
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],5

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r9d,DWORD PTR[44+r15]
	add	bl,dl
	mov	eax,DWORD PTR[48+rsi]
	add	r9d,2304563134
	xor	r12d,r8d
	movzx	ebx,bl
	mov	DWORD PTR[44+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,22
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],5

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r8d,DWORD PTR[48+r15]
	add	al,dl
	mov	ebx,DWORD PTR[52+rsi]
	add	r8d,1804603682
	xor	r12d,r11d
	movzx	eax,al
	mov	DWORD PTR[48+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,7
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],6

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r11d,DWORD PTR[52+r15]
	add	bl,dl
	mov	eax,DWORD PTR[56+rsi]
	add	r11d,4254626195
	xor	r12d,r10d
	movzx	ebx,bl
	mov	DWORD PTR[52+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,12
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],6

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r10d,DWORD PTR[56+r15]
	add	al,dl
	mov	ebx,DWORD PTR[60+rsi]
	add	r10d,2792965006
	xor	r12d,r9d
	movzx	eax,al
	mov	DWORD PTR[56+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,17
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],7

	add	r10d,r11d
	movdqu	xmm2,XMMWORD PTR[r13]
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r9d,DWORD PTR[60+r15]
	add	bl,dl
	mov	eax,DWORD PTR[64+rsi]
	add	r9d,1236535329
	xor	r12d,r8d
	movzx	ebx,bl
	mov	DWORD PTR[60+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,22
	mov	r12d,r10d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],7

	add	r9d,r10d
	psllq	xmm1,8
	pxor	xmm2,xmm0
	pxor	xmm2,xmm1
	pxor	xmm0,xmm0
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r8d,DWORD PTR[4+r15]
	add	al,dl
	mov	ebx,DWORD PTR[68+rsi]
	add	r8d,4129170786
	xor	r12d,r10d
	movzx	eax,al
	mov	DWORD PTR[64+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,5
	mov	r12d,r9d
	movd	xmm0,DWORD PTR[rax*4+rdi]

	add	r8d,r9d
	pxor	xmm1,xmm1
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r11d,DWORD PTR[24+r15]
	add	bl,dl
	mov	eax,DWORD PTR[72+rsi]
	add	r11d,3225465664
	xor	r12d,r9d
	movzx	ebx,bl
	mov	DWORD PTR[68+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,9
	mov	r12d,r8d
	movd	xmm1,DWORD PTR[rbx*4+rdi]

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r10d,DWORD PTR[44+r15]
	add	al,dl
	mov	ebx,DWORD PTR[76+rsi]
	add	r10d,643717713
	xor	r12d,r8d
	movzx	eax,al
	mov	DWORD PTR[72+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,14
	mov	r12d,r11d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],1

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r9d,DWORD PTR[r15]
	add	bl,dl
	mov	eax,DWORD PTR[80+rsi]
	add	r9d,3921069994
	xor	r12d,r11d
	movzx	ebx,bl
	mov	DWORD PTR[76+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,20
	mov	r12d,r10d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],1

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r8d,DWORD PTR[20+r15]
	add	al,dl
	mov	ebx,DWORD PTR[84+rsi]
	add	r8d,3593408605
	xor	r12d,r10d
	movzx	eax,al
	mov	DWORD PTR[80+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,5
	mov	r12d,r9d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],2

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r11d,DWORD PTR[40+r15]
	add	bl,dl
	mov	eax,DWORD PTR[88+rsi]
	add	r11d,38016083
	xor	r12d,r9d
	movzx	ebx,bl
	mov	DWORD PTR[84+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,9
	mov	r12d,r8d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],2

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r10d,DWORD PTR[60+r15]
	add	al,dl
	mov	ebx,DWORD PTR[92+rsi]
	add	r10d,3634488961
	xor	r12d,r8d
	movzx	eax,al
	mov	DWORD PTR[88+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,14
	mov	r12d,r11d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],3

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r9d,DWORD PTR[16+r15]
	add	bl,dl
	mov	eax,DWORD PTR[96+rsi]
	add	r9d,3889429448
	xor	r12d,r11d
	movzx	ebx,bl
	mov	DWORD PTR[92+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,20
	mov	r12d,r10d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],3

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r8d,DWORD PTR[36+r15]
	add	al,dl
	mov	ebx,DWORD PTR[100+rsi]
	add	r8d,568446438
	xor	r12d,r10d
	movzx	eax,al
	mov	DWORD PTR[96+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,5
	mov	r12d,r9d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],4

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r11d,DWORD PTR[56+r15]
	add	bl,dl
	mov	eax,DWORD PTR[104+rsi]
	add	r11d,3275163606
	xor	r12d,r9d
	movzx	ebx,bl
	mov	DWORD PTR[100+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,9
	mov	r12d,r8d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],4

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r10d,DWORD PTR[12+r15]
	add	al,dl
	mov	ebx,DWORD PTR[108+rsi]
	add	r10d,4107603335
	xor	r12d,r8d
	movzx	eax,al
	mov	DWORD PTR[104+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,14
	mov	r12d,r11d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],5

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r9d,DWORD PTR[32+r15]
	add	bl,dl
	mov	eax,DWORD PTR[112+rsi]
	add	r9d,1163531501
	xor	r12d,r11d
	movzx	ebx,bl
	mov	DWORD PTR[108+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,20
	mov	r12d,r10d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],5

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r11d
	add	r8d,DWORD PTR[52+r15]
	add	al,dl
	mov	ebx,DWORD PTR[116+rsi]
	add	r8d,2850285829
	xor	r12d,r10d
	movzx	eax,al
	mov	DWORD PTR[112+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,5
	mov	r12d,r9d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],6

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r10d
	add	r11d,DWORD PTR[8+r15]
	add	bl,dl
	mov	eax,DWORD PTR[120+rsi]
	add	r11d,4243563512
	xor	r12d,r9d
	movzx	ebx,bl
	mov	DWORD PTR[116+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,9
	mov	r12d,r8d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],6

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	and	r12d,r9d
	add	r10d,DWORD PTR[28+r15]
	add	al,dl
	mov	ebx,DWORD PTR[124+rsi]
	add	r10d,1735328473
	xor	r12d,r8d
	movzx	eax,al
	mov	DWORD PTR[120+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,14
	mov	r12d,r11d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],7

	add	r10d,r11d
	movdqu	xmm3,XMMWORD PTR[16+r13]
	add	bpl,32
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	and	r12d,r8d
	add	r9d,DWORD PTR[48+r15]
	add	bl,dl
	mov	eax,DWORD PTR[rbp*4+rdi]
	add	r9d,2368359562
	xor	r12d,r11d
	movzx	ebx,bl
	mov	DWORD PTR[124+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,20
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],7

	add	r9d,r10d
	mov	rsi,rcx
	xor	rcx,rcx
	mov	cl,sil
	lea	rsi,QWORD PTR[rbp*4+rdi]
	psllq	xmm1,8
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1
	pxor	xmm0,xmm0
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r9d
	add	r8d,DWORD PTR[20+r15]
	add	al,dl
	mov	ebx,DWORD PTR[4+rsi]
	add	r8d,4294588738
	movzx	eax,al
	add	r8d,r12d
	mov	DWORD PTR[rsi],edx
	add	cl,bl
	rol	r8d,4
	mov	r12d,r10d
	movd	xmm0,DWORD PTR[rax*4+rdi]

	add	r8d,r9d
	pxor	xmm1,xmm1
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r8d
	add	r11d,DWORD PTR[32+r15]
	add	bl,dl
	mov	eax,DWORD PTR[8+rsi]
	add	r11d,2272392833
	movzx	ebx,bl
	add	r11d,r12d
	mov	DWORD PTR[4+rsi],edx
	add	cl,al
	rol	r11d,11
	mov	r12d,r9d
	movd	xmm1,DWORD PTR[rbx*4+rdi]

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r11d
	add	r10d,DWORD PTR[44+r15]
	add	al,dl
	mov	ebx,DWORD PTR[12+rsi]
	add	r10d,1839030562
	movzx	eax,al
	add	r10d,r12d
	mov	DWORD PTR[8+rsi],edx
	add	cl,bl
	rol	r10d,16
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],1

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r10d
	add	r9d,DWORD PTR[56+r15]
	add	bl,dl
	mov	eax,DWORD PTR[16+rsi]
	add	r9d,4259657740
	movzx	ebx,bl
	add	r9d,r12d
	mov	DWORD PTR[12+rsi],edx
	add	cl,al
	rol	r9d,23
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],1

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r9d
	add	r8d,DWORD PTR[4+r15]
	add	al,dl
	mov	ebx,DWORD PTR[20+rsi]
	add	r8d,2763975236
	movzx	eax,al
	add	r8d,r12d
	mov	DWORD PTR[16+rsi],edx
	add	cl,bl
	rol	r8d,4
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],2

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r8d
	add	r11d,DWORD PTR[16+r15]
	add	bl,dl
	mov	eax,DWORD PTR[24+rsi]
	add	r11d,1272893353
	movzx	ebx,bl
	add	r11d,r12d
	mov	DWORD PTR[20+rsi],edx
	add	cl,al
	rol	r11d,11
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],2

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r11d
	add	r10d,DWORD PTR[28+r15]
	add	al,dl
	mov	ebx,DWORD PTR[28+rsi]
	add	r10d,4139469664
	movzx	eax,al
	add	r10d,r12d
	mov	DWORD PTR[24+rsi],edx
	add	cl,bl
	rol	r10d,16
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],3

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r10d
	add	r9d,DWORD PTR[40+r15]
	add	bl,dl
	mov	eax,DWORD PTR[32+rsi]
	add	r9d,3200236656
	movzx	ebx,bl
	add	r9d,r12d
	mov	DWORD PTR[28+rsi],edx
	add	cl,al
	rol	r9d,23
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],3

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r9d
	add	r8d,DWORD PTR[52+r15]
	add	al,dl
	mov	ebx,DWORD PTR[36+rsi]
	add	r8d,681279174
	movzx	eax,al
	add	r8d,r12d
	mov	DWORD PTR[32+rsi],edx
	add	cl,bl
	rol	r8d,4
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],4

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r8d
	add	r11d,DWORD PTR[r15]
	add	bl,dl
	mov	eax,DWORD PTR[40+rsi]
	add	r11d,3936430074
	movzx	ebx,bl
	add	r11d,r12d
	mov	DWORD PTR[36+rsi],edx
	add	cl,al
	rol	r11d,11
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],4

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r11d
	add	r10d,DWORD PTR[12+r15]
	add	al,dl
	mov	ebx,DWORD PTR[44+rsi]
	add	r10d,3572445317
	movzx	eax,al
	add	r10d,r12d
	mov	DWORD PTR[40+rsi],edx
	add	cl,bl
	rol	r10d,16
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],5

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r10d
	add	r9d,DWORD PTR[24+r15]
	add	bl,dl
	mov	eax,DWORD PTR[48+rsi]
	add	r9d,76029189
	movzx	ebx,bl
	add	r9d,r12d
	mov	DWORD PTR[44+rsi],edx
	add	cl,al
	rol	r9d,23
	mov	r12d,r11d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],5

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r9d
	add	r8d,DWORD PTR[36+r15]
	add	al,dl
	mov	ebx,DWORD PTR[52+rsi]
	add	r8d,3654602809
	movzx	eax,al
	add	r8d,r12d
	mov	DWORD PTR[48+rsi],edx
	add	cl,bl
	rol	r8d,4
	mov	r12d,r10d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],6

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r8d
	add	r11d,DWORD PTR[48+r15]
	add	bl,dl
	mov	eax,DWORD PTR[56+rsi]
	add	r11d,3873151461
	movzx	ebx,bl
	add	r11d,r12d
	mov	DWORD PTR[52+rsi],edx
	add	cl,al
	rol	r11d,11
	mov	r12d,r9d
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],6

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],eax
	xor	r12d,r11d
	add	r10d,DWORD PTR[60+r15]
	add	al,dl
	mov	ebx,DWORD PTR[60+rsi]
	add	r10d,530742520
	movzx	eax,al
	add	r10d,r12d
	mov	DWORD PTR[56+rsi],edx
	add	cl,bl
	rol	r10d,16
	mov	r12d,r8d
	pinsrw	xmm0,WORD PTR[rax*4+rdi],7

	add	r10d,r11d
	movdqu	xmm4,XMMWORD PTR[32+r13]
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],ebx
	xor	r12d,r10d
	add	r9d,DWORD PTR[8+r15]
	add	bl,dl
	mov	eax,DWORD PTR[64+rsi]
	add	r9d,3299628645
	movzx	ebx,bl
	add	r9d,r12d
	mov	DWORD PTR[60+rsi],edx
	add	cl,al
	rol	r9d,23
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],7

	add	r9d,r10d
	psllq	xmm1,8
	pxor	xmm4,xmm0
	pxor	xmm4,xmm1
	pxor	xmm0,xmm0
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r9d
	add	r8d,DWORD PTR[r15]
	add	al,dl
	mov	ebx,DWORD PTR[68+rsi]
	add	r8d,4096336452
	movzx	eax,al
	xor	r12d,r10d
	mov	DWORD PTR[64+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,6
	mov	r12d,-1
	movd	xmm0,DWORD PTR[rax*4+rdi]

	add	r8d,r9d
	pxor	xmm1,xmm1
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r8d
	add	r11d,DWORD PTR[28+r15]
	add	bl,dl
	mov	eax,DWORD PTR[72+rsi]
	add	r11d,1126891415
	movzx	ebx,bl
	xor	r12d,r9d
	mov	DWORD PTR[68+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,10
	mov	r12d,-1
	movd	xmm1,DWORD PTR[rbx*4+rdi]

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r11d
	add	r10d,DWORD PTR[56+r15]
	add	al,dl
	mov	ebx,DWORD PTR[76+rsi]
	add	r10d,2878612391
	movzx	eax,al
	xor	r12d,r8d
	mov	DWORD PTR[72+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,15
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],1

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r10d
	add	r9d,DWORD PTR[20+r15]
	add	bl,dl
	mov	eax,DWORD PTR[80+rsi]
	add	r9d,4237533241
	movzx	ebx,bl
	xor	r12d,r11d
	mov	DWORD PTR[76+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,21
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],1

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r9d
	add	r8d,DWORD PTR[48+r15]
	add	al,dl
	mov	ebx,DWORD PTR[84+rsi]
	add	r8d,1700485571
	movzx	eax,al
	xor	r12d,r10d
	mov	DWORD PTR[80+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,6
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],2

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r8d
	add	r11d,DWORD PTR[12+r15]
	add	bl,dl
	mov	eax,DWORD PTR[88+rsi]
	add	r11d,2399980690
	movzx	ebx,bl
	xor	r12d,r9d
	mov	DWORD PTR[84+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,10
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],2

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r11d
	add	r10d,DWORD PTR[40+r15]
	add	al,dl
	mov	ebx,DWORD PTR[92+rsi]
	add	r10d,4293915773
	movzx	eax,al
	xor	r12d,r8d
	mov	DWORD PTR[88+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,15
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],3

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r10d
	add	r9d,DWORD PTR[4+r15]
	add	bl,dl
	mov	eax,DWORD PTR[96+rsi]
	add	r9d,2240044497
	movzx	ebx,bl
	xor	r12d,r11d
	mov	DWORD PTR[92+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,21
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],3

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r9d
	add	r8d,DWORD PTR[32+r15]
	add	al,dl
	mov	ebx,DWORD PTR[100+rsi]
	add	r8d,1873313359
	movzx	eax,al
	xor	r12d,r10d
	mov	DWORD PTR[96+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,6
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],4

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r8d
	add	r11d,DWORD PTR[60+r15]
	add	bl,dl
	mov	eax,DWORD PTR[104+rsi]
	add	r11d,4264355552
	movzx	ebx,bl
	xor	r12d,r9d
	mov	DWORD PTR[100+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,10
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],4

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r11d
	add	r10d,DWORD PTR[24+r15]
	add	al,dl
	mov	ebx,DWORD PTR[108+rsi]
	add	r10d,2734768916
	movzx	eax,al
	xor	r12d,r8d
	mov	DWORD PTR[104+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,15
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],5

	add	r10d,r11d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r10d
	add	r9d,DWORD PTR[52+r15]
	add	bl,dl
	mov	eax,DWORD PTR[112+rsi]
	add	r9d,1309151649
	movzx	ebx,bl
	xor	r12d,r11d
	mov	DWORD PTR[108+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,21
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],5

	add	r9d,r10d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r11d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r9d
	add	r8d,DWORD PTR[16+r15]
	add	al,dl
	mov	ebx,DWORD PTR[116+rsi]
	add	r8d,4149444226
	movzx	eax,al
	xor	r12d,r10d
	mov	DWORD PTR[112+rsi],edx
	add	r8d,r12d
	add	cl,bl
	rol	r8d,6
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],6

	add	r8d,r9d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r10d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r8d
	add	r11d,DWORD PTR[44+r15]
	add	bl,dl
	mov	eax,DWORD PTR[120+rsi]
	add	r11d,3174756917
	movzx	ebx,bl
	xor	r12d,r9d
	mov	DWORD PTR[116+rsi],edx
	add	r11d,r12d
	add	cl,al
	rol	r11d,10
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],6

	add	r11d,r8d
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r9d
	mov	DWORD PTR[rcx*4+rdi],eax
	or	r12d,r11d
	add	r10d,DWORD PTR[8+r15]
	add	al,dl
	mov	ebx,DWORD PTR[124+rsi]
	add	r10d,718787259
	movzx	eax,al
	xor	r12d,r8d
	mov	DWORD PTR[120+rsi],edx
	add	r10d,r12d
	add	cl,bl
	rol	r10d,15
	mov	r12d,-1
	pinsrw	xmm0,WORD PTR[rax*4+rdi],7

	add	r10d,r11d
	movdqu	xmm5,XMMWORD PTR[48+r13]
	add	bpl,32
	mov	edx,DWORD PTR[rcx*4+rdi]
	xor	r12d,r8d
	mov	DWORD PTR[rcx*4+rdi],ebx
	or	r12d,r10d
	add	r9d,DWORD PTR[36+r15]
	add	bl,dl
	mov	eax,DWORD PTR[rbp*4+rdi]
	add	r9d,3951481745
	movzx	ebx,bl
	xor	r12d,r11d
	mov	DWORD PTR[124+rsi],edx
	add	r9d,r12d
	add	cl,al
	rol	r9d,21
	mov	r12d,-1
	pinsrw	xmm1,WORD PTR[rbx*4+rdi],7

	add	r9d,r10d
	mov	rsi,rbp
	xor	rbp,rbp
	mov	bpl,sil
	mov	rsi,rcx
	xor	rcx,rcx
	mov	cl,sil
	lea	rsi,QWORD PTR[rbp*4+rdi]
	psllq	xmm1,8
	pxor	xmm5,xmm0
	pxor	xmm5,xmm1
	add	r8d,DWORD PTR[rsp]
	add	r9d,DWORD PTR[4+rsp]
	add	r10d,DWORD PTR[8+rsp]
	add	r11d,DWORD PTR[12+rsp]

	movdqu	XMMWORD PTR[r13*1+r14],xmm2
	movdqu	XMMWORD PTR[16+r13*1+r14],xmm3
	movdqu	XMMWORD PTR[32+r13*1+r14],xmm4
	movdqu	XMMWORD PTR[48+r13*1+r14],xmm5
	lea	r15,QWORD PTR[64+r15]
	lea	r13,QWORD PTR[64+r13]
	cmp	r15,QWORD PTR[16+rsp]
	jb	$L$oop

	mov	r12,QWORD PTR[24+rsp]
	sub	cl,al
	mov	DWORD PTR[r12],r8d
	mov	DWORD PTR[4+r12],r9d
	mov	DWORD PTR[8+r12],r10d
	mov	DWORD PTR[12+r12],r11d
	sub	bpl,1
	mov	DWORD PTR[((-8))+rdi],ebp
	mov	DWORD PTR[((-4))+rdi],ecx

	mov	r15,QWORD PTR[40+rsp]
	mov	r14,QWORD PTR[48+rsp]
	mov	r13,QWORD PTR[56+rsp]
	mov	r12,QWORD PTR[64+rsp]
	mov	rbp,QWORD PTR[72+rsp]
	mov	rbx,QWORD PTR[80+rsp]
	lea	rsp,QWORD PTR[88+rsp]
$L$epilogue::
$L$abort::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rc4_md5_enc::
rc4_md5_enc	ENDP
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

	lea	r10,QWORD PTR[$L$body]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

	mov	r15,QWORD PTR[40+rax]
	mov	r14,QWORD PTR[48+rax]
	mov	r13,QWORD PTR[56+rax]
	mov	r12,QWORD PTR[64+rax]
	mov	rbp,QWORD PTR[72+rax]
	mov	rbx,QWORD PTR[80+rax]
	lea	rax,QWORD PTR[88+rax]

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
	DD	imagerel $L$SEH_begin_rc4_md5_enc
	DD	imagerel $L$SEH_end_rc4_md5_enc
	DD	imagerel $L$SEH_info_rc4_md5_enc

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_rc4_md5_enc::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
