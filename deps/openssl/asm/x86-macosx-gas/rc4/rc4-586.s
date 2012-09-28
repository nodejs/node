.file	"rc4-586.s"
.text
.globl	_RC4
.align	4
_RC4:
L_RC4_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%edi
	movl	24(%esp),%edx
	movl	28(%esp),%esi
	movl	32(%esp),%ebp
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	cmpl	$0,%edx
	je	L000abort
	movb	(%edi),%al
	movb	4(%edi),%bl
	addl	$8,%edi
	leal	(%esi,%edx,1),%ecx
	subl	%esi,%ebp
	movl	%ecx,24(%esp)
	incb	%al
	cmpl	$-1,256(%edi)
	je	L001RC4_CHAR
	movl	(%edi,%eax,4),%ecx
	andl	$-4,%edx
	jz	L002loop1
	leal	-4(%esi,%edx,1),%edx
	movl	%edx,28(%esp)
	movl	%ebp,32(%esp)
.align	4,0x90
L003loop4:
	addb	%cl,%bl
	movl	(%edi,%ebx,4),%edx
	movl	%ecx,(%edi,%ebx,4)
	movl	%edx,(%edi,%eax,4)
	addl	%ecx,%edx
	incb	%al
	andl	$255,%edx
	movl	(%edi,%eax,4),%ecx
	movl	(%edi,%edx,4),%ebp
	addb	%cl,%bl
	movl	(%edi,%ebx,4),%edx
	movl	%ecx,(%edi,%ebx,4)
	movl	%edx,(%edi,%eax,4)
	addl	%ecx,%edx
	incb	%al
	andl	$255,%edx
	rorl	$8,%ebp
	movl	(%edi,%eax,4),%ecx
	orl	(%edi,%edx,4),%ebp
	addb	%cl,%bl
	movl	(%edi,%ebx,4),%edx
	movl	%ecx,(%edi,%ebx,4)
	movl	%edx,(%edi,%eax,4)
	addl	%ecx,%edx
	incb	%al
	andl	$255,%edx
	rorl	$8,%ebp
	movl	(%edi,%eax,4),%ecx
	orl	(%edi,%edx,4),%ebp
	addb	%cl,%bl
	movl	(%edi,%ebx,4),%edx
	movl	%ecx,(%edi,%ebx,4)
	movl	%edx,(%edi,%eax,4)
	addl	%ecx,%edx
	incb	%al
	andl	$255,%edx
	rorl	$8,%ebp
	movl	32(%esp),%ecx
	orl	(%edi,%edx,4),%ebp
	rorl	$8,%ebp
	xorl	(%esi),%ebp
	cmpl	28(%esp),%esi
	movl	%ebp,(%ecx,%esi,1)
	leal	4(%esi),%esi
	movl	(%edi,%eax,4),%ecx
	jb	L003loop4
	cmpl	24(%esp),%esi
	je	L004done
	movl	32(%esp),%ebp
.align	4,0x90
L002loop1:
	addb	%cl,%bl
	movl	(%edi,%ebx,4),%edx
	movl	%ecx,(%edi,%ebx,4)
	movl	%edx,(%edi,%eax,4)
	addl	%ecx,%edx
	incb	%al
	andl	$255,%edx
	movl	(%edi,%edx,4),%edx
	xorb	(%esi),%dl
	leal	1(%esi),%esi
	movl	(%edi,%eax,4),%ecx
	cmpl	24(%esp),%esi
	movb	%dl,-1(%ebp,%esi,1)
	jb	L002loop1
	jmp	L004done
.align	4,0x90
L001RC4_CHAR:
	movzbl	(%edi,%eax,1),%ecx
L005cloop1:
	addb	%cl,%bl
	movzbl	(%edi,%ebx,1),%edx
	movb	%cl,(%edi,%ebx,1)
	movb	%dl,(%edi,%eax,1)
	addb	%cl,%dl
	movzbl	(%edi,%edx,1),%edx
	addb	$1,%al
	xorb	(%esi),%dl
	leal	1(%esi),%esi
	movzbl	(%edi,%eax,1),%ecx
	cmpl	24(%esp),%esi
	movb	%dl,-1(%ebp,%esi,1)
	jb	L005cloop1
L004done:
	decb	%al
	movb	%bl,-4(%edi)
	movb	%al,-8(%edi)
L000abort:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_RC4_set_key
.align	4
_RC4_set_key:
L_RC4_set_key_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%edi
	movl	24(%esp),%ebp
	movl	28(%esp),%esi
	leal	_OPENSSL_ia32cap_P,%edx
	leal	8(%edi),%edi
	leal	(%esi,%ebp,1),%esi
	negl	%ebp
	xorl	%eax,%eax
	movl	%ebp,-4(%edi)
	btl	$20,(%edx)
	jc	L006c1stloop
.align	4,0x90
L007w1stloop:
	movl	%eax,(%edi,%eax,4)
	addb	$1,%al
	jnc	L007w1stloop
	xorl	%ecx,%ecx
	xorl	%edx,%edx
.align	4,0x90
L008w2ndloop:
	movl	(%edi,%ecx,4),%eax
	addb	(%esi,%ebp,1),%dl
	addb	%al,%dl
	addl	$1,%ebp
	movl	(%edi,%edx,4),%ebx
	jnz	L009wnowrap
	movl	-4(%edi),%ebp
L009wnowrap:
	movl	%eax,(%edi,%edx,4)
	movl	%ebx,(%edi,%ecx,4)
	addb	$1,%cl
	jnc	L008w2ndloop
	jmp	L010exit
.align	4,0x90
L006c1stloop:
	movb	%al,(%edi,%eax,1)
	addb	$1,%al
	jnc	L006c1stloop
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	xorl	%ebx,%ebx
.align	4,0x90
L011c2ndloop:
	movb	(%edi,%ecx,1),%al
	addb	(%esi,%ebp,1),%dl
	addb	%al,%dl
	addl	$1,%ebp
	movb	(%edi,%edx,1),%bl
	jnz	L012cnowrap
	movl	-4(%edi),%ebp
L012cnowrap:
	movb	%al,(%edi,%edx,1)
	movb	%bl,(%edi,%ecx,1)
	addb	$1,%cl
	jnc	L011c2ndloop
	movl	$-1,256(%edi)
L010exit:
	xorl	%eax,%eax
	movl	%eax,-8(%edi)
	movl	%eax,-4(%edi)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_RC4_options
.align	4
_RC4_options:
L_RC4_options_begin:
	call	L013pic_point
L013pic_point:
	popl	%eax
	leal	L014opts-L013pic_point(%eax),%eax
	leal	_OPENSSL_ia32cap_P,%edx
	btl	$20,(%edx)
	jnc	L015skip
	addl	$12,%eax
L015skip:
	ret
.align	6,0x90
L014opts:
.byte	114,99,52,40,52,120,44,105,110,116,41,0
.byte	114,99,52,40,49,120,44,99,104,97,114,41,0
.byte	82,67,52,32,102,111,114,32,120,56,54,44,32,67,82,89
.byte	80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114
.byte	111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	6,0x90
.comm	_OPENSSL_ia32cap_P,4
