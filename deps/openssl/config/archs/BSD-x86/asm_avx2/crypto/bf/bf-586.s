.text
.globl	_BF_encrypt
.type	_BF_encrypt,@function
.align	4
_BF_encrypt:
L_BF_encrypt_begin:

	pushl	%ebp
	pushl	%ebx
	movl	12(%esp),%ebx
	movl	16(%esp),%ebp
	pushl	%esi
	pushl	%edi
	# Load the 2 words
	movl	(%ebx),%edi
	movl	4(%ebx),%esi
	xorl	%eax,%eax
	movl	(%ebp),%ebx
	xorl	%ecx,%ecx
	xorl	%ebx,%edi

	# Round 0
	movl	4(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 1
	movl	8(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 2
	movl	12(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 3
	movl	16(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 4
	movl	20(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 5
	movl	24(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 6
	movl	28(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 7
	movl	32(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 8
	movl	36(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 9
	movl	40(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 10
	movl	44(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 11
	movl	48(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 12
	movl	52(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 13
	movl	56(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 14
	movl	60(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 15
	movl	64(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	# Load parameter 0 (16) enc=1
	movl	20(%esp),%eax
	xorl	%ebx,%edi
	movl	68(%ebp),%edx
	xorl	%edx,%esi
	movl	%edi,4(%eax)
	movl	%esi,(%eax)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_BF_decrypt
.type	_BF_decrypt,@function
.align	4
_BF_decrypt:
L_BF_decrypt_begin:

	pushl	%ebp
	pushl	%ebx
	movl	12(%esp),%ebx
	movl	16(%esp),%ebp
	pushl	%esi
	pushl	%edi
	# Load the 2 words
	movl	(%ebx),%edi
	movl	4(%ebx),%esi
	xorl	%eax,%eax
	movl	68(%ebp),%ebx
	xorl	%ecx,%ecx
	xorl	%ebx,%edi

	# Round 16
	movl	64(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 15
	movl	60(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 14
	movl	56(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 13
	movl	52(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 12
	movl	48(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 11
	movl	44(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 10
	movl	40(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 9
	movl	36(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 8
	movl	32(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 7
	movl	28(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 6
	movl	24(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 5
	movl	20(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 4
	movl	16(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 3
	movl	12(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%edi

	# Round 2
	movl	8(%ebp),%edx
	movl	%edi,%ebx
	xorl	%edx,%esi
	shrl	$16,%ebx
	movl	%edi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	xorl	%eax,%eax
	xorl	%ebx,%esi

	# Round 1
	movl	4(%ebp),%edx
	movl	%esi,%ebx
	xorl	%edx,%edi
	shrl	$16,%ebx
	movl	%esi,%edx
	movb	%bh,%al
	andl	$255,%ebx
	movb	%dh,%cl
	andl	$255,%edx
	movl	72(%ebp,%eax,4),%eax
	movl	1096(%ebp,%ebx,4),%ebx
	addl	%eax,%ebx
	movl	2120(%ebp,%ecx,4),%eax
	xorl	%eax,%ebx
	movl	3144(%ebp,%edx,4),%edx
	addl	%edx,%ebx
	# Load parameter 0 (1) enc=0
	movl	20(%esp),%eax
	xorl	%ebx,%edi
	movl	(%ebp),%edx
	xorl	%edx,%esi
	movl	%edi,4(%eax)
	movl	%esi,(%eax)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_BF_cbc_encrypt
.type	_BF_cbc_encrypt,@function
.align	4
_BF_cbc_encrypt:
L_BF_cbc_encrypt_begin:

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),%ebp
	# getting iv ptr from parameter 4
	movl	36(%esp),%ebx
	movl	(%ebx),%esi
	movl	4(%ebx),%edi
	pushl	%edi
	pushl	%esi
	pushl	%edi
	pushl	%esi
	movl	%esp,%ebx
	movl	36(%esp),%esi
	movl	40(%esp),%edi
	# getting encrypt flag from parameter 5
	movl	56(%esp),%ecx
	# get and push parameter 3
	movl	48(%esp),%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,%ecx
	jz	L000decrypt
	andl	$4294967288,%ebp
	movl	8(%esp),%eax
	movl	12(%esp),%ebx
	jz	L001encrypt_finish
L002encrypt_loop:
	movl	(%esi),%ecx
	movl	4(%esi),%edx
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,8(%esp)
	movl	%ebx,12(%esp)
	call	L_BF_encrypt_begin
	movl	8(%esp),%eax
	movl	12(%esp),%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	L002encrypt_loop
L001encrypt_finish:
	movl	52(%esp),%ebp
	andl	$7,%ebp
	jz	L003finish
	call	L004PIC_point
L004PIC_point:
	popl	%edx
	leal	L005cbc_enc_jmp_table-L004PIC_point(%edx),%ecx
	movl	(%ecx,%ebp,4),%ebp
	addl	%edx,%ebp
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	jmp	*%ebp
L006ej7:
	movb	6(%esi),%dh
	shll	$8,%edx
L007ej6:
	movb	5(%esi),%dh
L008ej5:
	movb	4(%esi),%dl
L009ej4:
	movl	(%esi),%ecx
	jmp	L010ejend
L011ej3:
	movb	2(%esi),%ch
	shll	$8,%ecx
L012ej2:
	movb	1(%esi),%ch
L013ej1:
	movb	(%esi),%cl
L010ejend:
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,8(%esp)
	movl	%ebx,12(%esp)
	call	L_BF_encrypt_begin
	movl	8(%esp),%eax
	movl	12(%esp),%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	jmp	L003finish
L000decrypt:
	andl	$4294967288,%ebp
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	jz	L014decrypt_finish
L015decrypt_loop:
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,8(%esp)
	movl	%ebx,12(%esp)
	call	L_BF_decrypt_begin
	movl	8(%esp),%eax
	movl	12(%esp),%ebx
	bswap	%eax
	bswap	%ebx
	movl	16(%esp),%ecx
	movl	20(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%ecx,(%edi)
	movl	%edx,4(%edi)
	movl	%eax,16(%esp)
	movl	%ebx,20(%esp)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	L015decrypt_loop
L014decrypt_finish:
	movl	52(%esp),%ebp
	andl	$7,%ebp
	jz	L003finish
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	bswap	%eax
	bswap	%ebx
	movl	%eax,8(%esp)
	movl	%ebx,12(%esp)
	call	L_BF_decrypt_begin
	movl	8(%esp),%eax
	movl	12(%esp),%ebx
	bswap	%eax
	bswap	%ebx
	movl	16(%esp),%ecx
	movl	20(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
L016dj7:
	rorl	$16,%edx
	movb	%dl,6(%edi)
	shrl	$16,%edx
L017dj6:
	movb	%dh,5(%edi)
L018dj5:
	movb	%dl,4(%edi)
L019dj4:
	movl	%ecx,(%edi)
	jmp	L020djend
L021dj3:
	rorl	$16,%ecx
	movb	%cl,2(%edi)
	shll	$16,%ecx
L022dj2:
	movb	%ch,1(%esi)
L023dj1:
	movb	%cl,(%esi)
L020djend:
	jmp	L003finish
L003finish:
	movl	60(%esp),%ecx
	addl	$24,%esp
	movl	%eax,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.align	6,0x90
L005cbc_enc_jmp_table:
.long	0
.long	L013ej1-L004PIC_point
.long	L012ej2-L004PIC_point
.long	L011ej3-L004PIC_point
.long	L009ej4-L004PIC_point
.long	L008ej5-L004PIC_point
.long	L007ej6-L004PIC_point
.long	L006ej7-L004PIC_point
.align	6,0x90
