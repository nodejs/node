.text

.globl	aesni_gcm_encrypt
.type	aesni_gcm_encrypt,@function
aesni_gcm_encrypt:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	aesni_gcm_encrypt,.-aesni_gcm_encrypt

.globl	aesni_gcm_decrypt
.type	aesni_gcm_decrypt,@function
aesni_gcm_decrypt:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	aesni_gcm_decrypt,.-aesni_gcm_decrypt
