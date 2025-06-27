.text
.globl	DES_SPtrans
.type	_x86_DES_encrypt,@function
.align	16
_x86_DES_encrypt:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ecx

	movl	(%ecx),%eax
	xorl	%ebx,%ebx
	movl	4(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	8(%ecx),%eax
	xorl	%ebx,%ebx
	movl	12(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	16(%ecx),%eax
	xorl	%ebx,%ebx
	movl	20(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	24(%ecx),%eax
	xorl	%ebx,%ebx
	movl	28(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	32(%ecx),%eax
	xorl	%ebx,%ebx
	movl	36(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	40(%ecx),%eax
	xorl	%ebx,%ebx
	movl	44(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	48(%ecx),%eax
	xorl	%ebx,%ebx
	movl	52(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	56(%ecx),%eax
	xorl	%ebx,%ebx
	movl	60(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	64(%ecx),%eax
	xorl	%ebx,%ebx
	movl	68(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	72(%ecx),%eax
	xorl	%ebx,%ebx
	movl	76(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	80(%ecx),%eax
	xorl	%ebx,%ebx
	movl	84(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	88(%ecx),%eax
	xorl	%ebx,%ebx
	movl	92(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	96(%ecx),%eax
	xorl	%ebx,%ebx
	movl	100(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	104(%ecx),%eax
	xorl	%ebx,%ebx
	movl	108(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	112(%ecx),%eax
	xorl	%ebx,%ebx
	movl	116(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	120(%ecx),%eax
	xorl	%ebx,%ebx
	movl	124(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi
	addl	$4,%esp
	ret
.size	_x86_DES_encrypt,.-_x86_DES_encrypt
.type	_x86_DES_decrypt,@function
.align	16
_x86_DES_decrypt:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ecx

	movl	120(%ecx),%eax
	xorl	%ebx,%ebx
	movl	124(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	112(%ecx),%eax
	xorl	%ebx,%ebx
	movl	116(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	104(%ecx),%eax
	xorl	%ebx,%ebx
	movl	108(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	96(%ecx),%eax
	xorl	%ebx,%ebx
	movl	100(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	88(%ecx),%eax
	xorl	%ebx,%ebx
	movl	92(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	80(%ecx),%eax
	xorl	%ebx,%ebx
	movl	84(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	72(%ecx),%eax
	xorl	%ebx,%ebx
	movl	76(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	64(%ecx),%eax
	xorl	%ebx,%ebx
	movl	68(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	56(%ecx),%eax
	xorl	%ebx,%ebx
	movl	60(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	48(%ecx),%eax
	xorl	%ebx,%ebx
	movl	52(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	40(%ecx),%eax
	xorl	%ebx,%ebx
	movl	44(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	32(%ecx),%eax
	xorl	%ebx,%ebx
	movl	36(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	24(%ecx),%eax
	xorl	%ebx,%ebx
	movl	28(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	16(%ecx),%eax
	xorl	%ebx,%ebx
	movl	20(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi

	movl	8(%ecx),%eax
	xorl	%ebx,%ebx
	movl	12(%ecx),%edx
	xorl	%esi,%eax
	xorl	%ecx,%ecx
	xorl	%esi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%edi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%edi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%edi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%edi
	xorl	0x700(%ebp,%ecx,1),%edi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%edi
	xorl	0x500(%ebp,%edx,1),%edi

	movl	(%ecx),%eax
	xorl	%ebx,%ebx
	movl	4(%ecx),%edx
	xorl	%edi,%eax
	xorl	%ecx,%ecx
	xorl	%edi,%edx
	andl	$0xfcfcfcfc,%eax
	andl	$0xcfcfcfcf,%edx
	movb	%al,%bl
	movb	%ah,%cl
	rorl	$4,%edx
	xorl	(%ebp,%ebx,1),%esi
	movb	%dl,%bl
	xorl	0x200(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	shrl	$16,%eax
	xorl	0x100(%ebp,%ebx,1),%esi
	movb	%ah,%bl
	shrl	$16,%edx
	xorl	0x300(%ebp,%ecx,1),%esi
	movb	%dh,%cl
	andl	$0xff,%eax
	andl	$0xff,%edx
	xorl	0x600(%ebp,%ebx,1),%esi
	xorl	0x700(%ebp,%ecx,1),%esi
	movl	(%esp),%ecx
	xorl	0x400(%ebp,%eax,1),%esi
	xorl	0x500(%ebp,%edx,1),%esi
	addl	$4,%esp
	ret
.size	_x86_DES_decrypt,.-_x86_DES_decrypt
.globl	DES_encrypt1
.type	DES_encrypt1,@function
.align	16
DES_encrypt1:
.L_DES_encrypt1_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%esi
	pushl	%edi


	movl	12(%esp),%esi
	xorl	%ecx,%ecx
	pushl	%ebx
	pushl	%ebp
	movl	(%esi),%eax
	movl	28(%esp),%ebx
	movl	4(%esi),%edi


	roll	$4,%eax
	movl	%eax,%esi
	xorl	%edi,%eax
	andl	$0xf0f0f0f0,%eax
	xorl	%eax,%esi
	xorl	%eax,%edi

	roll	$20,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0xfff0000f,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$14,%eax
	movl	%eax,%edi
	xorl	%esi,%eax
	andl	$0x33333333,%eax
	xorl	%eax,%edi
	xorl	%eax,%esi

	roll	$22,%esi
	movl	%esi,%eax
	xorl	%edi,%esi
	andl	$0x03fc03fc,%esi
	xorl	%esi,%eax
	xorl	%esi,%edi

	roll	$9,%eax
	movl	%eax,%esi
	xorl	%edi,%eax
	andl	$0xaaaaaaaa,%eax
	xorl	%eax,%esi
	xorl	%eax,%edi

	roll	$1,%edi
	call	.L000pic_point
.L000pic_point:
	popl	%ebp
	leal	.Ldes_sptrans-.L000pic_point(%ebp),%ebp
	movl	24(%esp),%ecx
	cmpl	$0,%ebx
	je	.L001decrypt
	call	_x86_DES_encrypt
	jmp	.L002done
.L001decrypt:
	call	_x86_DES_decrypt
.L002done:


	movl	20(%esp),%edx
	rorl	$1,%esi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0xaaaaaaaa,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$23,%eax
	movl	%eax,%edi
	xorl	%esi,%eax
	andl	$0x03fc03fc,%eax
	xorl	%eax,%edi
	xorl	%eax,%esi

	roll	$10,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0x33333333,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$18,%esi
	movl	%esi,%edi
	xorl	%eax,%esi
	andl	$0xfff0000f,%esi
	xorl	%esi,%edi
	xorl	%esi,%eax

	roll	$12,%edi
	movl	%edi,%esi
	xorl	%eax,%edi
	andl	$0xf0f0f0f0,%edi
	xorl	%edi,%esi
	xorl	%edi,%eax

	rorl	$4,%eax
	movl	%eax,(%edx)
	movl	%esi,4(%edx)
	popl	%ebp
	popl	%ebx
	popl	%edi
	popl	%esi
	ret
.size	DES_encrypt1,.-.L_DES_encrypt1_begin
.globl	DES_encrypt2
.type	DES_encrypt2,@function
.align	16
DES_encrypt2:
.L_DES_encrypt2_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%esi
	pushl	%edi


	movl	12(%esp),%eax
	xorl	%ecx,%ecx
	pushl	%ebx
	pushl	%ebp
	movl	(%eax),%esi
	movl	28(%esp),%ebx
	roll	$3,%esi
	movl	4(%eax),%edi
	roll	$3,%edi
	call	.L003pic_point
.L003pic_point:
	popl	%ebp
	leal	.Ldes_sptrans-.L003pic_point(%ebp),%ebp
	movl	24(%esp),%ecx
	cmpl	$0,%ebx
	je	.L004decrypt
	call	_x86_DES_encrypt
	jmp	.L005done
.L004decrypt:
	call	_x86_DES_decrypt
.L005done:


	rorl	$3,%edi
	movl	20(%esp),%eax
	rorl	$3,%esi
	movl	%edi,(%eax)
	movl	%esi,4(%eax)
	popl	%ebp
	popl	%ebx
	popl	%edi
	popl	%esi
	ret
.size	DES_encrypt2,.-.L_DES_encrypt2_begin
.globl	DES_encrypt3
.type	DES_encrypt3,@function
.align	16
DES_encrypt3:
.L_DES_encrypt3_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebx
	movl	8(%esp),%ebx
	pushl	%ebp
	pushl	%esi
	pushl	%edi


	movl	(%ebx),%edi
	movl	4(%ebx),%esi
	subl	$12,%esp


	roll	$4,%edi
	movl	%edi,%edx
	xorl	%esi,%edi
	andl	$0xf0f0f0f0,%edi
	xorl	%edi,%edx
	xorl	%edi,%esi

	roll	$20,%esi
	movl	%esi,%edi
	xorl	%edx,%esi
	andl	$0xfff0000f,%esi
	xorl	%esi,%edi
	xorl	%esi,%edx

	roll	$14,%edi
	movl	%edi,%esi
	xorl	%edx,%edi
	andl	$0x33333333,%edi
	xorl	%edi,%esi
	xorl	%edi,%edx

	roll	$22,%edx
	movl	%edx,%edi
	xorl	%esi,%edx
	andl	$0x03fc03fc,%edx
	xorl	%edx,%edi
	xorl	%edx,%esi

	roll	$9,%edi
	movl	%edi,%edx
	xorl	%esi,%edi
	andl	$0xaaaaaaaa,%edi
	xorl	%edi,%edx
	xorl	%edi,%esi

	rorl	$3,%edx
	rorl	$2,%esi
	movl	%esi,4(%ebx)
	movl	36(%esp),%eax
	movl	%edx,(%ebx)
	movl	40(%esp),%edi
	movl	44(%esp),%esi
	movl	$1,8(%esp)
	movl	%eax,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	movl	$0,8(%esp)
	movl	%edi,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	movl	$1,8(%esp)
	movl	%esi,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	addl	$12,%esp
	movl	(%ebx),%edi
	movl	4(%ebx),%esi


	roll	$2,%esi
	roll	$3,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0xaaaaaaaa,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$23,%eax
	movl	%eax,%edi
	xorl	%esi,%eax
	andl	$0x03fc03fc,%eax
	xorl	%eax,%edi
	xorl	%eax,%esi

	roll	$10,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0x33333333,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$18,%esi
	movl	%esi,%edi
	xorl	%eax,%esi
	andl	$0xfff0000f,%esi
	xorl	%esi,%edi
	xorl	%esi,%eax

	roll	$12,%edi
	movl	%edi,%esi
	xorl	%eax,%edi
	andl	$0xf0f0f0f0,%edi
	xorl	%edi,%esi
	xorl	%edi,%eax

	rorl	$4,%eax
	movl	%eax,(%ebx)
	movl	%esi,4(%ebx)
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
.size	DES_encrypt3,.-.L_DES_encrypt3_begin
.globl	DES_decrypt3
.type	DES_decrypt3,@function
.align	16
DES_decrypt3:
.L_DES_decrypt3_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebx
	movl	8(%esp),%ebx
	pushl	%ebp
	pushl	%esi
	pushl	%edi


	movl	(%ebx),%edi
	movl	4(%ebx),%esi
	subl	$12,%esp


	roll	$4,%edi
	movl	%edi,%edx
	xorl	%esi,%edi
	andl	$0xf0f0f0f0,%edi
	xorl	%edi,%edx
	xorl	%edi,%esi

	roll	$20,%esi
	movl	%esi,%edi
	xorl	%edx,%esi
	andl	$0xfff0000f,%esi
	xorl	%esi,%edi
	xorl	%esi,%edx

	roll	$14,%edi
	movl	%edi,%esi
	xorl	%edx,%edi
	andl	$0x33333333,%edi
	xorl	%edi,%esi
	xorl	%edi,%edx

	roll	$22,%edx
	movl	%edx,%edi
	xorl	%esi,%edx
	andl	$0x03fc03fc,%edx
	xorl	%edx,%edi
	xorl	%edx,%esi

	roll	$9,%edi
	movl	%edi,%edx
	xorl	%esi,%edi
	andl	$0xaaaaaaaa,%edi
	xorl	%edi,%edx
	xorl	%edi,%esi

	rorl	$3,%edx
	rorl	$2,%esi
	movl	%esi,4(%ebx)
	movl	36(%esp),%esi
	movl	%edx,(%ebx)
	movl	40(%esp),%edi
	movl	44(%esp),%eax
	movl	$0,8(%esp)
	movl	%eax,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	movl	$1,8(%esp)
	movl	%edi,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	movl	$0,8(%esp)
	movl	%esi,4(%esp)
	movl	%ebx,(%esp)
	call	.L_DES_encrypt2_begin
	addl	$12,%esp
	movl	(%ebx),%edi
	movl	4(%ebx),%esi


	roll	$2,%esi
	roll	$3,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0xaaaaaaaa,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$23,%eax
	movl	%eax,%edi
	xorl	%esi,%eax
	andl	$0x03fc03fc,%eax
	xorl	%eax,%edi
	xorl	%eax,%esi

	roll	$10,%edi
	movl	%edi,%eax
	xorl	%esi,%edi
	andl	$0x33333333,%edi
	xorl	%edi,%eax
	xorl	%edi,%esi

	roll	$18,%esi
	movl	%esi,%edi
	xorl	%eax,%esi
	andl	$0xfff0000f,%esi
	xorl	%esi,%edi
	xorl	%esi,%eax

	roll	$12,%edi
	movl	%edi,%esi
	xorl	%eax,%edi
	andl	$0xf0f0f0f0,%edi
	xorl	%edi,%esi
	xorl	%edi,%eax

	rorl	$4,%eax
	movl	%eax,(%ebx)
	movl	%esi,4(%ebx)
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
.size	DES_decrypt3,.-.L_DES_decrypt3_begin
.globl	DES_ncbc_encrypt
.type	DES_ncbc_encrypt,@function
.align	16
DES_ncbc_encrypt:
.L_DES_ncbc_encrypt_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif


	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),%ebp

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

	movl	56(%esp),%ecx

	pushl	%ecx

	movl	52(%esp),%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,%ecx
	jz	.L006decrypt
	andl	$4294967288,%ebp
	movl	12(%esp),%eax
	movl	16(%esp),%ebx
	jz	.L007encrypt_finish
.L008encrypt_loop:
	movl	(%esi),%ecx
	movl	4(%esi),%edx
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	movl	%eax,12(%esp)
	movl	%ebx,16(%esp)
	call	.L_DES_encrypt1_begin
	movl	12(%esp),%eax
	movl	16(%esp),%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	.L008encrypt_loop
.L007encrypt_finish:
	movl	56(%esp),%ebp
	andl	$7,%ebp
	jz	.L009finish
	call	.L010PIC_point
.L010PIC_point:
	popl	%edx
	leal	.L011cbc_enc_jmp_table-.L010PIC_point(%edx),%ecx
	movl	(%ecx,%ebp,4),%ebp
	addl	%edx,%ebp
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	jmp	*%ebp
.L012ej7:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	6(%esi),%dh
	shll	$8,%edx
.L013ej6:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	5(%esi),%dh
.L014ej5:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	4(%esi),%dl
.L015ej4:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movl	(%esi),%ecx
	jmp	.L016ejend
.L017ej3:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	2(%esi),%ch
	shll	$8,%ecx
.L018ej2:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	1(%esi),%ch
.L019ej1:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	(%esi),%cl
.L016ejend:
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	movl	%eax,12(%esp)
	movl	%ebx,16(%esp)
	call	.L_DES_encrypt1_begin
	movl	12(%esp),%eax
	movl	16(%esp),%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	jmp	.L009finish
.L006decrypt:
	andl	$4294967288,%ebp
	movl	20(%esp),%eax
	movl	24(%esp),%ebx
	jz	.L020decrypt_finish
.L021decrypt_loop:
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%eax,12(%esp)
	movl	%ebx,16(%esp)
	call	.L_DES_encrypt1_begin
	movl	12(%esp),%eax
	movl	16(%esp),%ebx
	movl	20(%esp),%ecx
	movl	24(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%ecx,(%edi)
	movl	%edx,4(%edi)
	movl	%eax,20(%esp)
	movl	%ebx,24(%esp)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	.L021decrypt_loop
.L020decrypt_finish:
	movl	56(%esp),%ebp
	andl	$7,%ebp
	jz	.L009finish
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%eax,12(%esp)
	movl	%ebx,16(%esp)
	call	.L_DES_encrypt1_begin
	movl	12(%esp),%eax
	movl	16(%esp),%ebx
	movl	20(%esp),%ecx
	movl	24(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
.L022dj7:
	rorl	$16,%edx
	movb	%dl,6(%edi)
	shrl	$16,%edx
.L023dj6:
	movb	%dh,5(%edi)
.L024dj5:
	movb	%dl,4(%edi)
.L025dj4:
	movl	%ecx,(%edi)
	jmp	.L026djend
.L027dj3:
	rorl	$16,%ecx
	movb	%cl,2(%edi)
	shll	$16,%ecx
.L028dj2:
	movb	%ch,1(%esi)
.L029dj1:
	movb	%cl,(%esi)
.L026djend:
	jmp	.L009finish
.L009finish:
	movl	64(%esp),%ecx
	addl	$28,%esp
	movl	%eax,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.align	64
.L011cbc_enc_jmp_table:
.long	0
.long	.L019ej1-.L010PIC_point
.long	.L018ej2-.L010PIC_point
.long	.L017ej3-.L010PIC_point
.long	.L015ej4-.L010PIC_point
.long	.L014ej5-.L010PIC_point
.long	.L013ej6-.L010PIC_point
.long	.L012ej7-.L010PIC_point
.align	64
.size	DES_ncbc_encrypt,.-.L_DES_ncbc_encrypt_begin
.globl	DES_ede3_cbc_encrypt
.type	DES_ede3_cbc_encrypt,@function
.align	16
DES_ede3_cbc_encrypt:
.L_DES_ede3_cbc_encrypt_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif


	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),%ebp

	movl	44(%esp),%ebx
	movl	(%ebx),%esi
	movl	4(%ebx),%edi
	pushl	%edi
	pushl	%esi
	pushl	%edi
	pushl	%esi
	movl	%esp,%ebx
	movl	36(%esp),%esi
	movl	40(%esp),%edi

	movl	64(%esp),%ecx

	movl	56(%esp),%eax
	pushl	%eax

	movl	56(%esp),%eax
	pushl	%eax

	movl	56(%esp),%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,%ecx
	jz	.L030decrypt
	andl	$4294967288,%ebp
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	jz	.L031encrypt_finish
.L032encrypt_loop:
	movl	(%esi),%ecx
	movl	4(%esi),%edx
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	movl	%eax,16(%esp)
	movl	%ebx,20(%esp)
	call	.L_DES_encrypt3_begin
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	.L032encrypt_loop
.L031encrypt_finish:
	movl	60(%esp),%ebp
	andl	$7,%ebp
	jz	.L033finish
	call	.L034PIC_point
.L034PIC_point:
	popl	%edx
	leal	.L035cbc_enc_jmp_table-.L034PIC_point(%edx),%ecx
	movl	(%ecx,%ebp,4),%ebp
	addl	%edx,%ebp
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	jmp	*%ebp
.L036ej7:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	6(%esi),%dh
	shll	$8,%edx
.L037ej6:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	5(%esi),%dh
.L038ej5:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	4(%esi),%dl
.L039ej4:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movl	(%esi),%ecx
	jmp	.L040ejend
.L041ej3:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	2(%esi),%ch
	shll	$8,%ecx
.L042ej2:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	1(%esi),%ch
.L043ej1:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movb	(%esi),%cl
.L040ejend:
	xorl	%ecx,%eax
	xorl	%edx,%ebx
	movl	%eax,16(%esp)
	movl	%ebx,20(%esp)
	call	.L_DES_encrypt3_begin
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	jmp	.L033finish
.L030decrypt:
	andl	$4294967288,%ebp
	movl	24(%esp),%eax
	movl	28(%esp),%ebx
	jz	.L044decrypt_finish
.L045decrypt_loop:
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%eax,16(%esp)
	movl	%ebx,20(%esp)
	call	.L_DES_decrypt3_begin
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	movl	24(%esp),%ecx
	movl	28(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%ecx,(%edi)
	movl	%edx,4(%edi)
	movl	%eax,24(%esp)
	movl	%ebx,28(%esp)
	addl	$8,%esi
	addl	$8,%edi
	subl	$8,%ebp
	jnz	.L045decrypt_loop
.L044decrypt_finish:
	movl	60(%esp),%ebp
	andl	$7,%ebp
	jz	.L033finish
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	%eax,16(%esp)
	movl	%ebx,20(%esp)
	call	.L_DES_decrypt3_begin
	movl	16(%esp),%eax
	movl	20(%esp),%ebx
	movl	24(%esp),%ecx
	movl	28(%esp),%edx
	xorl	%eax,%ecx
	xorl	%ebx,%edx
	movl	(%esi),%eax
	movl	4(%esi),%ebx
.L046dj7:
	rorl	$16,%edx
	movb	%dl,6(%edi)
	shrl	$16,%edx
.L047dj6:
	movb	%dh,5(%edi)
.L048dj5:
	movb	%dl,4(%edi)
.L049dj4:
	movl	%ecx,(%edi)
	jmp	.L050djend
.L051dj3:
	rorl	$16,%ecx
	movb	%cl,2(%edi)
	shll	$16,%ecx
.L052dj2:
	movb	%ch,1(%esi)
.L053dj1:
	movb	%cl,(%esi)
.L050djend:
	jmp	.L033finish
.L033finish:
	movl	76(%esp),%ecx
	addl	$32,%esp
	movl	%eax,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.align	64
.L035cbc_enc_jmp_table:
.long	0
.long	.L043ej1-.L034PIC_point
.long	.L042ej2-.L034PIC_point
.long	.L041ej3-.L034PIC_point
.long	.L039ej4-.L034PIC_point
.long	.L038ej5-.L034PIC_point
.long	.L037ej6-.L034PIC_point
.long	.L036ej7-.L034PIC_point
.align	64
.size	DES_ede3_cbc_encrypt,.-.L_DES_ede3_cbc_encrypt_begin
.align	64
DES_SPtrans:
.Ldes_sptrans:
.long	34080768,524288,33554434,34080770
.long	33554432,526338,524290,33554434
.long	526338,34080768,34078720,2050
.long	33556482,33554432,0,524290
.long	524288,2,33556480,526336
.long	34080770,34078720,2050,33556480
.long	2,2048,526336,34078722
.long	2048,33556482,34078722,0
.long	0,34080770,33556480,524290
.long	34080768,524288,2050,33556480
.long	34078722,2048,526336,33554434
.long	526338,2,33554434,34078720
.long	34080770,526336,34078720,33556482
.long	33554432,2050,524290,0
.long	524288,33554432,33556482,34080768
.long	2,34078722,2048,526338
.long	1074823184,0,1081344,1074790400
.long	1073741840,32784,1073774592,1081344
.long	32768,1074790416,16,1073774592
.long	1048592,1074823168,1074790400,16
.long	1048576,1073774608,1074790416,32768
.long	1081360,1073741824,0,1048592
.long	1073774608,1081360,1074823168,1073741840
.long	1073741824,1048576,32784,1074823184
.long	1048592,1074823168,1073774592,1081360
.long	1074823184,1048592,1073741840,0
.long	1073741824,32784,1048576,1074790416
.long	32768,1073741824,1081360,1073774608
.long	1074823168,32768,0,1073741840
.long	16,1074823184,1081344,1074790400
.long	1074790416,1048576,32784,1073774592
.long	1073774608,16,1074790400,1081344
.long	67108865,67371264,256,67109121
.long	262145,67108864,67109121,262400
.long	67109120,262144,67371008,1
.long	67371265,257,1,67371009
.long	0,262145,67371264,256
.long	257,67371265,262144,67108865
.long	67371009,67109120,262401,67371008
.long	262400,0,67108864,262401
.long	67371264,256,1,262144
.long	257,262145,67371008,67109121
.long	0,67371264,262400,67371009
.long	262145,67108864,67371265,1
.long	262401,67108865,67108864,67371265
.long	262144,67109120,67109121,262400
.long	67109120,0,67371009,257
.long	67108865,262401,256,67371008
.long	4198408,268439552,8,272633864
.long	0,272629760,268439560,4194312
.long	272633856,268435464,268435456,4104
.long	268435464,4198408,4194304,268435456
.long	272629768,4198400,4096,8
.long	4198400,268439560,272629760,4096
.long	4104,0,4194312,272633856
.long	268439552,272629768,272633864,4194304
.long	272629768,4104,4194304,268435464
.long	4198400,268439552,8,272629760
.long	268439560,0,4096,4194312
.long	0,272629768,272633856,4096
.long	268435456,272633864,4198408,4194304
.long	272633864,8,268439552,4198408
.long	4194312,4198400,272629760,268439560
.long	4104,268435456,268435464,272633856
.long	134217728,65536,1024,134284320
.long	134283296,134218752,66592,134283264
.long	65536,32,134217760,66560
.long	134218784,134283296,134284288,0
.long	66560,134217728,65568,1056
.long	134218752,66592,0,134217760
.long	32,134218784,134284320,65568
.long	134283264,1024,1056,134284288
.long	134284288,134218784,65568,134283264
.long	65536,32,134217760,134218752
.long	134217728,66560,134284320,0
.long	66592,134217728,1024,65568
.long	134218784,1024,0,134284320
.long	134283296,134284288,1056,65536
.long	66560,134283296,134218752,1056
.long	32,66592,134283264,134217760
.long	2147483712,2097216,0,2149588992
.long	2097216,8192,2147491904,2097152
.long	8256,2149589056,2105344,2147483648
.long	2147491840,2147483712,2149580800,2105408
.long	2097152,2147491904,2149580864,0
.long	8192,64,2149588992,2149580864
.long	2149589056,2149580800,2147483648,8256
.long	64,2105344,2105408,2147491840
.long	8256,2147483648,2147491840,2105408
.long	2149588992,2097216,0,2147491840
.long	2147483648,8192,2149580864,2097152
.long	2097216,2149589056,2105344,64
.long	2149589056,2105344,2097152,2147491904
.long	2147483712,2149580800,2105408,0
.long	8192,2147483712,2147491904,2149588992
.long	2149580800,8256,64,2149580864
.long	16384,512,16777728,16777220
.long	16794116,16388,16896,0
.long	16777216,16777732,516,16793600
.long	4,16794112,16793600,516
.long	16777732,16384,16388,16794116
.long	0,16777728,16777220,16896
.long	16793604,16900,16794112,4
.long	16900,16793604,512,16777216
.long	16900,16793600,16793604,516
.long	16384,512,16777216,16793604
.long	16777732,16900,16896,0
.long	512,16777220,4,16777728
.long	0,16777732,16777728,16896
.long	516,16384,16794116,16777216
.long	16794112,4,16388,16794116
.long	16777220,16794112,16793600,16388
.long	545259648,545390592,131200,0
.long	537001984,8388736,545259520,545390720
.long	128,536870912,8519680,131200
.long	8519808,537002112,536871040,545259520
.long	131072,8519808,8388736,537001984
.long	545390720,536871040,0,8519680
.long	536870912,8388608,537002112,545259648
.long	8388608,131072,545390592,128
.long	8388608,131072,536871040,545390720
.long	131200,536870912,0,8519680
.long	545259648,537002112,537001984,8388736
.long	545390592,128,8388736,537001984
.long	545390720,8388608,545259520,536871040
.long	8519680,131200,537002112,545259520
.long	128,545390592,8519808,0
.long	536870912,545259648,131072,8519808

	.section ".note.gnu.property", "a"
	.p2align 2
	.long 1f - 0f
	.long 4f - 1f
	.long 5
0:
	.asciz "GNU"
1:
	.p2align 2
	.long 0xc0000002
	.long 3f - 2f
2:
	.long 3
3:
	.p2align 2
4:
