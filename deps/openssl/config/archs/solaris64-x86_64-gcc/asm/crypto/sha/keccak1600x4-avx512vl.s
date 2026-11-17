.text	



.globl	SHA3_avx512vl_capable
.type	SHA3_avx512vl_capable,@function
.align	32
SHA3_avx512vl_capable:
	movl	OPENSSL_ia32cap_P+8(%rip),%ecx
	xorl	%eax,%eax

	andl	$3221422080,%ecx
	cmpl	$3221422080,%ecx
	cmovel	%ecx,%eax
	.byte	0xf3,0xc3
.size	SHA3_avx512vl_capable, .-SHA3_avx512vl_capable
.text	














.type	keccak_1600_permute,@function
.align	32
keccak_1600_permute:
.cfi_startproc	
	movl	$24,%r13d
	leaq	iotas(%rip),%r14

.align	32
.Lkeccak_rnd_loop:







	vmovdqa64	%ymm0,%ymm25
	vpternlogq	$0x96,%ymm5,%ymm10,%ymm25
	vmovdqa64	%ymm1,%ymm26
	vpternlogq	$0x96,%ymm11,%ymm6,%ymm26
	vmovdqa64	%ymm2,%ymm27
	vpternlogq	$0x96,%ymm12,%ymm7,%ymm27

	vmovdqa64	%ymm3,%ymm28
	vpternlogq	$0x96,%ymm13,%ymm8,%ymm28
	vmovdqa64	%ymm4,%ymm29
	vpternlogq	$0x96,%ymm14,%ymm9,%ymm29
	vpternlogq	$0x96,%ymm20,%ymm15,%ymm25

	vpternlogq	$0x96,%ymm21,%ymm16,%ymm26
	vpternlogq	$0x96,%ymm22,%ymm17,%ymm27
	vpternlogq	$0x96,%ymm23,%ymm18,%ymm28






	vprolq	$1,%ymm26,%ymm30
	vprolq	$1,%ymm27,%ymm31
	vpternlogq	$0x96,%ymm24,%ymm19,%ymm29






	vpternlogq	$0x96,%ymm30,%ymm29,%ymm0
	vpternlogq	$0x96,%ymm30,%ymm29,%ymm10
	vpternlogq	$0x96,%ymm30,%ymm29,%ymm20

	vpternlogq	$0x96,%ymm30,%ymm29,%ymm5
	vpternlogq	$0x96,%ymm30,%ymm29,%ymm15
	vprolq	$1,%ymm28,%ymm30

	vpternlogq	$0x96,%ymm31,%ymm25,%ymm6
	vpternlogq	$0x96,%ymm31,%ymm25,%ymm16
	vpternlogq	$0x96,%ymm31,%ymm25,%ymm1

	vpternlogq	$0x96,%ymm31,%ymm25,%ymm11
	vpternlogq	$0x96,%ymm31,%ymm25,%ymm21
	vprolq	$1,%ymm29,%ymm31

	vpbroadcastq	(%r14),%ymm29
	addq	$8,%r14

	vpternlogq	$0x96,%ymm30,%ymm26,%ymm12
	vpternlogq	$0x96,%ymm30,%ymm26,%ymm7
	vpternlogq	$0x96,%ymm30,%ymm26,%ymm22

	vpternlogq	$0x96,%ymm30,%ymm26,%ymm17
	vpternlogq	$0x96,%ymm30,%ymm26,%ymm2
	vprolq	$1,%ymm25,%ymm30















	vpternlogq	$0x96,%ymm31,%ymm27,%ymm3
	vpternlogq	$0x96,%ymm31,%ymm27,%ymm13
	vpternlogq	$0x96,%ymm31,%ymm27,%ymm23

	vprolq	$44,%ymm6,%ymm6
	vpternlogq	$0x96,%ymm31,%ymm27,%ymm18
	vpternlogq	$0x96,%ymm31,%ymm27,%ymm8

	vprolq	$43,%ymm12,%ymm12
	vprolq	$21,%ymm18,%ymm18
	vpternlogq	$0x96,%ymm30,%ymm28,%ymm24

	vprolq	$14,%ymm24,%ymm24
	vprolq	$28,%ymm3,%ymm3
	vpternlogq	$0x96,%ymm30,%ymm28,%ymm9

	vprolq	$20,%ymm9,%ymm9
	vprolq	$3,%ymm10,%ymm10
	vpternlogq	$0x96,%ymm30,%ymm28,%ymm19

	vprolq	$45,%ymm16,%ymm16
	vprolq	$61,%ymm22,%ymm22
	vpternlogq	$0x96,%ymm30,%ymm28,%ymm4

	vprolq	$1,%ymm1,%ymm1
	vprolq	$6,%ymm7,%ymm7
	vpternlogq	$0x96,%ymm30,%ymm28,%ymm14








	vprolq	$25,%ymm13,%ymm13
	vprolq	$8,%ymm19,%ymm19
	vmovdqa64	%ymm0,%ymm30
	vpternlogq	$0xD2,%ymm12,%ymm6,%ymm30

	vprolq	$18,%ymm20,%ymm20
	vprolq	$27,%ymm4,%ymm4
	vpxorq	%ymm29,%ymm30,%ymm30

	vprolq	$36,%ymm5,%ymm5
	vprolq	$10,%ymm11,%ymm11
	vmovdqa64	%ymm6,%ymm31
	vpternlogq	$0xD2,%ymm18,%ymm12,%ymm31

	vprolq	$15,%ymm17,%ymm17
	vprolq	$56,%ymm23,%ymm23
	vpternlogq	$0xD2,%ymm24,%ymm18,%ymm12

	vprolq	$62,%ymm2,%ymm2
	vprolq	$55,%ymm8,%ymm8
	vpternlogq	$0xD2,%ymm0,%ymm24,%ymm18

	vprolq	$39,%ymm14,%ymm14
	vprolq	$41,%ymm15,%ymm15
	vpternlogq	$0xD2,%ymm6,%ymm0,%ymm24
	vmovdqa64	%ymm30,%ymm0
	vmovdqa64	%ymm31,%ymm6

	vprolq	$2,%ymm21,%ymm21
	vmovdqa64	%ymm3,%ymm30
	vpternlogq	$0xD2,%ymm10,%ymm9,%ymm30
	vmovdqa64	%ymm9,%ymm31
	vpternlogq	$0xD2,%ymm16,%ymm10,%ymm31

	vpternlogq	$0xD2,%ymm22,%ymm16,%ymm10
	vpternlogq	$0xD2,%ymm3,%ymm22,%ymm16
	vpternlogq	$0xD2,%ymm9,%ymm3,%ymm22
	vmovdqa64	%ymm30,%ymm3
	vmovdqa64	%ymm31,%ymm9

	vmovdqa64	%ymm1,%ymm30
	vpternlogq	$0xD2,%ymm13,%ymm7,%ymm30
	vmovdqa64	%ymm7,%ymm31
	vpternlogq	$0xD2,%ymm19,%ymm13,%ymm31
	vpternlogq	$0xD2,%ymm20,%ymm19,%ymm13

	vpternlogq	$0xD2,%ymm1,%ymm20,%ymm19
	vpternlogq	$0xD2,%ymm7,%ymm1,%ymm20
	vmovdqa64	%ymm30,%ymm1
	vmovdqa64	%ymm31,%ymm7
	vmovdqa64	%ymm4,%ymm30
	vpternlogq	$0xD2,%ymm11,%ymm5,%ymm30

	vmovdqa64	%ymm5,%ymm31
	vpternlogq	$0xD2,%ymm17,%ymm11,%ymm31
	vpternlogq	$0xD2,%ymm23,%ymm17,%ymm11
	vpternlogq	$0xD2,%ymm4,%ymm23,%ymm17

	vpternlogq	$0xD2,%ymm5,%ymm4,%ymm23
	vmovdqa64	%ymm30,%ymm4
	vmovdqa64	%ymm31,%ymm5
	vmovdqa64	%ymm2,%ymm30
	vpternlogq	$0xD2,%ymm14,%ymm8,%ymm30
	vmovdqa64	%ymm8,%ymm31
	vpternlogq	$0xD2,%ymm15,%ymm14,%ymm31

	vpternlogq	$0xD2,%ymm21,%ymm15,%ymm14
	vpternlogq	$0xD2,%ymm2,%ymm21,%ymm15
	vpternlogq	$0xD2,%ymm8,%ymm2,%ymm21
	vmovdqa64	%ymm30,%ymm2
	vmovdqa64	%ymm31,%ymm8


	vmovdqa64	%ymm3,%ymm30
	vmovdqa64	%ymm18,%ymm3
	vmovdqa64	%ymm17,%ymm18
	vmovdqa64	%ymm11,%ymm17
	vmovdqa64	%ymm7,%ymm11
	vmovdqa64	%ymm10,%ymm7
	vmovdqa64	%ymm1,%ymm10
	vmovdqa64	%ymm6,%ymm1
	vmovdqa64	%ymm9,%ymm6
	vmovdqa64	%ymm22,%ymm9
	vmovdqa64	%ymm14,%ymm22
	vmovdqa64	%ymm20,%ymm14
	vmovdqa64	%ymm2,%ymm20
	vmovdqa64	%ymm12,%ymm2
	vmovdqa64	%ymm13,%ymm12
	vmovdqa64	%ymm19,%ymm13
	vmovdqa64	%ymm23,%ymm19
	vmovdqa64	%ymm15,%ymm23
	vmovdqa64	%ymm4,%ymm15
	vmovdqa64	%ymm24,%ymm4
	vmovdqa64	%ymm21,%ymm24
	vmovdqa64	%ymm8,%ymm21
	vmovdqa64	%ymm16,%ymm8
	vmovdqa64	%ymm5,%ymm16
	vmovdqa64	%ymm30,%ymm5

	decl	%r13d
	jnz	.Lkeccak_rnd_loop
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_permute,.-keccak_1600_permute


.globl	keccak_1600_init_state
.type	keccak_1600_init_state,@function
.align	32
keccak_1600_init_state:
.cfi_startproc	
	vpxorq	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm1
	vmovdqa64	%ymm0,%ymm2
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm4
	vmovdqa64	%ymm0,%ymm5
	vmovdqa64	%ymm0,%ymm6
	vmovdqa64	%ymm0,%ymm7
	vmovdqa64	%ymm0,%ymm8
	vmovdqa64	%ymm0,%ymm9
	vmovdqa64	%ymm0,%ymm10
	vmovdqa64	%ymm0,%ymm11
	vmovdqa64	%ymm0,%ymm12
	vmovdqa64	%ymm0,%ymm13
	vmovdqa64	%ymm0,%ymm14
	vmovdqa64	%ymm0,%ymm15
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19
	vmovdqa64	%ymm0,%ymm20
	vmovdqa64	%ymm0,%ymm21
	vmovdqa64	%ymm0,%ymm22
	vmovdqa64	%ymm0,%ymm23
	vmovdqa64	%ymm0,%ymm24
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_init_state,.-keccak_1600_init_state

.globl	keccak_1600_load_state_x4
.type	keccak_1600_load_state_x4,@function
.align	32
keccak_1600_load_state_x4:
.cfi_startproc	
	vmovdqu64	0(%rdi),%ymm0
	vmovdqu64	32(%rdi),%ymm1
	vmovdqu64	64(%rdi),%ymm2
	vmovdqu64	96(%rdi),%ymm3
	vmovdqu64	128(%rdi),%ymm4
	vmovdqu64	160(%rdi),%ymm5
	vmovdqu64	192(%rdi),%ymm6
	vmovdqu64	224(%rdi),%ymm7
	vmovdqu64	256(%rdi),%ymm8
	vmovdqu64	288(%rdi),%ymm9
	vmovdqu64	320(%rdi),%ymm10
	vmovdqu64	352(%rdi),%ymm11
	vmovdqu64	384(%rdi),%ymm12
	vmovdqu64	416(%rdi),%ymm13
	vmovdqu64	448(%rdi),%ymm14
	vmovdqu64	480(%rdi),%ymm15
	vmovdqu64	512(%rdi),%ymm16
	vmovdqu64	544(%rdi),%ymm17
	vmovdqu64	576(%rdi),%ymm18
	vmovdqu64	608(%rdi),%ymm19
	vmovdqu64	640(%rdi),%ymm20
	vmovdqu64	672(%rdi),%ymm21
	vmovdqu64	704(%rdi),%ymm22
	vmovdqu64	736(%rdi),%ymm23
	vmovdqu64	768(%rdi),%ymm24
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_load_state_x4,.-keccak_1600_load_state_x4


.globl	keccak_1600_save_state_x4
.type	keccak_1600_save_state_x4,@function
.align	32
keccak_1600_save_state_x4:
.cfi_startproc	
	vmovdqu64	%ymm0,0(%rdi)
	vmovdqu64	%ymm1,32(%rdi)
	vmovdqu64	%ymm2,64(%rdi)
	vmovdqu64	%ymm3,96(%rdi)
	vmovdqu64	%ymm4,128(%rdi)
	vmovdqu64	%ymm5,160(%rdi)
	vmovdqu64	%ymm6,192(%rdi)
	vmovdqu64	%ymm7,224(%rdi)
	vmovdqu64	%ymm8,256(%rdi)
	vmovdqu64	%ymm9,288(%rdi)
	vmovdqu64	%ymm10,320(%rdi)
	vmovdqu64	%ymm11,352(%rdi)
	vmovdqu64	%ymm12,384(%rdi)
	vmovdqu64	%ymm13,416(%rdi)
	vmovdqu64	%ymm14,448(%rdi)
	vmovdqu64	%ymm15,480(%rdi)
	vmovdqu64	%ymm16,512(%rdi)
	vmovdqu64	%ymm17,544(%rdi)
	vmovdqu64	%ymm18,576(%rdi)
	vmovdqu64	%ymm19,608(%rdi)
	vmovdqu64	%ymm20,640(%rdi)
	vmovdqu64	%ymm21,672(%rdi)
	vmovdqu64	%ymm22,704(%rdi)
	vmovdqu64	%ymm23,736(%rdi)
	vmovdqu64	%ymm24,768(%rdi)
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_save_state_x4,.-keccak_1600_save_state_x4











.globl	keccak_1600_partial_add_x4
.type	keccak_1600_partial_add_x4,@function
.align	32
keccak_1600_partial_add_x4:
.cfi_startproc	
	movq	800(%r10),%r9
	testl	$7,%r9d
	jz	.Lstart_aligned_to_4x8


	movq	%r9,%r15

	andl	$7,%r9d
	negl	%r9d
	addl	$8,%r9d
	cmpl	%r9d,%r12d
	cmovnael	%r12d,%r9d

	leaq	byte_kmask_0_to_7(%rip),%rbx
	kmovb	(%rbx,%r9,1),%k1

	movq	%r15,%rbx
	andl	$~7,%ebx
	leaq	(%r10,%rbx,4),%r10

	movq	%r15,%rbx
	andl	$7,%ebx

	vmovdqu8	(%r10),%ymm31
	vmovdqu8	%ymm31,(%r10)

	vmovdqu8	(%rsi),%xmm31{%k1}{z}
	vmovdqu8	0(%r10,%rbx,1),%xmm30{%k1}{z}
	vpxorq	%xmm30,%xmm31,%xmm31
	vmovdqu8	%xmm31,0(%r10,%rbx,1){%k1}

	vmovdqu8	(%rdx),%xmm31{%k1}{z}
	vmovdqu8	8(%r10,%rbx,1),%xmm30{%k1}{z}
	vpxorq	%xmm30,%xmm31,%xmm31
	vmovdqu8	%xmm31,8(%r10,%rbx,1){%k1}

	vmovdqu8	(%rcx),%xmm31{%k1}{z}
	vmovdqu8	16(%r10,%rbx,1),%xmm30{%k1}{z}
	vpxorq	%xmm30,%xmm31,%xmm31
	vmovdqu8	%xmm31,16(%r10,%rbx,1){%k1}

	vmovdqu8	(%r8),%xmm31{%k1}{z}
	vmovdqu8	24(%r10,%rbx,1),%xmm30{%k1}{z}
	vpxorq	%xmm30,%xmm31,%xmm31
	vmovdqu8	%xmm31,24(%r10,%rbx,1){%k1}

	subq	%r9,%r12
	jz	.Lzero_bytes

	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx
	addq	%r9,%r8
	addq	$32,%r10
	xorq	%r9,%r9
	jmp	.Lymm_loop

.Lstart_aligned_to_4x8:
	leaq	(%r10,%r9,4),%r10
	xorq	%r9,%r9

.align	32
.Lymm_loop:
	cmpl	$8,%r12d
	jb	.Llt_8_bytes

	vmovq	(%rsi,%r9,1),%xmm31
	vpinsrq	$1,(%rdx,%r9,1),%xmm31,%xmm31
	vmovq	(%rcx,%r9,1),%xmm30
	vpinsrq	$1,(%r8,%r9,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	(%r10,%r9,4),%ymm31,%ymm31
	vmovdqu64	%ymm31,(%r10,%r9,4)
	addq	$8,%r9
	subq	$8,%r12
	jz	.Lzero_bytes

	jmp	.Lymm_loop

.align	32
.Lzero_bytes:
	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx
	addq	%r9,%r8
	.byte	0xf3,0xc3

.align	32
.Llt_8_bytes:
	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx
	addq	%r9,%r8
	leaq	(%r10,%r9,4),%r10

	leaq	byte_kmask_0_to_7(%rip),%rbx
	kmovb	(%rbx,%r12,1),%k1

	vmovdqu8	(%rsi),%xmm31{%k1}{z}
	vmovdqu8	(%rdx),%xmm30{%k1}{z}
	vpunpcklqdq	%xmm30,%xmm31,%xmm31
	vmovdqu8	(%rcx),%xmm30{%k1}{z}
	vmovdqu8	(%r8),%xmm29{%k1}{z}
	vpunpcklqdq	%xmm29,%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31

	vpxorq	(%r10),%ymm31,%ymm31
	vmovdqu64	%ymm31,(%r10)

	addq	%r12,%rsi
	addq	%r12,%rdx
	addq	%r12,%rcx
	addq	%r12,%r8
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_partial_add_x4,.-keccak_1600_partial_add_x4











.globl	keccak_1600_extract_bytes_x4
.type	keccak_1600_extract_bytes_x4,@function
.align	32
keccak_1600_extract_bytes_x4:
.cfi_startproc	
	orq	%r12,%r12
	jz	.Lextract_zero_bytes

	testl	$7,%r11d
	jz	.Lextract_start_aligned_to_4x8


	movq	%r11,%r9

	andl	$7,%r9d
	negl	%r9d
	addl	$8,%r9d
	cmpl	%r9d,%r12d
	cmovnael	%r12d,%r9d

	leaq	byte_kmask_0_to_7(%rip),%rbx
	kmovb	(%rbx,%r9,1),%k1

	movq	%r11,%rbx
	andl	$~7,%ebx
	leaq	(%r10,%rbx,4),%r10

	movq	%r11,%rbx
	andl	$7,%ebx

	vmovdqu8	0(%r10,%rbx,1),%xmm31{%k1}{z}
	vmovdqu8	%xmm31,(%rdi){%k1}

	vmovdqu8	8(%r10,%rbx,1),%xmm31{%k1}{z}
	vmovdqu8	%xmm31,(%rsi){%k1}

	vmovdqu8	16(%r10,%rbx,1),%xmm31{%k1}{z}
	vmovdqu8	%xmm31,(%rdx){%k1}

	vmovdqu8	24(%r10,%rbx,1),%xmm31{%k1}{z}
	vmovdqu8	%xmm31,(%rcx){%k1}


	addq	%r9,%rdi
	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx


	subq	%r9,%r12
	jz	.Lextract_zero_bytes


	addq	$32,%r10
	xorq	%r9,%r9
	jmp	.Lextract_ymm_loop

.Lextract_start_aligned_to_4x8:
	leaq	(%r10,%r11,4),%r10
	xorq	%r9,%r9

.align	32
.Lextract_ymm_loop:
	cmpq	$8,%r12
	jb	.Lextract_lt_8_bytes

	vmovdqu64	(%r10),%xmm31
	vmovdqu64	16(%r10),%xmm30
	vmovq	%xmm31,(%rdi,%r9,1)
	vpextrq	$1,%xmm31,(%rsi,%r9,1)
	vmovq	%xmm30,(%rdx,%r9,1)
	vpextrq	$1,%xmm30,(%rcx,%r9,1)
	addq	$8,%r9
	subq	$8,%r12
	jz	.Lzero_bytes_left

	addq	$32,%r10
	jmp	.Lextract_ymm_loop

.align	32
.Lzero_bytes_left:

	addq	%r9,%rdi
	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx
.Lextract_zero_bytes:
	.byte	0xf3,0xc3

.align	32
.Lextract_lt_8_bytes:
	addq	%r9,%rdi
	addq	%r9,%rsi
	addq	%r9,%rdx
	addq	%r9,%rcx

	leaq	byte_kmask_0_to_7(%rip),%r9
	kmovb	(%r9,%r12,1),%k1

	vmovq	0(%r10),%xmm31
	vmovdqu8	%xmm31,(%rdi){%k1}
	vmovq	8(%r10),%xmm31
	vmovdqu8	%xmm31,(%rsi){%k1}
	vmovq	16(%r10),%xmm31
	vmovdqu8	%xmm31,(%rdx){%k1}
	vmovq	24(%r10),%xmm31
	vmovdqu8	%xmm31,(%rcx){%k1}


	addq	%r12,%rdi
	addq	%r12,%rsi
	addq	%r12,%rdx
	addq	%r12,%rcx
	.byte	0xf3,0xc3
.cfi_endproc	
.size	keccak_1600_extract_bytes_x4,.-keccak_1600_extract_bytes_x4





















.globl	SHA3_shake128_x4_avx512vl
.type	SHA3_shake128_x4_avx512vl,@function
.align	32
SHA3_shake128_x4_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	movq	%rsp,%rbp
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24

	subq	$856,%rsp
	movq	%rsp,%rbx

.Lshake128_x4_body:
	movq	%rdi,0(%rbx)
	movq	%rsi,8(%rbx)
	movq	%rdx,16(%rbx)
	movq	%rcx,24(%rbx)
	movq	%r8,32(%rbx)

	leaq	48(%rbx),%rdi
	movq	%rdi,40(%rbx)


	call	keccak_1600_init_state

	call	keccak_1600_save_state_x4

	movq	$0,800(%rdi)

	movq	40(%rbx),%rdi
	movq	%r9,%rsi
	movq	16(%rbp),%rdx
	movq	24(%rbp),%rcx
	movq	32(%rbp),%r8
	movq	40(%rbp),%r9

	call	.L_SHA3_shake128_x4_inc_absorb_avx512vl

	movq	40(%rbx),%rdi
	call	.L_SHA3_shake128_x4_inc_finalize_avx512vl


	movq	0(%rbx),%rdi
	movq	8(%rbx),%rsi
	movq	16(%rbx),%rdx
	movq	24(%rbx),%rcx
	movq	32(%rbx),%r8
	movq	40(%rbx),%r9
	call	.L_SHA3_shake128_x4_inc_squeeze_avx512vl


	leaq	48(%rbx),%r9
	vpxorq	%ymm31,%ymm31,%ymm31
	vmovdqu64	%ymm31,0(%r9)
	vmovdqu64	%ymm31,32(%r9)
	vmovdqu64	%ymm31,64(%r9)
	vmovdqu64	%ymm31,96(%r9)
	vmovdqu64	%ymm31,128(%r9)
	vmovdqu64	%ymm31,160(%r9)
	vmovdqu64	%ymm31,192(%r9)
	vmovdqu64	%ymm31,224(%r9)
	vmovdqu64	%ymm31,256(%r9)
	vmovdqu64	%ymm31,288(%r9)
	vmovdqu64	%ymm31,320(%r9)
	vmovdqu64	%ymm31,352(%r9)
	vmovdqu64	%ymm31,384(%r9)
	vmovdqu64	%ymm31,416(%r9)
	vmovdqu64	%ymm31,448(%r9)
	vmovdqu64	%ymm31,480(%r9)
	vmovdqu64	%ymm31,512(%r9)
	vmovdqu64	%ymm31,544(%r9)
	vmovdqu64	%ymm31,576(%r9)
	vmovdqu64	%ymm31,608(%r9)
	vmovdqu64	%ymm31,640(%r9)
	vmovdqu64	%ymm31,672(%r9)
	vmovdqu64	%ymm31,704(%r9)
	vmovdqu64	%ymm31,736(%r9)
	vmovdqu64	%ymm31,768(%r9)
	vmovq	%xmm31,800(%r9)

.Lshake128_x4_epilogue:
	addq	$856,%rsp
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake128_x4_avx512vl,.-SHA3_shake128_x4_avx512vl













.globl	SHA3_shake128_x4_inc_absorb_avx512vl
.type	SHA3_shake128_x4_inc_absorb_avx512vl,@function
.align	32
SHA3_shake128_x4_inc_absorb_avx512vl:
.L_SHA3_shake128_x4_inc_absorb_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56

.Lshake128_absorb_body:

	movq	800(%rdi),%r14
	orq	%r14,%r14
	je	.Lshake128_absorb_main_loop_start


	movq	$168,%r12
	subq	%r14,%r12

	cmpq	%r12,%r9
	jbe	.Lshake128_absorb_skip_permute

	subq	%r12,%r9
	movq	%r9,%r11


	movq	%rdi,%r10
	call	keccak_1600_partial_add_x4

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	movq	$0,800(%rdi)
	jmp	.Lshake128_absorb_partial_block_done

.Lshake128_absorb_skip_permute:

	movq	%rdi,%r10
	movq	%r9,%r12
	movq	%r9,%r11
	call	keccak_1600_partial_add_x4

	leaq	(%r11,%r14,1),%r15
	movq	%r15,800(%rdi)

	cmpq	$168,%r15
	jb	.Lshake128_absorb_exit

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	call	keccak_1600_save_state_x4

	movq	$0,800(%rdi)
	jmp	.Lshake128_absorb_exit

.Lshake128_absorb_main_loop_start:
	call	keccak_1600_load_state_x4
	movq	%r9,%r11

.Lshake128_absorb_partial_block_done:
	xorq	%r12,%r12


.align	32
.Lshake128_absorb_while_loop:
	cmpq	$168,%r11
	jb	.Lshake128_absorb_while_loop_done


	vmovq	0(%rsi,%r12,1),%xmm31
	vpinsrq	$1,0(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	0(%rcx,%r12,1),%xmm30
	vpinsrq	$1,0(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm0,%ymm0
	vmovq	8(%rsi,%r12,1),%xmm31
	vpinsrq	$1,8(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	8(%rcx,%r12,1),%xmm30
	vpinsrq	$1,8(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm1,%ymm1
	vmovq	16(%rsi,%r12,1),%xmm31
	vpinsrq	$1,16(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	16(%rcx,%r12,1),%xmm30
	vpinsrq	$1,16(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm2,%ymm2
	vmovq	24(%rsi,%r12,1),%xmm31
	vpinsrq	$1,24(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	24(%rcx,%r12,1),%xmm30
	vpinsrq	$1,24(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm3,%ymm3
	vmovq	32(%rsi,%r12,1),%xmm31
	vpinsrq	$1,32(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	32(%rcx,%r12,1),%xmm30
	vpinsrq	$1,32(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm4,%ymm4
	vmovq	40(%rsi,%r12,1),%xmm31
	vpinsrq	$1,40(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	40(%rcx,%r12,1),%xmm30
	vpinsrq	$1,40(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm5,%ymm5
	vmovq	48(%rsi,%r12,1),%xmm31
	vpinsrq	$1,48(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	48(%rcx,%r12,1),%xmm30
	vpinsrq	$1,48(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm6,%ymm6
	vmovq	56(%rsi,%r12,1),%xmm31
	vpinsrq	$1,56(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	56(%rcx,%r12,1),%xmm30
	vpinsrq	$1,56(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm7,%ymm7
	vmovq	64(%rsi,%r12,1),%xmm31
	vpinsrq	$1,64(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	64(%rcx,%r12,1),%xmm30
	vpinsrq	$1,64(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm8,%ymm8
	vmovq	72(%rsi,%r12,1),%xmm31
	vpinsrq	$1,72(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	72(%rcx,%r12,1),%xmm30
	vpinsrq	$1,72(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm9,%ymm9
	vmovq	80(%rsi,%r12,1),%xmm31
	vpinsrq	$1,80(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	80(%rcx,%r12,1),%xmm30
	vpinsrq	$1,80(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm10,%ymm10
	vmovq	88(%rsi,%r12,1),%xmm31
	vpinsrq	$1,88(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	88(%rcx,%r12,1),%xmm30
	vpinsrq	$1,88(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm11,%ymm11
	vmovq	96(%rsi,%r12,1),%xmm31
	vpinsrq	$1,96(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	96(%rcx,%r12,1),%xmm30
	vpinsrq	$1,96(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm12,%ymm12
	vmovq	104(%rsi,%r12,1),%xmm31
	vpinsrq	$1,104(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	104(%rcx,%r12,1),%xmm30
	vpinsrq	$1,104(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm13,%ymm13
	vmovq	112(%rsi,%r12,1),%xmm31
	vpinsrq	$1,112(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	112(%rcx,%r12,1),%xmm30
	vpinsrq	$1,112(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm14,%ymm14
	vmovq	120(%rsi,%r12,1),%xmm31
	vpinsrq	$1,120(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	120(%rcx,%r12,1),%xmm30
	vpinsrq	$1,120(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm15,%ymm15
	vmovq	128(%rsi,%r12,1),%xmm31
	vpinsrq	$1,128(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	128(%rcx,%r12,1),%xmm30
	vpinsrq	$1,128(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm16,%ymm16
	vmovq	136(%rsi,%r12,1),%xmm31
	vpinsrq	$1,136(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	136(%rcx,%r12,1),%xmm30
	vpinsrq	$1,136(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm17,%ymm17
	vmovq	144(%rsi,%r12,1),%xmm31
	vpinsrq	$1,144(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	144(%rcx,%r12,1),%xmm30
	vpinsrq	$1,144(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm18,%ymm18
	vmovq	152(%rsi,%r12,1),%xmm31
	vpinsrq	$1,152(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	152(%rcx,%r12,1),%xmm30
	vpinsrq	$1,152(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm19,%ymm19
	vmovq	160(%rsi,%r12,1),%xmm31
	vpinsrq	$1,160(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	160(%rcx,%r12,1),%xmm30
	vpinsrq	$1,160(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm20,%ymm20
	subq	$168,%r11
	addq	$168,%r12
	call	keccak_1600_permute

	jmp	.Lshake128_absorb_while_loop

.align	32
.Lshake128_absorb_while_loop_done:
	call	keccak_1600_save_state_x4

	movq	%r11,800(%rdi)
	orq	%r11,%r11
	jz	.Lshake128_absorb_exit

	movq	$0,800(%rdi)


	movq	%rdi,%r10
	addq	%r12,%rsi
	addq	%r12,%rdx
	addq	%r12,%rcx
	addq	%r12,%r8
	movq	%r11,%r12
	call	keccak_1600_partial_add_x4

	movq	%r11,800(%rdi)

.Lshake128_absorb_exit:

	vpxorq	%xmm16,%xmm16,%xmm16
	vmovdqa64	%ymm16,%ymm17
	vmovdqa64	%ymm16,%ymm18
	vmovdqa64	%ymm16,%ymm19
	vmovdqa64	%ymm16,%ymm20
	vmovdqa64	%ymm16,%ymm21
	vmovdqa64	%ymm16,%ymm22
	vmovdqa64	%ymm16,%ymm23
	vmovdqa64	%ymm16,%ymm24
	vmovdqa64	%ymm16,%ymm25
	vmovdqa64	%ymm16,%ymm26
	vmovdqa64	%ymm16,%ymm27
	vmovdqa64	%ymm16,%ymm28
	vmovdqa64	%ymm16,%ymm29
	vmovdqa64	%ymm16,%ymm30
	vmovdqa64	%ymm16,%ymm31
.Lshake128_absorb_epilogue:
	vzeroall

	popq	%r15
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r15
	popq	%r14
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r14
	popq	%r13
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r13
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake128_x4_inc_absorb_avx512vl,.-SHA3_shake128_x4_inc_absorb_avx512vl









.globl	SHA3_shake128_x4_inc_finalize_avx512vl
.type	SHA3_shake128_x4_inc_finalize_avx512vl,@function
.align	32
SHA3_shake128_x4_inc_finalize_avx512vl:
.L_SHA3_shake128_x4_inc_finalize_avx512vl:
.cfi_startproc	
	movq	800(%rdi),%r11
	movq	%r11,%r10
	andl	$~7,%r10d
	andl	$7,%r11d


	vmovdqu32	(%rdi,%r10,4),%ymm31
	leaq	shake_msg_pad_x4(%rip),%r9
	subq	%r11,%r9
	vmovdqu32	(%r9),%ymm30
	vpxorq	%ymm30,%ymm31,%ymm31
	vmovdqu32	%ymm31,(%rdi,%r10,4)


	vmovdqu32	640(%rdi),%ymm31
	vmovdqa32	shake_terminator_byte_x4(%rip),%ymm30
	vpxorq	%ymm30,%ymm31,%ymm31
	vmovdqu32	%ymm31,640(%rdi)

	movq	$0,800(%rdi)
	vpxorq	%ymm31,%ymm31,%ymm31
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake128_x4_inc_finalize_avx512vl,.-SHA3_shake128_x4_inc_finalize_avx512vl













.globl	SHA3_shake128_x4_inc_squeeze_avx512vl
.type	SHA3_shake128_x4_inc_squeeze_avx512vl,@function
.align	32
SHA3_shake128_x4_inc_squeeze_avx512vl:
.L_SHA3_shake128_x4_inc_squeeze_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56

.Lshake128_squeeze_body:
	orq	%r8,%r8
	jz	.Lshake128_squeeze_done


	movq	800(%r9),%r15
	orq	%r15,%r15
	jnz	.Lshake128_squeeze_no_init_permute

	movq	%rdi,%r14
	movq	%r9,%rdi
	call	keccak_1600_load_state_x4

	movq	%r14,%rdi

	xorq	%rbp,%rbp
	jmp	.Lshake128_squeeze_loop

.align	32
.Lshake128_squeeze_no_init_permute:

	movq	%r9,%r10
	movq	%r9,%r14

	movq	%r15,%r12
	cmpq	%r15,%r8
	cmovnaeq	%r8,%r12

	subq	%r12,%r8

	movl	$168,%r11d
	subq	%r15,%r11

	subq	%r12,%r15
	movq	%r15,800(%r9)

	call	keccak_1600_extract_bytes_x4
	movq	%r14,%r9

	orq	%r15,%r15
	jnz	.Lshake128_squeeze_done

	movq	%rdi,%r13
	movq	%r14,%rdi
	call	keccak_1600_load_state_x4

	movq	%r13,%rdi
	xorq	%rbp,%rbp

.align	32
.Lshake128_squeeze_loop:
	cmpq	$168,%r8
	jb	.Lshake128_squeeze_final_extract

	call	keccak_1600_permute


	vextracti64x2	$1,%ymm0,%xmm31
	vmovq	%xmm0,0(%rdi,%rbp,1)
	vpextrq	$1,%xmm0,0(%rsi,%rbp,1)
	vmovq	%xmm31,0(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,0(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm1,%xmm31
	vmovq	%xmm1,8(%rdi,%rbp,1)
	vpextrq	$1,%xmm1,8(%rsi,%rbp,1)
	vmovq	%xmm31,8(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,8(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm2,%xmm31
	vmovq	%xmm2,16(%rdi,%rbp,1)
	vpextrq	$1,%xmm2,16(%rsi,%rbp,1)
	vmovq	%xmm31,16(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,16(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm3,%xmm31
	vmovq	%xmm3,24(%rdi,%rbp,1)
	vpextrq	$1,%xmm3,24(%rsi,%rbp,1)
	vmovq	%xmm31,24(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,24(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm4,%xmm31
	vmovq	%xmm4,32(%rdi,%rbp,1)
	vpextrq	$1,%xmm4,32(%rsi,%rbp,1)
	vmovq	%xmm31,32(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,32(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm5,%xmm31
	vmovq	%xmm5,40(%rdi,%rbp,1)
	vpextrq	$1,%xmm5,40(%rsi,%rbp,1)
	vmovq	%xmm31,40(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,40(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm6,%xmm31
	vmovq	%xmm6,48(%rdi,%rbp,1)
	vpextrq	$1,%xmm6,48(%rsi,%rbp,1)
	vmovq	%xmm31,48(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,48(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm7,%xmm31
	vmovq	%xmm7,56(%rdi,%rbp,1)
	vpextrq	$1,%xmm7,56(%rsi,%rbp,1)
	vmovq	%xmm31,56(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,56(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm8,%xmm31
	vmovq	%xmm8,64(%rdi,%rbp,1)
	vpextrq	$1,%xmm8,64(%rsi,%rbp,1)
	vmovq	%xmm31,64(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,64(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm9,%xmm31
	vmovq	%xmm9,72(%rdi,%rbp,1)
	vpextrq	$1,%xmm9,72(%rsi,%rbp,1)
	vmovq	%xmm31,72(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,72(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm10,%xmm31
	vmovq	%xmm10,80(%rdi,%rbp,1)
	vpextrq	$1,%xmm10,80(%rsi,%rbp,1)
	vmovq	%xmm31,80(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,80(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm11,%xmm31
	vmovq	%xmm11,88(%rdi,%rbp,1)
	vpextrq	$1,%xmm11,88(%rsi,%rbp,1)
	vmovq	%xmm31,88(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,88(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm12,%xmm31
	vmovq	%xmm12,96(%rdi,%rbp,1)
	vpextrq	$1,%xmm12,96(%rsi,%rbp,1)
	vmovq	%xmm31,96(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,96(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm13,%xmm31
	vmovq	%xmm13,104(%rdi,%rbp,1)
	vpextrq	$1,%xmm13,104(%rsi,%rbp,1)
	vmovq	%xmm31,104(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,104(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm14,%xmm31
	vmovq	%xmm14,112(%rdi,%rbp,1)
	vpextrq	$1,%xmm14,112(%rsi,%rbp,1)
	vmovq	%xmm31,112(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,112(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm15,%xmm31
	vmovq	%xmm15,120(%rdi,%rbp,1)
	vpextrq	$1,%xmm15,120(%rsi,%rbp,1)
	vmovq	%xmm31,120(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,120(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm16,%xmm31
	vmovq	%xmm16,128(%rdi,%rbp,1)
	vpextrq	$1,%xmm16,128(%rsi,%rbp,1)
	vmovq	%xmm31,128(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,128(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm17,%xmm31
	vmovq	%xmm17,136(%rdi,%rbp,1)
	vpextrq	$1,%xmm17,136(%rsi,%rbp,1)
	vmovq	%xmm31,136(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,136(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm18,%xmm31
	vmovq	%xmm18,144(%rdi,%rbp,1)
	vpextrq	$1,%xmm18,144(%rsi,%rbp,1)
	vmovq	%xmm31,144(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,144(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm19,%xmm31
	vmovq	%xmm19,152(%rdi,%rbp,1)
	vpextrq	$1,%xmm19,152(%rsi,%rbp,1)
	vmovq	%xmm31,152(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,152(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm20,%xmm31
	vmovq	%xmm20,160(%rdi,%rbp,1)
	vpextrq	$1,%xmm20,160(%rsi,%rbp,1)
	vmovq	%xmm31,160(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,160(%rcx,%rbp,1)
	addq	$168,%rbp
	subq	$168,%r8
	jmp	.Lshake128_squeeze_loop

.align	32
.Lshake128_squeeze_final_extract:
	orq	%r8,%r8
	jz	.Lshake128_squeeze_no_end_permute


	addq	%rbp,%rdi
	addq	%rbp,%rsi
	addq	%rbp,%rdx
	addq	%rbp,%rcx

	movl	$168,%r15d
	subq	%r8,%r15
	movq	%r15,800(%r9)

	call	keccak_1600_permute

	movq	%rdi,%r14
	movq	%r9,%rdi
	call	keccak_1600_save_state_x4

	movq	%r14,%rdi


	movq	%r9,%r10
	movq	%r8,%r12
	xorq	%r11,%r11
	call	keccak_1600_extract_bytes_x4

	jmp	.Lshake128_squeeze_done

.Lshake128_squeeze_no_end_permute:
	movq	$0,800(%r9)
	movq	%r9,%rdi
	call	keccak_1600_save_state_x4

.Lshake128_squeeze_done:

	vpxorq	%xmm16,%xmm16,%xmm16
	vmovdqa64	%ymm16,%ymm17
	vmovdqa64	%ymm16,%ymm18
	vmovdqa64	%ymm16,%ymm19
	vmovdqa64	%ymm16,%ymm20
	vmovdqa64	%ymm16,%ymm21
	vmovdqa64	%ymm16,%ymm22
	vmovdqa64	%ymm16,%ymm23
	vmovdqa64	%ymm16,%ymm24
	vmovdqa64	%ymm16,%ymm25
	vmovdqa64	%ymm16,%ymm26
	vmovdqa64	%ymm16,%ymm27
	vmovdqa64	%ymm16,%ymm28
	vmovdqa64	%ymm16,%ymm29
	vmovdqa64	%ymm16,%ymm30
	vmovdqa64	%ymm16,%ymm31
.Lshake128_squeeze_epilogue:
	vzeroall

	popq	%r15
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r15
	popq	%r14
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r14
	popq	%r13
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r13
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake128_x4_inc_squeeze_avx512vl,.-SHA3_shake128_x4_inc_squeeze_avx512vl





















.globl	SHA3_shake256_x4_avx512vl
.type	SHA3_shake256_x4_avx512vl,@function
.align	32
SHA3_shake256_x4_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	movq	%rsp,%rbp
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24

	subq	$856,%rsp
	movq	%rsp,%rbx

.Lshake256_x4_body:
	movq	%rdi,0(%rbx)
	movq	%rsi,8(%rbx)
	movq	%rdx,16(%rbx)
	movq	%rcx,24(%rbx)
	movq	%r8,32(%rbx)

	leaq	48(%rbx),%rdi
	movq	%rdi,40(%rbx)


	call	keccak_1600_init_state

	call	keccak_1600_save_state_x4

	movq	$0,800(%rdi)

	movq	40(%rbx),%rdi
	movq	%r9,%rsi
	movq	16(%rbp),%rdx
	movq	24(%rbp),%rcx
	movq	32(%rbp),%r8
	movq	40(%rbp),%r9

	call	.L_SHA3_shake256_x4_inc_absorb_avx512vl

	movq	40(%rbx),%rdi
	call	.L_SHA3_shake256_x4_inc_finalize_avx512vl


	movq	0(%rbx),%rdi
	movq	8(%rbx),%rsi
	movq	16(%rbx),%rdx
	movq	24(%rbx),%rcx
	movq	32(%rbx),%r8
	movq	40(%rbx),%r9
	call	.L_SHA3_shake256_x4_inc_squeeze_avx512vl


	leaq	48(%rbx),%r9
	vpxorq	%ymm31,%ymm31,%ymm31
	vmovdqu64	%ymm31,0(%r9)
	vmovdqu64	%ymm31,32(%r9)
	vmovdqu64	%ymm31,64(%r9)
	vmovdqu64	%ymm31,96(%r9)
	vmovdqu64	%ymm31,128(%r9)
	vmovdqu64	%ymm31,160(%r9)
	vmovdqu64	%ymm31,192(%r9)
	vmovdqu64	%ymm31,224(%r9)
	vmovdqu64	%ymm31,256(%r9)
	vmovdqu64	%ymm31,288(%r9)
	vmovdqu64	%ymm31,320(%r9)
	vmovdqu64	%ymm31,352(%r9)
	vmovdqu64	%ymm31,384(%r9)
	vmovdqu64	%ymm31,416(%r9)
	vmovdqu64	%ymm31,448(%r9)
	vmovdqu64	%ymm31,480(%r9)
	vmovdqu64	%ymm31,512(%r9)
	vmovdqu64	%ymm31,544(%r9)
	vmovdqu64	%ymm31,576(%r9)
	vmovdqu64	%ymm31,608(%r9)
	vmovdqu64	%ymm31,640(%r9)
	vmovdqu64	%ymm31,672(%r9)
	vmovdqu64	%ymm31,704(%r9)
	vmovdqu64	%ymm31,736(%r9)
	vmovdqu64	%ymm31,768(%r9)
	vmovq	%xmm31,800(%r9)

.Lshake256_x4_epilogue:
	addq	$856,%rsp
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake256_x4_avx512vl,.-SHA3_shake256_x4_avx512vl













.globl	SHA3_shake256_x4_inc_absorb_avx512vl
.type	SHA3_shake256_x4_inc_absorb_avx512vl,@function
.align	32
SHA3_shake256_x4_inc_absorb_avx512vl:
.L_SHA3_shake256_x4_inc_absorb_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56

.Lshake256_absorb_body:

	movq	800(%rdi),%r14
	orq	%r14,%r14
	je	.Lshake256_absorb_main_loop_start


	movq	$136,%r12
	subq	%r14,%r12

	cmpq	%r12,%r9
	jbe	.Lshake256_absorb_skip_permute

	subq	%r12,%r9
	movq	%r9,%r11


	movq	%rdi,%r10
	call	keccak_1600_partial_add_x4

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	movq	$0,800(%rdi)
	jmp	.Lshake256_absorb_partial_block_done

.Lshake256_absorb_skip_permute:

	movq	%rdi,%r10
	movq	%r9,%r12
	movq	%r9,%r11
	call	keccak_1600_partial_add_x4

	leaq	(%r11,%r14,1),%r15
	movq	%r15,800(%rdi)

	cmpq	$136,%r15
	jb	.Lshake256_absorb_exit

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	call	keccak_1600_save_state_x4

	movq	$0,800(%rdi)
	jmp	.Lshake256_absorb_exit

.Lshake256_absorb_main_loop_start:
	call	keccak_1600_load_state_x4
	movq	%r9,%r11

.Lshake256_absorb_partial_block_done:
	xorq	%r12,%r12


.align	32
.Lshake256_absorb_while_loop:
	cmpq	$136,%r11
	jb	.Lshake256_absorb_while_loop_done


	vmovq	0(%rsi,%r12,1),%xmm31
	vpinsrq	$1,0(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	0(%rcx,%r12,1),%xmm30
	vpinsrq	$1,0(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm0,%ymm0
	vmovq	8(%rsi,%r12,1),%xmm31
	vpinsrq	$1,8(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	8(%rcx,%r12,1),%xmm30
	vpinsrq	$1,8(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm1,%ymm1
	vmovq	16(%rsi,%r12,1),%xmm31
	vpinsrq	$1,16(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	16(%rcx,%r12,1),%xmm30
	vpinsrq	$1,16(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm2,%ymm2
	vmovq	24(%rsi,%r12,1),%xmm31
	vpinsrq	$1,24(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	24(%rcx,%r12,1),%xmm30
	vpinsrq	$1,24(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm3,%ymm3
	vmovq	32(%rsi,%r12,1),%xmm31
	vpinsrq	$1,32(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	32(%rcx,%r12,1),%xmm30
	vpinsrq	$1,32(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm4,%ymm4
	vmovq	40(%rsi,%r12,1),%xmm31
	vpinsrq	$1,40(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	40(%rcx,%r12,1),%xmm30
	vpinsrq	$1,40(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm5,%ymm5
	vmovq	48(%rsi,%r12,1),%xmm31
	vpinsrq	$1,48(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	48(%rcx,%r12,1),%xmm30
	vpinsrq	$1,48(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm6,%ymm6
	vmovq	56(%rsi,%r12,1),%xmm31
	vpinsrq	$1,56(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	56(%rcx,%r12,1),%xmm30
	vpinsrq	$1,56(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm7,%ymm7
	vmovq	64(%rsi,%r12,1),%xmm31
	vpinsrq	$1,64(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	64(%rcx,%r12,1),%xmm30
	vpinsrq	$1,64(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm8,%ymm8
	vmovq	72(%rsi,%r12,1),%xmm31
	vpinsrq	$1,72(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	72(%rcx,%r12,1),%xmm30
	vpinsrq	$1,72(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm9,%ymm9
	vmovq	80(%rsi,%r12,1),%xmm31
	vpinsrq	$1,80(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	80(%rcx,%r12,1),%xmm30
	vpinsrq	$1,80(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm10,%ymm10
	vmovq	88(%rsi,%r12,1),%xmm31
	vpinsrq	$1,88(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	88(%rcx,%r12,1),%xmm30
	vpinsrq	$1,88(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm11,%ymm11
	vmovq	96(%rsi,%r12,1),%xmm31
	vpinsrq	$1,96(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	96(%rcx,%r12,1),%xmm30
	vpinsrq	$1,96(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm12,%ymm12
	vmovq	104(%rsi,%r12,1),%xmm31
	vpinsrq	$1,104(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	104(%rcx,%r12,1),%xmm30
	vpinsrq	$1,104(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm13,%ymm13
	vmovq	112(%rsi,%r12,1),%xmm31
	vpinsrq	$1,112(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	112(%rcx,%r12,1),%xmm30
	vpinsrq	$1,112(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm14,%ymm14
	vmovq	120(%rsi,%r12,1),%xmm31
	vpinsrq	$1,120(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	120(%rcx,%r12,1),%xmm30
	vpinsrq	$1,120(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm15,%ymm15
	vmovq	128(%rsi,%r12,1),%xmm31
	vpinsrq	$1,128(%rdx,%r12,1),%xmm31,%xmm31
	vmovq	128(%rcx,%r12,1),%xmm30
	vpinsrq	$1,128(%r8,%r12,1),%xmm30,%xmm30
	vinserti32x4	$1,%xmm30,%ymm31,%ymm31
	vpxorq	%ymm31,%ymm16,%ymm16
	subq	$136,%r11
	addq	$136,%r12
	call	keccak_1600_permute

	jmp	.Lshake256_absorb_while_loop

.align	32
.Lshake256_absorb_while_loop_done:
	call	keccak_1600_save_state_x4

	movq	%r11,800(%rdi)
	orq	%r11,%r11
	jz	.Lshake256_absorb_exit

	movq	$0,800(%rdi)


	movq	%rdi,%r10
	addq	%r12,%rsi
	addq	%r12,%rdx
	addq	%r12,%rcx
	addq	%r12,%r8
	movq	%r11,%r12
	call	keccak_1600_partial_add_x4

	movq	%r11,800(%rdi)

.Lshake256_absorb_exit:

	vpxorq	%xmm16,%xmm16,%xmm16
	vmovdqa64	%ymm16,%ymm17
	vmovdqa64	%ymm16,%ymm18
	vmovdqa64	%ymm16,%ymm19
	vmovdqa64	%ymm16,%ymm20
	vmovdqa64	%ymm16,%ymm21
	vmovdqa64	%ymm16,%ymm22
	vmovdqa64	%ymm16,%ymm23
	vmovdqa64	%ymm16,%ymm24
	vmovdqa64	%ymm16,%ymm25
	vmovdqa64	%ymm16,%ymm26
	vmovdqa64	%ymm16,%ymm27
	vmovdqa64	%ymm16,%ymm28
	vmovdqa64	%ymm16,%ymm29
	vmovdqa64	%ymm16,%ymm30
	vmovdqa64	%ymm16,%ymm31
.Lshake256_absorb_epilogue:
	vzeroall

	popq	%r15
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r15
	popq	%r14
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r14
	popq	%r13
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r13
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake256_x4_inc_absorb_avx512vl,.-SHA3_shake256_x4_inc_absorb_avx512vl









.globl	SHA3_shake256_x4_inc_finalize_avx512vl
.type	SHA3_shake256_x4_inc_finalize_avx512vl,@function
.align	32
SHA3_shake256_x4_inc_finalize_avx512vl:
.L_SHA3_shake256_x4_inc_finalize_avx512vl:
.cfi_startproc	
	movq	800(%rdi),%r11
	movq	%r11,%r10
	andl	$~7,%r10d
	andl	$7,%r11d


	vmovdqu32	(%rdi,%r10,4),%ymm31
	leaq	shake_msg_pad_x4(%rip),%r9
	subq	%r11,%r9
	vmovdqu32	(%r9),%ymm30
	vpxorq	%ymm30,%ymm31,%ymm31
	vmovdqu32	%ymm31,(%rdi,%r10,4)


	vmovdqu32	512(%rdi),%ymm31
	vmovdqa32	shake_terminator_byte_x4(%rip),%ymm30
	vpxorq	%ymm30,%ymm31,%ymm31
	vmovdqu32	%ymm31,512(%rdi)

	movq	$0,800(%rdi)
	vpxorq	%ymm31,%ymm31,%ymm31
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake256_x4_inc_finalize_avx512vl,.-SHA3_shake256_x4_inc_finalize_avx512vl













.globl	SHA3_shake256_x4_inc_squeeze_avx512vl
.type	SHA3_shake256_x4_inc_squeeze_avx512vl,@function
.align	32
SHA3_shake256_x4_inc_squeeze_avx512vl:
.L_SHA3_shake256_x4_inc_squeeze_avx512vl:
.cfi_startproc	
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-16
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56

.Lshake256_squeeze_body:
	orq	%r8,%r8
	jz	.Lshake256_squeeze_done


	movq	800(%r9),%r15
	orq	%r15,%r15
	jnz	.Lshake256_squeeze_no_init_permute

	movq	%rdi,%r14
	movq	%r9,%rdi
	call	keccak_1600_load_state_x4

	movq	%r14,%rdi

	xorq	%rbp,%rbp
	jmp	.Lshake256_squeeze_loop

.align	32
.Lshake256_squeeze_no_init_permute:

	movq	%r9,%r10
	movq	%r9,%r14

	movq	%r15,%r12
	cmpq	%r15,%r8
	cmovnaeq	%r8,%r12

	subq	%r12,%r8

	movl	$136,%r11d
	subq	%r15,%r11

	subq	%r12,%r15
	movq	%r15,800(%r9)

	call	keccak_1600_extract_bytes_x4
	movq	%r14,%r9

	orq	%r15,%r15
	jnz	.Lshake256_squeeze_done

	movq	%rdi,%r13
	movq	%r14,%rdi
	call	keccak_1600_load_state_x4

	movq	%r13,%rdi
	xorq	%rbp,%rbp

.align	32
.Lshake256_squeeze_loop:
	cmpq	$136,%r8
	jb	.Lshake256_squeeze_final_extract

	call	keccak_1600_permute


	vextracti64x2	$1,%ymm0,%xmm31
	vmovq	%xmm0,0(%rdi,%rbp,1)
	vpextrq	$1,%xmm0,0(%rsi,%rbp,1)
	vmovq	%xmm31,0(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,0(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm1,%xmm31
	vmovq	%xmm1,8(%rdi,%rbp,1)
	vpextrq	$1,%xmm1,8(%rsi,%rbp,1)
	vmovq	%xmm31,8(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,8(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm2,%xmm31
	vmovq	%xmm2,16(%rdi,%rbp,1)
	vpextrq	$1,%xmm2,16(%rsi,%rbp,1)
	vmovq	%xmm31,16(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,16(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm3,%xmm31
	vmovq	%xmm3,24(%rdi,%rbp,1)
	vpextrq	$1,%xmm3,24(%rsi,%rbp,1)
	vmovq	%xmm31,24(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,24(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm4,%xmm31
	vmovq	%xmm4,32(%rdi,%rbp,1)
	vpextrq	$1,%xmm4,32(%rsi,%rbp,1)
	vmovq	%xmm31,32(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,32(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm5,%xmm31
	vmovq	%xmm5,40(%rdi,%rbp,1)
	vpextrq	$1,%xmm5,40(%rsi,%rbp,1)
	vmovq	%xmm31,40(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,40(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm6,%xmm31
	vmovq	%xmm6,48(%rdi,%rbp,1)
	vpextrq	$1,%xmm6,48(%rsi,%rbp,1)
	vmovq	%xmm31,48(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,48(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm7,%xmm31
	vmovq	%xmm7,56(%rdi,%rbp,1)
	vpextrq	$1,%xmm7,56(%rsi,%rbp,1)
	vmovq	%xmm31,56(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,56(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm8,%xmm31
	vmovq	%xmm8,64(%rdi,%rbp,1)
	vpextrq	$1,%xmm8,64(%rsi,%rbp,1)
	vmovq	%xmm31,64(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,64(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm9,%xmm31
	vmovq	%xmm9,72(%rdi,%rbp,1)
	vpextrq	$1,%xmm9,72(%rsi,%rbp,1)
	vmovq	%xmm31,72(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,72(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm10,%xmm31
	vmovq	%xmm10,80(%rdi,%rbp,1)
	vpextrq	$1,%xmm10,80(%rsi,%rbp,1)
	vmovq	%xmm31,80(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,80(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm11,%xmm31
	vmovq	%xmm11,88(%rdi,%rbp,1)
	vpextrq	$1,%xmm11,88(%rsi,%rbp,1)
	vmovq	%xmm31,88(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,88(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm12,%xmm31
	vmovq	%xmm12,96(%rdi,%rbp,1)
	vpextrq	$1,%xmm12,96(%rsi,%rbp,1)
	vmovq	%xmm31,96(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,96(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm13,%xmm31
	vmovq	%xmm13,104(%rdi,%rbp,1)
	vpextrq	$1,%xmm13,104(%rsi,%rbp,1)
	vmovq	%xmm31,104(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,104(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm14,%xmm31
	vmovq	%xmm14,112(%rdi,%rbp,1)
	vpextrq	$1,%xmm14,112(%rsi,%rbp,1)
	vmovq	%xmm31,112(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,112(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm15,%xmm31
	vmovq	%xmm15,120(%rdi,%rbp,1)
	vpextrq	$1,%xmm15,120(%rsi,%rbp,1)
	vmovq	%xmm31,120(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,120(%rcx,%rbp,1)
	vextracti64x2	$1,%ymm16,%xmm31
	vmovq	%xmm16,128(%rdi,%rbp,1)
	vpextrq	$1,%xmm16,128(%rsi,%rbp,1)
	vmovq	%xmm31,128(%rdx,%rbp,1)
	vpextrq	$1,%xmm31,128(%rcx,%rbp,1)
	addq	$136,%rbp
	subq	$136,%r8
	jmp	.Lshake256_squeeze_loop

.align	32
.Lshake256_squeeze_final_extract:
	orq	%r8,%r8
	jz	.Lshake256_squeeze_no_end_permute


	addq	%rbp,%rdi
	addq	%rbp,%rsi
	addq	%rbp,%rdx
	addq	%rbp,%rcx

	movl	$136,%r15d
	subq	%r8,%r15
	movq	%r15,800(%r9)

	call	keccak_1600_permute

	movq	%rdi,%r14
	movq	%r9,%rdi
	call	keccak_1600_save_state_x4

	movq	%r14,%rdi


	movq	%r9,%r10
	movq	%r8,%r12
	xorq	%r11,%r11
	call	keccak_1600_extract_bytes_x4

	jmp	.Lshake256_squeeze_done

.Lshake256_squeeze_no_end_permute:
	movq	$0,800(%r9)
	movq	%r9,%rdi
	call	keccak_1600_save_state_x4

.Lshake256_squeeze_done:

	vpxorq	%xmm16,%xmm16,%xmm16
	vmovdqa64	%ymm16,%ymm17
	vmovdqa64	%ymm16,%ymm18
	vmovdqa64	%ymm16,%ymm19
	vmovdqa64	%ymm16,%ymm20
	vmovdqa64	%ymm16,%ymm21
	vmovdqa64	%ymm16,%ymm22
	vmovdqa64	%ymm16,%ymm23
	vmovdqa64	%ymm16,%ymm24
	vmovdqa64	%ymm16,%ymm25
	vmovdqa64	%ymm16,%ymm26
	vmovdqa64	%ymm16,%ymm27
	vmovdqa64	%ymm16,%ymm28
	vmovdqa64	%ymm16,%ymm29
	vmovdqa64	%ymm16,%ymm30
	vmovdqa64	%ymm16,%ymm31
.Lshake256_squeeze_epilogue:
	vzeroall

	popq	%r15
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r15
	popq	%r14
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r14
	popq	%r13
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r13
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	popq	%rbx
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbx
	popq	%rbp
.cfi_adjust_cfa_offset	-8
.cfi_restore	%rbp
	.byte	0xf3,0xc3
.cfi_endproc	
.size	SHA3_shake256_x4_inc_squeeze_avx512vl,.-SHA3_shake256_x4_inc_squeeze_avx512vl

.section	.rodata
.align	128
.type	iotas,@object
iotas:
.quad	0x0000000000000001
.quad	0x0000000000008082
.quad	0x800000000000808a
.quad	0x8000000080008000
.quad	0x000000000000808b
.quad	0x0000000080000001
.quad	0x8000000080008081
.quad	0x8000000000008009
.quad	0x000000000000008a
.quad	0x0000000000000088
.quad	0x0000000080008009
.quad	0x000000008000000a
.quad	0x000000008000808b
.quad	0x800000000000008b
.quad	0x8000000000008089
.quad	0x8000000000008003
.quad	0x8000000000008002
.quad	0x8000000000000080
.quad	0x000000000000800a
.quad	0x800000008000000a
.quad	0x8000000080008081
.quad	0x8000000000008080
.quad	0x0000000080000001
.quad	0x8000000080008008
.size	iotas,.-iotas

.align	8
byte_kmask_0_to_7:
.byte	0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f

.align	32
shake_terminator_byte_x4:
.byte	0, 0, 0, 0, 0, 0, 0, 0x80
.byte	0, 0, 0, 0, 0, 0, 0, 0x80
.byte	0, 0, 0, 0, 0, 0, 0, 0x80
.byte	0, 0, 0, 0, 0, 0, 0, 0x80

.align	8
.byte	0, 0, 0, 0, 0, 0, 0, 0
shake_msg_pad_x4:
.byte	0x1F, 0, 0, 0, 0, 0, 0, 0
.byte	0x1F, 0, 0, 0, 0, 0, 0, 0
.byte	0x1F, 0, 0, 0, 0, 0, 0, 0
.byte	0x1F, 0, 0, 0, 0, 0, 0, 0

.byte	75,101,99,99,97,107,45,49,54,48,48,32,97,98,115,111,114,98,32,97,110,100,32,115,113,117,101,101,122,101,32,102,111,114,32,65,86,88,53,49,50,86,76,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
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
