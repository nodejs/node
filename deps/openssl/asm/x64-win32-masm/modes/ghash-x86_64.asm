OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

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


	movdqa	xmm0,xmm2
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm2
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

	movdqa	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,5
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm4,xmm0
	pslldq	xmm0,8
	psrldq	xmm4,8
	pxor	xmm0,xmm3
	pxor	xmm1,xmm4


	movdqa	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	movdqu	XMMWORD PTR[rcx],xmm2
	movdqu	XMMWORD PTR[16+rcx],xmm0
	DB	0F3h,0C3h		;repret
gcm_init_clmul	ENDP
PUBLIC	gcm_gmult_clmul

ALIGN	16
gcm_gmult_clmul	PROC PUBLIC
	movdqu	xmm0,XMMWORD PTR[rcx]
	movdqa	xmm5,XMMWORD PTR[$L$bswap_mask]
	movdqu	xmm2,XMMWORD PTR[rdx]
DB	102,15,56,0,197
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm2
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

	movdqa	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,5
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm4,xmm0
	pslldq	xmm0,8
	psrldq	xmm4,8
	pxor	xmm0,xmm3
	pxor	xmm1,xmm4


	movdqa	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	psrlq	xmm0,1
	pxor	xmm0,xmm4
DB	102,15,56,0,197
	movdqu	XMMWORD PTR[rcx],xmm0
	DB	0F3h,0C3h		;repret
gcm_gmult_clmul	ENDP
PUBLIC	gcm_ghash_clmul

ALIGN	16
gcm_ghash_clmul	PROC PUBLIC
$L$SEH_begin_gcm_ghash_clmul::

DB	048h,083h,0ech,058h

DB	00fh,029h,034h,024h

DB	00fh,029h,07ch,024h,010h

DB	044h,00fh,029h,044h,024h,020h

DB	044h,00fh,029h,04ch,024h,030h

DB	044h,00fh,029h,054h,024h,040h

	movdqa	xmm5,XMMWORD PTR[$L$bswap_mask]

	movdqu	xmm0,XMMWORD PTR[rcx]
	movdqu	xmm2,XMMWORD PTR[rdx]
DB	102,15,56,0,197

	sub	r9,010h
	jz	$L$odd_tail

	movdqu	xmm8,XMMWORD PTR[16+rdx]





	movdqu	xmm3,XMMWORD PTR[r8]
	movdqu	xmm6,XMMWORD PTR[16+r8]
DB	102,15,56,0,221
DB	102,15,56,0,245
	pxor	xmm0,xmm3
	movdqa	xmm7,xmm6
	pshufd	xmm3,xmm6,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm6
	pxor	xmm4,xmm2
DB	102,15,58,68,242,0
DB	102,15,58,68,250,17
DB	102,15,58,68,220,0
	pxor	xmm3,xmm6
	pxor	xmm3,xmm7

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm7,xmm3
	pxor	xmm6,xmm4
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm8,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm8

	lea	r8,QWORD PTR[32+r8]
	sub	r9,020h
	jbe	$L$even_tail

$L$mod_loop::
DB	102,65,15,58,68,192,0
DB	102,65,15,58,68,200,17
DB	102,15,58,68,220,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4
	movdqu	xmm3,XMMWORD PTR[r8]
	pxor	xmm0,xmm6
	pxor	xmm1,xmm7

	movdqu	xmm6,XMMWORD PTR[16+r8]
DB	102,15,56,0,221
DB	102,15,56,0,245

	movdqa	xmm7,xmm6
	pshufd	xmm9,xmm6,78
	pshufd	xmm10,xmm2,78
	pxor	xmm9,xmm6
	pxor	xmm10,xmm2
	pxor	xmm1,xmm3

	movdqa	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,5
	pxor	xmm0,xmm3
DB	102,15,58,68,242,0
	psllq	xmm0,57
	movdqa	xmm4,xmm0
	pslldq	xmm0,8
	psrldq	xmm4,8
	pxor	xmm0,xmm3
	pxor	xmm1,xmm4

DB	102,15,58,68,250,17
	movdqa	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	psrlq	xmm0,1
	pxor	xmm0,xmm4

DB	102,69,15,58,68,202,0
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm8,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm8

	pxor	xmm9,xmm6
	pxor	xmm9,xmm7
	movdqa	xmm10,xmm9
	psrldq	xmm9,8
	pslldq	xmm10,8
	pxor	xmm7,xmm9
	pxor	xmm6,xmm10

	lea	r8,QWORD PTR[32+r8]
	sub	r9,020h
	ja	$L$mod_loop

$L$even_tail::
DB	102,65,15,58,68,192,0
DB	102,65,15,58,68,200,17
DB	102,15,58,68,220,0
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1

	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4
	pxor	xmm0,xmm6
	pxor	xmm1,xmm7

	movdqa	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,5
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm4,xmm0
	pslldq	xmm0,8
	psrldq	xmm4,8
	pxor	xmm0,xmm3
	pxor	xmm1,xmm4


	movdqa	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	test	r9,r9
	jnz	$L$done

$L$odd_tail::
	movdqu	xmm3,XMMWORD PTR[r8]
DB	102,15,56,0,221
	pxor	xmm0,xmm3
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm2
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

	movdqa	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,5
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm4,xmm0
	pslldq	xmm0,8
	psrldq	xmm4,8
	pxor	xmm0,xmm3
	pxor	xmm1,xmm4


	movdqa	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	psrlq	xmm0,1
	pxor	xmm0,xmm4
$L$done::
DB	102,15,56,0,197
	movdqu	XMMWORD PTR[rcx],xmm0
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	add	rsp,058h
	DB	0F3h,0C3h		;repret
$L$SEH_end_gcm_ghash_clmul::
gcm_ghash_clmul	ENDP
ALIGN	64
$L$bswap_mask::
DB	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
$L$0x1c2_polynomial::
DB	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0c2h
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

	DD	imagerel $L$SEH_begin_gcm_ghash_clmul
	DD	imagerel $L$SEH_end_gcm_ghash_clmul
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

$L$SEH_info_gcm_ghash_clmul::
DB	001h,01fh,00bh,000h
DB	01fh,0a8h,004h,000h

DB	019h,098h,003h,000h

DB	013h,088h,002h,000h

DB	00dh,078h,001h,000h

DB	008h,068h,000h,000h

DB	004h,0a2h,000h,000h


.xdata	ENDS
END
