#include "mips_arch.h"

#if defined(_MIPS_ARCH_MIPS64R6)
# define ddivu(rs,rt)
# define mfqt(rd,rs,rt)	ddivu	rd,rs,rt
# define mfrm(rd,rs,rt)	dmodu	rd,rs,rt
#elif defined(_MIPS_ARCH_MIPS32R6)
# define divu(rs,rt)
# define mfqt(rd,rs,rt)	divu	rd,rs,rt
# define mfrm(rd,rs,rt)	modu	rd,rs,rt
#else
# define ddivu(rs,rt)	ddivu	$0,rs,rt
# define mfqt(rd,rs,rt)	mflo	rd
# define mfrm(rd,rs,rt)	mfhi	rd
#endif

.rdata
.asciiz	"mips3.s, Version 1.2"
.asciiz	"MIPS II/III/IV ISA artwork by Andy Polyakov <appro@fy.chalmers.se>"

.text
.set	noat

.align	5
.globl	bn_mul_add_words
.ent	bn_mul_add_words
bn_mul_add_words:
	.set	noreorder
	bgtz	$6,bn_mul_add_words_internal
	move	$2,$0
	jr	$31
	move	$4,$2
.end	bn_mul_add_words

.align	5
.ent	bn_mul_add_words_internal
bn_mul_add_words_internal:
	.set	reorder
	li	$3,-4
	and	$8,$6,$3
	beqz	$8,.L_bn_mul_add_words_tail

.L_bn_mul_add_words_loop:
	ld	$12,0($5)
	dmultu	($12,$7)
	ld	$13,0($4)
	ld	$14,8($5)
	ld	$15,8($4)
	ld	$8,2*8($5)
	ld	$9,2*8($4)
	daddu	$13,$2
	sltu	$2,$13,$2	# All manuals say it "compares 32-bit
				# values", but it seems to work fine
				# even on 64-bit registers.
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$13,$1
	daddu	$2,$12
	 dmultu	($14,$7)
	sltu	$1,$13,$1
	sd	$13,0($4)
	daddu	$2,$1

	ld	$10,3*8($5)
	ld	$11,3*8($4)
	daddu	$15,$2
	sltu	$2,$15,$2
	mflo	($1,$14,$7)
	mfhi	($14,$14,$7)
	daddu	$15,$1
	daddu	$2,$14
	 dmultu	($8,$7)
	sltu	$1,$15,$1
	sd	$15,8($4)
	daddu	$2,$1

	subu	$6,4
	daddu $4,4*8
	daddu $5,4*8
	daddu	$9,$2
	sltu	$2,$9,$2
	mflo	($1,$8,$7)
	mfhi	($8,$8,$7)
	daddu	$9,$1
	daddu	$2,$8
	 dmultu	($10,$7)
	sltu	$1,$9,$1
	sd	$9,-2*8($4)
	daddu	$2,$1


	and	$8,$6,$3
	daddu	$11,$2
	sltu	$2,$11,$2
	mflo	($1,$10,$7)
	mfhi	($10,$10,$7)
	daddu	$11,$1
	daddu	$2,$10
	sltu	$1,$11,$1
	sd	$11,-8($4)
	.set	noreorder
	bgtz	$8,.L_bn_mul_add_words_loop
	daddu	$2,$1

	beqz	$6,.L_bn_mul_add_words_return
	nop

.L_bn_mul_add_words_tail:
	.set	reorder
	ld	$12,0($5)
	dmultu	($12,$7)
	ld	$13,0($4)
	subu	$6,1
	daddu	$13,$2
	sltu	$2,$13,$2
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$13,$1
	daddu	$2,$12
	sltu	$1,$13,$1
	sd	$13,0($4)
	daddu	$2,$1
	beqz	$6,.L_bn_mul_add_words_return

	ld	$12,8($5)
	dmultu	($12,$7)
	ld	$13,8($4)
	subu	$6,1
	daddu	$13,$2
	sltu	$2,$13,$2
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$13,$1
	daddu	$2,$12
	sltu	$1,$13,$1
	sd	$13,8($4)
	daddu	$2,$1
	beqz	$6,.L_bn_mul_add_words_return

	ld	$12,2*8($5)
	dmultu	($12,$7)
	ld	$13,2*8($4)
	daddu	$13,$2
	sltu	$2,$13,$2
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$13,$1
	daddu	$2,$12
	sltu	$1,$13,$1
	sd	$13,2*8($4)
	daddu	$2,$1

.L_bn_mul_add_words_return:
	.set	noreorder
	jr	$31
	move	$4,$2
.end	bn_mul_add_words_internal

.align	5
.globl	bn_mul_words
.ent	bn_mul_words
bn_mul_words:
	.set	noreorder
	bgtz	$6,bn_mul_words_internal
	move	$2,$0
	jr	$31
	move	$4,$2
.end	bn_mul_words

.align	5
.ent	bn_mul_words_internal
bn_mul_words_internal:
	.set	reorder
	li	$3,-4
	and	$8,$6,$3
	beqz	$8,.L_bn_mul_words_tail

.L_bn_mul_words_loop:
	ld	$12,0($5)
	dmultu	($12,$7)
	ld	$14,8($5)
	ld	$8,2*8($5)
	ld	$10,3*8($5)
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$2,$1
	sltu	$13,$2,$1
	 dmultu	($14,$7)
	sd	$2,0($4)
	daddu	$2,$13,$12

	subu	$6,4
	daddu $4,4*8
	daddu $5,4*8
	mflo	($1,$14,$7)
	mfhi	($14,$14,$7)
	daddu	$2,$1
	sltu	$15,$2,$1
	 dmultu	($8,$7)
	sd	$2,-3*8($4)
	daddu	$2,$15,$14

	mflo	($1,$8,$7)
	mfhi	($8,$8,$7)
	daddu	$2,$1
	sltu	$9,$2,$1
	 dmultu	($10,$7)
	sd	$2,-2*8($4)
	daddu	$2,$9,$8

	and	$8,$6,$3
	mflo	($1,$10,$7)
	mfhi	($10,$10,$7)
	daddu	$2,$1
	sltu	$11,$2,$1
	sd	$2,-8($4)
	.set	noreorder
	bgtz	$8,.L_bn_mul_words_loop
	daddu	$2,$11,$10

	beqz	$6,.L_bn_mul_words_return
	nop

.L_bn_mul_words_tail:
	.set	reorder
	ld	$12,0($5)
	dmultu	($12,$7)
	subu	$6,1
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$2,$1
	sltu	$13,$2,$1
	sd	$2,0($4)
	daddu	$2,$13,$12
	beqz	$6,.L_bn_mul_words_return

	ld	$12,8($5)
	dmultu	($12,$7)
	subu	$6,1
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$2,$1
	sltu	$13,$2,$1
	sd	$2,8($4)
	daddu	$2,$13,$12
	beqz	$6,.L_bn_mul_words_return

	ld	$12,2*8($5)
	dmultu	($12,$7)
	mflo	($1,$12,$7)
	mfhi	($12,$12,$7)
	daddu	$2,$1
	sltu	$13,$2,$1
	sd	$2,2*8($4)
	daddu	$2,$13,$12

.L_bn_mul_words_return:
	.set	noreorder
	jr	$31
	move	$4,$2
.end	bn_mul_words_internal

.align	5
.globl	bn_sqr_words
.ent	bn_sqr_words
bn_sqr_words:
	.set	noreorder
	bgtz	$6,bn_sqr_words_internal
	move	$2,$0
	jr	$31
	move	$4,$2
.end	bn_sqr_words

.align	5
.ent	bn_sqr_words_internal
bn_sqr_words_internal:
	.set	reorder
	li	$3,-4
	and	$8,$6,$3
	beqz	$8,.L_bn_sqr_words_tail

.L_bn_sqr_words_loop:
	ld	$12,0($5)
	dmultu	($12,$12)
	ld	$14,8($5)
	ld	$8,2*8($5)
	ld	$10,3*8($5)
	mflo	($13,$12,$12)
	mfhi	($12,$12,$12)
	sd	$13,0($4)
	sd	$12,8($4)

	dmultu	($14,$14)
	subu	$6,4
	daddu $4,8*8
	daddu $5,4*8
	mflo	($15,$14,$14)
	mfhi	($14,$14,$14)
	sd	$15,-6*8($4)
	sd	$14,-5*8($4)

	dmultu	($8,$8)
	mflo	($9,$8,$8)
	mfhi	($8,$8,$8)
	sd	$9,-4*8($4)
	sd	$8,-3*8($4)


	dmultu	($10,$10)
	and	$8,$6,$3
	mflo	($11,$10,$10)
	mfhi	($10,$10,$10)
	sd	$11,-2*8($4)

	.set	noreorder
	bgtz	$8,.L_bn_sqr_words_loop
	sd	$10,-8($4)

	beqz	$6,.L_bn_sqr_words_return
	nop

.L_bn_sqr_words_tail:
	.set	reorder
	ld	$12,0($5)
	dmultu	($12,$12)
	subu	$6,1
	mflo	($13,$12,$12)
	mfhi	($12,$12,$12)
	sd	$13,0($4)
	sd	$12,8($4)
	beqz	$6,.L_bn_sqr_words_return

	ld	$12,8($5)
	dmultu	($12,$12)
	subu	$6,1
	mflo	($13,$12,$12)
	mfhi	($12,$12,$12)
	sd	$13,2*8($4)
	sd	$12,3*8($4)
	beqz	$6,.L_bn_sqr_words_return

	ld	$12,2*8($5)
	dmultu	($12,$12)
	mflo	($13,$12,$12)
	mfhi	($12,$12,$12)
	sd	$13,4*8($4)
	sd	$12,5*8($4)

.L_bn_sqr_words_return:
	.set	noreorder
	jr	$31
	move	$4,$2

.end	bn_sqr_words_internal

.align	5
.globl	bn_add_words
.ent	bn_add_words
bn_add_words:
	.set	noreorder
	bgtz	$7,bn_add_words_internal
	move	$2,$0
	jr	$31
	move	$4,$2
.end	bn_add_words

.align	5
.ent	bn_add_words_internal
bn_add_words_internal:
	.set	reorder
	li	$3,-4
	and	$1,$7,$3
	beqz	$1,.L_bn_add_words_tail

.L_bn_add_words_loop:
	ld	$12,0($5)
	ld	$8,0($6)
	subu	$7,4
	ld	$13,8($5)
	and	$1,$7,$3
	ld	$14,2*8($5)
	daddu $6,4*8
	ld	$15,3*8($5)
	daddu $4,4*8
	ld	$9,-3*8($6)
	daddu $5,4*8
	ld	$10,-2*8($6)
	ld	$11,-8($6)
	daddu	$8,$12
	sltu	$24,$8,$12
	daddu	$12,$8,$2
	sltu	$2,$12,$8
	sd	$12,-4*8($4)
	daddu	$2,$24

	daddu	$9,$13
	sltu	$25,$9,$13
	daddu	$13,$9,$2
	sltu	$2,$13,$9
	sd	$13,-3*8($4)
	daddu	$2,$25

	daddu	$10,$14
	sltu	$24,$10,$14
	daddu	$14,$10,$2
	sltu	$2,$14,$10
	sd	$14,-2*8($4)
	daddu	$2,$24

	daddu	$11,$15
	sltu	$25,$11,$15
	daddu	$15,$11,$2
	sltu	$2,$15,$11
	sd	$15,-8($4)

	.set	noreorder
	bgtz	$1,.L_bn_add_words_loop
	daddu	$2,$25

	beqz	$7,.L_bn_add_words_return
	nop

.L_bn_add_words_tail:
	.set	reorder
	ld	$12,0($5)
	ld	$8,0($6)
	daddu	$8,$12
	subu	$7,1
	sltu	$24,$8,$12
	daddu	$12,$8,$2
	sltu	$2,$12,$8
	sd	$12,0($4)
	daddu	$2,$24
	beqz	$7,.L_bn_add_words_return

	ld	$13,8($5)
	ld	$9,8($6)
	daddu	$9,$13
	subu	$7,1
	sltu	$25,$9,$13
	daddu	$13,$9,$2
	sltu	$2,$13,$9
	sd	$13,8($4)
	daddu	$2,$25
	beqz	$7,.L_bn_add_words_return

	ld	$14,2*8($5)
	ld	$10,2*8($6)
	daddu	$10,$14
	sltu	$24,$10,$14
	daddu	$14,$10,$2
	sltu	$2,$14,$10
	sd	$14,2*8($4)
	daddu	$2,$24

.L_bn_add_words_return:
	.set	noreorder
	jr	$31
	move	$4,$2

.end	bn_add_words_internal

.align	5
.globl	bn_sub_words
.ent	bn_sub_words
bn_sub_words:
	.set	noreorder
	bgtz	$7,bn_sub_words_internal
	move	$2,$0
	jr	$31
	move	$4,$0
.end	bn_sub_words

.align	5
.ent	bn_sub_words_internal
bn_sub_words_internal:
	.set	reorder
	li	$3,-4
	and	$1,$7,$3
	beqz	$1,.L_bn_sub_words_tail

.L_bn_sub_words_loop:
	ld	$12,0($5)
	ld	$8,0($6)
	subu	$7,4
	ld	$13,8($5)
	and	$1,$7,$3
	ld	$14,2*8($5)
	daddu $6,4*8
	ld	$15,3*8($5)
	daddu $4,4*8
	ld	$9,-3*8($6)
	daddu $5,4*8
	ld	$10,-2*8($6)
	ld	$11,-8($6)
	sltu	$24,$12,$8
	dsubu	$8,$12,$8
	dsubu	$12,$8,$2
	sgtu	$2,$12,$8
	sd	$12,-4*8($4)
	daddu	$2,$24

	sltu	$25,$13,$9
	dsubu	$9,$13,$9
	dsubu	$13,$9,$2
	sgtu	$2,$13,$9
	sd	$13,-3*8($4)
	daddu	$2,$25


	sltu	$24,$14,$10
	dsubu	$10,$14,$10
	dsubu	$14,$10,$2
	sgtu	$2,$14,$10
	sd	$14,-2*8($4)
	daddu	$2,$24

	sltu	$25,$15,$11
	dsubu	$11,$15,$11
	dsubu	$15,$11,$2
	sgtu	$2,$15,$11
	sd	$15,-8($4)

	.set	noreorder
	bgtz	$1,.L_bn_sub_words_loop
	daddu	$2,$25

	beqz	$7,.L_bn_sub_words_return
	nop

.L_bn_sub_words_tail:
	.set	reorder
	ld	$12,0($5)
	ld	$8,0($6)
	subu	$7,1
	sltu	$24,$12,$8
	dsubu	$8,$12,$8
	dsubu	$12,$8,$2
	sgtu	$2,$12,$8
	sd	$12,0($4)
	daddu	$2,$24
	beqz	$7,.L_bn_sub_words_return

	ld	$13,8($5)
	subu	$7,1
	ld	$9,8($6)
	sltu	$25,$13,$9
	dsubu	$9,$13,$9
	dsubu	$13,$9,$2
	sgtu	$2,$13,$9
	sd	$13,8($4)
	daddu	$2,$25
	beqz	$7,.L_bn_sub_words_return

	ld	$14,2*8($5)
	ld	$10,2*8($6)
	sltu	$24,$14,$10
	dsubu	$10,$14,$10
	dsubu	$14,$10,$2
	sgtu	$2,$14,$10
	sd	$14,2*8($4)
	daddu	$2,$24

.L_bn_sub_words_return:
	.set	noreorder
	jr	$31
	move	$4,$2
.end	bn_sub_words_internal

#if 0
/*
 * The bn_div_3_words entry point is re-used for constant-time interface.
 * Implementation is retained as historical reference.
 */
.align 5
.globl	bn_div_3_words
.ent	bn_div_3_words
bn_div_3_words:
	.set	noreorder
	move	$7,$4		# we know that bn_div_words does not
				# touch $7, $10, $11 and preserves $6
				# so that we can save two arguments
				# and return address in registers
				# instead of stack:-)

	ld	$4,($7)
	move	$10,$5
	bne	$4,$6,bn_div_3_words_internal
	ld	$5,-8($7)
	li	$2,-1
	jr	$31
	move	$4,$2
.end	bn_div_3_words

.align	5
.ent	bn_div_3_words_internal
bn_div_3_words_internal:
	.set	reorder
	move	$11,$31
	bal	bn_div_words_internal
	move	$31,$11
	dmultu	($10,$2)
	ld	$14,-2*8($7)
	move	$8,$0
	mfhi	($13,$10,$2)
	mflo	($12,$10,$2)
	sltu	$24,$13,$5
.L_bn_div_3_words_inner_loop:
	bnez	$24,.L_bn_div_3_words_inner_loop_done
	sgeu	$1,$14,$12
	seq	$25,$13,$5
	and	$1,$25
	sltu	$15,$12,$10
	daddu	$5,$6
	dsubu	$13,$15
	dsubu	$12,$10
	sltu	$24,$13,$5
	sltu	$8,$5,$6
	or	$24,$8
	.set	noreorder
	beqz	$1,.L_bn_div_3_words_inner_loop
	dsubu	$2,1
	daddu	$2,1
	.set	reorder
.L_bn_div_3_words_inner_loop_done:
	.set	noreorder
	jr	$31
	move	$4,$2
.end	bn_div_3_words_internal
#endif

.align	5
.globl	bn_div_words
.ent	bn_div_words
bn_div_words:
	.set	noreorder
	bnez	$6,bn_div_words_internal
	li	$2,-1		# I would rather signal div-by-zero
				# which can be done with 'break 7'
	jr	$31
	move	$4,$2
.end	bn_div_words

.align	5
.ent	bn_div_words_internal
bn_div_words_internal:
	move	$3,$0
	bltz	$6,.L_bn_div_words_body
	move	$25,$3
	dsll	$6,1
	bgtz	$6,.-4
	addu	$25,1

	.set	reorder
	negu	$13,$25
	li	$14,-1
	dsll	$14,$13
	and	$14,$4
	dsrl	$1,$5,$13
	.set	noreorder
	beqz	$14,.+12
	nop
	break	6		# signal overflow
	.set	reorder
	dsll	$4,$25
	dsll	$5,$25
	or	$4,$1
.L_bn_div_words_body:
	dsrl	$3,$6,4*8	# bits
	sgeu	$1,$4,$6
	.set	noreorder
	beqz	$1,.+12
	nop
	dsubu	$4,$6
	.set	reorder

	li	$8,-1
	dsrl	$9,$4,4*8	# bits
	dsrl	$8,4*8	# q=0xffffffff
	beq	$3,$9,.L_bn_div_words_skip_div1
	ddivu	($4,$3)
	mfqt	($8,$4,$3)
.L_bn_div_words_skip_div1:
	dmultu	($6,$8)
	dsll	$15,$4,4*8	# bits
	dsrl	$1,$5,4*8	# bits
	or	$15,$1
	mflo	($12,$6,$8)
	mfhi	($13,$6,$8)
.L_bn_div_words_inner_loop1:
	sltu	$14,$15,$12
	seq	$24,$9,$13
	sltu	$1,$9,$13
	and	$14,$24
	sltu	$2,$12,$6
	or	$1,$14
	.set	noreorder
	beqz	$1,.L_bn_div_words_inner_loop1_done
	dsubu	$13,$2
	dsubu	$12,$6
	b	.L_bn_div_words_inner_loop1
	dsubu	$8,1
	.set	reorder
.L_bn_div_words_inner_loop1_done:

	dsll	$5,4*8	# bits
	dsubu	$4,$15,$12
	dsll	$2,$8,4*8	# bits

	li	$8,-1
	dsrl	$9,$4,4*8	# bits
	dsrl	$8,4*8	# q=0xffffffff
	beq	$3,$9,.L_bn_div_words_skip_div2
	ddivu	($4,$3)
	mfqt	($8,$4,$3)
.L_bn_div_words_skip_div2:
	dmultu	($6,$8)
	dsll	$15,$4,4*8	# bits
	dsrl	$1,$5,4*8	# bits
	or	$15,$1
	mflo	($12,$6,$8)
	mfhi	($13,$6,$8)
.L_bn_div_words_inner_loop2:
	sltu	$14,$15,$12
	seq	$24,$9,$13
	sltu	$1,$9,$13
	and	$14,$24
	sltu	$3,$12,$6
	or	$1,$14
	.set	noreorder
	beqz	$1,.L_bn_div_words_inner_loop2_done
	dsubu	$13,$3
	dsubu	$12,$6
	b	.L_bn_div_words_inner_loop2
	dsubu	$8,1
	.set	reorder
.L_bn_div_words_inner_loop2_done:

	dsubu	$4,$15,$12
	or	$2,$8
	dsrl	$3,$4,$25	# $3 contains remainder if anybody wants it
	dsrl	$6,$25		# restore $6

	.set	noreorder
	move	$5,$3
	jr	$31
	move	$4,$2
.end	bn_div_words_internal

.align	5
.globl	bn_mul_comba8
.ent	bn_mul_comba8
bn_mul_comba8:
	.set	noreorder
	.frame	$29,6*8,$31
	.mask	0x003f0000,-8
	dsubu $29,6*8
	sd	$21,5*8($29)
	sd	$20,4*8($29)
	sd	$19,3*8($29)
	sd	$18,2*8($29)
	sd	$17,1*8($29)
	sd	$16,0*8($29)

	.set	reorder
	ld	$12,0($5)	# If compiled with -mips3 option on
				# R5000 box assembler barks on this
				# 1ine with "should not have mult/div
				# as last instruction in bb (R10K
				# bug)" warning. If anybody out there
				# has a clue about how to circumvent
				# this do send me a note.
				#		<appro@fy.chalmers.se>

	ld	$8,0($6)
	ld	$13,8($5)
	ld	$14,2*8($5)
	dmultu	($12,$8)		# mul_add_c(a[0],b[0],c1,c2,c3);
	ld	$15,3*8($5)
	ld	$9,8($6)
	ld	$10,2*8($6)
	ld	$11,3*8($6)
	mflo	($2,$12,$8)
	mfhi	($3,$12,$8)

	ld	$16,4*8($5)
	ld	$18,5*8($5)
	dmultu	($12,$9)		# mul_add_c(a[0],b[1],c2,c3,c1);
	ld	$20,6*8($5)
	ld	$5,7*8($5)
	ld	$17,4*8($6)
	ld	$19,5*8($6)
	mflo	($24,$12,$9)
	mfhi	($25,$12,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($13,$8)		# mul_add_c(a[1],b[0],c2,c3,c1);
	daddu	$7,$25,$1
	ld	$21,6*8($6)
	ld	$6,7*8($6)
	sd	$2,0($4)	# r[0]=c1;
	mflo	($24,$13,$8)
	mfhi	($25,$13,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$8)		# mul_add_c(a[2],b[0],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	sd	$3,8($4)	# r[1]=c2;

	mflo	($24,$14,$8)
	mfhi	($25,$14,$8)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($13,$9)		# mul_add_c(a[1],b[1],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	mflo	($24,$13,$9)
	mfhi	($25,$13,$9)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($12,$10)		# mul_add_c(a[0],b[2],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$12,$10)
	mfhi	($25,$12,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($12,$11)		# mul_add_c(a[0],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,2*8($4)	# r[2]=c3;

	mflo	($24,$12,$11)
	mfhi	($25,$12,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($13,$10)		# mul_add_c(a[1],b[2],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$7,$3,$25
	mflo	($24,$13,$10)
	mfhi	($25,$13,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($14,$9)		# mul_add_c(a[2],b[1],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$14,$9)
	mfhi	($25,$14,$9)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($15,$8)		# mul_add_c(a[3],b[0],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$15,$8)
	mfhi	($25,$15,$8)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($16,$8)		# mul_add_c(a[4],b[0],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,3*8($4)	# r[3]=c1;

	mflo	($24,$16,$8)
	mfhi	($25,$16,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($15,$9)		# mul_add_c(a[3],b[1],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	mflo	($24,$15,$9)
	mfhi	($25,$15,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($14,$10)		# mul_add_c(a[2],b[2],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$14,$10)
	mfhi	($25,$14,$10)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($13,$11)		# mul_add_c(a[1],b[3],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$13,$11)
	mfhi	($25,$13,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($12,$17)		# mul_add_c(a[0],b[4],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$12,$17)
	mfhi	($25,$12,$17)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($12,$19)		# mul_add_c(a[0],b[5],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,4*8($4)	# r[4]=c2;

	mflo	($24,$12,$19)
	mfhi	($25,$12,$19)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($13,$17)		# mul_add_c(a[1],b[4],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$13,$17)
	mfhi	($25,$13,$17)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($14,$11)		# mul_add_c(a[2],b[3],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$14,$11)
	mfhi	($25,$14,$11)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($15,$10)		# mul_add_c(a[3],b[2],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$15,$10)
	mfhi	($25,$15,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($16,$9)		# mul_add_c(a[4],b[1],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$16,$9)
	mfhi	($25,$16,$9)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($18,$8)		# mul_add_c(a[5],b[0],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$18,$8)
	mfhi	($25,$18,$8)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($20,$8)		# mul_add_c(a[6],b[0],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,5*8($4)	# r[5]=c3;

	mflo	($24,$20,$8)
	mfhi	($25,$20,$8)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($18,$9)		# mul_add_c(a[5],b[1],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$7,$3,$25
	mflo	($24,$18,$9)
	mfhi	($25,$18,$9)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($16,$10)		# mul_add_c(a[4],b[2],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$16,$10)
	mfhi	($25,$16,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($15,$11)		# mul_add_c(a[3],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$15,$11)
	mfhi	($25,$15,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($14,$17)		# mul_add_c(a[2],b[4],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$14,$17)
	mfhi	($25,$14,$17)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($13,$19)		# mul_add_c(a[1],b[5],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$13,$19)
	mfhi	($25,$13,$19)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($12,$21)		# mul_add_c(a[0],b[6],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$12,$21)
	mfhi	($25,$12,$21)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($12,$6)		# mul_add_c(a[0],b[7],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,6*8($4)	# r[6]=c1;

	mflo	($24,$12,$6)
	mfhi	($25,$12,$6)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($13,$21)		# mul_add_c(a[1],b[6],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	mflo	($24,$13,$21)
	mfhi	($25,$13,$21)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($14,$19)		# mul_add_c(a[2],b[5],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$14,$19)
	mfhi	($25,$14,$19)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($15,$17)		# mul_add_c(a[3],b[4],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$15,$17)
	mfhi	($25,$15,$17)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($16,$11)		# mul_add_c(a[4],b[3],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$16,$11)
	mfhi	($25,$16,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($18,$10)		# mul_add_c(a[5],b[2],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$18,$10)
	mfhi	($25,$18,$10)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($20,$9)		# mul_add_c(a[6],b[1],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$20,$9)
	mfhi	($25,$20,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($5,$8)		# mul_add_c(a[7],b[0],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$5,$8)
	mfhi	($25,$5,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($5,$9)		# mul_add_c(a[7],b[1],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,7*8($4)	# r[7]=c2;

	mflo	($24,$5,$9)
	mfhi	($25,$5,$9)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($20,$10)		# mul_add_c(a[6],b[2],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$20,$10)
	mfhi	($25,$20,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($18,$11)		# mul_add_c(a[5],b[3],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$18,$11)
	mfhi	($25,$18,$11)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($16,$17)		# mul_add_c(a[4],b[4],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$16,$17)
	mfhi	($25,$16,$17)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($15,$19)		# mul_add_c(a[3],b[5],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$15,$19)
	mfhi	($25,$15,$19)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($14,$21)		# mul_add_c(a[2],b[6],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$14,$21)
	mfhi	($25,$14,$21)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($13,$6)		# mul_add_c(a[1],b[7],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$13,$6)
	mfhi	($25,$13,$6)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($14,$6)		# mul_add_c(a[2],b[7],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,8*8($4)	# r[8]=c3;

	mflo	($24,$14,$6)
	mfhi	($25,$14,$6)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($15,$21)		# mul_add_c(a[3],b[6],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$7,$3,$25
	mflo	($24,$15,$21)
	mfhi	($25,$15,$21)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($16,$19)		# mul_add_c(a[4],b[5],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$16,$19)
	mfhi	($25,$16,$19)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($18,$17)		# mul_add_c(a[5],b[4],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$18,$17)
	mfhi	($25,$18,$17)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($20,$11)		# mul_add_c(a[6],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$20,$11)
	mfhi	($25,$20,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($5,$10)		# mul_add_c(a[7],b[2],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$5,$10)
	mfhi	($25,$5,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($5,$11)		# mul_add_c(a[7],b[3],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,9*8($4)	# r[9]=c1;

	mflo	($24,$5,$11)
	mfhi	($25,$5,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($20,$17)		# mul_add_c(a[6],b[4],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	mflo	($24,$20,$17)
	mfhi	($25,$20,$17)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($18,$19)		# mul_add_c(a[5],b[5],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$18,$19)
	mfhi	($25,$18,$19)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($16,$21)		# mul_add_c(a[4],b[6],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$16,$21)
	mfhi	($25,$16,$21)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($15,$6)		# mul_add_c(a[3],b[7],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$15,$6)
	mfhi	($25,$15,$6)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($16,$6)		# mul_add_c(a[4],b[7],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,10*8($4)	# r[10]=c2;

	mflo	($24,$16,$6)
	mfhi	($25,$16,$6)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($18,$21)		# mul_add_c(a[5],b[6],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$18,$21)
	mfhi	($25,$18,$21)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($20,$19)		# mul_add_c(a[6],b[5],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$20,$19)
	mfhi	($25,$20,$19)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($5,$17)		# mul_add_c(a[7],b[4],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	mflo	($24,$5,$17)
	mfhi	($25,$5,$17)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($5,$19)		# mul_add_c(a[7],b[5],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,11*8($4)	# r[11]=c3;

	mflo	($24,$5,$19)
	mfhi	($25,$5,$19)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($20,$21)		# mul_add_c(a[6],b[6],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$7,$3,$25
	mflo	($24,$20,$21)
	mfhi	($25,$20,$21)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($18,$6)		# mul_add_c(a[5],b[7],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$18,$6)
	mfhi	($25,$18,$6)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($20,$6)		# mul_add_c(a[6],b[7],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,12*8($4)	# r[12]=c1;

	mflo	($24,$20,$6)
	mfhi	($25,$20,$6)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($5,$21)		# mul_add_c(a[7],b[6],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	mflo	($24,$5,$21)
	mfhi	($25,$5,$21)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($5,$6)		# mul_add_c(a[7],b[7],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,13*8($4)	# r[13]=c2;

	mflo	($24,$5,$6)
	mfhi	($25,$5,$6)
	daddu	$7,$24
	sltu	$1,$7,$24
	daddu	$25,$1
	daddu	$2,$25
	sd	$7,14*8($4)	# r[14]=c3;
	sd	$2,15*8($4)	# r[15]=c1;

	.set	noreorder
	ld	$21,5*8($29)
	ld	$20,4*8($29)
	ld	$19,3*8($29)
	ld	$18,2*8($29)
	ld	$17,1*8($29)
	ld	$16,0*8($29)
	jr	$31
	daddu $29,6*8
.end	bn_mul_comba8

.align	5
.globl	bn_mul_comba4
.ent	bn_mul_comba4
bn_mul_comba4:
	.set	reorder
	ld	$12,0($5)
	ld	$8,0($6)
	ld	$13,8($5)
	ld	$14,2*8($5)
	dmultu	($12,$8)		# mul_add_c(a[0],b[0],c1,c2,c3);
	ld	$15,3*8($5)
	ld	$9,8($6)
	ld	$10,2*8($6)
	ld	$11,3*8($6)
	mflo	($2,$12,$8)
	mfhi	($3,$12,$8)
	sd	$2,0($4)

	dmultu	($12,$9)		# mul_add_c(a[0],b[1],c2,c3,c1);
	mflo	($24,$12,$9)
	mfhi	($25,$12,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($13,$8)		# mul_add_c(a[1],b[0],c2,c3,c1);
	daddu	$7,$25,$1
	mflo	($24,$13,$8)
	mfhi	($25,$13,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$8)		# mul_add_c(a[2],b[0],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	sd	$3,8($4)

	mflo	($24,$14,$8)
	mfhi	($25,$14,$8)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($13,$9)		# mul_add_c(a[1],b[1],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	mflo	($24,$13,$9)
	mfhi	($25,$13,$9)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($12,$10)		# mul_add_c(a[0],b[2],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$12,$10)
	mfhi	($25,$12,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($12,$11)		# mul_add_c(a[0],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,2*8($4)

	mflo	($24,$12,$11)
	mfhi	($25,$12,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($13,$10)		# mul_add_c(a[1],b[2],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$7,$3,$25
	mflo	($24,$13,$10)
	mfhi	($25,$13,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($14,$9)		# mul_add_c(a[2],b[1],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$14,$9)
	mfhi	($25,$14,$9)
	daddu	$2,$24
	sltu	$1,$2,$24
	dmultu	($15,$8)		# mul_add_c(a[3],b[0],c1,c2,c3);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	mflo	($24,$15,$8)
	mfhi	($25,$15,$8)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($15,$9)		# mul_add_c(a[3],b[1],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,3*8($4)

	mflo	($24,$15,$9)
	mfhi	($25,$15,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($14,$10)		# mul_add_c(a[2],b[2],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$2,$7,$25
	mflo	($24,$14,$10)
	mfhi	($25,$14,$10)
	daddu	$3,$24
	sltu	$1,$3,$24
	dmultu	($13,$11)		# mul_add_c(a[1],b[3],c2,c3,c1);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	mflo	($24,$13,$11)
	mfhi	($25,$13,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$11)		# mul_add_c(a[2],b[3],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,4*8($4)

	mflo	($24,$14,$11)
	mfhi	($25,$14,$11)
	daddu	$7,$24
	sltu	$1,$7,$24
	dmultu	($15,$10)		# mul_add_c(a[3],b[2],c3,c1,c2);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$3,$2,$25
	mflo	($24,$15,$10)
	mfhi	($25,$15,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($15,$11)		# mul_add_c(a[3],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,5*8($4)

	mflo	($24,$15,$11)
	mfhi	($25,$15,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	daddu	$25,$1
	daddu	$3,$25
	sd	$2,6*8($4)
	sd	$3,7*8($4)

	.set	noreorder
	jr	$31
	nop
.end	bn_mul_comba4

.align	5
.globl	bn_sqr_comba8
.ent	bn_sqr_comba8
bn_sqr_comba8:
	.set	reorder
	ld	$12,0($5)
	ld	$13,8($5)
	ld	$14,2*8($5)
	ld	$15,3*8($5)

	dmultu	($12,$12)		# mul_add_c(a[0],b[0],c1,c2,c3);
	ld	$8,4*8($5)
	ld	$9,5*8($5)
	ld	$10,6*8($5)
	ld	$11,7*8($5)
	mflo	($2,$12,$12)
	mfhi	($3,$12,$12)
	sd	$2,0($4)

	dmultu	($12,$13)		# mul_add_c2(a[0],b[1],c2,c3,c1);
	mflo	($24,$12,$13)
	mfhi	($25,$12,$13)
	slt	$2,$25,$0
	dsll	$25,1
	 dmultu	($14,$12)		# mul_add_c2(a[2],b[0],c3,c1,c2);
	slt	$6,$24,$0
	daddu	$25,$6
	dsll	$24,1
	daddu	$3,$24
	sltu	$1,$3,$24
	daddu	$7,$25,$1
	sd	$3,8($4)
	mflo	($24,$14,$12)
	mfhi	($25,$14,$12)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($13,$13)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$13,$13)
	mfhi	($25,$13,$13)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($12,$15)		# mul_add_c2(a[0],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,2*8($4)
	mflo	($24,$12,$15)
	mfhi	($25,$12,$15)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($13,$14)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$7,$3,$1
	daddu	$3,$25
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$13,$14)
	mfhi	($25,$13,$14)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($8,$12)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$8,$12)
	mfhi	($25,$8,$12)
	sd	$2,3*8($4)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($15,$13)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$2,$7,$1
	daddu	$7,$25
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$15,$13)
	mfhi	($25,$15,$13)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$14)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$1,$7,$1
	daddu	$7,$25
	daddu	$2,$1
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$14,$14)
	mfhi	($25,$14,$14)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($12,$9)		# mul_add_c2(a[0],b[5],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,4*8($4)
	mflo	($24,$12,$9)
	mfhi	($25,$12,$9)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($13,$8)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$13,$8)
	mfhi	($25,$13,$8)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($14,$15)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$1,$2,$1
	daddu	$2,$25
	daddu	$3,$1
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$14,$15)
	mfhi	($25,$14,$15)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($10,$12)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$1,$2,$1
	daddu	$2,$25
	daddu	$3,$1
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$10,$12)
	mfhi	($25,$10,$12)
	sd	$7,5*8($4)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($9,$13)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$7,$3,$1
	daddu	$3,$25
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$9,$13)
	mfhi	($25,$9,$13)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($8,$14)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$8,$14)
	mfhi	($25,$8,$14)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($15,$15)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$15,$15)
	mfhi	($25,$15,$15)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($12,$11)		# mul_add_c2(a[0],b[7],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,6*8($4)
	mflo	($24,$12,$11)
	mfhi	($25,$12,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($13,$10)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$2,$7,$1
	daddu	$7,$25
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$13,$10)
	mfhi	($25,$13,$10)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$9)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$1,$7,$1
	daddu	$7,$25
	daddu	$2,$1
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$14,$9)
	mfhi	($25,$14,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($15,$8)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$1,$7,$1
	daddu	$7,$25
	daddu	$2,$1
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$15,$8)
	mfhi	($25,$15,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($11,$13)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$1,$7,$1
	daddu	$7,$25
	daddu	$2,$1
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$11,$13)
	mfhi	($25,$11,$13)
	sd	$3,7*8($4)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($10,$14)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$10,$14)
	mfhi	($25,$10,$14)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($9,$15)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$1,$2,$1
	daddu	$2,$25
	daddu	$3,$1
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$9,$15)
	mfhi	($25,$9,$15)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($8,$8)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$1,$2,$1
	daddu	$2,$25
	daddu	$3,$1
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$8,$8)
	mfhi	($25,$8,$8)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($14,$11)		# mul_add_c2(a[2],b[7],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,8*8($4)
	mflo	($24,$14,$11)
	mfhi	($25,$14,$11)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($15,$10)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$7,$3,$1
	daddu	$3,$25
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$15,$10)
	mfhi	($25,$15,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($8,$9)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$8,$9)
	mfhi	($25,$8,$9)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($11,$15)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$11,$15)
	mfhi	($25,$11,$15)
	sd	$2,9*8($4)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($10,$8)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$2,$7,$1
	daddu	$7,$25
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$10,$8)
	mfhi	($25,$10,$8)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($9,$9)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$1,$7,$1
	daddu	$7,$25
	daddu	$2,$1
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$9,$9)
	mfhi	($25,$9,$9)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($8,$11)		# mul_add_c2(a[4],b[7],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,10*8($4)
	mflo	($24,$8,$11)
	mfhi	($25,$8,$11)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($9,$10)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$9,$10)
	mfhi	($25,$9,$10)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($11,$9)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$1,$2,$1
	daddu	$2,$25
	daddu	$3,$1
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$11,$9)
	mfhi	($25,$11,$9)
	sd	$7,11*8($4)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($10,$10)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$7,$3,$1
	daddu	$3,$25
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$10,$10)
	mfhi	($25,$10,$10)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($10,$11)		# mul_add_c2(a[6],b[7],c2,c3,c1);
	daddu	$25,$1
	daddu	$3,$25
	sltu	$1,$3,$25
	daddu	$7,$1
	sd	$2,12*8($4)
	mflo	($24,$10,$11)
	mfhi	($25,$10,$11)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($11,$11)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$2,$7,$1
	daddu	$7,$25
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$11,$11)
	mfhi	($25,$11,$11)
	sd	$3,13*8($4)

	daddu	$7,$24
	sltu	$1,$7,$24
	daddu	$25,$1
	daddu	$2,$25
	sd	$7,14*8($4)
	sd	$2,15*8($4)

	.set	noreorder
	jr	$31
	nop
.end	bn_sqr_comba8

.align	5
.globl	bn_sqr_comba4
.ent	bn_sqr_comba4
bn_sqr_comba4:
	.set	reorder
	ld	$12,0($5)
	ld	$13,8($5)
	dmultu	($12,$12)		# mul_add_c(a[0],b[0],c1,c2,c3);
	ld	$14,2*8($5)
	ld	$15,3*8($5)
	mflo	($2,$12,$12)
	mfhi	($3,$12,$12)
	sd	$2,0($4)

	dmultu	($12,$13)		# mul_add_c2(a[0],b[1],c2,c3,c1);
	mflo	($24,$12,$13)
	mfhi	($25,$12,$13)
	slt	$2,$25,$0
	dsll	$25,1
	 dmultu	($14,$12)		# mul_add_c2(a[2],b[0],c3,c1,c2);
	slt	$6,$24,$0
	daddu	$25,$6
	dsll	$24,1
	daddu	$3,$24
	sltu	$1,$3,$24
	daddu	$7,$25,$1
	sd	$3,8($4)
	mflo	($24,$14,$12)
	mfhi	($25,$14,$12)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($13,$13)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$13,$13)
	mfhi	($25,$13,$13)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($12,$15)		# mul_add_c2(a[0],b[3],c1,c2,c3);
	daddu	$25,$1
	daddu	$2,$25
	sltu	$1,$2,$25
	daddu	$3,$1
	sd	$7,2*8($4)
	mflo	($24,$12,$15)
	mfhi	($25,$12,$15)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($13,$14)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$7,$3,$1
	daddu	$3,$25
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$13,$14)
	mfhi	($25,$13,$14)
	daddu	$2,$24
	sltu	$1,$2,$24
	 dmultu	($15,$13)		# forward multiplication
	daddu	$2,$24
	daddu	$1,$25
	sltu	$24,$2,$24
	daddu	$3,$1
	daddu	$25,$24
	sltu	$1,$3,$1
	daddu	$3,$25
	daddu	$7,$1
	sltu	$25,$3,$25
	daddu	$7,$25
	mflo	($24,$15,$13)
	mfhi	($25,$15,$13)
	sd	$2,3*8($4)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$14)		# forward multiplication
	daddu	$3,$24
	daddu	$1,$25
	sltu	$24,$3,$24
	daddu	$7,$1
	daddu	$25,$24
	sltu	$2,$7,$1
	daddu	$7,$25
	sltu	$25,$7,$25
	daddu	$2,$25
	mflo	($24,$14,$14)
	mfhi	($25,$14,$14)
	daddu	$3,$24
	sltu	$1,$3,$24
	 dmultu	($14,$15)		# mul_add_c2(a[2],b[3],c3,c1,c2);
	daddu	$25,$1
	daddu	$7,$25
	sltu	$1,$7,$25
	daddu	$2,$1
	sd	$3,4*8($4)
	mflo	($24,$14,$15)
	mfhi	($25,$14,$15)
	daddu	$7,$24
	sltu	$1,$7,$24
	 dmultu	($15,$15)		# forward multiplication
	daddu	$7,$24
	daddu	$1,$25
	sltu	$24,$7,$24
	daddu	$2,$1
	daddu	$25,$24
	sltu	$3,$2,$1
	daddu	$2,$25
	sltu	$25,$2,$25
	daddu	$3,$25
	mflo	($24,$15,$15)
	mfhi	($25,$15,$15)
	sd	$7,5*8($4)

	daddu	$2,$24
	sltu	$1,$2,$24
	daddu	$25,$1
	daddu	$3,$25
	sd	$2,6*8($4)
	sd	$3,7*8($4)

	.set	noreorder
	jr	$31
	nop
.end	bn_sqr_comba4
