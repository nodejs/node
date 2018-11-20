OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	gcm_gmult_4bit

ALIGN	16
gcm_gmult_4bit	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_gcm_gmult_4bit::
	mov	rdi,rcx
	mov	rsi,rdx


	push	rbx
	push	rbp
	push	r12
$L$gmult_prologue::

	movzx	r8,BYTE PTR[15+rdi]
	lea	r11,QWORD PTR[$L$rem_4bit]
	xor	rax,rax
	xor	rbx,rbx
	mov	al,r8b
	mov	bl,r8b
	shl	al,4
	mov	rcx,14
	mov	r8,QWORD PTR[8+rax*1+rsi]
	mov	r9,QWORD PTR[rax*1+rsi]
	and	bl,0f0h
	mov	rdx,r8
	jmp	$L$oop1

ALIGN	16
$L$oop1::
	shr	r8,4
	and	rdx,0fh
	mov	r10,r9
	mov	al,BYTE PTR[rcx*1+rdi]
	shr	r9,4
	xor	r8,QWORD PTR[8+rbx*1+rsi]
	shl	r10,60
	xor	r9,QWORD PTR[rbx*1+rsi]
	mov	bl,al
	xor	r9,QWORD PTR[rdx*8+r11]
	mov	rdx,r8
	shl	al,4
	xor	r8,r10
	dec	rcx
	js	$L$break1

	shr	r8,4
	and	rdx,0fh
	mov	r10,r9
	shr	r9,4
	xor	r8,QWORD PTR[8+rax*1+rsi]
	shl	r10,60
	xor	r9,QWORD PTR[rax*1+rsi]
	and	bl,0f0h
	xor	r9,QWORD PTR[rdx*8+r11]
	mov	rdx,r8
	xor	r8,r10
	jmp	$L$oop1

ALIGN	16
$L$break1::
	shr	r8,4
	and	rdx,0fh
	mov	r10,r9
	shr	r9,4
	xor	r8,QWORD PTR[8+rax*1+rsi]
	shl	r10,60
	xor	r9,QWORD PTR[rax*1+rsi]
	and	bl,0f0h
	xor	r9,QWORD PTR[rdx*8+r11]
	mov	rdx,r8
	xor	r8,r10

	shr	r8,4
	and	rdx,0fh
	mov	r10,r9
	shr	r9,4
	xor	r8,QWORD PTR[8+rbx*1+rsi]
	shl	r10,60
	xor	r9,QWORD PTR[rbx*1+rsi]
	xor	r8,r10
	xor	r9,QWORD PTR[rdx*8+r11]

	bswap	r8
	bswap	r9
	mov	QWORD PTR[8+rdi],r8
	mov	QWORD PTR[rdi],r9

	mov	rbx,QWORD PTR[16+rsp]
	lea	rsp,QWORD PTR[24+rsp]
$L$gmult_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_gcm_gmult_4bit::
gcm_gmult_4bit	ENDP
PUBLIC	gcm_ghash_4bit

ALIGN	16
gcm_ghash_4bit	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_gcm_ghash_4bit::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,280
$L$ghash_prologue::
	mov	r14,rdx
	mov	r15,rcx
	sub	rsi,-128
	lea	rbp,QWORD PTR[((16+128))+rsp]
	xor	edx,edx
	mov	r8,QWORD PTR[((0+0-128))+rsi]
	mov	rax,QWORD PTR[((0+8-128))+rsi]
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	r9,QWORD PTR[((16+0-128))+rsi]
	shl	dl,4
	mov	rbx,QWORD PTR[((16+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[rbp],r8
	mov	r8,QWORD PTR[((32+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((0-128))+rbp],rax
	mov	rax,QWORD PTR[((32+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[1+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[8+rbp],r9
	mov	r9,QWORD PTR[((48+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((8-128))+rbp],rbx
	mov	rbx,QWORD PTR[((48+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[2+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[16+rbp],r8
	mov	r8,QWORD PTR[((64+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((16-128))+rbp],rax
	mov	rax,QWORD PTR[((64+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[3+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[24+rbp],r9
	mov	r9,QWORD PTR[((80+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((24-128))+rbp],rbx
	mov	rbx,QWORD PTR[((80+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[4+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[32+rbp],r8
	mov	r8,QWORD PTR[((96+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((32-128))+rbp],rax
	mov	rax,QWORD PTR[((96+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[5+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[40+rbp],r9
	mov	r9,QWORD PTR[((112+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((40-128))+rbp],rbx
	mov	rbx,QWORD PTR[((112+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[6+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[48+rbp],r8
	mov	r8,QWORD PTR[((128+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((48-128))+rbp],rax
	mov	rax,QWORD PTR[((128+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[7+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[56+rbp],r9
	mov	r9,QWORD PTR[((144+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((56-128))+rbp],rbx
	mov	rbx,QWORD PTR[((144+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[8+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[64+rbp],r8
	mov	r8,QWORD PTR[((160+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((64-128))+rbp],rax
	mov	rax,QWORD PTR[((160+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[9+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[72+rbp],r9
	mov	r9,QWORD PTR[((176+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((72-128))+rbp],rbx
	mov	rbx,QWORD PTR[((176+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[10+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[80+rbp],r8
	mov	r8,QWORD PTR[((192+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((80-128))+rbp],rax
	mov	rax,QWORD PTR[((192+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[11+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[88+rbp],r9
	mov	r9,QWORD PTR[((208+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((88-128))+rbp],rbx
	mov	rbx,QWORD PTR[((208+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[12+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[96+rbp],r8
	mov	r8,QWORD PTR[((224+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((96-128))+rbp],rax
	mov	rax,QWORD PTR[((224+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[13+rsp],dl
	or	rbx,r10
	mov	dl,al
	shr	rax,4
	mov	r10,r8
	shr	r8,4
	mov	QWORD PTR[104+rbp],r9
	mov	r9,QWORD PTR[((240+0-128))+rsi]
	shl	dl,4
	mov	QWORD PTR[((104-128))+rbp],rbx
	mov	rbx,QWORD PTR[((240+8-128))+rsi]
	shl	r10,60
	mov	BYTE PTR[14+rsp],dl
	or	rax,r10
	mov	dl,bl
	shr	rbx,4
	mov	r10,r9
	shr	r9,4
	mov	QWORD PTR[112+rbp],r8
	shl	dl,4
	mov	QWORD PTR[((112-128))+rbp],rax
	shl	r10,60
	mov	BYTE PTR[15+rsp],dl
	or	rbx,r10
	mov	QWORD PTR[120+rbp],r9
	mov	QWORD PTR[((120-128))+rbp],rbx
	add	rsi,-128
	mov	r8,QWORD PTR[8+rdi]
	mov	r9,QWORD PTR[rdi]
	add	r15,r14
	lea	r11,QWORD PTR[$L$rem_8bit]
	jmp	$L$outer_loop
ALIGN	16
$L$outer_loop::
	xor	r9,QWORD PTR[r14]
	mov	rdx,QWORD PTR[8+r14]
	lea	r14,QWORD PTR[16+r14]
	xor	rdx,r8
	mov	QWORD PTR[rdi],r9
	mov	QWORD PTR[8+rdi],rdx
	shr	rdx,32
	xor	rax,rax
	rol	edx,8
	mov	al,dl
	movzx	ebx,dl
	shl	al,4
	shr	ebx,4
	rol	edx,8
	mov	r8,QWORD PTR[8+rax*1+rsi]
	mov	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	xor	r12,r8
	mov	r10,r9
	shr	r8,8
	movzx	r12,r12b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	mov	edx,DWORD PTR[8+rdi]
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	mov	edx,DWORD PTR[4+rdi]
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	mov	edx,DWORD PTR[rdi]
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	shr	ecx,4
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r12,WORD PTR[r12*2+r11]
	movzx	ebx,dl
	shl	al,4
	movzx	r13,BYTE PTR[rcx*1+rsp]
	shr	ebx,4
	shl	r12,48
	xor	r13,r8
	mov	r10,r9
	xor	r9,r12
	shr	r8,8
	movzx	r13,r13b
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rcx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rcx*8+rbp]
	rol	edx,8
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	mov	al,dl
	xor	r8,r10
	movzx	r13,WORD PTR[r13*2+r11]
	movzx	ecx,dl
	shl	al,4
	movzx	r12,BYTE PTR[rbx*1+rsp]
	and	ecx,240
	shl	r13,48
	xor	r12,r8
	mov	r10,r9
	xor	r9,r13
	shr	r8,8
	movzx	r12,r12b
	mov	edx,DWORD PTR[((-4))+rdi]
	shr	r9,8
	xor	r8,QWORD PTR[((-128))+rbx*8+rbp]
	shl	r10,56
	xor	r9,QWORD PTR[rbx*8+rbp]
	movzx	r12,WORD PTR[r12*2+r11]
	xor	r8,QWORD PTR[8+rax*1+rsi]
	xor	r9,QWORD PTR[rax*1+rsi]
	shl	r12,48
	xor	r8,r10
	xor	r9,r12
	movzx	r13,r8b
	shr	r8,4
	mov	r10,r9
	shl	r13b,4
	shr	r9,4
	xor	r8,QWORD PTR[8+rcx*1+rsi]
	movzx	r13,WORD PTR[r13*2+r11]
	shl	r10,60
	xor	r9,QWORD PTR[rcx*1+rsi]
	xor	r8,r10
	shl	r13,48
	bswap	r8
	xor	r9,r13
	bswap	r9
	cmp	r14,r15
	jb	$L$outer_loop
	mov	QWORD PTR[8+rdi],r8
	mov	QWORD PTR[rdi],r9

	lea	rsi,QWORD PTR[280+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$ghash_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_gcm_ghash_4bit::
gcm_ghash_4bit	ENDP
PUBLIC	gcm_init_clmul

ALIGN	16
gcm_init_clmul	PROC PUBLIC
$L$_init_clmul::
$L$SEH_begin_gcm_init_clmul::

DB	048h,083h,0ech,018h
DB	00fh,029h,034h,024h
	movdqu	xmm2,XMMWORD PTR[rdx]
	pshufd	xmm2,xmm2,78


	pshufd	xmm4,xmm2,255
	movdqa	xmm3,xmm2
	psllq	xmm2,1
	pxor	xmm5,xmm5
	psrlq	xmm3,63
	pcmpgtd	xmm5,xmm4
	pslldq	xmm3,8
	por	xmm2,xmm3


	pand	xmm5,XMMWORD PTR[$L$0x1c2_polynomial]
	pxor	xmm2,xmm5


	pshufd	xmm6,xmm2,78
	movdqa	xmm0,xmm2
	pxor	xmm6,xmm2
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,222,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	pshufd	xmm3,xmm2,78
	pshufd	xmm4,xmm0,78
	pxor	xmm3,xmm2
	movdqu	XMMWORD PTR[rcx],xmm2
	pxor	xmm4,xmm0
	movdqu	XMMWORD PTR[16+rcx],xmm0
DB	102,15,58,15,227,8
	movdqu	XMMWORD PTR[32+rcx],xmm4
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,222,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	movdqa	xmm5,xmm0
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,222,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	pshufd	xmm3,xmm5,78
	pshufd	xmm4,xmm0,78
	pxor	xmm3,xmm5
	movdqu	XMMWORD PTR[48+rcx],xmm5
	pxor	xmm4,xmm0
	movdqu	XMMWORD PTR[64+rcx],xmm0
DB	102,15,58,15,227,8
	movdqu	XMMWORD PTR[80+rcx],xmm4
	movaps	xmm6,XMMWORD PTR[rsp]
	lea	rsp,QWORD PTR[24+rsp]
$L$SEH_end_gcm_init_clmul::
	DB	0F3h,0C3h		;repret
gcm_init_clmul	ENDP
PUBLIC	gcm_gmult_clmul

ALIGN	16
gcm_gmult_clmul	PROC PUBLIC
$L$_gmult_clmul::
	movdqu	xmm0,XMMWORD PTR[rcx]
	movdqa	xmm5,XMMWORD PTR[$L$bswap_mask]
	movdqu	xmm2,XMMWORD PTR[rdx]
	movdqu	xmm4,XMMWORD PTR[32+rdx]
DB	102,15,56,0,197
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,220,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
DB	102,15,56,0,197
	movdqu	XMMWORD PTR[rcx],xmm0
	DB	0F3h,0C3h		;repret
gcm_gmult_clmul	ENDP
PUBLIC	gcm_ghash_clmul

ALIGN	32
gcm_ghash_clmul	PROC PUBLIC
$L$_ghash_clmul::
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_gcm_ghash_clmul::

DB	048h,08dh,060h,0e0h
DB	00fh,029h,070h,0e0h
DB	00fh,029h,078h,0f0h
DB	044h,00fh,029h,000h
DB	044h,00fh,029h,048h,010h
DB	044h,00fh,029h,050h,020h
DB	044h,00fh,029h,058h,030h
DB	044h,00fh,029h,060h,040h
DB	044h,00fh,029h,068h,050h
DB	044h,00fh,029h,070h,060h
DB	044h,00fh,029h,078h,070h
	movdqa	xmm10,XMMWORD PTR[$L$bswap_mask]

	movdqu	xmm0,XMMWORD PTR[rcx]
	movdqu	xmm2,XMMWORD PTR[rdx]
	movdqu	xmm7,XMMWORD PTR[32+rdx]
DB	102,65,15,56,0,194

	sub	r9,010h
	jz	$L$odd_tail

	movdqu	xmm6,XMMWORD PTR[16+rdx]
	mov	eax,DWORD PTR[((OPENSSL_ia32cap_P+4))]
	cmp	r9,030h
	jb	$L$skip4x

	and	eax,71303168
	cmp	eax,4194304
	je	$L$skip4x

	sub	r9,030h
	mov	rax,0A040608020C0E000h
	movdqu	xmm14,XMMWORD PTR[48+rdx]
	movdqu	xmm15,XMMWORD PTR[64+rdx]




	movdqu	xmm3,XMMWORD PTR[48+r8]
	movdqu	xmm11,XMMWORD PTR[32+r8]
DB	102,65,15,56,0,218
DB	102,69,15,56,0,218
	movdqa	xmm5,xmm3
	pshufd	xmm4,xmm3,78
	pxor	xmm4,xmm3
DB	102,15,58,68,218,0
DB	102,15,58,68,234,17
DB	102,15,58,68,231,0

	movdqa	xmm13,xmm11
	pshufd	xmm12,xmm11,78
	pxor	xmm12,xmm11
DB	102,68,15,58,68,222,0
DB	102,68,15,58,68,238,17
DB	102,68,15,58,68,231,16
	xorps	xmm3,xmm11
	xorps	xmm5,xmm13
	movups	xmm7,XMMWORD PTR[80+rdx]
	xorps	xmm4,xmm12

	movdqu	xmm11,XMMWORD PTR[16+r8]
	movdqu	xmm8,XMMWORD PTR[r8]
DB	102,69,15,56,0,218
DB	102,69,15,56,0,194
	movdqa	xmm13,xmm11
	pshufd	xmm12,xmm11,78
	pxor	xmm0,xmm8
	pxor	xmm12,xmm11
DB	102,69,15,58,68,222,0
	movdqa	xmm1,xmm0
	pshufd	xmm8,xmm0,78
	pxor	xmm8,xmm0
DB	102,69,15,58,68,238,17
DB	102,68,15,58,68,231,0
	xorps	xmm3,xmm11
	xorps	xmm5,xmm13

	lea	r8,QWORD PTR[64+r8]
	sub	r9,040h
	jc	$L$tail4x

	jmp	$L$mod4_loop
ALIGN	32
$L$mod4_loop::
DB	102,65,15,58,68,199,0
	xorps	xmm4,xmm12
	movdqu	xmm11,XMMWORD PTR[48+r8]
DB	102,69,15,56,0,218
DB	102,65,15,58,68,207,17
	xorps	xmm0,xmm3
	movdqu	xmm3,XMMWORD PTR[32+r8]
	movdqa	xmm13,xmm11
DB	102,68,15,58,68,199,16
	pshufd	xmm12,xmm11,78
	xorps	xmm1,xmm5
	pxor	xmm12,xmm11
DB	102,65,15,56,0,218
	movups	xmm7,XMMWORD PTR[32+rdx]
	xorps	xmm8,xmm4
DB	102,68,15,58,68,218,0
	pshufd	xmm4,xmm3,78

	pxor	xmm8,xmm0
	movdqa	xmm5,xmm3
	pxor	xmm8,xmm1
	pxor	xmm4,xmm3
	movdqa	xmm9,xmm8
DB	102,68,15,58,68,234,17
	pslldq	xmm8,8
	psrldq	xmm9,8
	pxor	xmm0,xmm8
	movdqa	xmm8,XMMWORD PTR[$L$7_mask]
	pxor	xmm1,xmm9
DB	102,76,15,110,200

	pand	xmm8,xmm0
DB	102,69,15,56,0,200
	pxor	xmm9,xmm0
DB	102,68,15,58,68,231,0
	psllq	xmm9,57
	movdqa	xmm8,xmm9
	pslldq	xmm9,8
DB	102,15,58,68,222,0
	psrldq	xmm8,8
	pxor	xmm0,xmm9
	pxor	xmm1,xmm8
	movdqu	xmm8,XMMWORD PTR[r8]

	movdqa	xmm9,xmm0
	psrlq	xmm0,1
DB	102,15,58,68,238,17
	xorps	xmm3,xmm11
	movdqu	xmm11,XMMWORD PTR[16+r8]
DB	102,69,15,56,0,218
DB	102,15,58,68,231,16
	xorps	xmm5,xmm13
	movups	xmm7,XMMWORD PTR[80+rdx]
DB	102,69,15,56,0,194
	pxor	xmm1,xmm9
	pxor	xmm9,xmm0
	psrlq	xmm0,5

	movdqa	xmm13,xmm11
	pxor	xmm4,xmm12
	pshufd	xmm12,xmm11,78
	pxor	xmm0,xmm9
	pxor	xmm1,xmm8
	pxor	xmm12,xmm11
DB	102,69,15,58,68,222,0
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	movdqa	xmm1,xmm0
DB	102,69,15,58,68,238,17
	xorps	xmm3,xmm11
	pshufd	xmm8,xmm0,78
	pxor	xmm8,xmm0

DB	102,68,15,58,68,231,0
	xorps	xmm5,xmm13

	lea	r8,QWORD PTR[64+r8]
	sub	r9,040h
	jnc	$L$mod4_loop

$L$tail4x::
DB	102,65,15,58,68,199,0
DB	102,65,15,58,68,207,17
DB	102,68,15,58,68,199,16
	xorps	xmm4,xmm12
	xorps	xmm0,xmm3
	xorps	xmm1,xmm5
	pxor	xmm1,xmm0
	pxor	xmm8,xmm4

	pxor	xmm8,xmm1
	pxor	xmm1,xmm0

	movdqa	xmm9,xmm8
	psrldq	xmm8,8
	pslldq	xmm9,8
	pxor	xmm1,xmm8
	pxor	xmm0,xmm9

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	add	r9,040h
	jz	$L$done
	movdqu	xmm7,XMMWORD PTR[32+rdx]
	sub	r9,010h
	jz	$L$odd_tail
$L$skip4x::





	movdqu	xmm8,XMMWORD PTR[r8]
	movdqu	xmm3,XMMWORD PTR[16+r8]
DB	102,69,15,56,0,194
DB	102,65,15,56,0,218
	pxor	xmm0,xmm8

	movdqa	xmm5,xmm3
	pshufd	xmm4,xmm3,78
	pxor	xmm4,xmm3
DB	102,15,58,68,218,0
DB	102,15,58,68,234,17
DB	102,15,58,68,231,0

	lea	r8,QWORD PTR[32+r8]
	nop
	sub	r9,020h
	jbe	$L$even_tail
	nop
	jmp	$L$mod_loop

ALIGN	32
$L$mod_loop::
	movdqa	xmm1,xmm0
	movdqa	xmm8,xmm4
	pshufd	xmm4,xmm0,78
	pxor	xmm4,xmm0

DB	102,15,58,68,198,0
DB	102,15,58,68,206,17
DB	102,15,58,68,231,16

	pxor	xmm0,xmm3
	pxor	xmm1,xmm5
	movdqu	xmm9,XMMWORD PTR[r8]
	pxor	xmm8,xmm0
DB	102,69,15,56,0,202
	movdqu	xmm3,XMMWORD PTR[16+r8]

	pxor	xmm8,xmm1
	pxor	xmm1,xmm9
	pxor	xmm4,xmm8
DB	102,65,15,56,0,218
	movdqa	xmm8,xmm4
	psrldq	xmm8,8
	pslldq	xmm4,8
	pxor	xmm1,xmm8
	pxor	xmm0,xmm4

	movdqa	xmm5,xmm3

	movdqa	xmm9,xmm0
	movdqa	xmm8,xmm0
	psllq	xmm0,5
	pxor	xmm8,xmm0
DB	102,15,58,68,218,0
	psllq	xmm0,1
	pxor	xmm0,xmm8
	psllq	xmm0,57
	movdqa	xmm8,xmm0
	pslldq	xmm0,8
	psrldq	xmm8,8
	pxor	xmm0,xmm9
	pshufd	xmm4,xmm5,78
	pxor	xmm1,xmm8
	pxor	xmm4,xmm5

	movdqa	xmm9,xmm0
	psrlq	xmm0,1
DB	102,15,58,68,234,17
	pxor	xmm1,xmm9
	pxor	xmm9,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm9
	lea	r8,QWORD PTR[32+r8]
	psrlq	xmm0,1
DB	102,15,58,68,231,0
	pxor	xmm0,xmm1

	sub	r9,020h
	ja	$L$mod_loop

$L$even_tail::
	movdqa	xmm1,xmm0
	movdqa	xmm8,xmm4
	pshufd	xmm4,xmm0,78
	pxor	xmm4,xmm0

DB	102,15,58,68,198,0
DB	102,15,58,68,206,17
DB	102,15,58,68,231,16

	pxor	xmm0,xmm3
	pxor	xmm1,xmm5
	pxor	xmm8,xmm0
	pxor	xmm8,xmm1
	pxor	xmm4,xmm8
	movdqa	xmm8,xmm4
	psrldq	xmm8,8
	pslldq	xmm4,8
	pxor	xmm1,xmm8
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	test	r9,r9
	jnz	$L$done

$L$odd_tail::
	movdqu	xmm8,XMMWORD PTR[r8]
DB	102,69,15,56,0,194
	pxor	xmm0,xmm8
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,223,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4

	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3


	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
$L$done::
DB	102,65,15,56,0,194
	movdqu	XMMWORD PTR[rcx],xmm0
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_gcm_ghash_clmul::
	DB	0F3h,0C3h		;repret
gcm_ghash_clmul	ENDP
PUBLIC	gcm_init_avx

ALIGN	32
gcm_init_avx	PROC PUBLIC
$L$SEH_begin_gcm_init_avx::

DB	048h,083h,0ech,018h
DB	00fh,029h,034h,024h
	vzeroupper

	vmovdqu	xmm2,XMMWORD PTR[rdx]
	vpshufd	xmm2,xmm2,78


	vpshufd	xmm4,xmm2,255
	vpsrlq	xmm3,xmm2,63
	vpsllq	xmm2,xmm2,1
	vpxor	xmm5,xmm5,xmm5
	vpcmpgtd	xmm5,xmm5,xmm4
	vpslldq	xmm3,xmm3,8
	vpor	xmm2,xmm2,xmm3


	vpand	xmm5,xmm5,XMMWORD PTR[$L$0x1c2_polynomial]
	vpxor	xmm2,xmm2,xmm5

	vpunpckhqdq	xmm6,xmm2,xmm2
	vmovdqa	xmm0,xmm2
	vpxor	xmm6,xmm6,xmm2
	mov	r10,4
	jmp	$L$init_start_avx
ALIGN	32
$L$init_loop_avx::
	vpalignr	xmm5,xmm4,xmm3,8
	vmovdqu	XMMWORD PTR[(-16)+rcx],xmm5
	vpunpckhqdq	xmm3,xmm0,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm1,xmm0,xmm2,011h
	vpclmulqdq	xmm0,xmm0,xmm2,000h
	vpclmulqdq	xmm3,xmm3,xmm6,000h
	vpxor	xmm4,xmm1,xmm0
	vpxor	xmm3,xmm3,xmm4

	vpslldq	xmm4,xmm3,8
	vpsrldq	xmm3,xmm3,8
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm1,xmm1,xmm3
	vpsllq	xmm3,xmm0,57
	vpsllq	xmm4,xmm0,62
	vpxor	xmm4,xmm4,xmm3
	vpsllq	xmm3,xmm0,63
	vpxor	xmm4,xmm4,xmm3
	vpslldq	xmm3,xmm4,8
	vpsrldq	xmm4,xmm4,8
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm1,xmm1,xmm4

	vpsrlq	xmm4,xmm0,1
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm0,xmm0,xmm4
	vpsrlq	xmm4,xmm4,5
	vpxor	xmm0,xmm0,xmm4
	vpsrlq	xmm0,xmm0,1
	vpxor	xmm0,xmm0,xmm1
$L$init_start_avx::
	vmovdqa	xmm5,xmm0
	vpunpckhqdq	xmm3,xmm0,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm1,xmm0,xmm2,011h
	vpclmulqdq	xmm0,xmm0,xmm2,000h
	vpclmulqdq	xmm3,xmm3,xmm6,000h
	vpxor	xmm4,xmm1,xmm0
	vpxor	xmm3,xmm3,xmm4

	vpslldq	xmm4,xmm3,8
	vpsrldq	xmm3,xmm3,8
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm1,xmm1,xmm3
	vpsllq	xmm3,xmm0,57
	vpsllq	xmm4,xmm0,62
	vpxor	xmm4,xmm4,xmm3
	vpsllq	xmm3,xmm0,63
	vpxor	xmm4,xmm4,xmm3
	vpslldq	xmm3,xmm4,8
	vpsrldq	xmm4,xmm4,8
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm1,xmm1,xmm4

	vpsrlq	xmm4,xmm0,1
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm0,xmm0,xmm4
	vpsrlq	xmm4,xmm4,5
	vpxor	xmm0,xmm0,xmm4
	vpsrlq	xmm0,xmm0,1
	vpxor	xmm0,xmm0,xmm1
	vpshufd	xmm3,xmm5,78
	vpshufd	xmm4,xmm0,78
	vpxor	xmm3,xmm3,xmm5
	vmovdqu	XMMWORD PTR[rcx],xmm5
	vpxor	xmm4,xmm4,xmm0
	vmovdqu	XMMWORD PTR[16+rcx],xmm0
	lea	rcx,QWORD PTR[48+rcx]
	sub	r10,1
	jnz	$L$init_loop_avx

	vpalignr	xmm5,xmm3,xmm4,8
	vmovdqu	XMMWORD PTR[(-16)+rcx],xmm5

	vzeroupper
	movaps	xmm6,XMMWORD PTR[rsp]
	lea	rsp,QWORD PTR[24+rsp]
$L$SEH_end_gcm_init_avx::
	DB	0F3h,0C3h		;repret
gcm_init_avx	ENDP
PUBLIC	gcm_gmult_avx

ALIGN	32
gcm_gmult_avx	PROC PUBLIC
	jmp	$L$_gmult_clmul
gcm_gmult_avx	ENDP
PUBLIC	gcm_ghash_avx

ALIGN	32
gcm_ghash_avx	PROC PUBLIC
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_gcm_ghash_avx::

DB	048h,08dh,060h,0e0h
DB	00fh,029h,070h,0e0h
DB	00fh,029h,078h,0f0h
DB	044h,00fh,029h,000h
DB	044h,00fh,029h,048h,010h
DB	044h,00fh,029h,050h,020h
DB	044h,00fh,029h,058h,030h
DB	044h,00fh,029h,060h,040h
DB	044h,00fh,029h,068h,050h
DB	044h,00fh,029h,070h,060h
DB	044h,00fh,029h,078h,070h
	vzeroupper

	vmovdqu	xmm10,XMMWORD PTR[rcx]
	lea	r10,QWORD PTR[$L$0x1c2_polynomial]
	lea	rdx,QWORD PTR[64+rdx]
	vmovdqu	xmm13,XMMWORD PTR[$L$bswap_mask]
	vpshufb	xmm10,xmm10,xmm13
	cmp	r9,080h
	jb	$L$short_avx
	sub	r9,080h

	vmovdqu	xmm14,XMMWORD PTR[112+r8]
	vmovdqu	xmm6,XMMWORD PTR[((0-64))+rdx]
	vpshufb	xmm14,xmm14,xmm13
	vmovdqu	xmm7,XMMWORD PTR[((32-64))+rdx]

	vpunpckhqdq	xmm9,xmm14,xmm14
	vmovdqu	xmm15,XMMWORD PTR[96+r8]
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpxor	xmm9,xmm9,xmm14
	vpshufb	xmm15,xmm15,xmm13
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((16-64))+rdx]
	vpunpckhqdq	xmm8,xmm15,xmm15
	vmovdqu	xmm14,XMMWORD PTR[80+r8]
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vpxor	xmm8,xmm8,xmm15

	vpshufb	xmm14,xmm14,xmm13
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((48-64))+rdx]
	vpxor	xmm9,xmm9,xmm14
	vmovdqu	xmm15,XMMWORD PTR[64+r8]
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((80-64))+rdx]

	vpshufb	xmm15,xmm15,xmm13
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpxor	xmm4,xmm4,xmm1
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((64-64))+rdx]
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vpxor	xmm8,xmm8,xmm15

	vmovdqu	xmm14,XMMWORD PTR[48+r8]
	vpxor	xmm0,xmm0,xmm3
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpxor	xmm1,xmm1,xmm4
	vpshufb	xmm14,xmm14,xmm13
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((96-64))+rdx]
	vpxor	xmm2,xmm2,xmm5
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((128-64))+rdx]
	vpxor	xmm9,xmm9,xmm14

	vmovdqu	xmm15,XMMWORD PTR[32+r8]
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpxor	xmm4,xmm4,xmm1
	vpshufb	xmm15,xmm15,xmm13
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((112-64))+rdx]
	vpxor	xmm5,xmm5,xmm2
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vpxor	xmm8,xmm8,xmm15

	vmovdqu	xmm14,XMMWORD PTR[16+r8]
	vpxor	xmm0,xmm0,xmm3
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpxor	xmm1,xmm1,xmm4
	vpshufb	xmm14,xmm14,xmm13
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((144-64))+rdx]
	vpxor	xmm2,xmm2,xmm5
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((176-64))+rdx]
	vpxor	xmm9,xmm9,xmm14

	vmovdqu	xmm15,XMMWORD PTR[r8]
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpxor	xmm4,xmm4,xmm1
	vpshufb	xmm15,xmm15,xmm13
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((160-64))+rdx]
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm9,xmm7,010h

	lea	r8,QWORD PTR[128+r8]
	cmp	r9,080h
	jb	$L$tail_avx

	vpxor	xmm15,xmm15,xmm10
	sub	r9,080h
	jmp	$L$oop8x_avx

ALIGN	32
$L$oop8x_avx::
	vpunpckhqdq	xmm8,xmm15,xmm15
	vmovdqu	xmm14,XMMWORD PTR[112+r8]
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm8,xmm8,xmm15
	vpclmulqdq	xmm10,xmm15,xmm6,000h
	vpshufb	xmm14,xmm14,xmm13
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm11,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((0-64))+rdx]
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm12,xmm8,xmm7,000h
	vmovdqu	xmm7,XMMWORD PTR[((32-64))+rdx]
	vpxor	xmm9,xmm9,xmm14

	vmovdqu	xmm15,XMMWORD PTR[96+r8]
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpxor	xmm10,xmm10,xmm3
	vpshufb	xmm15,xmm15,xmm13
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vxorps	xmm11,xmm11,xmm4
	vmovdqu	xmm6,XMMWORD PTR[((16-64))+rdx]
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vpxor	xmm12,xmm12,xmm5
	vxorps	xmm8,xmm8,xmm15

	vmovdqu	xmm14,XMMWORD PTR[80+r8]
	vpxor	xmm12,xmm12,xmm10
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpxor	xmm12,xmm12,xmm11
	vpslldq	xmm9,xmm12,8
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vpsrldq	xmm12,xmm12,8
	vpxor	xmm10,xmm10,xmm9
	vmovdqu	xmm6,XMMWORD PTR[((48-64))+rdx]
	vpshufb	xmm14,xmm14,xmm13
	vxorps	xmm11,xmm11,xmm12
	vpxor	xmm4,xmm4,xmm1
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((80-64))+rdx]
	vpxor	xmm9,xmm9,xmm14
	vpxor	xmm5,xmm5,xmm2

	vmovdqu	xmm15,XMMWORD PTR[64+r8]
	vpalignr	xmm12,xmm10,xmm10,8
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpshufb	xmm15,xmm15,xmm13
	vpxor	xmm0,xmm0,xmm3
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((64-64))+rdx]
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vxorps	xmm8,xmm8,xmm15
	vpxor	xmm2,xmm2,xmm5

	vmovdqu	xmm14,XMMWORD PTR[48+r8]
	vpclmulqdq	xmm10,xmm10,XMMWORD PTR[r10],010h
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpshufb	xmm14,xmm14,xmm13
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((96-64))+rdx]
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((128-64))+rdx]
	vpxor	xmm9,xmm9,xmm14
	vpxor	xmm5,xmm5,xmm2

	vmovdqu	xmm15,XMMWORD PTR[32+r8]
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpshufb	xmm15,xmm15,xmm13
	vpxor	xmm0,xmm0,xmm3
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((112-64))+rdx]
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm2,xmm9,xmm7,000h
	vpxor	xmm8,xmm8,xmm15
	vpxor	xmm2,xmm2,xmm5
	vxorps	xmm10,xmm10,xmm12

	vmovdqu	xmm14,XMMWORD PTR[16+r8]
	vpalignr	xmm12,xmm10,xmm10,8
	vpclmulqdq	xmm3,xmm15,xmm6,000h
	vpshufb	xmm14,xmm14,xmm13
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm4,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((144-64))+rdx]
	vpclmulqdq	xmm10,xmm10,XMMWORD PTR[r10],010h
	vxorps	xmm12,xmm12,xmm11
	vpunpckhqdq	xmm9,xmm14,xmm14
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm5,xmm8,xmm7,010h
	vmovdqu	xmm7,XMMWORD PTR[((176-64))+rdx]
	vpxor	xmm9,xmm9,xmm14
	vpxor	xmm5,xmm5,xmm2

	vmovdqu	xmm15,XMMWORD PTR[r8]
	vpclmulqdq	xmm0,xmm14,xmm6,000h
	vpshufb	xmm15,xmm15,xmm13
	vpclmulqdq	xmm1,xmm14,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((160-64))+rdx]
	vpxor	xmm15,xmm15,xmm12
	vpclmulqdq	xmm2,xmm9,xmm7,010h
	vpxor	xmm15,xmm15,xmm10

	lea	r8,QWORD PTR[128+r8]
	sub	r9,080h
	jnc	$L$oop8x_avx

	add	r9,080h
	jmp	$L$tail_no_xor_avx

ALIGN	32
$L$short_avx::
	vmovdqu	xmm14,XMMWORD PTR[((-16))+r9*1+r8]
	lea	r8,QWORD PTR[r9*1+r8]
	vmovdqu	xmm6,XMMWORD PTR[((0-64))+rdx]
	vmovdqu	xmm7,XMMWORD PTR[((32-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13

	vmovdqa	xmm3,xmm0
	vmovdqa	xmm4,xmm1
	vmovdqa	xmm5,xmm2
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-32))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((16-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vpsrldq	xmm7,xmm7,8
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-48))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((48-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vmovdqu	xmm7,XMMWORD PTR[((80-64))+rdx]
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-64))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((64-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vpsrldq	xmm7,xmm7,8
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-80))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((96-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vmovdqu	xmm7,XMMWORD PTR[((128-64))+rdx]
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-96))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((112-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vpsrldq	xmm7,xmm7,8
	sub	r9,010h
	jz	$L$tail_avx

	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vmovdqu	xmm14,XMMWORD PTR[((-112))+r8]
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vmovdqu	xmm6,XMMWORD PTR[((144-64))+rdx]
	vpshufb	xmm15,xmm14,xmm13
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h
	vmovq	xmm7,QWORD PTR[((184-64))+rdx]
	sub	r9,010h
	jmp	$L$tail_avx

ALIGN	32
$L$tail_avx::
	vpxor	xmm15,xmm15,xmm10
$L$tail_no_xor_avx::
	vpunpckhqdq	xmm8,xmm15,xmm15
	vpxor	xmm3,xmm3,xmm0
	vpclmulqdq	xmm0,xmm15,xmm6,000h
	vpxor	xmm8,xmm8,xmm15
	vpxor	xmm4,xmm4,xmm1
	vpclmulqdq	xmm1,xmm15,xmm6,011h
	vpxor	xmm5,xmm5,xmm2
	vpclmulqdq	xmm2,xmm8,xmm7,000h

	vmovdqu	xmm12,XMMWORD PTR[r10]

	vpxor	xmm10,xmm3,xmm0
	vpxor	xmm11,xmm4,xmm1
	vpxor	xmm5,xmm5,xmm2

	vpxor	xmm5,xmm5,xmm10
	vpxor	xmm5,xmm5,xmm11
	vpslldq	xmm9,xmm5,8
	vpsrldq	xmm5,xmm5,8
	vpxor	xmm10,xmm10,xmm9
	vpxor	xmm11,xmm11,xmm5

	vpclmulqdq	xmm9,xmm10,xmm12,010h
	vpalignr	xmm10,xmm10,xmm10,8
	vpxor	xmm10,xmm10,xmm9

	vpclmulqdq	xmm9,xmm10,xmm12,010h
	vpalignr	xmm10,xmm10,xmm10,8
	vpxor	xmm10,xmm10,xmm11
	vpxor	xmm10,xmm10,xmm9

	cmp	r9,0
	jne	$L$short_avx

	vpshufb	xmm10,xmm10,xmm13
	vmovdqu	XMMWORD PTR[rcx],xmm10
	vzeroupper
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[168+rsp]
$L$SEH_end_gcm_ghash_avx::
	DB	0F3h,0C3h		;repret
gcm_ghash_avx	ENDP
ALIGN	64
$L$bswap_mask::
DB	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
$L$0x1c2_polynomial::
DB	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0c2h
$L$7_mask::
	DD	7,0,7,0
$L$7_mask_poly::
	DD	7,0,450,0
ALIGN	64

$L$rem_4bit::
	DD	0,0,0,471859200,0,943718400,0,610271232
	DD	0,1887436800,0,1822425088,0,1220542464,0,1423966208
	DD	0,3774873600,0,4246732800,0,3644850176,0,3311403008
	DD	0,2441084928,0,2376073216,0,2847932416,0,3051356160

$L$rem_8bit::
	DW	00000h,001C2h,00384h,00246h,00708h,006CAh,0048Ch,0054Eh
	DW	00E10h,00FD2h,00D94h,00C56h,00918h,008DAh,00A9Ch,00B5Eh
	DW	01C20h,01DE2h,01FA4h,01E66h,01B28h,01AEAh,018ACh,0196Eh
	DW	01230h,013F2h,011B4h,01076h,01538h,014FAh,016BCh,0177Eh
	DW	03840h,03982h,03BC4h,03A06h,03F48h,03E8Ah,03CCCh,03D0Eh
	DW	03650h,03792h,035D4h,03416h,03158h,0309Ah,032DCh,0331Eh
	DW	02460h,025A2h,027E4h,02626h,02368h,022AAh,020ECh,0212Eh
	DW	02A70h,02BB2h,029F4h,02836h,02D78h,02CBAh,02EFCh,02F3Eh
	DW	07080h,07142h,07304h,072C6h,07788h,0764Ah,0740Ch,075CEh
	DW	07E90h,07F52h,07D14h,07CD6h,07998h,0785Ah,07A1Ch,07BDEh
	DW	06CA0h,06D62h,06F24h,06EE6h,06BA8h,06A6Ah,0682Ch,069EEh
	DW	062B0h,06372h,06134h,060F6h,065B8h,0647Ah,0663Ch,067FEh
	DW	048C0h,04902h,04B44h,04A86h,04FC8h,04E0Ah,04C4Ch,04D8Eh
	DW	046D0h,04712h,04554h,04496h,041D8h,0401Ah,0425Ch,0439Eh
	DW	054E0h,05522h,05764h,056A6h,053E8h,0522Ah,0506Ch,051AEh
	DW	05AF0h,05B32h,05974h,058B6h,05DF8h,05C3Ah,05E7Ch,05FBEh
	DW	0E100h,0E0C2h,0E284h,0E346h,0E608h,0E7CAh,0E58Ch,0E44Eh
	DW	0EF10h,0EED2h,0EC94h,0ED56h,0E818h,0E9DAh,0EB9Ch,0EA5Eh
	DW	0FD20h,0FCE2h,0FEA4h,0FF66h,0FA28h,0FBEAh,0F9ACh,0F86Eh
	DW	0F330h,0F2F2h,0F0B4h,0F176h,0F438h,0F5FAh,0F7BCh,0F67Eh
	DW	0D940h,0D882h,0DAC4h,0DB06h,0DE48h,0DF8Ah,0DDCCh,0DC0Eh
	DW	0D750h,0D692h,0D4D4h,0D516h,0D058h,0D19Ah,0D3DCh,0D21Eh
	DW	0C560h,0C4A2h,0C6E4h,0C726h,0C268h,0C3AAh,0C1ECh,0C02Eh
	DW	0CB70h,0CAB2h,0C8F4h,0C936h,0CC78h,0CDBAh,0CFFCh,0CE3Eh
	DW	09180h,09042h,09204h,093C6h,09688h,0974Ah,0950Ch,094CEh
	DW	09F90h,09E52h,09C14h,09DD6h,09898h,0995Ah,09B1Ch,09ADEh
	DW	08DA0h,08C62h,08E24h,08FE6h,08AA8h,08B6Ah,0892Ch,088EEh
	DW	083B0h,08272h,08034h,081F6h,084B8h,0857Ah,0873Ch,086FEh
	DW	0A9C0h,0A802h,0AA44h,0AB86h,0AEC8h,0AF0Ah,0AD4Ch,0AC8Eh
	DW	0A7D0h,0A612h,0A454h,0A596h,0A0D8h,0A11Ah,0A35Ch,0A29Eh
	DW	0B5E0h,0B422h,0B664h,0B7A6h,0B2E8h,0B32Ah,0B16Ch,0B0AEh
	DW	0BBF0h,0BA32h,0B874h,0B9B6h,0BCF8h,0BD3Ah,0BF7Ch,0BEBEh

DB	71,72,65,83,72,32,102,111,114,32,120,56,54,95,54,52
DB	44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32
DB	60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111
DB	114,103,62,0
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
	DD	imagerel $L$SEH_begin_gcm_gmult_4bit
	DD	imagerel $L$SEH_end_gcm_gmult_4bit
	DD	imagerel $L$SEH_info_gcm_gmult_4bit

	DD	imagerel $L$SEH_begin_gcm_ghash_4bit
	DD	imagerel $L$SEH_end_gcm_ghash_4bit
	DD	imagerel $L$SEH_info_gcm_ghash_4bit

	DD	imagerel $L$SEH_begin_gcm_init_clmul
	DD	imagerel $L$SEH_end_gcm_init_clmul
	DD	imagerel $L$SEH_info_gcm_init_clmul

	DD	imagerel $L$SEH_begin_gcm_ghash_clmul
	DD	imagerel $L$SEH_end_gcm_ghash_clmul
	DD	imagerel $L$SEH_info_gcm_ghash_clmul
	DD	imagerel $L$SEH_begin_gcm_init_avx
	DD	imagerel $L$SEH_end_gcm_init_avx
	DD	imagerel $L$SEH_info_gcm_init_clmul

	DD	imagerel $L$SEH_begin_gcm_ghash_avx
	DD	imagerel $L$SEH_end_gcm_ghash_avx
	DD	imagerel $L$SEH_info_gcm_ghash_clmul
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_gcm_gmult_4bit::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$gmult_prologue,imagerel $L$gmult_epilogue
$L$SEH_info_gcm_ghash_4bit::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$ghash_prologue,imagerel $L$ghash_epilogue
$L$SEH_info_gcm_init_clmul::
DB	001h,008h,003h,000h
DB	008h,068h,000h,000h
DB	004h,022h,000h,000h
$L$SEH_info_gcm_ghash_clmul::
DB	001h,033h,016h,000h
DB	033h,0f8h,009h,000h
DB	02eh,0e8h,008h,000h
DB	029h,0d8h,007h,000h
DB	024h,0c8h,006h,000h
DB	01fh,0b8h,005h,000h
DB	01ah,0a8h,004h,000h
DB	015h,098h,003h,000h
DB	010h,088h,002h,000h
DB	00ch,078h,001h,000h
DB	008h,068h,000h,000h
DB	004h,001h,015h,000h

.xdata	ENDS
END
