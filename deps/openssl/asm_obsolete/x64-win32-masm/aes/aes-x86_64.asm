OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

ALIGN	16
_x86_64_AES_encrypt	PROC PRIVATE
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]

	mov	r13d,DWORD PTR[240+r15]
	sub	r13d,1
	jmp	$L$enc_loop
ALIGN	16
$L$enc_loop::

	movzx	esi,al
	movzx	edi,bl
	movzx	ebp,cl
	mov	r10d,DWORD PTR[rsi*8+r14]
	mov	r11d,DWORD PTR[rdi*8+r14]
	mov	r12d,DWORD PTR[rbp*8+r14]

	movzx	esi,bh
	movzx	edi,ch
	movzx	ebp,dl
	xor	r10d,DWORD PTR[3+rsi*8+r14]
	xor	r11d,DWORD PTR[3+rdi*8+r14]
	mov	r8d,DWORD PTR[rbp*8+r14]

	movzx	esi,dh
	shr	ecx,16
	movzx	ebp,ah
	xor	r12d,DWORD PTR[3+rsi*8+r14]
	shr	edx,16
	xor	r8d,DWORD PTR[3+rbp*8+r14]

	shr	ebx,16
	lea	r15,QWORD PTR[16+r15]
	shr	eax,16

	movzx	esi,cl
	movzx	edi,dl
	movzx	ebp,al
	xor	r10d,DWORD PTR[2+rsi*8+r14]
	xor	r11d,DWORD PTR[2+rdi*8+r14]
	xor	r12d,DWORD PTR[2+rbp*8+r14]

	movzx	esi,dh
	movzx	edi,ah
	movzx	ebp,bl
	xor	r10d,DWORD PTR[1+rsi*8+r14]
	xor	r11d,DWORD PTR[1+rdi*8+r14]
	xor	r8d,DWORD PTR[2+rbp*8+r14]

	mov	edx,DWORD PTR[12+r15]
	movzx	edi,bh
	movzx	ebp,ch
	mov	eax,DWORD PTR[r15]
	xor	r12d,DWORD PTR[1+rdi*8+r14]
	xor	r8d,DWORD PTR[1+rbp*8+r14]

	mov	ebx,DWORD PTR[4+r15]
	mov	ecx,DWORD PTR[8+r15]
	xor	eax,r10d
	xor	ebx,r11d
	xor	ecx,r12d
	xor	edx,r8d
	sub	r13d,1
	jnz	$L$enc_loop
	movzx	esi,al
	movzx	edi,bl
	movzx	ebp,cl
	movzx	r10d,BYTE PTR[2+rsi*8+r14]
	movzx	r11d,BYTE PTR[2+rdi*8+r14]
	movzx	r12d,BYTE PTR[2+rbp*8+r14]

	movzx	esi,dl
	movzx	edi,bh
	movzx	ebp,ch
	movzx	r8d,BYTE PTR[2+rsi*8+r14]
	mov	edi,DWORD PTR[rdi*8+r14]
	mov	ebp,DWORD PTR[rbp*8+r14]

	and	edi,00000ff00h
	and	ebp,00000ff00h

	xor	r10d,edi
	xor	r11d,ebp
	shr	ecx,16

	movzx	esi,dh
	movzx	edi,ah
	shr	edx,16
	mov	esi,DWORD PTR[rsi*8+r14]
	mov	edi,DWORD PTR[rdi*8+r14]

	and	esi,00000ff00h
	and	edi,00000ff00h
	shr	ebx,16
	xor	r12d,esi
	xor	r8d,edi
	shr	eax,16

	movzx	esi,cl
	movzx	edi,dl
	movzx	ebp,al
	mov	esi,DWORD PTR[rsi*8+r14]
	mov	edi,DWORD PTR[rdi*8+r14]
	mov	ebp,DWORD PTR[rbp*8+r14]

	and	esi,000ff0000h
	and	edi,000ff0000h
	and	ebp,000ff0000h

	xor	r10d,esi
	xor	r11d,edi
	xor	r12d,ebp

	movzx	esi,bl
	movzx	edi,dh
	movzx	ebp,ah
	mov	esi,DWORD PTR[rsi*8+r14]
	mov	edi,DWORD PTR[2+rdi*8+r14]
	mov	ebp,DWORD PTR[2+rbp*8+r14]

	and	esi,000ff0000h
	and	edi,0ff000000h
	and	ebp,0ff000000h

	xor	r8d,esi
	xor	r10d,edi
	xor	r11d,ebp

	movzx	esi,bh
	movzx	edi,ch
	mov	edx,DWORD PTR[((16+12))+r15]
	mov	esi,DWORD PTR[2+rsi*8+r14]
	mov	edi,DWORD PTR[2+rdi*8+r14]
	mov	eax,DWORD PTR[((16+0))+r15]

	and	esi,0ff000000h
	and	edi,0ff000000h

	xor	r12d,esi
	xor	r8d,edi

	mov	ebx,DWORD PTR[((16+4))+r15]
	mov	ecx,DWORD PTR[((16+8))+r15]
	xor	eax,r10d
	xor	ebx,r11d
	xor	ecx,r12d
	xor	edx,r8d
DB	0f3h,0c3h
_x86_64_AES_encrypt	ENDP

ALIGN	16
_x86_64_AES_encrypt_compact	PROC PRIVATE
	lea	r8,QWORD PTR[128+r14]
	mov	edi,DWORD PTR[((0-128))+r8]
	mov	ebp,DWORD PTR[((32-128))+r8]
	mov	r10d,DWORD PTR[((64-128))+r8]
	mov	r11d,DWORD PTR[((96-128))+r8]
	mov	edi,DWORD PTR[((128-128))+r8]
	mov	ebp,DWORD PTR[((160-128))+r8]
	mov	r10d,DWORD PTR[((192-128))+r8]
	mov	r11d,DWORD PTR[((224-128))+r8]
	jmp	$L$enc_loop_compact
ALIGN	16
$L$enc_loop_compact::
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]
	lea	r15,QWORD PTR[16+r15]
	movzx	r10d,al
	movzx	r11d,bl
	movzx	r12d,cl
	movzx	r8d,dl
	movzx	esi,bh
	movzx	edi,ch
	shr	ecx,16
	movzx	ebp,dh
	movzx	r10d,BYTE PTR[r10*1+r14]
	movzx	r11d,BYTE PTR[r11*1+r14]
	movzx	r12d,BYTE PTR[r12*1+r14]
	movzx	r8d,BYTE PTR[r8*1+r14]

	movzx	r9d,BYTE PTR[rsi*1+r14]
	movzx	esi,ah
	movzx	r13d,BYTE PTR[rdi*1+r14]
	movzx	edi,cl
	movzx	ebp,BYTE PTR[rbp*1+r14]
	movzx	esi,BYTE PTR[rsi*1+r14]

	shl	r9d,8
	shr	edx,16
	shl	r13d,8
	xor	r10d,r9d
	shr	eax,16
	movzx	r9d,dl
	shr	ebx,16
	xor	r11d,r13d
	shl	ebp,8
	movzx	r13d,al
	movzx	edi,BYTE PTR[rdi*1+r14]
	xor	r12d,ebp

	shl	esi,8
	movzx	ebp,bl
	shl	edi,16
	xor	r8d,esi
	movzx	r9d,BYTE PTR[r9*1+r14]
	movzx	esi,dh
	movzx	r13d,BYTE PTR[r13*1+r14]
	xor	r10d,edi

	shr	ecx,8
	movzx	edi,ah
	shl	r9d,16
	shr	ebx,8
	shl	r13d,16
	xor	r11d,r9d
	movzx	ebp,BYTE PTR[rbp*1+r14]
	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]
	movzx	edx,BYTE PTR[rcx*1+r14]
	movzx	ecx,BYTE PTR[rbx*1+r14]

	shl	ebp,16
	xor	r12d,r13d
	shl	esi,24
	xor	r8d,ebp
	shl	edi,24
	xor	r10d,esi
	shl	edx,24
	xor	r11d,edi
	shl	ecx,24
	mov	eax,r10d
	mov	ebx,r11d
	xor	ecx,r12d
	xor	edx,r8d
	cmp	r15,QWORD PTR[16+rsp]
	je	$L$enc_compact_done
	mov	r10d,080808080h
	mov	r11d,080808080h
	and	r10d,eax
	and	r11d,ebx
	mov	esi,r10d
	mov	edi,r11d
	shr	r10d,7
	lea	r8d,DWORD PTR[rax*1+rax]
	shr	r11d,7
	lea	r9d,DWORD PTR[rbx*1+rbx]
	sub	esi,r10d
	sub	edi,r11d
	and	r8d,0fefefefeh
	and	r9d,0fefefefeh
	and	esi,01b1b1b1bh
	and	edi,01b1b1b1bh
	mov	r10d,eax
	mov	r11d,ebx
	xor	r8d,esi
	xor	r9d,edi

	xor	eax,r8d
	xor	ebx,r9d
	mov	r12d,080808080h
	rol	eax,24
	mov	ebp,080808080h
	rol	ebx,24
	and	r12d,ecx
	and	ebp,edx
	xor	eax,r8d
	xor	ebx,r9d
	mov	esi,r12d
	ror	r10d,16
	mov	edi,ebp
	ror	r11d,16
	lea	r8d,DWORD PTR[rcx*1+rcx]
	shr	r12d,7
	xor	eax,r10d
	shr	ebp,7
	xor	ebx,r11d
	ror	r10d,8
	lea	r9d,DWORD PTR[rdx*1+rdx]
	ror	r11d,8
	sub	esi,r12d
	sub	edi,ebp
	xor	eax,r10d
	xor	ebx,r11d

	and	r8d,0fefefefeh
	and	r9d,0fefefefeh
	and	esi,01b1b1b1bh
	and	edi,01b1b1b1bh
	mov	r12d,ecx
	mov	ebp,edx
	xor	r8d,esi
	xor	r9d,edi

	ror	r12d,16
	xor	ecx,r8d
	ror	ebp,16
	xor	edx,r9d
	rol	ecx,24
	mov	esi,DWORD PTR[r14]
	rol	edx,24
	xor	ecx,r8d
	mov	edi,DWORD PTR[64+r14]
	xor	edx,r9d
	mov	r8d,DWORD PTR[128+r14]
	xor	ecx,r12d
	ror	r12d,8
	xor	edx,ebp
	ror	ebp,8
	xor	ecx,r12d
	mov	r9d,DWORD PTR[192+r14]
	xor	edx,ebp
	jmp	$L$enc_loop_compact
ALIGN	16
$L$enc_compact_done::
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]
DB	0f3h,0c3h
_x86_64_AES_encrypt_compact	ENDP
PUBLIC	AES_encrypt

ALIGN	16
PUBLIC	asm_AES_encrypt

asm_AES_encrypt::
AES_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_AES_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15


	mov	r10,rsp
	lea	rcx,QWORD PTR[((-63))+rdx]
	and	rsp,-64
	sub	rcx,rsp
	neg	rcx
	and	rcx,03c0h
	sub	rsp,rcx
	sub	rsp,32

	mov	QWORD PTR[16+rsp],rsi
	mov	QWORD PTR[24+rsp],r10
$L$enc_prologue::

	mov	r15,rdx
	mov	r13d,DWORD PTR[240+r15]

	mov	eax,DWORD PTR[rdi]
	mov	ebx,DWORD PTR[4+rdi]
	mov	ecx,DWORD PTR[8+rdi]
	mov	edx,DWORD PTR[12+rdi]

	shl	r13d,4
	lea	rbp,QWORD PTR[r13*1+r15]
	mov	QWORD PTR[rsp],r15
	mov	QWORD PTR[8+rsp],rbp


	lea	r14,QWORD PTR[(($L$AES_Te+2048))]
	lea	rbp,QWORD PTR[768+rsp]
	sub	rbp,r14
	and	rbp,0300h
	lea	r14,QWORD PTR[rbp*1+r14]

	call	_x86_64_AES_encrypt_compact

	mov	r9,QWORD PTR[16+rsp]
	mov	rsi,QWORD PTR[24+rsp]
	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$enc_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_AES_encrypt::
AES_encrypt	ENDP

ALIGN	16
_x86_64_AES_decrypt	PROC PRIVATE
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]

	mov	r13d,DWORD PTR[240+r15]
	sub	r13d,1
	jmp	$L$dec_loop
ALIGN	16
$L$dec_loop::

	movzx	esi,al
	movzx	edi,bl
	movzx	ebp,cl
	mov	r10d,DWORD PTR[rsi*8+r14]
	mov	r11d,DWORD PTR[rdi*8+r14]
	mov	r12d,DWORD PTR[rbp*8+r14]

	movzx	esi,dh
	movzx	edi,ah
	movzx	ebp,dl
	xor	r10d,DWORD PTR[3+rsi*8+r14]
	xor	r11d,DWORD PTR[3+rdi*8+r14]
	mov	r8d,DWORD PTR[rbp*8+r14]

	movzx	esi,bh
	shr	eax,16
	movzx	ebp,ch
	xor	r12d,DWORD PTR[3+rsi*8+r14]
	shr	edx,16
	xor	r8d,DWORD PTR[3+rbp*8+r14]

	shr	ebx,16
	lea	r15,QWORD PTR[16+r15]
	shr	ecx,16

	movzx	esi,cl
	movzx	edi,dl
	movzx	ebp,al
	xor	r10d,DWORD PTR[2+rsi*8+r14]
	xor	r11d,DWORD PTR[2+rdi*8+r14]
	xor	r12d,DWORD PTR[2+rbp*8+r14]

	movzx	esi,bh
	movzx	edi,ch
	movzx	ebp,bl
	xor	r10d,DWORD PTR[1+rsi*8+r14]
	xor	r11d,DWORD PTR[1+rdi*8+r14]
	xor	r8d,DWORD PTR[2+rbp*8+r14]

	movzx	esi,dh
	mov	edx,DWORD PTR[12+r15]
	movzx	ebp,ah
	xor	r12d,DWORD PTR[1+rsi*8+r14]
	mov	eax,DWORD PTR[r15]
	xor	r8d,DWORD PTR[1+rbp*8+r14]

	xor	eax,r10d
	mov	ebx,DWORD PTR[4+r15]
	mov	ecx,DWORD PTR[8+r15]
	xor	ecx,r12d
	xor	ebx,r11d
	xor	edx,r8d
	sub	r13d,1
	jnz	$L$dec_loop
	lea	r14,QWORD PTR[2048+r14]
	movzx	esi,al
	movzx	edi,bl
	movzx	ebp,cl
	movzx	r10d,BYTE PTR[rsi*1+r14]
	movzx	r11d,BYTE PTR[rdi*1+r14]
	movzx	r12d,BYTE PTR[rbp*1+r14]

	movzx	esi,dl
	movzx	edi,dh
	movzx	ebp,ah
	movzx	r8d,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]
	movzx	ebp,BYTE PTR[rbp*1+r14]

	shl	edi,8
	shl	ebp,8

	xor	r10d,edi
	xor	r11d,ebp
	shr	edx,16

	movzx	esi,bh
	movzx	edi,ch
	shr	eax,16
	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]

	shl	esi,8
	shl	edi,8
	shr	ebx,16
	xor	r12d,esi
	xor	r8d,edi
	shr	ecx,16

	movzx	esi,cl
	movzx	edi,dl
	movzx	ebp,al
	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]
	movzx	ebp,BYTE PTR[rbp*1+r14]

	shl	esi,16
	shl	edi,16
	shl	ebp,16

	xor	r10d,esi
	xor	r11d,edi
	xor	r12d,ebp

	movzx	esi,bl
	movzx	edi,bh
	movzx	ebp,ch
	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]
	movzx	ebp,BYTE PTR[rbp*1+r14]

	shl	esi,16
	shl	edi,24
	shl	ebp,24

	xor	r8d,esi
	xor	r10d,edi
	xor	r11d,ebp

	movzx	esi,dh
	movzx	edi,ah
	mov	edx,DWORD PTR[((16+12))+r15]
	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	edi,BYTE PTR[rdi*1+r14]
	mov	eax,DWORD PTR[((16+0))+r15]

	shl	esi,24
	shl	edi,24

	xor	r12d,esi
	xor	r8d,edi

	mov	ebx,DWORD PTR[((16+4))+r15]
	mov	ecx,DWORD PTR[((16+8))+r15]
	lea	r14,QWORD PTR[((-2048))+r14]
	xor	eax,r10d
	xor	ebx,r11d
	xor	ecx,r12d
	xor	edx,r8d
DB	0f3h,0c3h
_x86_64_AES_decrypt	ENDP

ALIGN	16
_x86_64_AES_decrypt_compact	PROC PRIVATE
	lea	r8,QWORD PTR[128+r14]
	mov	edi,DWORD PTR[((0-128))+r8]
	mov	ebp,DWORD PTR[((32-128))+r8]
	mov	r10d,DWORD PTR[((64-128))+r8]
	mov	r11d,DWORD PTR[((96-128))+r8]
	mov	edi,DWORD PTR[((128-128))+r8]
	mov	ebp,DWORD PTR[((160-128))+r8]
	mov	r10d,DWORD PTR[((192-128))+r8]
	mov	r11d,DWORD PTR[((224-128))+r8]
	jmp	$L$dec_loop_compact

ALIGN	16
$L$dec_loop_compact::
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]
	lea	r15,QWORD PTR[16+r15]
	movzx	r10d,al
	movzx	r11d,bl
	movzx	r12d,cl
	movzx	r8d,dl
	movzx	esi,dh
	movzx	edi,ah
	shr	edx,16
	movzx	ebp,bh
	movzx	r10d,BYTE PTR[r10*1+r14]
	movzx	r11d,BYTE PTR[r11*1+r14]
	movzx	r12d,BYTE PTR[r12*1+r14]
	movzx	r8d,BYTE PTR[r8*1+r14]

	movzx	r9d,BYTE PTR[rsi*1+r14]
	movzx	esi,ch
	movzx	r13d,BYTE PTR[rdi*1+r14]
	movzx	ebp,BYTE PTR[rbp*1+r14]
	movzx	esi,BYTE PTR[rsi*1+r14]

	shr	ecx,16
	shl	r13d,8
	shl	r9d,8
	movzx	edi,cl
	shr	eax,16
	xor	r10d,r9d
	shr	ebx,16
	movzx	r9d,dl

	shl	ebp,8
	xor	r11d,r13d
	shl	esi,8
	movzx	r13d,al
	movzx	edi,BYTE PTR[rdi*1+r14]
	xor	r12d,ebp
	movzx	ebp,bl

	shl	edi,16
	xor	r8d,esi
	movzx	r9d,BYTE PTR[r9*1+r14]
	movzx	esi,bh
	movzx	ebp,BYTE PTR[rbp*1+r14]
	xor	r10d,edi
	movzx	r13d,BYTE PTR[r13*1+r14]
	movzx	edi,ch

	shl	ebp,16
	shl	r9d,16
	shl	r13d,16
	xor	r8d,ebp
	movzx	ebp,dh
	xor	r11d,r9d
	shr	eax,8
	xor	r12d,r13d

	movzx	esi,BYTE PTR[rsi*1+r14]
	movzx	ebx,BYTE PTR[rdi*1+r14]
	movzx	ecx,BYTE PTR[rbp*1+r14]
	movzx	edx,BYTE PTR[rax*1+r14]

	mov	eax,r10d
	shl	esi,24
	shl	ebx,24
	shl	ecx,24
	xor	eax,esi
	shl	edx,24
	xor	ebx,r11d
	xor	ecx,r12d
	xor	edx,r8d
	cmp	r15,QWORD PTR[16+rsp]
	je	$L$dec_compact_done

	mov	rsi,QWORD PTR[((256+0))+r14]
	shl	rbx,32
	shl	rdx,32
	mov	rdi,QWORD PTR[((256+8))+r14]
	or	rax,rbx
	or	rcx,rdx
	mov	rbp,QWORD PTR[((256+16))+r14]
	mov	r9,rsi
	mov	r12,rsi
	and	r9,rax
	and	r12,rcx
	mov	rbx,r9
	mov	rdx,r12
	shr	r9,7
	lea	r8,QWORD PTR[rax*1+rax]
	shr	r12,7
	lea	r11,QWORD PTR[rcx*1+rcx]
	sub	rbx,r9
	sub	rdx,r12
	and	r8,rdi
	and	r11,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r8,rbx
	xor	r11,rdx
	mov	r10,rsi
	mov	r13,rsi

	and	r10,r8
	and	r13,r11
	mov	rbx,r10
	mov	rdx,r13
	shr	r10,7
	lea	r9,QWORD PTR[r8*1+r8]
	shr	r13,7
	lea	r12,QWORD PTR[r11*1+r11]
	sub	rbx,r10
	sub	rdx,r13
	and	r9,rdi
	and	r12,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r9,rbx
	xor	r12,rdx
	mov	r10,rsi
	mov	r13,rsi

	and	r10,r9
	and	r13,r12
	mov	rbx,r10
	mov	rdx,r13
	shr	r10,7
	xor	r8,rax
	shr	r13,7
	xor	r11,rcx
	sub	rbx,r10
	sub	rdx,r13
	lea	r10,QWORD PTR[r9*1+r9]
	lea	r13,QWORD PTR[r12*1+r12]
	xor	r9,rax
	xor	r12,rcx
	and	r10,rdi
	and	r13,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r10,rbx
	xor	r13,rdx

	xor	rax,r10
	xor	rcx,r13
	xor	r8,r10
	xor	r11,r13
	mov	rbx,rax
	mov	rdx,rcx
	xor	r9,r10
	shr	rbx,32
	xor	r12,r13
	shr	rdx,32
	xor	r10,r8
	rol	eax,8
	xor	r13,r11
	rol	ecx,8
	xor	r10,r9
	rol	ebx,8
	xor	r13,r12

	rol	edx,8
	xor	eax,r10d
	shr	r10,32
	xor	ecx,r13d
	shr	r13,32
	xor	ebx,r10d
	xor	edx,r13d

	mov	r10,r8
	rol	r8d,24
	mov	r13,r11
	rol	r11d,24
	shr	r10,32
	xor	eax,r8d
	shr	r13,32
	xor	ecx,r11d
	rol	r10d,24
	mov	r8,r9
	rol	r13d,24
	mov	r11,r12
	shr	r8,32
	xor	ebx,r10d
	shr	r11,32
	xor	edx,r13d

	mov	rsi,QWORD PTR[r14]
	rol	r9d,16
	mov	rdi,QWORD PTR[64+r14]
	rol	r12d,16
	mov	rbp,QWORD PTR[128+r14]
	rol	r8d,16
	mov	r10,QWORD PTR[192+r14]
	xor	eax,r9d
	rol	r11d,16
	xor	ecx,r12d
	mov	r13,QWORD PTR[256+r14]
	xor	ebx,r8d
	xor	edx,r11d
	jmp	$L$dec_loop_compact
ALIGN	16
$L$dec_compact_done::
	xor	eax,DWORD PTR[r15]
	xor	ebx,DWORD PTR[4+r15]
	xor	ecx,DWORD PTR[8+r15]
	xor	edx,DWORD PTR[12+r15]
DB	0f3h,0c3h
_x86_64_AES_decrypt_compact	ENDP
PUBLIC	AES_decrypt

ALIGN	16
PUBLIC	asm_AES_decrypt

asm_AES_decrypt::
AES_decrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_AES_decrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15


	mov	r10,rsp
	lea	rcx,QWORD PTR[((-63))+rdx]
	and	rsp,-64
	sub	rcx,rsp
	neg	rcx
	and	rcx,03c0h
	sub	rsp,rcx
	sub	rsp,32

	mov	QWORD PTR[16+rsp],rsi
	mov	QWORD PTR[24+rsp],r10
$L$dec_prologue::

	mov	r15,rdx
	mov	r13d,DWORD PTR[240+r15]

	mov	eax,DWORD PTR[rdi]
	mov	ebx,DWORD PTR[4+rdi]
	mov	ecx,DWORD PTR[8+rdi]
	mov	edx,DWORD PTR[12+rdi]

	shl	r13d,4
	lea	rbp,QWORD PTR[r13*1+r15]
	mov	QWORD PTR[rsp],r15
	mov	QWORD PTR[8+rsp],rbp


	lea	r14,QWORD PTR[(($L$AES_Td+2048))]
	lea	rbp,QWORD PTR[768+rsp]
	sub	rbp,r14
	and	rbp,0300h
	lea	r14,QWORD PTR[rbp*1+r14]
	shr	rbp,3
	add	r14,rbp

	call	_x86_64_AES_decrypt_compact

	mov	r9,QWORD PTR[16+rsp]
	mov	rsi,QWORD PTR[24+rsp]
	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$dec_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_AES_decrypt::
AES_decrypt	ENDP
PUBLIC	private_AES_set_encrypt_key

ALIGN	16
private_AES_set_encrypt_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_private_AES_set_encrypt_key::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,8
$L$enc_key_prologue::

	call	_x86_64_AES_set_encrypt_key

	mov	rbp,QWORD PTR[40+rsp]
	mov	rbx,QWORD PTR[48+rsp]
	add	rsp,56
$L$enc_key_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_private_AES_set_encrypt_key::
private_AES_set_encrypt_key	ENDP


ALIGN	16
_x86_64_AES_set_encrypt_key	PROC PRIVATE
	mov	ecx,esi
	mov	rsi,rdi
	mov	rdi,rdx

	test	rsi,-1
	jz	$L$badpointer
	test	rdi,-1
	jz	$L$badpointer

	lea	rbp,QWORD PTR[$L$AES_Te]
	lea	rbp,QWORD PTR[((2048+128))+rbp]


	mov	eax,DWORD PTR[((0-128))+rbp]
	mov	ebx,DWORD PTR[((32-128))+rbp]
	mov	r8d,DWORD PTR[((64-128))+rbp]
	mov	edx,DWORD PTR[((96-128))+rbp]
	mov	eax,DWORD PTR[((128-128))+rbp]
	mov	ebx,DWORD PTR[((160-128))+rbp]
	mov	r8d,DWORD PTR[((192-128))+rbp]
	mov	edx,DWORD PTR[((224-128))+rbp]

	cmp	ecx,128
	je	$L$10rounds
	cmp	ecx,192
	je	$L$12rounds
	cmp	ecx,256
	je	$L$14rounds
	mov	rax,-2
	jmp	$L$exit

$L$10rounds::
	mov	rax,QWORD PTR[rsi]
	mov	rdx,QWORD PTR[8+rsi]
	mov	QWORD PTR[rdi],rax
	mov	QWORD PTR[8+rdi],rdx

	shr	rdx,32
	xor	ecx,ecx
	jmp	$L$10shortcut
ALIGN	4
$L$10loop::
	mov	eax,DWORD PTR[rdi]
	mov	edx,DWORD PTR[12+rdi]
$L$10shortcut::
	movzx	esi,dl
	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shl	ebx,16
	xor	eax,ebx

	xor	eax,DWORD PTR[((1024-128))+rcx*4+rbp]
	mov	DWORD PTR[16+rdi],eax
	xor	eax,DWORD PTR[4+rdi]
	mov	DWORD PTR[20+rdi],eax
	xor	eax,DWORD PTR[8+rdi]
	mov	DWORD PTR[24+rdi],eax
	xor	eax,DWORD PTR[12+rdi]
	mov	DWORD PTR[28+rdi],eax
	add	ecx,1
	lea	rdi,QWORD PTR[16+rdi]
	cmp	ecx,10
	jl	$L$10loop

	mov	DWORD PTR[80+rdi],10
	xor	rax,rax
	jmp	$L$exit

$L$12rounds::
	mov	rax,QWORD PTR[rsi]
	mov	rbx,QWORD PTR[8+rsi]
	mov	rdx,QWORD PTR[16+rsi]
	mov	QWORD PTR[rdi],rax
	mov	QWORD PTR[8+rdi],rbx
	mov	QWORD PTR[16+rdi],rdx

	shr	rdx,32
	xor	ecx,ecx
	jmp	$L$12shortcut
ALIGN	4
$L$12loop::
	mov	eax,DWORD PTR[rdi]
	mov	edx,DWORD PTR[20+rdi]
$L$12shortcut::
	movzx	esi,dl
	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shl	ebx,16
	xor	eax,ebx

	xor	eax,DWORD PTR[((1024-128))+rcx*4+rbp]
	mov	DWORD PTR[24+rdi],eax
	xor	eax,DWORD PTR[4+rdi]
	mov	DWORD PTR[28+rdi],eax
	xor	eax,DWORD PTR[8+rdi]
	mov	DWORD PTR[32+rdi],eax
	xor	eax,DWORD PTR[12+rdi]
	mov	DWORD PTR[36+rdi],eax

	cmp	ecx,7
	je	$L$12break
	add	ecx,1

	xor	eax,DWORD PTR[16+rdi]
	mov	DWORD PTR[40+rdi],eax
	xor	eax,DWORD PTR[20+rdi]
	mov	DWORD PTR[44+rdi],eax

	lea	rdi,QWORD PTR[24+rdi]
	jmp	$L$12loop
$L$12break::
	mov	DWORD PTR[72+rdi],12
	xor	rax,rax
	jmp	$L$exit

$L$14rounds::
	mov	rax,QWORD PTR[rsi]
	mov	rbx,QWORD PTR[8+rsi]
	mov	rcx,QWORD PTR[16+rsi]
	mov	rdx,QWORD PTR[24+rsi]
	mov	QWORD PTR[rdi],rax
	mov	QWORD PTR[8+rdi],rbx
	mov	QWORD PTR[16+rdi],rcx
	mov	QWORD PTR[24+rdi],rdx

	shr	rdx,32
	xor	ecx,ecx
	jmp	$L$14shortcut
ALIGN	4
$L$14loop::
	mov	eax,DWORD PTR[rdi]
	mov	edx,DWORD PTR[28+rdi]
$L$14shortcut::
	movzx	esi,dl
	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shl	ebx,16
	xor	eax,ebx

	xor	eax,DWORD PTR[((1024-128))+rcx*4+rbp]
	mov	DWORD PTR[32+rdi],eax
	xor	eax,DWORD PTR[4+rdi]
	mov	DWORD PTR[36+rdi],eax
	xor	eax,DWORD PTR[8+rdi]
	mov	DWORD PTR[40+rdi],eax
	xor	eax,DWORD PTR[12+rdi]
	mov	DWORD PTR[44+rdi],eax

	cmp	ecx,6
	je	$L$14break
	add	ecx,1

	mov	edx,eax
	mov	eax,DWORD PTR[16+rdi]
	movzx	esi,dl
	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shr	edx,16
	shl	ebx,8
	movzx	esi,dl
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	movzx	esi,dh
	shl	ebx,16
	xor	eax,ebx

	movzx	ebx,BYTE PTR[((-128))+rsi*1+rbp]
	shl	ebx,24
	xor	eax,ebx

	mov	DWORD PTR[48+rdi],eax
	xor	eax,DWORD PTR[20+rdi]
	mov	DWORD PTR[52+rdi],eax
	xor	eax,DWORD PTR[24+rdi]
	mov	DWORD PTR[56+rdi],eax
	xor	eax,DWORD PTR[28+rdi]
	mov	DWORD PTR[60+rdi],eax

	lea	rdi,QWORD PTR[32+rdi]
	jmp	$L$14loop
$L$14break::
	mov	DWORD PTR[48+rdi],14
	xor	rax,rax
	jmp	$L$exit

$L$badpointer::
	mov	rax,-1
$L$exit::
DB	0f3h,0c3h
_x86_64_AES_set_encrypt_key	ENDP
PUBLIC	private_AES_set_decrypt_key

ALIGN	16
private_AES_set_decrypt_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_private_AES_set_decrypt_key::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	push	rdx
$L$dec_key_prologue::

	call	_x86_64_AES_set_encrypt_key
	mov	r8,QWORD PTR[rsp]
	cmp	eax,0
	jne	$L$abort

	mov	r14d,DWORD PTR[240+r8]
	xor	rdi,rdi
	lea	rcx,QWORD PTR[r14*4+rdi]
	mov	rsi,r8
	lea	rdi,QWORD PTR[rcx*4+r8]
ALIGN	4
$L$invert::
	mov	rax,QWORD PTR[rsi]
	mov	rbx,QWORD PTR[8+rsi]
	mov	rcx,QWORD PTR[rdi]
	mov	rdx,QWORD PTR[8+rdi]
	mov	QWORD PTR[rdi],rax
	mov	QWORD PTR[8+rdi],rbx
	mov	QWORD PTR[rsi],rcx
	mov	QWORD PTR[8+rsi],rdx
	lea	rsi,QWORD PTR[16+rsi]
	lea	rdi,QWORD PTR[((-16))+rdi]
	cmp	rdi,rsi
	jne	$L$invert

	lea	rax,QWORD PTR[(($L$AES_Te+2048+1024))]

	mov	rsi,QWORD PTR[40+rax]
	mov	rdi,QWORD PTR[48+rax]
	mov	rbp,QWORD PTR[56+rax]

	mov	r15,r8
	sub	r14d,1
ALIGN	4
$L$permute::
	lea	r15,QWORD PTR[16+r15]
	mov	rax,QWORD PTR[r15]
	mov	rcx,QWORD PTR[8+r15]
	mov	r9,rsi
	mov	r12,rsi
	and	r9,rax
	and	r12,rcx
	mov	rbx,r9
	mov	rdx,r12
	shr	r9,7
	lea	r8,QWORD PTR[rax*1+rax]
	shr	r12,7
	lea	r11,QWORD PTR[rcx*1+rcx]
	sub	rbx,r9
	sub	rdx,r12
	and	r8,rdi
	and	r11,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r8,rbx
	xor	r11,rdx
	mov	r10,rsi
	mov	r13,rsi

	and	r10,r8
	and	r13,r11
	mov	rbx,r10
	mov	rdx,r13
	shr	r10,7
	lea	r9,QWORD PTR[r8*1+r8]
	shr	r13,7
	lea	r12,QWORD PTR[r11*1+r11]
	sub	rbx,r10
	sub	rdx,r13
	and	r9,rdi
	and	r12,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r9,rbx
	xor	r12,rdx
	mov	r10,rsi
	mov	r13,rsi

	and	r10,r9
	and	r13,r12
	mov	rbx,r10
	mov	rdx,r13
	shr	r10,7
	xor	r8,rax
	shr	r13,7
	xor	r11,rcx
	sub	rbx,r10
	sub	rdx,r13
	lea	r10,QWORD PTR[r9*1+r9]
	lea	r13,QWORD PTR[r12*1+r12]
	xor	r9,rax
	xor	r12,rcx
	and	r10,rdi
	and	r13,rdi
	and	rbx,rbp
	and	rdx,rbp
	xor	r10,rbx
	xor	r13,rdx

	xor	rax,r10
	xor	rcx,r13
	xor	r8,r10
	xor	r11,r13
	mov	rbx,rax
	mov	rdx,rcx
	xor	r9,r10
	shr	rbx,32
	xor	r12,r13
	shr	rdx,32
	xor	r10,r8
	rol	eax,8
	xor	r13,r11
	rol	ecx,8
	xor	r10,r9
	rol	ebx,8
	xor	r13,r12

	rol	edx,8
	xor	eax,r10d
	shr	r10,32
	xor	ecx,r13d
	shr	r13,32
	xor	ebx,r10d
	xor	edx,r13d

	mov	r10,r8
	rol	r8d,24
	mov	r13,r11
	rol	r11d,24
	shr	r10,32
	xor	eax,r8d
	shr	r13,32
	xor	ecx,r11d
	rol	r10d,24
	mov	r8,r9
	rol	r13d,24
	mov	r11,r12
	shr	r8,32
	xor	ebx,r10d
	shr	r11,32
	xor	edx,r13d


	rol	r9d,16

	rol	r12d,16

	rol	r8d,16

	xor	eax,r9d
	rol	r11d,16
	xor	ecx,r12d

	xor	ebx,r8d
	xor	edx,r11d
	mov	DWORD PTR[r15],eax
	mov	DWORD PTR[4+r15],ebx
	mov	DWORD PTR[8+r15],ecx
	mov	DWORD PTR[12+r15],edx
	sub	r14d,1
	jnz	$L$permute

	xor	rax,rax
$L$abort::
	mov	r15,QWORD PTR[8+rsp]
	mov	r14,QWORD PTR[16+rsp]
	mov	r13,QWORD PTR[24+rsp]
	mov	r12,QWORD PTR[32+rsp]
	mov	rbp,QWORD PTR[40+rsp]
	mov	rbx,QWORD PTR[48+rsp]
	add	rsp,56
$L$dec_key_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_private_AES_set_decrypt_key::
private_AES_set_decrypt_key	ENDP
PUBLIC	AES_cbc_encrypt

ALIGN	16
EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	asm_AES_cbc_encrypt

asm_AES_cbc_encrypt::
AES_cbc_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_AES_cbc_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	cmp	rdx,0
	je	$L$cbc_epilogue
	pushfq
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$cbc_prologue::

	cld
	mov	r9d,r9d

	lea	r14,QWORD PTR[$L$AES_Te]
	cmp	r9,0
	jne	$L$cbc_picked_te
	lea	r14,QWORD PTR[$L$AES_Td]
$L$cbc_picked_te::

	mov	r10d,DWORD PTR[OPENSSL_ia32cap_P]
	cmp	rdx,512
	jb	$L$cbc_slow_prologue
	test	rdx,15
	jnz	$L$cbc_slow_prologue
	bt	r10d,28
	jc	$L$cbc_slow_prologue


	lea	r15,QWORD PTR[((-88-248))+rsp]
	and	r15,-64


	mov	r10,r14
	lea	r11,QWORD PTR[2304+r14]
	mov	r12,r15
	and	r10,0FFFh
	and	r11,0FFFh
	and	r12,0FFFh

	cmp	r12,r11
	jb	$L$cbc_te_break_out
	sub	r12,r11
	sub	r15,r12
	jmp	$L$cbc_te_ok
$L$cbc_te_break_out::
	sub	r12,r10
	and	r12,0FFFh
	add	r12,320
	sub	r15,r12
ALIGN	4
$L$cbc_te_ok::

	xchg	r15,rsp

	mov	QWORD PTR[16+rsp],r15
$L$cbc_fast_body::
	mov	QWORD PTR[24+rsp],rdi
	mov	QWORD PTR[32+rsp],rsi
	mov	QWORD PTR[40+rsp],rdx
	mov	QWORD PTR[48+rsp],rcx
	mov	QWORD PTR[56+rsp],r8
	mov	DWORD PTR[((80+240))+rsp],0
	mov	rbp,r8
	mov	rbx,r9
	mov	r9,rsi
	mov	r8,rdi
	mov	r15,rcx

	mov	eax,DWORD PTR[240+r15]

	mov	r10,r15
	sub	r10,r14
	and	r10,0fffh
	cmp	r10,2304
	jb	$L$cbc_do_ecopy
	cmp	r10,4096-248
	jb	$L$cbc_skip_ecopy
ALIGN	4
$L$cbc_do_ecopy::
	mov	rsi,r15
	lea	rdi,QWORD PTR[80+rsp]
	lea	r15,QWORD PTR[80+rsp]
	mov	ecx,240/8
	DD	090A548F3h
	mov	DWORD PTR[rdi],eax
$L$cbc_skip_ecopy::
	mov	QWORD PTR[rsp],r15

	mov	ecx,18
ALIGN	4
$L$cbc_prefetch_te::
	mov	r10,QWORD PTR[r14]
	mov	r11,QWORD PTR[32+r14]
	mov	r12,QWORD PTR[64+r14]
	mov	r13,QWORD PTR[96+r14]
	lea	r14,QWORD PTR[128+r14]
	sub	ecx,1
	jnz	$L$cbc_prefetch_te
	lea	r14,QWORD PTR[((-2304))+r14]

	cmp	rbx,0
	je	$L$FAST_DECRYPT


	mov	eax,DWORD PTR[rbp]
	mov	ebx,DWORD PTR[4+rbp]
	mov	ecx,DWORD PTR[8+rbp]
	mov	edx,DWORD PTR[12+rbp]

ALIGN	4
$L$cbc_fast_enc_loop::
	xor	eax,DWORD PTR[r8]
	xor	ebx,DWORD PTR[4+r8]
	xor	ecx,DWORD PTR[8+r8]
	xor	edx,DWORD PTR[12+r8]
	mov	r15,QWORD PTR[rsp]
	mov	QWORD PTR[24+rsp],r8

	call	_x86_64_AES_encrypt

	mov	r8,QWORD PTR[24+rsp]
	mov	r10,QWORD PTR[40+rsp]
	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	lea	r8,QWORD PTR[16+r8]
	lea	r9,QWORD PTR[16+r9]
	sub	r10,16
	test	r10,-16
	mov	QWORD PTR[40+rsp],r10
	jnz	$L$cbc_fast_enc_loop
	mov	rbp,QWORD PTR[56+rsp]
	mov	DWORD PTR[rbp],eax
	mov	DWORD PTR[4+rbp],ebx
	mov	DWORD PTR[8+rbp],ecx
	mov	DWORD PTR[12+rbp],edx

	jmp	$L$cbc_fast_cleanup


ALIGN	16
$L$FAST_DECRYPT::
	cmp	r9,r8
	je	$L$cbc_fast_dec_in_place

	mov	QWORD PTR[64+rsp],rbp
ALIGN	4
$L$cbc_fast_dec_loop::
	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	r15,QWORD PTR[rsp]
	mov	QWORD PTR[24+rsp],r8

	call	_x86_64_AES_decrypt

	mov	rbp,QWORD PTR[64+rsp]
	mov	r8,QWORD PTR[24+rsp]
	mov	r10,QWORD PTR[40+rsp]
	xor	eax,DWORD PTR[rbp]
	xor	ebx,DWORD PTR[4+rbp]
	xor	ecx,DWORD PTR[8+rbp]
	xor	edx,DWORD PTR[12+rbp]
	mov	rbp,r8

	sub	r10,16
	mov	QWORD PTR[40+rsp],r10
	mov	QWORD PTR[64+rsp],rbp

	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	lea	r8,QWORD PTR[16+r8]
	lea	r9,QWORD PTR[16+r9]
	jnz	$L$cbc_fast_dec_loop
	mov	r12,QWORD PTR[56+rsp]
	mov	r10,QWORD PTR[rbp]
	mov	r11,QWORD PTR[8+rbp]
	mov	QWORD PTR[r12],r10
	mov	QWORD PTR[8+r12],r11
	jmp	$L$cbc_fast_cleanup

ALIGN	16
$L$cbc_fast_dec_in_place::
	mov	r10,QWORD PTR[rbp]
	mov	r11,QWORD PTR[8+rbp]
	mov	QWORD PTR[((0+64))+rsp],r10
	mov	QWORD PTR[((8+64))+rsp],r11
ALIGN	4
$L$cbc_fast_dec_in_place_loop::
	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	r15,QWORD PTR[rsp]
	mov	QWORD PTR[24+rsp],r8

	call	_x86_64_AES_decrypt

	mov	r8,QWORD PTR[24+rsp]
	mov	r10,QWORD PTR[40+rsp]
	xor	eax,DWORD PTR[((0+64))+rsp]
	xor	ebx,DWORD PTR[((4+64))+rsp]
	xor	ecx,DWORD PTR[((8+64))+rsp]
	xor	edx,DWORD PTR[((12+64))+rsp]

	mov	r11,QWORD PTR[r8]
	mov	r12,QWORD PTR[8+r8]
	sub	r10,16
	jz	$L$cbc_fast_dec_in_place_done

	mov	QWORD PTR[((0+64))+rsp],r11
	mov	QWORD PTR[((8+64))+rsp],r12

	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	lea	r8,QWORD PTR[16+r8]
	lea	r9,QWORD PTR[16+r9]
	mov	QWORD PTR[40+rsp],r10
	jmp	$L$cbc_fast_dec_in_place_loop
$L$cbc_fast_dec_in_place_done::
	mov	rdi,QWORD PTR[56+rsp]
	mov	QWORD PTR[rdi],r11
	mov	QWORD PTR[8+rdi],r12

	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

ALIGN	4
$L$cbc_fast_cleanup::
	cmp	DWORD PTR[((80+240))+rsp],0
	lea	rdi,QWORD PTR[80+rsp]
	je	$L$cbc_exit
	mov	ecx,240/8
	xor	rax,rax
	DD	090AB48F3h

	jmp	$L$cbc_exit


ALIGN	16
$L$cbc_slow_prologue::

	lea	rbp,QWORD PTR[((-88))+rsp]
	and	rbp,-64

	lea	r10,QWORD PTR[((-88-63))+rcx]
	sub	r10,rbp
	neg	r10
	and	r10,03c0h
	sub	rbp,r10

	xchg	rbp,rsp

	mov	QWORD PTR[16+rsp],rbp
$L$cbc_slow_body::




	mov	QWORD PTR[56+rsp],r8
	mov	rbp,r8
	mov	rbx,r9
	mov	r9,rsi
	mov	r8,rdi
	mov	r15,rcx
	mov	r10,rdx

	mov	eax,DWORD PTR[240+r15]
	mov	QWORD PTR[rsp],r15
	shl	eax,4
	lea	rax,QWORD PTR[rax*1+r15]
	mov	QWORD PTR[8+rsp],rax


	lea	r14,QWORD PTR[2048+r14]
	lea	rax,QWORD PTR[((768-8))+rsp]
	sub	rax,r14
	and	rax,0300h
	lea	r14,QWORD PTR[rax*1+r14]

	cmp	rbx,0
	je	$L$SLOW_DECRYPT


	test	r10,-16
	mov	eax,DWORD PTR[rbp]
	mov	ebx,DWORD PTR[4+rbp]
	mov	ecx,DWORD PTR[8+rbp]
	mov	edx,DWORD PTR[12+rbp]
	jz	$L$cbc_slow_enc_tail

ALIGN	4
$L$cbc_slow_enc_loop::
	xor	eax,DWORD PTR[r8]
	xor	ebx,DWORD PTR[4+r8]
	xor	ecx,DWORD PTR[8+r8]
	xor	edx,DWORD PTR[12+r8]
	mov	r15,QWORD PTR[rsp]
	mov	QWORD PTR[24+rsp],r8
	mov	QWORD PTR[32+rsp],r9
	mov	QWORD PTR[40+rsp],r10

	call	_x86_64_AES_encrypt_compact

	mov	r8,QWORD PTR[24+rsp]
	mov	r9,QWORD PTR[32+rsp]
	mov	r10,QWORD PTR[40+rsp]
	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	lea	r8,QWORD PTR[16+r8]
	lea	r9,QWORD PTR[16+r9]
	sub	r10,16
	test	r10,-16
	jnz	$L$cbc_slow_enc_loop
	test	r10,15
	jnz	$L$cbc_slow_enc_tail
	mov	rbp,QWORD PTR[56+rsp]
	mov	DWORD PTR[rbp],eax
	mov	DWORD PTR[4+rbp],ebx
	mov	DWORD PTR[8+rbp],ecx
	mov	DWORD PTR[12+rbp],edx

	jmp	$L$cbc_exit

ALIGN	4
$L$cbc_slow_enc_tail::
	mov	r11,rax
	mov	r12,rcx
	mov	rcx,r10
	mov	rsi,r8
	mov	rdi,r9
	DD	09066A4F3h
	mov	rcx,16
	sub	rcx,r10
	xor	rax,rax
	DD	09066AAF3h
	mov	r8,r9
	mov	r10,16
	mov	rax,r11
	mov	rcx,r12
	jmp	$L$cbc_slow_enc_loop

ALIGN	16
$L$SLOW_DECRYPT::
	shr	rax,3
	add	r14,rax

	mov	r11,QWORD PTR[rbp]
	mov	r12,QWORD PTR[8+rbp]
	mov	QWORD PTR[((0+64))+rsp],r11
	mov	QWORD PTR[((8+64))+rsp],r12

ALIGN	4
$L$cbc_slow_dec_loop::
	mov	eax,DWORD PTR[r8]
	mov	ebx,DWORD PTR[4+r8]
	mov	ecx,DWORD PTR[8+r8]
	mov	edx,DWORD PTR[12+r8]
	mov	r15,QWORD PTR[rsp]
	mov	QWORD PTR[24+rsp],r8
	mov	QWORD PTR[32+rsp],r9
	mov	QWORD PTR[40+rsp],r10

	call	_x86_64_AES_decrypt_compact

	mov	r8,QWORD PTR[24+rsp]
	mov	r9,QWORD PTR[32+rsp]
	mov	r10,QWORD PTR[40+rsp]
	xor	eax,DWORD PTR[((0+64))+rsp]
	xor	ebx,DWORD PTR[((4+64))+rsp]
	xor	ecx,DWORD PTR[((8+64))+rsp]
	xor	edx,DWORD PTR[((12+64))+rsp]

	mov	r11,QWORD PTR[r8]
	mov	r12,QWORD PTR[8+r8]
	sub	r10,16
	jc	$L$cbc_slow_dec_partial
	jz	$L$cbc_slow_dec_done

	mov	QWORD PTR[((0+64))+rsp],r11
	mov	QWORD PTR[((8+64))+rsp],r12

	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	lea	r8,QWORD PTR[16+r8]
	lea	r9,QWORD PTR[16+r9]
	jmp	$L$cbc_slow_dec_loop
$L$cbc_slow_dec_done::
	mov	rdi,QWORD PTR[56+rsp]
	mov	QWORD PTR[rdi],r11
	mov	QWORD PTR[8+rdi],r12

	mov	DWORD PTR[r9],eax
	mov	DWORD PTR[4+r9],ebx
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx

	jmp	$L$cbc_exit

ALIGN	4
$L$cbc_slow_dec_partial::
	mov	rdi,QWORD PTR[56+rsp]
	mov	QWORD PTR[rdi],r11
	mov	QWORD PTR[8+rdi],r12

	mov	DWORD PTR[((0+64))+rsp],eax
	mov	DWORD PTR[((4+64))+rsp],ebx
	mov	DWORD PTR[((8+64))+rsp],ecx
	mov	DWORD PTR[((12+64))+rsp],edx

	mov	rdi,r9
	lea	rsi,QWORD PTR[64+rsp]
	lea	rcx,QWORD PTR[16+r10]
	DD	09066A4F3h
	jmp	$L$cbc_exit

ALIGN	16
$L$cbc_exit::
	mov	rsi,QWORD PTR[16+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$cbc_popfq::
	popfq
$L$cbc_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_AES_cbc_encrypt::
AES_cbc_encrypt	ENDP
ALIGN	64
$L$AES_Te::
	DD	0a56363c6h,0a56363c6h
	DD	0847c7cf8h,0847c7cf8h
	DD	0997777eeh,0997777eeh
	DD	08d7b7bf6h,08d7b7bf6h
	DD	00df2f2ffh,00df2f2ffh
	DD	0bd6b6bd6h,0bd6b6bd6h
	DD	0b16f6fdeh,0b16f6fdeh
	DD	054c5c591h,054c5c591h
	DD	050303060h,050303060h
	DD	003010102h,003010102h
	DD	0a96767ceh,0a96767ceh
	DD	07d2b2b56h,07d2b2b56h
	DD	019fefee7h,019fefee7h
	DD	062d7d7b5h,062d7d7b5h
	DD	0e6abab4dh,0e6abab4dh
	DD	09a7676ech,09a7676ech
	DD	045caca8fh,045caca8fh
	DD	09d82821fh,09d82821fh
	DD	040c9c989h,040c9c989h
	DD	0877d7dfah,0877d7dfah
	DD	015fafaefh,015fafaefh
	DD	0eb5959b2h,0eb5959b2h
	DD	0c947478eh,0c947478eh
	DD	00bf0f0fbh,00bf0f0fbh
	DD	0ecadad41h,0ecadad41h
	DD	067d4d4b3h,067d4d4b3h
	DD	0fda2a25fh,0fda2a25fh
	DD	0eaafaf45h,0eaafaf45h
	DD	0bf9c9c23h,0bf9c9c23h
	DD	0f7a4a453h,0f7a4a453h
	DD	0967272e4h,0967272e4h
	DD	05bc0c09bh,05bc0c09bh
	DD	0c2b7b775h,0c2b7b775h
	DD	01cfdfde1h,01cfdfde1h
	DD	0ae93933dh,0ae93933dh
	DD	06a26264ch,06a26264ch
	DD	05a36366ch,05a36366ch
	DD	0413f3f7eh,0413f3f7eh
	DD	002f7f7f5h,002f7f7f5h
	DD	04fcccc83h,04fcccc83h
	DD	05c343468h,05c343468h
	DD	0f4a5a551h,0f4a5a551h
	DD	034e5e5d1h,034e5e5d1h
	DD	008f1f1f9h,008f1f1f9h
	DD	0937171e2h,0937171e2h
	DD	073d8d8abh,073d8d8abh
	DD	053313162h,053313162h
	DD	03f15152ah,03f15152ah
	DD	00c040408h,00c040408h
	DD	052c7c795h,052c7c795h
	DD	065232346h,065232346h
	DD	05ec3c39dh,05ec3c39dh
	DD	028181830h,028181830h
	DD	0a1969637h,0a1969637h
	DD	00f05050ah,00f05050ah
	DD	0b59a9a2fh,0b59a9a2fh
	DD	00907070eh,00907070eh
	DD	036121224h,036121224h
	DD	09b80801bh,09b80801bh
	DD	03de2e2dfh,03de2e2dfh
	DD	026ebebcdh,026ebebcdh
	DD	06927274eh,06927274eh
	DD	0cdb2b27fh,0cdb2b27fh
	DD	09f7575eah,09f7575eah
	DD	01b090912h,01b090912h
	DD	09e83831dh,09e83831dh
	DD	0742c2c58h,0742c2c58h
	DD	02e1a1a34h,02e1a1a34h
	DD	02d1b1b36h,02d1b1b36h
	DD	0b26e6edch,0b26e6edch
	DD	0ee5a5ab4h,0ee5a5ab4h
	DD	0fba0a05bh,0fba0a05bh
	DD	0f65252a4h,0f65252a4h
	DD	04d3b3b76h,04d3b3b76h
	DD	061d6d6b7h,061d6d6b7h
	DD	0ceb3b37dh,0ceb3b37dh
	DD	07b292952h,07b292952h
	DD	03ee3e3ddh,03ee3e3ddh
	DD	0712f2f5eh,0712f2f5eh
	DD	097848413h,097848413h
	DD	0f55353a6h,0f55353a6h
	DD	068d1d1b9h,068d1d1b9h
	DD	000000000h,000000000h
	DD	02cededc1h,02cededc1h
	DD	060202040h,060202040h
	DD	01ffcfce3h,01ffcfce3h
	DD	0c8b1b179h,0c8b1b179h
	DD	0ed5b5bb6h,0ed5b5bb6h
	DD	0be6a6ad4h,0be6a6ad4h
	DD	046cbcb8dh,046cbcb8dh
	DD	0d9bebe67h,0d9bebe67h
	DD	04b393972h,04b393972h
	DD	0de4a4a94h,0de4a4a94h
	DD	0d44c4c98h,0d44c4c98h
	DD	0e85858b0h,0e85858b0h
	DD	04acfcf85h,04acfcf85h
	DD	06bd0d0bbh,06bd0d0bbh
	DD	02aefefc5h,02aefefc5h
	DD	0e5aaaa4fh,0e5aaaa4fh
	DD	016fbfbedh,016fbfbedh
	DD	0c5434386h,0c5434386h
	DD	0d74d4d9ah,0d74d4d9ah
	DD	055333366h,055333366h
	DD	094858511h,094858511h
	DD	0cf45458ah,0cf45458ah
	DD	010f9f9e9h,010f9f9e9h
	DD	006020204h,006020204h
	DD	0817f7ffeh,0817f7ffeh
	DD	0f05050a0h,0f05050a0h
	DD	0443c3c78h,0443c3c78h
	DD	0ba9f9f25h,0ba9f9f25h
	DD	0e3a8a84bh,0e3a8a84bh
	DD	0f35151a2h,0f35151a2h
	DD	0fea3a35dh,0fea3a35dh
	DD	0c0404080h,0c0404080h
	DD	08a8f8f05h,08a8f8f05h
	DD	0ad92923fh,0ad92923fh
	DD	0bc9d9d21h,0bc9d9d21h
	DD	048383870h,048383870h
	DD	004f5f5f1h,004f5f5f1h
	DD	0dfbcbc63h,0dfbcbc63h
	DD	0c1b6b677h,0c1b6b677h
	DD	075dadaafh,075dadaafh
	DD	063212142h,063212142h
	DD	030101020h,030101020h
	DD	01affffe5h,01affffe5h
	DD	00ef3f3fdh,00ef3f3fdh
	DD	06dd2d2bfh,06dd2d2bfh
	DD	04ccdcd81h,04ccdcd81h
	DD	0140c0c18h,0140c0c18h
	DD	035131326h,035131326h
	DD	02fececc3h,02fececc3h
	DD	0e15f5fbeh,0e15f5fbeh
	DD	0a2979735h,0a2979735h
	DD	0cc444488h,0cc444488h
	DD	03917172eh,03917172eh
	DD	057c4c493h,057c4c493h
	DD	0f2a7a755h,0f2a7a755h
	DD	0827e7efch,0827e7efch
	DD	0473d3d7ah,0473d3d7ah
	DD	0ac6464c8h,0ac6464c8h
	DD	0e75d5dbah,0e75d5dbah
	DD	02b191932h,02b191932h
	DD	0957373e6h,0957373e6h
	DD	0a06060c0h,0a06060c0h
	DD	098818119h,098818119h
	DD	0d14f4f9eh,0d14f4f9eh
	DD	07fdcdca3h,07fdcdca3h
	DD	066222244h,066222244h
	DD	07e2a2a54h,07e2a2a54h
	DD	0ab90903bh,0ab90903bh
	DD	08388880bh,08388880bh
	DD	0ca46468ch,0ca46468ch
	DD	029eeeec7h,029eeeec7h
	DD	0d3b8b86bh,0d3b8b86bh
	DD	03c141428h,03c141428h
	DD	079dedea7h,079dedea7h
	DD	0e25e5ebch,0e25e5ebch
	DD	01d0b0b16h,01d0b0b16h
	DD	076dbdbadh,076dbdbadh
	DD	03be0e0dbh,03be0e0dbh
	DD	056323264h,056323264h
	DD	04e3a3a74h,04e3a3a74h
	DD	01e0a0a14h,01e0a0a14h
	DD	0db494992h,0db494992h
	DD	00a06060ch,00a06060ch
	DD	06c242448h,06c242448h
	DD	0e45c5cb8h,0e45c5cb8h
	DD	05dc2c29fh,05dc2c29fh
	DD	06ed3d3bdh,06ed3d3bdh
	DD	0efacac43h,0efacac43h
	DD	0a66262c4h,0a66262c4h
	DD	0a8919139h,0a8919139h
	DD	0a4959531h,0a4959531h
	DD	037e4e4d3h,037e4e4d3h
	DD	08b7979f2h,08b7979f2h
	DD	032e7e7d5h,032e7e7d5h
	DD	043c8c88bh,043c8c88bh
	DD	05937376eh,05937376eh
	DD	0b76d6ddah,0b76d6ddah
	DD	08c8d8d01h,08c8d8d01h
	DD	064d5d5b1h,064d5d5b1h
	DD	0d24e4e9ch,0d24e4e9ch
	DD	0e0a9a949h,0e0a9a949h
	DD	0b46c6cd8h,0b46c6cd8h
	DD	0fa5656ach,0fa5656ach
	DD	007f4f4f3h,007f4f4f3h
	DD	025eaeacfh,025eaeacfh
	DD	0af6565cah,0af6565cah
	DD	08e7a7af4h,08e7a7af4h
	DD	0e9aeae47h,0e9aeae47h
	DD	018080810h,018080810h
	DD	0d5baba6fh,0d5baba6fh
	DD	0887878f0h,0887878f0h
	DD	06f25254ah,06f25254ah
	DD	0722e2e5ch,0722e2e5ch
	DD	0241c1c38h,0241c1c38h
	DD	0f1a6a657h,0f1a6a657h
	DD	0c7b4b473h,0c7b4b473h
	DD	051c6c697h,051c6c697h
	DD	023e8e8cbh,023e8e8cbh
	DD	07cdddda1h,07cdddda1h
	DD	09c7474e8h,09c7474e8h
	DD	0211f1f3eh,0211f1f3eh
	DD	0dd4b4b96h,0dd4b4b96h
	DD	0dcbdbd61h,0dcbdbd61h
	DD	0868b8b0dh,0868b8b0dh
	DD	0858a8a0fh,0858a8a0fh
	DD	0907070e0h,0907070e0h
	DD	0423e3e7ch,0423e3e7ch
	DD	0c4b5b571h,0c4b5b571h
	DD	0aa6666cch,0aa6666cch
	DD	0d8484890h,0d8484890h
	DD	005030306h,005030306h
	DD	001f6f6f7h,001f6f6f7h
	DD	0120e0e1ch,0120e0e1ch
	DD	0a36161c2h,0a36161c2h
	DD	05f35356ah,05f35356ah
	DD	0f95757aeh,0f95757aeh
	DD	0d0b9b969h,0d0b9b969h
	DD	091868617h,091868617h
	DD	058c1c199h,058c1c199h
	DD	0271d1d3ah,0271d1d3ah
	DD	0b99e9e27h,0b99e9e27h
	DD	038e1e1d9h,038e1e1d9h
	DD	013f8f8ebh,013f8f8ebh
	DD	0b398982bh,0b398982bh
	DD	033111122h,033111122h
	DD	0bb6969d2h,0bb6969d2h
	DD	070d9d9a9h,070d9d9a9h
	DD	0898e8e07h,0898e8e07h
	DD	0a7949433h,0a7949433h
	DD	0b69b9b2dh,0b69b9b2dh
	DD	0221e1e3ch,0221e1e3ch
	DD	092878715h,092878715h
	DD	020e9e9c9h,020e9e9c9h
	DD	049cece87h,049cece87h
	DD	0ff5555aah,0ff5555aah
	DD	078282850h,078282850h
	DD	07adfdfa5h,07adfdfa5h
	DD	08f8c8c03h,08f8c8c03h
	DD	0f8a1a159h,0f8a1a159h
	DD	080898909h,080898909h
	DD	0170d0d1ah,0170d0d1ah
	DD	0dabfbf65h,0dabfbf65h
	DD	031e6e6d7h,031e6e6d7h
	DD	0c6424284h,0c6424284h
	DD	0b86868d0h,0b86868d0h
	DD	0c3414182h,0c3414182h
	DD	0b0999929h,0b0999929h
	DD	0772d2d5ah,0772d2d5ah
	DD	0110f0f1eh,0110f0f1eh
	DD	0cbb0b07bh,0cbb0b07bh
	DD	0fc5454a8h,0fc5454a8h
	DD	0d6bbbb6dh,0d6bbbb6dh
	DD	03a16162ch,03a16162ch
DB	063h,07ch,077h,07bh,0f2h,06bh,06fh,0c5h
DB	030h,001h,067h,02bh,0feh,0d7h,0abh,076h
DB	0cah,082h,0c9h,07dh,0fah,059h,047h,0f0h
DB	0adh,0d4h,0a2h,0afh,09ch,0a4h,072h,0c0h
DB	0b7h,0fdh,093h,026h,036h,03fh,0f7h,0cch
DB	034h,0a5h,0e5h,0f1h,071h,0d8h,031h,015h
DB	004h,0c7h,023h,0c3h,018h,096h,005h,09ah
DB	007h,012h,080h,0e2h,0ebh,027h,0b2h,075h
DB	009h,083h,02ch,01ah,01bh,06eh,05ah,0a0h
DB	052h,03bh,0d6h,0b3h,029h,0e3h,02fh,084h
DB	053h,0d1h,000h,0edh,020h,0fch,0b1h,05bh
DB	06ah,0cbh,0beh,039h,04ah,04ch,058h,0cfh
DB	0d0h,0efh,0aah,0fbh,043h,04dh,033h,085h
DB	045h,0f9h,002h,07fh,050h,03ch,09fh,0a8h
DB	051h,0a3h,040h,08fh,092h,09dh,038h,0f5h
DB	0bch,0b6h,0dah,021h,010h,0ffh,0f3h,0d2h
DB	0cdh,00ch,013h,0ech,05fh,097h,044h,017h
DB	0c4h,0a7h,07eh,03dh,064h,05dh,019h,073h
DB	060h,081h,04fh,0dch,022h,02ah,090h,088h
DB	046h,0eeh,0b8h,014h,0deh,05eh,00bh,0dbh
DB	0e0h,032h,03ah,00ah,049h,006h,024h,05ch
DB	0c2h,0d3h,0ach,062h,091h,095h,0e4h,079h
DB	0e7h,0c8h,037h,06dh,08dh,0d5h,04eh,0a9h
DB	06ch,056h,0f4h,0eah,065h,07ah,0aeh,008h
DB	0bah,078h,025h,02eh,01ch,0a6h,0b4h,0c6h
DB	0e8h,0ddh,074h,01fh,04bh,0bdh,08bh,08ah
DB	070h,03eh,0b5h,066h,048h,003h,0f6h,00eh
DB	061h,035h,057h,0b9h,086h,0c1h,01dh,09eh
DB	0e1h,0f8h,098h,011h,069h,0d9h,08eh,094h
DB	09bh,01eh,087h,0e9h,0ceh,055h,028h,0dfh
DB	08ch,0a1h,089h,00dh,0bfh,0e6h,042h,068h
DB	041h,099h,02dh,00fh,0b0h,054h,0bbh,016h
DB	063h,07ch,077h,07bh,0f2h,06bh,06fh,0c5h
DB	030h,001h,067h,02bh,0feh,0d7h,0abh,076h
DB	0cah,082h,0c9h,07dh,0fah,059h,047h,0f0h
DB	0adh,0d4h,0a2h,0afh,09ch,0a4h,072h,0c0h
DB	0b7h,0fdh,093h,026h,036h,03fh,0f7h,0cch
DB	034h,0a5h,0e5h,0f1h,071h,0d8h,031h,015h
DB	004h,0c7h,023h,0c3h,018h,096h,005h,09ah
DB	007h,012h,080h,0e2h,0ebh,027h,0b2h,075h
DB	009h,083h,02ch,01ah,01bh,06eh,05ah,0a0h
DB	052h,03bh,0d6h,0b3h,029h,0e3h,02fh,084h
DB	053h,0d1h,000h,0edh,020h,0fch,0b1h,05bh
DB	06ah,0cbh,0beh,039h,04ah,04ch,058h,0cfh
DB	0d0h,0efh,0aah,0fbh,043h,04dh,033h,085h
DB	045h,0f9h,002h,07fh,050h,03ch,09fh,0a8h
DB	051h,0a3h,040h,08fh,092h,09dh,038h,0f5h
DB	0bch,0b6h,0dah,021h,010h,0ffh,0f3h,0d2h
DB	0cdh,00ch,013h,0ech,05fh,097h,044h,017h
DB	0c4h,0a7h,07eh,03dh,064h,05dh,019h,073h
DB	060h,081h,04fh,0dch,022h,02ah,090h,088h
DB	046h,0eeh,0b8h,014h,0deh,05eh,00bh,0dbh
DB	0e0h,032h,03ah,00ah,049h,006h,024h,05ch
DB	0c2h,0d3h,0ach,062h,091h,095h,0e4h,079h
DB	0e7h,0c8h,037h,06dh,08dh,0d5h,04eh,0a9h
DB	06ch,056h,0f4h,0eah,065h,07ah,0aeh,008h
DB	0bah,078h,025h,02eh,01ch,0a6h,0b4h,0c6h
DB	0e8h,0ddh,074h,01fh,04bh,0bdh,08bh,08ah
DB	070h,03eh,0b5h,066h,048h,003h,0f6h,00eh
DB	061h,035h,057h,0b9h,086h,0c1h,01dh,09eh
DB	0e1h,0f8h,098h,011h,069h,0d9h,08eh,094h
DB	09bh,01eh,087h,0e9h,0ceh,055h,028h,0dfh
DB	08ch,0a1h,089h,00dh,0bfh,0e6h,042h,068h
DB	041h,099h,02dh,00fh,0b0h,054h,0bbh,016h
DB	063h,07ch,077h,07bh,0f2h,06bh,06fh,0c5h
DB	030h,001h,067h,02bh,0feh,0d7h,0abh,076h
DB	0cah,082h,0c9h,07dh,0fah,059h,047h,0f0h
DB	0adh,0d4h,0a2h,0afh,09ch,0a4h,072h,0c0h
DB	0b7h,0fdh,093h,026h,036h,03fh,0f7h,0cch
DB	034h,0a5h,0e5h,0f1h,071h,0d8h,031h,015h
DB	004h,0c7h,023h,0c3h,018h,096h,005h,09ah
DB	007h,012h,080h,0e2h,0ebh,027h,0b2h,075h
DB	009h,083h,02ch,01ah,01bh,06eh,05ah,0a0h
DB	052h,03bh,0d6h,0b3h,029h,0e3h,02fh,084h
DB	053h,0d1h,000h,0edh,020h,0fch,0b1h,05bh
DB	06ah,0cbh,0beh,039h,04ah,04ch,058h,0cfh
DB	0d0h,0efh,0aah,0fbh,043h,04dh,033h,085h
DB	045h,0f9h,002h,07fh,050h,03ch,09fh,0a8h
DB	051h,0a3h,040h,08fh,092h,09dh,038h,0f5h
DB	0bch,0b6h,0dah,021h,010h,0ffh,0f3h,0d2h
DB	0cdh,00ch,013h,0ech,05fh,097h,044h,017h
DB	0c4h,0a7h,07eh,03dh,064h,05dh,019h,073h
DB	060h,081h,04fh,0dch,022h,02ah,090h,088h
DB	046h,0eeh,0b8h,014h,0deh,05eh,00bh,0dbh
DB	0e0h,032h,03ah,00ah,049h,006h,024h,05ch
DB	0c2h,0d3h,0ach,062h,091h,095h,0e4h,079h
DB	0e7h,0c8h,037h,06dh,08dh,0d5h,04eh,0a9h
DB	06ch,056h,0f4h,0eah,065h,07ah,0aeh,008h
DB	0bah,078h,025h,02eh,01ch,0a6h,0b4h,0c6h
DB	0e8h,0ddh,074h,01fh,04bh,0bdh,08bh,08ah
DB	070h,03eh,0b5h,066h,048h,003h,0f6h,00eh
DB	061h,035h,057h,0b9h,086h,0c1h,01dh,09eh
DB	0e1h,0f8h,098h,011h,069h,0d9h,08eh,094h
DB	09bh,01eh,087h,0e9h,0ceh,055h,028h,0dfh
DB	08ch,0a1h,089h,00dh,0bfh,0e6h,042h,068h
DB	041h,099h,02dh,00fh,0b0h,054h,0bbh,016h
DB	063h,07ch,077h,07bh,0f2h,06bh,06fh,0c5h
DB	030h,001h,067h,02bh,0feh,0d7h,0abh,076h
DB	0cah,082h,0c9h,07dh,0fah,059h,047h,0f0h
DB	0adh,0d4h,0a2h,0afh,09ch,0a4h,072h,0c0h
DB	0b7h,0fdh,093h,026h,036h,03fh,0f7h,0cch
DB	034h,0a5h,0e5h,0f1h,071h,0d8h,031h,015h
DB	004h,0c7h,023h,0c3h,018h,096h,005h,09ah
DB	007h,012h,080h,0e2h,0ebh,027h,0b2h,075h
DB	009h,083h,02ch,01ah,01bh,06eh,05ah,0a0h
DB	052h,03bh,0d6h,0b3h,029h,0e3h,02fh,084h
DB	053h,0d1h,000h,0edh,020h,0fch,0b1h,05bh
DB	06ah,0cbh,0beh,039h,04ah,04ch,058h,0cfh
DB	0d0h,0efh,0aah,0fbh,043h,04dh,033h,085h
DB	045h,0f9h,002h,07fh,050h,03ch,09fh,0a8h
DB	051h,0a3h,040h,08fh,092h,09dh,038h,0f5h
DB	0bch,0b6h,0dah,021h,010h,0ffh,0f3h,0d2h
DB	0cdh,00ch,013h,0ech,05fh,097h,044h,017h
DB	0c4h,0a7h,07eh,03dh,064h,05dh,019h,073h
DB	060h,081h,04fh,0dch,022h,02ah,090h,088h
DB	046h,0eeh,0b8h,014h,0deh,05eh,00bh,0dbh
DB	0e0h,032h,03ah,00ah,049h,006h,024h,05ch
DB	0c2h,0d3h,0ach,062h,091h,095h,0e4h,079h
DB	0e7h,0c8h,037h,06dh,08dh,0d5h,04eh,0a9h
DB	06ch,056h,0f4h,0eah,065h,07ah,0aeh,008h
DB	0bah,078h,025h,02eh,01ch,0a6h,0b4h,0c6h
DB	0e8h,0ddh,074h,01fh,04bh,0bdh,08bh,08ah
DB	070h,03eh,0b5h,066h,048h,003h,0f6h,00eh
DB	061h,035h,057h,0b9h,086h,0c1h,01dh,09eh
DB	0e1h,0f8h,098h,011h,069h,0d9h,08eh,094h
DB	09bh,01eh,087h,0e9h,0ceh,055h,028h,0dfh
DB	08ch,0a1h,089h,00dh,0bfh,0e6h,042h,068h
DB	041h,099h,02dh,00fh,0b0h,054h,0bbh,016h
	DD	000000001h,000000002h,000000004h,000000008h
	DD	000000010h,000000020h,000000040h,000000080h
	DD	00000001bh,000000036h,080808080h,080808080h
	DD	0fefefefeh,0fefefefeh,01b1b1b1bh,01b1b1b1bh
ALIGN	64
$L$AES_Td::
	DD	050a7f451h,050a7f451h
	DD	05365417eh,05365417eh
	DD	0c3a4171ah,0c3a4171ah
	DD	0965e273ah,0965e273ah
	DD	0cb6bab3bh,0cb6bab3bh
	DD	0f1459d1fh,0f1459d1fh
	DD	0ab58faach,0ab58faach
	DD	09303e34bh,09303e34bh
	DD	055fa3020h,055fa3020h
	DD	0f66d76adh,0f66d76adh
	DD	09176cc88h,09176cc88h
	DD	0254c02f5h,0254c02f5h
	DD	0fcd7e54fh,0fcd7e54fh
	DD	0d7cb2ac5h,0d7cb2ac5h
	DD	080443526h,080443526h
	DD	08fa362b5h,08fa362b5h
	DD	0495ab1deh,0495ab1deh
	DD	0671bba25h,0671bba25h
	DD	0980eea45h,0980eea45h
	DD	0e1c0fe5dh,0e1c0fe5dh
	DD	002752fc3h,002752fc3h
	DD	012f04c81h,012f04c81h
	DD	0a397468dh,0a397468dh
	DD	0c6f9d36bh,0c6f9d36bh
	DD	0e75f8f03h,0e75f8f03h
	DD	0959c9215h,0959c9215h
	DD	0eb7a6dbfh,0eb7a6dbfh
	DD	0da595295h,0da595295h
	DD	02d83bed4h,02d83bed4h
	DD	0d3217458h,0d3217458h
	DD	02969e049h,02969e049h
	DD	044c8c98eh,044c8c98eh
	DD	06a89c275h,06a89c275h
	DD	078798ef4h,078798ef4h
	DD	06b3e5899h,06b3e5899h
	DD	0dd71b927h,0dd71b927h
	DD	0b64fe1beh,0b64fe1beh
	DD	017ad88f0h,017ad88f0h
	DD	066ac20c9h,066ac20c9h
	DD	0b43ace7dh,0b43ace7dh
	DD	0184adf63h,0184adf63h
	DD	082311ae5h,082311ae5h
	DD	060335197h,060335197h
	DD	0457f5362h,0457f5362h
	DD	0e07764b1h,0e07764b1h
	DD	084ae6bbbh,084ae6bbbh
	DD	01ca081feh,01ca081feh
	DD	0942b08f9h,0942b08f9h
	DD	058684870h,058684870h
	DD	019fd458fh,019fd458fh
	DD	0876cde94h,0876cde94h
	DD	0b7f87b52h,0b7f87b52h
	DD	023d373abh,023d373abh
	DD	0e2024b72h,0e2024b72h
	DD	0578f1fe3h,0578f1fe3h
	DD	02aab5566h,02aab5566h
	DD	00728ebb2h,00728ebb2h
	DD	003c2b52fh,003c2b52fh
	DD	09a7bc586h,09a7bc586h
	DD	0a50837d3h,0a50837d3h
	DD	0f2872830h,0f2872830h
	DD	0b2a5bf23h,0b2a5bf23h
	DD	0ba6a0302h,0ba6a0302h
	DD	05c8216edh,05c8216edh
	DD	02b1ccf8ah,02b1ccf8ah
	DD	092b479a7h,092b479a7h
	DD	0f0f207f3h,0f0f207f3h
	DD	0a1e2694eh,0a1e2694eh
	DD	0cdf4da65h,0cdf4da65h
	DD	0d5be0506h,0d5be0506h
	DD	01f6234d1h,01f6234d1h
	DD	08afea6c4h,08afea6c4h
	DD	09d532e34h,09d532e34h
	DD	0a055f3a2h,0a055f3a2h
	DD	032e18a05h,032e18a05h
	DD	075ebf6a4h,075ebf6a4h
	DD	039ec830bh,039ec830bh
	DD	0aaef6040h,0aaef6040h
	DD	0069f715eh,0069f715eh
	DD	051106ebdh,051106ebdh
	DD	0f98a213eh,0f98a213eh
	DD	03d06dd96h,03d06dd96h
	DD	0ae053eddh,0ae053eddh
	DD	046bde64dh,046bde64dh
	DD	0b58d5491h,0b58d5491h
	DD	0055dc471h,0055dc471h
	DD	06fd40604h,06fd40604h
	DD	0ff155060h,0ff155060h
	DD	024fb9819h,024fb9819h
	DD	097e9bdd6h,097e9bdd6h
	DD	0cc434089h,0cc434089h
	DD	0779ed967h,0779ed967h
	DD	0bd42e8b0h,0bd42e8b0h
	DD	0888b8907h,0888b8907h
	DD	0385b19e7h,0385b19e7h
	DD	0dbeec879h,0dbeec879h
	DD	0470a7ca1h,0470a7ca1h
	DD	0e90f427ch,0e90f427ch
	DD	0c91e84f8h,0c91e84f8h
	DD	000000000h,000000000h
	DD	083868009h,083868009h
	DD	048ed2b32h,048ed2b32h
	DD	0ac70111eh,0ac70111eh
	DD	04e725a6ch,04e725a6ch
	DD	0fbff0efdh,0fbff0efdh
	DD	05638850fh,05638850fh
	DD	01ed5ae3dh,01ed5ae3dh
	DD	027392d36h,027392d36h
	DD	064d90f0ah,064d90f0ah
	DD	021a65c68h,021a65c68h
	DD	0d1545b9bh,0d1545b9bh
	DD	03a2e3624h,03a2e3624h
	DD	0b1670a0ch,0b1670a0ch
	DD	00fe75793h,00fe75793h
	DD	0d296eeb4h,0d296eeb4h
	DD	09e919b1bh,09e919b1bh
	DD	04fc5c080h,04fc5c080h
	DD	0a220dc61h,0a220dc61h
	DD	0694b775ah,0694b775ah
	DD	0161a121ch,0161a121ch
	DD	00aba93e2h,00aba93e2h
	DD	0e52aa0c0h,0e52aa0c0h
	DD	043e0223ch,043e0223ch
	DD	01d171b12h,01d171b12h
	DD	00b0d090eh,00b0d090eh
	DD	0adc78bf2h,0adc78bf2h
	DD	0b9a8b62dh,0b9a8b62dh
	DD	0c8a91e14h,0c8a91e14h
	DD	08519f157h,08519f157h
	DD	04c0775afh,04c0775afh
	DD	0bbdd99eeh,0bbdd99eeh
	DD	0fd607fa3h,0fd607fa3h
	DD	09f2601f7h,09f2601f7h
	DD	0bcf5725ch,0bcf5725ch
	DD	0c53b6644h,0c53b6644h
	DD	0347efb5bh,0347efb5bh
	DD	07629438bh,07629438bh
	DD	0dcc623cbh,0dcc623cbh
	DD	068fcedb6h,068fcedb6h
	DD	063f1e4b8h,063f1e4b8h
	DD	0cadc31d7h,0cadc31d7h
	DD	010856342h,010856342h
	DD	040229713h,040229713h
	DD	02011c684h,02011c684h
	DD	07d244a85h,07d244a85h
	DD	0f83dbbd2h,0f83dbbd2h
	DD	01132f9aeh,01132f9aeh
	DD	06da129c7h,06da129c7h
	DD	04b2f9e1dh,04b2f9e1dh
	DD	0f330b2dch,0f330b2dch
	DD	0ec52860dh,0ec52860dh
	DD	0d0e3c177h,0d0e3c177h
	DD	06c16b32bh,06c16b32bh
	DD	099b970a9h,099b970a9h
	DD	0fa489411h,0fa489411h
	DD	02264e947h,02264e947h
	DD	0c48cfca8h,0c48cfca8h
	DD	01a3ff0a0h,01a3ff0a0h
	DD	0d82c7d56h,0d82c7d56h
	DD	0ef903322h,0ef903322h
	DD	0c74e4987h,0c74e4987h
	DD	0c1d138d9h,0c1d138d9h
	DD	0fea2ca8ch,0fea2ca8ch
	DD	0360bd498h,0360bd498h
	DD	0cf81f5a6h,0cf81f5a6h
	DD	028de7aa5h,028de7aa5h
	DD	0268eb7dah,0268eb7dah
	DD	0a4bfad3fh,0a4bfad3fh
	DD	0e49d3a2ch,0e49d3a2ch
	DD	00d927850h,00d927850h
	DD	09bcc5f6ah,09bcc5f6ah
	DD	062467e54h,062467e54h
	DD	0c2138df6h,0c2138df6h
	DD	0e8b8d890h,0e8b8d890h
	DD	05ef7392eh,05ef7392eh
	DD	0f5afc382h,0f5afc382h
	DD	0be805d9fh,0be805d9fh
	DD	07c93d069h,07c93d069h
	DD	0a92dd56fh,0a92dd56fh
	DD	0b31225cfh,0b31225cfh
	DD	03b99acc8h,03b99acc8h
	DD	0a77d1810h,0a77d1810h
	DD	06e639ce8h,06e639ce8h
	DD	07bbb3bdbh,07bbb3bdbh
	DD	0097826cdh,0097826cdh
	DD	0f418596eh,0f418596eh
	DD	001b79aech,001b79aech
	DD	0a89a4f83h,0a89a4f83h
	DD	0656e95e6h,0656e95e6h
	DD	07ee6ffaah,07ee6ffaah
	DD	008cfbc21h,008cfbc21h
	DD	0e6e815efh,0e6e815efh
	DD	0d99be7bah,0d99be7bah
	DD	0ce366f4ah,0ce366f4ah
	DD	0d4099feah,0d4099feah
	DD	0d67cb029h,0d67cb029h
	DD	0afb2a431h,0afb2a431h
	DD	031233f2ah,031233f2ah
	DD	03094a5c6h,03094a5c6h
	DD	0c066a235h,0c066a235h
	DD	037bc4e74h,037bc4e74h
	DD	0a6ca82fch,0a6ca82fch
	DD	0b0d090e0h,0b0d090e0h
	DD	015d8a733h,015d8a733h
	DD	04a9804f1h,04a9804f1h
	DD	0f7daec41h,0f7daec41h
	DD	00e50cd7fh,00e50cd7fh
	DD	02ff69117h,02ff69117h
	DD	08dd64d76h,08dd64d76h
	DD	04db0ef43h,04db0ef43h
	DD	0544daacch,0544daacch
	DD	0df0496e4h,0df0496e4h
	DD	0e3b5d19eh,0e3b5d19eh
	DD	01b886a4ch,01b886a4ch
	DD	0b81f2cc1h,0b81f2cc1h
	DD	07f516546h,07f516546h
	DD	004ea5e9dh,004ea5e9dh
	DD	05d358c01h,05d358c01h
	DD	0737487fah,0737487fah
	DD	02e410bfbh,02e410bfbh
	DD	05a1d67b3h,05a1d67b3h
	DD	052d2db92h,052d2db92h
	DD	0335610e9h,0335610e9h
	DD	01347d66dh,01347d66dh
	DD	08c61d79ah,08c61d79ah
	DD	07a0ca137h,07a0ca137h
	DD	08e14f859h,08e14f859h
	DD	0893c13ebh,0893c13ebh
	DD	0ee27a9ceh,0ee27a9ceh
	DD	035c961b7h,035c961b7h
	DD	0ede51ce1h,0ede51ce1h
	DD	03cb1477ah,03cb1477ah
	DD	059dfd29ch,059dfd29ch
	DD	03f73f255h,03f73f255h
	DD	079ce1418h,079ce1418h
	DD	0bf37c773h,0bf37c773h
	DD	0eacdf753h,0eacdf753h
	DD	05baafd5fh,05baafd5fh
	DD	0146f3ddfh,0146f3ddfh
	DD	086db4478h,086db4478h
	DD	081f3afcah,081f3afcah
	DD	03ec468b9h,03ec468b9h
	DD	02c342438h,02c342438h
	DD	05f40a3c2h,05f40a3c2h
	DD	072c31d16h,072c31d16h
	DD	00c25e2bch,00c25e2bch
	DD	08b493c28h,08b493c28h
	DD	041950dffh,041950dffh
	DD	07101a839h,07101a839h
	DD	0deb30c08h,0deb30c08h
	DD	09ce4b4d8h,09ce4b4d8h
	DD	090c15664h,090c15664h
	DD	06184cb7bh,06184cb7bh
	DD	070b632d5h,070b632d5h
	DD	0745c6c48h,0745c6c48h
	DD	04257b8d0h,04257b8d0h
DB	052h,009h,06ah,0d5h,030h,036h,0a5h,038h
DB	0bfh,040h,0a3h,09eh,081h,0f3h,0d7h,0fbh
DB	07ch,0e3h,039h,082h,09bh,02fh,0ffh,087h
DB	034h,08eh,043h,044h,0c4h,0deh,0e9h,0cbh
DB	054h,07bh,094h,032h,0a6h,0c2h,023h,03dh
DB	0eeh,04ch,095h,00bh,042h,0fah,0c3h,04eh
DB	008h,02eh,0a1h,066h,028h,0d9h,024h,0b2h
DB	076h,05bh,0a2h,049h,06dh,08bh,0d1h,025h
DB	072h,0f8h,0f6h,064h,086h,068h,098h,016h
DB	0d4h,0a4h,05ch,0cch,05dh,065h,0b6h,092h
DB	06ch,070h,048h,050h,0fdh,0edh,0b9h,0dah
DB	05eh,015h,046h,057h,0a7h,08dh,09dh,084h
DB	090h,0d8h,0abh,000h,08ch,0bch,0d3h,00ah
DB	0f7h,0e4h,058h,005h,0b8h,0b3h,045h,006h
DB	0d0h,02ch,01eh,08fh,0cah,03fh,00fh,002h
DB	0c1h,0afh,0bdh,003h,001h,013h,08ah,06bh
DB	03ah,091h,011h,041h,04fh,067h,0dch,0eah
DB	097h,0f2h,0cfh,0ceh,0f0h,0b4h,0e6h,073h
DB	096h,0ach,074h,022h,0e7h,0adh,035h,085h
DB	0e2h,0f9h,037h,0e8h,01ch,075h,0dfh,06eh
DB	047h,0f1h,01ah,071h,01dh,029h,0c5h,089h
DB	06fh,0b7h,062h,00eh,0aah,018h,0beh,01bh
DB	0fch,056h,03eh,04bh,0c6h,0d2h,079h,020h
DB	09ah,0dbh,0c0h,0feh,078h,0cdh,05ah,0f4h
DB	01fh,0ddh,0a8h,033h,088h,007h,0c7h,031h
DB	0b1h,012h,010h,059h,027h,080h,0ech,05fh
DB	060h,051h,07fh,0a9h,019h,0b5h,04ah,00dh
DB	02dh,0e5h,07ah,09fh,093h,0c9h,09ch,0efh
DB	0a0h,0e0h,03bh,04dh,0aeh,02ah,0f5h,0b0h
DB	0c8h,0ebh,0bbh,03ch,083h,053h,099h,061h
DB	017h,02bh,004h,07eh,0bah,077h,0d6h,026h
DB	0e1h,069h,014h,063h,055h,021h,00ch,07dh
	DD	080808080h,080808080h,0fefefefeh,0fefefefeh
	DD	01b1b1b1bh,01b1b1b1bh,0,0
DB	052h,009h,06ah,0d5h,030h,036h,0a5h,038h
DB	0bfh,040h,0a3h,09eh,081h,0f3h,0d7h,0fbh
DB	07ch,0e3h,039h,082h,09bh,02fh,0ffh,087h
DB	034h,08eh,043h,044h,0c4h,0deh,0e9h,0cbh
DB	054h,07bh,094h,032h,0a6h,0c2h,023h,03dh
DB	0eeh,04ch,095h,00bh,042h,0fah,0c3h,04eh
DB	008h,02eh,0a1h,066h,028h,0d9h,024h,0b2h
DB	076h,05bh,0a2h,049h,06dh,08bh,0d1h,025h
DB	072h,0f8h,0f6h,064h,086h,068h,098h,016h
DB	0d4h,0a4h,05ch,0cch,05dh,065h,0b6h,092h
DB	06ch,070h,048h,050h,0fdh,0edh,0b9h,0dah
DB	05eh,015h,046h,057h,0a7h,08dh,09dh,084h
DB	090h,0d8h,0abh,000h,08ch,0bch,0d3h,00ah
DB	0f7h,0e4h,058h,005h,0b8h,0b3h,045h,006h
DB	0d0h,02ch,01eh,08fh,0cah,03fh,00fh,002h
DB	0c1h,0afh,0bdh,003h,001h,013h,08ah,06bh
DB	03ah,091h,011h,041h,04fh,067h,0dch,0eah
DB	097h,0f2h,0cfh,0ceh,0f0h,0b4h,0e6h,073h
DB	096h,0ach,074h,022h,0e7h,0adh,035h,085h
DB	0e2h,0f9h,037h,0e8h,01ch,075h,0dfh,06eh
DB	047h,0f1h,01ah,071h,01dh,029h,0c5h,089h
DB	06fh,0b7h,062h,00eh,0aah,018h,0beh,01bh
DB	0fch,056h,03eh,04bh,0c6h,0d2h,079h,020h
DB	09ah,0dbh,0c0h,0feh,078h,0cdh,05ah,0f4h
DB	01fh,0ddh,0a8h,033h,088h,007h,0c7h,031h
DB	0b1h,012h,010h,059h,027h,080h,0ech,05fh
DB	060h,051h,07fh,0a9h,019h,0b5h,04ah,00dh
DB	02dh,0e5h,07ah,09fh,093h,0c9h,09ch,0efh
DB	0a0h,0e0h,03bh,04dh,0aeh,02ah,0f5h,0b0h
DB	0c8h,0ebh,0bbh,03ch,083h,053h,099h,061h
DB	017h,02bh,004h,07eh,0bah,077h,0d6h,026h
DB	0e1h,069h,014h,063h,055h,021h,00ch,07dh
	DD	080808080h,080808080h,0fefefefeh,0fefefefeh
	DD	01b1b1b1bh,01b1b1b1bh,0,0
DB	052h,009h,06ah,0d5h,030h,036h,0a5h,038h
DB	0bfh,040h,0a3h,09eh,081h,0f3h,0d7h,0fbh
DB	07ch,0e3h,039h,082h,09bh,02fh,0ffh,087h
DB	034h,08eh,043h,044h,0c4h,0deh,0e9h,0cbh
DB	054h,07bh,094h,032h,0a6h,0c2h,023h,03dh
DB	0eeh,04ch,095h,00bh,042h,0fah,0c3h,04eh
DB	008h,02eh,0a1h,066h,028h,0d9h,024h,0b2h
DB	076h,05bh,0a2h,049h,06dh,08bh,0d1h,025h
DB	072h,0f8h,0f6h,064h,086h,068h,098h,016h
DB	0d4h,0a4h,05ch,0cch,05dh,065h,0b6h,092h
DB	06ch,070h,048h,050h,0fdh,0edh,0b9h,0dah
DB	05eh,015h,046h,057h,0a7h,08dh,09dh,084h
DB	090h,0d8h,0abh,000h,08ch,0bch,0d3h,00ah
DB	0f7h,0e4h,058h,005h,0b8h,0b3h,045h,006h
DB	0d0h,02ch,01eh,08fh,0cah,03fh,00fh,002h
DB	0c1h,0afh,0bdh,003h,001h,013h,08ah,06bh
DB	03ah,091h,011h,041h,04fh,067h,0dch,0eah
DB	097h,0f2h,0cfh,0ceh,0f0h,0b4h,0e6h,073h
DB	096h,0ach,074h,022h,0e7h,0adh,035h,085h
DB	0e2h,0f9h,037h,0e8h,01ch,075h,0dfh,06eh
DB	047h,0f1h,01ah,071h,01dh,029h,0c5h,089h
DB	06fh,0b7h,062h,00eh,0aah,018h,0beh,01bh
DB	0fch,056h,03eh,04bh,0c6h,0d2h,079h,020h
DB	09ah,0dbh,0c0h,0feh,078h,0cdh,05ah,0f4h
DB	01fh,0ddh,0a8h,033h,088h,007h,0c7h,031h
DB	0b1h,012h,010h,059h,027h,080h,0ech,05fh
DB	060h,051h,07fh,0a9h,019h,0b5h,04ah,00dh
DB	02dh,0e5h,07ah,09fh,093h,0c9h,09ch,0efh
DB	0a0h,0e0h,03bh,04dh,0aeh,02ah,0f5h,0b0h
DB	0c8h,0ebh,0bbh,03ch,083h,053h,099h,061h
DB	017h,02bh,004h,07eh,0bah,077h,0d6h,026h
DB	0e1h,069h,014h,063h,055h,021h,00ch,07dh
	DD	080808080h,080808080h,0fefefefeh,0fefefefeh
	DD	01b1b1b1bh,01b1b1b1bh,0,0
DB	052h,009h,06ah,0d5h,030h,036h,0a5h,038h
DB	0bfh,040h,0a3h,09eh,081h,0f3h,0d7h,0fbh
DB	07ch,0e3h,039h,082h,09bh,02fh,0ffh,087h
DB	034h,08eh,043h,044h,0c4h,0deh,0e9h,0cbh
DB	054h,07bh,094h,032h,0a6h,0c2h,023h,03dh
DB	0eeh,04ch,095h,00bh,042h,0fah,0c3h,04eh
DB	008h,02eh,0a1h,066h,028h,0d9h,024h,0b2h
DB	076h,05bh,0a2h,049h,06dh,08bh,0d1h,025h
DB	072h,0f8h,0f6h,064h,086h,068h,098h,016h
DB	0d4h,0a4h,05ch,0cch,05dh,065h,0b6h,092h
DB	06ch,070h,048h,050h,0fdh,0edh,0b9h,0dah
DB	05eh,015h,046h,057h,0a7h,08dh,09dh,084h
DB	090h,0d8h,0abh,000h,08ch,0bch,0d3h,00ah
DB	0f7h,0e4h,058h,005h,0b8h,0b3h,045h,006h
DB	0d0h,02ch,01eh,08fh,0cah,03fh,00fh,002h
DB	0c1h,0afh,0bdh,003h,001h,013h,08ah,06bh
DB	03ah,091h,011h,041h,04fh,067h,0dch,0eah
DB	097h,0f2h,0cfh,0ceh,0f0h,0b4h,0e6h,073h
DB	096h,0ach,074h,022h,0e7h,0adh,035h,085h
DB	0e2h,0f9h,037h,0e8h,01ch,075h,0dfh,06eh
DB	047h,0f1h,01ah,071h,01dh,029h,0c5h,089h
DB	06fh,0b7h,062h,00eh,0aah,018h,0beh,01bh
DB	0fch,056h,03eh,04bh,0c6h,0d2h,079h,020h
DB	09ah,0dbh,0c0h,0feh,078h,0cdh,05ah,0f4h
DB	01fh,0ddh,0a8h,033h,088h,007h,0c7h,031h
DB	0b1h,012h,010h,059h,027h,080h,0ech,05fh
DB	060h,051h,07fh,0a9h,019h,0b5h,04ah,00dh
DB	02dh,0e5h,07ah,09fh,093h,0c9h,09ch,0efh
DB	0a0h,0e0h,03bh,04dh,0aeh,02ah,0f5h,0b0h
DB	0c8h,0ebh,0bbh,03ch,083h,053h,099h,061h
DB	017h,02bh,004h,07eh,0bah,077h,0d6h,026h
DB	0e1h,069h,014h,063h,055h,021h,00ch,07dh
	DD	080808080h,080808080h,0fefefefeh,0fefefefeh
	DD	01b1b1b1bh,01b1b1b1bh,0,0
DB	65,69,83,32,102,111,114,32,120,56,54,95,54,52,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
ALIGN	64
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
block_se_handler	PROC PRIVATE
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
	jb	$L$in_block_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_block_prologue

	mov	rax,QWORD PTR[24+rax]
	lea	rax,QWORD PTR[48+rax]

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

$L$in_block_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	jmp	$L$common_seh_exit
block_se_handler	ENDP


ALIGN	16
key_se_handler	PROC PRIVATE
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
	jb	$L$in_key_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_key_prologue

	lea	rax,QWORD PTR[56+rax]

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

$L$in_key_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	jmp	$L$common_seh_exit
key_se_handler	ENDP


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
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	lea	r10,QWORD PTR[$L$cbc_prologue]
	cmp	rbx,r10
	jb	$L$in_cbc_prologue

	lea	r10,QWORD PTR[$L$cbc_fast_body]
	cmp	rbx,r10
	jb	$L$in_cbc_frame_setup

	lea	r10,QWORD PTR[$L$cbc_slow_prologue]
	cmp	rbx,r10
	jb	$L$in_cbc_body

	lea	r10,QWORD PTR[$L$cbc_slow_body]
	cmp	rbx,r10
	jb	$L$in_cbc_frame_setup

$L$in_cbc_body::
	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$cbc_epilogue]
	cmp	rbx,r10
	jae	$L$in_cbc_prologue

	lea	rax,QWORD PTR[8+rax]

	lea	r10,QWORD PTR[$L$cbc_popfq]
	cmp	rbx,r10
	jae	$L$in_cbc_prologue

	mov	rax,QWORD PTR[8+rax]
	lea	rax,QWORD PTR[56+rax]

$L$in_cbc_frame_setup::
	mov	rbx,QWORD PTR[((-16))+rax]
	mov	rbp,QWORD PTR[((-24))+rax]
	mov	r12,QWORD PTR[((-32))+rax]
	mov	r13,QWORD PTR[((-40))+rax]
	mov	r14,QWORD PTR[((-48))+rax]
	mov	r15,QWORD PTR[((-56))+rax]
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
cbc_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_AES_encrypt
	DD	imagerel $L$SEH_end_AES_encrypt
	DD	imagerel $L$SEH_info_AES_encrypt

	DD	imagerel $L$SEH_begin_AES_decrypt
	DD	imagerel $L$SEH_end_AES_decrypt
	DD	imagerel $L$SEH_info_AES_decrypt

	DD	imagerel $L$SEH_begin_private_AES_set_encrypt_key
	DD	imagerel $L$SEH_end_private_AES_set_encrypt_key
	DD	imagerel $L$SEH_info_private_AES_set_encrypt_key

	DD	imagerel $L$SEH_begin_private_AES_set_decrypt_key
	DD	imagerel $L$SEH_end_private_AES_set_decrypt_key
	DD	imagerel $L$SEH_info_private_AES_set_decrypt_key

	DD	imagerel $L$SEH_begin_AES_cbc_encrypt
	DD	imagerel $L$SEH_end_AES_cbc_encrypt
	DD	imagerel $L$SEH_info_AES_cbc_encrypt

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_AES_encrypt::
DB	9,0,0,0
	DD	imagerel block_se_handler
	DD	imagerel $L$enc_prologue,imagerel $L$enc_epilogue
$L$SEH_info_AES_decrypt::
DB	9,0,0,0
	DD	imagerel block_se_handler
	DD	imagerel $L$dec_prologue,imagerel $L$dec_epilogue
$L$SEH_info_private_AES_set_encrypt_key::
DB	9,0,0,0
	DD	imagerel key_se_handler
	DD	imagerel $L$enc_key_prologue,imagerel $L$enc_key_epilogue
$L$SEH_info_private_AES_set_decrypt_key::
DB	9,0,0,0
	DD	imagerel key_se_handler
	DD	imagerel $L$dec_key_prologue,imagerel $L$dec_key_epilogue
$L$SEH_info_AES_cbc_encrypt::
DB	9,0,0,0
	DD	imagerel cbc_se_handler

.xdata	ENDS
END
