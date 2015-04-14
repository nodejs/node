.rdata
.asciiz	"mips3.s, Version 1.1"
.asciiz	"MIPS III/IV ISA artwork by Andy Polyakov <appro@fy.chalmers.se>"

/*
 * ====================================================================
 * Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
 * project.
 *
 * Rights for redistribution and usage in source and binary forms are
 * granted according to the OpenSSL license. Warranty of any kind is
 * disclaimed.
 * ====================================================================
 */

/*
 * This is my modest contributon to the OpenSSL project (see
 * http://www.openssl.org/ for more information about it) and is
 * a drop-in MIPS III/IV ISA replacement for crypto/bn/bn_asm.c
 * module. For updates see http://fy.chalmers.se/~appro/hpe/.
 *
 * The module is designed to work with either of the "new" MIPS ABI(5),
 * namely N32 or N64, offered by IRIX 6.x. It's not ment to work under
 * IRIX 5.x not only because it doesn't support new ABIs but also
 * because 5.x kernels put R4x00 CPU into 32-bit mode and all those
 * 64-bit instructions (daddu, dmultu, etc.) found below gonna only
 * cause illegal instruction exception:-(
 *
 * In addition the code depends on preprocessor flags set up by MIPSpro
 * compiler driver (either as or cc) and therefore (probably?) can't be
 * compiled by the GNU assembler. GNU C driver manages fine though...
 * I mean as long as -mmips-as is specified or is the default option,
 * because then it simply invokes /usr/bin/as which in turn takes
 * perfect care of the preprocessor definitions. Another neat feature
 * offered by the MIPSpro assembler is an optimization pass. This gave
 * me the opportunity to have the code looking more regular as all those
 * architecture dependent instruction rescheduling details were left to
 * the assembler. Cool, huh?
 *
 * Performance improvement is astonishing! 'apps/openssl speed rsa dsa'
 * goes way over 3 times faster!
 *
 *					<appro@fy.chalmers.se>
 */
#include <asm.h>
#include <regdef.h>

#if _MIPS_ISA>=4
#define	MOVNZ(cond,dst,src)	\
	movn	dst,src,cond
#else
#define	MOVNZ(cond,dst,src)	\
	.set	noreorder;	\
	bnezl	cond,.+8;	\
	move	dst,src;	\
	.set	reorder
#endif

.text

.set	noat
.set	reorder

#define	MINUS4	v1

.align	5
LEAF(bn_mul_add_words)
	.set	noreorder
	bgtzl	a2,.L_bn_mul_add_words_proceed
	ld	t0,0(a1)
	jr	ra
	move	v0,zero
	.set	reorder

.L_bn_mul_add_words_proceed:
	li	MINUS4,-4
	and	ta0,a2,MINUS4
	move	v0,zero
	beqz	ta0,.L_bn_mul_add_words_tail

.L_bn_mul_add_words_loop:
	dmultu	t0,a3
	ld	t1,0(a0)
	ld	t2,8(a1)
	ld	t3,8(a0)
	ld	ta0,16(a1)
	ld	ta1,16(a0)
	daddu	t1,v0
	sltu	v0,t1,v0	/* All manuals say it "compares 32-bit
				 * values", but it seems to work fine
				 * even on 64-bit registers. */
	mflo	AT
	mfhi	t0
	daddu	t1,AT
	daddu	v0,t0
	sltu	AT,t1,AT
	sd	t1,0(a0)
	daddu	v0,AT

	dmultu	t2,a3
	ld	ta2,24(a1)
	ld	ta3,24(a0)
	daddu	t3,v0
	sltu	v0,t3,v0
	mflo	AT
	mfhi	t2
	daddu	t3,AT
	daddu	v0,t2
	sltu	AT,t3,AT
	sd	t3,8(a0)
	daddu	v0,AT

	dmultu	ta0,a3
	subu	a2,4
	PTR_ADD	a0,32
	PTR_ADD	a1,32
	daddu	ta1,v0
	sltu	v0,ta1,v0
	mflo	AT
	mfhi	ta0
	daddu	ta1,AT
	daddu	v0,ta0
	sltu	AT,ta1,AT
	sd	ta1,-16(a0)
	daddu	v0,AT


	dmultu	ta2,a3
	and	ta0,a2,MINUS4
	daddu	ta3,v0
	sltu	v0,ta3,v0
	mflo	AT
	mfhi	ta2
	daddu	ta3,AT
	daddu	v0,ta2
	sltu	AT,ta3,AT
	sd	ta3,-8(a0)
	daddu	v0,AT
	.set	noreorder
	bgtzl	ta0,.L_bn_mul_add_words_loop
	ld	t0,0(a1)

	bnezl	a2,.L_bn_mul_add_words_tail
	ld	t0,0(a1)
	.set	reorder

.L_bn_mul_add_words_return:
	jr	ra

.L_bn_mul_add_words_tail:
	dmultu	t0,a3
	ld	t1,0(a0)
	subu	a2,1
	daddu	t1,v0
	sltu	v0,t1,v0
	mflo	AT
	mfhi	t0
	daddu	t1,AT
	daddu	v0,t0
	sltu	AT,t1,AT
	sd	t1,0(a0)
	daddu	v0,AT
	beqz	a2,.L_bn_mul_add_words_return

	ld	t0,8(a1)
	dmultu	t0,a3
	ld	t1,8(a0)
	subu	a2,1
	daddu	t1,v0
	sltu	v0,t1,v0
	mflo	AT
	mfhi	t0
	daddu	t1,AT
	daddu	v0,t0
	sltu	AT,t1,AT
	sd	t1,8(a0)
	daddu	v0,AT
	beqz	a2,.L_bn_mul_add_words_return

	ld	t0,16(a1)
	dmultu	t0,a3
	ld	t1,16(a0)
	daddu	t1,v0
	sltu	v0,t1,v0
	mflo	AT
	mfhi	t0
	daddu	t1,AT
	daddu	v0,t0
	sltu	AT,t1,AT
	sd	t1,16(a0)
	daddu	v0,AT
	jr	ra
END(bn_mul_add_words)

.align	5
LEAF(bn_mul_words)
	.set	noreorder
	bgtzl	a2,.L_bn_mul_words_proceed
	ld	t0,0(a1)
	jr	ra
	move	v0,zero
	.set	reorder

.L_bn_mul_words_proceed:
	li	MINUS4,-4
	and	ta0,a2,MINUS4
	move	v0,zero
	beqz	ta0,.L_bn_mul_words_tail

.L_bn_mul_words_loop:
	dmultu	t0,a3
	ld	t2,8(a1)
	ld	ta0,16(a1)
	ld	ta2,24(a1)
	mflo	AT
	mfhi	t0
	daddu	v0,AT
	sltu	t1,v0,AT
	sd	v0,0(a0)
	daddu	v0,t1,t0

	dmultu	t2,a3
	subu	a2,4
	PTR_ADD	a0,32
	PTR_ADD	a1,32
	mflo	AT
	mfhi	t2
	daddu	v0,AT
	sltu	t3,v0,AT
	sd	v0,-24(a0)
	daddu	v0,t3,t2

	dmultu	ta0,a3
	mflo	AT
	mfhi	ta0
	daddu	v0,AT
	sltu	ta1,v0,AT
	sd	v0,-16(a0)
	daddu	v0,ta1,ta0


	dmultu	ta2,a3
	and	ta0,a2,MINUS4
	mflo	AT
	mfhi	ta2
	daddu	v0,AT
	sltu	ta3,v0,AT
	sd	v0,-8(a0)
	daddu	v0,ta3,ta2
	.set	noreorder
	bgtzl	ta0,.L_bn_mul_words_loop
	ld	t0,0(a1)

	bnezl	a2,.L_bn_mul_words_tail
	ld	t0,0(a1)
	.set	reorder

.L_bn_mul_words_return:
	jr	ra

.L_bn_mul_words_tail:
	dmultu	t0,a3
	subu	a2,1
	mflo	AT
	mfhi	t0
	daddu	v0,AT
	sltu	t1,v0,AT
	sd	v0,0(a0)
	daddu	v0,t1,t0
	beqz	a2,.L_bn_mul_words_return

	ld	t0,8(a1)
	dmultu	t0,a3
	subu	a2,1
	mflo	AT
	mfhi	t0
	daddu	v0,AT
	sltu	t1,v0,AT
	sd	v0,8(a0)
	daddu	v0,t1,t0
	beqz	a2,.L_bn_mul_words_return

	ld	t0,16(a1)
	dmultu	t0,a3
	mflo	AT
	mfhi	t0
	daddu	v0,AT
	sltu	t1,v0,AT
	sd	v0,16(a0)
	daddu	v0,t1,t0
	jr	ra
END(bn_mul_words)

.align	5
LEAF(bn_sqr_words)
	.set	noreorder
	bgtzl	a2,.L_bn_sqr_words_proceed
	ld	t0,0(a1)
	jr	ra
	move	v0,zero
	.set	reorder

.L_bn_sqr_words_proceed:
	li	MINUS4,-4
	and	ta0,a2,MINUS4
	move	v0,zero
	beqz	ta0,.L_bn_sqr_words_tail

.L_bn_sqr_words_loop:
	dmultu	t0,t0
	ld	t2,8(a1)
	ld	ta0,16(a1)
	ld	ta2,24(a1)
	mflo	t1
	mfhi	t0
	sd	t1,0(a0)
	sd	t0,8(a0)

	dmultu	t2,t2
	subu	a2,4
	PTR_ADD	a0,64
	PTR_ADD	a1,32
	mflo	t3
	mfhi	t2
	sd	t3,-48(a0)
	sd	t2,-40(a0)

	dmultu	ta0,ta0
	mflo	ta1
	mfhi	ta0
	sd	ta1,-32(a0)
	sd	ta0,-24(a0)


	dmultu	ta2,ta2
	and	ta0,a2,MINUS4
	mflo	ta3
	mfhi	ta2
	sd	ta3,-16(a0)
	sd	ta2,-8(a0)

	.set	noreorder
	bgtzl	ta0,.L_bn_sqr_words_loop
	ld	t0,0(a1)

	bnezl	a2,.L_bn_sqr_words_tail
	ld	t0,0(a1)
	.set	reorder

.L_bn_sqr_words_return:
	move	v0,zero
	jr	ra

.L_bn_sqr_words_tail:
	dmultu	t0,t0
	subu	a2,1
	mflo	t1
	mfhi	t0
	sd	t1,0(a0)
	sd	t0,8(a0)
	beqz	a2,.L_bn_sqr_words_return

	ld	t0,8(a1)
	dmultu	t0,t0
	subu	a2,1
	mflo	t1
	mfhi	t0
	sd	t1,16(a0)
	sd	t0,24(a0)
	beqz	a2,.L_bn_sqr_words_return

	ld	t0,16(a1)
	dmultu	t0,t0
	mflo	t1
	mfhi	t0
	sd	t1,32(a0)
	sd	t0,40(a0)
	jr	ra
END(bn_sqr_words)

.align	5
LEAF(bn_add_words)
	.set	noreorder
	bgtzl	a3,.L_bn_add_words_proceed
	ld	t0,0(a1)
	jr	ra
	move	v0,zero
	.set	reorder

.L_bn_add_words_proceed:
	li	MINUS4,-4
	and	AT,a3,MINUS4
	move	v0,zero
	beqz	AT,.L_bn_add_words_tail

.L_bn_add_words_loop:
	ld	ta0,0(a2)
	subu	a3,4
	ld	t1,8(a1)
	and	AT,a3,MINUS4
	ld	t2,16(a1)
	PTR_ADD	a2,32
	ld	t3,24(a1)
	PTR_ADD	a0,32
	ld	ta1,-24(a2)
	PTR_ADD	a1,32
	ld	ta2,-16(a2)
	ld	ta3,-8(a2)
	daddu	ta0,t0
	sltu	t8,ta0,t0
	daddu	t0,ta0,v0
	sltu	v0,t0,ta0
	sd	t0,-32(a0)
	daddu	v0,t8

	daddu	ta1,t1
	sltu	t9,ta1,t1
	daddu	t1,ta1,v0
	sltu	v0,t1,ta1
	sd	t1,-24(a0)
	daddu	v0,t9

	daddu	ta2,t2
	sltu	t8,ta2,t2
	daddu	t2,ta2,v0
	sltu	v0,t2,ta2
	sd	t2,-16(a0)
	daddu	v0,t8
	
	daddu	ta3,t3
	sltu	t9,ta3,t3
	daddu	t3,ta3,v0
	sltu	v0,t3,ta3
	sd	t3,-8(a0)
	daddu	v0,t9
	
	.set	noreorder
	bgtzl	AT,.L_bn_add_words_loop
	ld	t0,0(a1)

	bnezl	a3,.L_bn_add_words_tail
	ld	t0,0(a1)
	.set	reorder

.L_bn_add_words_return:
	jr	ra

.L_bn_add_words_tail:
	ld	ta0,0(a2)
	daddu	ta0,t0
	subu	a3,1
	sltu	t8,ta0,t0
	daddu	t0,ta0,v0
	sltu	v0,t0,ta0
	sd	t0,0(a0)
	daddu	v0,t8
	beqz	a3,.L_bn_add_words_return

	ld	t1,8(a1)
	ld	ta1,8(a2)
	daddu	ta1,t1
	subu	a3,1
	sltu	t9,ta1,t1
	daddu	t1,ta1,v0
	sltu	v0,t1,ta1
	sd	t1,8(a0)
	daddu	v0,t9
	beqz	a3,.L_bn_add_words_return

	ld	t2,16(a1)
	ld	ta2,16(a2)
	daddu	ta2,t2
	sltu	t8,ta2,t2
	daddu	t2,ta2,v0
	sltu	v0,t2,ta2
	sd	t2,16(a0)
	daddu	v0,t8
	jr	ra
END(bn_add_words)

.align	5
LEAF(bn_sub_words)
	.set	noreorder
	bgtzl	a3,.L_bn_sub_words_proceed
	ld	t0,0(a1)
	jr	ra
	move	v0,zero
	.set	reorder

.L_bn_sub_words_proceed:
	li	MINUS4,-4
	and	AT,a3,MINUS4
	move	v0,zero
	beqz	AT,.L_bn_sub_words_tail

.L_bn_sub_words_loop:
	ld	ta0,0(a2)
	subu	a3,4
	ld	t1,8(a1)
	and	AT,a3,MINUS4
	ld	t2,16(a1)
	PTR_ADD	a2,32
	ld	t3,24(a1)
	PTR_ADD	a0,32
	ld	ta1,-24(a2)
	PTR_ADD	a1,32
	ld	ta2,-16(a2)
	ld	ta3,-8(a2)
	sltu	t8,t0,ta0
	dsubu	t0,ta0
	dsubu	ta0,t0,v0
	sd	ta0,-32(a0)
	MOVNZ	(t0,v0,t8)

	sltu	t9,t1,ta1
	dsubu	t1,ta1
	dsubu	ta1,t1,v0
	sd	ta1,-24(a0)
	MOVNZ	(t1,v0,t9)


	sltu	t8,t2,ta2
	dsubu	t2,ta2
	dsubu	ta2,t2,v0
	sd	ta2,-16(a0)
	MOVNZ	(t2,v0,t8)

	sltu	t9,t3,ta3
	dsubu	t3,ta3
	dsubu	ta3,t3,v0
	sd	ta3,-8(a0)
	MOVNZ	(t3,v0,t9)

	.set	noreorder
	bgtzl	AT,.L_bn_sub_words_loop
	ld	t0,0(a1)

	bnezl	a3,.L_bn_sub_words_tail
	ld	t0,0(a1)
	.set	reorder

.L_bn_sub_words_return:
	jr	ra

.L_bn_sub_words_tail:
	ld	ta0,0(a2)
	subu	a3,1
	sltu	t8,t0,ta0
	dsubu	t0,ta0
	dsubu	ta0,t0,v0
	MOVNZ	(t0,v0,t8)
	sd	ta0,0(a0)
	beqz	a3,.L_bn_sub_words_return

	ld	t1,8(a1)
	subu	a3,1
	ld	ta1,8(a2)
	sltu	t9,t1,ta1
	dsubu	t1,ta1
	dsubu	ta1,t1,v0
	MOVNZ	(t1,v0,t9)
	sd	ta1,8(a0)
	beqz	a3,.L_bn_sub_words_return

	ld	t2,16(a1)
	ld	ta2,16(a2)
	sltu	t8,t2,ta2
	dsubu	t2,ta2
	dsubu	ta2,t2,v0
	MOVNZ	(t2,v0,t8)
	sd	ta2,16(a0)
	jr	ra
END(bn_sub_words)

#undef	MINUS4

.align 5
LEAF(bn_div_3_words)
	.set	reorder
	move	a3,a0		/* we know that bn_div_words doesn't
				 * touch a3, ta2, ta3 and preserves a2
				 * so that we can save two arguments
				 * and return address in registers
				 * instead of stack:-)
				 */
	ld	a0,(a3)
	move	ta2,a1
	ld	a1,-8(a3)
	bne	a0,a2,.L_bn_div_3_words_proceed
	li	v0,-1
	jr	ra
.L_bn_div_3_words_proceed:
	move	ta3,ra
	bal	bn_div_words
	move	ra,ta3
	dmultu	ta2,v0
	ld	t2,-16(a3)
	move	ta0,zero
	mfhi	t1
	mflo	t0
	sltu	t8,t1,v1
.L_bn_div_3_words_inner_loop:
	bnez	t8,.L_bn_div_3_words_inner_loop_done
	sgeu	AT,t2,t0
	seq	t9,t1,v1
	and	AT,t9
	sltu	t3,t0,ta2
	daddu	v1,a2
	dsubu	t1,t3
	dsubu	t0,ta2
	sltu	t8,t1,v1
	sltu	ta0,v1,a2
	or	t8,ta0
	.set	noreorder
	beqzl	AT,.L_bn_div_3_words_inner_loop
	dsubu	v0,1
	.set	reorder
.L_bn_div_3_words_inner_loop_done:
	jr	ra
END(bn_div_3_words)

.align	5
LEAF(bn_div_words)
	.set	noreorder
	bnezl	a2,.L_bn_div_words_proceed
	move	v1,zero
	jr	ra
	li	v0,-1		/* I'd rather signal div-by-zero
				 * which can be done with 'break 7' */

.L_bn_div_words_proceed:
	bltz	a2,.L_bn_div_words_body
	move	t9,v1
	dsll	a2,1
	bgtz	a2,.-4
	addu	t9,1

	.set	reorder
	negu	t1,t9
	li	t2,-1
	dsll	t2,t1
	and	t2,a0
	dsrl	AT,a1,t1
	.set	noreorder
	bnezl	t2,.+8
	break	6		/* signal overflow */
	.set	reorder
	dsll	a0,t9
	dsll	a1,t9
	or	a0,AT

#define	QT	ta0
#define	HH	ta1
#define	DH	v1
.L_bn_div_words_body:
	dsrl	DH,a2,32
	sgeu	AT,a0,a2
	.set	noreorder
	bnezl	AT,.+8
	dsubu	a0,a2
	.set	reorder

	li	QT,-1
	dsrl	HH,a0,32
	dsrl	QT,32	/* q=0xffffffff */
	beq	DH,HH,.L_bn_div_words_skip_div1
	ddivu	zero,a0,DH
	mflo	QT
.L_bn_div_words_skip_div1:
	dmultu	a2,QT
	dsll	t3,a0,32
	dsrl	AT,a1,32
	or	t3,AT
	mflo	t0
	mfhi	t1
.L_bn_div_words_inner_loop1:
	sltu	t2,t3,t0
	seq	t8,HH,t1
	sltu	AT,HH,t1
	and	t2,t8
	sltu	v0,t0,a2
	or	AT,t2
	.set	noreorder
	beqz	AT,.L_bn_div_words_inner_loop1_done
	dsubu	t1,v0
	dsubu	t0,a2
	b	.L_bn_div_words_inner_loop1
	dsubu	QT,1
	.set	reorder
.L_bn_div_words_inner_loop1_done:

	dsll	a1,32
	dsubu	a0,t3,t0
	dsll	v0,QT,32

	li	QT,-1
	dsrl	HH,a0,32
	dsrl	QT,32	/* q=0xffffffff */
	beq	DH,HH,.L_bn_div_words_skip_div2
	ddivu	zero,a0,DH
	mflo	QT
.L_bn_div_words_skip_div2:
#undef	DH
	dmultu	a2,QT
	dsll	t3,a0,32
	dsrl	AT,a1,32
	or	t3,AT
	mflo	t0
	mfhi	t1
.L_bn_div_words_inner_loop2:
	sltu	t2,t3,t0
	seq	t8,HH,t1
	sltu	AT,HH,t1
	and	t2,t8
	sltu	v1,t0,a2
	or	AT,t2
	.set	noreorder
	beqz	AT,.L_bn_div_words_inner_loop2_done
	dsubu	t1,v1
	dsubu	t0,a2
	b	.L_bn_div_words_inner_loop2
	dsubu	QT,1
	.set	reorder
.L_bn_div_words_inner_loop2_done:	
#undef	HH

	dsubu	a0,t3,t0
	or	v0,QT
	dsrl	v1,a0,t9	/* v1 contains remainder if anybody wants it */
	dsrl	a2,t9		/* restore a2 */
	jr	ra
#undef	QT
END(bn_div_words)

#define	a_0	t0
#define	a_1	t1
#define	a_2	t2
#define	a_3	t3
#define	b_0	ta0
#define	b_1	ta1
#define	b_2	ta2
#define	b_3	ta3

#define	a_4	s0
#define	a_5	s2
#define	a_6	s4
#define	a_7	a1	/* once we load a[7] we don't need a anymore */
#define	b_4	s1
#define	b_5	s3
#define	b_6	s5
#define	b_7	a2	/* once we load b[7] we don't need b anymore */

#define	t_1	t8
#define	t_2	t9

#define	c_1	v0
#define	c_2	v1
#define	c_3	a3

#define	FRAME_SIZE	48

.align	5
LEAF(bn_mul_comba8)
	.set	noreorder
	PTR_SUB	sp,FRAME_SIZE
	.frame	sp,64,ra
	.set	reorder
	ld	a_0,0(a1)	/* If compiled with -mips3 option on
				 * R5000 box assembler barks on this
				 * line with "shouldn't have mult/div
				 * as last instruction in bb (R10K
				 * bug)" warning. If anybody out there
				 * has a clue about how to circumvent
				 * this do send me a note.
				 *		<appro@fy.chalmers.se>
				 */
	ld	b_0,0(a2)
	ld	a_1,8(a1)
	ld	a_2,16(a1)
	ld	a_3,24(a1)
	ld	b_1,8(a2)
	ld	b_2,16(a2)
	ld	b_3,24(a2)
	dmultu	a_0,b_0		/* mul_add_c(a[0],b[0],c1,c2,c3); */
	sd	s0,0(sp)
	sd	s1,8(sp)
	sd	s2,16(sp)
	sd	s3,24(sp)
	sd	s4,32(sp)
	sd	s5,40(sp)
	mflo	c_1
	mfhi	c_2

	dmultu	a_0,b_1		/* mul_add_c(a[0],b[1],c2,c3,c1); */
	ld	a_4,32(a1)
	ld	a_5,40(a1)
	ld	a_6,48(a1)
	ld	a_7,56(a1)
	ld	b_4,32(a2)
	ld	b_5,40(a2)
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	c_3,t_2,AT
	dmultu	a_1,b_0		/* mul_add_c(a[1],b[0],c2,c3,c1); */
	ld	b_6,48(a2)
	ld	b_7,56(a2)
	sd	c_1,0(a0)	/* r[0]=c1; */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	sd	c_2,8(a0)	/* r[1]=c2; */

	dmultu	a_2,b_0		/* mul_add_c(a[2],b[0],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	dmultu	a_1,b_1		/* mul_add_c(a[1],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_0,b_2		/* mul_add_c(a[0],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,16(a0)	/* r[2]=c3; */

	dmultu	a_0,b_3		/* mul_add_c(a[0],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	c_3,c_2,t_2
	dmultu	a_1,b_2		/* mul_add_c(a[1],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_2,b_1		/* mul_add_c(a[2],b[1],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_3,b_0		/* mul_add_c(a[3],b[0],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,24(a0)	/* r[3]=c1; */

	dmultu	a_4,b_0		/* mul_add_c(a[4],b[0],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	dmultu	a_3,b_1		/* mul_add_c(a[3],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_2,b_2		/* mul_add_c(a[2],b[2],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_1,b_3		/* mul_add_c(a[1],b[3],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_0,b_4		/* mul_add_c(a[0],b[4],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,32(a0)	/* r[4]=c2; */

	dmultu	a_0,b_5		/* mul_add_c(a[0],b[5],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_1,b_4		/* mul_add_c(a[1],b[4],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_2,b_3		/* mul_add_c(a[2],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_3,b_2		/* mul_add_c(a[3],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_4,b_1		/* mul_add_c(a[4],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_5,b_0		/* mul_add_c(a[5],b[0],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,40(a0)	/* r[5]=c3; */

	dmultu	a_6,b_0		/* mul_add_c(a[6],b[0],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	c_3,c_2,t_2
	dmultu	a_5,b_1		/* mul_add_c(a[5],b[1],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_4,b_2		/* mul_add_c(a[4],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_3,b_3		/* mul_add_c(a[3],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_2,b_4		/* mul_add_c(a[2],b[4],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_1,b_5		/* mul_add_c(a[1],b[5],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_0,b_6		/* mul_add_c(a[0],b[6],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,48(a0)	/* r[6]=c1; */

	dmultu	a_0,b_7		/* mul_add_c(a[0],b[7],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	dmultu	a_1,b_6		/* mul_add_c(a[1],b[6],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_2,b_5		/* mul_add_c(a[2],b[5],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_3,b_4		/* mul_add_c(a[3],b[4],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_4,b_3		/* mul_add_c(a[4],b[3],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_5,b_2		/* mul_add_c(a[5],b[2],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_6,b_1		/* mul_add_c(a[6],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_7,b_0		/* mul_add_c(a[7],b[0],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,56(a0)	/* r[7]=c2; */

	dmultu	a_7,b_1		/* mul_add_c(a[7],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_6,b_2		/* mul_add_c(a[6],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_5,b_3		/* mul_add_c(a[5],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_4,b_4		/* mul_add_c(a[4],b[4],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_3,b_5		/* mul_add_c(a[3],b[5],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_2,b_6		/* mul_add_c(a[2],b[6],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_1,b_7		/* mul_add_c(a[1],b[7],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,64(a0)	/* r[8]=c3; */

	dmultu	a_2,b_7		/* mul_add_c(a[2],b[7],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	c_3,c_2,t_2
	dmultu	a_3,b_6		/* mul_add_c(a[3],b[6],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_4,b_5		/* mul_add_c(a[4],b[5],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_5,b_4		/* mul_add_c(a[5],b[4],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_6,b_3		/* mul_add_c(a[6],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_7,b_2		/* mul_add_c(a[7],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,72(a0)	/* r[9]=c1; */

	dmultu	a_7,b_3		/* mul_add_c(a[7],b[3],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	dmultu	a_6,b_4		/* mul_add_c(a[6],b[4],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_5,b_5		/* mul_add_c(a[5],b[5],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_4,b_6		/* mul_add_c(a[4],b[6],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_3,b_7		/* mul_add_c(a[3],b[7],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,80(a0)	/* r[10]=c2; */

	dmultu	a_4,b_7		/* mul_add_c(a[4],b[7],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_5,b_6		/* mul_add_c(a[5],b[6],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_6,b_5		/* mul_add_c(a[6],b[5],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_7,b_4		/* mul_add_c(a[7],b[4],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,88(a0)	/* r[11]=c3; */

	dmultu	a_7,b_5		/* mul_add_c(a[7],b[5],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	c_3,c_2,t_2
	dmultu	a_6,b_6		/* mul_add_c(a[6],b[6],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_5,b_7		/* mul_add_c(a[5],b[7],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,96(a0)	/* r[12]=c1; */

	dmultu	a_6,b_7		/* mul_add_c(a[6],b[7],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	dmultu	a_7,b_6		/* mul_add_c(a[7],b[6],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,104(a0)	/* r[13]=c2; */

	dmultu	a_7,b_7		/* mul_add_c(a[7],b[7],c3,c1,c2); */
	ld	s0,0(sp)
	ld	s1,8(sp)
	ld	s2,16(sp)
	ld	s3,24(sp)
	ld	s4,32(sp)
	ld	s5,40(sp)
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sd	c_3,112(a0)	/* r[14]=c3; */
	sd	c_1,120(a0)	/* r[15]=c1; */

	PTR_ADD	sp,FRAME_SIZE

	jr	ra
END(bn_mul_comba8)

.align	5
LEAF(bn_mul_comba4)
	.set	reorder
	ld	a_0,0(a1)
	ld	b_0,0(a2)
	ld	a_1,8(a1)
	ld	a_2,16(a1)
	dmultu	a_0,b_0		/* mul_add_c(a[0],b[0],c1,c2,c3); */
	ld	a_3,24(a1)
	ld	b_1,8(a2)
	ld	b_2,16(a2)
	ld	b_3,24(a2)
	mflo	c_1
	mfhi	c_2
	sd	c_1,0(a0)

	dmultu	a_0,b_1		/* mul_add_c(a[0],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	c_3,t_2,AT
	dmultu	a_1,b_0		/* mul_add_c(a[1],b[0],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	sd	c_2,8(a0)

	dmultu	a_2,b_0		/* mul_add_c(a[2],b[0],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	dmultu	a_1,b_1		/* mul_add_c(a[1],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_0,b_2		/* mul_add_c(a[0],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,16(a0)

	dmultu	a_0,b_3		/* mul_add_c(a[0],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	c_3,c_2,t_2
	dmultu	a_1,b_2		/* mul_add_c(a[1],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_2,b_1		/* mul_add_c(a[2],b[1],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_3,b_0		/* mul_add_c(a[3],b[0],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,24(a0)

	dmultu	a_3,b_1		/* mul_add_c(a[3],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	c_1,c_3,t_2
	dmultu	a_2,b_2		/* mul_add_c(a[2],b[2],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_1,b_3		/* mul_add_c(a[1],b[3],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,32(a0)

	dmultu	a_2,b_3		/* mul_add_c(a[2],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	c_2,c_1,t_2
	dmultu	a_3,b_2		/* mul_add_c(a[3],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,40(a0)

	dmultu	a_3,b_3		/* mul_add_c(a[3],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sd	c_1,48(a0)
	sd	c_2,56(a0)

	jr	ra
END(bn_mul_comba4)

#undef	a_4
#undef	a_5
#undef	a_6
#undef	a_7
#define	a_4	b_0
#define	a_5	b_1
#define	a_6	b_2
#define	a_7	b_3

.align	5
LEAF(bn_sqr_comba8)
	.set	reorder
	ld	a_0,0(a1)
	ld	a_1,8(a1)
	ld	a_2,16(a1)
	ld	a_3,24(a1)

	dmultu	a_0,a_0		/* mul_add_c(a[0],b[0],c1,c2,c3); */
	ld	a_4,32(a1)
	ld	a_5,40(a1)
	ld	a_6,48(a1)
	ld	a_7,56(a1)
	mflo	c_1
	mfhi	c_2
	sd	c_1,0(a0)

	dmultu	a_0,a_1		/* mul_add_c2(a[0],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	c_3,t_2,AT
	sd	c_2,8(a0)

	dmultu	a_2,a_0		/* mul_add_c2(a[2],b[0],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_1,a_1		/* mul_add_c(a[1],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,16(a0)

	dmultu	a_0,a_3		/* mul_add_c2(a[0],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	c_3,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_1,a_2		/* mul_add_c2(a[1],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,24(a0)

	dmultu	a_4,a_0		/* mul_add_c2(a[4],b[0],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_3,a_1		/* mul_add_c2(a[3],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_1,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_2,a_2		/* mul_add_c(a[2],b[2],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,32(a0)

	dmultu	a_0,a_5		/* mul_add_c2(a[0],b[5],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_1,a_4		/* mul_add_c2(a[1],b[4],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_2,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_2,a_3		/* mul_add_c2(a[2],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_2,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,40(a0)

	dmultu	a_6,a_0		/* mul_add_c2(a[6],b[0],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	c_3,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_5,a_1		/* mul_add_c2(a[5],b[1],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_4,a_2		/* mul_add_c2(a[4],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_3,a_3		/* mul_add_c(a[3],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,48(a0)

	dmultu	a_0,a_7		/* mul_add_c2(a[0],b[7],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_1,a_6		/* mul_add_c2(a[1],b[6],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_1,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_2,a_5		/* mul_add_c2(a[2],b[5],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_1,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_3,a_4		/* mul_add_c2(a[3],b[4],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_1,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,56(a0)

	dmultu	a_7,a_1		/* mul_add_c2(a[7],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_6,a_2		/* mul_add_c2(a[6],b[2],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_2,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_5,a_3		/* mul_add_c2(a[5],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_2,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_4,a_4		/* mul_add_c(a[4],b[4],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,64(a0)

	dmultu	a_2,a_7		/* mul_add_c2(a[2],b[7],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	c_3,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_3,a_6		/* mul_add_c2(a[3],b[6],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_4,a_5		/* mul_add_c2(a[4],b[5],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,72(a0)

	dmultu	a_7,a_3		/* mul_add_c2(a[7],b[3],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_6,a_4		/* mul_add_c2(a[6],b[4],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_1,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_5,a_5		/* mul_add_c(a[5],b[5],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,80(a0)

	dmultu	a_4,a_7		/* mul_add_c2(a[4],b[7],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_5,a_6		/* mul_add_c2(a[5],b[6],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_2,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,88(a0)

	dmultu	a_7,a_5		/* mul_add_c2(a[7],b[5],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	c_3,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_6,a_6		/* mul_add_c(a[6],b[6],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,96(a0)

	dmultu	a_6,a_7		/* mul_add_c2(a[6],b[7],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,104(a0)

	dmultu	a_7,a_7		/* mul_add_c(a[7],b[7],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sd	c_3,112(a0)
	sd	c_1,120(a0)

	jr	ra
END(bn_sqr_comba8)

.align	5
LEAF(bn_sqr_comba4)
	.set	reorder
	ld	a_0,0(a1)
	ld	a_1,8(a1)
	ld	a_2,16(a1)
	ld	a_3,24(a1)
	dmultu	a_0,a_0		/* mul_add_c(a[0],b[0],c1,c2,c3); */
	mflo	c_1
	mfhi	c_2
	sd	c_1,0(a0)

	dmultu	a_0,a_1		/* mul_add_c2(a[0],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	c_3,t_2,AT
	sd	c_2,8(a0)

	dmultu	a_2,a_0		/* mul_add_c2(a[2],b[0],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	dmultu	a_1,a_1		/* mul_add_c(a[1],b[1],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,16(a0)

	dmultu	a_0,a_3		/* mul_add_c2(a[0],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	c_3,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	dmultu	a_1,a_2		/* mul_add_c(a2[1],b[2],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	slt	AT,t_2,zero
	daddu	c_3,AT
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sltu	AT,c_2,t_2
	daddu	c_3,AT
	sd	c_1,24(a0)

	dmultu	a_3,a_1		/* mul_add_c2(a[3],b[1],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	slt	c_1,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	dmultu	a_2,a_2		/* mul_add_c(a[2],b[2],c2,c3,c1); */
	mflo	t_1
	mfhi	t_2
	daddu	c_2,t_1
	sltu	AT,c_2,t_1
	daddu	t_2,AT
	daddu	c_3,t_2
	sltu	AT,c_3,t_2
	daddu	c_1,AT
	sd	c_2,32(a0)

	dmultu	a_2,a_3		/* mul_add_c2(a[2],b[3],c3,c1,c2); */
	mflo	t_1
	mfhi	t_2
	slt	c_2,t_2,zero
	dsll	t_2,1
	slt	a2,t_1,zero
	daddu	t_2,a2
	dsll	t_1,1
	daddu	c_3,t_1
	sltu	AT,c_3,t_1
	daddu	t_2,AT
	daddu	c_1,t_2
	sltu	AT,c_1,t_2
	daddu	c_2,AT
	sd	c_3,40(a0)

	dmultu	a_3,a_3		/* mul_add_c(a[3],b[3],c1,c2,c3); */
	mflo	t_1
	mfhi	t_2
	daddu	c_1,t_1
	sltu	AT,c_1,t_1
	daddu	t_2,AT
	daddu	c_2,t_2
	sd	c_1,48(a0)
	sd	c_2,56(a0)

	jr	ra
END(bn_sqr_comba4)
