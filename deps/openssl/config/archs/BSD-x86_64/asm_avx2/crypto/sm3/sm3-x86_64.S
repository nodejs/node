.data	
.align	16
SHUFF_MASK:
.byte	3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12

.text	








.globl	ossl_hwsm3_block_data_order
.type	ossl_hwsm3_block_data_order,@function
.align	32
ossl_hwsm3_block_data_order:
.cfi_startproc	
.byte	243,15,30,250

	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
.cfi_def_cfa_register	%rbp
.Lossl_hwsm3_block_data_order_seh_setfp:

.Lossl_hwsm3_block_data_order_seh_prolog_end:
	orq	%rdx,%rdx
	je	.done_hash




	vmovdqu	(%rdi),%xmm6
	vmovdqu	16(%rdi),%xmm7

	vpshufd	$0x1B,%xmm6,%xmm0
	vpshufd	$0x1B,%xmm7,%xmm1
	vpunpckhqdq	%xmm0,%xmm1,%xmm6
	vpunpcklqdq	%xmm0,%xmm1,%xmm7
	vpsrld	$9,%xmm7,%xmm2
	vpslld	$23,%xmm7,%xmm3
	vpxor	%xmm3,%xmm2,%xmm1
	vpsrld	$19,%xmm7,%xmm4
	vpslld	$13,%xmm7,%xmm5
	vpxor	%xmm5,%xmm4,%xmm0

	vpblendd	$0x3,%xmm0,%xmm1,%xmm7

	vmovdqa	SHUFF_MASK(%rip),%xmm12

.align	32
.block_loop:
	vmovdqa	%xmm6,%xmm10
	vmovdqa	%xmm7,%xmm11


	vmovdqu	(%rsi),%xmm2
	vmovdqu	16(%rsi),%xmm3
	vmovdqu	32(%rsi),%xmm4
	vmovdqu	48(%rsi),%xmm5
	vpshufb	%xmm12,%xmm2,%xmm2
	vpshufb	%xmm12,%xmm3,%xmm3
	vpshufb	%xmm12,%xmm4,%xmm4
	vpshufb	%xmm12,%xmm5,%xmm5

	vpalignr	$12,%xmm3,%xmm4,%xmm8
	vpsrldq	$4,%xmm5,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc2
	vpalignr	$12,%xmm2,%xmm3,%xmm9
	vpalignr	$8,%xmm4,%xmm5,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x00
	vpunpckhqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x02
	vmovdqa	%xmm8,%xmm2
	vpalignr	$12,%xmm4,%xmm5,%xmm8
	vpsrldq	$4,%xmm2,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc3
	vpalignr	$12,%xmm3,%xmm4,%xmm9
	vpalignr	$8,%xmm5,%xmm2,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x04
	vpunpckhqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x06
	vmovdqa	%xmm8,%xmm3
	vpalignr	$12,%xmm5,%xmm2,%xmm8
	vpsrldq	$4,%xmm3,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc4
	vpalignr	$12,%xmm4,%xmm5,%xmm9
	vpalignr	$8,%xmm2,%xmm3,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x08
	vpunpckhqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x0a
	vmovdqa	%xmm8,%xmm4
	vpalignr	$12,%xmm2,%xmm3,%xmm8
	vpsrldq	$4,%xmm4,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc5
	vpalignr	$12,%xmm5,%xmm2,%xmm9
	vpalignr	$8,%xmm3,%xmm4,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x0c
	vpunpckhqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x0e
	vmovdqa	%xmm8,%xmm5
	vpalignr	$12,%xmm3,%xmm4,%xmm8
	vpsrldq	$4,%xmm5,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc2
	vpalignr	$12,%xmm2,%xmm3,%xmm9
	vpalignr	$8,%xmm4,%xmm5,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x10
	vpunpckhqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x12
	vmovdqa	%xmm8,%xmm2
	vpalignr	$12,%xmm4,%xmm5,%xmm8
	vpsrldq	$4,%xmm2,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc3
	vpalignr	$12,%xmm3,%xmm4,%xmm9
	vpalignr	$8,%xmm5,%xmm2,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x14
	vpunpckhqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x16
	vmovdqa	%xmm8,%xmm3
	vpalignr	$12,%xmm5,%xmm2,%xmm8
	vpsrldq	$4,%xmm3,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc4
	vpalignr	$12,%xmm4,%xmm5,%xmm9
	vpalignr	$8,%xmm2,%xmm3,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x18
	vpunpckhqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x1a
	vmovdqa	%xmm8,%xmm4
	vpalignr	$12,%xmm2,%xmm3,%xmm8
	vpsrldq	$4,%xmm4,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc5
	vpalignr	$12,%xmm5,%xmm2,%xmm9
	vpalignr	$8,%xmm3,%xmm4,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x1c
	vpunpckhqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x1e
	vmovdqa	%xmm8,%xmm5
	vpalignr	$12,%xmm3,%xmm4,%xmm8
	vpsrldq	$4,%xmm5,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc2
	vpalignr	$12,%xmm2,%xmm3,%xmm9
	vpalignr	$8,%xmm4,%xmm5,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x20
	vpunpckhqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x22
	vmovdqa	%xmm8,%xmm2
	vpalignr	$12,%xmm4,%xmm5,%xmm8
	vpsrldq	$4,%xmm2,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc3
	vpalignr	$12,%xmm3,%xmm4,%xmm9
	vpalignr	$8,%xmm5,%xmm2,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x24
	vpunpckhqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x26
	vmovdqa	%xmm8,%xmm3
	vpalignr	$12,%xmm5,%xmm2,%xmm8
	vpsrldq	$4,%xmm3,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc4
	vpalignr	$12,%xmm4,%xmm5,%xmm9
	vpalignr	$8,%xmm2,%xmm3,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x28
	vpunpckhqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x2a
	vmovdqa	%xmm8,%xmm4
	vpalignr	$12,%xmm2,%xmm3,%xmm8
	vpsrldq	$4,%xmm4,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc5
	vpalignr	$12,%xmm5,%xmm2,%xmm9
	vpalignr	$8,%xmm3,%xmm4,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x2c
	vpunpckhqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x2e
	vmovdqa	%xmm8,%xmm5
	vpalignr	$12,%xmm3,%xmm4,%xmm8
	vpsrldq	$4,%xmm5,%xmm9
.byte	0xc4,0x62,0x30,0xda,0xc2
	vpalignr	$12,%xmm2,%xmm3,%xmm9
	vpalignr	$8,%xmm4,%xmm5,%xmm1
.byte	0xc4,0x62,0x31,0xda,0xc1
	vpunpcklqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x30
	vpunpckhqdq	%xmm3,%xmm2,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x32
	vmovdqa	%xmm8,%xmm2
	vpunpcklqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x34
	vpunpckhqdq	%xmm4,%xmm3,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x36
	vpunpcklqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x38
	vpunpckhqdq	%xmm5,%xmm4,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x3a
	vpunpcklqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x49,0xde,0xf9,0x3c
	vpunpckhqdq	%xmm2,%xmm5,%xmm1
.byte	0xc4,0xe3,0x41,0xde,0xf1,0x3e

	vpxor	%xmm10,%xmm6,%xmm6
	vpxor	%xmm11,%xmm7,%xmm7
	addq	$64,%rsi
	decq	%rdx
	jnz	.block_loop


	vpslld	$9,%xmm7,%xmm2
	vpsrld	$23,%xmm7,%xmm3
	vpxor	%xmm3,%xmm2,%xmm1
	vpslld	$19,%xmm7,%xmm4
	vpsrld	$13,%xmm7,%xmm5
	vpxor	%xmm5,%xmm4,%xmm0
	vpblendd	$0x3,%xmm0,%xmm1,%xmm7
	vpshufd	$0x1B,%xmm6,%xmm0
	vpshufd	$0x1B,%xmm7,%xmm1

	vpunpcklqdq	%xmm1,%xmm0,%xmm6
	vpunpckhqdq	%xmm1,%xmm0,%xmm7

	vmovdqu	%xmm6,(%rdi)
	vmovdqu	%xmm7,16(%rdi)
.done_hash:

	popq	%rbp
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
	.section ".note.gnu.property", "a"
	.p2align 3
	.long 1f - 0f
	.long 4f - 1f
	.long 5
0:
	# "GNU" encoded with .byte, since .asciz isn't supported
	# on Solaris.
	.byte 0x47
	.byte 0x4e
	.byte 0x55
	.byte 0
1:
	.p2align 3
	.long 0xc0000002
	.long 3f - 2f
2:
	.long 3
3:
	.p2align 3
4:
