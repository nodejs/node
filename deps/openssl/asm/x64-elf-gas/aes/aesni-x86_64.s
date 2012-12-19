.text

.globl	aesni_encrypt
.type	aesni_encrypt,@function
.align	16
aesni_encrypt:
	movups	(%rdi),%xmm2
	movl	240(%rdx),%eax
	movups	(%rdx),%xmm0
	movups	16(%rdx),%xmm1
	leaq	32(%rdx),%rdx
	xorps	%xmm0,%xmm2
.Loop_enc1_1:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rdx),%xmm1
	leaq	16(%rdx),%rdx
	jnz	.Loop_enc1_1

.byte	102,15,56,221,209
	movups	%xmm2,(%rsi)
	.byte	0xf3,0xc3
.size	aesni_encrypt,.-aesni_encrypt

.globl	aesni_decrypt
.type	aesni_decrypt,@function
.align	16
aesni_decrypt:
	movups	(%rdi),%xmm2
	movl	240(%rdx),%eax
	movups	(%rdx),%xmm0
	movups	16(%rdx),%xmm1
	leaq	32(%rdx),%rdx
	xorps	%xmm0,%xmm2
.Loop_dec1_2:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rdx),%xmm1
	leaq	16(%rdx),%rdx
	jnz	.Loop_dec1_2

.byte	102,15,56,223,209
	movups	%xmm2,(%rsi)
	.byte	0xf3,0xc3
.size	aesni_decrypt, .-aesni_decrypt
.type	_aesni_encrypt3,@function
.align	16
_aesni_encrypt3:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	movups	(%rcx),%xmm0

.Lenc_loop3:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
	movups	(%rcx),%xmm0
	jnz	.Lenc_loop3

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,221,208
.byte	102,15,56,221,216
.byte	102,15,56,221,224
	.byte	0xf3,0xc3
.size	_aesni_encrypt3,.-_aesni_encrypt3
.type	_aesni_decrypt3,@function
.align	16
_aesni_decrypt3:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	movups	(%rcx),%xmm0

.Ldec_loop3:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
	decl	%eax
.byte	102,15,56,222,225
	movups	16(%rcx),%xmm1
.byte	102,15,56,222,208
.byte	102,15,56,222,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,222,224
	movups	(%rcx),%xmm0
	jnz	.Ldec_loop3

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,223,208
.byte	102,15,56,223,216
.byte	102,15,56,223,224
	.byte	0xf3,0xc3
.size	_aesni_decrypt3,.-_aesni_decrypt3
.type	_aesni_encrypt4,@function
.align	16
_aesni_encrypt4:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	xorps	%xmm0,%xmm5
	movups	(%rcx),%xmm0

.Lenc_loop4:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
.byte	102,15,56,220,232
	movups	(%rcx),%xmm0
	jnz	.Lenc_loop4

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,221,208
.byte	102,15,56,221,216
.byte	102,15,56,221,224
.byte	102,15,56,221,232
	.byte	0xf3,0xc3
.size	_aesni_encrypt4,.-_aesni_encrypt4
.type	_aesni_decrypt4,@function
.align	16
_aesni_decrypt4:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	xorps	%xmm0,%xmm5
	movups	(%rcx),%xmm0

.Ldec_loop4:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
	decl	%eax
.byte	102,15,56,222,225
.byte	102,15,56,222,233
	movups	16(%rcx),%xmm1
.byte	102,15,56,222,208
.byte	102,15,56,222,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,222,224
.byte	102,15,56,222,232
	movups	(%rcx),%xmm0
	jnz	.Ldec_loop4

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,223,208
.byte	102,15,56,223,216
.byte	102,15,56,223,224
.byte	102,15,56,223,232
	.byte	0xf3,0xc3
.size	_aesni_decrypt4,.-_aesni_decrypt4
.type	_aesni_encrypt6,@function
.align	16
_aesni_encrypt6:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
.byte	102,15,56,220,209
	pxor	%xmm0,%xmm4
.byte	102,15,56,220,217
	pxor	%xmm0,%xmm5
.byte	102,15,56,220,225
	pxor	%xmm0,%xmm6
.byte	102,15,56,220,233
	pxor	%xmm0,%xmm7
	decl	%eax
.byte	102,15,56,220,241
	movups	(%rcx),%xmm0
.byte	102,15,56,220,249
	jmp	.Lenc_loop6_enter
.align	16
.Lenc_loop6:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.Lenc_loop6_enter:
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
.byte	102,15,56,220,232
.byte	102,15,56,220,240
.byte	102,15,56,220,248
	movups	(%rcx),%xmm0
	jnz	.Lenc_loop6

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.byte	102,15,56,221,208
.byte	102,15,56,221,216
.byte	102,15,56,221,224
.byte	102,15,56,221,232
.byte	102,15,56,221,240
.byte	102,15,56,221,248
	.byte	0xf3,0xc3
.size	_aesni_encrypt6,.-_aesni_encrypt6
.type	_aesni_decrypt6,@function
.align	16
_aesni_decrypt6:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
.byte	102,15,56,222,209
	pxor	%xmm0,%xmm4
.byte	102,15,56,222,217
	pxor	%xmm0,%xmm5
.byte	102,15,56,222,225
	pxor	%xmm0,%xmm6
.byte	102,15,56,222,233
	pxor	%xmm0,%xmm7
	decl	%eax
.byte	102,15,56,222,241
	movups	(%rcx),%xmm0
.byte	102,15,56,222,249
	jmp	.Ldec_loop6_enter
.align	16
.Ldec_loop6:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
	decl	%eax
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
.Ldec_loop6_enter:
	movups	16(%rcx),%xmm1
.byte	102,15,56,222,208
.byte	102,15,56,222,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,222,224
.byte	102,15,56,222,232
.byte	102,15,56,222,240
.byte	102,15,56,222,248
	movups	(%rcx),%xmm0
	jnz	.Ldec_loop6

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
.byte	102,15,56,223,208
.byte	102,15,56,223,216
.byte	102,15,56,223,224
.byte	102,15,56,223,232
.byte	102,15,56,223,240
.byte	102,15,56,223,248
	.byte	0xf3,0xc3
.size	_aesni_decrypt6,.-_aesni_decrypt6
.type	_aesni_encrypt8,@function
.align	16
_aesni_encrypt8:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
.byte	102,15,56,220,209
	pxor	%xmm0,%xmm4
.byte	102,15,56,220,217
	pxor	%xmm0,%xmm5
.byte	102,15,56,220,225
	pxor	%xmm0,%xmm6
.byte	102,15,56,220,233
	pxor	%xmm0,%xmm7
	decl	%eax
.byte	102,15,56,220,241
	pxor	%xmm0,%xmm8
.byte	102,15,56,220,249
	pxor	%xmm0,%xmm9
	movups	(%rcx),%xmm0
.byte	102,68,15,56,220,193
.byte	102,68,15,56,220,201
	movups	16(%rcx),%xmm1
	jmp	.Lenc_loop8_enter
.align	16
.Lenc_loop8:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.byte	102,68,15,56,220,193
.byte	102,68,15,56,220,201
	movups	16(%rcx),%xmm1
.Lenc_loop8_enter:
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
.byte	102,15,56,220,232
.byte	102,15,56,220,240
.byte	102,15,56,220,248
.byte	102,68,15,56,220,192
.byte	102,68,15,56,220,200
	movups	(%rcx),%xmm0
	jnz	.Lenc_loop8

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.byte	102,68,15,56,220,193
.byte	102,68,15,56,220,201
.byte	102,15,56,221,208
.byte	102,15,56,221,216
.byte	102,15,56,221,224
.byte	102,15,56,221,232
.byte	102,15,56,221,240
.byte	102,15,56,221,248
.byte	102,68,15,56,221,192
.byte	102,68,15,56,221,200
	.byte	0xf3,0xc3
.size	_aesni_encrypt8,.-_aesni_encrypt8
.type	_aesni_decrypt8,@function
.align	16
_aesni_decrypt8:
	movups	(%rcx),%xmm0
	shrl	$1,%eax
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
.byte	102,15,56,222,209
	pxor	%xmm0,%xmm4
.byte	102,15,56,222,217
	pxor	%xmm0,%xmm5
.byte	102,15,56,222,225
	pxor	%xmm0,%xmm6
.byte	102,15,56,222,233
	pxor	%xmm0,%xmm7
	decl	%eax
.byte	102,15,56,222,241
	pxor	%xmm0,%xmm8
.byte	102,15,56,222,249
	pxor	%xmm0,%xmm9
	movups	(%rcx),%xmm0
.byte	102,68,15,56,222,193
.byte	102,68,15,56,222,201
	movups	16(%rcx),%xmm1
	jmp	.Ldec_loop8_enter
.align	16
.Ldec_loop8:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
	decl	%eax
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
.byte	102,68,15,56,222,193
.byte	102,68,15,56,222,201
	movups	16(%rcx),%xmm1
.Ldec_loop8_enter:
.byte	102,15,56,222,208
.byte	102,15,56,222,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,222,224
.byte	102,15,56,222,232
.byte	102,15,56,222,240
.byte	102,15,56,222,248
.byte	102,68,15,56,222,192
.byte	102,68,15,56,222,200
	movups	(%rcx),%xmm0
	jnz	.Ldec_loop8

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
.byte	102,68,15,56,222,193
.byte	102,68,15,56,222,201
.byte	102,15,56,223,208
.byte	102,15,56,223,216
.byte	102,15,56,223,224
.byte	102,15,56,223,232
.byte	102,15,56,223,240
.byte	102,15,56,223,248
.byte	102,68,15,56,223,192
.byte	102,68,15,56,223,200
	.byte	0xf3,0xc3
.size	_aesni_decrypt8,.-_aesni_decrypt8
.globl	aesni_ecb_encrypt
.type	aesni_ecb_encrypt,@function
.align	16
aesni_ecb_encrypt:
	andq	$-16,%rdx
	jz	.Lecb_ret

	movl	240(%rcx),%eax
	movups	(%rcx),%xmm0
	movq	%rcx,%r11
	movl	%eax,%r10d
	testl	%r8d,%r8d
	jz	.Lecb_decrypt

	cmpq	$128,%rdx
	jb	.Lecb_enc_tail

	movdqu	(%rdi),%xmm2
	movdqu	16(%rdi),%xmm3
	movdqu	32(%rdi),%xmm4
	movdqu	48(%rdi),%xmm5
	movdqu	64(%rdi),%xmm6
	movdqu	80(%rdi),%xmm7
	movdqu	96(%rdi),%xmm8
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi
	subq	$128,%rdx
	jmp	.Lecb_enc_loop8_enter
.align	16
.Lecb_enc_loop8:
	movups	%xmm2,(%rsi)
	movq	%r11,%rcx
	movdqu	(%rdi),%xmm2
	movl	%r10d,%eax
	movups	%xmm3,16(%rsi)
	movdqu	16(%rdi),%xmm3
	movups	%xmm4,32(%rsi)
	movdqu	32(%rdi),%xmm4
	movups	%xmm5,48(%rsi)
	movdqu	48(%rdi),%xmm5
	movups	%xmm6,64(%rsi)
	movdqu	64(%rdi),%xmm6
	movups	%xmm7,80(%rsi)
	movdqu	80(%rdi),%xmm7
	movups	%xmm8,96(%rsi)
	movdqu	96(%rdi),%xmm8
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi
.Lecb_enc_loop8_enter:

	call	_aesni_encrypt8

	subq	$128,%rdx
	jnc	.Lecb_enc_loop8

	movups	%xmm2,(%rsi)
	movq	%r11,%rcx
	movups	%xmm3,16(%rsi)
	movl	%r10d,%eax
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi
	addq	$128,%rdx
	jz	.Lecb_ret

.Lecb_enc_tail:
	movups	(%rdi),%xmm2
	cmpq	$32,%rdx
	jb	.Lecb_enc_one
	movups	16(%rdi),%xmm3
	je	.Lecb_enc_two
	movups	32(%rdi),%xmm4
	cmpq	$64,%rdx
	jb	.Lecb_enc_three
	movups	48(%rdi),%xmm5
	je	.Lecb_enc_four
	movups	64(%rdi),%xmm6
	cmpq	$96,%rdx
	jb	.Lecb_enc_five
	movups	80(%rdi),%xmm7
	je	.Lecb_enc_six
	movdqu	96(%rdi),%xmm8
	call	_aesni_encrypt8
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_3:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_3

.byte	102,15,56,221,209
	movups	%xmm2,(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_two:
	xorps	%xmm4,%xmm4
	call	_aesni_encrypt3
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_three:
	call	_aesni_encrypt3
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_four:
	call	_aesni_encrypt4
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_five:
	xorps	%xmm7,%xmm7
	call	_aesni_encrypt6
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_six:
	call	_aesni_encrypt6
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	jmp	.Lecb_ret

.align	16
.Lecb_decrypt:
	cmpq	$128,%rdx
	jb	.Lecb_dec_tail

	movdqu	(%rdi),%xmm2
	movdqu	16(%rdi),%xmm3
	movdqu	32(%rdi),%xmm4
	movdqu	48(%rdi),%xmm5
	movdqu	64(%rdi),%xmm6
	movdqu	80(%rdi),%xmm7
	movdqu	96(%rdi),%xmm8
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi
	subq	$128,%rdx
	jmp	.Lecb_dec_loop8_enter
.align	16
.Lecb_dec_loop8:
	movups	%xmm2,(%rsi)
	movq	%r11,%rcx
	movdqu	(%rdi),%xmm2
	movl	%r10d,%eax
	movups	%xmm3,16(%rsi)
	movdqu	16(%rdi),%xmm3
	movups	%xmm4,32(%rsi)
	movdqu	32(%rdi),%xmm4
	movups	%xmm5,48(%rsi)
	movdqu	48(%rdi),%xmm5
	movups	%xmm6,64(%rsi)
	movdqu	64(%rdi),%xmm6
	movups	%xmm7,80(%rsi)
	movdqu	80(%rdi),%xmm7
	movups	%xmm8,96(%rsi)
	movdqu	96(%rdi),%xmm8
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi
.Lecb_dec_loop8_enter:

	call	_aesni_decrypt8

	movups	(%r11),%xmm0
	subq	$128,%rdx
	jnc	.Lecb_dec_loop8

	movups	%xmm2,(%rsi)
	movq	%r11,%rcx
	movups	%xmm3,16(%rsi)
	movl	%r10d,%eax
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi
	addq	$128,%rdx
	jz	.Lecb_ret

.Lecb_dec_tail:
	movups	(%rdi),%xmm2
	cmpq	$32,%rdx
	jb	.Lecb_dec_one
	movups	16(%rdi),%xmm3
	je	.Lecb_dec_two
	movups	32(%rdi),%xmm4
	cmpq	$64,%rdx
	jb	.Lecb_dec_three
	movups	48(%rdi),%xmm5
	je	.Lecb_dec_four
	movups	64(%rdi),%xmm6
	cmpq	$96,%rdx
	jb	.Lecb_dec_five
	movups	80(%rdi),%xmm7
	je	.Lecb_dec_six
	movups	96(%rdi),%xmm8
	movups	(%rcx),%xmm0
	call	_aesni_decrypt8
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_4:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_4

.byte	102,15,56,223,209
	movups	%xmm2,(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_two:
	xorps	%xmm4,%xmm4
	call	_aesni_decrypt3
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_three:
	call	_aesni_decrypt3
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_four:
	call	_aesni_decrypt4
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_five:
	xorps	%xmm7,%xmm7
	call	_aesni_decrypt6
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_dec_six:
	call	_aesni_decrypt6
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)

.Lecb_ret:
	.byte	0xf3,0xc3
.size	aesni_ecb_encrypt,.-aesni_ecb_encrypt
.globl	aesni_ccm64_encrypt_blocks
.type	aesni_ccm64_encrypt_blocks,@function
.align	16
aesni_ccm64_encrypt_blocks:
	movl	240(%rcx),%eax
	movdqu	(%r8),%xmm9
	movdqa	.Lincrement64(%rip),%xmm6
	movdqa	.Lbswap_mask(%rip),%xmm7

	shrl	$1,%eax
	leaq	0(%rcx),%r11
	movdqu	(%r9),%xmm3
	movdqa	%xmm9,%xmm2
	movl	%eax,%r10d
.byte	102,68,15,56,0,207
	jmp	.Lccm64_enc_outer
.align	16
.Lccm64_enc_outer:
	movups	(%r11),%xmm0
	movl	%r10d,%eax
	movups	(%rdi),%xmm8

	xorps	%xmm0,%xmm2
	movups	16(%r11),%xmm1
	xorps	%xmm8,%xmm0
	leaq	32(%r11),%rcx
	xorps	%xmm0,%xmm3
	movups	(%rcx),%xmm0

.Lccm64_enc2_loop:
.byte	102,15,56,220,209
	decl	%eax
.byte	102,15,56,220,217
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,216
	movups	0(%rcx),%xmm0
	jnz	.Lccm64_enc2_loop
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	paddq	%xmm6,%xmm9
.byte	102,15,56,221,208
.byte	102,15,56,221,216

	decq	%rdx
	leaq	16(%rdi),%rdi
	xorps	%xmm2,%xmm8
	movdqa	%xmm9,%xmm2
	movups	%xmm8,(%rsi)
	leaq	16(%rsi),%rsi
.byte	102,15,56,0,215
	jnz	.Lccm64_enc_outer

	movups	%xmm3,(%r9)
	.byte	0xf3,0xc3
.size	aesni_ccm64_encrypt_blocks,.-aesni_ccm64_encrypt_blocks
.globl	aesni_ccm64_decrypt_blocks
.type	aesni_ccm64_decrypt_blocks,@function
.align	16
aesni_ccm64_decrypt_blocks:
	movl	240(%rcx),%eax
	movups	(%r8),%xmm9
	movdqu	(%r9),%xmm3
	movdqa	.Lincrement64(%rip),%xmm6
	movdqa	.Lbswap_mask(%rip),%xmm7

	movaps	%xmm9,%xmm2
	movl	%eax,%r10d
	movq	%rcx,%r11
.byte	102,68,15,56,0,207
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_5:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_5

.byte	102,15,56,221,209
	movups	(%rdi),%xmm8
	paddq	%xmm6,%xmm9
	leaq	16(%rdi),%rdi
	jmp	.Lccm64_dec_outer
.align	16
.Lccm64_dec_outer:
	xorps	%xmm2,%xmm8
	movdqa	%xmm9,%xmm2
	movl	%r10d,%eax
	movups	%xmm8,(%rsi)
	leaq	16(%rsi),%rsi
.byte	102,15,56,0,215

	subq	$1,%rdx
	jz	.Lccm64_dec_break

	movups	(%r11),%xmm0
	shrl	$1,%eax
	movups	16(%r11),%xmm1
	xorps	%xmm0,%xmm8
	leaq	32(%r11),%rcx
	xorps	%xmm0,%xmm2
	xorps	%xmm8,%xmm3
	movups	(%rcx),%xmm0

.Lccm64_dec2_loop:
.byte	102,15,56,220,209
	decl	%eax
.byte	102,15,56,220,217
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,216
	movups	0(%rcx),%xmm0
	jnz	.Lccm64_dec2_loop
	movups	(%rdi),%xmm8
	paddq	%xmm6,%xmm9
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	leaq	16(%rdi),%rdi
.byte	102,15,56,221,208
.byte	102,15,56,221,216
	jmp	.Lccm64_dec_outer

.align	16
.Lccm64_dec_break:

	movups	(%r11),%xmm0
	movups	16(%r11),%xmm1
	xorps	%xmm0,%xmm8
	leaq	32(%r11),%r11
	xorps	%xmm8,%xmm3
.Loop_enc1_6:
.byte	102,15,56,220,217
	decl	%eax
	movups	(%r11),%xmm1
	leaq	16(%r11),%r11
	jnz	.Loop_enc1_6

.byte	102,15,56,221,217
	movups	%xmm3,(%r9)
	.byte	0xf3,0xc3
.size	aesni_ccm64_decrypt_blocks,.-aesni_ccm64_decrypt_blocks
.globl	aesni_ctr32_encrypt_blocks
.type	aesni_ctr32_encrypt_blocks,@function
.align	16
aesni_ctr32_encrypt_blocks:
	cmpq	$1,%rdx
	je	.Lctr32_one_shortcut

	movdqu	(%r8),%xmm14
	movdqa	.Lbswap_mask(%rip),%xmm15
	xorl	%eax,%eax
.byte	102,69,15,58,22,242,3
.byte	102,68,15,58,34,240,3

	movl	240(%rcx),%eax
	bswapl	%r10d
	pxor	%xmm12,%xmm12
	pxor	%xmm13,%xmm13
.byte	102,69,15,58,34,226,0
	leaq	3(%r10),%r11
.byte	102,69,15,58,34,235,0
	incl	%r10d
.byte	102,69,15,58,34,226,1
	incq	%r11
.byte	102,69,15,58,34,235,1
	incl	%r10d
.byte	102,69,15,58,34,226,2
	incq	%r11
.byte	102,69,15,58,34,235,2
	movdqa	%xmm12,-40(%rsp)
.byte	102,69,15,56,0,231
	movdqa	%xmm13,-24(%rsp)
.byte	102,69,15,56,0,239

	pshufd	$192,%xmm12,%xmm2
	pshufd	$128,%xmm12,%xmm3
	pshufd	$64,%xmm12,%xmm4
	cmpq	$6,%rdx
	jb	.Lctr32_tail
	shrl	$1,%eax
	movq	%rcx,%r11
	movl	%eax,%r10d
	subq	$6,%rdx
	jmp	.Lctr32_loop6

.align	16
.Lctr32_loop6:
	pshufd	$192,%xmm13,%xmm5
	por	%xmm14,%xmm2
	movups	(%r11),%xmm0
	pshufd	$128,%xmm13,%xmm6
	por	%xmm14,%xmm3
	movups	16(%r11),%xmm1
	pshufd	$64,%xmm13,%xmm7
	por	%xmm14,%xmm4
	por	%xmm14,%xmm5
	xorps	%xmm0,%xmm2
	por	%xmm14,%xmm6
	por	%xmm14,%xmm7




	pxor	%xmm0,%xmm3
.byte	102,15,56,220,209
	leaq	32(%r11),%rcx
	pxor	%xmm0,%xmm4
.byte	102,15,56,220,217
	movdqa	.Lincrement32(%rip),%xmm13
	pxor	%xmm0,%xmm5
.byte	102,15,56,220,225
	movdqa	-40(%rsp),%xmm12
	pxor	%xmm0,%xmm6
.byte	102,15,56,220,233
	pxor	%xmm0,%xmm7
	movups	(%rcx),%xmm0
	decl	%eax
.byte	102,15,56,220,241
.byte	102,15,56,220,249
	jmp	.Lctr32_enc_loop6_enter
.align	16
.Lctr32_enc_loop6:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.Lctr32_enc_loop6_enter:
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
.byte	102,15,56,220,232
.byte	102,15,56,220,240
.byte	102,15,56,220,248
	movups	(%rcx),%xmm0
	jnz	.Lctr32_enc_loop6

.byte	102,15,56,220,209
	paddd	%xmm13,%xmm12
.byte	102,15,56,220,217
	paddd	-24(%rsp),%xmm13
.byte	102,15,56,220,225
	movdqa	%xmm12,-40(%rsp)
.byte	102,15,56,220,233
	movdqa	%xmm13,-24(%rsp)
.byte	102,15,56,220,241
.byte	102,69,15,56,0,231
.byte	102,15,56,220,249
.byte	102,69,15,56,0,239

.byte	102,15,56,221,208
	movups	(%rdi),%xmm8
.byte	102,15,56,221,216
	movups	16(%rdi),%xmm9
.byte	102,15,56,221,224
	movups	32(%rdi),%xmm10
.byte	102,15,56,221,232
	movups	48(%rdi),%xmm11
.byte	102,15,56,221,240
	movups	64(%rdi),%xmm1
.byte	102,15,56,221,248
	movups	80(%rdi),%xmm0
	leaq	96(%rdi),%rdi

	xorps	%xmm2,%xmm8
	pshufd	$192,%xmm12,%xmm2
	xorps	%xmm3,%xmm9
	pshufd	$128,%xmm12,%xmm3
	movups	%xmm8,(%rsi)
	xorps	%xmm4,%xmm10
	pshufd	$64,%xmm12,%xmm4
	movups	%xmm9,16(%rsi)
	xorps	%xmm5,%xmm11
	movups	%xmm10,32(%rsi)
	xorps	%xmm6,%xmm1
	movups	%xmm11,48(%rsi)
	xorps	%xmm7,%xmm0
	movups	%xmm1,64(%rsi)
	movups	%xmm0,80(%rsi)
	leaq	96(%rsi),%rsi
	movl	%r10d,%eax
	subq	$6,%rdx
	jnc	.Lctr32_loop6

	addq	$6,%rdx
	jz	.Lctr32_done
	movq	%r11,%rcx
	leal	1(%rax,%rax,1),%eax

.Lctr32_tail:
	por	%xmm14,%xmm2
	movups	(%rdi),%xmm8
	cmpq	$2,%rdx
	jb	.Lctr32_one

	por	%xmm14,%xmm3
	movups	16(%rdi),%xmm9
	je	.Lctr32_two

	pshufd	$192,%xmm13,%xmm5
	por	%xmm14,%xmm4
	movups	32(%rdi),%xmm10
	cmpq	$4,%rdx
	jb	.Lctr32_three

	pshufd	$128,%xmm13,%xmm6
	por	%xmm14,%xmm5
	movups	48(%rdi),%xmm11
	je	.Lctr32_four

	por	%xmm14,%xmm6
	xorps	%xmm7,%xmm7

	call	_aesni_encrypt6

	movups	64(%rdi),%xmm1
	xorps	%xmm2,%xmm8
	xorps	%xmm3,%xmm9
	movups	%xmm8,(%rsi)
	xorps	%xmm4,%xmm10
	movups	%xmm9,16(%rsi)
	xorps	%xmm5,%xmm11
	movups	%xmm10,32(%rsi)
	xorps	%xmm6,%xmm1
	movups	%xmm11,48(%rsi)
	movups	%xmm1,64(%rsi)
	jmp	.Lctr32_done

.align	16
.Lctr32_one_shortcut:
	movups	(%r8),%xmm2
	movups	(%rdi),%xmm8
	movl	240(%rcx),%eax
.Lctr32_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_7:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_7

.byte	102,15,56,221,209
	xorps	%xmm2,%xmm8
	movups	%xmm8,(%rsi)
	jmp	.Lctr32_done

.align	16
.Lctr32_two:
	xorps	%xmm4,%xmm4
	call	_aesni_encrypt3
	xorps	%xmm2,%xmm8
	xorps	%xmm3,%xmm9
	movups	%xmm8,(%rsi)
	movups	%xmm9,16(%rsi)
	jmp	.Lctr32_done

.align	16
.Lctr32_three:
	call	_aesni_encrypt3
	xorps	%xmm2,%xmm8
	xorps	%xmm3,%xmm9
	movups	%xmm8,(%rsi)
	xorps	%xmm4,%xmm10
	movups	%xmm9,16(%rsi)
	movups	%xmm10,32(%rsi)
	jmp	.Lctr32_done

.align	16
.Lctr32_four:
	call	_aesni_encrypt4
	xorps	%xmm2,%xmm8
	xorps	%xmm3,%xmm9
	movups	%xmm8,(%rsi)
	xorps	%xmm4,%xmm10
	movups	%xmm9,16(%rsi)
	xorps	%xmm5,%xmm11
	movups	%xmm10,32(%rsi)
	movups	%xmm11,48(%rsi)

.Lctr32_done:
	.byte	0xf3,0xc3
.size	aesni_ctr32_encrypt_blocks,.-aesni_ctr32_encrypt_blocks
.globl	aesni_xts_encrypt
.type	aesni_xts_encrypt,@function
.align	16
aesni_xts_encrypt:
	leaq	-104(%rsp),%rsp
	movups	(%r9),%xmm15
	movl	240(%r8),%eax
	movl	240(%rcx),%r10d
	movups	(%r8),%xmm0
	movups	16(%r8),%xmm1
	leaq	32(%r8),%r8
	xorps	%xmm0,%xmm15
.Loop_enc1_8:
.byte	102,68,15,56,220,249
	decl	%eax
	movups	(%r8),%xmm1
	leaq	16(%r8),%r8
	jnz	.Loop_enc1_8

.byte	102,68,15,56,221,249
	movq	%rcx,%r11
	movl	%r10d,%eax
	movq	%rdx,%r9
	andq	$-16,%rdx

	movdqa	.Lxts_magic(%rip),%xmm8
	pxor	%xmm14,%xmm14
	pcmpgtd	%xmm15,%xmm14
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm10
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm11
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm12
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm13
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	subq	$96,%rdx
	jc	.Lxts_enc_short

	shrl	$1,%eax
	subl	$1,%eax
	movl	%eax,%r10d
	jmp	.Lxts_enc_grandloop

.align	16
.Lxts_enc_grandloop:
	pshufd	$19,%xmm14,%xmm9
	movdqa	%xmm15,%xmm14
	paddq	%xmm15,%xmm15
	movdqu	0(%rdi),%xmm2
	pand	%xmm8,%xmm9
	movdqu	16(%rdi),%xmm3
	pxor	%xmm9,%xmm15

	movdqu	32(%rdi),%xmm4
	pxor	%xmm10,%xmm2
	movdqu	48(%rdi),%xmm5
	pxor	%xmm11,%xmm3
	movdqu	64(%rdi),%xmm6
	pxor	%xmm12,%xmm4
	movdqu	80(%rdi),%xmm7
	leaq	96(%rdi),%rdi
	pxor	%xmm13,%xmm5
	movups	(%r11),%xmm0
	pxor	%xmm14,%xmm6
	pxor	%xmm15,%xmm7



	movups	16(%r11),%xmm1
	pxor	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
	movdqa	%xmm10,0(%rsp)
.byte	102,15,56,220,209
	leaq	32(%r11),%rcx
	pxor	%xmm0,%xmm4
	movdqa	%xmm11,16(%rsp)
.byte	102,15,56,220,217
	pxor	%xmm0,%xmm5
	movdqa	%xmm12,32(%rsp)
.byte	102,15,56,220,225
	pxor	%xmm0,%xmm6
	movdqa	%xmm13,48(%rsp)
.byte	102,15,56,220,233
	pxor	%xmm0,%xmm7
	movups	(%rcx),%xmm0
	decl	%eax
	movdqa	%xmm14,64(%rsp)
.byte	102,15,56,220,241
	movdqa	%xmm15,80(%rsp)
.byte	102,15,56,220,249
	pxor	%xmm14,%xmm14
	pcmpgtd	%xmm15,%xmm14
	jmp	.Lxts_enc_loop6_enter

.align	16
.Lxts_enc_loop6:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
	decl	%eax
.byte	102,15,56,220,225
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
.Lxts_enc_loop6_enter:
	movups	16(%rcx),%xmm1
.byte	102,15,56,220,208
.byte	102,15,56,220,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,220,224
.byte	102,15,56,220,232
.byte	102,15,56,220,240
.byte	102,15,56,220,248
	movups	(%rcx),%xmm0
	jnz	.Lxts_enc_loop6

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	paddq	%xmm15,%xmm15
.byte	102,15,56,220,209
	pand	%xmm8,%xmm9
.byte	102,15,56,220,217
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,220,225
	pxor	%xmm9,%xmm15
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249
	movups	16(%rcx),%xmm1

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm10
	paddq	%xmm15,%xmm15
.byte	102,15,56,220,208
	pand	%xmm8,%xmm9
.byte	102,15,56,220,216
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,220,224
	pxor	%xmm9,%xmm15
.byte	102,15,56,220,232
.byte	102,15,56,220,240
.byte	102,15,56,220,248
	movups	32(%rcx),%xmm0

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm11
	paddq	%xmm15,%xmm15
.byte	102,15,56,220,209
	pand	%xmm8,%xmm9
.byte	102,15,56,220,217
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,220,225
	pxor	%xmm9,%xmm15
.byte	102,15,56,220,233
.byte	102,15,56,220,241
.byte	102,15,56,220,249

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm12
	paddq	%xmm15,%xmm15
.byte	102,15,56,221,208
	pand	%xmm8,%xmm9
.byte	102,15,56,221,216
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,221,224
	pxor	%xmm9,%xmm15
.byte	102,15,56,221,232
.byte	102,15,56,221,240
.byte	102,15,56,221,248

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm13
	paddq	%xmm15,%xmm15
	xorps	0(%rsp),%xmm2
	pand	%xmm8,%xmm9
	xorps	16(%rsp),%xmm3
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15

	xorps	32(%rsp),%xmm4
	movups	%xmm2,0(%rsi)
	xorps	48(%rsp),%xmm5
	movups	%xmm3,16(%rsi)
	xorps	64(%rsp),%xmm6
	movups	%xmm4,32(%rsi)
	xorps	80(%rsp),%xmm7
	movups	%xmm5,48(%rsi)
	movl	%r10d,%eax
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	leaq	96(%rsi),%rsi
	subq	$96,%rdx
	jnc	.Lxts_enc_grandloop

	leal	3(%rax,%rax,1),%eax
	movq	%r11,%rcx
	movl	%eax,%r10d

.Lxts_enc_short:
	addq	$96,%rdx
	jz	.Lxts_enc_done

	cmpq	$32,%rdx
	jb	.Lxts_enc_one
	je	.Lxts_enc_two

	cmpq	$64,%rdx
	jb	.Lxts_enc_three
	je	.Lxts_enc_four

	pshufd	$19,%xmm14,%xmm9
	movdqa	%xmm15,%xmm14
	paddq	%xmm15,%xmm15
	movdqu	(%rdi),%xmm2
	pand	%xmm8,%xmm9
	movdqu	16(%rdi),%xmm3
	pxor	%xmm9,%xmm15

	movdqu	32(%rdi),%xmm4
	pxor	%xmm10,%xmm2
	movdqu	48(%rdi),%xmm5
	pxor	%xmm11,%xmm3
	movdqu	64(%rdi),%xmm6
	leaq	80(%rdi),%rdi
	pxor	%xmm12,%xmm4
	pxor	%xmm13,%xmm5
	pxor	%xmm14,%xmm6

	call	_aesni_encrypt6

	xorps	%xmm10,%xmm2
	movdqa	%xmm15,%xmm10
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	movdqu	%xmm2,(%rsi)
	xorps	%xmm13,%xmm5
	movdqu	%xmm3,16(%rsi)
	xorps	%xmm14,%xmm6
	movdqu	%xmm4,32(%rsi)
	movdqu	%xmm5,48(%rsi)
	movdqu	%xmm6,64(%rsi)
	leaq	80(%rsi),%rsi
	jmp	.Lxts_enc_done

.align	16
.Lxts_enc_one:
	movups	(%rdi),%xmm2
	leaq	16(%rdi),%rdi
	xorps	%xmm10,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_9:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_9

.byte	102,15,56,221,209
	xorps	%xmm10,%xmm2
	movdqa	%xmm11,%xmm10
	movups	%xmm2,(%rsi)
	leaq	16(%rsi),%rsi
	jmp	.Lxts_enc_done

.align	16
.Lxts_enc_two:
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	leaq	32(%rdi),%rdi
	xorps	%xmm10,%xmm2
	xorps	%xmm11,%xmm3

	call	_aesni_encrypt3

	xorps	%xmm10,%xmm2
	movdqa	%xmm12,%xmm10
	xorps	%xmm11,%xmm3
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	leaq	32(%rsi),%rsi
	jmp	.Lxts_enc_done

.align	16
.Lxts_enc_three:
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	movups	32(%rdi),%xmm4
	leaq	48(%rdi),%rdi
	xorps	%xmm10,%xmm2
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4

	call	_aesni_encrypt3

	xorps	%xmm10,%xmm2
	movdqa	%xmm13,%xmm10
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	leaq	48(%rsi),%rsi
	jmp	.Lxts_enc_done

.align	16
.Lxts_enc_four:
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	movups	32(%rdi),%xmm4
	xorps	%xmm10,%xmm2
	movups	48(%rdi),%xmm5
	leaq	64(%rdi),%rdi
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	xorps	%xmm13,%xmm5

	call	_aesni_encrypt4

	xorps	%xmm10,%xmm2
	movdqa	%xmm15,%xmm10
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	movups	%xmm2,(%rsi)
	xorps	%xmm13,%xmm5
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	leaq	64(%rsi),%rsi
	jmp	.Lxts_enc_done

.align	16
.Lxts_enc_done:
	andq	$15,%r9
	jz	.Lxts_enc_ret
	movq	%r9,%rdx

.Lxts_enc_steal:
	movzbl	(%rdi),%eax
	movzbl	-16(%rsi),%ecx
	leaq	1(%rdi),%rdi
	movb	%al,-16(%rsi)
	movb	%cl,0(%rsi)
	leaq	1(%rsi),%rsi
	subq	$1,%rdx
	jnz	.Lxts_enc_steal

	subq	%r9,%rsi
	movq	%r11,%rcx
	movl	%r10d,%eax

	movups	-16(%rsi),%xmm2
	xorps	%xmm10,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_10:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_10

.byte	102,15,56,221,209
	xorps	%xmm10,%xmm2
	movups	%xmm2,-16(%rsi)

.Lxts_enc_ret:
	leaq	104(%rsp),%rsp
.Lxts_enc_epilogue:
	.byte	0xf3,0xc3
.size	aesni_xts_encrypt,.-aesni_xts_encrypt
.globl	aesni_xts_decrypt
.type	aesni_xts_decrypt,@function
.align	16
aesni_xts_decrypt:
	leaq	-104(%rsp),%rsp
	movups	(%r9),%xmm15
	movl	240(%r8),%eax
	movl	240(%rcx),%r10d
	movups	(%r8),%xmm0
	movups	16(%r8),%xmm1
	leaq	32(%r8),%r8
	xorps	%xmm0,%xmm15
.Loop_enc1_11:
.byte	102,68,15,56,220,249
	decl	%eax
	movups	(%r8),%xmm1
	leaq	16(%r8),%r8
	jnz	.Loop_enc1_11

.byte	102,68,15,56,221,249
	xorl	%eax,%eax
	testq	$15,%rdx
	setnz	%al
	shlq	$4,%rax
	subq	%rax,%rdx

	movq	%rcx,%r11
	movl	%r10d,%eax
	movq	%rdx,%r9
	andq	$-16,%rdx

	movdqa	.Lxts_magic(%rip),%xmm8
	pxor	%xmm14,%xmm14
	pcmpgtd	%xmm15,%xmm14
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm10
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm11
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm12
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm13
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm9
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15
	subq	$96,%rdx
	jc	.Lxts_dec_short

	shrl	$1,%eax
	subl	$1,%eax
	movl	%eax,%r10d
	jmp	.Lxts_dec_grandloop

.align	16
.Lxts_dec_grandloop:
	pshufd	$19,%xmm14,%xmm9
	movdqa	%xmm15,%xmm14
	paddq	%xmm15,%xmm15
	movdqu	0(%rdi),%xmm2
	pand	%xmm8,%xmm9
	movdqu	16(%rdi),%xmm3
	pxor	%xmm9,%xmm15

	movdqu	32(%rdi),%xmm4
	pxor	%xmm10,%xmm2
	movdqu	48(%rdi),%xmm5
	pxor	%xmm11,%xmm3
	movdqu	64(%rdi),%xmm6
	pxor	%xmm12,%xmm4
	movdqu	80(%rdi),%xmm7
	leaq	96(%rdi),%rdi
	pxor	%xmm13,%xmm5
	movups	(%r11),%xmm0
	pxor	%xmm14,%xmm6
	pxor	%xmm15,%xmm7



	movups	16(%r11),%xmm1
	pxor	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
	movdqa	%xmm10,0(%rsp)
.byte	102,15,56,222,209
	leaq	32(%r11),%rcx
	pxor	%xmm0,%xmm4
	movdqa	%xmm11,16(%rsp)
.byte	102,15,56,222,217
	pxor	%xmm0,%xmm5
	movdqa	%xmm12,32(%rsp)
.byte	102,15,56,222,225
	pxor	%xmm0,%xmm6
	movdqa	%xmm13,48(%rsp)
.byte	102,15,56,222,233
	pxor	%xmm0,%xmm7
	movups	(%rcx),%xmm0
	decl	%eax
	movdqa	%xmm14,64(%rsp)
.byte	102,15,56,222,241
	movdqa	%xmm15,80(%rsp)
.byte	102,15,56,222,249
	pxor	%xmm14,%xmm14
	pcmpgtd	%xmm15,%xmm14
	jmp	.Lxts_dec_loop6_enter

.align	16
.Lxts_dec_loop6:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
	decl	%eax
.byte	102,15,56,222,225
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
.Lxts_dec_loop6_enter:
	movups	16(%rcx),%xmm1
.byte	102,15,56,222,208
.byte	102,15,56,222,216
	leaq	32(%rcx),%rcx
.byte	102,15,56,222,224
.byte	102,15,56,222,232
.byte	102,15,56,222,240
.byte	102,15,56,222,248
	movups	(%rcx),%xmm0
	jnz	.Lxts_dec_loop6

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	paddq	%xmm15,%xmm15
.byte	102,15,56,222,209
	pand	%xmm8,%xmm9
.byte	102,15,56,222,217
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,222,225
	pxor	%xmm9,%xmm15
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249
	movups	16(%rcx),%xmm1

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm10
	paddq	%xmm15,%xmm15
.byte	102,15,56,222,208
	pand	%xmm8,%xmm9
.byte	102,15,56,222,216
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,222,224
	pxor	%xmm9,%xmm15
.byte	102,15,56,222,232
.byte	102,15,56,222,240
.byte	102,15,56,222,248
	movups	32(%rcx),%xmm0

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm11
	paddq	%xmm15,%xmm15
.byte	102,15,56,222,209
	pand	%xmm8,%xmm9
.byte	102,15,56,222,217
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,222,225
	pxor	%xmm9,%xmm15
.byte	102,15,56,222,233
.byte	102,15,56,222,241
.byte	102,15,56,222,249

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm12
	paddq	%xmm15,%xmm15
.byte	102,15,56,223,208
	pand	%xmm8,%xmm9
.byte	102,15,56,223,216
	pcmpgtd	%xmm15,%xmm14
.byte	102,15,56,223,224
	pxor	%xmm9,%xmm15
.byte	102,15,56,223,232
.byte	102,15,56,223,240
.byte	102,15,56,223,248

	pshufd	$19,%xmm14,%xmm9
	pxor	%xmm14,%xmm14
	movdqa	%xmm15,%xmm13
	paddq	%xmm15,%xmm15
	xorps	0(%rsp),%xmm2
	pand	%xmm8,%xmm9
	xorps	16(%rsp),%xmm3
	pcmpgtd	%xmm15,%xmm14
	pxor	%xmm9,%xmm15

	xorps	32(%rsp),%xmm4
	movups	%xmm2,0(%rsi)
	xorps	48(%rsp),%xmm5
	movups	%xmm3,16(%rsi)
	xorps	64(%rsp),%xmm6
	movups	%xmm4,32(%rsi)
	xorps	80(%rsp),%xmm7
	movups	%xmm5,48(%rsi)
	movl	%r10d,%eax
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	leaq	96(%rsi),%rsi
	subq	$96,%rdx
	jnc	.Lxts_dec_grandloop

	leal	3(%rax,%rax,1),%eax
	movq	%r11,%rcx
	movl	%eax,%r10d

.Lxts_dec_short:
	addq	$96,%rdx
	jz	.Lxts_dec_done

	cmpq	$32,%rdx
	jb	.Lxts_dec_one
	je	.Lxts_dec_two

	cmpq	$64,%rdx
	jb	.Lxts_dec_three
	je	.Lxts_dec_four

	pshufd	$19,%xmm14,%xmm9
	movdqa	%xmm15,%xmm14
	paddq	%xmm15,%xmm15
	movdqu	(%rdi),%xmm2
	pand	%xmm8,%xmm9
	movdqu	16(%rdi),%xmm3
	pxor	%xmm9,%xmm15

	movdqu	32(%rdi),%xmm4
	pxor	%xmm10,%xmm2
	movdqu	48(%rdi),%xmm5
	pxor	%xmm11,%xmm3
	movdqu	64(%rdi),%xmm6
	leaq	80(%rdi),%rdi
	pxor	%xmm12,%xmm4
	pxor	%xmm13,%xmm5
	pxor	%xmm14,%xmm6

	call	_aesni_decrypt6

	xorps	%xmm10,%xmm2
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	movdqu	%xmm2,(%rsi)
	xorps	%xmm13,%xmm5
	movdqu	%xmm3,16(%rsi)
	xorps	%xmm14,%xmm6
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm14,%xmm14
	movdqu	%xmm5,48(%rsi)
	pcmpgtd	%xmm15,%xmm14
	movdqu	%xmm6,64(%rsi)
	leaq	80(%rsi),%rsi
	pshufd	$19,%xmm14,%xmm11
	andq	$15,%r9
	jz	.Lxts_dec_ret

	movdqa	%xmm15,%xmm10
	paddq	%xmm15,%xmm15
	pand	%xmm8,%xmm11
	pxor	%xmm15,%xmm11
	jmp	.Lxts_dec_done2

.align	16
.Lxts_dec_one:
	movups	(%rdi),%xmm2
	leaq	16(%rdi),%rdi
	xorps	%xmm10,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_12:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_12

.byte	102,15,56,223,209
	xorps	%xmm10,%xmm2
	movdqa	%xmm11,%xmm10
	movups	%xmm2,(%rsi)
	movdqa	%xmm12,%xmm11
	leaq	16(%rsi),%rsi
	jmp	.Lxts_dec_done

.align	16
.Lxts_dec_two:
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	leaq	32(%rdi),%rdi
	xorps	%xmm10,%xmm2
	xorps	%xmm11,%xmm3

	call	_aesni_decrypt3

	xorps	%xmm10,%xmm2
	movdqa	%xmm12,%xmm10
	xorps	%xmm11,%xmm3
	movdqa	%xmm13,%xmm11
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	leaq	32(%rsi),%rsi
	jmp	.Lxts_dec_done

.align	16
.Lxts_dec_three:
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	movups	32(%rdi),%xmm4
	leaq	48(%rdi),%rdi
	xorps	%xmm10,%xmm2
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4

	call	_aesni_decrypt3

	xorps	%xmm10,%xmm2
	movdqa	%xmm13,%xmm10
	xorps	%xmm11,%xmm3
	movdqa	%xmm15,%xmm11
	xorps	%xmm12,%xmm4
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	leaq	48(%rsi),%rsi
	jmp	.Lxts_dec_done

.align	16
.Lxts_dec_four:
	pshufd	$19,%xmm14,%xmm9
	movdqa	%xmm15,%xmm14
	paddq	%xmm15,%xmm15
	movups	(%rdi),%xmm2
	pand	%xmm8,%xmm9
	movups	16(%rdi),%xmm3
	pxor	%xmm9,%xmm15

	movups	32(%rdi),%xmm4
	xorps	%xmm10,%xmm2
	movups	48(%rdi),%xmm5
	leaq	64(%rdi),%rdi
	xorps	%xmm11,%xmm3
	xorps	%xmm12,%xmm4
	xorps	%xmm13,%xmm5

	call	_aesni_decrypt4

	xorps	%xmm10,%xmm2
	movdqa	%xmm14,%xmm10
	xorps	%xmm11,%xmm3
	movdqa	%xmm15,%xmm11
	xorps	%xmm12,%xmm4
	movups	%xmm2,(%rsi)
	xorps	%xmm13,%xmm5
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	leaq	64(%rsi),%rsi
	jmp	.Lxts_dec_done

.align	16
.Lxts_dec_done:
	andq	$15,%r9
	jz	.Lxts_dec_ret
.Lxts_dec_done2:
	movq	%r9,%rdx
	movq	%r11,%rcx
	movl	%r10d,%eax

	movups	(%rdi),%xmm2
	xorps	%xmm11,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_13:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_13

.byte	102,15,56,223,209
	xorps	%xmm11,%xmm2
	movups	%xmm2,(%rsi)

.Lxts_dec_steal:
	movzbl	16(%rdi),%eax
	movzbl	(%rsi),%ecx
	leaq	1(%rdi),%rdi
	movb	%al,(%rsi)
	movb	%cl,16(%rsi)
	leaq	1(%rsi),%rsi
	subq	$1,%rdx
	jnz	.Lxts_dec_steal

	subq	%r9,%rsi
	movq	%r11,%rcx
	movl	%r10d,%eax

	movups	(%rsi),%xmm2
	xorps	%xmm10,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_14:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_14

.byte	102,15,56,223,209
	xorps	%xmm10,%xmm2
	movups	%xmm2,(%rsi)

.Lxts_dec_ret:
	leaq	104(%rsp),%rsp
.Lxts_dec_epilogue:
	.byte	0xf3,0xc3
.size	aesni_xts_decrypt,.-aesni_xts_decrypt
.globl	aesni_cbc_encrypt
.type	aesni_cbc_encrypt,@function
.align	16
aesni_cbc_encrypt:
	testq	%rdx,%rdx
	jz	.Lcbc_ret

	movl	240(%rcx),%r10d
	movq	%rcx,%r11
	testl	%r9d,%r9d
	jz	.Lcbc_decrypt

	movups	(%r8),%xmm2
	movl	%r10d,%eax
	cmpq	$16,%rdx
	jb	.Lcbc_enc_tail
	subq	$16,%rdx
	jmp	.Lcbc_enc_loop
.align	16
.Lcbc_enc_loop:
	movups	(%rdi),%xmm3
	leaq	16(%rdi),%rdi

	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm3
	leaq	32(%rcx),%rcx
	xorps	%xmm3,%xmm2
.Loop_enc1_15:
.byte	102,15,56,220,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_15

.byte	102,15,56,221,209
	movl	%r10d,%eax
	movq	%r11,%rcx
	movups	%xmm2,0(%rsi)
	leaq	16(%rsi),%rsi
	subq	$16,%rdx
	jnc	.Lcbc_enc_loop
	addq	$16,%rdx
	jnz	.Lcbc_enc_tail
	movups	%xmm2,(%r8)
	jmp	.Lcbc_ret

.Lcbc_enc_tail:
	movq	%rdx,%rcx
	xchgq	%rdi,%rsi
.long	0x9066A4F3

	movl	$16,%ecx
	subq	%rdx,%rcx
	xorl	%eax,%eax
.long	0x9066AAF3

	leaq	-16(%rdi),%rdi
	movl	%r10d,%eax
	movq	%rdi,%rsi
	movq	%r11,%rcx
	xorq	%rdx,%rdx
	jmp	.Lcbc_enc_loop


.align	16
.Lcbc_decrypt:
	movups	(%r8),%xmm9
	movl	%r10d,%eax
	cmpq	$112,%rdx
	jbe	.Lcbc_dec_tail
	shrl	$1,%r10d
	subq	$112,%rdx
	movl	%r10d,%eax
	movaps	%xmm9,-24(%rsp)
	jmp	.Lcbc_dec_loop8_enter
.align	16
.Lcbc_dec_loop8:
	movaps	%xmm0,-24(%rsp)
	movups	%xmm9,(%rsi)
	leaq	16(%rsi),%rsi
.Lcbc_dec_loop8_enter:
	movups	(%rcx),%xmm0
	movups	(%rdi),%xmm2
	movups	16(%rdi),%xmm3
	movups	16(%rcx),%xmm1

	leaq	32(%rcx),%rcx
	movdqu	32(%rdi),%xmm4
	xorps	%xmm0,%xmm2
	movdqu	48(%rdi),%xmm5
	xorps	%xmm0,%xmm3
	movdqu	64(%rdi),%xmm6
.byte	102,15,56,222,209
	pxor	%xmm0,%xmm4
	movdqu	80(%rdi),%xmm7
.byte	102,15,56,222,217
	pxor	%xmm0,%xmm5
	movdqu	96(%rdi),%xmm8
.byte	102,15,56,222,225
	pxor	%xmm0,%xmm6
	movdqu	112(%rdi),%xmm9
.byte	102,15,56,222,233
	pxor	%xmm0,%xmm7
	decl	%eax
.byte	102,15,56,222,241
	pxor	%xmm0,%xmm8
.byte	102,15,56,222,249
	pxor	%xmm0,%xmm9
	movups	(%rcx),%xmm0
.byte	102,68,15,56,222,193
.byte	102,68,15,56,222,201
	movups	16(%rcx),%xmm1

	call	.Ldec_loop8_enter

	movups	(%rdi),%xmm1
	movups	16(%rdi),%xmm0
	xorps	-24(%rsp),%xmm2
	xorps	%xmm1,%xmm3
	movups	32(%rdi),%xmm1
	xorps	%xmm0,%xmm4
	movups	48(%rdi),%xmm0
	xorps	%xmm1,%xmm5
	movups	64(%rdi),%xmm1
	xorps	%xmm0,%xmm6
	movups	80(%rdi),%xmm0
	xorps	%xmm1,%xmm7
	movups	96(%rdi),%xmm1
	xorps	%xmm0,%xmm8
	movups	112(%rdi),%xmm0
	xorps	%xmm1,%xmm9
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movl	%r10d,%eax
	movups	%xmm6,64(%rsi)
	movq	%r11,%rcx
	movups	%xmm7,80(%rsi)
	leaq	128(%rdi),%rdi
	movups	%xmm8,96(%rsi)
	leaq	112(%rsi),%rsi
	subq	$128,%rdx
	ja	.Lcbc_dec_loop8

	movaps	%xmm9,%xmm2
	movaps	%xmm0,%xmm9
	addq	$112,%rdx
	jle	.Lcbc_dec_tail_collected
	movups	%xmm2,(%rsi)
	leal	1(%r10,%r10,1),%eax
	leaq	16(%rsi),%rsi
.Lcbc_dec_tail:
	movups	(%rdi),%xmm2
	movaps	%xmm2,%xmm8
	cmpq	$16,%rdx
	jbe	.Lcbc_dec_one

	movups	16(%rdi),%xmm3
	movaps	%xmm3,%xmm7
	cmpq	$32,%rdx
	jbe	.Lcbc_dec_two

	movups	32(%rdi),%xmm4
	movaps	%xmm4,%xmm6
	cmpq	$48,%rdx
	jbe	.Lcbc_dec_three

	movups	48(%rdi),%xmm5
	cmpq	$64,%rdx
	jbe	.Lcbc_dec_four

	movups	64(%rdi),%xmm6
	cmpq	$80,%rdx
	jbe	.Lcbc_dec_five

	movups	80(%rdi),%xmm7
	cmpq	$96,%rdx
	jbe	.Lcbc_dec_six

	movups	96(%rdi),%xmm8
	movaps	%xmm9,-24(%rsp)
	call	_aesni_decrypt8
	movups	(%rdi),%xmm1
	movups	16(%rdi),%xmm0
	xorps	-24(%rsp),%xmm2
	xorps	%xmm1,%xmm3
	movups	32(%rdi),%xmm1
	xorps	%xmm0,%xmm4
	movups	48(%rdi),%xmm0
	xorps	%xmm1,%xmm5
	movups	64(%rdi),%xmm1
	xorps	%xmm0,%xmm6
	movups	80(%rdi),%xmm0
	xorps	%xmm1,%xmm7
	movups	96(%rdi),%xmm9
	xorps	%xmm0,%xmm8
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	leaq	96(%rsi),%rsi
	movaps	%xmm8,%xmm2
	subq	$112,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_16:
.byte	102,15,56,222,209
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_16

.byte	102,15,56,223,209
	xorps	%xmm9,%xmm2
	movaps	%xmm8,%xmm9
	subq	$16,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_two:
	xorps	%xmm4,%xmm4
	call	_aesni_decrypt3
	xorps	%xmm9,%xmm2
	xorps	%xmm8,%xmm3
	movups	%xmm2,(%rsi)
	movaps	%xmm7,%xmm9
	movaps	%xmm3,%xmm2
	leaq	16(%rsi),%rsi
	subq	$32,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_three:
	call	_aesni_decrypt3
	xorps	%xmm9,%xmm2
	xorps	%xmm8,%xmm3
	movups	%xmm2,(%rsi)
	xorps	%xmm7,%xmm4
	movups	%xmm3,16(%rsi)
	movaps	%xmm6,%xmm9
	movaps	%xmm4,%xmm2
	leaq	32(%rsi),%rsi
	subq	$48,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_four:
	call	_aesni_decrypt4
	xorps	%xmm9,%xmm2
	movups	48(%rdi),%xmm9
	xorps	%xmm8,%xmm3
	movups	%xmm2,(%rsi)
	xorps	%xmm7,%xmm4
	movups	%xmm3,16(%rsi)
	xorps	%xmm6,%xmm5
	movups	%xmm4,32(%rsi)
	movaps	%xmm5,%xmm2
	leaq	48(%rsi),%rsi
	subq	$64,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_five:
	xorps	%xmm7,%xmm7
	call	_aesni_decrypt6
	movups	16(%rdi),%xmm1
	movups	32(%rdi),%xmm0
	xorps	%xmm9,%xmm2
	xorps	%xmm8,%xmm3
	xorps	%xmm1,%xmm4
	movups	48(%rdi),%xmm1
	xorps	%xmm0,%xmm5
	movups	64(%rdi),%xmm9
	xorps	%xmm1,%xmm6
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	leaq	64(%rsi),%rsi
	movaps	%xmm6,%xmm2
	subq	$80,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_six:
	call	_aesni_decrypt6
	movups	16(%rdi),%xmm1
	movups	32(%rdi),%xmm0
	xorps	%xmm9,%xmm2
	xorps	%xmm8,%xmm3
	xorps	%xmm1,%xmm4
	movups	48(%rdi),%xmm1
	xorps	%xmm0,%xmm5
	movups	64(%rdi),%xmm0
	xorps	%xmm1,%xmm6
	movups	80(%rdi),%xmm9
	xorps	%xmm0,%xmm7
	movups	%xmm2,(%rsi)
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	leaq	80(%rsi),%rsi
	movaps	%xmm7,%xmm2
	subq	$96,%rdx
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_tail_collected:
	andq	$15,%rdx
	movups	%xmm9,(%r8)
	jnz	.Lcbc_dec_tail_partial
	movups	%xmm2,(%rsi)
	jmp	.Lcbc_dec_ret
.align	16
.Lcbc_dec_tail_partial:
	movaps	%xmm2,-24(%rsp)
	movq	$16,%rcx
	movq	%rsi,%rdi
	subq	%rdx,%rcx
	leaq	-24(%rsp),%rsi
.long	0x9066A4F3


.Lcbc_dec_ret:
.Lcbc_ret:
	.byte	0xf3,0xc3
.size	aesni_cbc_encrypt,.-aesni_cbc_encrypt
.globl	aesni_set_decrypt_key
.type	aesni_set_decrypt_key,@function
.align	16
aesni_set_decrypt_key:
.byte	0x48,0x83,0xEC,0x08

	call	__aesni_set_encrypt_key
	shll	$4,%esi
	testl	%eax,%eax
	jnz	.Ldec_key_ret
	leaq	16(%rdx,%rsi,1),%rdi

	movups	(%rdx),%xmm0
	movups	(%rdi),%xmm1
	movups	%xmm0,(%rdi)
	movups	%xmm1,(%rdx)
	leaq	16(%rdx),%rdx
	leaq	-16(%rdi),%rdi

.Ldec_key_inverse:
	movups	(%rdx),%xmm0
	movups	(%rdi),%xmm1
.byte	102,15,56,219,192
.byte	102,15,56,219,201
	leaq	16(%rdx),%rdx
	leaq	-16(%rdi),%rdi
	movups	%xmm0,16(%rdi)
	movups	%xmm1,-16(%rdx)
	cmpq	%rdx,%rdi
	ja	.Ldec_key_inverse

	movups	(%rdx),%xmm0
.byte	102,15,56,219,192
	movups	%xmm0,(%rdi)
.Ldec_key_ret:
	addq	$8,%rsp
	.byte	0xf3,0xc3
.LSEH_end_set_decrypt_key:
.size	aesni_set_decrypt_key,.-aesni_set_decrypt_key
.globl	aesni_set_encrypt_key
.type	aesni_set_encrypt_key,@function
.align	16
aesni_set_encrypt_key:
__aesni_set_encrypt_key:
.byte	0x48,0x83,0xEC,0x08

	movq	$-1,%rax
	testq	%rdi,%rdi
	jz	.Lenc_key_ret
	testq	%rdx,%rdx
	jz	.Lenc_key_ret

	movups	(%rdi),%xmm0
	xorps	%xmm4,%xmm4
	leaq	16(%rdx),%rax
	cmpl	$256,%esi
	je	.L14rounds
	cmpl	$192,%esi
	je	.L12rounds
	cmpl	$128,%esi
	jne	.Lbad_keybits

.L10rounds:
	movl	$9,%esi
	movups	%xmm0,(%rdx)
.byte	102,15,58,223,200,1
	call	.Lkey_expansion_128_cold
.byte	102,15,58,223,200,2
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,4
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,8
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,16
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,32
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,64
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,128
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,27
	call	.Lkey_expansion_128
.byte	102,15,58,223,200,54
	call	.Lkey_expansion_128
	movups	%xmm0,(%rax)
	movl	%esi,80(%rax)
	xorl	%eax,%eax
	jmp	.Lenc_key_ret

.align	16
.L12rounds:
	movq	16(%rdi),%xmm2
	movl	$11,%esi
	movups	%xmm0,(%rdx)
.byte	102,15,58,223,202,1
	call	.Lkey_expansion_192a_cold
.byte	102,15,58,223,202,2
	call	.Lkey_expansion_192b
.byte	102,15,58,223,202,4
	call	.Lkey_expansion_192a
.byte	102,15,58,223,202,8
	call	.Lkey_expansion_192b
.byte	102,15,58,223,202,16
	call	.Lkey_expansion_192a
.byte	102,15,58,223,202,32
	call	.Lkey_expansion_192b
.byte	102,15,58,223,202,64
	call	.Lkey_expansion_192a
.byte	102,15,58,223,202,128
	call	.Lkey_expansion_192b
	movups	%xmm0,(%rax)
	movl	%esi,48(%rax)
	xorq	%rax,%rax
	jmp	.Lenc_key_ret

.align	16
.L14rounds:
	movups	16(%rdi),%xmm2
	movl	$13,%esi
	leaq	16(%rax),%rax
	movups	%xmm0,(%rdx)
	movups	%xmm2,16(%rdx)
.byte	102,15,58,223,202,1
	call	.Lkey_expansion_256a_cold
.byte	102,15,58,223,200,1
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,2
	call	.Lkey_expansion_256a
.byte	102,15,58,223,200,2
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,4
	call	.Lkey_expansion_256a
.byte	102,15,58,223,200,4
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,8
	call	.Lkey_expansion_256a
.byte	102,15,58,223,200,8
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,16
	call	.Lkey_expansion_256a
.byte	102,15,58,223,200,16
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,32
	call	.Lkey_expansion_256a
.byte	102,15,58,223,200,32
	call	.Lkey_expansion_256b
.byte	102,15,58,223,202,64
	call	.Lkey_expansion_256a
	movups	%xmm0,(%rax)
	movl	%esi,16(%rax)
	xorq	%rax,%rax
	jmp	.Lenc_key_ret

.align	16
.Lbad_keybits:
	movq	$-2,%rax
.Lenc_key_ret:
	addq	$8,%rsp
	.byte	0xf3,0xc3
.LSEH_end_set_encrypt_key:

.align	16
.Lkey_expansion_128:
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_128_cold:
	shufps	$16,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$255,%xmm1,%xmm1
	xorps	%xmm1,%xmm0
	.byte	0xf3,0xc3

.align	16
.Lkey_expansion_192a:
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_192a_cold:
	movaps	%xmm2,%xmm5
.Lkey_expansion_192b_warm:
	shufps	$16,%xmm0,%xmm4
	movdqa	%xmm2,%xmm3
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	pslldq	$4,%xmm3
	xorps	%xmm4,%xmm0
	pshufd	$85,%xmm1,%xmm1
	pxor	%xmm3,%xmm2
	pxor	%xmm1,%xmm0
	pshufd	$255,%xmm0,%xmm3
	pxor	%xmm3,%xmm2
	.byte	0xf3,0xc3

.align	16
.Lkey_expansion_192b:
	movaps	%xmm0,%xmm3
	shufps	$68,%xmm0,%xmm5
	movups	%xmm5,(%rax)
	shufps	$78,%xmm2,%xmm3
	movups	%xmm3,16(%rax)
	leaq	32(%rax),%rax
	jmp	.Lkey_expansion_192b_warm

.align	16
.Lkey_expansion_256a:
	movups	%xmm2,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_256a_cold:
	shufps	$16,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$255,%xmm1,%xmm1
	xorps	%xmm1,%xmm0
	.byte	0xf3,0xc3

.align	16
.Lkey_expansion_256b:
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax

	shufps	$16,%xmm2,%xmm4
	xorps	%xmm4,%xmm2
	shufps	$140,%xmm2,%xmm4
	xorps	%xmm4,%xmm2
	shufps	$170,%xmm1,%xmm1
	xorps	%xmm1,%xmm2
	.byte	0xf3,0xc3
.size	aesni_set_encrypt_key,.-aesni_set_encrypt_key
.size	__aesni_set_encrypt_key,.-__aesni_set_encrypt_key
.align	64
.Lbswap_mask:
.byte	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
.Lincrement32:
.long	6,6,6,0
.Lincrement64:
.long	1,0,0,0
.Lxts_magic:
.long	0x87,0,1,0

.byte	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69,83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	64
