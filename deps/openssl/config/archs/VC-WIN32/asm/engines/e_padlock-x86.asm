%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
global	_padlock_capability
align	16
_padlock_capability:
L$_padlock_capability_begin:
	push	ebx
	pushfd
	pop	eax
	mov	ecx,eax
	xor	eax,2097152
	push	eax
	popfd
	pushfd
	pop	eax
	xor	ecx,eax
	xor	eax,eax
	bt	ecx,21
	jnc	NEAR L$000noluck
	cpuid
	xor	eax,eax
	cmp	ebx,0x746e6543
	jne	NEAR L$001zhaoxin
	cmp	edx,0x48727561
	jne	NEAR L$000noluck
	cmp	ecx,0x736c7561
	jne	NEAR L$000noluck
	jmp	NEAR L$002zhaoxinEnd
L$001zhaoxin:
	cmp	ebx,0x68532020
	jne	NEAR L$000noluck
	cmp	edx,0x68676e61
	jne	NEAR L$000noluck
	cmp	ecx,0x20206961
	jne	NEAR L$000noluck
L$002zhaoxinEnd:
	mov	eax,3221225472
	cpuid
	mov	edx,eax
	xor	eax,eax
	cmp	edx,3221225473
	jb	NEAR L$000noluck
	mov	eax,1
	cpuid
	or	eax,15
	xor	ebx,ebx
	and	eax,4095
	cmp	eax,1791
	sete	bl
	mov	eax,3221225473
	push	ebx
	cpuid
	pop	ebx
	mov	eax,edx
	shl	ebx,4
	and	eax,4294967279
	or	eax,ebx
L$000noluck:
	pop	ebx
	ret
global	_padlock_key_bswap
align	16
_padlock_key_bswap:
L$_padlock_key_bswap_begin:
	mov	edx,DWORD [4+esp]
	mov	ecx,DWORD [240+edx]
L$003bswap_loop:
	mov	eax,DWORD [edx]
	bswap	eax
	mov	DWORD [edx],eax
	lea	edx,[4+edx]
	sub	ecx,1
	jnz	NEAR L$003bswap_loop
	ret
global	_padlock_verify_context
align	16
_padlock_verify_context:
L$_padlock_verify_context_begin:
	mov	edx,DWORD [4+esp]
	lea	eax,[L$padlock_saved_context]
	pushfd
	call	__padlock_verify_ctx
L$004verify_pic_point:
	lea	esp,[4+esp]
	ret
align	16
__padlock_verify_ctx:
	bt	DWORD [4+esp],30
	jnc	NEAR L$005verified
	cmp	edx,DWORD [eax]
	je	NEAR L$005verified
	pushfd
	popfd
L$005verified:
	mov	DWORD [eax],edx
	ret
global	_padlock_reload_key
align	16
_padlock_reload_key:
L$_padlock_reload_key_begin:
	pushfd
	popfd
	ret
global	_padlock_aes_block
align	16
_padlock_aes_block:
L$_padlock_aes_block_begin:
	push	edi
	push	esi
	push	ebx
	mov	edi,DWORD [16+esp]
	mov	esi,DWORD [20+esp]
	mov	edx,DWORD [24+esp]
	mov	ecx,1
	lea	ebx,[32+edx]
	lea	edx,[16+edx]
db	243,15,167,200
	pop	ebx
	pop	esi
	pop	edi
	ret
global	_padlock_ecb_encrypt
align	16
_padlock_ecb_encrypt:
L$_padlock_ecb_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	mov	ecx,DWORD [32+esp]
	test	edx,15
	jnz	NEAR L$006ecb_abort
	test	ecx,15
	jnz	NEAR L$006ecb_abort
	lea	eax,[L$padlock_saved_context]
	pushfd
	cld
	call	__padlock_verify_ctx
L$007ecb_pic_point:
	lea	edx,[16+edx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD [edx],32
	jnz	NEAR L$008ecb_aligned
	test	edi,15
	setz	al
	test	esi,15
	setz	bl
	test	eax,ebx
	jnz	NEAR L$008ecb_aligned
	neg	eax
	mov	ebx,512
	not	eax
	lea	ebp,[esp-24]
	cmp	ecx,ebx
	cmovc	ebx,ecx
	and	eax,ebx
	mov	ebx,ecx
	neg	eax
	and	ebx,511
	lea	esp,[ebp*1+eax]
	mov	eax,512
	cmovz	ebx,eax
	mov	eax,ebp
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	cmp	ecx,ebx
	ja	NEAR L$009ecb_loop
	mov	eax,esi
	cmp	ebp,esp
	cmove	eax,edi
	add	eax,ecx
	neg	eax
	and	eax,4095
	cmp	eax,128
	mov	eax,-128
	cmovae	eax,ebx
	and	ebx,eax
	jz	NEAR L$010ecb_unaligned_tail
	jmp	NEAR L$009ecb_loop
align	16
L$009ecb_loop:
	mov	DWORD [ebp],edi
	mov	DWORD [4+ebp],esi
	mov	DWORD [8+ebp],ecx
	mov	ecx,ebx
	mov	DWORD [12+ebp],ebx
	test	edi,15
	cmovnz	edi,esp
	test	esi,15
	jz	NEAR L$011ecb_inp_aligned
	shr	ecx,2
db	243,165
	sub	edi,ebx
	mov	ecx,ebx
	mov	esi,edi
L$011ecb_inp_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,200
	mov	edi,DWORD [ebp]
	mov	ebx,DWORD [12+ebp]
	test	edi,15
	jz	NEAR L$012ecb_out_aligned
	mov	ecx,ebx
	lea	esi,[esp]
	shr	ecx,2
db	243,165
	sub	edi,ebx
L$012ecb_out_aligned:
	mov	esi,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	add	edi,ebx
	add	esi,ebx
	sub	ecx,ebx
	mov	ebx,512
	jz	NEAR L$013ecb_break
	cmp	ecx,ebx
	jae	NEAR L$009ecb_loop
L$010ecb_unaligned_tail:
	xor	eax,eax
	cmp	esp,ebp
	cmove	eax,ecx
	sub	esp,eax
	mov	eax,edi
	mov	ebx,ecx
	shr	ecx,2
	lea	edi,[esp]
db	243,165
	mov	esi,esp
	mov	edi,eax
	mov	ecx,ebx
	jmp	NEAR L$009ecb_loop
align	16
L$013ecb_break:
	cmp	esp,ebp
	je	NEAR L$014ecb_done
	pxor	xmm0,xmm0
	lea	eax,[esp]
L$015ecb_bzero:
	movaps	[eax],xmm0
	lea	eax,[16+eax]
	cmp	ebp,eax
	ja	NEAR L$015ecb_bzero
L$014ecb_done:
	mov	ebp,DWORD [16+ebp]
	lea	esp,[24+ebp]
	jmp	NEAR L$016ecb_exit
align	16
L$008ecb_aligned:
	lea	ebp,[ecx*1+esi]
	neg	ebp
	and	ebp,4095
	xor	eax,eax
	cmp	ebp,128
	mov	ebp,127
	cmovae	ebp,eax
	and	ebp,ecx
	sub	ecx,ebp
	jz	NEAR L$017ecb_aligned_tail
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,200
	test	ebp,ebp
	jz	NEAR L$016ecb_exit
L$017ecb_aligned_tail:
	mov	ecx,ebp
	lea	ebp,[esp-24]
	mov	esp,ebp
	mov	eax,ebp
	sub	esp,ecx
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	mov	eax,edi
	mov	ebx,ecx
	shr	ecx,2
	lea	edi,[esp]
db	243,165
	mov	esi,esp
	mov	edi,eax
	mov	ecx,ebx
	jmp	NEAR L$009ecb_loop
L$016ecb_exit:
	mov	eax,1
	lea	esp,[4+esp]
L$006ecb_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_padlock_cbc_encrypt
align	16
_padlock_cbc_encrypt:
L$_padlock_cbc_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	mov	ecx,DWORD [32+esp]
	test	edx,15
	jnz	NEAR L$018cbc_abort
	test	ecx,15
	jnz	NEAR L$018cbc_abort
	lea	eax,[L$padlock_saved_context]
	pushfd
	cld
	call	__padlock_verify_ctx
L$019cbc_pic_point:
	lea	edx,[16+edx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD [edx],32
	jnz	NEAR L$020cbc_aligned
	test	edi,15
	setz	al
	test	esi,15
	setz	bl
	test	eax,ebx
	jnz	NEAR L$020cbc_aligned
	neg	eax
	mov	ebx,512
	not	eax
	lea	ebp,[esp-24]
	cmp	ecx,ebx
	cmovc	ebx,ecx
	and	eax,ebx
	mov	ebx,ecx
	neg	eax
	and	ebx,511
	lea	esp,[ebp*1+eax]
	mov	eax,512
	cmovz	ebx,eax
	mov	eax,ebp
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	cmp	ecx,ebx
	ja	NEAR L$021cbc_loop
	mov	eax,esi
	cmp	ebp,esp
	cmove	eax,edi
	add	eax,ecx
	neg	eax
	and	eax,4095
	cmp	eax,64
	mov	eax,-64
	cmovae	eax,ebx
	and	ebx,eax
	jz	NEAR L$022cbc_unaligned_tail
	jmp	NEAR L$021cbc_loop
align	16
L$021cbc_loop:
	mov	DWORD [ebp],edi
	mov	DWORD [4+ebp],esi
	mov	DWORD [8+ebp],ecx
	mov	ecx,ebx
	mov	DWORD [12+ebp],ebx
	test	edi,15
	cmovnz	edi,esp
	test	esi,15
	jz	NEAR L$023cbc_inp_aligned
	shr	ecx,2
db	243,165
	sub	edi,ebx
	mov	ecx,ebx
	mov	esi,edi
L$023cbc_inp_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,208
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
	mov	edi,DWORD [ebp]
	mov	ebx,DWORD [12+ebp]
	test	edi,15
	jz	NEAR L$024cbc_out_aligned
	mov	ecx,ebx
	lea	esi,[esp]
	shr	ecx,2
db	243,165
	sub	edi,ebx
L$024cbc_out_aligned:
	mov	esi,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	add	edi,ebx
	add	esi,ebx
	sub	ecx,ebx
	mov	ebx,512
	jz	NEAR L$025cbc_break
	cmp	ecx,ebx
	jae	NEAR L$021cbc_loop
L$022cbc_unaligned_tail:
	xor	eax,eax
	cmp	esp,ebp
	cmove	eax,ecx
	sub	esp,eax
	mov	eax,edi
	mov	ebx,ecx
	shr	ecx,2
	lea	edi,[esp]
db	243,165
	mov	esi,esp
	mov	edi,eax
	mov	ecx,ebx
	jmp	NEAR L$021cbc_loop
align	16
L$025cbc_break:
	cmp	esp,ebp
	je	NEAR L$026cbc_done
	pxor	xmm0,xmm0
	lea	eax,[esp]
L$027cbc_bzero:
	movaps	[eax],xmm0
	lea	eax,[16+eax]
	cmp	ebp,eax
	ja	NEAR L$027cbc_bzero
L$026cbc_done:
	mov	ebp,DWORD [16+ebp]
	lea	esp,[24+ebp]
	jmp	NEAR L$028cbc_exit
align	16
L$020cbc_aligned:
	lea	ebp,[ecx*1+esi]
	neg	ebp
	and	ebp,4095
	xor	eax,eax
	cmp	ebp,64
	mov	ebp,63
	cmovae	ebp,eax
	and	ebp,ecx
	sub	ecx,ebp
	jz	NEAR L$029cbc_aligned_tail
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,208
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
	test	ebp,ebp
	jz	NEAR L$028cbc_exit
L$029cbc_aligned_tail:
	mov	ecx,ebp
	lea	ebp,[esp-24]
	mov	esp,ebp
	mov	eax,ebp
	sub	esp,ecx
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	mov	eax,edi
	mov	ebx,ecx
	shr	ecx,2
	lea	edi,[esp]
db	243,165
	mov	esi,esp
	mov	edi,eax
	mov	ecx,ebx
	jmp	NEAR L$021cbc_loop
L$028cbc_exit:
	mov	eax,1
	lea	esp,[4+esp]
L$018cbc_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_padlock_cfb_encrypt
align	16
_padlock_cfb_encrypt:
L$_padlock_cfb_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	mov	ecx,DWORD [32+esp]
	test	edx,15
	jnz	NEAR L$030cfb_abort
	test	ecx,15
	jnz	NEAR L$030cfb_abort
	lea	eax,[L$padlock_saved_context]
	pushfd
	cld
	call	__padlock_verify_ctx
L$031cfb_pic_point:
	lea	edx,[16+edx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD [edx],32
	jnz	NEAR L$032cfb_aligned
	test	edi,15
	setz	al
	test	esi,15
	setz	bl
	test	eax,ebx
	jnz	NEAR L$032cfb_aligned
	neg	eax
	mov	ebx,512
	not	eax
	lea	ebp,[esp-24]
	cmp	ecx,ebx
	cmovc	ebx,ecx
	and	eax,ebx
	mov	ebx,ecx
	neg	eax
	and	ebx,511
	lea	esp,[ebp*1+eax]
	mov	eax,512
	cmovz	ebx,eax
	mov	eax,ebp
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	jmp	NEAR L$033cfb_loop
align	16
L$033cfb_loop:
	mov	DWORD [ebp],edi
	mov	DWORD [4+ebp],esi
	mov	DWORD [8+ebp],ecx
	mov	ecx,ebx
	mov	DWORD [12+ebp],ebx
	test	edi,15
	cmovnz	edi,esp
	test	esi,15
	jz	NEAR L$034cfb_inp_aligned
	shr	ecx,2
db	243,165
	sub	edi,ebx
	mov	ecx,ebx
	mov	esi,edi
L$034cfb_inp_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,224
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
	mov	edi,DWORD [ebp]
	mov	ebx,DWORD [12+ebp]
	test	edi,15
	jz	NEAR L$035cfb_out_aligned
	mov	ecx,ebx
	lea	esi,[esp]
	shr	ecx,2
db	243,165
	sub	edi,ebx
L$035cfb_out_aligned:
	mov	esi,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	add	edi,ebx
	add	esi,ebx
	sub	ecx,ebx
	mov	ebx,512
	jnz	NEAR L$033cfb_loop
	cmp	esp,ebp
	je	NEAR L$036cfb_done
	pxor	xmm0,xmm0
	lea	eax,[esp]
L$037cfb_bzero:
	movaps	[eax],xmm0
	lea	eax,[16+eax]
	cmp	ebp,eax
	ja	NEAR L$037cfb_bzero
L$036cfb_done:
	mov	ebp,DWORD [16+ebp]
	lea	esp,[24+ebp]
	jmp	NEAR L$038cfb_exit
align	16
L$032cfb_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,224
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
L$038cfb_exit:
	mov	eax,1
	lea	esp,[4+esp]
L$030cfb_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_padlock_ofb_encrypt
align	16
_padlock_ofb_encrypt:
L$_padlock_ofb_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	mov	ecx,DWORD [32+esp]
	test	edx,15
	jnz	NEAR L$039ofb_abort
	test	ecx,15
	jnz	NEAR L$039ofb_abort
	lea	eax,[L$padlock_saved_context]
	pushfd
	cld
	call	__padlock_verify_ctx
L$040ofb_pic_point:
	lea	edx,[16+edx]
	xor	eax,eax
	xor	ebx,ebx
	test	DWORD [edx],32
	jnz	NEAR L$041ofb_aligned
	test	edi,15
	setz	al
	test	esi,15
	setz	bl
	test	eax,ebx
	jnz	NEAR L$041ofb_aligned
	neg	eax
	mov	ebx,512
	not	eax
	lea	ebp,[esp-24]
	cmp	ecx,ebx
	cmovc	ebx,ecx
	and	eax,ebx
	mov	ebx,ecx
	neg	eax
	and	ebx,511
	lea	esp,[ebp*1+eax]
	mov	eax,512
	cmovz	ebx,eax
	mov	eax,ebp
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	jmp	NEAR L$042ofb_loop
align	16
L$042ofb_loop:
	mov	DWORD [ebp],edi
	mov	DWORD [4+ebp],esi
	mov	DWORD [8+ebp],ecx
	mov	ecx,ebx
	mov	DWORD [12+ebp],ebx
	test	edi,15
	cmovnz	edi,esp
	test	esi,15
	jz	NEAR L$043ofb_inp_aligned
	shr	ecx,2
db	243,165
	sub	edi,ebx
	mov	ecx,ebx
	mov	esi,edi
L$043ofb_inp_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,232
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
	mov	edi,DWORD [ebp]
	mov	ebx,DWORD [12+ebp]
	test	edi,15
	jz	NEAR L$044ofb_out_aligned
	mov	ecx,ebx
	lea	esi,[esp]
	shr	ecx,2
db	243,165
	sub	edi,ebx
L$044ofb_out_aligned:
	mov	esi,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	add	edi,ebx
	add	esi,ebx
	sub	ecx,ebx
	mov	ebx,512
	jnz	NEAR L$042ofb_loop
	cmp	esp,ebp
	je	NEAR L$045ofb_done
	pxor	xmm0,xmm0
	lea	eax,[esp]
L$046ofb_bzero:
	movaps	[eax],xmm0
	lea	eax,[16+eax]
	cmp	ebp,eax
	ja	NEAR L$046ofb_bzero
L$045ofb_done:
	mov	ebp,DWORD [16+ebp]
	lea	esp,[24+ebp]
	jmp	NEAR L$047ofb_exit
align	16
L$041ofb_aligned:
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,232
	movaps	xmm0,[eax]
	movaps	[edx-16],xmm0
L$047ofb_exit:
	mov	eax,1
	lea	esp,[4+esp]
L$039ofb_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_padlock_ctr32_encrypt
align	16
_padlock_ctr32_encrypt:
L$_padlock_ctr32_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	mov	ecx,DWORD [32+esp]
	test	edx,15
	jnz	NEAR L$048ctr32_abort
	test	ecx,15
	jnz	NEAR L$048ctr32_abort
	lea	eax,[L$padlock_saved_context]
	pushfd
	cld
	call	__padlock_verify_ctx
L$049ctr32_pic_point:
	lea	edx,[16+edx]
	xor	eax,eax
	movq	mm0,[edx-16]
	mov	ebx,512
	not	eax
	lea	ebp,[esp-24]
	cmp	ecx,ebx
	cmovc	ebx,ecx
	and	eax,ebx
	mov	ebx,ecx
	neg	eax
	and	ebx,511
	lea	esp,[ebp*1+eax]
	mov	eax,512
	cmovz	ebx,eax
	mov	eax,ebp
	and	ebp,-16
	and	esp,-16
	mov	DWORD [16+ebp],eax
	jmp	NEAR L$050ctr32_loop
align	16
L$050ctr32_loop:
	mov	DWORD [ebp],edi
	mov	DWORD [4+ebp],esi
	mov	DWORD [8+ebp],ecx
	mov	ecx,ebx
	mov	DWORD [12+ebp],ebx
	mov	ecx,DWORD [edx-4]
	xor	edi,edi
	mov	eax,DWORD [edx-8]
L$051ctr32_prepare:
	mov	DWORD [12+edi*1+esp],ecx
	bswap	ecx
	movq	[edi*1+esp],mm0
	inc	ecx
	mov	DWORD [8+edi*1+esp],eax
	bswap	ecx
	lea	edi,[16+edi]
	cmp	edi,ebx
	jb	NEAR L$051ctr32_prepare
	mov	DWORD [edx-4],ecx
	lea	esi,[esp]
	lea	edi,[esp]
	mov	ecx,ebx
	lea	eax,[edx-16]
	lea	ebx,[16+edx]
	shr	ecx,4
db	243,15,167,200
	mov	edi,DWORD [ebp]
	mov	ebx,DWORD [12+ebp]
	mov	esi,DWORD [4+ebp]
	xor	ecx,ecx
L$052ctr32_xor:
	movups	xmm1,[ecx*1+esi]
	lea	ecx,[16+ecx]
	pxor	xmm1,[ecx*1+esp-16]
	movups	[ecx*1+edi-16],xmm1
	cmp	ecx,ebx
	jb	NEAR L$052ctr32_xor
	mov	ecx,DWORD [8+ebp]
	add	edi,ebx
	add	esi,ebx
	sub	ecx,ebx
	mov	ebx,512
	jnz	NEAR L$050ctr32_loop
	pxor	xmm0,xmm0
	lea	eax,[esp]
L$053ctr32_bzero:
	movaps	[eax],xmm0
	lea	eax,[16+eax]
	cmp	ebp,eax
	ja	NEAR L$053ctr32_bzero
L$054ctr32_done:
	mov	ebp,DWORD [16+ebp]
	lea	esp,[24+ebp]
	mov	eax,1
	lea	esp,[4+esp]
	emms
L$048ctr32_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_padlock_xstore
align	16
_padlock_xstore:
L$_padlock_xstore_begin:
	push	edi
	mov	edi,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
db	15,167,192
	pop	edi
	ret
align	16
__win32_segv_handler:
	mov	eax,1
	mov	edx,DWORD [4+esp]
	mov	ecx,DWORD [12+esp]
	cmp	DWORD [edx],3221225477
	jne	NEAR L$055ret
	add	DWORD [184+ecx],4
	mov	eax,0
L$055ret:
	ret
%if	__NASM_VERSION_ID__ >= 0x02030000
safeseh	__win32_segv_handler
%endif
global	_padlock_sha1_oneshot
align	16
_padlock_sha1_oneshot:
L$_padlock_sha1_oneshot_begin:
	push	edi
	push	esi
	xor	eax,eax
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	push	__win32_segv_handler
db	100,255,48
db	100,137,32
	mov	edx,esp
	add	esp,-128
	movups	xmm0,[edi]
	and	esp,-16
	mov	eax,DWORD [16+edi]
	movaps	[esp],xmm0
	mov	edi,esp
	mov	DWORD [16+esp],eax
	xor	eax,eax
db	243,15,166,200
	movaps	xmm0,[esp]
	mov	eax,DWORD [16+esp]
	mov	esp,edx
db	100,143,5,0,0,0,0
	lea	esp,[4+esp]
	mov	edi,DWORD [16+esp]
	movups	[edi],xmm0
	mov	DWORD [16+edi],eax
	pop	esi
	pop	edi
	ret
global	_padlock_sha1_blocks
align	16
_padlock_sha1_blocks:
L$_padlock_sha1_blocks_begin:
	push	edi
	push	esi
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	edx,esp
	mov	ecx,DWORD [20+esp]
	add	esp,-128
	movups	xmm0,[edi]
	and	esp,-16
	mov	eax,DWORD [16+edi]
	movaps	[esp],xmm0
	mov	edi,esp
	mov	DWORD [16+esp],eax
	mov	eax,-1
db	243,15,166,200
	movaps	xmm0,[esp]
	mov	eax,DWORD [16+esp]
	mov	esp,edx
	mov	edi,DWORD [12+esp]
	movups	[edi],xmm0
	mov	DWORD [16+edi],eax
	pop	esi
	pop	edi
	ret
global	_padlock_sha256_oneshot
align	16
_padlock_sha256_oneshot:
L$_padlock_sha256_oneshot_begin:
	push	edi
	push	esi
	xor	eax,eax
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	push	__win32_segv_handler
db	100,255,48
db	100,137,32
	mov	edx,esp
	add	esp,-128
	movups	xmm0,[edi]
	and	esp,-16
	movups	xmm1,[16+edi]
	movaps	[esp],xmm0
	mov	edi,esp
	movaps	[16+esp],xmm1
	xor	eax,eax
db	243,15,166,208
	movaps	xmm0,[esp]
	movaps	xmm1,[16+esp]
	mov	esp,edx
db	100,143,5,0,0,0,0
	lea	esp,[4+esp]
	mov	edi,DWORD [16+esp]
	movups	[edi],xmm0
	movups	[16+edi],xmm1
	pop	esi
	pop	edi
	ret
global	_padlock_sha256_blocks
align	16
_padlock_sha256_blocks:
L$_padlock_sha256_blocks_begin:
	push	edi
	push	esi
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	mov	edx,esp
	add	esp,-128
	movups	xmm0,[edi]
	and	esp,-16
	movups	xmm1,[16+edi]
	movaps	[esp],xmm0
	mov	edi,esp
	movaps	[16+esp],xmm1
	mov	eax,-1
db	243,15,166,208
	movaps	xmm0,[esp]
	movaps	xmm1,[16+esp]
	mov	esp,edx
	mov	edi,DWORD [12+esp]
	movups	[edi],xmm0
	movups	[16+edi],xmm1
	pop	esi
	pop	edi
	ret
global	_padlock_sha512_blocks
align	16
_padlock_sha512_blocks:
L$_padlock_sha512_blocks_begin:
	push	edi
	push	esi
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	mov	edx,esp
	add	esp,-128
	movups	xmm0,[edi]
	and	esp,-16
	movups	xmm1,[16+edi]
	movups	xmm2,[32+edi]
	movups	xmm3,[48+edi]
	movaps	[esp],xmm0
	mov	edi,esp
	movaps	[16+esp],xmm1
	movaps	[32+esp],xmm2
	movaps	[48+esp],xmm3
db	243,15,166,224
	movaps	xmm0,[esp]
	movaps	xmm1,[16+esp]
	movaps	xmm2,[32+esp]
	movaps	xmm3,[48+esp]
	mov	esp,edx
	mov	edi,DWORD [12+esp]
	movups	[edi],xmm0
	movups	[16+edi],xmm1
	movups	[32+edi],xmm2
	movups	[48+edi],xmm3
	pop	esi
	pop	edi
	ret
db	86,73,65,32,80,97,100,108,111,99,107,32,120,56,54,32
db	109,111,100,117,108,101,44,32,67,82,89,80,84,79,71,65
db	77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101
db	110,115,115,108,46,111,114,103,62,0
align	16
section	.data align=4
align	4
L$padlock_saved_context:
dd	0
