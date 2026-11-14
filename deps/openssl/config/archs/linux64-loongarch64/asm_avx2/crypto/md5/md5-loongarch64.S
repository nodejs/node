.text

.globl ossl_md5_block_asm_data_order
.type ossl_md5_block_asm_data_order function
ossl_md5_block_asm_data_order:
	# $a0 = arg #1 (ctx, MD5_CTX pointer)
	# $a1 = arg #2 (ptr, data pointer)
	# $a2 = arg #3 (nbr, number of 16-word blocks to process)
	beqz	$a2,.Lend		# cmp nbr with 0, jmp if nbr == 0

	# ptr is '$a1'
	# end is '$a3'
	slli.d	$t0,$a2,6
	add.d	$a3,$a1,$t0

	# A is '$a4'
	# B is '$a5'
	# C is '$a6'
	# D is '$a7'
	ld.w	$a4,$a0,0	# a4 = ctx->A
	ld.w	$a5,$a0,4	# a5 = ctx->B
	ld.w	$a6,$a0,8	# a6 = ctx->C
	ld.w	$a7,$a0,12	# a7 = ctx->D

# BEGIN of loop over 16-word blocks
.align 6
.Lloop:
	# save old values of A, B, C, D
	move	$t3,$a4
	move	$t4,$a5
	move	$t5,$a6
	move	$t6,$a7

	preld	0,$a1,0
	preld	0,$a1,64
	ld.w	$t0,$a1,0		/* (NEXT STEP) X[0] */
	xor	$t1,$a6,$a7		/* y ^ z */
	add.w	$t2,$a4,$t0		/* dst + X[k] */
	lu12i.w	$t8,-166230			/* load bits [31:12] of constant */
	and     $t1,$a5,$t1			/* x & ... */
	ori	$t8,$t8,1144			/* load bits [11:0] of constant */
	xor     $t1,$a7,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,1*4		/* (NEXT STEP) X[1] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[1] */
	rotri.w	$a4,$a4,32-7		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w   $a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-95109			/* load bits [31:12] of constant */
	and     $t1,$a4,$t1			/* x & ... */
	ori	$t8,$t8,1878			/* load bits [11:0] of constant */
	xor     $t1,$a6,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,2*4		/* (NEXT STEP) X[2] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[2] */
	rotri.w	$a7,$a7,32-12		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w   $a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,147975			/* load bits [31:12] of constant */
	and     $t1,$a7,$t1			/* x & ... */
	ori	$t8,$t8,219			/* load bits [11:0] of constant */
	xor     $t1,$a5,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,3*4		/* (NEXT STEP) X[3] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[3] */
	rotri.w	$a6,$a6,32-17		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w   $a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-255012			/* load bits [31:12] of constant */
	and     $t1,$a6,$t1			/* x & ... */
	ori	$t8,$t8,3822			/* load bits [11:0] of constant */
	xor     $t1,$a4,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,4*4		/* (NEXT STEP) X[4] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[4] */
	rotri.w	$a5,$a5,32-22		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w   $a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-43072			/* load bits [31:12] of constant */
	and     $t1,$a5,$t1			/* x & ... */
	ori	$t8,$t8,4015			/* load bits [11:0] of constant */
	xor     $t1,$a7,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,5*4		/* (NEXT STEP) X[5] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[5] */
	rotri.w	$a4,$a4,32-7		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w   $a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,292988			/* load bits [31:12] of constant */
	and     $t1,$a4,$t1			/* x & ... */
	ori	$t8,$t8,1578			/* load bits [11:0] of constant */
	xor     $t1,$a6,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,6*4		/* (NEXT STEP) X[6] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[6] */
	rotri.w	$a7,$a7,32-12		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w   $a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-359676			/* load bits [31:12] of constant */
	and     $t1,$a7,$t1			/* x & ... */
	ori	$t8,$t8,1555			/* load bits [11:0] of constant */
	xor     $t1,$a5,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,7*4		/* (NEXT STEP) X[7] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[7] */
	rotri.w	$a6,$a6,32-17		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w   $a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-11159			/* load bits [31:12] of constant */
	and     $t1,$a6,$t1			/* x & ... */
	ori	$t8,$t8,1281			/* load bits [11:0] of constant */
	xor     $t1,$a4,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,8*4		/* (NEXT STEP) X[8] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[8] */
	rotri.w	$a5,$a5,32-22		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w   $a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,432137			/* load bits [31:12] of constant */
	and     $t1,$a5,$t1			/* x & ... */
	ori	$t8,$t8,2264			/* load bits [11:0] of constant */
	xor     $t1,$a7,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,9*4		/* (NEXT STEP) X[9] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[9] */
	rotri.w	$a4,$a4,32-7		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w   $a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-478129			/* load bits [31:12] of constant */
	and     $t1,$a4,$t1			/* x & ... */
	ori	$t8,$t8,1967			/* load bits [11:0] of constant */
	xor     $t1,$a6,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,10*4		/* (NEXT STEP) X[10] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[10] */
	rotri.w	$a7,$a7,32-12		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w   $a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-11			/* load bits [31:12] of constant */
	and     $t1,$a7,$t1			/* x & ... */
	ori	$t8,$t8,2993			/* load bits [11:0] of constant */
	xor     $t1,$a5,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,11*4		/* (NEXT STEP) X[11] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[11] */
	rotri.w	$a6,$a6,32-17		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w   $a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-485939			/* load bits [31:12] of constant */
	and     $t1,$a6,$t1			/* x & ... */
	ori	$t8,$t8,1982			/* load bits [11:0] of constant */
	xor     $t1,$a4,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,12*4		/* (NEXT STEP) X[12] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[12] */
	rotri.w	$a5,$a5,32-22		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w   $a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,440577			/* load bits [31:12] of constant */
	and     $t1,$a5,$t1			/* x & ... */
	ori	$t8,$t8,290			/* load bits [11:0] of constant */
	xor     $t1,$a7,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,13*4		/* (NEXT STEP) X[13] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[13] */
	rotri.w	$a4,$a4,32-7		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w   $a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-9849			/* load bits [31:12] of constant */
	and     $t1,$a4,$t1			/* x & ... */
	ori	$t8,$t8,403			/* load bits [11:0] of constant */
	xor     $t1,$a6,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,14*4		/* (NEXT STEP) X[14] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[14] */
	rotri.w	$a7,$a7,32-12		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w   $a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-366700			/* load bits [31:12] of constant */
	and     $t1,$a7,$t1			/* x & ... */
	ori	$t8,$t8,910			/* load bits [11:0] of constant */
	xor     $t1,$a5,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,15*4		/* (NEXT STEP) X[15] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[15] */
	rotri.w	$a6,$a6,32-17		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w   $a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,301888			/* load bits [31:12] of constant */
	and     $t1,$a6,$t1			/* x & ... */
	ori	$t8,$t8,2081			/* load bits [11:0] of constant */
	xor     $t1,$a4,$t1			/* z ^ ... */
	add.w   $t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,1*4		/* (NEXT STEP) X[1] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[1] */
	rotri.w	$a5,$a5,32-22		/* dst <<< s */
	move	$t0,$a7		/* (NEXT ROUND) $t0 = z' (copy of z) */
	nor	$t1,$zero,$a7	/* (NEXT ROUND) $t1 = not z' (copy of not z) */
	add.w   $a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-40478			/* load bits [31:12] of Constant */
	and	$t0,$a5,$t0			/* x & z */
	ori	$t8,$t8,1378			/* load bits [11:0] of Constant */
	and	$t1,$a6,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,6*4		/* (NEXT STEP) X[6] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[6] */
	rotri.w $a4,$a4,32-5		/* dst <<< s */
	move	$t0,$a6		/* (NEXT STEP) z' = $a6 */
	nor	$t1,$zero,$a6	/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-261109			/* load bits [31:12] of Constant */
	and	$t0,$a4,$t0			/* x & z */
	ori	$t8,$t8,832			/* load bits [11:0] of Constant */
	and	$t1,$a5,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,11*4		/* (NEXT STEP) X[11] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[11] */
	rotri.w $a7,$a7,32-9		/* dst <<< s */
	move	$t0,$a5		/* (NEXT STEP) z' = $a5 */
	nor	$t1,$zero,$a5	/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,157157			/* load bits [31:12] of Constant */
	and	$t0,$a7,$t0			/* x & z */
	ori	$t8,$t8,2641			/* load bits [11:0] of Constant */
	and	$t1,$a4,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,0*4		/* (NEXT STEP) X[0] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[0] */
	rotri.w $a6,$a6,32-14		/* dst <<< s */
	move	$t0,$a4		/* (NEXT STEP) z' = $a4 */
	nor	$t1,$zero,$a4	/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-91284			/* load bits [31:12] of Constant */
	and	$t0,$a6,$t0			/* x & z */
	ori	$t8,$t8,1962			/* load bits [11:0] of Constant */
	and	$t1,$a7,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,5*4		/* (NEXT STEP) X[5] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[5] */
	rotri.w $a5,$a5,32-20		/* dst <<< s */
	move	$t0,$a7		/* (NEXT STEP) z' = $a7 */
	nor	$t1,$zero,$a7	/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-171279			/* load bits [31:12] of Constant */
	and	$t0,$a5,$t0			/* x & z */
	ori	$t8,$t8,93			/* load bits [11:0] of Constant */
	and	$t1,$a6,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,10*4		/* (NEXT STEP) X[10] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[10] */
	rotri.w $a4,$a4,32-5		/* dst <<< s */
	move	$t0,$a6		/* (NEXT STEP) z' = $a6 */
	nor	$t1,$zero,$a6	/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,9281			/* load bits [31:12] of Constant */
	and	$t0,$a4,$t0			/* x & z */
	ori	$t8,$t8,1107			/* load bits [11:0] of Constant */
	and	$t1,$a5,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,15*4		/* (NEXT STEP) X[15] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[15] */
	rotri.w $a7,$a7,32-9		/* dst <<< s */
	move	$t0,$a5		/* (NEXT STEP) z' = $a5 */
	nor	$t1,$zero,$a5	/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-161250			/* load bits [31:12] of Constant */
	and	$t0,$a7,$t0			/* x & z */
	ori	$t8,$t8,1665			/* load bits [11:0] of Constant */
	and	$t1,$a4,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,4*4		/* (NEXT STEP) X[4] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[4] */
	rotri.w $a6,$a6,32-14		/* dst <<< s */
	move	$t0,$a4		/* (NEXT STEP) z' = $a4 */
	nor	$t1,$zero,$a4	/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-99009			/* load bits [31:12] of Constant */
	and	$t0,$a6,$t0			/* x & z */
	ori	$t8,$t8,3016			/* load bits [11:0] of Constant */
	and	$t1,$a7,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,9*4		/* (NEXT STEP) X[9] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[9] */
	rotri.w $a5,$a5,32-20		/* dst <<< s */
	move	$t0,$a7		/* (NEXT STEP) z' = $a7 */
	nor	$t1,$zero,$a7	/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,138780			/* load bits [31:12] of Constant */
	and	$t0,$a5,$t0			/* x & z */
	ori	$t8,$t8,3558			/* load bits [11:0] of Constant */
	and	$t1,$a6,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,14*4		/* (NEXT STEP) X[14] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[14] */
	rotri.w $a4,$a4,32-5		/* dst <<< s */
	move	$t0,$a6		/* (NEXT STEP) z' = $a6 */
	nor	$t1,$zero,$a6	/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-248976			/* load bits [31:12] of Constant */
	and	$t0,$a4,$t0			/* x & z */
	ori	$t8,$t8,2006			/* load bits [11:0] of Constant */
	and	$t1,$a5,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,3*4		/* (NEXT STEP) X[3] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[3] */
	rotri.w $a7,$a7,32-9		/* dst <<< s */
	move	$t0,$a5		/* (NEXT STEP) z' = $a5 */
	nor	$t1,$zero,$a5	/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-45744			/* load bits [31:12] of Constant */
	and	$t0,$a7,$t0			/* x & z */
	ori	$t8,$t8,3463			/* load bits [11:0] of Constant */
	and	$t1,$a4,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,8*4		/* (NEXT STEP) X[8] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[8] */
	rotri.w $a6,$a6,32-14		/* dst <<< s */
	move	$t0,$a4		/* (NEXT STEP) z' = $a4 */
	nor	$t1,$zero,$a4	/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,284065			/* load bits [31:12] of Constant */
	and	$t0,$a6,$t0			/* x & z */
	ori	$t8,$t8,1261			/* load bits [11:0] of Constant */
	and	$t1,$a7,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,13*4		/* (NEXT STEP) X[13] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[13] */
	rotri.w $a5,$a5,32-20		/* dst <<< s */
	move	$t0,$a7		/* (NEXT STEP) z' = $a7 */
	nor	$t1,$zero,$a7	/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-352706			/* load bits [31:12] of Constant */
	and	$t0,$a5,$t0			/* x & z */
	ori	$t8,$t8,2309			/* load bits [11:0] of Constant */
	and	$t1,$a6,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,2*4		/* (NEXT STEP) X[2] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[2] */
	rotri.w $a4,$a4,32-5		/* dst <<< s */
	move	$t0,$a6		/* (NEXT STEP) z' = $a6 */
	nor	$t1,$zero,$a6	/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-12550			/* load bits [31:12] of Constant */
	and	$t0,$a4,$t0			/* x & z */
	ori	$t8,$t8,1016			/* load bits [11:0] of Constant */
	and	$t1,$a5,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,7*4		/* (NEXT STEP) X[7] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[7] */
	rotri.w $a7,$a7,32-9		/* dst <<< s */
	move	$t0,$a5		/* (NEXT STEP) z' = $a5 */
	nor	$t1,$zero,$a5	/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,423664			/* load bits [31:12] of Constant */
	and	$t0,$a7,$t0			/* x & z */
	ori	$t8,$t8,729			/* load bits [11:0] of Constant */
	and	$t1,$a4,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,12*4		/* (NEXT STEP) X[12] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[12] */
	rotri.w $a6,$a6,32-14		/* dst <<< s */
	move	$t0,$a4		/* (NEXT STEP) z' = $a4 */
	nor	$t1,$zero,$a4	/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-470364			/* load bits [31:12] of Constant */
	and	$t0,$a6,$t0			/* x & z */
	ori	$t8,$t8,3210			/* load bits [11:0] of Constant */
	and	$t1,$a7,$t1			/* y & (not z) */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	or	$t1,$t0,$t1			/* (y & (not z)) | (x & z) */
	ld.w	$t0,$a1,5*4		/* (NEXT STEP) X[5] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[5] */
	rotri.w $a5,$a5,32-20		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT ROUND) $t1 = y ^ z */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-93			/* load bits [31:12] of Constant */
	xor	$t1,$a5,$t1			/* x ^ ... */
	ori	$t8,$t8,2370			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,8*4		/* (NEXT STEP) X[8] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[8] */
	rotri.w $a4,$a4,32-4		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-493793			/* load bits [31:12] of Constant */
	xor	$t1,$a4,$t1			/* x ^ ... */
	ori	$t8,$t8,1665			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,11*4		/* (NEXT STEP) X[11] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[11] */
	rotri.w $a7,$a7,32-11		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,448982			/* load bits [31:12] of Constant */
	xor	$t1,$a7,$t1			/* x ^ ... */
	ori	$t8,$t8,290			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,14*4		/* (NEXT STEP) X[14] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[14] */
	rotri.w $a6,$a6,32-16		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-8621			/* load bits [31:12] of Constant */
	xor	$t1,$a6,$t1			/* x ^ ... */
	ori	$t8,$t8,2060			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,1*4		/* (NEXT STEP) X[1] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[1] */
	rotri.w $a5,$a5,32-23		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-373778			/* load bits [31:12] of Constant */
	xor	$t1,$a5,$t1			/* x ^ ... */
	ori	$t8,$t8,2628			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,4*4		/* (NEXT STEP) X[4] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[4] */
	rotri.w $a4,$a4,32-4		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,310764			/* load bits [31:12] of Constant */
	xor	$t1,$a4,$t1			/* x ^ ... */
	ori	$t8,$t8,4009			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,7*4		/* (NEXT STEP) X[7] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[7] */
	rotri.w $a7,$a7,32-11		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-37964			/* load bits [31:12] of Constant */
	xor	$t1,$a7,$t1			/* x ^ ... */
	ori	$t8,$t8,2912			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,10*4		/* (NEXT STEP) X[10] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[10] */
	rotri.w $a6,$a6,32-16		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-267269			/* load bits [31:12] of Constant */
	xor	$t1,$a6,$t1			/* x ^ ... */
	ori	$t8,$t8,3184			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,13*4		/* (NEXT STEP) X[13] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[13] */
	rotri.w $a5,$a5,32-23		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,166327			/* load bits [31:12] of Constant */
	xor	$t1,$a5,$t1			/* x ^ ... */
	ori	$t8,$t8,3782			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,0*4		/* (NEXT STEP) X[0] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[0] */
	rotri.w $a4,$a4,32-4		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-87534			/* load bits [31:12] of Constant */
	xor	$t1,$a4,$t1			/* x ^ ... */
	ori	$t8,$t8,2042			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,3*4		/* (NEXT STEP) X[3] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[3] */
	rotri.w $a7,$a7,32-11		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,-176397			/* load bits [31:12] of Constant */
	xor	$t1,$a7,$t1			/* x ^ ... */
	ori	$t8,$t8,133			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,6*4		/* (NEXT STEP) X[6] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[6] */
	rotri.w $a6,$a6,32-16		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,18561			/* load bits [31:12] of Constant */
	xor	$t1,$a6,$t1			/* x ^ ... */
	ori	$t8,$t8,3333			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,9*4		/* (NEXT STEP) X[9] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[9] */
	rotri.w $a5,$a5,32-23		/* dst <<< s */
	xor	$t1,$a6,$a7	/* (NEXT STEP) y ^ z */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-156339			/* load bits [31:12] of Constant */
	xor	$t1,$a5,$t1			/* x ^ ... */
	ori	$t8,$t8,57			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,12*4		/* (NEXT STEP) X[12] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[12] */
	rotri.w $a4,$a4,32-4		/* dst <<< s */
	xor	$t1,$a5,$a6	/* (NEXT STEP) y ^ z */
	add.w	$a4,$a4,$a5		/* dst += x */
	lu12i.w	$t8,-102983			/* load bits [31:12] of Constant */
	xor	$t1,$a4,$t1			/* x ^ ... */
	ori	$t8,$t8,2533			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,15*4		/* (NEXT STEP) X[15] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[15] */
	rotri.w $a7,$a7,32-11		/* dst <<< s */
	xor	$t1,$a4,$a5	/* (NEXT STEP) y ^ z */
	add.w	$a7,$a7,$a4		/* dst += x */
	lu12i.w	$t8,129575			/* load bits [31:12] of Constant */
	xor	$t1,$a7,$t1			/* x ^ ... */
	ori	$t8,$t8,3320			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,2*4		/* (NEXT STEP) X[2] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[2] */
	rotri.w $a6,$a6,32-16		/* dst <<< s */
	xor	$t1,$a7,$a4	/* (NEXT STEP) y ^ z */
	add.w	$a6,$a6,$a7		/* dst += x */
	lu12i.w	$t8,-243003			/* load bits [31:12] of Constant */
	xor	$t1,$a6,$t1			/* x ^ ... */
	ori	$t8,$t8,1637			/* load bits [11:0] of Constant */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,0*4		/* (NEXT STEP) X[0] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[0] */
	rotri.w $a5,$a5,32-23		/* dst <<< s */
	nor	$t1,$zero,$a7	/* (NEXT ROUND) $t1 = not z */
	add.w	$a5,$a5,$a6		/* dst += x */
	lu12i.w	$t8,-48494			/* load bits [31:12] of Constant */
	or	$t1,$a5,$t1			/* x | ... */
	ori	$t8,$t8,580			/* load bits [11:0] of Constant */
	xor	$t1,$a6,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,7*4		/* (NEXT STEP) X[7] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[7] */
	rotri.w $a4,$a4,32-6			/* dst <<< s */
	nor	$t1,$zero,$a6			/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5			/* dst += x */
	lu12i.w	$t8,275119			/* load bits [31:12] of Constant */
	or	$t1,$a4,$t1			/* x | ... */
	ori	$t8,$t8,3991			/* load bits [11:0] of Constant */
	xor	$t1,$a5,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,14*4		/* (NEXT STEP) X[14] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[14] */
	rotri.w $a7,$a7,32-10			/* dst <<< s */
	nor	$t1,$zero,$a5			/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4			/* dst += x */
	lu12i.w	$t8,-345790			/* load bits [31:12] of Constant */
	or	$t1,$a7,$t1			/* x | ... */
	ori	$t8,$t8,935			/* load bits [11:0] of Constant */
	xor	$t1,$a4,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,5*4		/* (NEXT STEP) X[5] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[5] */
	rotri.w $a6,$a6,32-15			/* dst <<< s */
	nor	$t1,$zero,$a4			/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7			/* dst += x */
	lu12i.w	$t8,-14022			/* load bits [31:12] of Constant */
	or	$t1,$a6,$t1			/* x | ... */
	ori	$t8,$t8,57			/* load bits [11:0] of Constant */
	xor	$t1,$a7,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,12*4		/* (NEXT STEP) X[12] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[12] */
	rotri.w $a5,$a5,32-21			/* dst <<< s */
	nor	$t1,$zero,$a7			/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6			/* dst += x */
	lu12i.w	$t8,415157			/* load bits [31:12] of Constant */
	or	$t1,$a5,$t1			/* x | ... */
	ori	$t8,$t8,2499			/* load bits [11:0] of Constant */
	xor	$t1,$a6,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,3*4		/* (NEXT STEP) X[3] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[3] */
	rotri.w $a4,$a4,32-6			/* dst <<< s */
	nor	$t1,$zero,$a6			/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5			/* dst += x */
	lu12i.w	$t8,-462644			/* load bits [31:12] of Constant */
	or	$t1,$a4,$t1			/* x | ... */
	ori	$t8,$t8,3218			/* load bits [11:0] of Constant */
	xor	$t1,$a5,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,10*4		/* (NEXT STEP) X[10] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[10] */
	rotri.w $a7,$a7,32-10			/* dst <<< s */
	nor	$t1,$zero,$a5			/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4			/* dst += x */
	lu12i.w	$t8,-257			/* load bits [31:12] of Constant */
	or	$t1,$a7,$t1			/* x | ... */
	ori	$t8,$t8,1149			/* load bits [11:0] of Constant */
	xor	$t1,$a4,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,1*4		/* (NEXT STEP) X[1] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[1] */
	rotri.w $a6,$a6,32-15			/* dst <<< s */
	nor	$t1,$zero,$a4			/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7			/* dst += x */
	lu12i.w	$t8,-501691			/* load bits [31:12] of Constant */
	or	$t1,$a6,$t1			/* x | ... */
	ori	$t8,$t8,3537			/* load bits [11:0] of Constant */
	xor	$t1,$a7,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,8*4		/* (NEXT STEP) X[8] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[8] */
	rotri.w $a5,$a5,32-21			/* dst <<< s */
	nor	$t1,$zero,$a7			/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6			/* dst += x */
	lu12i.w	$t8,457351			/* load bits [31:12] of Constant */
	or	$t1,$a5,$t1			/* x | ... */
	ori	$t8,$t8,3663			/* load bits [11:0] of Constant */
	xor	$t1,$a6,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,15*4		/* (NEXT STEP) X[15] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[15] */
	rotri.w $a4,$a4,32-6			/* dst <<< s */
	nor	$t1,$zero,$a6			/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5			/* dst += x */
	lu12i.w	$t8,-7474			/* load bits [31:12] of Constant */
	or	$t1,$a4,$t1			/* x | ... */
	ori	$t8,$t8,1760			/* load bits [11:0] of Constant */
	xor	$t1,$a5,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,6*4		/* (NEXT STEP) X[6] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[6] */
	rotri.w $a7,$a7,32-10			/* dst <<< s */
	nor	$t1,$zero,$a5			/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4			/* dst += x */
	lu12i.w	$t8,-380908			/* load bits [31:12] of Constant */
	or	$t1,$a7,$t1			/* x | ... */
	ori	$t8,$t8,788			/* load bits [11:0] of Constant */
	xor	$t1,$a4,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,13*4		/* (NEXT STEP) X[13] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[13] */
	rotri.w $a6,$a6,32-15			/* dst <<< s */
	nor	$t1,$zero,$a4			/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7			/* dst += x */
	lu12i.w	$t8,319617			/* load bits [31:12] of Constant */
	or	$t1,$a6,$t1			/* x | ... */
	ori	$t8,$t8,417			/* load bits [11:0] of Constant */
	xor	$t1,$a7,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,4*4		/* (NEXT STEP) X[4] */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$t2,$a4,$t0			/* (NEXT STEP) dst + X[4] */
	rotri.w $a5,$a5,32-21			/* dst <<< s */
	nor	$t1,$zero,$a7			/* (NEXT STEP) not z' = not $a7 */
	add.w	$a5,$a5,$a6			/* dst += x */
	lu12i.w	$t8,-35529			/* load bits [31:12] of Constant */
	or	$t1,$a5,$t1			/* x | ... */
	ori	$t8,$t8,3714			/* load bits [11:0] of Constant */
	xor	$t1,$a6,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,11*4		/* (NEXT STEP) X[11] */
	add.w	$a4,$t7,$t1			/* dst += ... */
	add.w	$t2,$a7,$t0			/* (NEXT STEP) dst + X[11] */
	rotri.w $a4,$a4,32-6			/* dst <<< s */
	nor	$t1,$zero,$a6			/* (NEXT STEP) not z' = not $a6 */
	add.w	$a4,$a4,$a5			/* dst += x */
	lu12i.w	$t8,-273489			/* load bits [31:12] of Constant */
	or	$t1,$a4,$t1			/* x | ... */
	ori	$t8,$t8,565			/* load bits [11:0] of Constant */
	xor	$t1,$a5,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,2*4		/* (NEXT STEP) X[2] */
	add.w	$a7,$t7,$t1			/* dst += ... */
	add.w	$t2,$a6,$t0			/* (NEXT STEP) dst + X[2] */
	rotri.w $a7,$a7,32-10			/* dst <<< s */
	nor	$t1,$zero,$a5			/* (NEXT STEP) not z' = not $a5 */
	add.w	$a7,$a7,$a4			/* dst += x */
	lu12i.w	$t8,175485			/* load bits [31:12] of Constant */
	or	$t1,$a7,$t1			/* x | ... */
	ori	$t8,$t8,699			/* load bits [11:0] of Constant */
	xor	$t1,$a4,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	ld.w	$t0,$a1,9*4		/* (NEXT STEP) X[9] */
	add.w	$a6,$t7,$t1			/* dst += ... */
	add.w	$t2,$a5,$t0			/* (NEXT STEP) dst + X[9] */
	rotri.w $a6,$a6,32-15			/* dst <<< s */
	nor	$t1,$zero,$a4			/* (NEXT STEP) not z' = not $a4 */
	add.w	$a6,$a6,$a7			/* dst += x */
	lu12i.w	$t8,-83859			/* load bits [31:12] of Constant */
	or	$t1,$a6,$t1			/* x | ... */
	ori	$t8,$t8,913			/* load bits [11:0] of Constant */
	xor	$t1,$a7,$t1			/* y ^ ... */
	add.w	$t7,$t2,$t8			/* dst + X[k] + Const */
	add.w	$a4,$t3,$a4			/* (NEXT LOOP) add old value of A */
	add.w	$a5,$t7,$t1			/* dst += ... */
	add.w	$a7,$t6,$a7			/* (NEXT LOOP) add old value of D */
	rotri.w $a5,$a5,32-21			/* dst <<< s */
	addi.d	$a1,$a1,64			/* (NEXT LOOP) ptr += 64 */
	add.w	$a5,$a5,$a6			/* dst += x */
	# add old values of B, C
	add.w	$a5,$t4,$a5
	add.w	$a6,$t5,$a6

	bltu	$a1,$a3,.Lloop	# jmp if ptr < end

	st.w	$a4,$a0,0	# ctx->A = A
	st.w	$a5,$a0,4	# ctx->B = B
	st.w	$a6,$a0,8	# ctx->C = C
	st.w	$a7,$a0,12	# ctx->D = D

.Lend:
	jr	$ra
.size ossl_md5_block_asm_data_order,.-ossl_md5_block_asm_data_order
