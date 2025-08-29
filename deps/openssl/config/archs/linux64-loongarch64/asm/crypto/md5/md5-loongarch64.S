.text

.globl ossl_md5_block_asm_data_order
.type ossl_md5_block_asm_data_order function
ossl_md5_block_asm_data_order:
	# $r4 = arg #1 (ctx, MD5_CTX pointer)
	# $r5 = arg #2 (ptr, data pointer)
	# $r6 = arg #3 (nbr, number of 16-word blocks to process)
	beqz	$r6,.Lend		# cmp nbr with 0, jmp if nbr == 0

	# ptr is '$r5'
	# end is '$r7'
	slli.d	$r12,$r6,6
	add.d	$r7,$r5,$r12

	# A is '$r8'
	# B is '$r9'
	# C is '$r10'
	# D is '$r11'
	ld.w	$r8,$r4,0	# a4 = ctx->A
	ld.w	$r9,$r4,4	# a5 = ctx->B
	ld.w	$r10,$r4,8	# a6 = ctx->C
	ld.w	$r11,$r4,12	# a7 = ctx->D

# BEGIN of loop over 16-word blocks
.align 6
.Lloop:
	# save old values of A, B, C, D
	move	$r15,$r8
	move	$r16,$r9
	move	$r17,$r10
	move	$r18,$r11

	preld	0,$r5,0
	preld	0,$r5,64
	ld.w	$r12,$r5,0		/* (NEXT STEP) X[0] */
	xor	$r13,$r10,$r11		/* y ^ z */
	add.w	$r14,$r8,$r12		/* dst + X[k] */
	lu12i.w	$r20,-166230			/* load bits [31:12] of constant */
	and     $r13,$r9,$r13			/* x & ... */
	ori	$r20,$r20,1144			/* load bits [11:0] of constant */
	xor     $r13,$r11,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,1*4		/* (NEXT STEP) X[1] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[1] */
	rotri.w	$r8,$r8,32-7		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w   $r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-95109			/* load bits [31:12] of constant */
	and     $r13,$r8,$r13			/* x & ... */
	ori	$r20,$r20,1878			/* load bits [11:0] of constant */
	xor     $r13,$r10,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,2*4		/* (NEXT STEP) X[2] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[2] */
	rotri.w	$r11,$r11,32-12		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w   $r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,147975			/* load bits [31:12] of constant */
	and     $r13,$r11,$r13			/* x & ... */
	ori	$r20,$r20,219			/* load bits [11:0] of constant */
	xor     $r13,$r9,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,3*4		/* (NEXT STEP) X[3] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[3] */
	rotri.w	$r10,$r10,32-17		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w   $r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-255012			/* load bits [31:12] of constant */
	and     $r13,$r10,$r13			/* x & ... */
	ori	$r20,$r20,3822			/* load bits [11:0] of constant */
	xor     $r13,$r8,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,4*4		/* (NEXT STEP) X[4] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[4] */
	rotri.w	$r9,$r9,32-22		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w   $r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-43072			/* load bits [31:12] of constant */
	and     $r13,$r9,$r13			/* x & ... */
	ori	$r20,$r20,4015			/* load bits [11:0] of constant */
	xor     $r13,$r11,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,5*4		/* (NEXT STEP) X[5] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[5] */
	rotri.w	$r8,$r8,32-7		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w   $r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,292988			/* load bits [31:12] of constant */
	and     $r13,$r8,$r13			/* x & ... */
	ori	$r20,$r20,1578			/* load bits [11:0] of constant */
	xor     $r13,$r10,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,6*4		/* (NEXT STEP) X[6] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[6] */
	rotri.w	$r11,$r11,32-12		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w   $r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-359676			/* load bits [31:12] of constant */
	and     $r13,$r11,$r13			/* x & ... */
	ori	$r20,$r20,1555			/* load bits [11:0] of constant */
	xor     $r13,$r9,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,7*4		/* (NEXT STEP) X[7] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[7] */
	rotri.w	$r10,$r10,32-17		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w   $r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-11159			/* load bits [31:12] of constant */
	and     $r13,$r10,$r13			/* x & ... */
	ori	$r20,$r20,1281			/* load bits [11:0] of constant */
	xor     $r13,$r8,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,8*4		/* (NEXT STEP) X[8] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[8] */
	rotri.w	$r9,$r9,32-22		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w   $r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,432137			/* load bits [31:12] of constant */
	and     $r13,$r9,$r13			/* x & ... */
	ori	$r20,$r20,2264			/* load bits [11:0] of constant */
	xor     $r13,$r11,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,9*4		/* (NEXT STEP) X[9] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[9] */
	rotri.w	$r8,$r8,32-7		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w   $r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-478129			/* load bits [31:12] of constant */
	and     $r13,$r8,$r13			/* x & ... */
	ori	$r20,$r20,1967			/* load bits [11:0] of constant */
	xor     $r13,$r10,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,10*4		/* (NEXT STEP) X[10] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[10] */
	rotri.w	$r11,$r11,32-12		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w   $r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-11			/* load bits [31:12] of constant */
	and     $r13,$r11,$r13			/* x & ... */
	ori	$r20,$r20,2993			/* load bits [11:0] of constant */
	xor     $r13,$r9,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,11*4		/* (NEXT STEP) X[11] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[11] */
	rotri.w	$r10,$r10,32-17		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w   $r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-485939			/* load bits [31:12] of constant */
	and     $r13,$r10,$r13			/* x & ... */
	ori	$r20,$r20,1982			/* load bits [11:0] of constant */
	xor     $r13,$r8,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,12*4		/* (NEXT STEP) X[12] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[12] */
	rotri.w	$r9,$r9,32-22		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w   $r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,440577			/* load bits [31:12] of constant */
	and     $r13,$r9,$r13			/* x & ... */
	ori	$r20,$r20,290			/* load bits [11:0] of constant */
	xor     $r13,$r11,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,13*4		/* (NEXT STEP) X[13] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[13] */
	rotri.w	$r8,$r8,32-7		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w   $r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-9849			/* load bits [31:12] of constant */
	and     $r13,$r8,$r13			/* x & ... */
	ori	$r20,$r20,403			/* load bits [11:0] of constant */
	xor     $r13,$r10,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,14*4		/* (NEXT STEP) X[14] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[14] */
	rotri.w	$r11,$r11,32-12		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w   $r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-366700			/* load bits [31:12] of constant */
	and     $r13,$r11,$r13			/* x & ... */
	ori	$r20,$r20,910			/* load bits [11:0] of constant */
	xor     $r13,$r9,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,15*4		/* (NEXT STEP) X[15] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[15] */
	rotri.w	$r10,$r10,32-17		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w   $r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,301888			/* load bits [31:12] of constant */
	and     $r13,$r10,$r13			/* x & ... */
	ori	$r20,$r20,2081			/* load bits [11:0] of constant */
	xor     $r13,$r8,$r13			/* z ^ ... */
	add.w   $r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,1*4		/* (NEXT STEP) X[1] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[1] */
	rotri.w	$r9,$r9,32-22		/* dst <<< s */
	move	$r12,$r11		/* (NEXT ROUND) $r12 = z' (copy of z) */
	nor	$r13,$r0,$r11	/* (NEXT ROUND) $r13 = not z' (copy of not z) */
	add.w   $r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-40478			/* load bits [31:12] of Constant */
	and	$r12,$r9,$r12			/* x & z */
	ori	$r20,$r20,1378			/* load bits [11:0] of Constant */
	and	$r13,$r10,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,6*4		/* (NEXT STEP) X[6] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[6] */
	rotri.w $r8,$r8,32-5		/* dst <<< s */
	move	$r12,$r10		/* (NEXT STEP) z' = $r10 */
	nor	$r13,$r0,$r10	/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-261109			/* load bits [31:12] of Constant */
	and	$r12,$r8,$r12			/* x & z */
	ori	$r20,$r20,832			/* load bits [11:0] of Constant */
	and	$r13,$r9,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,11*4		/* (NEXT STEP) X[11] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[11] */
	rotri.w $r11,$r11,32-9		/* dst <<< s */
	move	$r12,$r9		/* (NEXT STEP) z' = $r9 */
	nor	$r13,$r0,$r9	/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,157157			/* load bits [31:12] of Constant */
	and	$r12,$r11,$r12			/* x & z */
	ori	$r20,$r20,2641			/* load bits [11:0] of Constant */
	and	$r13,$r8,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,0*4		/* (NEXT STEP) X[0] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[0] */
	rotri.w $r10,$r10,32-14		/* dst <<< s */
	move	$r12,$r8		/* (NEXT STEP) z' = $r8 */
	nor	$r13,$r0,$r8	/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-91284			/* load bits [31:12] of Constant */
	and	$r12,$r10,$r12			/* x & z */
	ori	$r20,$r20,1962			/* load bits [11:0] of Constant */
	and	$r13,$r11,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,5*4		/* (NEXT STEP) X[5] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[5] */
	rotri.w $r9,$r9,32-20		/* dst <<< s */
	move	$r12,$r11		/* (NEXT STEP) z' = $r11 */
	nor	$r13,$r0,$r11	/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-171279			/* load bits [31:12] of Constant */
	and	$r12,$r9,$r12			/* x & z */
	ori	$r20,$r20,93			/* load bits [11:0] of Constant */
	and	$r13,$r10,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,10*4		/* (NEXT STEP) X[10] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[10] */
	rotri.w $r8,$r8,32-5		/* dst <<< s */
	move	$r12,$r10		/* (NEXT STEP) z' = $r10 */
	nor	$r13,$r0,$r10	/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,9281			/* load bits [31:12] of Constant */
	and	$r12,$r8,$r12			/* x & z */
	ori	$r20,$r20,1107			/* load bits [11:0] of Constant */
	and	$r13,$r9,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,15*4		/* (NEXT STEP) X[15] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[15] */
	rotri.w $r11,$r11,32-9		/* dst <<< s */
	move	$r12,$r9		/* (NEXT STEP) z' = $r9 */
	nor	$r13,$r0,$r9	/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-161250			/* load bits [31:12] of Constant */
	and	$r12,$r11,$r12			/* x & z */
	ori	$r20,$r20,1665			/* load bits [11:0] of Constant */
	and	$r13,$r8,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,4*4		/* (NEXT STEP) X[4] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[4] */
	rotri.w $r10,$r10,32-14		/* dst <<< s */
	move	$r12,$r8		/* (NEXT STEP) z' = $r8 */
	nor	$r13,$r0,$r8	/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-99009			/* load bits [31:12] of Constant */
	and	$r12,$r10,$r12			/* x & z */
	ori	$r20,$r20,3016			/* load bits [11:0] of Constant */
	and	$r13,$r11,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,9*4		/* (NEXT STEP) X[9] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[9] */
	rotri.w $r9,$r9,32-20		/* dst <<< s */
	move	$r12,$r11		/* (NEXT STEP) z' = $r11 */
	nor	$r13,$r0,$r11	/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,138780			/* load bits [31:12] of Constant */
	and	$r12,$r9,$r12			/* x & z */
	ori	$r20,$r20,3558			/* load bits [11:0] of Constant */
	and	$r13,$r10,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,14*4		/* (NEXT STEP) X[14] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[14] */
	rotri.w $r8,$r8,32-5		/* dst <<< s */
	move	$r12,$r10		/* (NEXT STEP) z' = $r10 */
	nor	$r13,$r0,$r10	/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-248976			/* load bits [31:12] of Constant */
	and	$r12,$r8,$r12			/* x & z */
	ori	$r20,$r20,2006			/* load bits [11:0] of Constant */
	and	$r13,$r9,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,3*4		/* (NEXT STEP) X[3] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[3] */
	rotri.w $r11,$r11,32-9		/* dst <<< s */
	move	$r12,$r9		/* (NEXT STEP) z' = $r9 */
	nor	$r13,$r0,$r9	/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-45744			/* load bits [31:12] of Constant */
	and	$r12,$r11,$r12			/* x & z */
	ori	$r20,$r20,3463			/* load bits [11:0] of Constant */
	and	$r13,$r8,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,8*4		/* (NEXT STEP) X[8] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[8] */
	rotri.w $r10,$r10,32-14		/* dst <<< s */
	move	$r12,$r8		/* (NEXT STEP) z' = $r8 */
	nor	$r13,$r0,$r8	/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,284065			/* load bits [31:12] of Constant */
	and	$r12,$r10,$r12			/* x & z */
	ori	$r20,$r20,1261			/* load bits [11:0] of Constant */
	and	$r13,$r11,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,13*4		/* (NEXT STEP) X[13] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[13] */
	rotri.w $r9,$r9,32-20		/* dst <<< s */
	move	$r12,$r11		/* (NEXT STEP) z' = $r11 */
	nor	$r13,$r0,$r11	/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-352706			/* load bits [31:12] of Constant */
	and	$r12,$r9,$r12			/* x & z */
	ori	$r20,$r20,2309			/* load bits [11:0] of Constant */
	and	$r13,$r10,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,2*4		/* (NEXT STEP) X[2] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[2] */
	rotri.w $r8,$r8,32-5		/* dst <<< s */
	move	$r12,$r10		/* (NEXT STEP) z' = $r10 */
	nor	$r13,$r0,$r10	/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-12550			/* load bits [31:12] of Constant */
	and	$r12,$r8,$r12			/* x & z */
	ori	$r20,$r20,1016			/* load bits [11:0] of Constant */
	and	$r13,$r9,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,7*4		/* (NEXT STEP) X[7] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[7] */
	rotri.w $r11,$r11,32-9		/* dst <<< s */
	move	$r12,$r9		/* (NEXT STEP) z' = $r9 */
	nor	$r13,$r0,$r9	/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,423664			/* load bits [31:12] of Constant */
	and	$r12,$r11,$r12			/* x & z */
	ori	$r20,$r20,729			/* load bits [11:0] of Constant */
	and	$r13,$r8,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,12*4		/* (NEXT STEP) X[12] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[12] */
	rotri.w $r10,$r10,32-14		/* dst <<< s */
	move	$r12,$r8		/* (NEXT STEP) z' = $r8 */
	nor	$r13,$r0,$r8	/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-470364			/* load bits [31:12] of Constant */
	and	$r12,$r10,$r12			/* x & z */
	ori	$r20,$r20,3210			/* load bits [11:0] of Constant */
	and	$r13,$r11,$r13			/* y & (not z) */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	or	$r13,$r12,$r13			/* (y & (not z)) | (x & z) */
	ld.w	$r12,$r5,5*4		/* (NEXT STEP) X[5] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[5] */
	rotri.w $r9,$r9,32-20		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT ROUND) $r13 = y ^ z */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-93			/* load bits [31:12] of Constant */
	xor	$r13,$r9,$r13			/* x ^ ... */
	ori	$r20,$r20,2370			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,8*4		/* (NEXT STEP) X[8] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[8] */
	rotri.w $r8,$r8,32-4		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-493793			/* load bits [31:12] of Constant */
	xor	$r13,$r8,$r13			/* x ^ ... */
	ori	$r20,$r20,1665			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,11*4		/* (NEXT STEP) X[11] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[11] */
	rotri.w $r11,$r11,32-11		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,448982			/* load bits [31:12] of Constant */
	xor	$r13,$r11,$r13			/* x ^ ... */
	ori	$r20,$r20,290			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,14*4		/* (NEXT STEP) X[14] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[14] */
	rotri.w $r10,$r10,32-16		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-8621			/* load bits [31:12] of Constant */
	xor	$r13,$r10,$r13			/* x ^ ... */
	ori	$r20,$r20,2060			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,1*4		/* (NEXT STEP) X[1] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[1] */
	rotri.w $r9,$r9,32-23		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-373778			/* load bits [31:12] of Constant */
	xor	$r13,$r9,$r13			/* x ^ ... */
	ori	$r20,$r20,2628			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,4*4		/* (NEXT STEP) X[4] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[4] */
	rotri.w $r8,$r8,32-4		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,310764			/* load bits [31:12] of Constant */
	xor	$r13,$r8,$r13			/* x ^ ... */
	ori	$r20,$r20,4009			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,7*4		/* (NEXT STEP) X[7] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[7] */
	rotri.w $r11,$r11,32-11		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-37964			/* load bits [31:12] of Constant */
	xor	$r13,$r11,$r13			/* x ^ ... */
	ori	$r20,$r20,2912			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,10*4		/* (NEXT STEP) X[10] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[10] */
	rotri.w $r10,$r10,32-16		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-267269			/* load bits [31:12] of Constant */
	xor	$r13,$r10,$r13			/* x ^ ... */
	ori	$r20,$r20,3184			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,13*4		/* (NEXT STEP) X[13] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[13] */
	rotri.w $r9,$r9,32-23		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,166327			/* load bits [31:12] of Constant */
	xor	$r13,$r9,$r13			/* x ^ ... */
	ori	$r20,$r20,3782			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,0*4		/* (NEXT STEP) X[0] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[0] */
	rotri.w $r8,$r8,32-4		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-87534			/* load bits [31:12] of Constant */
	xor	$r13,$r8,$r13			/* x ^ ... */
	ori	$r20,$r20,2042			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,3*4		/* (NEXT STEP) X[3] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[3] */
	rotri.w $r11,$r11,32-11		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,-176397			/* load bits [31:12] of Constant */
	xor	$r13,$r11,$r13			/* x ^ ... */
	ori	$r20,$r20,133			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,6*4		/* (NEXT STEP) X[6] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[6] */
	rotri.w $r10,$r10,32-16		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,18561			/* load bits [31:12] of Constant */
	xor	$r13,$r10,$r13			/* x ^ ... */
	ori	$r20,$r20,3333			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,9*4		/* (NEXT STEP) X[9] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[9] */
	rotri.w $r9,$r9,32-23		/* dst <<< s */
	xor	$r13,$r10,$r11	/* (NEXT STEP) y ^ z */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-156339			/* load bits [31:12] of Constant */
	xor	$r13,$r9,$r13			/* x ^ ... */
	ori	$r20,$r20,57			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,12*4		/* (NEXT STEP) X[12] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[12] */
	rotri.w $r8,$r8,32-4		/* dst <<< s */
	xor	$r13,$r9,$r10	/* (NEXT STEP) y ^ z */
	add.w	$r8,$r8,$r9		/* dst += x */
	lu12i.w	$r20,-102983			/* load bits [31:12] of Constant */
	xor	$r13,$r8,$r13			/* x ^ ... */
	ori	$r20,$r20,2533			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,15*4		/* (NEXT STEP) X[15] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[15] */
	rotri.w $r11,$r11,32-11		/* dst <<< s */
	xor	$r13,$r8,$r9	/* (NEXT STEP) y ^ z */
	add.w	$r11,$r11,$r8		/* dst += x */
	lu12i.w	$r20,129575			/* load bits [31:12] of Constant */
	xor	$r13,$r11,$r13			/* x ^ ... */
	ori	$r20,$r20,3320			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,2*4		/* (NEXT STEP) X[2] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[2] */
	rotri.w $r10,$r10,32-16		/* dst <<< s */
	xor	$r13,$r11,$r8	/* (NEXT STEP) y ^ z */
	add.w	$r10,$r10,$r11		/* dst += x */
	lu12i.w	$r20,-243003			/* load bits [31:12] of Constant */
	xor	$r13,$r10,$r13			/* x ^ ... */
	ori	$r20,$r20,1637			/* load bits [11:0] of Constant */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,0*4		/* (NEXT STEP) X[0] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[0] */
	rotri.w $r9,$r9,32-23		/* dst <<< s */
	nor	$r13,$r0,$r11	/* (NEXT ROUND) $r13 = not z */
	add.w	$r9,$r9,$r10		/* dst += x */
	lu12i.w	$r20,-48494			/* load bits [31:12] of Constant */
	or	$r13,$r9,$r13			/* x | ... */
	ori	$r20,$r20,580			/* load bits [11:0] of Constant */
	xor	$r13,$r10,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,7*4		/* (NEXT STEP) X[7] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[7] */
	rotri.w $r8,$r8,32-6			/* dst <<< s */
	nor	$r13,$r0,$r10			/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9			/* dst += x */
	lu12i.w	$r20,275119			/* load bits [31:12] of Constant */
	or	$r13,$r8,$r13			/* x | ... */
	ori	$r20,$r20,3991			/* load bits [11:0] of Constant */
	xor	$r13,$r9,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,14*4		/* (NEXT STEP) X[14] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[14] */
	rotri.w $r11,$r11,32-10			/* dst <<< s */
	nor	$r13,$r0,$r9			/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8			/* dst += x */
	lu12i.w	$r20,-345790			/* load bits [31:12] of Constant */
	or	$r13,$r11,$r13			/* x | ... */
	ori	$r20,$r20,935			/* load bits [11:0] of Constant */
	xor	$r13,$r8,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,5*4		/* (NEXT STEP) X[5] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[5] */
	rotri.w $r10,$r10,32-15			/* dst <<< s */
	nor	$r13,$r0,$r8			/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11			/* dst += x */
	lu12i.w	$r20,-14022			/* load bits [31:12] of Constant */
	or	$r13,$r10,$r13			/* x | ... */
	ori	$r20,$r20,57			/* load bits [11:0] of Constant */
	xor	$r13,$r11,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,12*4		/* (NEXT STEP) X[12] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[12] */
	rotri.w $r9,$r9,32-21			/* dst <<< s */
	nor	$r13,$r0,$r11			/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10			/* dst += x */
	lu12i.w	$r20,415157			/* load bits [31:12] of Constant */
	or	$r13,$r9,$r13			/* x | ... */
	ori	$r20,$r20,2499			/* load bits [11:0] of Constant */
	xor	$r13,$r10,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,3*4		/* (NEXT STEP) X[3] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[3] */
	rotri.w $r8,$r8,32-6			/* dst <<< s */
	nor	$r13,$r0,$r10			/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9			/* dst += x */
	lu12i.w	$r20,-462644			/* load bits [31:12] of Constant */
	or	$r13,$r8,$r13			/* x | ... */
	ori	$r20,$r20,3218			/* load bits [11:0] of Constant */
	xor	$r13,$r9,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,10*4		/* (NEXT STEP) X[10] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[10] */
	rotri.w $r11,$r11,32-10			/* dst <<< s */
	nor	$r13,$r0,$r9			/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8			/* dst += x */
	lu12i.w	$r20,-257			/* load bits [31:12] of Constant */
	or	$r13,$r11,$r13			/* x | ... */
	ori	$r20,$r20,1149			/* load bits [11:0] of Constant */
	xor	$r13,$r8,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,1*4		/* (NEXT STEP) X[1] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[1] */
	rotri.w $r10,$r10,32-15			/* dst <<< s */
	nor	$r13,$r0,$r8			/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11			/* dst += x */
	lu12i.w	$r20,-501691			/* load bits [31:12] of Constant */
	or	$r13,$r10,$r13			/* x | ... */
	ori	$r20,$r20,3537			/* load bits [11:0] of Constant */
	xor	$r13,$r11,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,8*4		/* (NEXT STEP) X[8] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[8] */
	rotri.w $r9,$r9,32-21			/* dst <<< s */
	nor	$r13,$r0,$r11			/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10			/* dst += x */
	lu12i.w	$r20,457351			/* load bits [31:12] of Constant */
	or	$r13,$r9,$r13			/* x | ... */
	ori	$r20,$r20,3663			/* load bits [11:0] of Constant */
	xor	$r13,$r10,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,15*4		/* (NEXT STEP) X[15] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[15] */
	rotri.w $r8,$r8,32-6			/* dst <<< s */
	nor	$r13,$r0,$r10			/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9			/* dst += x */
	lu12i.w	$r20,-7474			/* load bits [31:12] of Constant */
	or	$r13,$r8,$r13			/* x | ... */
	ori	$r20,$r20,1760			/* load bits [11:0] of Constant */
	xor	$r13,$r9,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,6*4		/* (NEXT STEP) X[6] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[6] */
	rotri.w $r11,$r11,32-10			/* dst <<< s */
	nor	$r13,$r0,$r9			/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8			/* dst += x */
	lu12i.w	$r20,-380908			/* load bits [31:12] of Constant */
	or	$r13,$r11,$r13			/* x | ... */
	ori	$r20,$r20,788			/* load bits [11:0] of Constant */
	xor	$r13,$r8,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,13*4		/* (NEXT STEP) X[13] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[13] */
	rotri.w $r10,$r10,32-15			/* dst <<< s */
	nor	$r13,$r0,$r8			/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11			/* dst += x */
	lu12i.w	$r20,319617			/* load bits [31:12] of Constant */
	or	$r13,$r10,$r13			/* x | ... */
	ori	$r20,$r20,417			/* load bits [11:0] of Constant */
	xor	$r13,$r11,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,4*4		/* (NEXT STEP) X[4] */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r14,$r8,$r12			/* (NEXT STEP) dst + X[4] */
	rotri.w $r9,$r9,32-21			/* dst <<< s */
	nor	$r13,$r0,$r11			/* (NEXT STEP) not z' = not $r11 */
	add.w	$r9,$r9,$r10			/* dst += x */
	lu12i.w	$r20,-35529			/* load bits [31:12] of Constant */
	or	$r13,$r9,$r13			/* x | ... */
	ori	$r20,$r20,3714			/* load bits [11:0] of Constant */
	xor	$r13,$r10,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,11*4		/* (NEXT STEP) X[11] */
	add.w	$r8,$r19,$r13			/* dst += ... */
	add.w	$r14,$r11,$r12			/* (NEXT STEP) dst + X[11] */
	rotri.w $r8,$r8,32-6			/* dst <<< s */
	nor	$r13,$r0,$r10			/* (NEXT STEP) not z' = not $r10 */
	add.w	$r8,$r8,$r9			/* dst += x */
	lu12i.w	$r20,-273489			/* load bits [31:12] of Constant */
	or	$r13,$r8,$r13			/* x | ... */
	ori	$r20,$r20,565			/* load bits [11:0] of Constant */
	xor	$r13,$r9,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,2*4		/* (NEXT STEP) X[2] */
	add.w	$r11,$r19,$r13			/* dst += ... */
	add.w	$r14,$r10,$r12			/* (NEXT STEP) dst + X[2] */
	rotri.w $r11,$r11,32-10			/* dst <<< s */
	nor	$r13,$r0,$r9			/* (NEXT STEP) not z' = not $r9 */
	add.w	$r11,$r11,$r8			/* dst += x */
	lu12i.w	$r20,175485			/* load bits [31:12] of Constant */
	or	$r13,$r11,$r13			/* x | ... */
	ori	$r20,$r20,699			/* load bits [11:0] of Constant */
	xor	$r13,$r8,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	ld.w	$r12,$r5,9*4		/* (NEXT STEP) X[9] */
	add.w	$r10,$r19,$r13			/* dst += ... */
	add.w	$r14,$r9,$r12			/* (NEXT STEP) dst + X[9] */
	rotri.w $r10,$r10,32-15			/* dst <<< s */
	nor	$r13,$r0,$r8			/* (NEXT STEP) not z' = not $r8 */
	add.w	$r10,$r10,$r11			/* dst += x */
	lu12i.w	$r20,-83859			/* load bits [31:12] of Constant */
	or	$r13,$r10,$r13			/* x | ... */
	ori	$r20,$r20,913			/* load bits [11:0] of Constant */
	xor	$r13,$r11,$r13			/* y ^ ... */
	add.w	$r19,$r14,$r20			/* dst + X[k] + Const */
	add.w	$r8,$r15,$r8			/* (NEXT LOOP) add old value of A */
	add.w	$r9,$r19,$r13			/* dst += ... */
	add.w	$r11,$r18,$r11			/* (NEXT LOOP) add old value of D */
	rotri.w $r9,$r9,32-21			/* dst <<< s */
	addi.d	$r5,$r5,64			/* (NEXT LOOP) ptr += 64 */
	add.w	$r9,$r9,$r10			/* dst += x */
	# add old values of B, C
	add.w	$r9,$r16,$r9
	add.w	$r10,$r17,$r10

	bltu	$r5,$r7,.Lloop	# jmp if ptr < end

	st.w	$r8,$r4,0	# ctx->A = A
	st.w	$r9,$r4,4	# ctx->B = B
	st.w	$r10,$r4,8	# ctx->C = C
	st.w	$r11,$r4,12	# ctx->D = D

.Lend:
	jr	$r1
.size ossl_md5_block_asm_data_order,.-ossl_md5_block_asm_data_order
