OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

PUBLIC	rsaz_avx2_eligible

rsaz_avx2_eligible	PROC PUBLIC
	xor	eax,eax
	DB	0F3h,0C3h		;repret
rsaz_avx2_eligible	ENDP

PUBLIC	rsaz_1024_sqr_avx2
PUBLIC	rsaz_1024_mul_avx2
PUBLIC	rsaz_1024_norm2red_avx2
PUBLIC	rsaz_1024_red2norm_avx2
PUBLIC	rsaz_1024_scatter5_avx2
PUBLIC	rsaz_1024_gather5_avx2

rsaz_1024_sqr_avx2	PROC PUBLIC
rsaz_1024_mul_avx2::
rsaz_1024_norm2red_avx2::
rsaz_1024_red2norm_avx2::
rsaz_1024_scatter5_avx2::
rsaz_1024_gather5_avx2::
DB	00fh,00bh
	DB	0F3h,0C3h		;repret
rsaz_1024_sqr_avx2	ENDP

.text$	ENDS
END
