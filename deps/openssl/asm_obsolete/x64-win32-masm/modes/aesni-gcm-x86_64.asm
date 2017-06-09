OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

PUBLIC	aesni_gcm_encrypt

aesni_gcm_encrypt	PROC PUBLIC
	xor	eax,eax
	DB	0F3h,0C3h		;repret
aesni_gcm_encrypt	ENDP

PUBLIC	aesni_gcm_decrypt

aesni_gcm_decrypt	PROC PUBLIC
	xor	eax,eax
	DB	0F3h,0C3h		;repret
aesni_gcm_decrypt	ENDP

.text$	ENDS
END
