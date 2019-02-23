.text	
.globl	_padlock_capability

.p2align	4
_padlock_capability:
	movq	%rbx,%r8
	xorl	%eax,%eax
	cpuid
	xorl	%eax,%eax
	cmpl	$0x746e6543,%ebx
	jne	L$zhaoxin
	cmpl	$0x48727561,%edx
	jne	L$noluck
	cmpl	$0x736c7561,%ecx
	jne	L$noluck
	jmp	L$zhaoxinEnd
L$zhaoxin:
	cmpl	$0x68532020,%ebx
	jne	L$noluck
	cmpl	$0x68676e61,%edx
	jne	L$noluck
	cmpl	$0x20206961,%ecx
	jne	L$noluck
L$zhaoxinEnd:
	movl	$0xC0000000,%eax
	cpuid
	movl	%eax,%edx
	xorl	%eax,%eax
	cmpl	$0xC0000001,%edx
	jb	L$noluck
	movl	$0xC0000001,%eax
	cpuid
	movl	%edx,%eax
	andl	$0xffffffef,%eax
	orl	$0x10,%eax
L$noluck:
	movq	%r8,%rbx
	.byte	0xf3,0xc3


.globl	_padlock_key_bswap

.p2align	4
_padlock_key_bswap:
	movl	240(%rdi),%edx
L$bswap_loop:
	movl	(%rdi),%eax
	bswapl	%eax
	movl	%eax,(%rdi)
	leaq	4(%rdi),%rdi
	subl	$1,%edx
	jnz	L$bswap_loop
	.byte	0xf3,0xc3


.globl	_padlock_verify_context

.p2align	4
_padlock_verify_context:
	movq	%rdi,%rdx
	pushf
	leaq	L$padlock_saved_context(%rip),%rax
	call	_padlock_verify_ctx
	leaq	8(%rsp),%rsp
	.byte	0xf3,0xc3



.p2align	4
_padlock_verify_ctx:
	movq	8(%rsp),%r8
	btq	$30,%r8
	jnc	L$verified
	cmpq	(%rax),%rdx
	je	L$verified
	pushf
	popf
L$verified:
	movq	%rdx,(%rax)
	.byte	0xf3,0xc3


.globl	_padlock_reload_key

.p2align	4
_padlock_reload_key:
	pushf
	popf
	.byte	0xf3,0xc3


.globl	_padlock_aes_block

.p2align	4
_padlock_aes_block:
	movq	%rbx,%r8
	movq	$1,%rcx
	leaq	32(%rdx),%rbx
	leaq	16(%rdx),%rdx
.byte	0xf3,0x0f,0xa7,0xc8
	movq	%r8,%rbx
	.byte	0xf3,0xc3


.globl	_padlock_xstore

.p2align	4
_padlock_xstore:
	movl	%esi,%edx
.byte	0x0f,0xa7,0xc0
	.byte	0xf3,0xc3


.globl	_padlock_sha1_oneshot

.p2align	4
_padlock_sha1_oneshot:
	movq	%rdx,%rcx
	movq	%rdi,%rdx
	movups	(%rdi),%xmm0
	subq	$128+8,%rsp
	movl	16(%rdi),%eax
	movaps	%xmm0,(%rsp)
	movq	%rsp,%rdi
	movl	%eax,16(%rsp)
	xorq	%rax,%rax
.byte	0xf3,0x0f,0xa6,0xc8
	movaps	(%rsp),%xmm0
	movl	16(%rsp),%eax
	addq	$128+8,%rsp
	movups	%xmm0,(%rdx)
	movl	%eax,16(%rdx)
	.byte	0xf3,0xc3


.globl	_padlock_sha1_blocks

.p2align	4
_padlock_sha1_blocks:
	movq	%rdx,%rcx
	movq	%rdi,%rdx
	movups	(%rdi),%xmm0
	subq	$128+8,%rsp
	movl	16(%rdi),%eax
	movaps	%xmm0,(%rsp)
	movq	%rsp,%rdi
	movl	%eax,16(%rsp)
	movq	$-1,%rax
.byte	0xf3,0x0f,0xa6,0xc8
	movaps	(%rsp),%xmm0
	movl	16(%rsp),%eax
	addq	$128+8,%rsp
	movups	%xmm0,(%rdx)
	movl	%eax,16(%rdx)
	.byte	0xf3,0xc3


.globl	_padlock_sha256_oneshot

.p2align	4
_padlock_sha256_oneshot:
	movq	%rdx,%rcx
	movq	%rdi,%rdx
	movups	(%rdi),%xmm0
	subq	$128+8,%rsp
	movups	16(%rdi),%xmm1
	movaps	%xmm0,(%rsp)
	movq	%rsp,%rdi
	movaps	%xmm1,16(%rsp)
	xorq	%rax,%rax
.byte	0xf3,0x0f,0xa6,0xd0
	movaps	(%rsp),%xmm0
	movaps	16(%rsp),%xmm1
	addq	$128+8,%rsp
	movups	%xmm0,(%rdx)
	movups	%xmm1,16(%rdx)
	.byte	0xf3,0xc3


.globl	_padlock_sha256_blocks

.p2align	4
_padlock_sha256_blocks:
	movq	%rdx,%rcx
	movq	%rdi,%rdx
	movups	(%rdi),%xmm0
	subq	$128+8,%rsp
	movups	16(%rdi),%xmm1
	movaps	%xmm0,(%rsp)
	movq	%rsp,%rdi
	movaps	%xmm1,16(%rsp)
	movq	$-1,%rax
.byte	0xf3,0x0f,0xa6,0xd0
	movaps	(%rsp),%xmm0
	movaps	16(%rsp),%xmm1
	addq	$128+8,%rsp
	movups	%xmm0,(%rdx)
	movups	%xmm1,16(%rdx)
	.byte	0xf3,0xc3


.globl	_padlock_sha512_blocks

.p2align	4
_padlock_sha512_blocks:
	movq	%rdx,%rcx
	movq	%rdi,%rdx
	movups	(%rdi),%xmm0
	subq	$128+8,%rsp
	movups	16(%rdi),%xmm1
	movups	32(%rdi),%xmm2
	movups	48(%rdi),%xmm3
	movaps	%xmm0,(%rsp)
	movq	%rsp,%rdi
	movaps	%xmm1,16(%rsp)
	movaps	%xmm2,32(%rsp)
	movaps	%xmm3,48(%rsp)
.byte	0xf3,0x0f,0xa6,0xe0
	movaps	(%rsp),%xmm0
	movaps	16(%rsp),%xmm1
	movaps	32(%rsp),%xmm2
	movaps	48(%rsp),%xmm3
	addq	$128+8,%rsp
	movups	%xmm0,(%rdx)
	movups	%xmm1,16(%rdx)
	movups	%xmm2,32(%rdx)
	movups	%xmm3,48(%rdx)
	.byte	0xf3,0xc3

.globl	_padlock_ecb_encrypt

.p2align	4
_padlock_ecb_encrypt:
	pushq	%rbp
	pushq	%rbx

	xorl	%eax,%eax
	testq	$15,%rdx
	jnz	L$ecb_abort
	testq	$15,%rcx
	jnz	L$ecb_abort
	leaq	L$padlock_saved_context(%rip),%rax
	pushf
	cld
	call	_padlock_verify_ctx
	leaq	16(%rdx),%rdx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	testl	$32,(%rdx)
	jnz	L$ecb_aligned
	testq	$0x0f,%rdi
	setz	%al
	testq	$0x0f,%rsi
	setz	%bl
	testl	%ebx,%eax
	jnz	L$ecb_aligned
	negq	%rax
	movq	$512,%rbx
	notq	%rax
	leaq	(%rsp),%rbp
	cmpq	%rbx,%rcx
	cmovcq	%rcx,%rbx
	andq	%rbx,%rax
	movq	%rcx,%rbx
	negq	%rax
	andq	$512-1,%rbx
	leaq	(%rax,%rbp,1),%rsp
	movq	$512,%rax
	cmovzq	%rax,%rbx
	cmpq	%rbx,%rcx
	ja	L$ecb_loop
	movq	%rsi,%rax
	cmpq	%rsp,%rbp
	cmoveq	%rdi,%rax
	addq	%rcx,%rax
	negq	%rax
	andq	$0xfff,%rax
	cmpq	$128,%rax
	movq	$-128,%rax
	cmovaeq	%rbx,%rax
	andq	%rax,%rbx
	jz	L$ecb_unaligned_tail
	jmp	L$ecb_loop
.p2align	4
L$ecb_loop:
	cmpq	%rcx,%rbx
	cmovaq	%rcx,%rbx
	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11
	testq	$0x0f,%rdi
	cmovnzq	%rsp,%rdi
	testq	$0x0f,%rsi
	jz	L$ecb_inp_aligned
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
	movq	%rbx,%rcx
	movq	%rdi,%rsi
L$ecb_inp_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,200
	movq	%r8,%rdi
	movq	%r11,%rbx
	testq	$0x0f,%rdi
	jz	L$ecb_out_aligned
	movq	%rbx,%rcx
	leaq	(%rsp),%rsi
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
L$ecb_out_aligned:
	movq	%r9,%rsi
	movq	%r10,%rcx
	addq	%rbx,%rdi
	addq	%rbx,%rsi
	subq	%rbx,%rcx
	movq	$512,%rbx
	jz	L$ecb_break
	cmpq	%rbx,%rcx
	jae	L$ecb_loop
L$ecb_unaligned_tail:
	xorl	%eax,%eax
	cmpq	%rsp,%rbp
	cmoveq	%rcx,%rax
	movq	%rdi,%r8
	movq	%rcx,%rbx
	subq	%rax,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	movq	%rsp,%rsi
	movq	%r8,%rdi
	movq	%rbx,%rcx
	jmp	L$ecb_loop
.p2align	4
L$ecb_break:
	cmpq	%rbp,%rsp
	je	L$ecb_done

	pxor	%xmm0,%xmm0
	leaq	(%rsp),%rax
L$ecb_bzero:
	movaps	%xmm0,(%rax)
	leaq	16(%rax),%rax
	cmpq	%rax,%rbp
	ja	L$ecb_bzero

L$ecb_done:
	leaq	(%rbp),%rsp
	jmp	L$ecb_exit

.p2align	4
L$ecb_aligned:
	leaq	(%rsi,%rcx,1),%rbp
	negq	%rbp
	andq	$0xfff,%rbp
	xorl	%eax,%eax
	cmpq	$128,%rbp
	movq	$128-1,%rbp
	cmovaeq	%rax,%rbp
	andq	%rcx,%rbp
	subq	%rbp,%rcx
	jz	L$ecb_aligned_tail
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,200
	testq	%rbp,%rbp
	jz	L$ecb_exit

L$ecb_aligned_tail:
	movq	%rdi,%r8
	movq	%rbp,%rbx
	movq	%rbp,%rcx
	leaq	(%rsp),%rbp
	subq	%rcx,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	leaq	(%r8),%rdi
	leaq	(%rsp),%rsi
	movq	%rbx,%rcx
	jmp	L$ecb_loop
L$ecb_exit:
	movl	$1,%eax
	leaq	8(%rsp),%rsp
L$ecb_abort:
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3

.globl	_padlock_cbc_encrypt

.p2align	4
_padlock_cbc_encrypt:
	pushq	%rbp
	pushq	%rbx

	xorl	%eax,%eax
	testq	$15,%rdx
	jnz	L$cbc_abort
	testq	$15,%rcx
	jnz	L$cbc_abort
	leaq	L$padlock_saved_context(%rip),%rax
	pushf
	cld
	call	_padlock_verify_ctx
	leaq	16(%rdx),%rdx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	testl	$32,(%rdx)
	jnz	L$cbc_aligned
	testq	$0x0f,%rdi
	setz	%al
	testq	$0x0f,%rsi
	setz	%bl
	testl	%ebx,%eax
	jnz	L$cbc_aligned
	negq	%rax
	movq	$512,%rbx
	notq	%rax
	leaq	(%rsp),%rbp
	cmpq	%rbx,%rcx
	cmovcq	%rcx,%rbx
	andq	%rbx,%rax
	movq	%rcx,%rbx
	negq	%rax
	andq	$512-1,%rbx
	leaq	(%rax,%rbp,1),%rsp
	movq	$512,%rax
	cmovzq	%rax,%rbx
	cmpq	%rbx,%rcx
	ja	L$cbc_loop
	movq	%rsi,%rax
	cmpq	%rsp,%rbp
	cmoveq	%rdi,%rax
	addq	%rcx,%rax
	negq	%rax
	andq	$0xfff,%rax
	cmpq	$64,%rax
	movq	$-64,%rax
	cmovaeq	%rbx,%rax
	andq	%rax,%rbx
	jz	L$cbc_unaligned_tail
	jmp	L$cbc_loop
.p2align	4
L$cbc_loop:
	cmpq	%rcx,%rbx
	cmovaq	%rcx,%rbx
	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11
	testq	$0x0f,%rdi
	cmovnzq	%rsp,%rdi
	testq	$0x0f,%rsi
	jz	L$cbc_inp_aligned
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
	movq	%rbx,%rcx
	movq	%rdi,%rsi
L$cbc_inp_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,208
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
	movq	%r8,%rdi
	movq	%r11,%rbx
	testq	$0x0f,%rdi
	jz	L$cbc_out_aligned
	movq	%rbx,%rcx
	leaq	(%rsp),%rsi
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
L$cbc_out_aligned:
	movq	%r9,%rsi
	movq	%r10,%rcx
	addq	%rbx,%rdi
	addq	%rbx,%rsi
	subq	%rbx,%rcx
	movq	$512,%rbx
	jz	L$cbc_break
	cmpq	%rbx,%rcx
	jae	L$cbc_loop
L$cbc_unaligned_tail:
	xorl	%eax,%eax
	cmpq	%rsp,%rbp
	cmoveq	%rcx,%rax
	movq	%rdi,%r8
	movq	%rcx,%rbx
	subq	%rax,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	movq	%rsp,%rsi
	movq	%r8,%rdi
	movq	%rbx,%rcx
	jmp	L$cbc_loop
.p2align	4
L$cbc_break:
	cmpq	%rbp,%rsp
	je	L$cbc_done

	pxor	%xmm0,%xmm0
	leaq	(%rsp),%rax
L$cbc_bzero:
	movaps	%xmm0,(%rax)
	leaq	16(%rax),%rax
	cmpq	%rax,%rbp
	ja	L$cbc_bzero

L$cbc_done:
	leaq	(%rbp),%rsp
	jmp	L$cbc_exit

.p2align	4
L$cbc_aligned:
	leaq	(%rsi,%rcx,1),%rbp
	negq	%rbp
	andq	$0xfff,%rbp
	xorl	%eax,%eax
	cmpq	$64,%rbp
	movq	$64-1,%rbp
	cmovaeq	%rax,%rbp
	andq	%rcx,%rbp
	subq	%rbp,%rcx
	jz	L$cbc_aligned_tail
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,208
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
	testq	%rbp,%rbp
	jz	L$cbc_exit

L$cbc_aligned_tail:
	movq	%rdi,%r8
	movq	%rbp,%rbx
	movq	%rbp,%rcx
	leaq	(%rsp),%rbp
	subq	%rcx,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	leaq	(%r8),%rdi
	leaq	(%rsp),%rsi
	movq	%rbx,%rcx
	jmp	L$cbc_loop
L$cbc_exit:
	movl	$1,%eax
	leaq	8(%rsp),%rsp
L$cbc_abort:
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3

.globl	_padlock_cfb_encrypt

.p2align	4
_padlock_cfb_encrypt:
	pushq	%rbp
	pushq	%rbx

	xorl	%eax,%eax
	testq	$15,%rdx
	jnz	L$cfb_abort
	testq	$15,%rcx
	jnz	L$cfb_abort
	leaq	L$padlock_saved_context(%rip),%rax
	pushf
	cld
	call	_padlock_verify_ctx
	leaq	16(%rdx),%rdx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	testl	$32,(%rdx)
	jnz	L$cfb_aligned
	testq	$0x0f,%rdi
	setz	%al
	testq	$0x0f,%rsi
	setz	%bl
	testl	%ebx,%eax
	jnz	L$cfb_aligned
	negq	%rax
	movq	$512,%rbx
	notq	%rax
	leaq	(%rsp),%rbp
	cmpq	%rbx,%rcx
	cmovcq	%rcx,%rbx
	andq	%rbx,%rax
	movq	%rcx,%rbx
	negq	%rax
	andq	$512-1,%rbx
	leaq	(%rax,%rbp,1),%rsp
	movq	$512,%rax
	cmovzq	%rax,%rbx
	jmp	L$cfb_loop
.p2align	4
L$cfb_loop:
	cmpq	%rcx,%rbx
	cmovaq	%rcx,%rbx
	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11
	testq	$0x0f,%rdi
	cmovnzq	%rsp,%rdi
	testq	$0x0f,%rsi
	jz	L$cfb_inp_aligned
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
	movq	%rbx,%rcx
	movq	%rdi,%rsi
L$cfb_inp_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,224
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
	movq	%r8,%rdi
	movq	%r11,%rbx
	testq	$0x0f,%rdi
	jz	L$cfb_out_aligned
	movq	%rbx,%rcx
	leaq	(%rsp),%rsi
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
L$cfb_out_aligned:
	movq	%r9,%rsi
	movq	%r10,%rcx
	addq	%rbx,%rdi
	addq	%rbx,%rsi
	subq	%rbx,%rcx
	movq	$512,%rbx
	jnz	L$cfb_loop
	cmpq	%rbp,%rsp
	je	L$cfb_done

	pxor	%xmm0,%xmm0
	leaq	(%rsp),%rax
L$cfb_bzero:
	movaps	%xmm0,(%rax)
	leaq	16(%rax),%rax
	cmpq	%rax,%rbp
	ja	L$cfb_bzero

L$cfb_done:
	leaq	(%rbp),%rsp
	jmp	L$cfb_exit

.p2align	4
L$cfb_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,224
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
L$cfb_exit:
	movl	$1,%eax
	leaq	8(%rsp),%rsp
L$cfb_abort:
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3

.globl	_padlock_ofb_encrypt

.p2align	4
_padlock_ofb_encrypt:
	pushq	%rbp
	pushq	%rbx

	xorl	%eax,%eax
	testq	$15,%rdx
	jnz	L$ofb_abort
	testq	$15,%rcx
	jnz	L$ofb_abort
	leaq	L$padlock_saved_context(%rip),%rax
	pushf
	cld
	call	_padlock_verify_ctx
	leaq	16(%rdx),%rdx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	testl	$32,(%rdx)
	jnz	L$ofb_aligned
	testq	$0x0f,%rdi
	setz	%al
	testq	$0x0f,%rsi
	setz	%bl
	testl	%ebx,%eax
	jnz	L$ofb_aligned
	negq	%rax
	movq	$512,%rbx
	notq	%rax
	leaq	(%rsp),%rbp
	cmpq	%rbx,%rcx
	cmovcq	%rcx,%rbx
	andq	%rbx,%rax
	movq	%rcx,%rbx
	negq	%rax
	andq	$512-1,%rbx
	leaq	(%rax,%rbp,1),%rsp
	movq	$512,%rax
	cmovzq	%rax,%rbx
	jmp	L$ofb_loop
.p2align	4
L$ofb_loop:
	cmpq	%rcx,%rbx
	cmovaq	%rcx,%rbx
	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11
	testq	$0x0f,%rdi
	cmovnzq	%rsp,%rdi
	testq	$0x0f,%rsi
	jz	L$ofb_inp_aligned
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
	movq	%rbx,%rcx
	movq	%rdi,%rsi
L$ofb_inp_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,232
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
	movq	%r8,%rdi
	movq	%r11,%rbx
	testq	$0x0f,%rdi
	jz	L$ofb_out_aligned
	movq	%rbx,%rcx
	leaq	(%rsp),%rsi
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
L$ofb_out_aligned:
	movq	%r9,%rsi
	movq	%r10,%rcx
	addq	%rbx,%rdi
	addq	%rbx,%rsi
	subq	%rbx,%rcx
	movq	$512,%rbx
	jnz	L$ofb_loop
	cmpq	%rbp,%rsp
	je	L$ofb_done

	pxor	%xmm0,%xmm0
	leaq	(%rsp),%rax
L$ofb_bzero:
	movaps	%xmm0,(%rax)
	leaq	16(%rax),%rax
	cmpq	%rax,%rbp
	ja	L$ofb_bzero

L$ofb_done:
	leaq	(%rbp),%rsp
	jmp	L$ofb_exit

.p2align	4
L$ofb_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,232
	movdqa	(%rax),%xmm0
	movdqa	%xmm0,-16(%rdx)
L$ofb_exit:
	movl	$1,%eax
	leaq	8(%rsp),%rsp
L$ofb_abort:
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3

.globl	_padlock_ctr32_encrypt

.p2align	4
_padlock_ctr32_encrypt:
	pushq	%rbp
	pushq	%rbx

	xorl	%eax,%eax
	testq	$15,%rdx
	jnz	L$ctr32_abort
	testq	$15,%rcx
	jnz	L$ctr32_abort
	leaq	L$padlock_saved_context(%rip),%rax
	pushf
	cld
	call	_padlock_verify_ctx
	leaq	16(%rdx),%rdx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	testl	$32,(%rdx)
	jnz	L$ctr32_aligned
	testq	$0x0f,%rdi
	setz	%al
	testq	$0x0f,%rsi
	setz	%bl
	testl	%ebx,%eax
	jnz	L$ctr32_aligned
	negq	%rax
	movq	$512,%rbx
	notq	%rax
	leaq	(%rsp),%rbp
	cmpq	%rbx,%rcx
	cmovcq	%rcx,%rbx
	andq	%rbx,%rax
	movq	%rcx,%rbx
	negq	%rax
	andq	$512-1,%rbx
	leaq	(%rax,%rbp,1),%rsp
	movq	$512,%rax
	cmovzq	%rax,%rbx
L$ctr32_reenter:
	movl	-4(%rdx),%eax
	bswapl	%eax
	negl	%eax
	andl	$31,%eax
	movq	$512,%rbx
	shll	$4,%eax
	cmovzq	%rbx,%rax
	cmpq	%rax,%rcx
	cmovaq	%rax,%rbx
	cmovbeq	%rcx,%rbx
	cmpq	%rbx,%rcx
	ja	L$ctr32_loop
	movq	%rsi,%rax
	cmpq	%rsp,%rbp
	cmoveq	%rdi,%rax
	addq	%rcx,%rax
	negq	%rax
	andq	$0xfff,%rax
	cmpq	$32,%rax
	movq	$-32,%rax
	cmovaeq	%rbx,%rax
	andq	%rax,%rbx
	jz	L$ctr32_unaligned_tail
	jmp	L$ctr32_loop
.p2align	4
L$ctr32_loop:
	cmpq	%rcx,%rbx
	cmovaq	%rcx,%rbx
	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11
	testq	$0x0f,%rdi
	cmovnzq	%rsp,%rdi
	testq	$0x0f,%rsi
	jz	L$ctr32_inp_aligned
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
	movq	%rbx,%rcx
	movq	%rdi,%rsi
L$ctr32_inp_aligned:
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,216
	movl	-4(%rdx),%eax
	testl	$0xffff0000,%eax
	jnz	L$ctr32_no_carry
	bswapl	%eax
	addl	$0x10000,%eax
	bswapl	%eax
	movl	%eax,-4(%rdx)
L$ctr32_no_carry:
	movq	%r8,%rdi
	movq	%r11,%rbx
	testq	$0x0f,%rdi
	jz	L$ctr32_out_aligned
	movq	%rbx,%rcx
	leaq	(%rsp),%rsi
	shrq	$3,%rcx
.byte	0xf3,0x48,0xa5
	subq	%rbx,%rdi
L$ctr32_out_aligned:
	movq	%r9,%rsi
	movq	%r10,%rcx
	addq	%rbx,%rdi
	addq	%rbx,%rsi
	subq	%rbx,%rcx
	movq	$512,%rbx
	jz	L$ctr32_break
	cmpq	%rbx,%rcx
	jae	L$ctr32_loop
	movq	%rcx,%rbx
	movq	%rsi,%rax
	cmpq	%rsp,%rbp
	cmoveq	%rdi,%rax
	addq	%rcx,%rax
	negq	%rax
	andq	$0xfff,%rax
	cmpq	$32,%rax
	movq	$-32,%rax
	cmovaeq	%rbx,%rax
	andq	%rax,%rbx
	jnz	L$ctr32_loop
L$ctr32_unaligned_tail:
	xorl	%eax,%eax
	cmpq	%rsp,%rbp
	cmoveq	%rcx,%rax
	movq	%rdi,%r8
	movq	%rcx,%rbx
	subq	%rax,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	movq	%rsp,%rsi
	movq	%r8,%rdi
	movq	%rbx,%rcx
	jmp	L$ctr32_loop
.p2align	4
L$ctr32_break:
	cmpq	%rbp,%rsp
	je	L$ctr32_done

	pxor	%xmm0,%xmm0
	leaq	(%rsp),%rax
L$ctr32_bzero:
	movaps	%xmm0,(%rax)
	leaq	16(%rax),%rax
	cmpq	%rax,%rbp
	ja	L$ctr32_bzero

L$ctr32_done:
	leaq	(%rbp),%rsp
	jmp	L$ctr32_exit

.p2align	4
L$ctr32_aligned:
	movl	-4(%rdx),%eax
	bswapl	%eax
	negl	%eax
	andl	$0xffff,%eax
	movq	$1048576,%rbx
	shll	$4,%eax
	cmovzq	%rbx,%rax
	cmpq	%rax,%rcx
	cmovaq	%rax,%rbx
	cmovbeq	%rcx,%rbx
	jbe	L$ctr32_aligned_skip

L$ctr32_aligned_loop:
	movq	%rcx,%r10
	movq	%rbx,%rcx
	movq	%rbx,%r11

	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,216

	movl	-4(%rdx),%eax
	bswapl	%eax
	addl	$0x10000,%eax
	bswapl	%eax
	movl	%eax,-4(%rdx)

	movq	%r10,%rcx
	subq	%r11,%rcx
	movq	$1048576,%rbx
	jz	L$ctr32_exit
	cmpq	%rbx,%rcx
	jae	L$ctr32_aligned_loop

L$ctr32_aligned_skip:
	leaq	(%rsi,%rcx,1),%rbp
	negq	%rbp
	andq	$0xfff,%rbp
	xorl	%eax,%eax
	cmpq	$32,%rbp
	movq	$32-1,%rbp
	cmovaeq	%rax,%rbp
	andq	%rcx,%rbp
	subq	%rbp,%rcx
	jz	L$ctr32_aligned_tail
	leaq	-16(%rdx),%rax
	leaq	16(%rdx),%rbx
	shrq	$4,%rcx
.byte	0xf3,0x0f,0xa7,216
	testq	%rbp,%rbp
	jz	L$ctr32_exit

L$ctr32_aligned_tail:
	movq	%rdi,%r8
	movq	%rbp,%rbx
	movq	%rbp,%rcx
	leaq	(%rsp),%rbp
	subq	%rcx,%rsp
	shrq	$3,%rcx
	leaq	(%rsp),%rdi
.byte	0xf3,0x48,0xa5
	leaq	(%r8),%rdi
	leaq	(%rsp),%rsi
	movq	%rbx,%rcx
	jmp	L$ctr32_loop
L$ctr32_exit:
	movl	$1,%eax
	leaq	8(%rsp),%rsp
L$ctr32_abort:
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3

.byte	86,73,65,32,80,97,100,108,111,99,107,32,120,56,54,95,54,52,32,109,111,100,117,108,101,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4
.data	
.p2align	3
L$padlock_saved_context:
.quad	0
