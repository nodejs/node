default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64

global	padlock_capability

ALIGN	16
padlock_capability:
	mov	r8,rbx
	xor	eax,eax
	cpuid
	xor	eax,eax
	cmp	ebx,0x746e6543
	jne	NEAR $L$zhaoxin
	cmp	edx,0x48727561
	jne	NEAR $L$noluck
	cmp	ecx,0x736c7561
	jne	NEAR $L$noluck
	jmp	NEAR $L$zhaoxinEnd
$L$zhaoxin:
	cmp	ebx,0x68532020
	jne	NEAR $L$noluck
	cmp	edx,0x68676e61
	jne	NEAR $L$noluck
	cmp	ecx,0x20206961
	jne	NEAR $L$noluck
$L$zhaoxinEnd:
	mov	eax,0xC0000000
	cpuid
	mov	edx,eax
	xor	eax,eax
	cmp	edx,0xC0000001
	jb	NEAR $L$noluck
	mov	eax,0xC0000001
	cpuid
	mov	eax,edx
	and	eax,0xffffffef
	or	eax,0x10
$L$noluck:
	mov	rbx,r8
	DB	0F3h,0C3h		;repret


global	padlock_key_bswap

ALIGN	16
padlock_key_bswap:
	mov	edx,DWORD[240+rcx]
	inc	edx
	shl	edx,2
$L$bswap_loop:
	mov	eax,DWORD[rcx]
	bswap	eax
	mov	DWORD[rcx],eax
	lea	rcx,[4+rcx]
	sub	edx,1
	jnz	NEAR $L$bswap_loop
	DB	0F3h,0C3h		;repret


global	padlock_verify_context

ALIGN	16
padlock_verify_context:
	mov	rdx,rcx
	pushf
	lea	rax,[$L$padlock_saved_context]
	call	_padlock_verify_ctx
	lea	rsp,[8+rsp]
	DB	0F3h,0C3h		;repret



ALIGN	16
_padlock_verify_ctx:
	mov	r8,QWORD[8+rsp]
	bt	r8,30
	jnc	NEAR $L$verified
	cmp	rdx,QWORD[rax]
	je	NEAR $L$verified
	pushf
	popf
$L$verified:
	mov	QWORD[rax],rdx
	DB	0F3h,0C3h		;repret


global	padlock_reload_key

ALIGN	16
padlock_reload_key:
	pushf
	popf
	DB	0F3h,0C3h		;repret


global	padlock_aes_block

ALIGN	16
padlock_aes_block:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_aes_block:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	r8,rbx
	mov	rcx,1
	lea	rbx,[32+rdx]
	lea	rdx,[16+rdx]
DB	0xf3,0x0f,0xa7,0xc8
	mov	rbx,r8
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_aes_block:

global	padlock_xstore

ALIGN	16
padlock_xstore:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_xstore:
	mov	rdi,rcx
	mov	rsi,rdx


	mov	edx,esi
DB	0x0f,0xa7,0xc0
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_xstore:

global	padlock_sha1_oneshot

ALIGN	16
padlock_sha1_oneshot:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_sha1_oneshot:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,rdx
	mov	rdx,rdi
	movups	xmm0,XMMWORD[rdi]
	sub	rsp,128+8
	mov	eax,DWORD[16+rdi]
	movaps	XMMWORD[rsp],xmm0
	mov	rdi,rsp
	mov	DWORD[16+rsp],eax
	xor	rax,rax
DB	0xf3,0x0f,0xa6,0xc8
	movaps	xmm0,XMMWORD[rsp]
	mov	eax,DWORD[16+rsp]
	add	rsp,128+8
	movups	XMMWORD[rdx],xmm0
	mov	DWORD[16+rdx],eax
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_sha1_oneshot:

global	padlock_sha1_blocks

ALIGN	16
padlock_sha1_blocks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_sha1_blocks:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,rdx
	mov	rdx,rdi
	movups	xmm0,XMMWORD[rdi]
	sub	rsp,128+8
	mov	eax,DWORD[16+rdi]
	movaps	XMMWORD[rsp],xmm0
	mov	rdi,rsp
	mov	DWORD[16+rsp],eax
	mov	rax,-1
DB	0xf3,0x0f,0xa6,0xc8
	movaps	xmm0,XMMWORD[rsp]
	mov	eax,DWORD[16+rsp]
	add	rsp,128+8
	movups	XMMWORD[rdx],xmm0
	mov	DWORD[16+rdx],eax
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_sha1_blocks:

global	padlock_sha256_oneshot

ALIGN	16
padlock_sha256_oneshot:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_sha256_oneshot:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,rdx
	mov	rdx,rdi
	movups	xmm0,XMMWORD[rdi]
	sub	rsp,128+8
	movups	xmm1,XMMWORD[16+rdi]
	movaps	XMMWORD[rsp],xmm0
	mov	rdi,rsp
	movaps	XMMWORD[16+rsp],xmm1
	xor	rax,rax
DB	0xf3,0x0f,0xa6,0xd0
	movaps	xmm0,XMMWORD[rsp]
	movaps	xmm1,XMMWORD[16+rsp]
	add	rsp,128+8
	movups	XMMWORD[rdx],xmm0
	movups	XMMWORD[16+rdx],xmm1
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_sha256_oneshot:

global	padlock_sha256_blocks

ALIGN	16
padlock_sha256_blocks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_sha256_blocks:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,rdx
	mov	rdx,rdi
	movups	xmm0,XMMWORD[rdi]
	sub	rsp,128+8
	movups	xmm1,XMMWORD[16+rdi]
	movaps	XMMWORD[rsp],xmm0
	mov	rdi,rsp
	movaps	XMMWORD[16+rsp],xmm1
	mov	rax,-1
DB	0xf3,0x0f,0xa6,0xd0
	movaps	xmm0,XMMWORD[rsp]
	movaps	xmm1,XMMWORD[16+rsp]
	add	rsp,128+8
	movups	XMMWORD[rdx],xmm0
	movups	XMMWORD[16+rdx],xmm1
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_sha256_blocks:

global	padlock_sha512_blocks

ALIGN	16
padlock_sha512_blocks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_sha512_blocks:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,rdx
	mov	rdx,rdi
	movups	xmm0,XMMWORD[rdi]
	sub	rsp,128+8
	movups	xmm1,XMMWORD[16+rdi]
	movups	xmm2,XMMWORD[32+rdi]
	movups	xmm3,XMMWORD[48+rdi]
	movaps	XMMWORD[rsp],xmm0
	mov	rdi,rsp
	movaps	XMMWORD[16+rsp],xmm1
	movaps	XMMWORD[32+rsp],xmm2
	movaps	XMMWORD[48+rsp],xmm3
DB	0xf3,0x0f,0xa6,0xe0
	movaps	xmm0,XMMWORD[rsp]
	movaps	xmm1,XMMWORD[16+rsp]
	movaps	xmm2,XMMWORD[32+rsp]
	movaps	xmm3,XMMWORD[48+rsp]
	add	rsp,128+8
	movups	XMMWORD[rdx],xmm0
	movups	XMMWORD[16+rdx],xmm1
	movups	XMMWORD[32+rdx],xmm2
	movups	XMMWORD[48+rdx],xmm3
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_sha512_blocks:
global	padlock_ecb_encrypt

ALIGN	16
padlock_ecb_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_ecb_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx

	xor	eax,eax
	test	rdx,15
	jnz	NEAR $L$ecb_abort
	test	rcx,15
	jnz	NEAR $L$ecb_abort
	lea	rax,[$L$padlock_saved_context]
	pushf
	cld
	call	_padlock_verify_ctx
	lea	rdx,[16+rdx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD[rdx],32
	jnz	NEAR $L$ecb_aligned
	test	rdi,0x0f
	setz	al
	test	rsi,0x0f
	setz	bl
	test	eax,ebx
	jnz	NEAR $L$ecb_aligned
	neg	rax
	mov	rbx,512
	not	rax
	lea	rbp,[rsp]
	cmp	rcx,rbx
	cmovc	rbx,rcx
	and	rax,rbx
	mov	rbx,rcx
	neg	rax
	and	rbx,512-1
	lea	rsp,[rbp*1+rax]
	mov	rax,512
	cmovz	rbx,rax
	cmp	rcx,rbx
	ja	NEAR $L$ecb_loop
	mov	rax,rsi
	cmp	rbp,rsp
	cmove	rax,rdi
	add	rax,rcx
	neg	rax
	and	rax,0xfff
	cmp	rax,128
	mov	rax,-128
	cmovae	rax,rbx
	and	rbx,rax
	jz	NEAR $L$ecb_unaligned_tail
	jmp	NEAR $L$ecb_loop
ALIGN	16
$L$ecb_loop:
	cmp	rbx,rcx
	cmova	rbx,rcx
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx
	test	rdi,0x0f
	cmovnz	rdi,rsp
	test	rsi,0x0f
	jz	NEAR $L$ecb_inp_aligned
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
	mov	rcx,rbx
	mov	rsi,rdi
$L$ecb_inp_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,200
	mov	rdi,r8
	mov	rbx,r11
	test	rdi,0x0f
	jz	NEAR $L$ecb_out_aligned
	mov	rcx,rbx
	lea	rsi,[rsp]
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
$L$ecb_out_aligned:
	mov	rsi,r9
	mov	rcx,r10
	add	rdi,rbx
	add	rsi,rbx
	sub	rcx,rbx
	mov	rbx,512
	jz	NEAR $L$ecb_break
	cmp	rcx,rbx
	jae	NEAR $L$ecb_loop
$L$ecb_unaligned_tail:
	xor	eax,eax
	cmp	rbp,rsp
	cmove	rax,rcx
	mov	r8,rdi
	mov	rbx,rcx
	sub	rsp,rax
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	mov	rsi,rsp
	mov	rdi,r8
	mov	rcx,rbx
	jmp	NEAR $L$ecb_loop
ALIGN	16
$L$ecb_break:
	cmp	rsp,rbp
	je	NEAR $L$ecb_done

	pxor	xmm0,xmm0
	lea	rax,[rsp]
$L$ecb_bzero:
	movaps	XMMWORD[rax],xmm0
	lea	rax,[16+rax]
	cmp	rbp,rax
	ja	NEAR $L$ecb_bzero

$L$ecb_done:
	lea	rsp,[rbp]
	jmp	NEAR $L$ecb_exit

ALIGN	16
$L$ecb_aligned:
	lea	rbp,[rcx*1+rsi]
	neg	rbp
	and	rbp,0xfff
	xor	eax,eax
	cmp	rbp,128
	mov	rbp,128-1
	cmovae	rbp,rax
	and	rbp,rcx
	sub	rcx,rbp
	jz	NEAR $L$ecb_aligned_tail
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,200
	test	rbp,rbp
	jz	NEAR $L$ecb_exit

$L$ecb_aligned_tail:
	mov	r8,rdi
	mov	rbx,rbp
	mov	rcx,rbp
	lea	rbp,[rsp]
	sub	rsp,rcx
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	lea	rdi,[r8]
	lea	rsi,[rsp]
	mov	rcx,rbx
	jmp	NEAR $L$ecb_loop
$L$ecb_exit:
	mov	eax,1
	lea	rsp,[8+rsp]
$L$ecb_abort:
	pop	rbx
	pop	rbp
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_ecb_encrypt:
global	padlock_cbc_encrypt

ALIGN	16
padlock_cbc_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_cbc_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx

	xor	eax,eax
	test	rdx,15
	jnz	NEAR $L$cbc_abort
	test	rcx,15
	jnz	NEAR $L$cbc_abort
	lea	rax,[$L$padlock_saved_context]
	pushf
	cld
	call	_padlock_verify_ctx
	lea	rdx,[16+rdx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD[rdx],32
	jnz	NEAR $L$cbc_aligned
	test	rdi,0x0f
	setz	al
	test	rsi,0x0f
	setz	bl
	test	eax,ebx
	jnz	NEAR $L$cbc_aligned
	neg	rax
	mov	rbx,512
	not	rax
	lea	rbp,[rsp]
	cmp	rcx,rbx
	cmovc	rbx,rcx
	and	rax,rbx
	mov	rbx,rcx
	neg	rax
	and	rbx,512-1
	lea	rsp,[rbp*1+rax]
	mov	rax,512
	cmovz	rbx,rax
	cmp	rcx,rbx
	ja	NEAR $L$cbc_loop
	mov	rax,rsi
	cmp	rbp,rsp
	cmove	rax,rdi
	add	rax,rcx
	neg	rax
	and	rax,0xfff
	cmp	rax,64
	mov	rax,-64
	cmovae	rax,rbx
	and	rbx,rax
	jz	NEAR $L$cbc_unaligned_tail
	jmp	NEAR $L$cbc_loop
ALIGN	16
$L$cbc_loop:
	cmp	rbx,rcx
	cmova	rbx,rcx
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx
	test	rdi,0x0f
	cmovnz	rdi,rsp
	test	rsi,0x0f
	jz	NEAR $L$cbc_inp_aligned
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
	mov	rcx,rbx
	mov	rsi,rdi
$L$cbc_inp_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,208
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
	mov	rdi,r8
	mov	rbx,r11
	test	rdi,0x0f
	jz	NEAR $L$cbc_out_aligned
	mov	rcx,rbx
	lea	rsi,[rsp]
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
$L$cbc_out_aligned:
	mov	rsi,r9
	mov	rcx,r10
	add	rdi,rbx
	add	rsi,rbx
	sub	rcx,rbx
	mov	rbx,512
	jz	NEAR $L$cbc_break
	cmp	rcx,rbx
	jae	NEAR $L$cbc_loop
$L$cbc_unaligned_tail:
	xor	eax,eax
	cmp	rbp,rsp
	cmove	rax,rcx
	mov	r8,rdi
	mov	rbx,rcx
	sub	rsp,rax
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	mov	rsi,rsp
	mov	rdi,r8
	mov	rcx,rbx
	jmp	NEAR $L$cbc_loop
ALIGN	16
$L$cbc_break:
	cmp	rsp,rbp
	je	NEAR $L$cbc_done

	pxor	xmm0,xmm0
	lea	rax,[rsp]
$L$cbc_bzero:
	movaps	XMMWORD[rax],xmm0
	lea	rax,[16+rax]
	cmp	rbp,rax
	ja	NEAR $L$cbc_bzero

$L$cbc_done:
	lea	rsp,[rbp]
	jmp	NEAR $L$cbc_exit

ALIGN	16
$L$cbc_aligned:
	lea	rbp,[rcx*1+rsi]
	neg	rbp
	and	rbp,0xfff
	xor	eax,eax
	cmp	rbp,64
	mov	rbp,64-1
	cmovae	rbp,rax
	and	rbp,rcx
	sub	rcx,rbp
	jz	NEAR $L$cbc_aligned_tail
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,208
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
	test	rbp,rbp
	jz	NEAR $L$cbc_exit

$L$cbc_aligned_tail:
	mov	r8,rdi
	mov	rbx,rbp
	mov	rcx,rbp
	lea	rbp,[rsp]
	sub	rsp,rcx
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	lea	rdi,[r8]
	lea	rsi,[rsp]
	mov	rcx,rbx
	jmp	NEAR $L$cbc_loop
$L$cbc_exit:
	mov	eax,1
	lea	rsp,[8+rsp]
$L$cbc_abort:
	pop	rbx
	pop	rbp
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_cbc_encrypt:
global	padlock_cfb_encrypt

ALIGN	16
padlock_cfb_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_cfb_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx

	xor	eax,eax
	test	rdx,15
	jnz	NEAR $L$cfb_abort
	test	rcx,15
	jnz	NEAR $L$cfb_abort
	lea	rax,[$L$padlock_saved_context]
	pushf
	cld
	call	_padlock_verify_ctx
	lea	rdx,[16+rdx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD[rdx],32
	jnz	NEAR $L$cfb_aligned
	test	rdi,0x0f
	setz	al
	test	rsi,0x0f
	setz	bl
	test	eax,ebx
	jnz	NEAR $L$cfb_aligned
	neg	rax
	mov	rbx,512
	not	rax
	lea	rbp,[rsp]
	cmp	rcx,rbx
	cmovc	rbx,rcx
	and	rax,rbx
	mov	rbx,rcx
	neg	rax
	and	rbx,512-1
	lea	rsp,[rbp*1+rax]
	mov	rax,512
	cmovz	rbx,rax
	jmp	NEAR $L$cfb_loop
ALIGN	16
$L$cfb_loop:
	cmp	rbx,rcx
	cmova	rbx,rcx
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx
	test	rdi,0x0f
	cmovnz	rdi,rsp
	test	rsi,0x0f
	jz	NEAR $L$cfb_inp_aligned
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
	mov	rcx,rbx
	mov	rsi,rdi
$L$cfb_inp_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,224
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
	mov	rdi,r8
	mov	rbx,r11
	test	rdi,0x0f
	jz	NEAR $L$cfb_out_aligned
	mov	rcx,rbx
	lea	rsi,[rsp]
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
$L$cfb_out_aligned:
	mov	rsi,r9
	mov	rcx,r10
	add	rdi,rbx
	add	rsi,rbx
	sub	rcx,rbx
	mov	rbx,512
	jnz	NEAR $L$cfb_loop
	cmp	rsp,rbp
	je	NEAR $L$cfb_done

	pxor	xmm0,xmm0
	lea	rax,[rsp]
$L$cfb_bzero:
	movaps	XMMWORD[rax],xmm0
	lea	rax,[16+rax]
	cmp	rbp,rax
	ja	NEAR $L$cfb_bzero

$L$cfb_done:
	lea	rsp,[rbp]
	jmp	NEAR $L$cfb_exit

ALIGN	16
$L$cfb_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,224
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
$L$cfb_exit:
	mov	eax,1
	lea	rsp,[8+rsp]
$L$cfb_abort:
	pop	rbx
	pop	rbp
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_cfb_encrypt:
global	padlock_ofb_encrypt

ALIGN	16
padlock_ofb_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_ofb_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx

	xor	eax,eax
	test	rdx,15
	jnz	NEAR $L$ofb_abort
	test	rcx,15
	jnz	NEAR $L$ofb_abort
	lea	rax,[$L$padlock_saved_context]
	pushf
	cld
	call	_padlock_verify_ctx
	lea	rdx,[16+rdx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD[rdx],32
	jnz	NEAR $L$ofb_aligned
	test	rdi,0x0f
	setz	al
	test	rsi,0x0f
	setz	bl
	test	eax,ebx
	jnz	NEAR $L$ofb_aligned
	neg	rax
	mov	rbx,512
	not	rax
	lea	rbp,[rsp]
	cmp	rcx,rbx
	cmovc	rbx,rcx
	and	rax,rbx
	mov	rbx,rcx
	neg	rax
	and	rbx,512-1
	lea	rsp,[rbp*1+rax]
	mov	rax,512
	cmovz	rbx,rax
	jmp	NEAR $L$ofb_loop
ALIGN	16
$L$ofb_loop:
	cmp	rbx,rcx
	cmova	rbx,rcx
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx
	test	rdi,0x0f
	cmovnz	rdi,rsp
	test	rsi,0x0f
	jz	NEAR $L$ofb_inp_aligned
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
	mov	rcx,rbx
	mov	rsi,rdi
$L$ofb_inp_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,232
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
	mov	rdi,r8
	mov	rbx,r11
	test	rdi,0x0f
	jz	NEAR $L$ofb_out_aligned
	mov	rcx,rbx
	lea	rsi,[rsp]
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
$L$ofb_out_aligned:
	mov	rsi,r9
	mov	rcx,r10
	add	rdi,rbx
	add	rsi,rbx
	sub	rcx,rbx
	mov	rbx,512
	jnz	NEAR $L$ofb_loop
	cmp	rsp,rbp
	je	NEAR $L$ofb_done

	pxor	xmm0,xmm0
	lea	rax,[rsp]
$L$ofb_bzero:
	movaps	XMMWORD[rax],xmm0
	lea	rax,[16+rax]
	cmp	rbp,rax
	ja	NEAR $L$ofb_bzero

$L$ofb_done:
	lea	rsp,[rbp]
	jmp	NEAR $L$ofb_exit

ALIGN	16
$L$ofb_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,232
	movdqa	xmm0,XMMWORD[rax]
	movdqa	XMMWORD[(-16)+rdx],xmm0
$L$ofb_exit:
	mov	eax,1
	lea	rsp,[8+rsp]
$L$ofb_abort:
	pop	rbx
	pop	rbp
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_ofb_encrypt:
global	padlock_ctr32_encrypt

ALIGN	16
padlock_ctr32_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_padlock_ctr32_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx

	xor	eax,eax
	test	rdx,15
	jnz	NEAR $L$ctr32_abort
	test	rcx,15
	jnz	NEAR $L$ctr32_abort
	lea	rax,[$L$padlock_saved_context]
	pushf
	cld
	call	_padlock_verify_ctx
	lea	rdx,[16+rdx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD[rdx],32
	jnz	NEAR $L$ctr32_aligned
	test	rdi,0x0f
	setz	al
	test	rsi,0x0f
	setz	bl
	test	eax,ebx
	jnz	NEAR $L$ctr32_aligned
	neg	rax
	mov	rbx,512
	not	rax
	lea	rbp,[rsp]
	cmp	rcx,rbx
	cmovc	rbx,rcx
	and	rax,rbx
	mov	rbx,rcx
	neg	rax
	and	rbx,512-1
	lea	rsp,[rbp*1+rax]
	mov	rax,512
	cmovz	rbx,rax
$L$ctr32_reenter:
	mov	eax,DWORD[((-4))+rdx]
	bswap	eax
	neg	eax
	and	eax,31
	mov	rbx,512
	shl	eax,4
	cmovz	rax,rbx
	cmp	rcx,rax
	cmova	rbx,rax
	cmovbe	rbx,rcx
	cmp	rcx,rbx
	ja	NEAR $L$ctr32_loop
	mov	rax,rsi
	cmp	rbp,rsp
	cmove	rax,rdi
	add	rax,rcx
	neg	rax
	and	rax,0xfff
	cmp	rax,32
	mov	rax,-32
	cmovae	rax,rbx
	and	rbx,rax
	jz	NEAR $L$ctr32_unaligned_tail
	jmp	NEAR $L$ctr32_loop
ALIGN	16
$L$ctr32_loop:
	cmp	rbx,rcx
	cmova	rbx,rcx
	mov	r8,rdi
	mov	r9,rsi
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx
	test	rdi,0x0f
	cmovnz	rdi,rsp
	test	rsi,0x0f
	jz	NEAR $L$ctr32_inp_aligned
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
	mov	rcx,rbx
	mov	rsi,rdi
$L$ctr32_inp_aligned:
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,216
	mov	eax,DWORD[((-4))+rdx]
	test	eax,0xffff0000
	jnz	NEAR $L$ctr32_no_carry
	bswap	eax
	add	eax,0x10000
	bswap	eax
	mov	DWORD[((-4))+rdx],eax
$L$ctr32_no_carry:
	mov	rdi,r8
	mov	rbx,r11
	test	rdi,0x0f
	jz	NEAR $L$ctr32_out_aligned
	mov	rcx,rbx
	lea	rsi,[rsp]
	shr	rcx,3
DB	0xf3,0x48,0xa5
	sub	rdi,rbx
$L$ctr32_out_aligned:
	mov	rsi,r9
	mov	rcx,r10
	add	rdi,rbx
	add	rsi,rbx
	sub	rcx,rbx
	mov	rbx,512
	jz	NEAR $L$ctr32_break
	cmp	rcx,rbx
	jae	NEAR $L$ctr32_loop
	mov	rbx,rcx
	mov	rax,rsi
	cmp	rbp,rsp
	cmove	rax,rdi
	add	rax,rcx
	neg	rax
	and	rax,0xfff
	cmp	rax,32
	mov	rax,-32
	cmovae	rax,rbx
	and	rbx,rax
	jnz	NEAR $L$ctr32_loop
$L$ctr32_unaligned_tail:
	xor	eax,eax
	cmp	rbp,rsp
	cmove	rax,rcx
	mov	r8,rdi
	mov	rbx,rcx
	sub	rsp,rax
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	mov	rsi,rsp
	mov	rdi,r8
	mov	rcx,rbx
	jmp	NEAR $L$ctr32_loop
ALIGN	16
$L$ctr32_break:
	cmp	rsp,rbp
	je	NEAR $L$ctr32_done

	pxor	xmm0,xmm0
	lea	rax,[rsp]
$L$ctr32_bzero:
	movaps	XMMWORD[rax],xmm0
	lea	rax,[16+rax]
	cmp	rbp,rax
	ja	NEAR $L$ctr32_bzero

$L$ctr32_done:
	lea	rsp,[rbp]
	jmp	NEAR $L$ctr32_exit

ALIGN	16
$L$ctr32_aligned:
	mov	eax,DWORD[((-4))+rdx]
	bswap	eax
	neg	eax
	and	eax,0xffff
	mov	rbx,1048576
	shl	eax,4
	cmovz	rax,rbx
	cmp	rcx,rax
	cmova	rbx,rax
	cmovbe	rbx,rcx
	jbe	NEAR $L$ctr32_aligned_skip

$L$ctr32_aligned_loop:
	mov	r10,rcx
	mov	rcx,rbx
	mov	r11,rbx

	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,216

	mov	eax,DWORD[((-4))+rdx]
	bswap	eax
	add	eax,0x10000
	bswap	eax
	mov	DWORD[((-4))+rdx],eax

	mov	rcx,r10
	sub	rcx,r11
	mov	rbx,1048576
	jz	NEAR $L$ctr32_exit
	cmp	rcx,rbx
	jae	NEAR $L$ctr32_aligned_loop

$L$ctr32_aligned_skip:
	lea	rbp,[rcx*1+rsi]
	neg	rbp
	and	rbp,0xfff
	xor	eax,eax
	cmp	rbp,32
	mov	rbp,32-1
	cmovae	rbp,rax
	and	rbp,rcx
	sub	rcx,rbp
	jz	NEAR $L$ctr32_aligned_tail
	lea	rax,[((-16))+rdx]
	lea	rbx,[16+rdx]
	shr	rcx,4
DB	0xf3,0x0f,0xa7,216
	test	rbp,rbp
	jz	NEAR $L$ctr32_exit

$L$ctr32_aligned_tail:
	mov	r8,rdi
	mov	rbx,rbp
	mov	rcx,rbp
	lea	rbp,[rsp]
	sub	rsp,rcx
	shr	rcx,3
	lea	rdi,[rsp]
DB	0xf3,0x48,0xa5
	lea	rdi,[r8]
	lea	rsi,[rsp]
	mov	rcx,rbx
	jmp	NEAR $L$ctr32_loop
$L$ctr32_exit:
	mov	eax,1
	lea	rsp,[8+rsp]
$L$ctr32_abort:
	pop	rbx
	pop	rbp
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_padlock_ctr32_encrypt:
DB	86,73,65,32,80,97,100,108,111,99,107,32,120,56,54,95
DB	54,52,32,109,111,100,117,108,101,44,32,67,82,89,80,84
DB	79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64
DB	111,112,101,110,115,115,108,46,111,114,103,62,0
ALIGN	16
section	.data data align=8

ALIGN	8
$L$padlock_saved_context:
	DQ	0
