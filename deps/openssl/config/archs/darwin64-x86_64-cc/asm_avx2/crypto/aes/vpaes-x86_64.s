.text	

















.p2align	4
_vpaes_encrypt_core:

	movq	%rdx,%r9
	movq	$16,%r11
	movl	240(%rdx),%eax
	movdqa	%xmm9,%xmm1
	movdqa	L$k_ipt(%rip),%xmm2
	pandn	%xmm0,%xmm1
	movdqu	(%r9),%xmm5
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
.byte	102,15,56,0,208
	movdqa	L$k_ipt+16(%rip),%xmm0
.byte	102,15,56,0,193
	pxor	%xmm5,%xmm2
	addq	$16,%r9
	pxor	%xmm2,%xmm0
	leaq	L$k_mc_backward(%rip),%r10
	jmp	L$enc_entry

.p2align	4
L$enc_loop:

	movdqa	%xmm13,%xmm4
	movdqa	%xmm12,%xmm0
.byte	102,15,56,0,226
.byte	102,15,56,0,195
	pxor	%xmm5,%xmm4
	movdqa	%xmm15,%xmm5
	pxor	%xmm4,%xmm0
	movdqa	-64(%r11,%r10,1),%xmm1
.byte	102,15,56,0,234
	movdqa	(%r11,%r10,1),%xmm4
	movdqa	%xmm14,%xmm2
.byte	102,15,56,0,211
	movdqa	%xmm0,%xmm3
	pxor	%xmm5,%xmm2
.byte	102,15,56,0,193
	addq	$16,%r9
	pxor	%xmm2,%xmm0
.byte	102,15,56,0,220
	addq	$16,%r11
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,193
	andq	$0x30,%r11
	subq	$1,%rax
	pxor	%xmm3,%xmm0

L$enc_entry:

	movdqa	%xmm9,%xmm1
	movdqa	%xmm11,%xmm5
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
.byte	102,15,56,0,232
	movdqa	%xmm10,%xmm3
	pxor	%xmm1,%xmm0
.byte	102,15,56,0,217
	movdqa	%xmm10,%xmm4
	pxor	%xmm5,%xmm3
.byte	102,15,56,0,224
	movdqa	%xmm10,%xmm2
	pxor	%xmm5,%xmm4
.byte	102,15,56,0,211
	movdqa	%xmm10,%xmm3
	pxor	%xmm0,%xmm2
.byte	102,15,56,0,220
	movdqu	(%r9),%xmm5
	pxor	%xmm1,%xmm3
	jnz	L$enc_loop


	movdqa	-96(%r10),%xmm4
	movdqa	-80(%r10),%xmm0
.byte	102,15,56,0,226
	pxor	%xmm5,%xmm4
.byte	102,15,56,0,195
	movdqa	64(%r11,%r10,1),%xmm1
	pxor	%xmm4,%xmm0
.byte	102,15,56,0,193
	.byte	0xf3,0xc3









.p2align	4
_vpaes_decrypt_core:

	movq	%rdx,%r9
	movl	240(%rdx),%eax
	movdqa	%xmm9,%xmm1
	movdqa	L$k_dipt(%rip),%xmm2
	pandn	%xmm0,%xmm1
	movq	%rax,%r11
	psrld	$4,%xmm1
	movdqu	(%r9),%xmm5
	shlq	$4,%r11
	pand	%xmm9,%xmm0
.byte	102,15,56,0,208
	movdqa	L$k_dipt+16(%rip),%xmm0
	xorq	$0x30,%r11
	leaq	L$k_dsbd(%rip),%r10
.byte	102,15,56,0,193
	andq	$0x30,%r11
	pxor	%xmm5,%xmm2
	movdqa	L$k_mc_forward+48(%rip),%xmm5
	pxor	%xmm2,%xmm0
	addq	$16,%r9
	addq	%r10,%r11
	jmp	L$dec_entry

.p2align	4
L$dec_loop:



	movdqa	-32(%r10),%xmm4
	movdqa	-16(%r10),%xmm1
.byte	102,15,56,0,226
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	0(%r10),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	16(%r10),%xmm1

.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	32(%r10),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	48(%r10),%xmm1

.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	64(%r10),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	80(%r10),%xmm1

.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	addq	$16,%r9
.byte	102,15,58,15,237,12
	pxor	%xmm1,%xmm0
	subq	$1,%rax

L$dec_entry:

	movdqa	%xmm9,%xmm1
	pandn	%xmm0,%xmm1
	movdqa	%xmm11,%xmm2
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
.byte	102,15,56,0,208
	movdqa	%xmm10,%xmm3
	pxor	%xmm1,%xmm0
.byte	102,15,56,0,217
	movdqa	%xmm10,%xmm4
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,224
	pxor	%xmm2,%xmm4
	movdqa	%xmm10,%xmm2
.byte	102,15,56,0,211
	movdqa	%xmm10,%xmm3
	pxor	%xmm0,%xmm2
.byte	102,15,56,0,220
	movdqu	(%r9),%xmm0
	pxor	%xmm1,%xmm3
	jnz	L$dec_loop


	movdqa	96(%r10),%xmm4
.byte	102,15,56,0,226
	pxor	%xmm0,%xmm4
	movdqa	112(%r10),%xmm0
	movdqa	-352(%r11),%xmm2
.byte	102,15,56,0,195
	pxor	%xmm4,%xmm0
.byte	102,15,56,0,194
	.byte	0xf3,0xc3









.p2align	4
_vpaes_schedule_core:






	call	_vpaes_preheat
	movdqa	L$k_rcon(%rip),%xmm8
	movdqu	(%rdi),%xmm0


	movdqa	%xmm0,%xmm3
	leaq	L$k_ipt(%rip),%r11
	call	_vpaes_schedule_transform
	movdqa	%xmm0,%xmm7

	leaq	L$k_sr(%rip),%r10
	testq	%rcx,%rcx
	jnz	L$schedule_am_decrypting


	movdqu	%xmm0,(%rdx)
	jmp	L$schedule_go

L$schedule_am_decrypting:

	movdqa	(%r8,%r10,1),%xmm1
.byte	102,15,56,0,217
	movdqu	%xmm3,(%rdx)
	xorq	$0x30,%r8

L$schedule_go:
	cmpl	$192,%esi
	ja	L$schedule_256
	je	L$schedule_192










L$schedule_128:
	movl	$10,%esi

L$oop_schedule_128:
	call	_vpaes_schedule_round
	decq	%rsi
	jz	L$schedule_mangle_last
	call	_vpaes_schedule_mangle
	jmp	L$oop_schedule_128
















.p2align	4
L$schedule_192:
	movdqu	8(%rdi),%xmm0
	call	_vpaes_schedule_transform
	movdqa	%xmm0,%xmm6
	pxor	%xmm4,%xmm4
	movhlps	%xmm4,%xmm6
	movl	$4,%esi

L$oop_schedule_192:
	call	_vpaes_schedule_round
.byte	102,15,58,15,198,8
	call	_vpaes_schedule_mangle
	call	_vpaes_schedule_192_smear
	call	_vpaes_schedule_mangle
	call	_vpaes_schedule_round
	decq	%rsi
	jz	L$schedule_mangle_last
	call	_vpaes_schedule_mangle
	call	_vpaes_schedule_192_smear
	jmp	L$oop_schedule_192











.p2align	4
L$schedule_256:
	movdqu	16(%rdi),%xmm0
	call	_vpaes_schedule_transform
	movl	$7,%esi

L$oop_schedule_256:
	call	_vpaes_schedule_mangle
	movdqa	%xmm0,%xmm6


	call	_vpaes_schedule_round
	decq	%rsi
	jz	L$schedule_mangle_last
	call	_vpaes_schedule_mangle


	pshufd	$0xFF,%xmm0,%xmm0
	movdqa	%xmm7,%xmm5
	movdqa	%xmm6,%xmm7
	call	_vpaes_schedule_low_round
	movdqa	%xmm5,%xmm7

	jmp	L$oop_schedule_256












.p2align	4
L$schedule_mangle_last:

	leaq	L$k_deskew(%rip),%r11
	testq	%rcx,%rcx
	jnz	L$schedule_mangle_last_dec


	movdqa	(%r8,%r10,1),%xmm1
.byte	102,15,56,0,193
	leaq	L$k_opt(%rip),%r11
	addq	$32,%rdx

L$schedule_mangle_last_dec:
	addq	$-16,%rdx
	pxor	L$k_s63(%rip),%xmm0
	call	_vpaes_schedule_transform
	movdqu	%xmm0,(%rdx)


	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	.byte	0xf3,0xc3


















.p2align	4
_vpaes_schedule_192_smear:

	pshufd	$0x80,%xmm6,%xmm1
	pshufd	$0xFE,%xmm7,%xmm0
	pxor	%xmm1,%xmm6
	pxor	%xmm1,%xmm1
	pxor	%xmm0,%xmm6
	movdqa	%xmm6,%xmm0
	movhlps	%xmm1,%xmm6
	.byte	0xf3,0xc3






















.p2align	4
_vpaes_schedule_round:


	pxor	%xmm1,%xmm1
.byte	102,65,15,58,15,200,15
.byte	102,69,15,58,15,192,15
	pxor	%xmm1,%xmm7


	pshufd	$0xFF,%xmm0,%xmm0
.byte	102,15,58,15,192,1




_vpaes_schedule_low_round:

	movdqa	%xmm7,%xmm1
	pslldq	$4,%xmm7
	pxor	%xmm1,%xmm7
	movdqa	%xmm7,%xmm1
	pslldq	$8,%xmm7
	pxor	%xmm1,%xmm7
	pxor	L$k_s63(%rip),%xmm7


	movdqa	%xmm9,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
	movdqa	%xmm11,%xmm2
.byte	102,15,56,0,208
	pxor	%xmm1,%xmm0
	movdqa	%xmm10,%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
	movdqa	%xmm10,%xmm4
.byte	102,15,56,0,224
	pxor	%xmm2,%xmm4
	movdqa	%xmm10,%xmm2
.byte	102,15,56,0,211
	pxor	%xmm0,%xmm2
	movdqa	%xmm10,%xmm3
.byte	102,15,56,0,220
	pxor	%xmm1,%xmm3
	movdqa	%xmm13,%xmm4
.byte	102,15,56,0,226
	movdqa	%xmm12,%xmm0
.byte	102,15,56,0,195
	pxor	%xmm4,%xmm0


	pxor	%xmm7,%xmm0
	movdqa	%xmm0,%xmm7
	.byte	0xf3,0xc3













.p2align	4
_vpaes_schedule_transform:

	movdqa	%xmm9,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
	movdqa	(%r11),%xmm2
.byte	102,15,56,0,208
	movdqa	16(%r11),%xmm0
.byte	102,15,56,0,193
	pxor	%xmm2,%xmm0
	.byte	0xf3,0xc3



























.p2align	4
_vpaes_schedule_mangle:

	movdqa	%xmm0,%xmm4
	movdqa	L$k_mc_forward(%rip),%xmm5
	testq	%rcx,%rcx
	jnz	L$schedule_mangle_dec


	addq	$16,%rdx
	pxor	L$k_s63(%rip),%xmm4
.byte	102,15,56,0,229
	movdqa	%xmm4,%xmm3
.byte	102,15,56,0,229
	pxor	%xmm4,%xmm3
.byte	102,15,56,0,229
	pxor	%xmm4,%xmm3

	jmp	L$schedule_mangle_both
.p2align	4
L$schedule_mangle_dec:

	leaq	L$k_dksd(%rip),%r11
	movdqa	%xmm9,%xmm1
	pandn	%xmm4,%xmm1
	psrld	$4,%xmm1
	pand	%xmm9,%xmm4

	movdqa	0(%r11),%xmm2
.byte	102,15,56,0,212
	movdqa	16(%r11),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221

	movdqa	32(%r11),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	48(%r11),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221

	movdqa	64(%r11),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	80(%r11),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221

	movdqa	96(%r11),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	112(%r11),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3

	addq	$-16,%rdx

L$schedule_mangle_both:
	movdqa	(%r8,%r10,1),%xmm1
.byte	102,15,56,0,217
	addq	$-16,%r8
	andq	$0x30,%r8
	movdqu	%xmm3,(%rdx)
	.byte	0xf3,0xc3






.globl	_vpaes_set_encrypt_key

.p2align	4
_vpaes_set_encrypt_key:

.byte	243,15,30,250
	movl	%esi,%eax
	shrl	$5,%eax
	addl	$5,%eax
	movl	%eax,240(%rdx)

	movl	$0,%ecx
	movl	$0x30,%r8d
	call	_vpaes_schedule_core
	xorl	%eax,%eax
	.byte	0xf3,0xc3



.globl	_vpaes_set_decrypt_key

.p2align	4
_vpaes_set_decrypt_key:

.byte	243,15,30,250
	movl	%esi,%eax
	shrl	$5,%eax
	addl	$5,%eax
	movl	%eax,240(%rdx)
	shll	$4,%eax
	leaq	16(%rdx,%rax,1),%rdx

	movl	$1,%ecx
	movl	%esi,%r8d
	shrl	$1,%r8d
	andl	$32,%r8d
	xorl	$32,%r8d
	call	_vpaes_schedule_core
	xorl	%eax,%eax
	.byte	0xf3,0xc3



.globl	_vpaes_encrypt

.p2align	4
_vpaes_encrypt:

.byte	243,15,30,250
	movdqu	(%rdi),%xmm0
	call	_vpaes_preheat
	call	_vpaes_encrypt_core
	movdqu	%xmm0,(%rsi)
	.byte	0xf3,0xc3



.globl	_vpaes_decrypt

.p2align	4
_vpaes_decrypt:

.byte	243,15,30,250
	movdqu	(%rdi),%xmm0
	call	_vpaes_preheat
	call	_vpaes_decrypt_core
	movdqu	%xmm0,(%rsi)
	.byte	0xf3,0xc3


.globl	_vpaes_cbc_encrypt

.p2align	4
_vpaes_cbc_encrypt:

.byte	243,15,30,250
	xchgq	%rcx,%rdx
	subq	$16,%rcx
	jc	L$cbc_abort
	movdqu	(%r8),%xmm6
	subq	%rdi,%rsi
	call	_vpaes_preheat
	cmpl	$0,%r9d
	je	L$cbc_dec_loop
	jmp	L$cbc_enc_loop
.p2align	4
L$cbc_enc_loop:
	movdqu	(%rdi),%xmm0
	pxor	%xmm6,%xmm0
	call	_vpaes_encrypt_core
	movdqa	%xmm0,%xmm6
	movdqu	%xmm0,(%rsi,%rdi,1)
	leaq	16(%rdi),%rdi
	subq	$16,%rcx
	jnc	L$cbc_enc_loop
	jmp	L$cbc_done
.p2align	4
L$cbc_dec_loop:
	movdqu	(%rdi),%xmm0
	movdqa	%xmm0,%xmm7
	call	_vpaes_decrypt_core
	pxor	%xmm6,%xmm0
	movdqa	%xmm7,%xmm6
	movdqu	%xmm0,(%rsi,%rdi,1)
	leaq	16(%rdi),%rdi
	subq	$16,%rcx
	jnc	L$cbc_dec_loop
L$cbc_done:
	movdqu	%xmm6,(%r8)
L$cbc_abort:
	.byte	0xf3,0xc3









.p2align	4
_vpaes_preheat:

	leaq	L$k_s0F(%rip),%r10
	movdqa	-32(%r10),%xmm10
	movdqa	-16(%r10),%xmm11
	movdqa	0(%r10),%xmm9
	movdqa	48(%r10),%xmm13
	movdqa	64(%r10),%xmm12
	movdqa	80(%r10),%xmm15
	movdqa	96(%r10),%xmm14
	.byte	0xf3,0xc3








.p2align	6
_vpaes_consts:
L$k_inv:
.quad	0x0E05060F0D080180, 0x040703090A0B0C02
.quad	0x01040A060F0B0780, 0x030D0E0C02050809

L$k_s0F:
.quad	0x0F0F0F0F0F0F0F0F, 0x0F0F0F0F0F0F0F0F

L$k_ipt:
.quad	0xC2B2E8985A2A7000, 0xCABAE09052227808
.quad	0x4C01307D317C4D00, 0xCD80B1FCB0FDCC81

L$k_sb1:
.quad	0xB19BE18FCB503E00, 0xA5DF7A6E142AF544
.quad	0x3618D415FAE22300, 0x3BF7CCC10D2ED9EF
L$k_sb2:
.quad	0xE27A93C60B712400, 0x5EB7E955BC982FCD
.quad	0x69EB88400AE12900, 0xC2A163C8AB82234A
L$k_sbo:
.quad	0xD0D26D176FBDC700, 0x15AABF7AC502A878
.quad	0xCFE474A55FBB6A00, 0x8E1E90D1412B35FA

L$k_mc_forward:
.quad	0x0407060500030201, 0x0C0F0E0D080B0A09
.quad	0x080B0A0904070605, 0x000302010C0F0E0D
.quad	0x0C0F0E0D080B0A09, 0x0407060500030201
.quad	0x000302010C0F0E0D, 0x080B0A0904070605

L$k_mc_backward:
.quad	0x0605040702010003, 0x0E0D0C0F0A09080B
.quad	0x020100030E0D0C0F, 0x0A09080B06050407
.quad	0x0E0D0C0F0A09080B, 0x0605040702010003
.quad	0x0A09080B06050407, 0x020100030E0D0C0F

L$k_sr:
.quad	0x0706050403020100, 0x0F0E0D0C0B0A0908
.quad	0x030E09040F0A0500, 0x0B06010C07020D08
.quad	0x0F060D040B020900, 0x070E050C030A0108
.quad	0x0B0E0104070A0D00, 0x0306090C0F020508

L$k_rcon:
.quad	0x1F8391B9AF9DEEB6, 0x702A98084D7C7D81

L$k_s63:
.quad	0x5B5B5B5B5B5B5B5B, 0x5B5B5B5B5B5B5B5B

L$k_opt:
.quad	0xFF9F4929D6B66000, 0xF7974121DEBE6808
.quad	0x01EDBD5150BCEC00, 0xE10D5DB1B05C0CE0

L$k_deskew:
.quad	0x07E4A34047A4E300, 0x1DFEB95A5DBEF91A
.quad	0x5F36B5DC83EA6900, 0x2841C2ABF49D1E77





L$k_dksd:
.quad	0xFEB91A5DA3E44700, 0x0740E3A45A1DBEF9
.quad	0x41C277F4B5368300, 0x5FDC69EAAB289D1E
L$k_dksb:
.quad	0x9A4FCA1F8550D500, 0x03D653861CC94C99
.quad	0x115BEDA7B6FC4A00, 0xD993256F7E3482C8
L$k_dkse:
.quad	0xD5031CCA1FC9D600, 0x53859A4C994F5086
.quad	0xA23196054FDC7BE8, 0xCD5EF96A20B31487
L$k_dks9:
.quad	0xB6116FC87ED9A700, 0x4AED933482255BFC
.quad	0x4576516227143300, 0x8BB89FACE9DAFDCE





L$k_dipt:
.quad	0x0F505B040B545F00, 0x154A411E114E451A
.quad	0x86E383E660056500, 0x12771772F491F194

L$k_dsb9:
.quad	0x851C03539A86D600, 0xCAD51F504F994CC9
.quad	0xC03B1789ECD74900, 0x725E2C9EB2FBA565
L$k_dsbd:
.quad	0x7D57CCDFE6B1A200, 0xF56E9B13882A4439
.quad	0x3CE2FAF724C6CB00, 0x2931180D15DEEFD3
L$k_dsbb:
.quad	0xD022649296B44200, 0x602646F6B0F2D404
.quad	0xC19498A6CD596700, 0xF3FF0C3E3255AA6B
L$k_dsbe:
.quad	0x46F2929626D4D000, 0x2242600464B4F6B0
.quad	0x0C55A6CDFFAAC100, 0x9467F36B98593E32
L$k_dsbo:
.quad	0x1387EA537EF94000, 0xC7AA6DB9D4943E2D
.quad	0x12D7560F93441D00, 0xCA4B8159D8C58E9C
.byte	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105,111,110,32,65,69,83,32,102,111,114,32,120,56,54,95,54,52,47,83,83,83,69,51,44,32,77,105,107,101,32,72,97,109,98,117,114,103,32,40,83,116,97,110,102,111,114,100,32,85,110,105,118,101,114,115,105,116,121,41,0
.p2align	6

