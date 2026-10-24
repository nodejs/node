.text	

.globl	ossl_aes_cbc_vaes_cleanup_eligible
.hidden	ossl_aes_cbc_vaes_cleanup_eligible
.type	ossl_aes_cbc_vaes_cleanup_eligible,@function
.align	16
ossl_aes_cbc_vaes_cleanup_eligible:
.cfi_startproc	
.byte	243,15,30,250
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_aes_cbc_vaes_cleanup_eligible,.-ossl_aes_cbc_vaes_cleanup_eligible

.globl	ossl_aes_cbc_vaes_cleanup
.hidden	ossl_aes_cbc_vaes_cleanup
.type	ossl_aes_cbc_vaes_cleanup,@function
ossl_aes_cbc_vaes_cleanup:
.cfi_startproc	
.byte	243,15,30,250
.byte	0x0f,0x0b
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
