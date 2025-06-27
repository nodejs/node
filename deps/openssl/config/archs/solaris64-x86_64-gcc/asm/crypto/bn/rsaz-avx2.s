.text	

.globl	rsaz_1024_sqr_avx2
.type	rsaz_1024_sqr_avx2,@function
.align	64
rsaz_1024_sqr_avx2:
.cfi_startproc	
	leaq	(%rsp),%rax
.cfi_def_cfa_register	%rax
	pushq	%rbx
.cfi_offset	%rbx,-16
	pushq	%rbp
.cfi_offset	%rbp,-24
	pushq	%r12
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_offset	%r15,-56
	vzeroupper
	movq	%rax,%rbp
.cfi_def_cfa_register	%rbp
	movq	%rdx,%r13
	subq	$832,%rsp
	movq	%r13,%r15
	subq	$-128,%rdi
	subq	$-128,%rsi
	subq	$-128,%r13

	andq	$4095,%r15
	addq	$320,%r15
	shrq	$12,%r15
	vpxor	%ymm9,%ymm9,%ymm9
	jz	.Lsqr_1024_no_n_copy





	subq	$320,%rsp
	vmovdqu	0-128(%r13),%ymm0
	andq	$-2048,%rsp
	vmovdqu	32-128(%r13),%ymm1
	vmovdqu	64-128(%r13),%ymm2
	vmovdqu	96-128(%r13),%ymm3
	vmovdqu	128-128(%r13),%ymm4
	vmovdqu	160-128(%r13),%ymm5
	vmovdqu	192-128(%r13),%ymm6
	vmovdqu	224-128(%r13),%ymm7
	vmovdqu	256-128(%r13),%ymm8
	leaq	832+128(%rsp),%r13
	vmovdqu	%ymm0,0-128(%r13)
	vmovdqu	%ymm1,32-128(%r13)
	vmovdqu	%ymm2,64-128(%r13)
	vmovdqu	%ymm3,96-128(%r13)
	vmovdqu	%ymm4,128-128(%r13)
	vmovdqu	%ymm5,160-128(%r13)
	vmovdqu	%ymm6,192-128(%r13)
	vmovdqu	%ymm7,224-128(%r13)
	vmovdqu	%ymm8,256-128(%r13)
	vmovdqu	%ymm9,288-128(%r13)

.Lsqr_1024_no_n_copy:
	andq	$-1024,%rsp

	vmovdqu	32-128(%rsi),%ymm1
	vmovdqu	64-128(%rsi),%ymm2
	vmovdqu	96-128(%rsi),%ymm3
	vmovdqu	128-128(%rsi),%ymm4
	vmovdqu	160-128(%rsi),%ymm5
	vmovdqu	192-128(%rsi),%ymm6
	vmovdqu	224-128(%rsi),%ymm7
	vmovdqu	256-128(%rsi),%ymm8

	leaq	192(%rsp),%rbx
	vmovdqu	.Land_mask(%rip),%ymm15
	jmp	.LOOP_GRANDE_SQR_1024

.align	32
.LOOP_GRANDE_SQR_1024:
	leaq	576+128(%rsp),%r9
	leaq	448(%rsp),%r12




	vpaddq	%ymm1,%ymm1,%ymm1
	vpbroadcastq	0-128(%rsi),%ymm10
	vpaddq	%ymm2,%ymm2,%ymm2
	vmovdqa	%ymm1,0-128(%r9)
	vpaddq	%ymm3,%ymm3,%ymm3
	vmovdqa	%ymm2,32-128(%r9)
	vpaddq	%ymm4,%ymm4,%ymm4
	vmovdqa	%ymm3,64-128(%r9)
	vpaddq	%ymm5,%ymm5,%ymm5
	vmovdqa	%ymm4,96-128(%r9)
	vpaddq	%ymm6,%ymm6,%ymm6
	vmovdqa	%ymm5,128-128(%r9)
	vpaddq	%ymm7,%ymm7,%ymm7
	vmovdqa	%ymm6,160-128(%r9)
	vpaddq	%ymm8,%ymm8,%ymm8
	vmovdqa	%ymm7,192-128(%r9)
	vpxor	%ymm9,%ymm9,%ymm9
	vmovdqa	%ymm8,224-128(%r9)

	vpmuludq	0-128(%rsi),%ymm10,%ymm0
	vpbroadcastq	32-128(%rsi),%ymm11
	vmovdqu	%ymm9,288-192(%rbx)
	vpmuludq	%ymm10,%ymm1,%ymm1
	vmovdqu	%ymm9,320-448(%r12)
	vpmuludq	%ymm10,%ymm2,%ymm2
	vmovdqu	%ymm9,352-448(%r12)
	vpmuludq	%ymm10,%ymm3,%ymm3
	vmovdqu	%ymm9,384-448(%r12)
	vpmuludq	%ymm10,%ymm4,%ymm4
	vmovdqu	%ymm9,416-448(%r12)
	vpmuludq	%ymm10,%ymm5,%ymm5
	vmovdqu	%ymm9,448-448(%r12)
	vpmuludq	%ymm10,%ymm6,%ymm6
	vmovdqu	%ymm9,480-448(%r12)
	vpmuludq	%ymm10,%ymm7,%ymm7
	vmovdqu	%ymm9,512-448(%r12)
	vpmuludq	%ymm10,%ymm8,%ymm8
	vpbroadcastq	64-128(%rsi),%ymm10
	vmovdqu	%ymm9,544-448(%r12)

	movq	%rsi,%r15
	movl	$4,%r14d
	jmp	.Lsqr_entry_1024
.align	32
.LOOP_SQR_1024:
	vpbroadcastq	32-128(%r15),%ymm11
	vpmuludq	0-128(%rsi),%ymm10,%ymm0
	vpaddq	0-192(%rbx),%ymm0,%ymm0
	vpmuludq	0-128(%r9),%ymm10,%ymm1
	vpaddq	32-192(%rbx),%ymm1,%ymm1
	vpmuludq	32-128(%r9),%ymm10,%ymm2
	vpaddq	64-192(%rbx),%ymm2,%ymm2
	vpmuludq	64-128(%r9),%ymm10,%ymm3
	vpaddq	96-192(%rbx),%ymm3,%ymm3
	vpmuludq	96-128(%r9),%ymm10,%ymm4
	vpaddq	128-192(%rbx),%ymm4,%ymm4
	vpmuludq	128-128(%r9),%ymm10,%ymm5
	vpaddq	160-192(%rbx),%ymm5,%ymm5
	vpmuludq	160-128(%r9),%ymm10,%ymm6
	vpaddq	192-192(%rbx),%ymm6,%ymm6
	vpmuludq	192-128(%r9),%ymm10,%ymm7
	vpaddq	224-192(%rbx),%ymm7,%ymm7
	vpmuludq	224-128(%r9),%ymm10,%ymm8
	vpbroadcastq	64-128(%r15),%ymm10
	vpaddq	256-192(%rbx),%ymm8,%ymm8
.Lsqr_entry_1024:
	vmovdqu	%ymm0,0-192(%rbx)
	vmovdqu	%ymm1,32-192(%rbx)

	vpmuludq	32-128(%rsi),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	32-128(%r9),%ymm11,%ymm14
	vpaddq	%ymm14,%ymm3,%ymm3
	vpmuludq	64-128(%r9),%ymm11,%ymm13
	vpaddq	%ymm13,%ymm4,%ymm4
	vpmuludq	96-128(%r9),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	128-128(%r9),%ymm11,%ymm14
	vpaddq	%ymm14,%ymm6,%ymm6
	vpmuludq	160-128(%r9),%ymm11,%ymm13
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	192-128(%r9),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	224-128(%r9),%ymm11,%ymm0
	vpbroadcastq	96-128(%r15),%ymm11
	vpaddq	288-192(%rbx),%ymm0,%ymm0

	vmovdqu	%ymm2,64-192(%rbx)
	vmovdqu	%ymm3,96-192(%rbx)

	vpmuludq	64-128(%rsi),%ymm10,%ymm13
	vpaddq	%ymm13,%ymm4,%ymm4
	vpmuludq	64-128(%r9),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	96-128(%r9),%ymm10,%ymm14
	vpaddq	%ymm14,%ymm6,%ymm6
	vpmuludq	128-128(%r9),%ymm10,%ymm13
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	160-128(%r9),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	192-128(%r9),%ymm10,%ymm14
	vpaddq	%ymm14,%ymm0,%ymm0
	vpmuludq	224-128(%r9),%ymm10,%ymm1
	vpbroadcastq	128-128(%r15),%ymm10
	vpaddq	320-448(%r12),%ymm1,%ymm1

	vmovdqu	%ymm4,128-192(%rbx)
	vmovdqu	%ymm5,160-192(%rbx)

	vpmuludq	96-128(%rsi),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm6,%ymm6
	vpmuludq	96-128(%r9),%ymm11,%ymm14
	vpaddq	%ymm14,%ymm7,%ymm7
	vpmuludq	128-128(%r9),%ymm11,%ymm13
	vpaddq	%ymm13,%ymm8,%ymm8
	vpmuludq	160-128(%r9),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm0,%ymm0
	vpmuludq	192-128(%r9),%ymm11,%ymm14
	vpaddq	%ymm14,%ymm1,%ymm1
	vpmuludq	224-128(%r9),%ymm11,%ymm2
	vpbroadcastq	160-128(%r15),%ymm11
	vpaddq	352-448(%r12),%ymm2,%ymm2

	vmovdqu	%ymm6,192-192(%rbx)
	vmovdqu	%ymm7,224-192(%rbx)

	vpmuludq	128-128(%rsi),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	128-128(%r9),%ymm10,%ymm14
	vpaddq	%ymm14,%ymm0,%ymm0
	vpmuludq	160-128(%r9),%ymm10,%ymm13
	vpaddq	%ymm13,%ymm1,%ymm1
	vpmuludq	192-128(%r9),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	224-128(%r9),%ymm10,%ymm3
	vpbroadcastq	192-128(%r15),%ymm10
	vpaddq	384-448(%r12),%ymm3,%ymm3

	vmovdqu	%ymm8,256-192(%rbx)
	vmovdqu	%ymm0,288-192(%rbx)
	leaq	8(%rbx),%rbx

	vpmuludq	160-128(%rsi),%ymm11,%ymm13
	vpaddq	%ymm13,%ymm1,%ymm1
	vpmuludq	160-128(%r9),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	192-128(%r9),%ymm11,%ymm14
	vpaddq	%ymm14,%ymm3,%ymm3
	vpmuludq	224-128(%r9),%ymm11,%ymm4
	vpbroadcastq	224-128(%r15),%ymm11
	vpaddq	416-448(%r12),%ymm4,%ymm4

	vmovdqu	%ymm1,320-448(%r12)
	vmovdqu	%ymm2,352-448(%r12)

	vpmuludq	192-128(%rsi),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm3,%ymm3
	vpmuludq	192-128(%r9),%ymm10,%ymm14
	vpbroadcastq	256-128(%r15),%ymm0
	vpaddq	%ymm14,%ymm4,%ymm4
	vpmuludq	224-128(%r9),%ymm10,%ymm5
	vpbroadcastq	0+8-128(%r15),%ymm10
	vpaddq	448-448(%r12),%ymm5,%ymm5

	vmovdqu	%ymm3,384-448(%r12)
	vmovdqu	%ymm4,416-448(%r12)
	leaq	8(%r15),%r15

	vpmuludq	224-128(%rsi),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	224-128(%r9),%ymm11,%ymm6
	vpaddq	480-448(%r12),%ymm6,%ymm6

	vpmuludq	256-128(%rsi),%ymm0,%ymm7
	vmovdqu	%ymm5,448-448(%r12)
	vpaddq	512-448(%r12),%ymm7,%ymm7
	vmovdqu	%ymm6,480-448(%r12)
	vmovdqu	%ymm7,512-448(%r12)
	leaq	8(%r12),%r12

	decl	%r14d
	jnz	.LOOP_SQR_1024

	vmovdqu	256(%rsp),%ymm8
	vmovdqu	288(%rsp),%ymm1
	vmovdqu	320(%rsp),%ymm2
	leaq	192(%rsp),%rbx

	vpsrlq	$29,%ymm8,%ymm14
	vpand	%ymm15,%ymm8,%ymm8
	vpsrlq	$29,%ymm1,%ymm11
	vpand	%ymm15,%ymm1,%ymm1

	vpermq	$0x93,%ymm14,%ymm14
	vpxor	%ymm9,%ymm9,%ymm9
	vpermq	$0x93,%ymm11,%ymm11

	vpblendd	$3,%ymm9,%ymm14,%ymm10
	vpblendd	$3,%ymm14,%ymm11,%ymm14
	vpaddq	%ymm10,%ymm8,%ymm8
	vpblendd	$3,%ymm11,%ymm9,%ymm11
	vpaddq	%ymm14,%ymm1,%ymm1
	vpaddq	%ymm11,%ymm2,%ymm2
	vmovdqu	%ymm1,288-192(%rbx)
	vmovdqu	%ymm2,320-192(%rbx)

	movq	(%rsp),%rax
	movq	8(%rsp),%r10
	movq	16(%rsp),%r11
	movq	24(%rsp),%r12
	vmovdqu	32(%rsp),%ymm1
	vmovdqu	64-192(%rbx),%ymm2
	vmovdqu	96-192(%rbx),%ymm3
	vmovdqu	128-192(%rbx),%ymm4
	vmovdqu	160-192(%rbx),%ymm5
	vmovdqu	192-192(%rbx),%ymm6
	vmovdqu	224-192(%rbx),%ymm7

	movq	%rax,%r9
	imull	%ecx,%eax
	andl	$0x1fffffff,%eax
	vmovd	%eax,%xmm12

	movq	%rax,%rdx
	imulq	-128(%r13),%rax
	vpbroadcastq	%xmm12,%ymm12
	addq	%rax,%r9
	movq	%rdx,%rax
	imulq	8-128(%r13),%rax
	shrq	$29,%r9
	addq	%rax,%r10
	movq	%rdx,%rax
	imulq	16-128(%r13),%rax
	addq	%r9,%r10
	addq	%rax,%r11
	imulq	24-128(%r13),%rdx
	addq	%rdx,%r12

	movq	%r10,%rax
	imull	%ecx,%eax
	andl	$0x1fffffff,%eax

	movl	$9,%r14d
	jmp	.LOOP_REDUCE_1024

.align	32
.LOOP_REDUCE_1024:
	vmovd	%eax,%xmm13
	vpbroadcastq	%xmm13,%ymm13

	vpmuludq	32-128(%r13),%ymm12,%ymm10
	movq	%rax,%rdx
	imulq	-128(%r13),%rax
	vpaddq	%ymm10,%ymm1,%ymm1
	addq	%rax,%r10
	vpmuludq	64-128(%r13),%ymm12,%ymm14
	movq	%rdx,%rax
	imulq	8-128(%r13),%rax
	vpaddq	%ymm14,%ymm2,%ymm2
	vpmuludq	96-128(%r13),%ymm12,%ymm11
.byte	0x67
	addq	%rax,%r11
.byte	0x67
	movq	%rdx,%rax
	imulq	16-128(%r13),%rax
	shrq	$29,%r10
	vpaddq	%ymm11,%ymm3,%ymm3
	vpmuludq	128-128(%r13),%ymm12,%ymm10
	addq	%rax,%r12
	addq	%r10,%r11
	vpaddq	%ymm10,%ymm4,%ymm4
	vpmuludq	160-128(%r13),%ymm12,%ymm14
	movq	%r11,%rax
	imull	%ecx,%eax
	vpaddq	%ymm14,%ymm5,%ymm5
	vpmuludq	192-128(%r13),%ymm12,%ymm11
	andl	$0x1fffffff,%eax
	vpaddq	%ymm11,%ymm6,%ymm6
	vpmuludq	224-128(%r13),%ymm12,%ymm10
	vpaddq	%ymm10,%ymm7,%ymm7
	vpmuludq	256-128(%r13),%ymm12,%ymm14
	vmovd	%eax,%xmm12

	vpaddq	%ymm14,%ymm8,%ymm8

	vpbroadcastq	%xmm12,%ymm12

	vpmuludq	32-8-128(%r13),%ymm13,%ymm11
	vmovdqu	96-8-128(%r13),%ymm14
	movq	%rax,%rdx
	imulq	-128(%r13),%rax
	vpaddq	%ymm11,%ymm1,%ymm1
	vpmuludq	64-8-128(%r13),%ymm13,%ymm10
	vmovdqu	128-8-128(%r13),%ymm11
	addq	%rax,%r11
	movq	%rdx,%rax
	imulq	8-128(%r13),%rax
	vpaddq	%ymm10,%ymm2,%ymm2
	addq	%r12,%rax
	shrq	$29,%r11
	vpmuludq	%ymm13,%ymm14,%ymm14
	vmovdqu	160-8-128(%r13),%ymm10
	addq	%r11,%rax
	vpaddq	%ymm14,%ymm3,%ymm3
	vpmuludq	%ymm13,%ymm11,%ymm11
	vmovdqu	192-8-128(%r13),%ymm14
.byte	0x67
	movq	%rax,%r12
	imull	%ecx,%eax
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	%ymm13,%ymm10,%ymm10
.byte	0xc4,0x41,0x7e,0x6f,0x9d,0x58,0x00,0x00,0x00
	andl	$0x1fffffff,%eax
	vpaddq	%ymm10,%ymm5,%ymm5
	vpmuludq	%ymm13,%ymm14,%ymm14
	vmovdqu	256-8-128(%r13),%ymm10
	vpaddq	%ymm14,%ymm6,%ymm6
	vpmuludq	%ymm13,%ymm11,%ymm11
	vmovdqu	288-8-128(%r13),%ymm9
	vmovd	%eax,%xmm0
	imulq	-128(%r13),%rax
	vpaddq	%ymm11,%ymm7,%ymm7
	vpmuludq	%ymm13,%ymm10,%ymm10
	vmovdqu	32-16-128(%r13),%ymm14
	vpbroadcastq	%xmm0,%ymm0
	vpaddq	%ymm10,%ymm8,%ymm8
	vpmuludq	%ymm13,%ymm9,%ymm9
	vmovdqu	64-16-128(%r13),%ymm11
	addq	%rax,%r12

	vmovdqu	32-24-128(%r13),%ymm13
	vpmuludq	%ymm12,%ymm14,%ymm14
	vmovdqu	96-16-128(%r13),%ymm10
	vpaddq	%ymm14,%ymm1,%ymm1
	vpmuludq	%ymm0,%ymm13,%ymm13
	vpmuludq	%ymm12,%ymm11,%ymm11
.byte	0xc4,0x41,0x7e,0x6f,0xb5,0xf0,0xff,0xff,0xff
	vpaddq	%ymm1,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm2
	vpmuludq	%ymm12,%ymm10,%ymm10
	vmovdqu	160-16-128(%r13),%ymm11
.byte	0x67
	vmovq	%xmm13,%rax
	vmovdqu	%ymm13,(%rsp)
	vpaddq	%ymm10,%ymm3,%ymm3
	vpmuludq	%ymm12,%ymm14,%ymm14
	vmovdqu	192-16-128(%r13),%ymm10
	vpaddq	%ymm14,%ymm4,%ymm4
	vpmuludq	%ymm12,%ymm11,%ymm11
	vmovdqu	224-16-128(%r13),%ymm14
	vpaddq	%ymm11,%ymm5,%ymm5
	vpmuludq	%ymm12,%ymm10,%ymm10
	vmovdqu	256-16-128(%r13),%ymm11
	vpaddq	%ymm10,%ymm6,%ymm6
	vpmuludq	%ymm12,%ymm14,%ymm14
	shrq	$29,%r12
	vmovdqu	288-16-128(%r13),%ymm10
	addq	%r12,%rax
	vpaddq	%ymm14,%ymm7,%ymm7
	vpmuludq	%ymm12,%ymm11,%ymm11

	movq	%rax,%r9
	imull	%ecx,%eax
	vpaddq	%ymm11,%ymm8,%ymm8
	vpmuludq	%ymm12,%ymm10,%ymm10
	andl	$0x1fffffff,%eax
	vmovd	%eax,%xmm12
	vmovdqu	96-24-128(%r13),%ymm11
.byte	0x67
	vpaddq	%ymm10,%ymm9,%ymm9
	vpbroadcastq	%xmm12,%ymm12

	vpmuludq	64-24-128(%r13),%ymm0,%ymm14
	vmovdqu	128-24-128(%r13),%ymm10
	movq	%rax,%rdx
	imulq	-128(%r13),%rax
	movq	8(%rsp),%r10
	vpaddq	%ymm14,%ymm2,%ymm1
	vpmuludq	%ymm0,%ymm11,%ymm11
	vmovdqu	160-24-128(%r13),%ymm14
	addq	%rax,%r9
	movq	%rdx,%rax
	imulq	8-128(%r13),%rax
.byte	0x67
	shrq	$29,%r9
	movq	16(%rsp),%r11
	vpaddq	%ymm11,%ymm3,%ymm2
	vpmuludq	%ymm0,%ymm10,%ymm10
	vmovdqu	192-24-128(%r13),%ymm11
	addq	%rax,%r10
	movq	%rdx,%rax
	imulq	16-128(%r13),%rax
	vpaddq	%ymm10,%ymm4,%ymm3
	vpmuludq	%ymm0,%ymm14,%ymm14
	vmovdqu	224-24-128(%r13),%ymm10
	imulq	24-128(%r13),%rdx
	addq	%rax,%r11
	leaq	(%r9,%r10,1),%rax
	vpaddq	%ymm14,%ymm5,%ymm4
	vpmuludq	%ymm0,%ymm11,%ymm11
	vmovdqu	256-24-128(%r13),%ymm14
	movq	%rax,%r10
	imull	%ecx,%eax
	vpmuludq	%ymm0,%ymm10,%ymm10
	vpaddq	%ymm11,%ymm6,%ymm5
	vmovdqu	288-24-128(%r13),%ymm11
	andl	$0x1fffffff,%eax
	vpaddq	%ymm10,%ymm7,%ymm6
	vpmuludq	%ymm0,%ymm14,%ymm14
	addq	24(%rsp),%rdx
	vpaddq	%ymm14,%ymm8,%ymm7
	vpmuludq	%ymm0,%ymm11,%ymm11
	vpaddq	%ymm11,%ymm9,%ymm8
	vmovq	%r12,%xmm9
	movq	%rdx,%r12

	decl	%r14d
	jnz	.LOOP_REDUCE_1024
	leaq	448(%rsp),%r12
	vpaddq	%ymm9,%ymm13,%ymm0
	vpxor	%ymm9,%ymm9,%ymm9

	vpaddq	288-192(%rbx),%ymm0,%ymm0
	vpaddq	320-448(%r12),%ymm1,%ymm1
	vpaddq	352-448(%r12),%ymm2,%ymm2
	vpaddq	384-448(%r12),%ymm3,%ymm3
	vpaddq	416-448(%r12),%ymm4,%ymm4
	vpaddq	448-448(%r12),%ymm5,%ymm5
	vpaddq	480-448(%r12),%ymm6,%ymm6
	vpaddq	512-448(%r12),%ymm7,%ymm7
	vpaddq	544-448(%r12),%ymm8,%ymm8

	vpsrlq	$29,%ymm0,%ymm14
	vpand	%ymm15,%ymm0,%ymm0
	vpsrlq	$29,%ymm1,%ymm11
	vpand	%ymm15,%ymm1,%ymm1
	vpsrlq	$29,%ymm2,%ymm12
	vpermq	$0x93,%ymm14,%ymm14
	vpand	%ymm15,%ymm2,%ymm2
	vpsrlq	$29,%ymm3,%ymm13
	vpermq	$0x93,%ymm11,%ymm11
	vpand	%ymm15,%ymm3,%ymm3
	vpermq	$0x93,%ymm12,%ymm12

	vpblendd	$3,%ymm9,%ymm14,%ymm10
	vpermq	$0x93,%ymm13,%ymm13
	vpblendd	$3,%ymm14,%ymm11,%ymm14
	vpaddq	%ymm10,%ymm0,%ymm0
	vpblendd	$3,%ymm11,%ymm12,%ymm11
	vpaddq	%ymm14,%ymm1,%ymm1
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm11,%ymm2,%ymm2
	vpblendd	$3,%ymm13,%ymm9,%ymm13
	vpaddq	%ymm12,%ymm3,%ymm3
	vpaddq	%ymm13,%ymm4,%ymm4

	vpsrlq	$29,%ymm0,%ymm14
	vpand	%ymm15,%ymm0,%ymm0
	vpsrlq	$29,%ymm1,%ymm11
	vpand	%ymm15,%ymm1,%ymm1
	vpsrlq	$29,%ymm2,%ymm12
	vpermq	$0x93,%ymm14,%ymm14
	vpand	%ymm15,%ymm2,%ymm2
	vpsrlq	$29,%ymm3,%ymm13
	vpermq	$0x93,%ymm11,%ymm11
	vpand	%ymm15,%ymm3,%ymm3
	vpermq	$0x93,%ymm12,%ymm12

	vpblendd	$3,%ymm9,%ymm14,%ymm10
	vpermq	$0x93,%ymm13,%ymm13
	vpblendd	$3,%ymm14,%ymm11,%ymm14
	vpaddq	%ymm10,%ymm0,%ymm0
	vpblendd	$3,%ymm11,%ymm12,%ymm11
	vpaddq	%ymm14,%ymm1,%ymm1
	vmovdqu	%ymm0,0-128(%rdi)
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm11,%ymm2,%ymm2
	vmovdqu	%ymm1,32-128(%rdi)
	vpblendd	$3,%ymm13,%ymm9,%ymm13
	vpaddq	%ymm12,%ymm3,%ymm3
	vmovdqu	%ymm2,64-128(%rdi)
	vpaddq	%ymm13,%ymm4,%ymm4
	vmovdqu	%ymm3,96-128(%rdi)
	vpsrlq	$29,%ymm4,%ymm14
	vpand	%ymm15,%ymm4,%ymm4
	vpsrlq	$29,%ymm5,%ymm11
	vpand	%ymm15,%ymm5,%ymm5
	vpsrlq	$29,%ymm6,%ymm12
	vpermq	$0x93,%ymm14,%ymm14
	vpand	%ymm15,%ymm6,%ymm6
	vpsrlq	$29,%ymm7,%ymm13
	vpermq	$0x93,%ymm11,%ymm11
	vpand	%ymm15,%ymm7,%ymm7
	vpsrlq	$29,%ymm8,%ymm0
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm8,%ymm8
	vpermq	$0x93,%ymm13,%ymm13

	vpblendd	$3,%ymm9,%ymm14,%ymm10
	vpermq	$0x93,%ymm0,%ymm0
	vpblendd	$3,%ymm14,%ymm11,%ymm14
	vpaddq	%ymm10,%ymm4,%ymm4
	vpblendd	$3,%ymm11,%ymm12,%ymm11
	vpaddq	%ymm14,%ymm5,%ymm5
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm11,%ymm6,%ymm6
	vpblendd	$3,%ymm13,%ymm0,%ymm13
	vpaddq	%ymm12,%ymm7,%ymm7
	vpaddq	%ymm13,%ymm8,%ymm8

	vpsrlq	$29,%ymm4,%ymm14
	vpand	%ymm15,%ymm4,%ymm4
	vpsrlq	$29,%ymm5,%ymm11
	vpand	%ymm15,%ymm5,%ymm5
	vpsrlq	$29,%ymm6,%ymm12
	vpermq	$0x93,%ymm14,%ymm14
	vpand	%ymm15,%ymm6,%ymm6
	vpsrlq	$29,%ymm7,%ymm13
	vpermq	$0x93,%ymm11,%ymm11
	vpand	%ymm15,%ymm7,%ymm7
	vpsrlq	$29,%ymm8,%ymm0
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm8,%ymm8
	vpermq	$0x93,%ymm13,%ymm13

	vpblendd	$3,%ymm9,%ymm14,%ymm10
	vpermq	$0x93,%ymm0,%ymm0
	vpblendd	$3,%ymm14,%ymm11,%ymm14
	vpaddq	%ymm10,%ymm4,%ymm4
	vpblendd	$3,%ymm11,%ymm12,%ymm11
	vpaddq	%ymm14,%ymm5,%ymm5
	vmovdqu	%ymm4,128-128(%rdi)
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm11,%ymm6,%ymm6
	vmovdqu	%ymm5,160-128(%rdi)
	vpblendd	$3,%ymm13,%ymm0,%ymm13
	vpaddq	%ymm12,%ymm7,%ymm7
	vmovdqu	%ymm6,192-128(%rdi)
	vpaddq	%ymm13,%ymm8,%ymm8
	vmovdqu	%ymm7,224-128(%rdi)
	vmovdqu	%ymm8,256-128(%rdi)

	movq	%rdi,%rsi
	decl	%r8d
	jne	.LOOP_GRANDE_SQR_1024

	vzeroall
	movq	%rbp,%rax
.cfi_def_cfa_register	%rax
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lsqr_1024_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_1024_sqr_avx2,.-rsaz_1024_sqr_avx2
.globl	rsaz_1024_mul_avx2
.type	rsaz_1024_mul_avx2,@function
.align	64
rsaz_1024_mul_avx2:
.cfi_startproc	
	leaq	(%rsp),%rax
.cfi_def_cfa_register	%rax
	pushq	%rbx
.cfi_offset	%rbx,-16
	pushq	%rbp
.cfi_offset	%rbp,-24
	pushq	%r12
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_offset	%r15,-56
	movq	%rax,%rbp
.cfi_def_cfa_register	%rbp
	vzeroall
	movq	%rdx,%r13
	subq	$64,%rsp






.byte	0x67,0x67
	movq	%rsi,%r15
	andq	$4095,%r15
	addq	$320,%r15
	shrq	$12,%r15
	movq	%rsi,%r15
	cmovnzq	%r13,%rsi
	cmovnzq	%r15,%r13

	movq	%rcx,%r15
	subq	$-128,%rsi
	subq	$-128,%rcx
	subq	$-128,%rdi

	andq	$4095,%r15
	addq	$320,%r15
.byte	0x67,0x67
	shrq	$12,%r15
	jz	.Lmul_1024_no_n_copy





	subq	$320,%rsp
	vmovdqu	0-128(%rcx),%ymm0
	andq	$-512,%rsp
	vmovdqu	32-128(%rcx),%ymm1
	vmovdqu	64-128(%rcx),%ymm2
	vmovdqu	96-128(%rcx),%ymm3
	vmovdqu	128-128(%rcx),%ymm4
	vmovdqu	160-128(%rcx),%ymm5
	vmovdqu	192-128(%rcx),%ymm6
	vmovdqu	224-128(%rcx),%ymm7
	vmovdqu	256-128(%rcx),%ymm8
	leaq	64+128(%rsp),%rcx
	vmovdqu	%ymm0,0-128(%rcx)
	vpxor	%ymm0,%ymm0,%ymm0
	vmovdqu	%ymm1,32-128(%rcx)
	vpxor	%ymm1,%ymm1,%ymm1
	vmovdqu	%ymm2,64-128(%rcx)
	vpxor	%ymm2,%ymm2,%ymm2
	vmovdqu	%ymm3,96-128(%rcx)
	vpxor	%ymm3,%ymm3,%ymm3
	vmovdqu	%ymm4,128-128(%rcx)
	vpxor	%ymm4,%ymm4,%ymm4
	vmovdqu	%ymm5,160-128(%rcx)
	vpxor	%ymm5,%ymm5,%ymm5
	vmovdqu	%ymm6,192-128(%rcx)
	vpxor	%ymm6,%ymm6,%ymm6
	vmovdqu	%ymm7,224-128(%rcx)
	vpxor	%ymm7,%ymm7,%ymm7
	vmovdqu	%ymm8,256-128(%rcx)
	vmovdqa	%ymm0,%ymm8
	vmovdqu	%ymm9,288-128(%rcx)
.Lmul_1024_no_n_copy:
	andq	$-64,%rsp

	movq	(%r13),%rbx
	vpbroadcastq	(%r13),%ymm10
	vmovdqu	%ymm0,(%rsp)
	xorq	%r9,%r9
.byte	0x67
	xorq	%r10,%r10
	xorq	%r11,%r11
	xorq	%r12,%r12

	vmovdqu	.Land_mask(%rip),%ymm15
	movl	$9,%r14d
	vmovdqu	%ymm9,288-128(%rdi)
	jmp	.Loop_mul_1024

.align	32
.Loop_mul_1024:
	vpsrlq	$29,%ymm3,%ymm9
	movq	%rbx,%rax
	imulq	-128(%rsi),%rax
	addq	%r9,%rax
	movq	%rbx,%r10
	imulq	8-128(%rsi),%r10
	addq	8(%rsp),%r10

	movq	%rax,%r9
	imull	%r8d,%eax
	andl	$0x1fffffff,%eax

	movq	%rbx,%r11
	imulq	16-128(%rsi),%r11
	addq	16(%rsp),%r11

	movq	%rbx,%r12
	imulq	24-128(%rsi),%r12
	addq	24(%rsp),%r12
	vpmuludq	32-128(%rsi),%ymm10,%ymm0
	vmovd	%eax,%xmm11
	vpaddq	%ymm0,%ymm1,%ymm1
	vpmuludq	64-128(%rsi),%ymm10,%ymm12
	vpbroadcastq	%xmm11,%ymm11
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	96-128(%rsi),%ymm10,%ymm13
	vpand	%ymm15,%ymm3,%ymm3
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	128-128(%rsi),%ymm10,%ymm0
	vpaddq	%ymm0,%ymm4,%ymm4
	vpmuludq	160-128(%rsi),%ymm10,%ymm12
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	192-128(%rsi),%ymm10,%ymm13
	vpaddq	%ymm13,%ymm6,%ymm6
	vpmuludq	224-128(%rsi),%ymm10,%ymm0
	vpermq	$0x93,%ymm9,%ymm9
	vpaddq	%ymm0,%ymm7,%ymm7
	vpmuludq	256-128(%rsi),%ymm10,%ymm12
	vpbroadcastq	8(%r13),%ymm10
	vpaddq	%ymm12,%ymm8,%ymm8

	movq	%rax,%rdx
	imulq	-128(%rcx),%rax
	addq	%rax,%r9
	movq	%rdx,%rax
	imulq	8-128(%rcx),%rax
	addq	%rax,%r10
	movq	%rdx,%rax
	imulq	16-128(%rcx),%rax
	addq	%rax,%r11
	shrq	$29,%r9
	imulq	24-128(%rcx),%rdx
	addq	%rdx,%r12
	addq	%r9,%r10

	vpmuludq	32-128(%rcx),%ymm11,%ymm13
	vmovq	%xmm10,%rbx
	vpaddq	%ymm13,%ymm1,%ymm1
	vpmuludq	64-128(%rcx),%ymm11,%ymm0
	vpaddq	%ymm0,%ymm2,%ymm2
	vpmuludq	96-128(%rcx),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm3,%ymm3
	vpmuludq	128-128(%rcx),%ymm11,%ymm13
	vpaddq	%ymm13,%ymm4,%ymm4
	vpmuludq	160-128(%rcx),%ymm11,%ymm0
	vpaddq	%ymm0,%ymm5,%ymm5
	vpmuludq	192-128(%rcx),%ymm11,%ymm12
	vpaddq	%ymm12,%ymm6,%ymm6
	vpmuludq	224-128(%rcx),%ymm11,%ymm13
	vpblendd	$3,%ymm14,%ymm9,%ymm12
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	256-128(%rcx),%ymm11,%ymm0
	vpaddq	%ymm12,%ymm3,%ymm3
	vpaddq	%ymm0,%ymm8,%ymm8

	movq	%rbx,%rax
	imulq	-128(%rsi),%rax
	addq	%rax,%r10
	vmovdqu	-8+32-128(%rsi),%ymm12
	movq	%rbx,%rax
	imulq	8-128(%rsi),%rax
	addq	%rax,%r11
	vmovdqu	-8+64-128(%rsi),%ymm13

	movq	%r10,%rax
	vpblendd	$0xfc,%ymm14,%ymm9,%ymm9
	imull	%r8d,%eax
	vpaddq	%ymm9,%ymm4,%ymm4
	andl	$0x1fffffff,%eax

	imulq	16-128(%rsi),%rbx
	addq	%rbx,%r12
	vpmuludq	%ymm10,%ymm12,%ymm12
	vmovd	%eax,%xmm11
	vmovdqu	-8+96-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm1,%ymm1
	vpmuludq	%ymm10,%ymm13,%ymm13
	vpbroadcastq	%xmm11,%ymm11
	vmovdqu	-8+128-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm2,%ymm2
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-8+160-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm3,%ymm3
	vpmuludq	%ymm10,%ymm12,%ymm12
	vmovdqu	-8+192-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm4,%ymm4
	vpmuludq	%ymm10,%ymm13,%ymm13
	vmovdqu	-8+224-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-8+256-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm6,%ymm6
	vpmuludq	%ymm10,%ymm12,%ymm12
	vmovdqu	-8+288-128(%rsi),%ymm9
	vpaddq	%ymm12,%ymm7,%ymm7
	vpmuludq	%ymm10,%ymm13,%ymm13
	vpaddq	%ymm13,%ymm8,%ymm8
	vpmuludq	%ymm10,%ymm9,%ymm9
	vpbroadcastq	16(%r13),%ymm10

	movq	%rax,%rdx
	imulq	-128(%rcx),%rax
	addq	%rax,%r10
	vmovdqu	-8+32-128(%rcx),%ymm0
	movq	%rdx,%rax
	imulq	8-128(%rcx),%rax
	addq	%rax,%r11
	vmovdqu	-8+64-128(%rcx),%ymm12
	shrq	$29,%r10
	imulq	16-128(%rcx),%rdx
	addq	%rdx,%r12
	addq	%r10,%r11

	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovq	%xmm10,%rbx
	vmovdqu	-8+96-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm1,%ymm1
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-8+128-128(%rcx),%ymm0
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-8+160-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-8+192-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm4,%ymm4
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-8+224-128(%rcx),%ymm0
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-8+256-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm6,%ymm6
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-8+288-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm7,%ymm7
	vpmuludq	%ymm11,%ymm12,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	%ymm11,%ymm13,%ymm13
	vpaddq	%ymm13,%ymm9,%ymm9

	vmovdqu	-16+32-128(%rsi),%ymm0
	movq	%rbx,%rax
	imulq	-128(%rsi),%rax
	addq	%r11,%rax

	vmovdqu	-16+64-128(%rsi),%ymm12
	movq	%rax,%r11
	imull	%r8d,%eax
	andl	$0x1fffffff,%eax

	imulq	8-128(%rsi),%rbx
	addq	%rbx,%r12
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovd	%eax,%xmm11
	vmovdqu	-16+96-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm1,%ymm1
	vpmuludq	%ymm10,%ymm12,%ymm12
	vpbroadcastq	%xmm11,%ymm11
	vmovdqu	-16+128-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	%ymm10,%ymm13,%ymm13
	vmovdqu	-16+160-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-16+192-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm4,%ymm4
	vpmuludq	%ymm10,%ymm12,%ymm12
	vmovdqu	-16+224-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	%ymm10,%ymm13,%ymm13
	vmovdqu	-16+256-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm6,%ymm6
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-16+288-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm7,%ymm7
	vpmuludq	%ymm10,%ymm12,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	%ymm10,%ymm13,%ymm13
	vpbroadcastq	24(%r13),%ymm10
	vpaddq	%ymm13,%ymm9,%ymm9

	vmovdqu	-16+32-128(%rcx),%ymm0
	movq	%rax,%rdx
	imulq	-128(%rcx),%rax
	addq	%rax,%r11
	vmovdqu	-16+64-128(%rcx),%ymm12
	imulq	8-128(%rcx),%rdx
	addq	%rdx,%r12
	shrq	$29,%r11

	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovq	%xmm10,%rbx
	vmovdqu	-16+96-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm1,%ymm1
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-16+128-128(%rcx),%ymm0
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-16+160-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-16+192-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm4,%ymm4
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-16+224-128(%rcx),%ymm0
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-16+256-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm6,%ymm6
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-16+288-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm7,%ymm7
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-24+32-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-24+64-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm9,%ymm9

	addq	%r11,%r12
	imulq	-128(%rsi),%rbx
	addq	%rbx,%r12

	movq	%r12,%rax
	imull	%r8d,%eax
	andl	$0x1fffffff,%eax

	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovd	%eax,%xmm11
	vmovdqu	-24+96-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm1,%ymm1
	vpmuludq	%ymm10,%ymm12,%ymm12
	vpbroadcastq	%xmm11,%ymm11
	vmovdqu	-24+128-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm2,%ymm2
	vpmuludq	%ymm10,%ymm13,%ymm13
	vmovdqu	-24+160-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-24+192-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm4,%ymm4
	vpmuludq	%ymm10,%ymm12,%ymm12
	vmovdqu	-24+224-128(%rsi),%ymm0
	vpaddq	%ymm12,%ymm5,%ymm5
	vpmuludq	%ymm10,%ymm13,%ymm13
	vmovdqu	-24+256-128(%rsi),%ymm12
	vpaddq	%ymm13,%ymm6,%ymm6
	vpmuludq	%ymm10,%ymm0,%ymm0
	vmovdqu	-24+288-128(%rsi),%ymm13
	vpaddq	%ymm0,%ymm7,%ymm7
	vpmuludq	%ymm10,%ymm12,%ymm12
	vpaddq	%ymm12,%ymm8,%ymm8
	vpmuludq	%ymm10,%ymm13,%ymm13
	vpbroadcastq	32(%r13),%ymm10
	vpaddq	%ymm13,%ymm9,%ymm9
	addq	$32,%r13

	vmovdqu	-24+32-128(%rcx),%ymm0
	imulq	-128(%rcx),%rax
	addq	%rax,%r12
	shrq	$29,%r12

	vmovdqu	-24+64-128(%rcx),%ymm12
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovq	%xmm10,%rbx
	vmovdqu	-24+96-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm1,%ymm0
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	%ymm0,(%rsp)
	vpaddq	%ymm12,%ymm2,%ymm1
	vmovdqu	-24+128-128(%rcx),%ymm0
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-24+160-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm3,%ymm2
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-24+192-128(%rcx),%ymm13
	vpaddq	%ymm0,%ymm4,%ymm3
	vpmuludq	%ymm11,%ymm12,%ymm12
	vmovdqu	-24+224-128(%rcx),%ymm0
	vpaddq	%ymm12,%ymm5,%ymm4
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovdqu	-24+256-128(%rcx),%ymm12
	vpaddq	%ymm13,%ymm6,%ymm5
	vpmuludq	%ymm11,%ymm0,%ymm0
	vmovdqu	-24+288-128(%rcx),%ymm13
	movq	%r12,%r9
	vpaddq	%ymm0,%ymm7,%ymm6
	vpmuludq	%ymm11,%ymm12,%ymm12
	addq	(%rsp),%r9
	vpaddq	%ymm12,%ymm8,%ymm7
	vpmuludq	%ymm11,%ymm13,%ymm13
	vmovq	%r12,%xmm12
	vpaddq	%ymm13,%ymm9,%ymm8

	decl	%r14d
	jnz	.Loop_mul_1024
	vpaddq	(%rsp),%ymm12,%ymm0

	vpsrlq	$29,%ymm0,%ymm12
	vpand	%ymm15,%ymm0,%ymm0
	vpsrlq	$29,%ymm1,%ymm13
	vpand	%ymm15,%ymm1,%ymm1
	vpsrlq	$29,%ymm2,%ymm10
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm2,%ymm2
	vpsrlq	$29,%ymm3,%ymm11
	vpermq	$0x93,%ymm13,%ymm13
	vpand	%ymm15,%ymm3,%ymm3

	vpblendd	$3,%ymm14,%ymm12,%ymm9
	vpermq	$0x93,%ymm10,%ymm10
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpermq	$0x93,%ymm11,%ymm11
	vpaddq	%ymm9,%ymm0,%ymm0
	vpblendd	$3,%ymm13,%ymm10,%ymm13
	vpaddq	%ymm12,%ymm1,%ymm1
	vpblendd	$3,%ymm10,%ymm11,%ymm10
	vpaddq	%ymm13,%ymm2,%ymm2
	vpblendd	$3,%ymm11,%ymm14,%ymm11
	vpaddq	%ymm10,%ymm3,%ymm3
	vpaddq	%ymm11,%ymm4,%ymm4

	vpsrlq	$29,%ymm0,%ymm12
	vpand	%ymm15,%ymm0,%ymm0
	vpsrlq	$29,%ymm1,%ymm13
	vpand	%ymm15,%ymm1,%ymm1
	vpsrlq	$29,%ymm2,%ymm10
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm2,%ymm2
	vpsrlq	$29,%ymm3,%ymm11
	vpermq	$0x93,%ymm13,%ymm13
	vpand	%ymm15,%ymm3,%ymm3
	vpermq	$0x93,%ymm10,%ymm10

	vpblendd	$3,%ymm14,%ymm12,%ymm9
	vpermq	$0x93,%ymm11,%ymm11
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm9,%ymm0,%ymm0
	vpblendd	$3,%ymm13,%ymm10,%ymm13
	vpaddq	%ymm12,%ymm1,%ymm1
	vpblendd	$3,%ymm10,%ymm11,%ymm10
	vpaddq	%ymm13,%ymm2,%ymm2
	vpblendd	$3,%ymm11,%ymm14,%ymm11
	vpaddq	%ymm10,%ymm3,%ymm3
	vpaddq	%ymm11,%ymm4,%ymm4

	vmovdqu	%ymm0,0-128(%rdi)
	vmovdqu	%ymm1,32-128(%rdi)
	vmovdqu	%ymm2,64-128(%rdi)
	vmovdqu	%ymm3,96-128(%rdi)
	vpsrlq	$29,%ymm4,%ymm12
	vpand	%ymm15,%ymm4,%ymm4
	vpsrlq	$29,%ymm5,%ymm13
	vpand	%ymm15,%ymm5,%ymm5
	vpsrlq	$29,%ymm6,%ymm10
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm6,%ymm6
	vpsrlq	$29,%ymm7,%ymm11
	vpermq	$0x93,%ymm13,%ymm13
	vpand	%ymm15,%ymm7,%ymm7
	vpsrlq	$29,%ymm8,%ymm0
	vpermq	$0x93,%ymm10,%ymm10
	vpand	%ymm15,%ymm8,%ymm8
	vpermq	$0x93,%ymm11,%ymm11

	vpblendd	$3,%ymm14,%ymm12,%ymm9
	vpermq	$0x93,%ymm0,%ymm0
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm9,%ymm4,%ymm4
	vpblendd	$3,%ymm13,%ymm10,%ymm13
	vpaddq	%ymm12,%ymm5,%ymm5
	vpblendd	$3,%ymm10,%ymm11,%ymm10
	vpaddq	%ymm13,%ymm6,%ymm6
	vpblendd	$3,%ymm11,%ymm0,%ymm11
	vpaddq	%ymm10,%ymm7,%ymm7
	vpaddq	%ymm11,%ymm8,%ymm8

	vpsrlq	$29,%ymm4,%ymm12
	vpand	%ymm15,%ymm4,%ymm4
	vpsrlq	$29,%ymm5,%ymm13
	vpand	%ymm15,%ymm5,%ymm5
	vpsrlq	$29,%ymm6,%ymm10
	vpermq	$0x93,%ymm12,%ymm12
	vpand	%ymm15,%ymm6,%ymm6
	vpsrlq	$29,%ymm7,%ymm11
	vpermq	$0x93,%ymm13,%ymm13
	vpand	%ymm15,%ymm7,%ymm7
	vpsrlq	$29,%ymm8,%ymm0
	vpermq	$0x93,%ymm10,%ymm10
	vpand	%ymm15,%ymm8,%ymm8
	vpermq	$0x93,%ymm11,%ymm11

	vpblendd	$3,%ymm14,%ymm12,%ymm9
	vpermq	$0x93,%ymm0,%ymm0
	vpblendd	$3,%ymm12,%ymm13,%ymm12
	vpaddq	%ymm9,%ymm4,%ymm4
	vpblendd	$3,%ymm13,%ymm10,%ymm13
	vpaddq	%ymm12,%ymm5,%ymm5
	vpblendd	$3,%ymm10,%ymm11,%ymm10
	vpaddq	%ymm13,%ymm6,%ymm6
	vpblendd	$3,%ymm11,%ymm0,%ymm11
	vpaddq	%ymm10,%ymm7,%ymm7
	vpaddq	%ymm11,%ymm8,%ymm8

	vmovdqu	%ymm4,128-128(%rdi)
	vmovdqu	%ymm5,160-128(%rdi)
	vmovdqu	%ymm6,192-128(%rdi)
	vmovdqu	%ymm7,224-128(%rdi)
	vmovdqu	%ymm8,256-128(%rdi)
	vzeroupper

	movq	%rbp,%rax
.cfi_def_cfa_register	%rax
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lmul_1024_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_1024_mul_avx2,.-rsaz_1024_mul_avx2
.globl	rsaz_1024_red2norm_avx2
.type	rsaz_1024_red2norm_avx2,@function
.align	32
rsaz_1024_red2norm_avx2:
.cfi_startproc	
	subq	$-128,%rsi
	xorq	%rax,%rax
	movq	-128(%rsi),%r8
	movq	-120(%rsi),%r9
	movq	-112(%rsi),%r10
	shlq	$0,%r8
	shlq	$29,%r9
	movq	%r10,%r11
	shlq	$58,%r10
	shrq	$6,%r11
	addq	%r8,%rax
	addq	%r9,%rax
	addq	%r10,%rax
	adcq	$0,%r11
	movq	%rax,0(%rdi)
	movq	%r11,%rax
	movq	-104(%rsi),%r8
	movq	-96(%rsi),%r9
	shlq	$23,%r8
	movq	%r9,%r10
	shlq	$52,%r9
	shrq	$12,%r10
	addq	%r8,%rax
	addq	%r9,%rax
	adcq	$0,%r10
	movq	%rax,8(%rdi)
	movq	%r10,%rax
	movq	-88(%rsi),%r11
	movq	-80(%rsi),%r8
	shlq	$17,%r11
	movq	%r8,%r9
	shlq	$46,%r8
	shrq	$18,%r9
	addq	%r11,%rax
	addq	%r8,%rax
	adcq	$0,%r9
	movq	%rax,16(%rdi)
	movq	%r9,%rax
	movq	-72(%rsi),%r10
	movq	-64(%rsi),%r11
	shlq	$11,%r10
	movq	%r11,%r8
	shlq	$40,%r11
	shrq	$24,%r8
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,24(%rdi)
	movq	%r8,%rax
	movq	-56(%rsi),%r9
	movq	-48(%rsi),%r10
	movq	-40(%rsi),%r11
	shlq	$5,%r9
	shlq	$34,%r10
	movq	%r11,%r8
	shlq	$63,%r11
	shrq	$1,%r8
	addq	%r9,%rax
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,32(%rdi)
	movq	%r8,%rax
	movq	-32(%rsi),%r9
	movq	-24(%rsi),%r10
	shlq	$28,%r9
	movq	%r10,%r11
	shlq	$57,%r10
	shrq	$7,%r11
	addq	%r9,%rax
	addq	%r10,%rax
	adcq	$0,%r11
	movq	%rax,40(%rdi)
	movq	%r11,%rax
	movq	-16(%rsi),%r8
	movq	-8(%rsi),%r9
	shlq	$22,%r8
	movq	%r9,%r10
	shlq	$51,%r9
	shrq	$13,%r10
	addq	%r8,%rax
	addq	%r9,%rax
	adcq	$0,%r10
	movq	%rax,48(%rdi)
	movq	%r10,%rax
	movq	0(%rsi),%r11
	movq	8(%rsi),%r8
	shlq	$16,%r11
	movq	%r8,%r9
	shlq	$45,%r8
	shrq	$19,%r9
	addq	%r11,%rax
	addq	%r8,%rax
	adcq	$0,%r9
	movq	%rax,56(%rdi)
	movq	%r9,%rax
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	shlq	$10,%r10
	movq	%r11,%r8
	shlq	$39,%r11
	shrq	$25,%r8
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,64(%rdi)
	movq	%r8,%rax
	movq	32(%rsi),%r9
	movq	40(%rsi),%r10
	movq	48(%rsi),%r11
	shlq	$4,%r9
	shlq	$33,%r10
	movq	%r11,%r8
	shlq	$62,%r11
	shrq	$2,%r8
	addq	%r9,%rax
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,72(%rdi)
	movq	%r8,%rax
	movq	56(%rsi),%r9
	movq	64(%rsi),%r10
	shlq	$27,%r9
	movq	%r10,%r11
	shlq	$56,%r10
	shrq	$8,%r11
	addq	%r9,%rax
	addq	%r10,%rax
	adcq	$0,%r11
	movq	%rax,80(%rdi)
	movq	%r11,%rax
	movq	72(%rsi),%r8
	movq	80(%rsi),%r9
	shlq	$21,%r8
	movq	%r9,%r10
	shlq	$50,%r9
	shrq	$14,%r10
	addq	%r8,%rax
	addq	%r9,%rax
	adcq	$0,%r10
	movq	%rax,88(%rdi)
	movq	%r10,%rax
	movq	88(%rsi),%r11
	movq	96(%rsi),%r8
	shlq	$15,%r11
	movq	%r8,%r9
	shlq	$44,%r8
	shrq	$20,%r9
	addq	%r11,%rax
	addq	%r8,%rax
	adcq	$0,%r9
	movq	%rax,96(%rdi)
	movq	%r9,%rax
	movq	104(%rsi),%r10
	movq	112(%rsi),%r11
	shlq	$9,%r10
	movq	%r11,%r8
	shlq	$38,%r11
	shrq	$26,%r8
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,104(%rdi)
	movq	%r8,%rax
	movq	120(%rsi),%r9
	movq	128(%rsi),%r10
	movq	136(%rsi),%r11
	shlq	$3,%r9
	shlq	$32,%r10
	movq	%r11,%r8
	shlq	$61,%r11
	shrq	$3,%r8
	addq	%r9,%rax
	addq	%r10,%rax
	addq	%r11,%rax
	adcq	$0,%r8
	movq	%rax,112(%rdi)
	movq	%r8,%rax
	movq	144(%rsi),%r9
	movq	152(%rsi),%r10
	shlq	$26,%r9
	movq	%r10,%r11
	shlq	$55,%r10
	shrq	$9,%r11
	addq	%r9,%rax
	addq	%r10,%rax
	adcq	$0,%r11
	movq	%rax,120(%rdi)
	movq	%r11,%rax
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_1024_red2norm_avx2,.-rsaz_1024_red2norm_avx2

.globl	rsaz_1024_norm2red_avx2
.type	rsaz_1024_norm2red_avx2,@function
.align	32
rsaz_1024_norm2red_avx2:
.cfi_startproc	
	subq	$-128,%rdi
	movq	(%rsi),%r8
	movl	$0x1fffffff,%eax
	movq	8(%rsi),%r9
	movq	%r8,%r11
	shrq	$0,%r11
	andq	%rax,%r11
	movq	%r11,-128(%rdi)
	movq	%r8,%r10
	shrq	$29,%r10
	andq	%rax,%r10
	movq	%r10,-120(%rdi)
	shrdq	$58,%r9,%r8
	andq	%rax,%r8
	movq	%r8,-112(%rdi)
	movq	16(%rsi),%r10
	movq	%r9,%r8
	shrq	$23,%r8
	andq	%rax,%r8
	movq	%r8,-104(%rdi)
	shrdq	$52,%r10,%r9
	andq	%rax,%r9
	movq	%r9,-96(%rdi)
	movq	24(%rsi),%r11
	movq	%r10,%r9
	shrq	$17,%r9
	andq	%rax,%r9
	movq	%r9,-88(%rdi)
	shrdq	$46,%r11,%r10
	andq	%rax,%r10
	movq	%r10,-80(%rdi)
	movq	32(%rsi),%r8
	movq	%r11,%r10
	shrq	$11,%r10
	andq	%rax,%r10
	movq	%r10,-72(%rdi)
	shrdq	$40,%r8,%r11
	andq	%rax,%r11
	movq	%r11,-64(%rdi)
	movq	40(%rsi),%r9
	movq	%r8,%r11
	shrq	$5,%r11
	andq	%rax,%r11
	movq	%r11,-56(%rdi)
	movq	%r8,%r10
	shrq	$34,%r10
	andq	%rax,%r10
	movq	%r10,-48(%rdi)
	shrdq	$63,%r9,%r8
	andq	%rax,%r8
	movq	%r8,-40(%rdi)
	movq	48(%rsi),%r10
	movq	%r9,%r8
	shrq	$28,%r8
	andq	%rax,%r8
	movq	%r8,-32(%rdi)
	shrdq	$57,%r10,%r9
	andq	%rax,%r9
	movq	%r9,-24(%rdi)
	movq	56(%rsi),%r11
	movq	%r10,%r9
	shrq	$22,%r9
	andq	%rax,%r9
	movq	%r9,-16(%rdi)
	shrdq	$51,%r11,%r10
	andq	%rax,%r10
	movq	%r10,-8(%rdi)
	movq	64(%rsi),%r8
	movq	%r11,%r10
	shrq	$16,%r10
	andq	%rax,%r10
	movq	%r10,0(%rdi)
	shrdq	$45,%r8,%r11
	andq	%rax,%r11
	movq	%r11,8(%rdi)
	movq	72(%rsi),%r9
	movq	%r8,%r11
	shrq	$10,%r11
	andq	%rax,%r11
	movq	%r11,16(%rdi)
	shrdq	$39,%r9,%r8
	andq	%rax,%r8
	movq	%r8,24(%rdi)
	movq	80(%rsi),%r10
	movq	%r9,%r8
	shrq	$4,%r8
	andq	%rax,%r8
	movq	%r8,32(%rdi)
	movq	%r9,%r11
	shrq	$33,%r11
	andq	%rax,%r11
	movq	%r11,40(%rdi)
	shrdq	$62,%r10,%r9
	andq	%rax,%r9
	movq	%r9,48(%rdi)
	movq	88(%rsi),%r11
	movq	%r10,%r9
	shrq	$27,%r9
	andq	%rax,%r9
	movq	%r9,56(%rdi)
	shrdq	$56,%r11,%r10
	andq	%rax,%r10
	movq	%r10,64(%rdi)
	movq	96(%rsi),%r8
	movq	%r11,%r10
	shrq	$21,%r10
	andq	%rax,%r10
	movq	%r10,72(%rdi)
	shrdq	$50,%r8,%r11
	andq	%rax,%r11
	movq	%r11,80(%rdi)
	movq	104(%rsi),%r9
	movq	%r8,%r11
	shrq	$15,%r11
	andq	%rax,%r11
	movq	%r11,88(%rdi)
	shrdq	$44,%r9,%r8
	andq	%rax,%r8
	movq	%r8,96(%rdi)
	movq	112(%rsi),%r10
	movq	%r9,%r8
	shrq	$9,%r8
	andq	%rax,%r8
	movq	%r8,104(%rdi)
	shrdq	$38,%r10,%r9
	andq	%rax,%r9
	movq	%r9,112(%rdi)
	movq	120(%rsi),%r11
	movq	%r10,%r9
	shrq	$3,%r9
	andq	%rax,%r9
	movq	%r9,120(%rdi)
	movq	%r10,%r8
	shrq	$32,%r8
	andq	%rax,%r8
	movq	%r8,128(%rdi)
	shrdq	$61,%r11,%r10
	andq	%rax,%r10
	movq	%r10,136(%rdi)
	xorq	%r8,%r8
	movq	%r11,%r10
	shrq	$26,%r10
	andq	%rax,%r10
	movq	%r10,144(%rdi)
	shrdq	$55,%r8,%r11
	andq	%rax,%r11
	movq	%r11,152(%rdi)
	movq	%r8,160(%rdi)
	movq	%r8,168(%rdi)
	movq	%r8,176(%rdi)
	movq	%r8,184(%rdi)
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_1024_norm2red_avx2,.-rsaz_1024_norm2red_avx2
.globl	rsaz_1024_scatter5_avx2
.type	rsaz_1024_scatter5_avx2,@function
.align	32
rsaz_1024_scatter5_avx2:
.cfi_startproc	
	vzeroupper
	vmovdqu	.Lscatter_permd(%rip),%ymm5
	shll	$4,%edx
	leaq	(%rdi,%rdx,1),%rdi
	movl	$9,%eax
	jmp	.Loop_scatter_1024

.align	32
.Loop_scatter_1024:
	vmovdqu	(%rsi),%ymm0
	leaq	32(%rsi),%rsi
	vpermd	%ymm0,%ymm5,%ymm0
	vmovdqu	%xmm0,(%rdi)
	leaq	512(%rdi),%rdi
	decl	%eax
	jnz	.Loop_scatter_1024

	vzeroupper
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_1024_scatter5_avx2,.-rsaz_1024_scatter5_avx2

.globl	rsaz_1024_gather5_avx2
.type	rsaz_1024_gather5_avx2,@function
.align	32
rsaz_1024_gather5_avx2:
.cfi_startproc	
	vzeroupper
	movq	%rsp,%r11
.cfi_def_cfa_register	%r11
	leaq	-256(%rsp),%rsp
	andq	$-32,%rsp
	leaq	.Linc(%rip),%r10
	leaq	-128(%rsp),%rax

	vmovd	%edx,%xmm4
	vmovdqa	(%r10),%ymm0
	vmovdqa	32(%r10),%ymm1
	vmovdqa	64(%r10),%ymm5
	vpbroadcastd	%xmm4,%ymm4

	vpaddd	%ymm5,%ymm0,%ymm2
	vpcmpeqd	%ymm4,%ymm0,%ymm0
	vpaddd	%ymm5,%ymm1,%ymm3
	vpcmpeqd	%ymm4,%ymm1,%ymm1
	vmovdqa	%ymm0,0+128(%rax)
	vpaddd	%ymm5,%ymm2,%ymm0
	vpcmpeqd	%ymm4,%ymm2,%ymm2
	vmovdqa	%ymm1,32+128(%rax)
	vpaddd	%ymm5,%ymm3,%ymm1
	vpcmpeqd	%ymm4,%ymm3,%ymm3
	vmovdqa	%ymm2,64+128(%rax)
	vpaddd	%ymm5,%ymm0,%ymm2
	vpcmpeqd	%ymm4,%ymm0,%ymm0
	vmovdqa	%ymm3,96+128(%rax)
	vpaddd	%ymm5,%ymm1,%ymm3
	vpcmpeqd	%ymm4,%ymm1,%ymm1
	vmovdqa	%ymm0,128+128(%rax)
	vpaddd	%ymm5,%ymm2,%ymm8
	vpcmpeqd	%ymm4,%ymm2,%ymm2
	vmovdqa	%ymm1,160+128(%rax)
	vpaddd	%ymm5,%ymm3,%ymm9
	vpcmpeqd	%ymm4,%ymm3,%ymm3
	vmovdqa	%ymm2,192+128(%rax)
	vpaddd	%ymm5,%ymm8,%ymm10
	vpcmpeqd	%ymm4,%ymm8,%ymm8
	vmovdqa	%ymm3,224+128(%rax)
	vpaddd	%ymm5,%ymm9,%ymm11
	vpcmpeqd	%ymm4,%ymm9,%ymm9
	vpaddd	%ymm5,%ymm10,%ymm12
	vpcmpeqd	%ymm4,%ymm10,%ymm10
	vpaddd	%ymm5,%ymm11,%ymm13
	vpcmpeqd	%ymm4,%ymm11,%ymm11
	vpaddd	%ymm5,%ymm12,%ymm14
	vpcmpeqd	%ymm4,%ymm12,%ymm12
	vpaddd	%ymm5,%ymm13,%ymm15
	vpcmpeqd	%ymm4,%ymm13,%ymm13
	vpcmpeqd	%ymm4,%ymm14,%ymm14
	vpcmpeqd	%ymm4,%ymm15,%ymm15

	vmovdqa	-32(%r10),%ymm7
	leaq	128(%rsi),%rsi
	movl	$9,%edx

.Loop_gather_1024:
	vmovdqa	0-128(%rsi),%ymm0
	vmovdqa	32-128(%rsi),%ymm1
	vmovdqa	64-128(%rsi),%ymm2
	vmovdqa	96-128(%rsi),%ymm3
	vpand	0+128(%rax),%ymm0,%ymm0
	vpand	32+128(%rax),%ymm1,%ymm1
	vpand	64+128(%rax),%ymm2,%ymm2
	vpor	%ymm0,%ymm1,%ymm4
	vpand	96+128(%rax),%ymm3,%ymm3
	vmovdqa	128-128(%rsi),%ymm0
	vmovdqa	160-128(%rsi),%ymm1
	vpor	%ymm2,%ymm3,%ymm5
	vmovdqa	192-128(%rsi),%ymm2
	vmovdqa	224-128(%rsi),%ymm3
	vpand	128+128(%rax),%ymm0,%ymm0
	vpand	160+128(%rax),%ymm1,%ymm1
	vpand	192+128(%rax),%ymm2,%ymm2
	vpor	%ymm0,%ymm4,%ymm4
	vpand	224+128(%rax),%ymm3,%ymm3
	vpand	256-128(%rsi),%ymm8,%ymm0
	vpor	%ymm1,%ymm5,%ymm5
	vpand	288-128(%rsi),%ymm9,%ymm1
	vpor	%ymm2,%ymm4,%ymm4
	vpand	320-128(%rsi),%ymm10,%ymm2
	vpor	%ymm3,%ymm5,%ymm5
	vpand	352-128(%rsi),%ymm11,%ymm3
	vpor	%ymm0,%ymm4,%ymm4
	vpand	384-128(%rsi),%ymm12,%ymm0
	vpor	%ymm1,%ymm5,%ymm5
	vpand	416-128(%rsi),%ymm13,%ymm1
	vpor	%ymm2,%ymm4,%ymm4
	vpand	448-128(%rsi),%ymm14,%ymm2
	vpor	%ymm3,%ymm5,%ymm5
	vpand	480-128(%rsi),%ymm15,%ymm3
	leaq	512(%rsi),%rsi
	vpor	%ymm0,%ymm4,%ymm4
	vpor	%ymm1,%ymm5,%ymm5
	vpor	%ymm2,%ymm4,%ymm4
	vpor	%ymm3,%ymm5,%ymm5

	vpor	%ymm5,%ymm4,%ymm4
	vextracti128	$1,%ymm4,%xmm5
	vpor	%xmm4,%xmm5,%xmm5
	vpermd	%ymm5,%ymm7,%ymm5
	vmovdqu	%ymm5,(%rdi)
	leaq	32(%rdi),%rdi
	decl	%edx
	jnz	.Loop_gather_1024

	vpxor	%ymm0,%ymm0,%ymm0
	vmovdqu	%ymm0,(%rdi)
	vzeroupper
	leaq	(%r11),%rsp
.cfi_def_cfa_register	%rsp
	.byte	0xf3,0xc3
.cfi_endproc	
.LSEH_end_rsaz_1024_gather5:
.size	rsaz_1024_gather5_avx2,.-rsaz_1024_gather5_avx2

.globl	rsaz_avx2_eligible
.type	rsaz_avx2_eligible,@function
.align	32
rsaz_avx2_eligible:
	movl	OPENSSL_ia32cap_P+8(%rip),%eax
	movl	$524544,%ecx
	movl	$0,%edx
	andl	%eax,%ecx
	cmpl	$524544,%ecx
	cmovel	%edx,%eax
	andl	$32,%eax
	shrl	$5,%eax
	.byte	0xf3,0xc3
.size	rsaz_avx2_eligible,.-rsaz_avx2_eligible

.align	64
.Land_mask:
.quad	0x1fffffff,0x1fffffff,0x1fffffff,0x1fffffff
.Lscatter_permd:
.long	0,2,4,6,7,7,7,7
.Lgather_permd:
.long	0,7,1,7,2,7,3,7
.Linc:
.long	0,0,0,0, 1,1,1,1
.long	2,2,2,2, 3,3,3,3
.long	4,4,4,4, 4,4,4,4
.align	64
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
