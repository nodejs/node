#include "mips_arch.h"

.text

.set	noat
.set	noreorder

.align	5
.globl	bn_mul_mont
.ent	bn_mul_mont
bn_mul_mont:
	slt	$1,$9,4
	bnez	$1,1f
	li	$2,0
	slt	$1,$9,17	# on in-order CPU
	bnez	$1,bn_mul_mont_internal
	nop
1:	jr	$31
	li	$4,0
.end	bn_mul_mont

.align	5
.ent	bn_mul_mont_internal
bn_mul_mont_internal:
	.frame	$30,14*8,$31
	.mask	0x40000000|16711680,-8
	dsubu $29,14*8
	sd	$30,(14-1)*8($29)
	sd	$23,(14-2)*8($29)
	sd	$22,(14-3)*8($29)
	sd	$21,(14-4)*8($29)
	sd	$20,(14-5)*8($29)
	sd	$19,(14-6)*8($29)
	sd	$18,(14-7)*8($29)
	sd	$17,(14-8)*8($29)
	sd	$16,(14-9)*8($29)
	move	$30,$29

	.set	reorder
	ld	$8,0($8)
	ld	$13,0($6)	# bp[0]
	ld	$12,0($5)	# ap[0]
	ld	$14,0($7)	# np[0]

	dsubu $29,2*8	# place for two extra words
	sll	$9,3
	li	$1,-4096
	dsubu $29,$9
	and	$29,$1

	dmultu	($12,$13)
	ld	$17,8($5)
	ld	$19,8($7)
	mflo	($10,$12,$13)
	mfhi	($11,$12,$13)
	dmultu	($10,$8)
	mflo	($23,$10,$8)

	dmultu	($17,$13)
	mflo	($16,$17,$13)
	mfhi	($17,$17,$13)

	dmultu	($14,$23)
	mflo	($24,$14,$23)
	mfhi	($25,$14,$23)
	dmultu	($19,$23)
	daddu	$24,$10
	sltu	$1,$24,$10
	daddu	$25,$1
	mflo	($18,$19,$23)
	mfhi	($19,$19,$23)

	move	$15,$29
	li	$22,2*8
.align	4
.L1st:
	.set	noreorder
	daddu $12,$5,$22
	daddu $14,$7,$22
	ld	$12,($12)
	ld	$14,($14)

	dmultu	($12,$13)
	daddu	$10,$16,$11
	daddu	$24,$18,$25
	sltu	$1,$10,$11
	sltu	$2,$24,$25
	daddu	$11,$17,$1
	daddu	$25,$19,$2
	mflo	($16,$12,$13)
	mfhi	($17,$12,$13)

	daddu	$24,$10
	sltu	$1,$24,$10
	dmultu	($14,$23)
	daddu	$25,$1
	addu	$22,8
	sd	$24,($15)
	sltu	$2,$22,$9
	mflo	($18,$14,$23)
	mfhi	($19,$14,$23)

	bnez	$2,.L1st
	daddu $15,8
	.set	reorder

	daddu	$10,$16,$11
	sltu	$1,$10,$11
	daddu	$11,$17,$1

	daddu	$24,$18,$25
	sltu	$2,$24,$25
	daddu	$25,$19,$2
	daddu	$24,$10
	sltu	$1,$24,$10
	daddu	$25,$1

	sd	$24,($15)

	daddu	$25,$11
	sltu	$1,$25,$11
	sd	$25,8($15)
	sd	$1,2*8($15)

	li	$21,8
.align	4
.Louter:
	daddu $13,$6,$21
	ld	$13,($13)
	ld	$12,($5)
	ld	$17,8($5)
	ld	$20,($29)

	dmultu	($12,$13)
	ld	$14,($7)
	ld	$19,8($7)
	mflo	($10,$12,$13)
	mfhi	($11,$12,$13)
	daddu	$10,$20
	dmultu	($10,$8)
	sltu	$1,$10,$20
	daddu	$11,$1
	mflo	($23,$10,$8)

	dmultu	($17,$13)
	mflo	($16,$17,$13)
	mfhi	($17,$17,$13)

	dmultu	($14,$23)
	mflo	($24,$14,$23)
	mfhi	($25,$14,$23)

	dmultu	($19,$23)
	daddu	$24,$10
	sltu	$1,$24,$10
	daddu	$25,$1
	mflo	($18,$19,$23)
	mfhi	($19,$19,$23)

	move	$15,$29
	li	$22,2*8
	ld	$20,8($15)
.align	4
.Linner:
	.set	noreorder
	daddu $12,$5,$22
	daddu $14,$7,$22
	ld	$12,($12)
	ld	$14,($14)

	dmultu	($12,$13)
	daddu	$10,$16,$11
	daddu	$24,$18,$25
	sltu	$1,$10,$11
	sltu	$2,$24,$25
	daddu	$11,$17,$1
	daddu	$25,$19,$2
	mflo	($16,$12,$13)
	mfhi	($17,$12,$13)

	daddu	$10,$20
	addu	$22,8
	dmultu	($14,$23)
	sltu	$1,$10,$20
	daddu	$24,$10
	daddu	$11,$1
	sltu	$2,$24,$10
	ld	$20,2*8($15)
	daddu	$25,$2
	sltu	$1,$22,$9
	mflo	($18,$14,$23)
	mfhi	($19,$14,$23)
	sd	$24,($15)
	bnez	$1,.Linner
	daddu $15,8
	.set	reorder

	daddu	$10,$16,$11
	sltu	$1,$10,$11
	daddu	$11,$17,$1
	daddu	$10,$20
	sltu	$2,$10,$20
	daddu	$11,$2

	ld	$20,2*8($15)
	daddu	$24,$18,$25
	sltu	$1,$24,$25
	daddu	$25,$19,$1
	daddu	$24,$10
	sltu	$2,$24,$10
	daddu	$25,$2
	sd	$24,($15)

	daddu	$24,$25,$11
	sltu	$25,$24,$11
	daddu	$24,$20
	sltu	$1,$24,$20
	daddu	$25,$1
	sd	$24,8($15)
	sd	$25,2*8($15)

	addu	$21,8
	sltu	$2,$21,$9
	bnez	$2,.Louter

	.set	noreorder
	daddu $20,$29,$9	# &tp[num]
	move	$15,$29
	move	$5,$29
	li	$11,0		# clear borrow bit

.align	4
.Lsub:	ld	$10,($15)
	ld	$24,($7)
	daddu $15,8
	daddu $7,8
	dsubu	$24,$10,$24	# tp[i]-np[i]
	sgtu	$1,$24,$10
	dsubu	$10,$24,$11
	sgtu	$11,$10,$24
	sd	$10,($4)
	or	$11,$1
	sltu	$1,$15,$20
	bnez	$1,.Lsub
	daddu $4,8

	dsubu	$11,$25,$11	# handle upmost overflow bit
	move	$15,$29
	dsubu $4,$9	# restore rp
	not	$25,$11

.Lcopy:	ld	$14,($15)	# conditional move
	ld	$12,($4)
	sd	$0,($15)
	daddu $15,8
	and	$14,$11
	and	$12,$25
	or	$12,$14
	sltu	$1,$15,$20
	sd	$12,($4)
	bnez	$1,.Lcopy
	daddu $4,8

	li	$4,1
	li	$2,1

	.set	noreorder
	move	$29,$30
	ld	$30,(14-1)*8($29)
	ld	$23,(14-2)*8($29)
	ld	$22,(14-3)*8($29)
	ld	$21,(14-4)*8($29)
	ld	$20,(14-5)*8($29)
	ld	$19,(14-6)*8($29)
	ld	$18,(14-7)*8($29)
	ld	$17,(14-8)*8($29)
	ld	$16,(14-9)*8($29)
	jr	$31
	daddu $29,14*8
.end	bn_mul_mont_internal
.rdata
.asciiz	"Montgomery Multiplication for MIPS, CRYPTOGAMS by <appro@openssl.org>"
