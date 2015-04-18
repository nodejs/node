.file	"../openssl/crypto/bn/asm/x86.s"
.text
.globl	_bn_mul_add_words
.align	4
_bn_mul_add_words:
L_bn_mul_add_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	xorl	%esi,%esi
	movl	20(%esp),%edi
	movl	28(%esp),%ecx
	movl	24(%esp),%ebx
	andl	$4294967288,%ecx
	movl	32(%esp),%ebp
	pushl	%ecx
	jz	L000maw_finish
L001maw_loop:
	movl	%ecx,(%esp)
	# Round 0

	movl	(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi
	# Round 4

	movl	4(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	4(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi
	# Round 8

	movl	8(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	8(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi
	# Round 12

	movl	12(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	12(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi
	# Round 16

	movl	16(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	16(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi
	# Round 20

	movl	20(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	20(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi
	# Round 24

	movl	24(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	24(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
	# Round 28

	movl	28(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	28(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,28(%edi)
	movl	%edx,%esi

	movl	(%esp),%ecx
	addl	$32,%ebx
	addl	$32,%edi
	subl	$8,%ecx
	jnz	L001maw_loop
L000maw_finish:
	movl	32(%esp),%ecx
	andl	$7,%ecx
	jnz	L002maw_finish2
	jmp	L003maw_end
L002maw_finish2:
	# Tail Round 0

	movl	(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 1

	movl	4(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	4(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,4(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 2

	movl	8(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	8(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,8(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 3

	movl	12(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	12(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,12(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 4

	movl	16(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	16(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,16(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 5

	movl	20(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	20(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	decl	%ecx
	movl	%eax,20(%edi)
	movl	%edx,%esi
	jz	L003maw_end
	# Tail Round 6

	movl	24(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	24(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
L003maw_end:
	movl	%esi,%eax
	popl	%ecx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_mul_words
.align	4
_bn_mul_words:
L_bn_mul_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	xorl	%esi,%esi
	movl	20(%esp),%edi
	movl	24(%esp),%ebx
	movl	28(%esp),%ebp
	movl	32(%esp),%ecx
	andl	$4294967288,%ebp
	jz	L004mw_finish
L005mw_loop:
	# Round 0

	movl	(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi
	# Round 4

	movl	4(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi
	# Round 8

	movl	8(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi
	# Round 12

	movl	12(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi
	# Round 16

	movl	16(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi
	# Round 20

	movl	20(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi
	# Round 24

	movl	24(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
	# Round 28

	movl	28(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,28(%edi)
	movl	%edx,%esi

	addl	$32,%ebx
	addl	$32,%edi
	subl	$8,%ebp
	jz	L004mw_finish
	jmp	L005mw_loop
L004mw_finish:
	movl	28(%esp),%ebp
	andl	$7,%ebp
	jnz	L006mw_finish2
	jmp	L007mw_end
L006mw_finish2:
	# Tail Round 0

	movl	(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 1

	movl	4(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 2

	movl	8(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 3

	movl	12(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 4

	movl	16(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 5

	movl	20(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	L007mw_end
	# Tail Round 6

	movl	24(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
L007mw_end:
	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_sqr_words
.align	4
_bn_sqr_words:
L_bn_sqr_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%esi
	movl	24(%esp),%edi
	movl	28(%esp),%ebx
	andl	$4294967288,%ebx
	jz	L008sw_finish
L009sw_loop:
	# Round 0

	movl	(%edi),%eax
	mull	%eax
	movl	%eax,(%esi)
	movl	%edx,4(%esi)
	# Round 4

	movl	4(%edi),%eax
	mull	%eax
	movl	%eax,8(%esi)
	movl	%edx,12(%esi)
	# Round 8

	movl	8(%edi),%eax
	mull	%eax
	movl	%eax,16(%esi)
	movl	%edx,20(%esi)
	# Round 12

	movl	12(%edi),%eax
	mull	%eax
	movl	%eax,24(%esi)
	movl	%edx,28(%esi)
	# Round 16

	movl	16(%edi),%eax
	mull	%eax
	movl	%eax,32(%esi)
	movl	%edx,36(%esi)
	# Round 20

	movl	20(%edi),%eax
	mull	%eax
	movl	%eax,40(%esi)
	movl	%edx,44(%esi)
	# Round 24

	movl	24(%edi),%eax
	mull	%eax
	movl	%eax,48(%esi)
	movl	%edx,52(%esi)
	# Round 28

	movl	28(%edi),%eax
	mull	%eax
	movl	%eax,56(%esi)
	movl	%edx,60(%esi)

	addl	$32,%edi
	addl	$64,%esi
	subl	$8,%ebx
	jnz	L009sw_loop
L008sw_finish:
	movl	28(%esp),%ebx
	andl	$7,%ebx
	jz	L010sw_end
	# Tail Round 0

	movl	(%edi),%eax
	mull	%eax
	movl	%eax,(%esi)
	decl	%ebx
	movl	%edx,4(%esi)
	jz	L010sw_end
	# Tail Round 1

	movl	4(%edi),%eax
	mull	%eax
	movl	%eax,8(%esi)
	decl	%ebx
	movl	%edx,12(%esi)
	jz	L010sw_end
	# Tail Round 2

	movl	8(%edi),%eax
	mull	%eax
	movl	%eax,16(%esi)
	decl	%ebx
	movl	%edx,20(%esi)
	jz	L010sw_end
	# Tail Round 3

	movl	12(%edi),%eax
	mull	%eax
	movl	%eax,24(%esi)
	decl	%ebx
	movl	%edx,28(%esi)
	jz	L010sw_end
	# Tail Round 4

	movl	16(%edi),%eax
	mull	%eax
	movl	%eax,32(%esi)
	decl	%ebx
	movl	%edx,36(%esi)
	jz	L010sw_end
	# Tail Round 5

	movl	20(%edi),%eax
	mull	%eax
	movl	%eax,40(%esi)
	decl	%ebx
	movl	%edx,44(%esi)
	jz	L010sw_end
	# Tail Round 6

	movl	24(%edi),%eax
	mull	%eax
	movl	%eax,48(%esi)
	movl	%edx,52(%esi)
L010sw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_div_words
.align	4
_bn_div_words:
L_bn_div_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%edx
	movl	24(%esp),%eax
	movl	28(%esp),%ebx
	divl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_add_words
.align	4
_bn_add_words:
L_bn_add_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%ebx
	movl	24(%esp),%esi
	movl	28(%esp),%edi
	movl	32(%esp),%ebp
	xorl	%eax,%eax
	andl	$4294967288,%ebp
	jz	L011aw_finish
L012aw_loop:
	# Round 0

	movl	(%esi),%ecx
	movl	(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,(%ebx)
	# Round 1

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,4(%ebx)
	# Round 2

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,8(%ebx)
	# Round 3

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,12(%ebx)
	# Round 4

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,16(%ebx)
	# Round 5

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,20(%ebx)
	# Round 6

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
	# Round 7

	movl	28(%esi),%ecx
	movl	28(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,28(%ebx)

	addl	$32,%esi
	addl	$32,%edi
	addl	$32,%ebx
	subl	$8,%ebp
	jnz	L012aw_loop
L011aw_finish:
	movl	32(%esp),%ebp
	andl	$7,%ebp
	jz	L013aw_end
	# Tail Round 0

	movl	(%esi),%ecx
	movl	(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,(%ebx)
	jz	L013aw_end
	# Tail Round 1

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,4(%ebx)
	jz	L013aw_end
	# Tail Round 2

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,8(%ebx)
	jz	L013aw_end
	# Tail Round 3

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,12(%ebx)
	jz	L013aw_end
	# Tail Round 4

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,16(%ebx)
	jz	L013aw_end
	# Tail Round 5

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,20(%ebx)
	jz	L013aw_end
	# Tail Round 6

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
L013aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_sub_words
.align	4
_bn_sub_words:
L_bn_sub_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%ebx
	movl	24(%esp),%esi
	movl	28(%esp),%edi
	movl	32(%esp),%ebp
	xorl	%eax,%eax
	andl	$4294967288,%ebp
	jz	L014aw_finish
L015aw_loop:
	# Round 0

	movl	(%esi),%ecx
	movl	(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,(%ebx)
	# Round 1

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,4(%ebx)
	# Round 2

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,8(%ebx)
	# Round 3

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,12(%ebx)
	# Round 4

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,16(%ebx)
	# Round 5

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,20(%ebx)
	# Round 6

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
	# Round 7

	movl	28(%esi),%ecx
	movl	28(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,28(%ebx)

	addl	$32,%esi
	addl	$32,%edi
	addl	$32,%ebx
	subl	$8,%ebp
	jnz	L015aw_loop
L014aw_finish:
	movl	32(%esp),%ebp
	andl	$7,%ebp
	jz	L016aw_end
	# Tail Round 0

	movl	(%esi),%ecx
	movl	(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,(%ebx)
	jz	L016aw_end
	# Tail Round 1

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,4(%ebx)
	jz	L016aw_end
	# Tail Round 2

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,8(%ebx)
	jz	L016aw_end
	# Tail Round 3

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,12(%ebx)
	jz	L016aw_end
	# Tail Round 4

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,16(%ebx)
	jz	L016aw_end
	# Tail Round 5

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,20(%ebx)
	jz	L016aw_end
	# Tail Round 6

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
L016aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_bn_mul_comba8
.align	4
_bn_mul_comba8:
L_bn_mul_comba8_begin:
	pushl	%esi
	movl	12(%esp),%esi
	pushl	%edi
	movl	20(%esp),%edi
	pushl	%ebp
	pushl	%ebx
	xorl	%ebx,%ebx
	movl	(%esi),%eax
	xorl	%ecx,%ecx
	movl	(%edi),%edx
	# ################## Calculate word 0

	xorl	%ebp,%ebp
	# mul a[0]*b[0]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%eax)
	movl	4(%esi),%eax
	# saved r[0]

	# ################## Calculate word 1

	xorl	%ebx,%ebx
	# mul a[1]*b[0]

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	# mul a[0]*b[1]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,4(%eax)
	movl	8(%esi),%eax
	# saved r[1]

	# ################## Calculate word 2

	xorl	%ecx,%ecx
	# mul a[2]*b[0]

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx
	# mul a[1]*b[1]

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	# mul a[0]*b[2]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%eax)
	movl	12(%esi),%eax
	# saved r[2]

	# ################## Calculate word 3

	xorl	%ebp,%ebp
	# mul a[3]*b[0]

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp
	# mul a[2]*b[1]

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp
	# mul a[1]*b[2]

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	# mul a[0]*b[3]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,12(%eax)
	movl	16(%esi),%eax
	# saved r[3]

	# ################## Calculate word 4

	xorl	%ebx,%ebx
	# mul a[4]*b[0]

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	# mul a[3]*b[1]

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx
	# mul a[2]*b[2]

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx
	# mul a[1]*b[3]

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx
	# mul a[0]*b[4]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%eax)
	movl	20(%esi),%eax
	# saved r[4]

	# ################## Calculate word 5

	xorl	%ecx,%ecx
	# mul a[5]*b[0]

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx
	# mul a[4]*b[1]

	mull	%edx
	addl	%eax,%ebp
	movl	12(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	# mul a[3]*b[2]

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx
	# mul a[2]*b[3]

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	16(%edi),%edx
	adcl	$0,%ecx
	# mul a[1]*b[4]

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx
	# mul a[0]*b[5]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,20(%eax)
	movl	24(%esi),%eax
	# saved r[5]

	# ################## Calculate word 6

	xorl	%ebp,%ebp
	# mul a[6]*b[0]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp
	# mul a[5]*b[1]

	mull	%edx
	addl	%eax,%ebx
	movl	16(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp
	# mul a[4]*b[2]

	mull	%edx
	addl	%eax,%ebx
	movl	12(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	# mul a[3]*b[3]

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	16(%edi),%edx
	adcl	$0,%ebp
	# mul a[2]*b[4]

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	20(%edi),%edx
	adcl	$0,%ebp
	# mul a[1]*b[5]

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp
	# mul a[0]*b[6]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,24(%eax)
	movl	28(%esi),%eax
	# saved r[6]

	# ################## Calculate word 7

	xorl	%ebx,%ebx
	# mul a[7]*b[0]

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	# mul a[6]*b[1]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx
	# mul a[5]*b[2]

	mull	%edx
	addl	%eax,%ecx
	movl	16(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx
	# mul a[4]*b[3]

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx
	# mul a[3]*b[4]

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	20(%edi),%edx
	adcl	$0,%ebx
	# mul a[2]*b[5]

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	24(%edi),%edx
	adcl	$0,%ebx
	# mul a[1]*b[6]

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx
	# mul a[0]*b[7]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,28(%eax)
	movl	28(%esi),%eax
	# saved r[7]

	# ################## Calculate word 8

	xorl	%ecx,%ecx
	# mul a[7]*b[1]

	mull	%edx
	addl	%eax,%ebp
	movl	24(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	# mul a[6]*b[2]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx
	# mul a[5]*b[3]

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	16(%edi),%edx
	adcl	$0,%ecx
	# mul a[4]*b[4]

	mull	%edx
	addl	%eax,%ebp
	movl	12(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx
	# mul a[3]*b[5]

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	24(%edi),%edx
	adcl	$0,%ecx
	# mul a[2]*b[6]

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	28(%edi),%edx
	adcl	$0,%ecx
	# mul a[1]*b[7]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,32(%eax)
	movl	28(%esi),%eax
	# saved r[8]

	# ################## Calculate word 9

	xorl	%ebp,%ebp
	# mul a[7]*b[2]

	mull	%edx
	addl	%eax,%ebx
	movl	24(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	# mul a[6]*b[3]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	16(%edi),%edx
	adcl	$0,%ebp
	# mul a[5]*b[4]

	mull	%edx
	addl	%eax,%ebx
	movl	16(%esi),%eax
	adcl	%edx,%ecx
	movl	20(%edi),%edx
	adcl	$0,%ebp
	# mul a[4]*b[5]

	mull	%edx
	addl	%eax,%ebx
	movl	12(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp
	# mul a[3]*b[6]

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	28(%edi),%edx
	adcl	$0,%ebp
	# mul a[2]*b[7]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,36(%eax)
	movl	28(%esi),%eax
	# saved r[9]

	# ################## Calculate word 10

	xorl	%ebx,%ebx
	# mul a[7]*b[3]

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx
	# mul a[6]*b[4]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esi),%eax
	adcl	%edx,%ebp
	movl	20(%edi),%edx
	adcl	$0,%ebx
	# mul a[5]*b[5]

	mull	%edx
	addl	%eax,%ecx
	movl	16(%esi),%eax
	adcl	%edx,%ebp
	movl	24(%edi),%edx
	adcl	$0,%ebx
	# mul a[4]*b[6]

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx
	# mul a[3]*b[7]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,40(%eax)
	movl	28(%esi),%eax
	# saved r[10]

	# ################## Calculate word 11

	xorl	%ecx,%ecx
	# mul a[7]*b[4]

	mull	%edx
	addl	%eax,%ebp
	movl	24(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx
	# mul a[6]*b[5]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esi),%eax
	adcl	%edx,%ebx
	movl	24(%edi),%edx
	adcl	$0,%ecx
	# mul a[5]*b[6]

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	28(%edi),%edx
	adcl	$0,%ecx
	# mul a[4]*b[7]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,44(%eax)
	movl	28(%esi),%eax
	# saved r[11]

	# ################## Calculate word 12

	xorl	%ebp,%ebp
	# mul a[7]*b[5]

	mull	%edx
	addl	%eax,%ebx
	movl	24(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp
	# mul a[6]*b[6]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	28(%edi),%edx
	adcl	$0,%ebp
	# mul a[5]*b[7]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,48(%eax)
	movl	28(%esi),%eax
	# saved r[12]

	# ################## Calculate word 13

	xorl	%ebx,%ebx
	# mul a[7]*b[6]

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx
	# mul a[6]*b[7]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,52(%eax)
	movl	28(%esi),%eax
	# saved r[13]

	# ################## Calculate word 14

	xorl	%ecx,%ecx
	# mul a[7]*b[7]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	adcl	$0,%ecx
	movl	%ebp,56(%eax)
	# saved r[14]

	# save r[15]

	movl	%ebx,60(%eax)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.globl	_bn_mul_comba4
.align	4
_bn_mul_comba4:
L_bn_mul_comba4_begin:
	pushl	%esi
	movl	12(%esp),%esi
	pushl	%edi
	movl	20(%esp),%edi
	pushl	%ebp
	pushl	%ebx
	xorl	%ebx,%ebx
	movl	(%esi),%eax
	xorl	%ecx,%ecx
	movl	(%edi),%edx
	# ################## Calculate word 0

	xorl	%ebp,%ebp
	# mul a[0]*b[0]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%eax)
	movl	4(%esi),%eax
	# saved r[0]

	# ################## Calculate word 1

	xorl	%ebx,%ebx
	# mul a[1]*b[0]

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	# mul a[0]*b[1]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,4(%eax)
	movl	8(%esi),%eax
	# saved r[1]

	# ################## Calculate word 2

	xorl	%ecx,%ecx
	# mul a[2]*b[0]

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx
	# mul a[1]*b[1]

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	# mul a[0]*b[2]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%eax)
	movl	12(%esi),%eax
	# saved r[2]

	# ################## Calculate word 3

	xorl	%ebp,%ebp
	# mul a[3]*b[0]

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp
	# mul a[2]*b[1]

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp
	# mul a[1]*b[2]

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	# mul a[0]*b[3]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,12(%eax)
	movl	12(%esi),%eax
	# saved r[3]

	# ################## Calculate word 4

	xorl	%ebx,%ebx
	# mul a[3]*b[1]

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx
	# mul a[2]*b[2]

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx
	# mul a[1]*b[3]

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%eax)
	movl	12(%esi),%eax
	# saved r[4]

	# ################## Calculate word 5

	xorl	%ecx,%ecx
	# mul a[3]*b[2]

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx
	# mul a[2]*b[3]

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,20(%eax)
	movl	12(%esi),%eax
	# saved r[5]

	# ################## Calculate word 6

	xorl	%ebp,%ebp
	# mul a[3]*b[3]

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	adcl	$0,%ebp
	movl	%ebx,24(%eax)
	# saved r[6]

	# save r[7]

	movl	%ecx,28(%eax)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.globl	_bn_sqr_comba8
.align	4
_bn_sqr_comba8:
L_bn_sqr_comba8_begin:
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	pushl	%ebx
	movl	20(%esp),%edi
	movl	24(%esp),%esi
	xorl	%ebx,%ebx
	xorl	%ecx,%ecx
	movl	(%esi),%eax
	# ############### Calculate word 0

	xorl	%ebp,%ebp
	# sqr a[0]*a[0]

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%edi)
	movl	4(%esi),%eax
	# saved r[0]

	# ############### Calculate word 1

	xorl	%ebx,%ebx
	# sqr a[1]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx
	movl	%ecx,4(%edi)
	movl	(%esi),%edx
	# saved r[1]

	# ############### Calculate word 2

	xorl	%ecx,%ecx
	# sqr a[2]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	4(%esi),%eax
	adcl	$0,%ecx
	# sqr a[1]*a[1]

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%edi)
	movl	12(%esi),%eax
	# saved r[2]

	# ############### Calculate word 3

	xorl	%ebp,%ebp
	# sqr a[3]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	8(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx
	# sqr a[2]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	16(%esi),%eax
	adcl	$0,%ebp
	movl	%ebx,12(%edi)
	movl	(%esi),%edx
	# saved r[3]

	# ############### Calculate word 4

	xorl	%ebx,%ebx
	# sqr a[4]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	12(%esi),%eax
	adcl	$0,%ebx
	movl	4(%esi),%edx
	# sqr a[3]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx
	# sqr a[2]*a[2]

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%edi)
	movl	20(%esi),%eax
	# saved r[4]

	# ############### Calculate word 5

	xorl	%ecx,%ecx
	# sqr a[5]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	16(%esi),%eax
	adcl	$0,%ecx
	movl	4(%esi),%edx
	# sqr a[4]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	12(%esi),%eax
	adcl	$0,%ecx
	movl	8(%esi),%edx
	# sqr a[3]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	24(%esi),%eax
	adcl	$0,%ecx
	movl	%ebp,20(%edi)
	movl	(%esi),%edx
	# saved r[5]

	# ############### Calculate word 6

	xorl	%ebp,%ebp
	# sqr a[6]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	20(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx
	# sqr a[5]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	16(%esi),%eax
	adcl	$0,%ebp
	movl	8(%esi),%edx
	# sqr a[4]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	12(%esi),%eax
	adcl	$0,%ebp
	# sqr a[3]*a[3]

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,24(%edi)
	movl	28(%esi),%eax
	# saved r[6]

	# ############### Calculate word 7

	xorl	%ebx,%ebx
	# sqr a[7]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	24(%esi),%eax
	adcl	$0,%ebx
	movl	4(%esi),%edx
	# sqr a[6]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	20(%esi),%eax
	adcl	$0,%ebx
	movl	8(%esi),%edx
	# sqr a[5]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	16(%esi),%eax
	adcl	$0,%ebx
	movl	12(%esi),%edx
	# sqr a[4]*a[3]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	28(%esi),%eax
	adcl	$0,%ebx
	movl	%ecx,28(%edi)
	movl	4(%esi),%edx
	# saved r[7]

	# ############### Calculate word 8

	xorl	%ecx,%ecx
	# sqr a[7]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	24(%esi),%eax
	adcl	$0,%ecx
	movl	8(%esi),%edx
	# sqr a[6]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	20(%esi),%eax
	adcl	$0,%ecx
	movl	12(%esi),%edx
	# sqr a[5]*a[3]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	16(%esi),%eax
	adcl	$0,%ecx
	# sqr a[4]*a[4]

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	8(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,32(%edi)
	movl	28(%esi),%eax
	# saved r[8]

	# ############### Calculate word 9

	xorl	%ebp,%ebp
	# sqr a[7]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%eax
	adcl	$0,%ebp
	movl	12(%esi),%edx
	# sqr a[6]*a[3]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	20(%esi),%eax
	adcl	$0,%ebp
	movl	16(%esi),%edx
	# sqr a[5]*a[4]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	28(%esi),%eax
	adcl	$0,%ebp
	movl	%ebx,36(%edi)
	movl	12(%esi),%edx
	# saved r[9]

	# ############### Calculate word 10

	xorl	%ebx,%ebx
	# sqr a[7]*a[3]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	24(%esi),%eax
	adcl	$0,%ebx
	movl	16(%esi),%edx
	# sqr a[6]*a[4]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	20(%esi),%eax
	adcl	$0,%ebx
	# sqr a[5]*a[5]

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	16(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,40(%edi)
	movl	28(%esi),%eax
	# saved r[10]

	# ############### Calculate word 11

	xorl	%ecx,%ecx
	# sqr a[7]*a[4]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	24(%esi),%eax
	adcl	$0,%ecx
	movl	20(%esi),%edx
	# sqr a[6]*a[5]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	28(%esi),%eax
	adcl	$0,%ecx
	movl	%ebp,44(%edi)
	movl	20(%esi),%edx
	# saved r[11]

	# ############### Calculate word 12

	xorl	%ebp,%ebp
	# sqr a[7]*a[5]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%eax
	adcl	$0,%ebp
	# sqr a[6]*a[6]

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,48(%edi)
	movl	28(%esi),%eax
	# saved r[12]

	# ############### Calculate word 13

	xorl	%ebx,%ebx
	# sqr a[7]*a[6]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	28(%esi),%eax
	adcl	$0,%ebx
	movl	%ecx,52(%edi)
	# saved r[13]

	# ############### Calculate word 14

	xorl	%ecx,%ecx
	# sqr a[7]*a[7]

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	adcl	$0,%ecx
	movl	%ebp,56(%edi)
	# saved r[14]

	movl	%ebx,60(%edi)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.globl	_bn_sqr_comba4
.align	4
_bn_sqr_comba4:
L_bn_sqr_comba4_begin:
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	pushl	%ebx
	movl	20(%esp),%edi
	movl	24(%esp),%esi
	xorl	%ebx,%ebx
	xorl	%ecx,%ecx
	movl	(%esi),%eax
	# ############### Calculate word 0

	xorl	%ebp,%ebp
	# sqr a[0]*a[0]

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%edi)
	movl	4(%esi),%eax
	# saved r[0]

	# ############### Calculate word 1

	xorl	%ebx,%ebx
	# sqr a[1]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx
	movl	%ecx,4(%edi)
	movl	(%esi),%edx
	# saved r[1]

	# ############### Calculate word 2

	xorl	%ecx,%ecx
	# sqr a[2]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	4(%esi),%eax
	adcl	$0,%ecx
	# sqr a[1]*a[1]

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%edi)
	movl	12(%esi),%eax
	# saved r[2]

	# ############### Calculate word 3

	xorl	%ebp,%ebp
	# sqr a[3]*a[0]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	8(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx
	# sqr a[2]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	12(%esi),%eax
	adcl	$0,%ebp
	movl	%ebx,12(%edi)
	movl	4(%esi),%edx
	# saved r[3]

	# ############### Calculate word 4

	xorl	%ebx,%ebx
	# sqr a[3]*a[1]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx
	# sqr a[2]*a[2]

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%edi)
	movl	12(%esi),%eax
	# saved r[4]

	# ############### Calculate word 5

	xorl	%ecx,%ecx
	# sqr a[3]*a[2]

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	12(%esi),%eax
	adcl	$0,%ecx
	movl	%ebp,20(%edi)
	# saved r[5]

	# ############### Calculate word 6

	xorl	%ebp,%ebp
	# sqr a[3]*a[3]

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	adcl	$0,%ebp
	movl	%ebx,24(%edi)
	# saved r[6]

	movl	%ecx,28(%edi)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
