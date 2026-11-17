.text	

.globl	ossl_aes_cbc_vaes_cleanup_eligible
.hidden	ossl_aes_cbc_vaes_cleanup_eligible
.type	ossl_aes_cbc_vaes_cleanup_eligible,@function
.align	16
ossl_aes_cbc_vaes_cleanup_eligible:
.cfi_startproc	
.byte	243,15,30,250
	movl	$1,%eax
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_aes_cbc_vaes_cleanup_eligible,.-ossl_aes_cbc_vaes_cleanup_eligible







.globl	ossl_aes_cbc_vaes_cleanup
.hidden	ossl_aes_cbc_vaes_cleanup
.type	ossl_aes_cbc_vaes_cleanup,@function
.align	32
ossl_aes_cbc_vaes_cleanup:
.cfi_startproc	
.byte	243,15,30,250
	vpxord	%zmm0,%zmm0,%zmm0
	testq	%rsi,%rsi
	jz	.Lvaes_cbc_clear_registers

.Lvaes_cbc_clear_keys:
	vmovdqu64	%zmm0,(%rdi)
	addq	$64,%rdi
	decq	%rsi
	jnz	.Lvaes_cbc_clear_keys

.Lvaes_cbc_clear_registers:
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
.cfi_endproc	
.size	ossl_aes_cbc_vaes_cleanup,.-ossl_aes_cbc_vaes_cleanup
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
