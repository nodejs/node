.text	

.globl	_ossl_aes_cbc_vaes_cleanup_eligible
.private_extern	_ossl_aes_cbc_vaes_cleanup_eligible

.p2align	4
_ossl_aes_cbc_vaes_cleanup_eligible:

.byte	243,15,30,250
	movl	$1,%eax
	.byte	0xf3,0xc3









.globl	_ossl_aes_cbc_vaes_cleanup
.private_extern	_ossl_aes_cbc_vaes_cleanup

.p2align	5
_ossl_aes_cbc_vaes_cleanup:

.byte	243,15,30,250
	vpxord	%zmm0,%zmm0,%zmm0
	testq	%rsi,%rsi
	jz	L$vaes_cbc_clear_registers

L$vaes_cbc_clear_keys:
	vmovdqu64	%zmm0,(%rdi)
	addq	$64,%rdi
	decq	%rsi
	jnz	L$vaes_cbc_clear_keys

L$vaes_cbc_clear_registers:
	vpxord	%zmm16,%zmm16,%zmm16
	vpxord	%zmm17,%zmm17,%zmm17
	vpxord	%zmm18,%zmm18,%zmm18
	vpxord	%zmm19,%zmm19,%zmm19
	vpxord	%zmm20,%zmm20,%zmm20
	vpxord	%zmm21,%zmm21,%zmm21
	vpxord	%zmm22,%zmm22,%zmm22
	vpxord	%zmm23,%zmm23,%zmm23
	vpxord	%zmm24,%zmm24,%zmm24
	vpxord	%zmm25,%zmm25,%zmm25
	vpxord	%zmm26,%zmm26,%zmm26
	vpxord	%zmm27,%zmm27,%zmm27
	vpxord	%zmm28,%zmm28,%zmm28
	vpxord	%zmm29,%zmm29,%zmm29
	vpxord	%zmm30,%zmm30,%zmm30
	vpxord	%zmm31,%zmm31,%zmm31

	vzeroall


	xorl	%eax,%eax
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	xorl	%esi,%esi
	xorl	%edi,%edi
	xorl	%r8d,%r8d
	xorl	%r9d,%r9d
	xorl	%r10d,%r10d
	xorl	%r11d,%r11d
	.byte	0xf3,0xc3


