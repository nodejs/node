.file	"../openssl/crypto/bn/asm/x86.s"
.text
.globl	bn_mul_add_words
.type	bn_mul_add_words,@function
.align	16
bn_mul_add_words:
.L_bn_mul_add_words_begin:
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
	jz	.L000maw_finish
.L001maw_loop:
	movl	%ecx,(%esp)

	movl	(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi

	movl	4(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	4(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi

	movl	8(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	8(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi

	movl	12(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	12(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi

	movl	16(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	16(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi

	movl	20(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	20(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi

	movl	24(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	24(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi

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
	jnz	.L001maw_loop
.L000maw_finish:
	movl	32(%esp),%ecx
	andl	$7,%ecx
	jnz	.L002maw_finish2
	jmp	.L003maw_end
.L002maw_finish2:

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
	jz	.L003maw_end

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
	jz	.L003maw_end

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
	jz	.L003maw_end

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
	jz	.L003maw_end

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
	jz	.L003maw_end

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
	jz	.L003maw_end

	movl	24(%ebx),%eax
	mull	%ebp
	addl	%esi,%eax
	movl	24(%edi),%esi
	adcl	$0,%edx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
.L003maw_end:
	movl	%esi,%eax
	popl	%ecx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	bn_mul_add_words,.-.L_bn_mul_add_words_begin
.globl	bn_mul_words
.type	bn_mul_words,@function
.align	16
bn_mul_words:
.L_bn_mul_words_begin:
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
	jz	.L004mw_finish
.L005mw_loop:

	movl	(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi

	movl	4(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi

	movl	8(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi

	movl	12(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi

	movl	16(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi

	movl	20(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi

	movl	24(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi

	movl	28(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,28(%edi)
	movl	%edx,%esi

	addl	$32,%ebx
	addl	$32,%edi
	subl	$8,%ebp
	jz	.L004mw_finish
	jmp	.L005mw_loop
.L004mw_finish:
	movl	28(%esp),%ebp
	andl	$7,%ebp
	jnz	.L006mw_finish2
	jmp	.L007mw_end
.L006mw_finish2:

	movl	(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	4(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,4(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	8(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,8(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	12(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,12(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	16(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,16(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	20(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,20(%edi)
	movl	%edx,%esi
	decl	%ebp
	jz	.L007mw_end

	movl	24(%ebx),%eax
	mull	%ecx
	addl	%esi,%eax
	adcl	$0,%edx
	movl	%eax,24(%edi)
	movl	%edx,%esi
.L007mw_end:
	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	bn_mul_words,.-.L_bn_mul_words_begin
.globl	bn_sqr_words
.type	bn_sqr_words,@function
.align	16
bn_sqr_words:
.L_bn_sqr_words_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%esi
	movl	24(%esp),%edi
	movl	28(%esp),%ebx
	andl	$4294967288,%ebx
	jz	.L008sw_finish
.L009sw_loop:

	movl	(%edi),%eax
	mull	%eax
	movl	%eax,(%esi)
	movl	%edx,4(%esi)

	movl	4(%edi),%eax
	mull	%eax
	movl	%eax,8(%esi)
	movl	%edx,12(%esi)

	movl	8(%edi),%eax
	mull	%eax
	movl	%eax,16(%esi)
	movl	%edx,20(%esi)

	movl	12(%edi),%eax
	mull	%eax
	movl	%eax,24(%esi)
	movl	%edx,28(%esi)

	movl	16(%edi),%eax
	mull	%eax
	movl	%eax,32(%esi)
	movl	%edx,36(%esi)

	movl	20(%edi),%eax
	mull	%eax
	movl	%eax,40(%esi)
	movl	%edx,44(%esi)

	movl	24(%edi),%eax
	mull	%eax
	movl	%eax,48(%esi)
	movl	%edx,52(%esi)

	movl	28(%edi),%eax
	mull	%eax
	movl	%eax,56(%esi)
	movl	%edx,60(%esi)

	addl	$32,%edi
	addl	$64,%esi
	subl	$8,%ebx
	jnz	.L009sw_loop
.L008sw_finish:
	movl	28(%esp),%ebx
	andl	$7,%ebx
	jz	.L010sw_end

	movl	(%edi),%eax
	mull	%eax
	movl	%eax,(%esi)
	decl	%ebx
	movl	%edx,4(%esi)
	jz	.L010sw_end

	movl	4(%edi),%eax
	mull	%eax
	movl	%eax,8(%esi)
	decl	%ebx
	movl	%edx,12(%esi)
	jz	.L010sw_end

	movl	8(%edi),%eax
	mull	%eax
	movl	%eax,16(%esi)
	decl	%ebx
	movl	%edx,20(%esi)
	jz	.L010sw_end

	movl	12(%edi),%eax
	mull	%eax
	movl	%eax,24(%esi)
	decl	%ebx
	movl	%edx,28(%esi)
	jz	.L010sw_end

	movl	16(%edi),%eax
	mull	%eax
	movl	%eax,32(%esi)
	decl	%ebx
	movl	%edx,36(%esi)
	jz	.L010sw_end

	movl	20(%edi),%eax
	mull	%eax
	movl	%eax,40(%esi)
	decl	%ebx
	movl	%edx,44(%esi)
	jz	.L010sw_end

	movl	24(%edi),%eax
	mull	%eax
	movl	%eax,48(%esi)
	movl	%edx,52(%esi)
.L010sw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	bn_sqr_words,.-.L_bn_sqr_words_begin
.globl	bn_div_words
.type	bn_div_words,@function
.align	16
bn_div_words:
.L_bn_div_words_begin:
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
.size	bn_div_words,.-.L_bn_div_words_begin
.globl	bn_add_words
.type	bn_add_words,@function
.align	16
bn_add_words:
.L_bn_add_words_begin:
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
	jz	.L011aw_finish
.L012aw_loop:

	movl	(%esi),%ecx
	movl	(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,(%ebx)

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,4(%ebx)

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,8(%ebx)

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,12(%ebx)

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,16(%ebx)

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,20(%ebx)

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)

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
	jnz	.L012aw_loop
.L011aw_finish:
	movl	32(%esp),%ebp
	andl	$7,%ebp
	jz	.L013aw_end

	movl	(%esi),%ecx
	movl	(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,(%ebx)
	jz	.L013aw_end

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,4(%ebx)
	jz	.L013aw_end

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,8(%ebx)
	jz	.L013aw_end

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,12(%ebx)
	jz	.L013aw_end

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,16(%ebx)
	jz	.L013aw_end

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,20(%ebx)
	jz	.L013aw_end

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	addl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	addl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
.L013aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	bn_add_words,.-.L_bn_add_words_begin
.globl	bn_sub_words
.type	bn_sub_words,@function
.align	16
bn_sub_words:
.L_bn_sub_words_begin:
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
	jz	.L014aw_finish
.L015aw_loop:

	movl	(%esi),%ecx
	movl	(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,(%ebx)

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,4(%ebx)

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,8(%ebx)

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,12(%ebx)

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,16(%ebx)

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,20(%ebx)

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)

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
	jnz	.L015aw_loop
.L014aw_finish:
	movl	32(%esp),%ebp
	andl	$7,%ebp
	jz	.L016aw_end

	movl	(%esi),%ecx
	movl	(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,(%ebx)
	jz	.L016aw_end

	movl	4(%esi),%ecx
	movl	4(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,4(%ebx)
	jz	.L016aw_end

	movl	8(%esi),%ecx
	movl	8(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,8(%ebx)
	jz	.L016aw_end

	movl	12(%esi),%ecx
	movl	12(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,12(%ebx)
	jz	.L016aw_end

	movl	16(%esi),%ecx
	movl	16(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,16(%ebx)
	jz	.L016aw_end

	movl	20(%esi),%ecx
	movl	20(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	decl	%ebp
	movl	%ecx,20(%ebx)
	jz	.L016aw_end

	movl	24(%esi),%ecx
	movl	24(%edi),%edx
	subl	%eax,%ecx
	movl	$0,%eax
	adcl	%eax,%eax
	subl	%edx,%ecx
	adcl	$0,%eax
	movl	%ecx,24(%ebx)
.L016aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	bn_sub_words,.-.L_bn_sub_words_begin
.globl	bn_mul_comba8
.type	bn_mul_comba8,@function
.align	16
bn_mul_comba8:
.L_bn_mul_comba8_begin:
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

	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%eax)
	movl	4(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,4(%eax)
	movl	8(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%eax)
	movl	12(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,12(%eax)
	movl	16(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%eax)
	movl	20(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	12(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	16(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,20(%eax)
	movl	24(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	16(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	12(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	16(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	20(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,24(%eax)
	movl	28(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	16(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	20(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	24(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,28(%eax)
	movl	28(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	24(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	16(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	12(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	24(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	28(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,32(%eax)
	movl	28(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	24(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	16(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	16(%esi),%eax
	adcl	%edx,%ecx
	movl	20(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	12(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	28(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,36(%eax)
	movl	28(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esi),%eax
	adcl	%edx,%ebp
	movl	20(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	16(%esi),%eax
	adcl	%edx,%ebp
	movl	24(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	12(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	16(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,40(%eax)
	movl	28(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	24(%esi),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esi),%eax
	adcl	%edx,%ebx
	movl	24(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	16(%esi),%eax
	adcl	%edx,%ebx
	movl	28(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	20(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,44(%eax)
	movl	28(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	24(%esi),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esi),%eax
	adcl	%edx,%ecx
	movl	28(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	24(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,48(%eax)
	movl	28(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	24(%esi),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	28(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,52(%eax)
	movl	28(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	adcl	$0,%ecx
	movl	%ebp,56(%eax)


	movl	%ebx,60(%eax)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.size	bn_mul_comba8,.-.L_bn_mul_comba8_begin
.globl	bn_mul_comba4
.type	bn_mul_comba4,@function
.align	16
bn_mul_comba4:
.L_bn_mul_comba4_begin:
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

	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%eax)
	movl	4(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	(%esi),%eax
	adcl	%edx,%ebp
	movl	4(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,4(%eax)
	movl	8(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	4(%esi),%eax
	adcl	%edx,%ebx
	movl	4(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	(%esi),%eax
	adcl	%edx,%ebx
	movl	8(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%eax)
	movl	12(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	8(%esi),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	4(%esi),%eax
	adcl	%edx,%ecx
	movl	8(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	(%esi),%eax
	adcl	%edx,%ecx
	movl	12(%edi),%edx
	adcl	$0,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	movl	4(%edi),%edx
	adcl	$0,%ebp
	movl	%ebx,12(%eax)
	movl	12(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	8(%esi),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	4(%esi),%eax
	adcl	%edx,%ebp
	movl	12(%edi),%edx
	adcl	$0,%ebx

	mull	%edx
	addl	%eax,%ecx
	movl	20(%esp),%eax
	adcl	%edx,%ebp
	movl	8(%edi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%eax)
	movl	12(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	8(%esi),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx

	mull	%edx
	addl	%eax,%ebp
	movl	20(%esp),%eax
	adcl	%edx,%ebx
	movl	12(%edi),%edx
	adcl	$0,%ecx
	movl	%ebp,20(%eax)
	movl	12(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%ebx
	movl	20(%esp),%eax
	adcl	%edx,%ecx
	adcl	$0,%ebp
	movl	%ebx,24(%eax)


	movl	%ecx,28(%eax)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.size	bn_mul_comba4,.-.L_bn_mul_comba4_begin
.globl	bn_sqr_comba8
.type	bn_sqr_comba8,@function
.align	16
bn_sqr_comba8:
.L_bn_sqr_comba8_begin:
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	pushl	%ebx
	movl	20(%esp),%edi
	movl	24(%esp),%esi
	xorl	%ebx,%ebx
	xorl	%ecx,%ecx
	movl	(%esi),%eax

	xorl	%ebp,%ebp

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%edi)
	movl	4(%esi),%eax


	xorl	%ebx,%ebx

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


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	4(%esi),%eax
	adcl	$0,%ecx

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%edi)
	movl	12(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	8(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx

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


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	12(%esi),%eax
	adcl	$0,%ebx
	movl	4(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%edi)
	movl	20(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	16(%esi),%eax
	adcl	$0,%ecx
	movl	4(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	12(%esi),%eax
	adcl	$0,%ecx
	movl	8(%esi),%edx

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


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	20(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	16(%esi),%eax
	adcl	$0,%ebp
	movl	8(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	12(%esi),%eax
	adcl	$0,%ebp

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,24(%edi)
	movl	28(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	24(%esi),%eax
	adcl	$0,%ebx
	movl	4(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	20(%esi),%eax
	adcl	$0,%ebx
	movl	8(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	16(%esi),%eax
	adcl	$0,%ebx
	movl	12(%esi),%edx

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


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	24(%esi),%eax
	adcl	$0,%ecx
	movl	8(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	20(%esi),%eax
	adcl	$0,%ecx
	movl	12(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	16(%esi),%eax
	adcl	$0,%ecx

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	8(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,32(%edi)
	movl	28(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%eax
	adcl	$0,%ebp
	movl	12(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	20(%esi),%eax
	adcl	$0,%ebp
	movl	16(%esi),%edx

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


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	24(%esi),%eax
	adcl	$0,%ebx
	movl	16(%esi),%edx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	20(%esi),%eax
	adcl	$0,%ebx

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	16(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,40(%edi)
	movl	28(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	24(%esi),%eax
	adcl	$0,%ecx
	movl	20(%esi),%edx

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


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%eax
	adcl	$0,%ebp

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	24(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,48(%edi)
	movl	28(%esi),%eax


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	28(%esi),%eax
	adcl	$0,%ebx
	movl	%ecx,52(%edi)


	xorl	%ecx,%ecx

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	adcl	$0,%ecx
	movl	%ebp,56(%edi)

	movl	%ebx,60(%edi)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.size	bn_sqr_comba8,.-.L_bn_sqr_comba8_begin
.globl	bn_sqr_comba4
.type	bn_sqr_comba4,@function
.align	16
bn_sqr_comba4:
.L_bn_sqr_comba4_begin:
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	pushl	%ebx
	movl	20(%esp),%edi
	movl	24(%esp),%esi
	xorl	%ebx,%ebx
	xorl	%ecx,%ecx
	movl	(%esi),%eax

	xorl	%ebp,%ebp

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	(%esi),%edx
	adcl	$0,%ebp
	movl	%ebx,(%edi)
	movl	4(%esi),%eax


	xorl	%ebx,%ebx

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


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	4(%esi),%eax
	adcl	$0,%ecx

	mull	%eax
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	(%esi),%edx
	adcl	$0,%ecx
	movl	%ebp,8(%edi)
	movl	12(%esi),%eax


	xorl	%ebp,%ebp

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebp
	addl	%eax,%ebx
	adcl	%edx,%ecx
	movl	8(%esi),%eax
	adcl	$0,%ebp
	movl	4(%esi),%edx

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


	xorl	%ebx,%ebx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ebx
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%eax
	adcl	$0,%ebx

	mull	%eax
	addl	%eax,%ecx
	adcl	%edx,%ebp
	movl	8(%esi),%edx
	adcl	$0,%ebx
	movl	%ecx,16(%edi)
	movl	12(%esi),%eax


	xorl	%ecx,%ecx

	mull	%edx
	addl	%eax,%eax
	adcl	%edx,%edx
	adcl	$0,%ecx
	addl	%eax,%ebp
	adcl	%edx,%ebx
	movl	12(%esi),%eax
	adcl	$0,%ecx
	movl	%ebp,20(%edi)


	xorl	%ebp,%ebp

	mull	%eax
	addl	%eax,%ebx
	adcl	%edx,%ecx
	adcl	$0,%ebp
	movl	%ebx,24(%edi)

	movl	%ecx,28(%edi)
	popl	%ebx
	popl	%ebp
	popl	%edi
	popl	%esi
	ret
.size	bn_sqr_comba4,.-.L_bn_sqr_comba4_begin
