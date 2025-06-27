.text	


.globl	_Camellia_EncryptBlock

.p2align	4
_Camellia_EncryptBlock:

	movl	$128,%eax
	subl	%edi,%eax
	movl	$3,%edi
	adcl	$0,%edi
	jmp	L$enc_rounds



.globl	_Camellia_EncryptBlock_Rounds

.p2align	4
L$enc_rounds:
_Camellia_EncryptBlock_Rounds:

	pushq	%rbx

	pushq	%rbp

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$enc_prologue:


	movq	%rcx,%r13
	movq	%rdx,%r14

	shll	$6,%edi
	leaq	L$Camellia_SBOX(%rip),%rbp
	leaq	(%r14,%rdi,1),%r15

	movl	0(%rsi),%r8d
	movl	4(%rsi),%r9d
	movl	8(%rsi),%r10d
	bswapl	%r8d
	movl	12(%rsi),%r11d
	bswapl	%r9d
	bswapl	%r10d
	bswapl	%r11d

	call	_x86_64_Camellia_encrypt

	bswapl	%r8d
	bswapl	%r9d
	bswapl	%r10d
	movl	%r8d,0(%r13)
	bswapl	%r11d
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	movl	%r11d,12(%r13)

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%rbp

	movq	32(%rsp),%rbx

	leaq	40(%rsp),%rsp

L$enc_epilogue:
	.byte	0xf3,0xc3




.p2align	4
_x86_64_Camellia_encrypt:

	xorl	0(%r14),%r9d
	xorl	4(%r14),%r8d
	xorl	8(%r14),%r11d
	xorl	12(%r14),%r10d
.p2align	4
L$eloop:
	movl	16(%r14),%ebx
	movl	20(%r14),%eax

	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	24(%r14),%ebx
	movl	28(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	32(%r14),%ebx
	movl	36(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	40(%r14),%ebx
	movl	44(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	48(%r14),%ebx
	movl	52(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	56(%r14),%ebx
	movl	60(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	64(%r14),%ebx
	movl	68(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	leaq	64(%r14),%r14
	cmpq	%r15,%r14
	movl	8(%r14),%edx
	movl	12(%r14),%ecx
	je	L$edone

	andl	%r8d,%eax
	orl	%r11d,%edx
	roll	$1,%eax
	xorl	%edx,%r10d
	xorl	%eax,%r9d
	andl	%r10d,%ecx
	orl	%r9d,%ebx
	roll	$1,%ecx
	xorl	%ebx,%r8d
	xorl	%ecx,%r11d
	jmp	L$eloop

.p2align	4
L$edone:
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	xorl	%r8d,%ecx
	xorl	%r9d,%edx

	movl	%eax,%r8d
	movl	%ebx,%r9d
	movl	%ecx,%r10d
	movl	%edx,%r11d

.byte	0xf3,0xc3




.globl	_Camellia_DecryptBlock

.p2align	4
_Camellia_DecryptBlock:

	movl	$128,%eax
	subl	%edi,%eax
	movl	$3,%edi
	adcl	$0,%edi
	jmp	L$dec_rounds



.globl	_Camellia_DecryptBlock_Rounds

.p2align	4
L$dec_rounds:
_Camellia_DecryptBlock_Rounds:

	pushq	%rbx

	pushq	%rbp

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$dec_prologue:


	movq	%rcx,%r13
	movq	%rdx,%r15

	shll	$6,%edi
	leaq	L$Camellia_SBOX(%rip),%rbp
	leaq	(%r15,%rdi,1),%r14

	movl	0(%rsi),%r8d
	movl	4(%rsi),%r9d
	movl	8(%rsi),%r10d
	bswapl	%r8d
	movl	12(%rsi),%r11d
	bswapl	%r9d
	bswapl	%r10d
	bswapl	%r11d

	call	_x86_64_Camellia_decrypt

	bswapl	%r8d
	bswapl	%r9d
	bswapl	%r10d
	movl	%r8d,0(%r13)
	bswapl	%r11d
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	movl	%r11d,12(%r13)

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%rbp

	movq	32(%rsp),%rbx

	leaq	40(%rsp),%rsp

L$dec_epilogue:
	.byte	0xf3,0xc3




.p2align	4
_x86_64_Camellia_decrypt:

	xorl	0(%r14),%r9d
	xorl	4(%r14),%r8d
	xorl	8(%r14),%r11d
	xorl	12(%r14),%r10d
.p2align	4
L$dloop:
	movl	-8(%r14),%ebx
	movl	-4(%r14),%eax

	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-16(%r14),%ebx
	movl	-12(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-24(%r14),%ebx
	movl	-20(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-32(%r14),%ebx
	movl	-28(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-40(%r14),%ebx
	movl	-36(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-48(%r14),%ebx
	movl	-44(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	-56(%r14),%ebx
	movl	-52(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	leaq	-64(%r14),%r14
	cmpq	%r15,%r14
	movl	0(%r14),%edx
	movl	4(%r14),%ecx
	je	L$ddone

	andl	%r8d,%eax
	orl	%r11d,%edx
	roll	$1,%eax
	xorl	%edx,%r10d
	xorl	%eax,%r9d
	andl	%r10d,%ecx
	orl	%r9d,%ebx
	roll	$1,%ecx
	xorl	%ebx,%r8d
	xorl	%ecx,%r11d

	jmp	L$dloop

.p2align	4
L$ddone:
	xorl	%r10d,%ecx
	xorl	%r11d,%edx
	xorl	%r8d,%eax
	xorl	%r9d,%ebx

	movl	%ecx,%r8d
	movl	%edx,%r9d
	movl	%eax,%r10d
	movl	%ebx,%r11d

.byte	0xf3,0xc3


.globl	_Camellia_Ekeygen

.p2align	4
_Camellia_Ekeygen:

	pushq	%rbx

	pushq	%rbp

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$key_prologue:

	movl	%edi,%r15d
	movq	%rdx,%r13

	movl	0(%rsi),%r8d
	movl	4(%rsi),%r9d
	movl	8(%rsi),%r10d
	movl	12(%rsi),%r11d

	bswapl	%r8d
	bswapl	%r9d
	bswapl	%r10d
	bswapl	%r11d
	movl	%r9d,0(%r13)
	movl	%r8d,4(%r13)
	movl	%r11d,8(%r13)
	movl	%r10d,12(%r13)
	cmpq	$128,%r15
	je	L$1st128

	movl	16(%rsi),%r8d
	movl	20(%rsi),%r9d
	cmpq	$192,%r15
	je	L$1st192
	movl	24(%rsi),%r10d
	movl	28(%rsi),%r11d
	jmp	L$1st256
L$1st192:
	movl	%r8d,%r10d
	movl	%r9d,%r11d
	notl	%r10d
	notl	%r11d
L$1st256:
	bswapl	%r8d
	bswapl	%r9d
	bswapl	%r10d
	bswapl	%r11d
	movl	%r9d,32(%r13)
	movl	%r8d,36(%r13)
	movl	%r11d,40(%r13)
	movl	%r10d,44(%r13)
	xorl	0(%r13),%r9d
	xorl	4(%r13),%r8d
	xorl	8(%r13),%r11d
	xorl	12(%r13),%r10d

L$1st128:
	leaq	L$Camellia_SIGMA(%rip),%r14
	leaq	L$Camellia_SBOX(%rip),%rbp

	movl	0(%r14),%ebx
	movl	4(%r14),%eax
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	8(%r14),%ebx
	movl	12(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	16(%r14),%ebx
	movl	20(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	xorl	0(%r13),%r9d
	xorl	4(%r13),%r8d
	xorl	8(%r13),%r11d
	xorl	12(%r13),%r10d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	24(%r14),%ebx
	movl	28(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	32(%r14),%ebx
	movl	36(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	cmpq	$128,%r15
	jne	L$2nd256

	leaq	128(%r13),%r13
	shlq	$32,%r8
	shlq	$32,%r10
	orq	%r9,%r8
	orq	%r11,%r10
	movq	-128(%r13),%rax
	movq	-120(%r13),%rbx
	movq	%r8,-112(%r13)
	movq	%r10,-104(%r13)
	movq	%rax,%r11
	shlq	$15,%rax
	movq	%rbx,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%rax
	shlq	$15,%rbx
	orq	%r11,%rbx
	movq	%rax,-96(%r13)
	movq	%rbx,-88(%r13)
	movq	%r8,%r11
	shlq	$15,%r8
	movq	%r10,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%r8
	shlq	$15,%r10
	orq	%r11,%r10
	movq	%r8,-80(%r13)
	movq	%r10,-72(%r13)
	movq	%r8,%r11
	shlq	$15,%r8
	movq	%r10,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%r8
	shlq	$15,%r10
	orq	%r11,%r10
	movq	%r8,-64(%r13)
	movq	%r10,-56(%r13)
	movq	%rax,%r11
	shlq	$30,%rax
	movq	%rbx,%r9
	shrq	$34,%r9
	shrq	$34,%r11
	orq	%r9,%rax
	shlq	$30,%rbx
	orq	%r11,%rbx
	movq	%rax,-48(%r13)
	movq	%rbx,-40(%r13)
	movq	%r8,%r11
	shlq	$15,%r8
	movq	%r10,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%r8
	shlq	$15,%r10
	orq	%r11,%r10
	movq	%r8,-32(%r13)
	movq	%rax,%r11
	shlq	$15,%rax
	movq	%rbx,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%rax
	shlq	$15,%rbx
	orq	%r11,%rbx
	movq	%rbx,-24(%r13)
	movq	%r8,%r11
	shlq	$15,%r8
	movq	%r10,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%r8
	shlq	$15,%r10
	orq	%r11,%r10
	movq	%r8,-16(%r13)
	movq	%r10,-8(%r13)
	movq	%rax,%r11
	shlq	$17,%rax
	movq	%rbx,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%rax
	shlq	$17,%rbx
	orq	%r11,%rbx
	movq	%rax,0(%r13)
	movq	%rbx,8(%r13)
	movq	%rax,%r11
	shlq	$17,%rax
	movq	%rbx,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%rax
	shlq	$17,%rbx
	orq	%r11,%rbx
	movq	%rax,16(%r13)
	movq	%rbx,24(%r13)
	movq	%r8,%r11
	shlq	$34,%r8
	movq	%r10,%r9
	shrq	$30,%r9
	shrq	$30,%r11
	orq	%r9,%r8
	shlq	$34,%r10
	orq	%r11,%r10
	movq	%r8,32(%r13)
	movq	%r10,40(%r13)
	movq	%rax,%r11
	shlq	$17,%rax
	movq	%rbx,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%rax
	shlq	$17,%rbx
	orq	%r11,%rbx
	movq	%rax,48(%r13)
	movq	%rbx,56(%r13)
	movq	%r8,%r11
	shlq	$17,%r8
	movq	%r10,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%r8
	shlq	$17,%r10
	orq	%r11,%r10
	movq	%r8,64(%r13)
	movq	%r10,72(%r13)
	movl	$3,%eax
	jmp	L$done
.p2align	4
L$2nd256:
	movl	%r9d,48(%r13)
	movl	%r8d,52(%r13)
	movl	%r11d,56(%r13)
	movl	%r10d,60(%r13)
	xorl	32(%r13),%r9d
	xorl	36(%r13),%r8d
	xorl	40(%r13),%r11d
	xorl	44(%r13),%r10d
	xorl	%r8d,%eax
	xorl	%r9d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	40(%r14),%ebx
	movl	44(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r10d
	xorl	%ecx,%r11d
	xorl	%edx,%r11d
	xorl	%r10d,%eax
	xorl	%r11d,%ebx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	movl	2052(%rbp,%rsi,8),%edx
	movl	0(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	shrl	$16,%eax
	movzbl	%bh,%edi
	xorl	4(%rbp,%rsi,8),%edx
	shrl	$16,%ebx
	xorl	4(%rbp,%rdi,8),%ecx
	movzbl	%ah,%esi
	movzbl	%bl,%edi
	xorl	0(%rbp,%rsi,8),%edx
	xorl	2052(%rbp,%rdi,8),%ecx
	movzbl	%al,%esi
	movzbl	%bh,%edi
	xorl	2048(%rbp,%rsi,8),%edx
	xorl	2048(%rbp,%rdi,8),%ecx
	movl	48(%r14),%ebx
	movl	52(%r14),%eax
	xorl	%edx,%ecx
	rorl	$8,%edx
	xorl	%ecx,%r8d
	xorl	%ecx,%r9d
	xorl	%edx,%r9d
	movq	0(%r13),%rax
	movq	8(%r13),%rbx
	movq	32(%r13),%rcx
	movq	40(%r13),%rdx
	movq	48(%r13),%r14
	movq	56(%r13),%r15
	leaq	128(%r13),%r13
	shlq	$32,%r8
	shlq	$32,%r10
	orq	%r9,%r8
	orq	%r11,%r10
	movq	%r8,-112(%r13)
	movq	%r10,-104(%r13)
	movq	%rcx,%r11
	shlq	$15,%rcx
	movq	%rdx,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%rcx
	shlq	$15,%rdx
	orq	%r11,%rdx
	movq	%rcx,-96(%r13)
	movq	%rdx,-88(%r13)
	movq	%r14,%r11
	shlq	$15,%r14
	movq	%r15,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%r14
	shlq	$15,%r15
	orq	%r11,%r15
	movq	%r14,-80(%r13)
	movq	%r15,-72(%r13)
	movq	%rcx,%r11
	shlq	$15,%rcx
	movq	%rdx,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%rcx
	shlq	$15,%rdx
	orq	%r11,%rdx
	movq	%rcx,-64(%r13)
	movq	%rdx,-56(%r13)
	movq	%r8,%r11
	shlq	$30,%r8
	movq	%r10,%r9
	shrq	$34,%r9
	shrq	$34,%r11
	orq	%r9,%r8
	shlq	$30,%r10
	orq	%r11,%r10
	movq	%r8,-48(%r13)
	movq	%r10,-40(%r13)
	movq	%rax,%r11
	shlq	$45,%rax
	movq	%rbx,%r9
	shrq	$19,%r9
	shrq	$19,%r11
	orq	%r9,%rax
	shlq	$45,%rbx
	orq	%r11,%rbx
	movq	%rax,-32(%r13)
	movq	%rbx,-24(%r13)
	movq	%r14,%r11
	shlq	$30,%r14
	movq	%r15,%r9
	shrq	$34,%r9
	shrq	$34,%r11
	orq	%r9,%r14
	shlq	$30,%r15
	orq	%r11,%r15
	movq	%r14,-16(%r13)
	movq	%r15,-8(%r13)
	movq	%rax,%r11
	shlq	$15,%rax
	movq	%rbx,%r9
	shrq	$49,%r9
	shrq	$49,%r11
	orq	%r9,%rax
	shlq	$15,%rbx
	orq	%r11,%rbx
	movq	%rax,0(%r13)
	movq	%rbx,8(%r13)
	movq	%rcx,%r11
	shlq	$30,%rcx
	movq	%rdx,%r9
	shrq	$34,%r9
	shrq	$34,%r11
	orq	%r9,%rcx
	shlq	$30,%rdx
	orq	%r11,%rdx
	movq	%rcx,16(%r13)
	movq	%rdx,24(%r13)
	movq	%r8,%r11
	shlq	$30,%r8
	movq	%r10,%r9
	shrq	$34,%r9
	shrq	$34,%r11
	orq	%r9,%r8
	shlq	$30,%r10
	orq	%r11,%r10
	movq	%r8,32(%r13)
	movq	%r10,40(%r13)
	movq	%rax,%r11
	shlq	$17,%rax
	movq	%rbx,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%rax
	shlq	$17,%rbx
	orq	%r11,%rbx
	movq	%rax,48(%r13)
	movq	%rbx,56(%r13)
	movq	%r14,%r11
	shlq	$32,%r14
	movq	%r15,%r9
	shrq	$32,%r9
	shrq	$32,%r11
	orq	%r9,%r14
	shlq	$32,%r15
	orq	%r11,%r15
	movq	%r14,64(%r13)
	movq	%r15,72(%r13)
	movq	%rcx,%r11
	shlq	$34,%rcx
	movq	%rdx,%r9
	shrq	$30,%r9
	shrq	$30,%r11
	orq	%r9,%rcx
	shlq	$34,%rdx
	orq	%r11,%rdx
	movq	%rcx,80(%r13)
	movq	%rdx,88(%r13)
	movq	%r14,%r11
	shlq	$17,%r14
	movq	%r15,%r9
	shrq	$47,%r9
	shrq	$47,%r11
	orq	%r9,%r14
	shlq	$17,%r15
	orq	%r11,%r15
	movq	%r14,96(%r13)
	movq	%r15,104(%r13)
	movq	%rax,%r11
	shlq	$34,%rax
	movq	%rbx,%r9
	shrq	$30,%r9
	shrq	$30,%r11
	orq	%r9,%rax
	shlq	$34,%rbx
	orq	%r11,%rbx
	movq	%rax,112(%r13)
	movq	%rbx,120(%r13)
	movq	%r8,%r11
	shlq	$51,%r8
	movq	%r10,%r9
	shrq	$13,%r9
	shrq	$13,%r11
	orq	%r9,%r8
	shlq	$51,%r10
	orq	%r11,%r10
	movq	%r8,128(%r13)
	movq	%r10,136(%r13)
	movl	$4,%eax
L$done:
	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%rbp

	movq	32(%rsp),%rbx

	leaq	40(%rsp),%rsp

L$key_epilogue:
	.byte	0xf3,0xc3


.p2align	6
L$Camellia_SIGMA:
.long	0x3bcc908b, 0xa09e667f, 0x4caa73b2, 0xb67ae858
.long	0xe94f82be, 0xc6ef372f, 0xf1d36f1c, 0x54ff53a5
.long	0xde682d1d, 0x10e527fa, 0xb3e6c1fd, 0xb05688c2
.long	0,          0,          0,          0
L$Camellia_SBOX:
.long	0x70707000,0x70700070
.long	0x82828200,0x2c2c002c
.long	0x2c2c2c00,0xb3b300b3
.long	0xececec00,0xc0c000c0
.long	0xb3b3b300,0xe4e400e4
.long	0x27272700,0x57570057
.long	0xc0c0c000,0xeaea00ea
.long	0xe5e5e500,0xaeae00ae
.long	0xe4e4e400,0x23230023
.long	0x85858500,0x6b6b006b
.long	0x57575700,0x45450045
.long	0x35353500,0xa5a500a5
.long	0xeaeaea00,0xeded00ed
.long	0x0c0c0c00,0x4f4f004f
.long	0xaeaeae00,0x1d1d001d
.long	0x41414100,0x92920092
.long	0x23232300,0x86860086
.long	0xefefef00,0xafaf00af
.long	0x6b6b6b00,0x7c7c007c
.long	0x93939300,0x1f1f001f
.long	0x45454500,0x3e3e003e
.long	0x19191900,0xdcdc00dc
.long	0xa5a5a500,0x5e5e005e
.long	0x21212100,0x0b0b000b
.long	0xededed00,0xa6a600a6
.long	0x0e0e0e00,0x39390039
.long	0x4f4f4f00,0xd5d500d5
.long	0x4e4e4e00,0x5d5d005d
.long	0x1d1d1d00,0xd9d900d9
.long	0x65656500,0x5a5a005a
.long	0x92929200,0x51510051
.long	0xbdbdbd00,0x6c6c006c
.long	0x86868600,0x8b8b008b
.long	0xb8b8b800,0x9a9a009a
.long	0xafafaf00,0xfbfb00fb
.long	0x8f8f8f00,0xb0b000b0
.long	0x7c7c7c00,0x74740074
.long	0xebebeb00,0x2b2b002b
.long	0x1f1f1f00,0xf0f000f0
.long	0xcecece00,0x84840084
.long	0x3e3e3e00,0xdfdf00df
.long	0x30303000,0xcbcb00cb
.long	0xdcdcdc00,0x34340034
.long	0x5f5f5f00,0x76760076
.long	0x5e5e5e00,0x6d6d006d
.long	0xc5c5c500,0xa9a900a9
.long	0x0b0b0b00,0xd1d100d1
.long	0x1a1a1a00,0x04040004
.long	0xa6a6a600,0x14140014
.long	0xe1e1e100,0x3a3a003a
.long	0x39393900,0xdede00de
.long	0xcacaca00,0x11110011
.long	0xd5d5d500,0x32320032
.long	0x47474700,0x9c9c009c
.long	0x5d5d5d00,0x53530053
.long	0x3d3d3d00,0xf2f200f2
.long	0xd9d9d900,0xfefe00fe
.long	0x01010100,0xcfcf00cf
.long	0x5a5a5a00,0xc3c300c3
.long	0xd6d6d600,0x7a7a007a
.long	0x51515100,0x24240024
.long	0x56565600,0xe8e800e8
.long	0x6c6c6c00,0x60600060
.long	0x4d4d4d00,0x69690069
.long	0x8b8b8b00,0xaaaa00aa
.long	0x0d0d0d00,0xa0a000a0
.long	0x9a9a9a00,0xa1a100a1
.long	0x66666600,0x62620062
.long	0xfbfbfb00,0x54540054
.long	0xcccccc00,0x1e1e001e
.long	0xb0b0b000,0xe0e000e0
.long	0x2d2d2d00,0x64640064
.long	0x74747400,0x10100010
.long	0x12121200,0x00000000
.long	0x2b2b2b00,0xa3a300a3
.long	0x20202000,0x75750075
.long	0xf0f0f000,0x8a8a008a
.long	0xb1b1b100,0xe6e600e6
.long	0x84848400,0x09090009
.long	0x99999900,0xdddd00dd
.long	0xdfdfdf00,0x87870087
.long	0x4c4c4c00,0x83830083
.long	0xcbcbcb00,0xcdcd00cd
.long	0xc2c2c200,0x90900090
.long	0x34343400,0x73730073
.long	0x7e7e7e00,0xf6f600f6
.long	0x76767600,0x9d9d009d
.long	0x05050500,0xbfbf00bf
.long	0x6d6d6d00,0x52520052
.long	0xb7b7b700,0xd8d800d8
.long	0xa9a9a900,0xc8c800c8
.long	0x31313100,0xc6c600c6
.long	0xd1d1d100,0x81810081
.long	0x17171700,0x6f6f006f
.long	0x04040400,0x13130013
.long	0xd7d7d700,0x63630063
.long	0x14141400,0xe9e900e9
.long	0x58585800,0xa7a700a7
.long	0x3a3a3a00,0x9f9f009f
.long	0x61616100,0xbcbc00bc
.long	0xdedede00,0x29290029
.long	0x1b1b1b00,0xf9f900f9
.long	0x11111100,0x2f2f002f
.long	0x1c1c1c00,0xb4b400b4
.long	0x32323200,0x78780078
.long	0x0f0f0f00,0x06060006
.long	0x9c9c9c00,0xe7e700e7
.long	0x16161600,0x71710071
.long	0x53535300,0xd4d400d4
.long	0x18181800,0xabab00ab
.long	0xf2f2f200,0x88880088
.long	0x22222200,0x8d8d008d
.long	0xfefefe00,0x72720072
.long	0x44444400,0xb9b900b9
.long	0xcfcfcf00,0xf8f800f8
.long	0xb2b2b200,0xacac00ac
.long	0xc3c3c300,0x36360036
.long	0xb5b5b500,0x2a2a002a
.long	0x7a7a7a00,0x3c3c003c
.long	0x91919100,0xf1f100f1
.long	0x24242400,0x40400040
.long	0x08080800,0xd3d300d3
.long	0xe8e8e800,0xbbbb00bb
.long	0xa8a8a800,0x43430043
.long	0x60606000,0x15150015
.long	0xfcfcfc00,0xadad00ad
.long	0x69696900,0x77770077
.long	0x50505000,0x80800080
.long	0xaaaaaa00,0x82820082
.long	0xd0d0d000,0xecec00ec
.long	0xa0a0a000,0x27270027
.long	0x7d7d7d00,0xe5e500e5
.long	0xa1a1a100,0x85850085
.long	0x89898900,0x35350035
.long	0x62626200,0x0c0c000c
.long	0x97979700,0x41410041
.long	0x54545400,0xefef00ef
.long	0x5b5b5b00,0x93930093
.long	0x1e1e1e00,0x19190019
.long	0x95959500,0x21210021
.long	0xe0e0e000,0x0e0e000e
.long	0xffffff00,0x4e4e004e
.long	0x64646400,0x65650065
.long	0xd2d2d200,0xbdbd00bd
.long	0x10101000,0xb8b800b8
.long	0xc4c4c400,0x8f8f008f
.long	0x00000000,0xebeb00eb
.long	0x48484800,0xcece00ce
.long	0xa3a3a300,0x30300030
.long	0xf7f7f700,0x5f5f005f
.long	0x75757500,0xc5c500c5
.long	0xdbdbdb00,0x1a1a001a
.long	0x8a8a8a00,0xe1e100e1
.long	0x03030300,0xcaca00ca
.long	0xe6e6e600,0x47470047
.long	0xdadada00,0x3d3d003d
.long	0x09090900,0x01010001
.long	0x3f3f3f00,0xd6d600d6
.long	0xdddddd00,0x56560056
.long	0x94949400,0x4d4d004d
.long	0x87878700,0x0d0d000d
.long	0x5c5c5c00,0x66660066
.long	0x83838300,0xcccc00cc
.long	0x02020200,0x2d2d002d
.long	0xcdcdcd00,0x12120012
.long	0x4a4a4a00,0x20200020
.long	0x90909000,0xb1b100b1
.long	0x33333300,0x99990099
.long	0x73737300,0x4c4c004c
.long	0x67676700,0xc2c200c2
.long	0xf6f6f600,0x7e7e007e
.long	0xf3f3f300,0x05050005
.long	0x9d9d9d00,0xb7b700b7
.long	0x7f7f7f00,0x31310031
.long	0xbfbfbf00,0x17170017
.long	0xe2e2e200,0xd7d700d7
.long	0x52525200,0x58580058
.long	0x9b9b9b00,0x61610061
.long	0xd8d8d800,0x1b1b001b
.long	0x26262600,0x1c1c001c
.long	0xc8c8c800,0x0f0f000f
.long	0x37373700,0x16160016
.long	0xc6c6c600,0x18180018
.long	0x3b3b3b00,0x22220022
.long	0x81818100,0x44440044
.long	0x96969600,0xb2b200b2
.long	0x6f6f6f00,0xb5b500b5
.long	0x4b4b4b00,0x91910091
.long	0x13131300,0x08080008
.long	0xbebebe00,0xa8a800a8
.long	0x63636300,0xfcfc00fc
.long	0x2e2e2e00,0x50500050
.long	0xe9e9e900,0xd0d000d0
.long	0x79797900,0x7d7d007d
.long	0xa7a7a700,0x89890089
.long	0x8c8c8c00,0x97970097
.long	0x9f9f9f00,0x5b5b005b
.long	0x6e6e6e00,0x95950095
.long	0xbcbcbc00,0xffff00ff
.long	0x8e8e8e00,0xd2d200d2
.long	0x29292900,0xc4c400c4
.long	0xf5f5f500,0x48480048
.long	0xf9f9f900,0xf7f700f7
.long	0xb6b6b600,0xdbdb00db
.long	0x2f2f2f00,0x03030003
.long	0xfdfdfd00,0xdada00da
.long	0xb4b4b400,0x3f3f003f
.long	0x59595900,0x94940094
.long	0x78787800,0x5c5c005c
.long	0x98989800,0x02020002
.long	0x06060600,0x4a4a004a
.long	0x6a6a6a00,0x33330033
.long	0xe7e7e700,0x67670067
.long	0x46464600,0xf3f300f3
.long	0x71717100,0x7f7f007f
.long	0xbababa00,0xe2e200e2
.long	0xd4d4d400,0x9b9b009b
.long	0x25252500,0x26260026
.long	0xababab00,0x37370037
.long	0x42424200,0x3b3b003b
.long	0x88888800,0x96960096
.long	0xa2a2a200,0x4b4b004b
.long	0x8d8d8d00,0xbebe00be
.long	0xfafafa00,0x2e2e002e
.long	0x72727200,0x79790079
.long	0x07070700,0x8c8c008c
.long	0xb9b9b900,0x6e6e006e
.long	0x55555500,0x8e8e008e
.long	0xf8f8f800,0xf5f500f5
.long	0xeeeeee00,0xb6b600b6
.long	0xacacac00,0xfdfd00fd
.long	0x0a0a0a00,0x59590059
.long	0x36363600,0x98980098
.long	0x49494900,0x6a6a006a
.long	0x2a2a2a00,0x46460046
.long	0x68686800,0xbaba00ba
.long	0x3c3c3c00,0x25250025
.long	0x38383800,0x42420042
.long	0xf1f1f100,0xa2a200a2
.long	0xa4a4a400,0xfafa00fa
.long	0x40404000,0x07070007
.long	0x28282800,0x55550055
.long	0xd3d3d300,0xeeee00ee
.long	0x7b7b7b00,0x0a0a000a
.long	0xbbbbbb00,0x49490049
.long	0xc9c9c900,0x68680068
.long	0x43434300,0x38380038
.long	0xc1c1c100,0xa4a400a4
.long	0x15151500,0x28280028
.long	0xe3e3e300,0x7b7b007b
.long	0xadadad00,0xc9c900c9
.long	0xf4f4f400,0xc1c100c1
.long	0x77777700,0xe3e300e3
.long	0xc7c7c700,0xf4f400f4
.long	0x80808000,0xc7c700c7
.long	0x9e9e9e00,0x9e9e009e
.long	0x00e0e0e0,0x38003838
.long	0x00050505,0x41004141
.long	0x00585858,0x16001616
.long	0x00d9d9d9,0x76007676
.long	0x00676767,0xd900d9d9
.long	0x004e4e4e,0x93009393
.long	0x00818181,0x60006060
.long	0x00cbcbcb,0xf200f2f2
.long	0x00c9c9c9,0x72007272
.long	0x000b0b0b,0xc200c2c2
.long	0x00aeaeae,0xab00abab
.long	0x006a6a6a,0x9a009a9a
.long	0x00d5d5d5,0x75007575
.long	0x00181818,0x06000606
.long	0x005d5d5d,0x57005757
.long	0x00828282,0xa000a0a0
.long	0x00464646,0x91009191
.long	0x00dfdfdf,0xf700f7f7
.long	0x00d6d6d6,0xb500b5b5
.long	0x00272727,0xc900c9c9
.long	0x008a8a8a,0xa200a2a2
.long	0x00323232,0x8c008c8c
.long	0x004b4b4b,0xd200d2d2
.long	0x00424242,0x90009090
.long	0x00dbdbdb,0xf600f6f6
.long	0x001c1c1c,0x07000707
.long	0x009e9e9e,0xa700a7a7
.long	0x009c9c9c,0x27002727
.long	0x003a3a3a,0x8e008e8e
.long	0x00cacaca,0xb200b2b2
.long	0x00252525,0x49004949
.long	0x007b7b7b,0xde00dede
.long	0x000d0d0d,0x43004343
.long	0x00717171,0x5c005c5c
.long	0x005f5f5f,0xd700d7d7
.long	0x001f1f1f,0xc700c7c7
.long	0x00f8f8f8,0x3e003e3e
.long	0x00d7d7d7,0xf500f5f5
.long	0x003e3e3e,0x8f008f8f
.long	0x009d9d9d,0x67006767
.long	0x007c7c7c,0x1f001f1f
.long	0x00606060,0x18001818
.long	0x00b9b9b9,0x6e006e6e
.long	0x00bebebe,0xaf00afaf
.long	0x00bcbcbc,0x2f002f2f
.long	0x008b8b8b,0xe200e2e2
.long	0x00161616,0x85008585
.long	0x00343434,0x0d000d0d
.long	0x004d4d4d,0x53005353
.long	0x00c3c3c3,0xf000f0f0
.long	0x00727272,0x9c009c9c
.long	0x00959595,0x65006565
.long	0x00ababab,0xea00eaea
.long	0x008e8e8e,0xa300a3a3
.long	0x00bababa,0xae00aeae
.long	0x007a7a7a,0x9e009e9e
.long	0x00b3b3b3,0xec00ecec
.long	0x00020202,0x80008080
.long	0x00b4b4b4,0x2d002d2d
.long	0x00adadad,0x6b006b6b
.long	0x00a2a2a2,0xa800a8a8
.long	0x00acacac,0x2b002b2b
.long	0x00d8d8d8,0x36003636
.long	0x009a9a9a,0xa600a6a6
.long	0x00171717,0xc500c5c5
.long	0x001a1a1a,0x86008686
.long	0x00353535,0x4d004d4d
.long	0x00cccccc,0x33003333
.long	0x00f7f7f7,0xfd00fdfd
.long	0x00999999,0x66006666
.long	0x00616161,0x58005858
.long	0x005a5a5a,0x96009696
.long	0x00e8e8e8,0x3a003a3a
.long	0x00242424,0x09000909
.long	0x00565656,0x95009595
.long	0x00404040,0x10001010
.long	0x00e1e1e1,0x78007878
.long	0x00636363,0xd800d8d8
.long	0x00090909,0x42004242
.long	0x00333333,0xcc00cccc
.long	0x00bfbfbf,0xef00efef
.long	0x00989898,0x26002626
.long	0x00979797,0xe500e5e5
.long	0x00858585,0x61006161
.long	0x00686868,0x1a001a1a
.long	0x00fcfcfc,0x3f003f3f
.long	0x00ececec,0x3b003b3b
.long	0x000a0a0a,0x82008282
.long	0x00dadada,0xb600b6b6
.long	0x006f6f6f,0xdb00dbdb
.long	0x00535353,0xd400d4d4
.long	0x00626262,0x98009898
.long	0x00a3a3a3,0xe800e8e8
.long	0x002e2e2e,0x8b008b8b
.long	0x00080808,0x02000202
.long	0x00afafaf,0xeb00ebeb
.long	0x00282828,0x0a000a0a
.long	0x00b0b0b0,0x2c002c2c
.long	0x00747474,0x1d001d1d
.long	0x00c2c2c2,0xb000b0b0
.long	0x00bdbdbd,0x6f006f6f
.long	0x00363636,0x8d008d8d
.long	0x00222222,0x88008888
.long	0x00383838,0x0e000e0e
.long	0x00646464,0x19001919
.long	0x001e1e1e,0x87008787
.long	0x00393939,0x4e004e4e
.long	0x002c2c2c,0x0b000b0b
.long	0x00a6a6a6,0xa900a9a9
.long	0x00303030,0x0c000c0c
.long	0x00e5e5e5,0x79007979
.long	0x00444444,0x11001111
.long	0x00fdfdfd,0x7f007f7f
.long	0x00888888,0x22002222
.long	0x009f9f9f,0xe700e7e7
.long	0x00656565,0x59005959
.long	0x00878787,0xe100e1e1
.long	0x006b6b6b,0xda00dada
.long	0x00f4f4f4,0x3d003d3d
.long	0x00232323,0xc800c8c8
.long	0x00484848,0x12001212
.long	0x00101010,0x04000404
.long	0x00d1d1d1,0x74007474
.long	0x00515151,0x54005454
.long	0x00c0c0c0,0x30003030
.long	0x00f9f9f9,0x7e007e7e
.long	0x00d2d2d2,0xb400b4b4
.long	0x00a0a0a0,0x28002828
.long	0x00555555,0x55005555
.long	0x00a1a1a1,0x68006868
.long	0x00414141,0x50005050
.long	0x00fafafa,0xbe00bebe
.long	0x00434343,0xd000d0d0
.long	0x00131313,0xc400c4c4
.long	0x00c4c4c4,0x31003131
.long	0x002f2f2f,0xcb00cbcb
.long	0x00a8a8a8,0x2a002a2a
.long	0x00b6b6b6,0xad00adad
.long	0x003c3c3c,0x0f000f0f
.long	0x002b2b2b,0xca00caca
.long	0x00c1c1c1,0x70007070
.long	0x00ffffff,0xff00ffff
.long	0x00c8c8c8,0x32003232
.long	0x00a5a5a5,0x69006969
.long	0x00202020,0x08000808
.long	0x00898989,0x62006262
.long	0x00000000,0x00000000
.long	0x00909090,0x24002424
.long	0x00474747,0xd100d1d1
.long	0x00efefef,0xfb00fbfb
.long	0x00eaeaea,0xba00baba
.long	0x00b7b7b7,0xed00eded
.long	0x00151515,0x45004545
.long	0x00060606,0x81008181
.long	0x00cdcdcd,0x73007373
.long	0x00b5b5b5,0x6d006d6d
.long	0x00121212,0x84008484
.long	0x007e7e7e,0x9f009f9f
.long	0x00bbbbbb,0xee00eeee
.long	0x00292929,0x4a004a4a
.long	0x000f0f0f,0xc300c3c3
.long	0x00b8b8b8,0x2e002e2e
.long	0x00070707,0xc100c1c1
.long	0x00040404,0x01000101
.long	0x009b9b9b,0xe600e6e6
.long	0x00949494,0x25002525
.long	0x00212121,0x48004848
.long	0x00666666,0x99009999
.long	0x00e6e6e6,0xb900b9b9
.long	0x00cecece,0xb300b3b3
.long	0x00ededed,0x7b007b7b
.long	0x00e7e7e7,0xf900f9f9
.long	0x003b3b3b,0xce00cece
.long	0x00fefefe,0xbf00bfbf
.long	0x007f7f7f,0xdf00dfdf
.long	0x00c5c5c5,0x71007171
.long	0x00a4a4a4,0x29002929
.long	0x00373737,0xcd00cdcd
.long	0x00b1b1b1,0x6c006c6c
.long	0x004c4c4c,0x13001313
.long	0x00919191,0x64006464
.long	0x006e6e6e,0x9b009b9b
.long	0x008d8d8d,0x63006363
.long	0x00767676,0x9d009d9d
.long	0x00030303,0xc000c0c0
.long	0x002d2d2d,0x4b004b4b
.long	0x00dedede,0xb700b7b7
.long	0x00969696,0xa500a5a5
.long	0x00262626,0x89008989
.long	0x007d7d7d,0x5f005f5f
.long	0x00c6c6c6,0xb100b1b1
.long	0x005c5c5c,0x17001717
.long	0x00d3d3d3,0xf400f4f4
.long	0x00f2f2f2,0xbc00bcbc
.long	0x004f4f4f,0xd300d3d3
.long	0x00191919,0x46004646
.long	0x003f3f3f,0xcf00cfcf
.long	0x00dcdcdc,0x37003737
.long	0x00797979,0x5e005e5e
.long	0x001d1d1d,0x47004747
.long	0x00525252,0x94009494
.long	0x00ebebeb,0xfa00fafa
.long	0x00f3f3f3,0xfc00fcfc
.long	0x006d6d6d,0x5b005b5b
.long	0x005e5e5e,0x97009797
.long	0x00fbfbfb,0xfe00fefe
.long	0x00696969,0x5a005a5a
.long	0x00b2b2b2,0xac00acac
.long	0x00f0f0f0,0x3c003c3c
.long	0x00313131,0x4c004c4c
.long	0x000c0c0c,0x03000303
.long	0x00d4d4d4,0x35003535
.long	0x00cfcfcf,0xf300f3f3
.long	0x008c8c8c,0x23002323
.long	0x00e2e2e2,0xb800b8b8
.long	0x00757575,0x5d005d5d
.long	0x00a9a9a9,0x6a006a6a
.long	0x004a4a4a,0x92009292
.long	0x00575757,0xd500d5d5
.long	0x00848484,0x21002121
.long	0x00111111,0x44004444
.long	0x00454545,0x51005151
.long	0x001b1b1b,0xc600c6c6
.long	0x00f5f5f5,0x7d007d7d
.long	0x00e4e4e4,0x39003939
.long	0x000e0e0e,0x83008383
.long	0x00737373,0xdc00dcdc
.long	0x00aaaaaa,0xaa00aaaa
.long	0x00f1f1f1,0x7c007c7c
.long	0x00dddddd,0x77007777
.long	0x00595959,0x56005656
.long	0x00141414,0x05000505
.long	0x006c6c6c,0x1b001b1b
.long	0x00929292,0xa400a4a4
.long	0x00545454,0x15001515
.long	0x00d0d0d0,0x34003434
.long	0x00787878,0x1e001e1e
.long	0x00707070,0x1c001c1c
.long	0x00e3e3e3,0xf800f8f8
.long	0x00494949,0x52005252
.long	0x00808080,0x20002020
.long	0x00505050,0x14001414
.long	0x00a7a7a7,0xe900e9e9
.long	0x00f6f6f6,0xbd00bdbd
.long	0x00777777,0xdd00dddd
.long	0x00939393,0xe400e4e4
.long	0x00868686,0xa100a1a1
.long	0x00838383,0xe000e0e0
.long	0x002a2a2a,0x8a008a8a
.long	0x00c7c7c7,0xf100f1f1
.long	0x005b5b5b,0xd600d6d6
.long	0x00e9e9e9,0x7a007a7a
.long	0x00eeeeee,0xbb00bbbb
.long	0x008f8f8f,0xe300e3e3
.long	0x00010101,0x40004040
.long	0x003d3d3d,0x4f004f4f
.globl	_Camellia_cbc_encrypt

.p2align	4
_Camellia_cbc_encrypt:

.byte	243,15,30,250
	cmpq	$0,%rdx
	je	L$cbc_abort
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$cbc_prologue:

	movq	%rsp,%rbp

	subq	$64,%rsp
	andq	$-64,%rsp



	leaq	-64-63(%rcx),%r10
	subq	%rsp,%r10
	negq	%r10
	andq	$0x3C0,%r10
	subq	%r10,%rsp


	movq	%rdi,%r12
	movq	%rsi,%r13
	movq	%r8,%rbx
	movq	%rcx,%r14
	movl	272(%rcx),%r15d

	movq	%r8,40(%rsp)
	movq	%rbp,48(%rsp)


L$cbc_body:
	leaq	L$Camellia_SBOX(%rip),%rbp

	movl	$32,%ecx
.p2align	2
L$cbc_prefetch_sbox:
	movq	0(%rbp),%rax
	movq	32(%rbp),%rsi
	movq	64(%rbp),%rdi
	movq	96(%rbp),%r11
	leaq	128(%rbp),%rbp
	loop	L$cbc_prefetch_sbox
	subq	$4096,%rbp
	shlq	$6,%r15
	movq	%rdx,%rcx
	leaq	(%r14,%r15,1),%r15

	cmpl	$0,%r9d
	je	L$CBC_DECRYPT

	andq	$-16,%rdx
	andq	$15,%rcx
	leaq	(%r12,%rdx,1),%rdx
	movq	%r14,0(%rsp)
	movq	%rdx,8(%rsp)
	movq	%rcx,16(%rsp)

	cmpq	%r12,%rdx
	movl	0(%rbx),%r8d
	movl	4(%rbx),%r9d
	movl	8(%rbx),%r10d
	movl	12(%rbx),%r11d
	je	L$cbc_enc_tail
	jmp	L$cbc_eloop

.p2align	4
L$cbc_eloop:
	xorl	0(%r12),%r8d
	xorl	4(%r12),%r9d
	xorl	8(%r12),%r10d
	bswapl	%r8d
	xorl	12(%r12),%r11d
	bswapl	%r9d
	bswapl	%r10d
	bswapl	%r11d

	call	_x86_64_Camellia_encrypt

	movq	0(%rsp),%r14
	bswapl	%r8d
	movq	8(%rsp),%rdx
	bswapl	%r9d
	movq	16(%rsp),%rcx
	bswapl	%r10d
	movl	%r8d,0(%r13)
	bswapl	%r11d
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	leaq	16(%r12),%r12
	movl	%r11d,12(%r13)
	cmpq	%rdx,%r12
	leaq	16(%r13),%r13
	jne	L$cbc_eloop

	cmpq	$0,%rcx
	jne	L$cbc_enc_tail

	movq	40(%rsp),%r13
	movl	%r8d,0(%r13)
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	movl	%r11d,12(%r13)
	jmp	L$cbc_done

.p2align	4
L$cbc_enc_tail:
	xorq	%rax,%rax
	movq	%rax,0+24(%rsp)
	movq	%rax,8+24(%rsp)
	movq	%rax,16(%rsp)

L$cbc_enc_pushf:
	pushfq
	cld
	movq	%r12,%rsi
	leaq	8+24(%rsp),%rdi
.long	0x9066A4F3
	popfq
L$cbc_enc_popf:

	leaq	24(%rsp),%r12
	leaq	16+24(%rsp),%rax
	movq	%rax,8(%rsp)
	jmp	L$cbc_eloop

.p2align	4
L$CBC_DECRYPT:
	xchgq	%r14,%r15
	addq	$15,%rdx
	andq	$15,%rcx
	andq	$-16,%rdx
	movq	%r14,0(%rsp)
	leaq	(%r12,%rdx,1),%rdx
	movq	%rdx,8(%rsp)
	movq	%rcx,16(%rsp)

	movq	(%rbx),%rax
	movq	8(%rbx),%rbx
	jmp	L$cbc_dloop
.p2align	4
L$cbc_dloop:
	movl	0(%r12),%r8d
	movl	4(%r12),%r9d
	movl	8(%r12),%r10d
	bswapl	%r8d
	movl	12(%r12),%r11d
	bswapl	%r9d
	movq	%rax,0+24(%rsp)
	bswapl	%r10d
	movq	%rbx,8+24(%rsp)
	bswapl	%r11d

	call	_x86_64_Camellia_decrypt

	movq	0(%rsp),%r14
	movq	8(%rsp),%rdx
	movq	16(%rsp),%rcx

	bswapl	%r8d
	movq	(%r12),%rax
	bswapl	%r9d
	movq	8(%r12),%rbx
	bswapl	%r10d
	xorl	0+24(%rsp),%r8d
	bswapl	%r11d
	xorl	4+24(%rsp),%r9d
	xorl	8+24(%rsp),%r10d
	leaq	16(%r12),%r12
	xorl	12+24(%rsp),%r11d
	cmpq	%rdx,%r12
	je	L$cbc_ddone

	movl	%r8d,0(%r13)
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	movl	%r11d,12(%r13)

	leaq	16(%r13),%r13
	jmp	L$cbc_dloop

.p2align	4
L$cbc_ddone:
	movq	40(%rsp),%rdx
	cmpq	$0,%rcx
	jne	L$cbc_dec_tail

	movl	%r8d,0(%r13)
	movl	%r9d,4(%r13)
	movl	%r10d,8(%r13)
	movl	%r11d,12(%r13)

	movq	%rax,(%rdx)
	movq	%rbx,8(%rdx)
	jmp	L$cbc_done
.p2align	4
L$cbc_dec_tail:
	movl	%r8d,0+24(%rsp)
	movl	%r9d,4+24(%rsp)
	movl	%r10d,8+24(%rsp)
	movl	%r11d,12+24(%rsp)

L$cbc_dec_pushf:
	pushfq
	cld
	leaq	8+24(%rsp),%rsi
	leaq	(%r13),%rdi
.long	0x9066A4F3
	popfq
L$cbc_dec_popf:

	movq	%rax,(%rdx)
	movq	%rbx,8(%rdx)
	jmp	L$cbc_done

.p2align	4
L$cbc_done:
	movq	48(%rsp),%rcx

	movq	0(%rcx),%r15

	movq	8(%rcx),%r14

	movq	16(%rcx),%r13

	movq	24(%rcx),%r12

	movq	32(%rcx),%rbp

	movq	40(%rcx),%rbx

	leaq	48(%rcx),%rsp

L$cbc_abort:
	.byte	0xf3,0xc3



.byte	67,97,109,101,108,108,105,97,32,102,111,114,32,120,56,54,95,54,52,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
