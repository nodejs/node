#include "mips_arch.h"

.text

.set	noat
.set	noreorder
.align	5
.globl	sha1_block_data_order
.ent	sha1_block_data_order
sha1_block_data_order:
	.frame	$29,16*8,$31
	.mask	0xc0ff0000,-8
	.set	noreorder
	dsubu $29,16*8
	sd	$31,(16-1)*8($29)
	sd	$30,(16-2)*8($29)
	sd	$23,(16-3)*8($29)
	sd	$22,(16-4)*8($29)
	sd	$21,(16-5)*8($29)
	sd	$20,(16-6)*8($29)
	sd	$19,(16-7)*8($29)
	sd	$18,(16-8)*8($29)
	sd	$17,(16-9)*8($29)
	sd	$16,(16-10)*8($29)
	dsll $6,6
	daddu $6,$5
	sd	$6,0($29)
	lw	$1,0($4)
	lw	$2,4($4)
	lw	$3,8($4)
	lw	$7,12($4)
	b	.Loop
	lw	$24,16($4)
.align	4
.Loop:
	.set	reorder
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lui	$31,0x5a82
	lw	$8,($5)
	ori	$31,0x7999	# K_00_19
#else
	lwl	$8,3($5)
	lui	$31,0x5a82
	lwr	$8,0($5)
	ori	$31,0x7999	# K_00_19
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$8,$8	# byte swap(0)
	rotr	$8,$8,16
#else
	srl	$25,$8,24	# byte swap(0)
	srl	$6,$8,8
	andi	$30,$8,0xFF00
	sll	$8,$8,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$8,$25
	or	$6,$30
	or	$8,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 0
	xor	$25,$3,$7
	rotr	$6,$1,27
	and	$25,$2
	addu	$24,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$9,1*4($5)
#else
	 lwl	$9,1*4+3($5)
	 lwr	$9,1*4+0($5)
#endif
	xor	$25,$7
	addu	$24,$8
	rotr	$2,$2,2
	addu	$24,$25
#else
	 lwl	$9,1*4+3($5)
	sll	$25,$1,5	# 0
	addu	$24,$31
	 lwr	$9,1*4+0($5)
	srl	$6,$1,27
	addu	$24,$25
	xor	$25,$3,$7
	addu	$24,$6
	sll	$30,$2,30
	and	$25,$2
	srl	$2,$2,2
	xor	$25,$7
	addu	$24,$8
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$9,$9	# byte swap(1)
	rotr	$9,$9,16
#else
	srl	$25,$9,24	# byte swap(1)
	srl	$6,$9,8
	andi	$30,$9,0xFF00
	sll	$9,$9,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$9,$25
	or	$6,$30
	or	$9,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 1
	xor	$25,$2,$3
	rotr	$6,$24,27
	and	$25,$1
	addu	$7,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$10,2*4($5)
#else
	 lwl	$10,2*4+3($5)
	 lwr	$10,2*4+0($5)
#endif
	xor	$25,$3
	addu	$7,$9
	rotr	$1,$1,2
	addu	$7,$25
#else
	 lwl	$10,2*4+3($5)
	sll	$25,$24,5	# 1
	addu	$7,$31
	 lwr	$10,2*4+0($5)
	srl	$6,$24,27
	addu	$7,$25
	xor	$25,$2,$3
	addu	$7,$6
	sll	$30,$1,30
	and	$25,$1
	srl	$1,$1,2
	xor	$25,$3
	addu	$7,$9
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$10,$10	# byte swap(2)
	rotr	$10,$10,16
#else
	srl	$25,$10,24	# byte swap(2)
	srl	$6,$10,8
	andi	$30,$10,0xFF00
	sll	$10,$10,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$10,$25
	or	$6,$30
	or	$10,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 2
	xor	$25,$1,$2
	rotr	$6,$7,27
	and	$25,$24
	addu	$3,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$11,3*4($5)
#else
	 lwl	$11,3*4+3($5)
	 lwr	$11,3*4+0($5)
#endif
	xor	$25,$2
	addu	$3,$10
	rotr	$24,$24,2
	addu	$3,$25
#else
	 lwl	$11,3*4+3($5)
	sll	$25,$7,5	# 2
	addu	$3,$31
	 lwr	$11,3*4+0($5)
	srl	$6,$7,27
	addu	$3,$25
	xor	$25,$1,$2
	addu	$3,$6
	sll	$30,$24,30
	and	$25,$24
	srl	$24,$24,2
	xor	$25,$2
	addu	$3,$10
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$11,$11	# byte swap(3)
	rotr	$11,$11,16
#else
	srl	$25,$11,24	# byte swap(3)
	srl	$6,$11,8
	andi	$30,$11,0xFF00
	sll	$11,$11,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$11,$25
	or	$6,$30
	or	$11,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 3
	xor	$25,$24,$1
	rotr	$6,$3,27
	and	$25,$7
	addu	$2,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$12,4*4($5)
#else
	 lwl	$12,4*4+3($5)
	 lwr	$12,4*4+0($5)
#endif
	xor	$25,$1
	addu	$2,$11
	rotr	$7,$7,2
	addu	$2,$25
#else
	 lwl	$12,4*4+3($5)
	sll	$25,$3,5	# 3
	addu	$2,$31
	 lwr	$12,4*4+0($5)
	srl	$6,$3,27
	addu	$2,$25
	xor	$25,$24,$1
	addu	$2,$6
	sll	$30,$7,30
	and	$25,$7
	srl	$7,$7,2
	xor	$25,$1
	addu	$2,$11
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$12,$12	# byte swap(4)
	rotr	$12,$12,16
#else
	srl	$25,$12,24	# byte swap(4)
	srl	$6,$12,8
	andi	$30,$12,0xFF00
	sll	$12,$12,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$12,$25
	or	$6,$30
	or	$12,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 4
	xor	$25,$7,$24
	rotr	$6,$2,27
	and	$25,$3
	addu	$1,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$13,5*4($5)
#else
	 lwl	$13,5*4+3($5)
	 lwr	$13,5*4+0($5)
#endif
	xor	$25,$24
	addu	$1,$12
	rotr	$3,$3,2
	addu	$1,$25
#else
	 lwl	$13,5*4+3($5)
	sll	$25,$2,5	# 4
	addu	$1,$31
	 lwr	$13,5*4+0($5)
	srl	$6,$2,27
	addu	$1,$25
	xor	$25,$7,$24
	addu	$1,$6
	sll	$30,$3,30
	and	$25,$3
	srl	$3,$3,2
	xor	$25,$24
	addu	$1,$12
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$13,$13	# byte swap(5)
	rotr	$13,$13,16
#else
	srl	$25,$13,24	# byte swap(5)
	srl	$6,$13,8
	andi	$30,$13,0xFF00
	sll	$13,$13,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$13,$25
	or	$6,$30
	or	$13,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 5
	xor	$25,$3,$7
	rotr	$6,$1,27
	and	$25,$2
	addu	$24,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$14,6*4($5)
#else
	 lwl	$14,6*4+3($5)
	 lwr	$14,6*4+0($5)
#endif
	xor	$25,$7
	addu	$24,$13
	rotr	$2,$2,2
	addu	$24,$25
#else
	 lwl	$14,6*4+3($5)
	sll	$25,$1,5	# 5
	addu	$24,$31
	 lwr	$14,6*4+0($5)
	srl	$6,$1,27
	addu	$24,$25
	xor	$25,$3,$7
	addu	$24,$6
	sll	$30,$2,30
	and	$25,$2
	srl	$2,$2,2
	xor	$25,$7
	addu	$24,$13
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$14,$14	# byte swap(6)
	rotr	$14,$14,16
#else
	srl	$25,$14,24	# byte swap(6)
	srl	$6,$14,8
	andi	$30,$14,0xFF00
	sll	$14,$14,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$14,$25
	or	$6,$30
	or	$14,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 6
	xor	$25,$2,$3
	rotr	$6,$24,27
	and	$25,$1
	addu	$7,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$15,7*4($5)
#else
	 lwl	$15,7*4+3($5)
	 lwr	$15,7*4+0($5)
#endif
	xor	$25,$3
	addu	$7,$14
	rotr	$1,$1,2
	addu	$7,$25
#else
	 lwl	$15,7*4+3($5)
	sll	$25,$24,5	# 6
	addu	$7,$31
	 lwr	$15,7*4+0($5)
	srl	$6,$24,27
	addu	$7,$25
	xor	$25,$2,$3
	addu	$7,$6
	sll	$30,$1,30
	and	$25,$1
	srl	$1,$1,2
	xor	$25,$3
	addu	$7,$14
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$15,$15	# byte swap(7)
	rotr	$15,$15,16
#else
	srl	$25,$15,24	# byte swap(7)
	srl	$6,$15,8
	andi	$30,$15,0xFF00
	sll	$15,$15,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$15,$25
	or	$6,$30
	or	$15,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 7
	xor	$25,$1,$2
	rotr	$6,$7,27
	and	$25,$24
	addu	$3,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$16,8*4($5)
#else
	 lwl	$16,8*4+3($5)
	 lwr	$16,8*4+0($5)
#endif
	xor	$25,$2
	addu	$3,$15
	rotr	$24,$24,2
	addu	$3,$25
#else
	 lwl	$16,8*4+3($5)
	sll	$25,$7,5	# 7
	addu	$3,$31
	 lwr	$16,8*4+0($5)
	srl	$6,$7,27
	addu	$3,$25
	xor	$25,$1,$2
	addu	$3,$6
	sll	$30,$24,30
	and	$25,$24
	srl	$24,$24,2
	xor	$25,$2
	addu	$3,$15
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$16,$16	# byte swap(8)
	rotr	$16,$16,16
#else
	srl	$25,$16,24	# byte swap(8)
	srl	$6,$16,8
	andi	$30,$16,0xFF00
	sll	$16,$16,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$16,$25
	or	$6,$30
	or	$16,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 8
	xor	$25,$24,$1
	rotr	$6,$3,27
	and	$25,$7
	addu	$2,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$17,9*4($5)
#else
	 lwl	$17,9*4+3($5)
	 lwr	$17,9*4+0($5)
#endif
	xor	$25,$1
	addu	$2,$16
	rotr	$7,$7,2
	addu	$2,$25
#else
	 lwl	$17,9*4+3($5)
	sll	$25,$3,5	# 8
	addu	$2,$31
	 lwr	$17,9*4+0($5)
	srl	$6,$3,27
	addu	$2,$25
	xor	$25,$24,$1
	addu	$2,$6
	sll	$30,$7,30
	and	$25,$7
	srl	$7,$7,2
	xor	$25,$1
	addu	$2,$16
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$17,$17	# byte swap(9)
	rotr	$17,$17,16
#else
	srl	$25,$17,24	# byte swap(9)
	srl	$6,$17,8
	andi	$30,$17,0xFF00
	sll	$17,$17,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$17,$25
	or	$6,$30
	or	$17,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 9
	xor	$25,$7,$24
	rotr	$6,$2,27
	and	$25,$3
	addu	$1,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$18,10*4($5)
#else
	 lwl	$18,10*4+3($5)
	 lwr	$18,10*4+0($5)
#endif
	xor	$25,$24
	addu	$1,$17
	rotr	$3,$3,2
	addu	$1,$25
#else
	 lwl	$18,10*4+3($5)
	sll	$25,$2,5	# 9
	addu	$1,$31
	 lwr	$18,10*4+0($5)
	srl	$6,$2,27
	addu	$1,$25
	xor	$25,$7,$24
	addu	$1,$6
	sll	$30,$3,30
	and	$25,$3
	srl	$3,$3,2
	xor	$25,$24
	addu	$1,$17
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$18,$18	# byte swap(10)
	rotr	$18,$18,16
#else
	srl	$25,$18,24	# byte swap(10)
	srl	$6,$18,8
	andi	$30,$18,0xFF00
	sll	$18,$18,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$18,$25
	or	$6,$30
	or	$18,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 10
	xor	$25,$3,$7
	rotr	$6,$1,27
	and	$25,$2
	addu	$24,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$19,11*4($5)
#else
	 lwl	$19,11*4+3($5)
	 lwr	$19,11*4+0($5)
#endif
	xor	$25,$7
	addu	$24,$18
	rotr	$2,$2,2
	addu	$24,$25
#else
	 lwl	$19,11*4+3($5)
	sll	$25,$1,5	# 10
	addu	$24,$31
	 lwr	$19,11*4+0($5)
	srl	$6,$1,27
	addu	$24,$25
	xor	$25,$3,$7
	addu	$24,$6
	sll	$30,$2,30
	and	$25,$2
	srl	$2,$2,2
	xor	$25,$7
	addu	$24,$18
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$19,$19	# byte swap(11)
	rotr	$19,$19,16
#else
	srl	$25,$19,24	# byte swap(11)
	srl	$6,$19,8
	andi	$30,$19,0xFF00
	sll	$19,$19,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$19,$25
	or	$6,$30
	or	$19,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 11
	xor	$25,$2,$3
	rotr	$6,$24,27
	and	$25,$1
	addu	$7,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$20,12*4($5)
#else
	 lwl	$20,12*4+3($5)
	 lwr	$20,12*4+0($5)
#endif
	xor	$25,$3
	addu	$7,$19
	rotr	$1,$1,2
	addu	$7,$25
#else
	 lwl	$20,12*4+3($5)
	sll	$25,$24,5	# 11
	addu	$7,$31
	 lwr	$20,12*4+0($5)
	srl	$6,$24,27
	addu	$7,$25
	xor	$25,$2,$3
	addu	$7,$6
	sll	$30,$1,30
	and	$25,$1
	srl	$1,$1,2
	xor	$25,$3
	addu	$7,$19
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$20,$20	# byte swap(12)
	rotr	$20,$20,16
#else
	srl	$25,$20,24	# byte swap(12)
	srl	$6,$20,8
	andi	$30,$20,0xFF00
	sll	$20,$20,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$20,$25
	or	$6,$30
	or	$20,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 12
	xor	$25,$1,$2
	rotr	$6,$7,27
	and	$25,$24
	addu	$3,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$21,13*4($5)
#else
	 lwl	$21,13*4+3($5)
	 lwr	$21,13*4+0($5)
#endif
	xor	$25,$2
	addu	$3,$20
	rotr	$24,$24,2
	addu	$3,$25
#else
	 lwl	$21,13*4+3($5)
	sll	$25,$7,5	# 12
	addu	$3,$31
	 lwr	$21,13*4+0($5)
	srl	$6,$7,27
	addu	$3,$25
	xor	$25,$1,$2
	addu	$3,$6
	sll	$30,$24,30
	and	$25,$24
	srl	$24,$24,2
	xor	$25,$2
	addu	$3,$20
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$21,$21	# byte swap(13)
	rotr	$21,$21,16
#else
	srl	$25,$21,24	# byte swap(13)
	srl	$6,$21,8
	andi	$30,$21,0xFF00
	sll	$21,$21,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$21,$25
	or	$6,$30
	or	$21,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 13
	xor	$25,$24,$1
	rotr	$6,$3,27
	and	$25,$7
	addu	$2,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$22,14*4($5)
#else
	 lwl	$22,14*4+3($5)
	 lwr	$22,14*4+0($5)
#endif
	xor	$25,$1
	addu	$2,$21
	rotr	$7,$7,2
	addu	$2,$25
#else
	 lwl	$22,14*4+3($5)
	sll	$25,$3,5	# 13
	addu	$2,$31
	 lwr	$22,14*4+0($5)
	srl	$6,$3,27
	addu	$2,$25
	xor	$25,$24,$1
	addu	$2,$6
	sll	$30,$7,30
	and	$25,$7
	srl	$7,$7,2
	xor	$25,$1
	addu	$2,$21
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$22,$22	# byte swap(14)
	rotr	$22,$22,16
#else
	srl	$25,$22,24	# byte swap(14)
	srl	$6,$22,8
	andi	$30,$22,0xFF00
	sll	$22,$22,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$22,$25
	or	$6,$30
	or	$22,$6
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 14
	xor	$25,$7,$24
	rotr	$6,$2,27
	and	$25,$3
	addu	$1,$6
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	 lw	$23,15*4($5)
#else
	 lwl	$23,15*4+3($5)
	 lwr	$23,15*4+0($5)
#endif
	xor	$25,$24
	addu	$1,$22
	rotr	$3,$3,2
	addu	$1,$25
#else
	 lwl	$23,15*4+3($5)
	sll	$25,$2,5	# 14
	addu	$1,$31
	 lwr	$23,15*4+0($5)
	srl	$6,$2,27
	addu	$1,$25
	xor	$25,$7,$24
	addu	$1,$6
	sll	$30,$3,30
	and	$25,$3
	srl	$3,$3,2
	xor	$25,$24
	addu	$1,$22
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$23,$23	# byte swap(15)
	rotr	$23,$23,16
#else
	srl	$25,$23,24	# byte swap(15)
	srl	$6,$23,8
	andi	$30,$23,0xFF00
	sll	$23,$23,24
	andi	$6,0xFF00
	sll	$30,$30,8
	or	$23,$25
	or	$23,$6
	or	$23,$30
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 15
	 xor	$8,$10
	xor	$25,$3,$7
	rotr	$6,$1,27
	 xor	$8,$16
	and	$25,$2
	addu	$24,$6
	 xor	$8,$21
	xor	$25,$7
	addu	$24,$23
	 rotr	$8,$8,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$8,$10
	sll	$25,$1,5	# 15
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$8,$16
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$8,$21
	sll	$30,$2,30
	and	$25,$2
	 srl	$6,$8,31
	 addu	$8,$8
	srl	$2,$2,2
	xor	$25,$7
	 or	$8,$6
	addu	$24,$23
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 16
	 xor	$9,$11
	xor	$25,$2,$3
	rotr	$6,$24,27
	 xor	$9,$17
	and	$25,$1
	addu	$7,$6
	 xor	$9,$22
	xor	$25,$3
	addu	$7,$8
	 rotr	$9,$9,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$9,$11
	sll	$25,$24,5	# 16
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$9,$17
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$9,$22
	sll	$30,$1,30
	and	$25,$1
	 srl	$6,$9,31
	 addu	$9,$9
	srl	$1,$1,2
	xor	$25,$3
	 or	$9,$6
	addu	$7,$8
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 17
	 xor	$10,$12
	xor	$25,$1,$2
	rotr	$6,$7,27
	 xor	$10,$18
	and	$25,$24
	addu	$3,$6
	 xor	$10,$23
	xor	$25,$2
	addu	$3,$9
	 rotr	$10,$10,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$10,$12
	sll	$25,$7,5	# 17
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$10,$18
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$10,$23
	sll	$30,$24,30
	and	$25,$24
	 srl	$6,$10,31
	 addu	$10,$10
	srl	$24,$24,2
	xor	$25,$2
	 or	$10,$6
	addu	$3,$9
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 18
	 xor	$11,$13
	xor	$25,$24,$1
	rotr	$6,$3,27
	 xor	$11,$19
	and	$25,$7
	addu	$2,$6
	 xor	$11,$8
	xor	$25,$1
	addu	$2,$10
	 rotr	$11,$11,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$11,$13
	sll	$25,$3,5	# 18
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$11,$19
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$11,$8
	sll	$30,$7,30
	and	$25,$7
	 srl	$6,$11,31
	 addu	$11,$11
	srl	$7,$7,2
	xor	$25,$1
	 or	$11,$6
	addu	$2,$10
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 19
	 xor	$12,$14
	xor	$25,$7,$24
	rotr	$6,$2,27
	 xor	$12,$20
	and	$25,$3
	addu	$1,$6
	 xor	$12,$9
	xor	$25,$24
	addu	$1,$11
	 rotr	$12,$12,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$12,$14
	sll	$25,$2,5	# 19
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$12,$20
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$12,$9
	sll	$30,$3,30
	and	$25,$3
	 srl	$6,$12,31
	 addu	$12,$12
	srl	$3,$3,2
	xor	$25,$24
	 or	$12,$6
	addu	$1,$11
	or	$3,$30
	addu	$1,$25
#endif
	lui	$31,0x6ed9
	ori	$31,0xeba1	# K_20_39
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$13,$15
	addu	$24,$31		# 20
	rotr	$6,$1,27
	 xor	$13,$21
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$13,$10
	xor	$25,$2
	addu	$24,$12
	 rotr	$13,$13,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$13,$15
	sll	$25,$1,5	# 20
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$13,$21
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$13,$10
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$13,31
	 addu	$13,$13
	srl	$2,$2,2
	addu	$24,$12
	 or	$13,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$14,$16
	addu	$7,$31		# 21
	rotr	$6,$24,27
	 xor	$14,$22
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$14,$11
	xor	$25,$1
	addu	$7,$13
	 rotr	$14,$14,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$14,$16
	sll	$25,$24,5	# 21
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$14,$22
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$14,$11
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$14,31
	 addu	$14,$14
	srl	$1,$1,2
	addu	$7,$13
	 or	$14,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$15,$17
	addu	$3,$31		# 22
	rotr	$6,$7,27
	 xor	$15,$23
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$15,$12
	xor	$25,$24
	addu	$3,$14
	 rotr	$15,$15,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$15,$17
	sll	$25,$7,5	# 22
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$15,$23
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$15,$12
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$15,31
	 addu	$15,$15
	srl	$24,$24,2
	addu	$3,$14
	 or	$15,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$16,$18
	addu	$2,$31		# 23
	rotr	$6,$3,27
	 xor	$16,$8
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$16,$13
	xor	$25,$7
	addu	$2,$15
	 rotr	$16,$16,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$16,$18
	sll	$25,$3,5	# 23
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$16,$8
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$16,$13
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$16,31
	 addu	$16,$16
	srl	$7,$7,2
	addu	$2,$15
	 or	$16,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$17,$19
	addu	$1,$31		# 24
	rotr	$6,$2,27
	 xor	$17,$9
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$17,$14
	xor	$25,$3
	addu	$1,$16
	 rotr	$17,$17,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$17,$19
	sll	$25,$2,5	# 24
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$17,$9
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$17,$14
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$17,31
	 addu	$17,$17
	srl	$3,$3,2
	addu	$1,$16
	 or	$17,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$18,$20
	addu	$24,$31		# 25
	rotr	$6,$1,27
	 xor	$18,$10
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$18,$15
	xor	$25,$2
	addu	$24,$17
	 rotr	$18,$18,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$18,$20
	sll	$25,$1,5	# 25
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$18,$10
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$18,$15
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$18,31
	 addu	$18,$18
	srl	$2,$2,2
	addu	$24,$17
	 or	$18,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$19,$21
	addu	$7,$31		# 26
	rotr	$6,$24,27
	 xor	$19,$11
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$19,$16
	xor	$25,$1
	addu	$7,$18
	 rotr	$19,$19,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$19,$21
	sll	$25,$24,5	# 26
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$19,$11
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$19,$16
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$19,31
	 addu	$19,$19
	srl	$1,$1,2
	addu	$7,$18
	 or	$19,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$20,$22
	addu	$3,$31		# 27
	rotr	$6,$7,27
	 xor	$20,$12
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$20,$17
	xor	$25,$24
	addu	$3,$19
	 rotr	$20,$20,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$20,$22
	sll	$25,$7,5	# 27
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$20,$12
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$20,$17
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$20,31
	 addu	$20,$20
	srl	$24,$24,2
	addu	$3,$19
	 or	$20,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$21,$23
	addu	$2,$31		# 28
	rotr	$6,$3,27
	 xor	$21,$13
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$21,$18
	xor	$25,$7
	addu	$2,$20
	 rotr	$21,$21,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$21,$23
	sll	$25,$3,5	# 28
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$21,$13
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$21,$18
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$21,31
	 addu	$21,$21
	srl	$7,$7,2
	addu	$2,$20
	 or	$21,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$22,$8
	addu	$1,$31		# 29
	rotr	$6,$2,27
	 xor	$22,$14
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$22,$19
	xor	$25,$3
	addu	$1,$21
	 rotr	$22,$22,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$22,$8
	sll	$25,$2,5	# 29
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$22,$14
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$22,$19
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$22,31
	 addu	$22,$22
	srl	$3,$3,2
	addu	$1,$21
	 or	$22,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$23,$9
	addu	$24,$31		# 30
	rotr	$6,$1,27
	 xor	$23,$15
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$23,$20
	xor	$25,$2
	addu	$24,$22
	 rotr	$23,$23,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$23,$9
	sll	$25,$1,5	# 30
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$23,$15
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$23,$20
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$23,31
	 addu	$23,$23
	srl	$2,$2,2
	addu	$24,$22
	 or	$23,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$8,$10
	addu	$7,$31		# 31
	rotr	$6,$24,27
	 xor	$8,$16
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$8,$21
	xor	$25,$1
	addu	$7,$23
	 rotr	$8,$8,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$8,$10
	sll	$25,$24,5	# 31
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$8,$16
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$8,$21
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$8,31
	 addu	$8,$8
	srl	$1,$1,2
	addu	$7,$23
	 or	$8,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$9,$11
	addu	$3,$31		# 32
	rotr	$6,$7,27
	 xor	$9,$17
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$9,$22
	xor	$25,$24
	addu	$3,$8
	 rotr	$9,$9,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$9,$11
	sll	$25,$7,5	# 32
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$9,$17
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$9,$22
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$9,31
	 addu	$9,$9
	srl	$24,$24,2
	addu	$3,$8
	 or	$9,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$10,$12
	addu	$2,$31		# 33
	rotr	$6,$3,27
	 xor	$10,$18
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$10,$23
	xor	$25,$7
	addu	$2,$9
	 rotr	$10,$10,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$10,$12
	sll	$25,$3,5	# 33
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$10,$18
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$10,$23
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$10,31
	 addu	$10,$10
	srl	$7,$7,2
	addu	$2,$9
	 or	$10,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$11,$13
	addu	$1,$31		# 34
	rotr	$6,$2,27
	 xor	$11,$19
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$11,$8
	xor	$25,$3
	addu	$1,$10
	 rotr	$11,$11,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$11,$13
	sll	$25,$2,5	# 34
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$11,$19
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$11,$8
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$11,31
	 addu	$11,$11
	srl	$3,$3,2
	addu	$1,$10
	 or	$11,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$12,$14
	addu	$24,$31		# 35
	rotr	$6,$1,27
	 xor	$12,$20
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$12,$9
	xor	$25,$2
	addu	$24,$11
	 rotr	$12,$12,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$12,$14
	sll	$25,$1,5	# 35
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$12,$20
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$12,$9
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$12,31
	 addu	$12,$12
	srl	$2,$2,2
	addu	$24,$11
	 or	$12,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$13,$15
	addu	$7,$31		# 36
	rotr	$6,$24,27
	 xor	$13,$21
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$13,$10
	xor	$25,$1
	addu	$7,$12
	 rotr	$13,$13,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$13,$15
	sll	$25,$24,5	# 36
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$13,$21
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$13,$10
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$13,31
	 addu	$13,$13
	srl	$1,$1,2
	addu	$7,$12
	 or	$13,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$14,$16
	addu	$3,$31		# 37
	rotr	$6,$7,27
	 xor	$14,$22
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$14,$11
	xor	$25,$24
	addu	$3,$13
	 rotr	$14,$14,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$14,$16
	sll	$25,$7,5	# 37
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$14,$22
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$14,$11
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$14,31
	 addu	$14,$14
	srl	$24,$24,2
	addu	$3,$13
	 or	$14,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$15,$17
	addu	$2,$31		# 38
	rotr	$6,$3,27
	 xor	$15,$23
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$15,$12
	xor	$25,$7
	addu	$2,$14
	 rotr	$15,$15,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$15,$17
	sll	$25,$3,5	# 38
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$15,$23
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$15,$12
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$15,31
	 addu	$15,$15
	srl	$7,$7,2
	addu	$2,$14
	 or	$15,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$16,$18
	addu	$1,$31		# 39
	rotr	$6,$2,27
	 xor	$16,$8
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$16,$13
	xor	$25,$3
	addu	$1,$15
	 rotr	$16,$16,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$16,$18
	sll	$25,$2,5	# 39
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$16,$8
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$16,$13
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$16,31
	 addu	$16,$16
	srl	$3,$3,2
	addu	$1,$15
	 or	$16,$6
	or	$3,$30
	addu	$1,$25
#endif
	lui	$31,0x8f1b
	ori	$31,0xbcdc	# K_40_59
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 40
	and	$25,$3,$7
	 xor	$17,$19
	rotr	$6,$1,27
	addu	$24,$25
	 xor	$17,$9
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$17,$14
	and	$25,$2
	addu	$24,$16
	 rotr	$17,$17,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$17,$19
	sll	$25,$1,5	# 40
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$17,$9
	and	$25,$3,$7
	addu	$24,$6
	 xor	$17,$14
	sll	$30,$2,30
	addu	$24,$25
	 srl	$6,$17,31
	xor	$25,$3,$7
	 addu	$17,$17
	and	$25,$2
	srl	$2,$2,2
	 or	$17,$6
	addu	$24,$16
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 41
	and	$25,$2,$3
	 xor	$18,$20
	rotr	$6,$24,27
	addu	$7,$25
	 xor	$18,$10
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$18,$15
	and	$25,$1
	addu	$7,$17
	 rotr	$18,$18,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$18,$20
	sll	$25,$24,5	# 41
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$18,$10
	and	$25,$2,$3
	addu	$7,$6
	 xor	$18,$15
	sll	$30,$1,30
	addu	$7,$25
	 srl	$6,$18,31
	xor	$25,$2,$3
	 addu	$18,$18
	and	$25,$1
	srl	$1,$1,2
	 or	$18,$6
	addu	$7,$17
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 42
	and	$25,$1,$2
	 xor	$19,$21
	rotr	$6,$7,27
	addu	$3,$25
	 xor	$19,$11
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$19,$16
	and	$25,$24
	addu	$3,$18
	 rotr	$19,$19,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$19,$21
	sll	$25,$7,5	# 42
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$19,$11
	and	$25,$1,$2
	addu	$3,$6
	 xor	$19,$16
	sll	$30,$24,30
	addu	$3,$25
	 srl	$6,$19,31
	xor	$25,$1,$2
	 addu	$19,$19
	and	$25,$24
	srl	$24,$24,2
	 or	$19,$6
	addu	$3,$18
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 43
	and	$25,$24,$1
	 xor	$20,$22
	rotr	$6,$3,27
	addu	$2,$25
	 xor	$20,$12
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$20,$17
	and	$25,$7
	addu	$2,$19
	 rotr	$20,$20,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$20,$22
	sll	$25,$3,5	# 43
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$20,$12
	and	$25,$24,$1
	addu	$2,$6
	 xor	$20,$17
	sll	$30,$7,30
	addu	$2,$25
	 srl	$6,$20,31
	xor	$25,$24,$1
	 addu	$20,$20
	and	$25,$7
	srl	$7,$7,2
	 or	$20,$6
	addu	$2,$19
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 44
	and	$25,$7,$24
	 xor	$21,$23
	rotr	$6,$2,27
	addu	$1,$25
	 xor	$21,$13
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$21,$18
	and	$25,$3
	addu	$1,$20
	 rotr	$21,$21,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$21,$23
	sll	$25,$2,5	# 44
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$21,$13
	and	$25,$7,$24
	addu	$1,$6
	 xor	$21,$18
	sll	$30,$3,30
	addu	$1,$25
	 srl	$6,$21,31
	xor	$25,$7,$24
	 addu	$21,$21
	and	$25,$3
	srl	$3,$3,2
	 or	$21,$6
	addu	$1,$20
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 45
	and	$25,$3,$7
	 xor	$22,$8
	rotr	$6,$1,27
	addu	$24,$25
	 xor	$22,$14
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$22,$19
	and	$25,$2
	addu	$24,$21
	 rotr	$22,$22,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$22,$8
	sll	$25,$1,5	# 45
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$22,$14
	and	$25,$3,$7
	addu	$24,$6
	 xor	$22,$19
	sll	$30,$2,30
	addu	$24,$25
	 srl	$6,$22,31
	xor	$25,$3,$7
	 addu	$22,$22
	and	$25,$2
	srl	$2,$2,2
	 or	$22,$6
	addu	$24,$21
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 46
	and	$25,$2,$3
	 xor	$23,$9
	rotr	$6,$24,27
	addu	$7,$25
	 xor	$23,$15
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$23,$20
	and	$25,$1
	addu	$7,$22
	 rotr	$23,$23,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$23,$9
	sll	$25,$24,5	# 46
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$23,$15
	and	$25,$2,$3
	addu	$7,$6
	 xor	$23,$20
	sll	$30,$1,30
	addu	$7,$25
	 srl	$6,$23,31
	xor	$25,$2,$3
	 addu	$23,$23
	and	$25,$1
	srl	$1,$1,2
	 or	$23,$6
	addu	$7,$22
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 47
	and	$25,$1,$2
	 xor	$8,$10
	rotr	$6,$7,27
	addu	$3,$25
	 xor	$8,$16
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$8,$21
	and	$25,$24
	addu	$3,$23
	 rotr	$8,$8,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$8,$10
	sll	$25,$7,5	# 47
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$8,$16
	and	$25,$1,$2
	addu	$3,$6
	 xor	$8,$21
	sll	$30,$24,30
	addu	$3,$25
	 srl	$6,$8,31
	xor	$25,$1,$2
	 addu	$8,$8
	and	$25,$24
	srl	$24,$24,2
	 or	$8,$6
	addu	$3,$23
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 48
	and	$25,$24,$1
	 xor	$9,$11
	rotr	$6,$3,27
	addu	$2,$25
	 xor	$9,$17
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$9,$22
	and	$25,$7
	addu	$2,$8
	 rotr	$9,$9,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$9,$11
	sll	$25,$3,5	# 48
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$9,$17
	and	$25,$24,$1
	addu	$2,$6
	 xor	$9,$22
	sll	$30,$7,30
	addu	$2,$25
	 srl	$6,$9,31
	xor	$25,$24,$1
	 addu	$9,$9
	and	$25,$7
	srl	$7,$7,2
	 or	$9,$6
	addu	$2,$8
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 49
	and	$25,$7,$24
	 xor	$10,$12
	rotr	$6,$2,27
	addu	$1,$25
	 xor	$10,$18
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$10,$23
	and	$25,$3
	addu	$1,$9
	 rotr	$10,$10,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$10,$12
	sll	$25,$2,5	# 49
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$10,$18
	and	$25,$7,$24
	addu	$1,$6
	 xor	$10,$23
	sll	$30,$3,30
	addu	$1,$25
	 srl	$6,$10,31
	xor	$25,$7,$24
	 addu	$10,$10
	and	$25,$3
	srl	$3,$3,2
	 or	$10,$6
	addu	$1,$9
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 50
	and	$25,$3,$7
	 xor	$11,$13
	rotr	$6,$1,27
	addu	$24,$25
	 xor	$11,$19
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$11,$8
	and	$25,$2
	addu	$24,$10
	 rotr	$11,$11,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$11,$13
	sll	$25,$1,5	# 50
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$11,$19
	and	$25,$3,$7
	addu	$24,$6
	 xor	$11,$8
	sll	$30,$2,30
	addu	$24,$25
	 srl	$6,$11,31
	xor	$25,$3,$7
	 addu	$11,$11
	and	$25,$2
	srl	$2,$2,2
	 or	$11,$6
	addu	$24,$10
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 51
	and	$25,$2,$3
	 xor	$12,$14
	rotr	$6,$24,27
	addu	$7,$25
	 xor	$12,$20
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$12,$9
	and	$25,$1
	addu	$7,$11
	 rotr	$12,$12,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$12,$14
	sll	$25,$24,5	# 51
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$12,$20
	and	$25,$2,$3
	addu	$7,$6
	 xor	$12,$9
	sll	$30,$1,30
	addu	$7,$25
	 srl	$6,$12,31
	xor	$25,$2,$3
	 addu	$12,$12
	and	$25,$1
	srl	$1,$1,2
	 or	$12,$6
	addu	$7,$11
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 52
	and	$25,$1,$2
	 xor	$13,$15
	rotr	$6,$7,27
	addu	$3,$25
	 xor	$13,$21
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$13,$10
	and	$25,$24
	addu	$3,$12
	 rotr	$13,$13,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$13,$15
	sll	$25,$7,5	# 52
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$13,$21
	and	$25,$1,$2
	addu	$3,$6
	 xor	$13,$10
	sll	$30,$24,30
	addu	$3,$25
	 srl	$6,$13,31
	xor	$25,$1,$2
	 addu	$13,$13
	and	$25,$24
	srl	$24,$24,2
	 or	$13,$6
	addu	$3,$12
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 53
	and	$25,$24,$1
	 xor	$14,$16
	rotr	$6,$3,27
	addu	$2,$25
	 xor	$14,$22
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$14,$11
	and	$25,$7
	addu	$2,$13
	 rotr	$14,$14,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$14,$16
	sll	$25,$3,5	# 53
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$14,$22
	and	$25,$24,$1
	addu	$2,$6
	 xor	$14,$11
	sll	$30,$7,30
	addu	$2,$25
	 srl	$6,$14,31
	xor	$25,$24,$1
	 addu	$14,$14
	and	$25,$7
	srl	$7,$7,2
	 or	$14,$6
	addu	$2,$13
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 54
	and	$25,$7,$24
	 xor	$15,$17
	rotr	$6,$2,27
	addu	$1,$25
	 xor	$15,$23
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$15,$12
	and	$25,$3
	addu	$1,$14
	 rotr	$15,$15,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$15,$17
	sll	$25,$2,5	# 54
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$15,$23
	and	$25,$7,$24
	addu	$1,$6
	 xor	$15,$12
	sll	$30,$3,30
	addu	$1,$25
	 srl	$6,$15,31
	xor	$25,$7,$24
	 addu	$15,$15
	and	$25,$3
	srl	$3,$3,2
	 or	$15,$6
	addu	$1,$14
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$24,$31		# 55
	and	$25,$3,$7
	 xor	$16,$18
	rotr	$6,$1,27
	addu	$24,$25
	 xor	$16,$8
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$16,$13
	and	$25,$2
	addu	$24,$15
	 rotr	$16,$16,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$16,$18
	sll	$25,$1,5	# 55
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$16,$8
	and	$25,$3,$7
	addu	$24,$6
	 xor	$16,$13
	sll	$30,$2,30
	addu	$24,$25
	 srl	$6,$16,31
	xor	$25,$3,$7
	 addu	$16,$16
	and	$25,$2
	srl	$2,$2,2
	 or	$16,$6
	addu	$24,$15
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$7,$31		# 56
	and	$25,$2,$3
	 xor	$17,$19
	rotr	$6,$24,27
	addu	$7,$25
	 xor	$17,$9
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$17,$14
	and	$25,$1
	addu	$7,$16
	 rotr	$17,$17,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$17,$19
	sll	$25,$24,5	# 56
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$17,$9
	and	$25,$2,$3
	addu	$7,$6
	 xor	$17,$14
	sll	$30,$1,30
	addu	$7,$25
	 srl	$6,$17,31
	xor	$25,$2,$3
	 addu	$17,$17
	and	$25,$1
	srl	$1,$1,2
	 or	$17,$6
	addu	$7,$16
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$3,$31		# 57
	and	$25,$1,$2
	 xor	$18,$20
	rotr	$6,$7,27
	addu	$3,$25
	 xor	$18,$10
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$18,$15
	and	$25,$24
	addu	$3,$17
	 rotr	$18,$18,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$18,$20
	sll	$25,$7,5	# 57
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$18,$10
	and	$25,$1,$2
	addu	$3,$6
	 xor	$18,$15
	sll	$30,$24,30
	addu	$3,$25
	 srl	$6,$18,31
	xor	$25,$1,$2
	 addu	$18,$18
	and	$25,$24
	srl	$24,$24,2
	 or	$18,$6
	addu	$3,$17
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$2,$31		# 58
	and	$25,$24,$1
	 xor	$19,$21
	rotr	$6,$3,27
	addu	$2,$25
	 xor	$19,$11
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$19,$16
	and	$25,$7
	addu	$2,$18
	 rotr	$19,$19,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$19,$21
	sll	$25,$3,5	# 58
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$19,$11
	and	$25,$24,$1
	addu	$2,$6
	 xor	$19,$16
	sll	$30,$7,30
	addu	$2,$25
	 srl	$6,$19,31
	xor	$25,$24,$1
	 addu	$19,$19
	and	$25,$7
	srl	$7,$7,2
	 or	$19,$6
	addu	$2,$18
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	addu	$1,$31		# 59
	and	$25,$7,$24
	 xor	$20,$22
	rotr	$6,$2,27
	addu	$1,$25
	 xor	$20,$12
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$20,$17
	and	$25,$3
	addu	$1,$19
	 rotr	$20,$20,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$20,$22
	sll	$25,$2,5	# 59
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$20,$12
	and	$25,$7,$24
	addu	$1,$6
	 xor	$20,$17
	sll	$30,$3,30
	addu	$1,$25
	 srl	$6,$20,31
	xor	$25,$7,$24
	 addu	$20,$20
	and	$25,$3
	srl	$3,$3,2
	 or	$20,$6
	addu	$1,$19
	or	$3,$30
	addu	$1,$25
#endif
	lui	$31,0xca62
	ori	$31,0xc1d6	# K_60_79
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$21,$23
	addu	$24,$31		# 60
	rotr	$6,$1,27
	 xor	$21,$13
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$21,$18
	xor	$25,$2
	addu	$24,$20
	 rotr	$21,$21,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$21,$23
	sll	$25,$1,5	# 60
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$21,$13
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$21,$18
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$21,31
	 addu	$21,$21
	srl	$2,$2,2
	addu	$24,$20
	 or	$21,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$22,$8
	addu	$7,$31		# 61
	rotr	$6,$24,27
	 xor	$22,$14
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$22,$19
	xor	$25,$1
	addu	$7,$21
	 rotr	$22,$22,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$22,$8
	sll	$25,$24,5	# 61
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$22,$14
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$22,$19
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$22,31
	 addu	$22,$22
	srl	$1,$1,2
	addu	$7,$21
	 or	$22,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$23,$9
	addu	$3,$31		# 62
	rotr	$6,$7,27
	 xor	$23,$15
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$23,$20
	xor	$25,$24
	addu	$3,$22
	 rotr	$23,$23,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$23,$9
	sll	$25,$7,5	# 62
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$23,$15
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$23,$20
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$23,31
	 addu	$23,$23
	srl	$24,$24,2
	addu	$3,$22
	 or	$23,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$8,$10
	addu	$2,$31		# 63
	rotr	$6,$3,27
	 xor	$8,$16
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$8,$21
	xor	$25,$7
	addu	$2,$23
	 rotr	$8,$8,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$8,$10
	sll	$25,$3,5	# 63
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$8,$16
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$8,$21
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$8,31
	 addu	$8,$8
	srl	$7,$7,2
	addu	$2,$23
	 or	$8,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$9,$11
	addu	$1,$31		# 64
	rotr	$6,$2,27
	 xor	$9,$17
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$9,$22
	xor	$25,$3
	addu	$1,$8
	 rotr	$9,$9,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$9,$11
	sll	$25,$2,5	# 64
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$9,$17
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$9,$22
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$9,31
	 addu	$9,$9
	srl	$3,$3,2
	addu	$1,$8
	 or	$9,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$10,$12
	addu	$24,$31		# 65
	rotr	$6,$1,27
	 xor	$10,$18
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$10,$23
	xor	$25,$2
	addu	$24,$9
	 rotr	$10,$10,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$10,$12
	sll	$25,$1,5	# 65
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$10,$18
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$10,$23
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$10,31
	 addu	$10,$10
	srl	$2,$2,2
	addu	$24,$9
	 or	$10,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$11,$13
	addu	$7,$31		# 66
	rotr	$6,$24,27
	 xor	$11,$19
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$11,$8
	xor	$25,$1
	addu	$7,$10
	 rotr	$11,$11,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$11,$13
	sll	$25,$24,5	# 66
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$11,$19
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$11,$8
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$11,31
	 addu	$11,$11
	srl	$1,$1,2
	addu	$7,$10
	 or	$11,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$12,$14
	addu	$3,$31		# 67
	rotr	$6,$7,27
	 xor	$12,$20
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$12,$9
	xor	$25,$24
	addu	$3,$11
	 rotr	$12,$12,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$12,$14
	sll	$25,$7,5	# 67
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$12,$20
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$12,$9
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$12,31
	 addu	$12,$12
	srl	$24,$24,2
	addu	$3,$11
	 or	$12,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$13,$15
	addu	$2,$31		# 68
	rotr	$6,$3,27
	 xor	$13,$21
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$13,$10
	xor	$25,$7
	addu	$2,$12
	 rotr	$13,$13,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$13,$15
	sll	$25,$3,5	# 68
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$13,$21
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$13,$10
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$13,31
	 addu	$13,$13
	srl	$7,$7,2
	addu	$2,$12
	 or	$13,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$14,$16
	addu	$1,$31		# 69
	rotr	$6,$2,27
	 xor	$14,$22
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$14,$11
	xor	$25,$3
	addu	$1,$13
	 rotr	$14,$14,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$14,$16
	sll	$25,$2,5	# 69
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$14,$22
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$14,$11
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$14,31
	 addu	$14,$14
	srl	$3,$3,2
	addu	$1,$13
	 or	$14,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$15,$17
	addu	$24,$31		# 70
	rotr	$6,$1,27
	 xor	$15,$23
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$15,$12
	xor	$25,$2
	addu	$24,$14
	 rotr	$15,$15,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$15,$17
	sll	$25,$1,5	# 70
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$15,$23
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$15,$12
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$15,31
	 addu	$15,$15
	srl	$2,$2,2
	addu	$24,$14
	 or	$15,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$16,$18
	addu	$7,$31		# 71
	rotr	$6,$24,27
	 xor	$16,$8
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$16,$13
	xor	$25,$1
	addu	$7,$15
	 rotr	$16,$16,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$16,$18
	sll	$25,$24,5	# 71
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$16,$8
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$16,$13
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$16,31
	 addu	$16,$16
	srl	$1,$1,2
	addu	$7,$15
	 or	$16,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$17,$19
	addu	$3,$31		# 72
	rotr	$6,$7,27
	 xor	$17,$9
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$17,$14
	xor	$25,$24
	addu	$3,$16
	 rotr	$17,$17,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$17,$19
	sll	$25,$7,5	# 72
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$17,$9
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$17,$14
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$17,31
	 addu	$17,$17
	srl	$24,$24,2
	addu	$3,$16
	 or	$17,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$18,$20
	addu	$2,$31		# 73
	rotr	$6,$3,27
	 xor	$18,$10
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$18,$15
	xor	$25,$7
	addu	$2,$17
	 rotr	$18,$18,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$18,$20
	sll	$25,$3,5	# 73
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$18,$10
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$18,$15
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$18,31
	 addu	$18,$18
	srl	$7,$7,2
	addu	$2,$17
	 or	$18,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$19,$21
	addu	$1,$31		# 74
	rotr	$6,$2,27
	 xor	$19,$11
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$19,$16
	xor	$25,$3
	addu	$1,$18
	 rotr	$19,$19,31
	rotr	$3,$3,2
	addu	$1,$25
#else
	 xor	$19,$21
	sll	$25,$2,5	# 74
	addu	$1,$31
	srl	$6,$2,27
	addu	$1,$25
	 xor	$19,$11
	xor	$25,$7,$24
	addu	$1,$6
	 xor	$19,$16
	sll	$30,$3,30
	xor	$25,$3
	 srl	$6,$19,31
	 addu	$19,$19
	srl	$3,$3,2
	addu	$1,$18
	 or	$19,$6
	or	$3,$30
	addu	$1,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$20,$22
	addu	$24,$31		# 75
	rotr	$6,$1,27
	 xor	$20,$12
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$20,$17
	xor	$25,$2
	addu	$24,$19
	 rotr	$20,$20,31
	rotr	$2,$2,2
	addu	$24,$25
#else
	 xor	$20,$22
	sll	$25,$1,5	# 75
	addu	$24,$31
	srl	$6,$1,27
	addu	$24,$25
	 xor	$20,$12
	xor	$25,$3,$7
	addu	$24,$6
	 xor	$20,$17
	sll	$30,$2,30
	xor	$25,$2
	 srl	$6,$20,31
	 addu	$20,$20
	srl	$2,$2,2
	addu	$24,$19
	 or	$20,$6
	or	$2,$30
	addu	$24,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$21,$23
	addu	$7,$31		# 76
	rotr	$6,$24,27
	 xor	$21,$13
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$21,$18
	xor	$25,$1
	addu	$7,$20
	 rotr	$21,$21,31
	rotr	$1,$1,2
	addu	$7,$25
#else
	 xor	$21,$23
	sll	$25,$24,5	# 76
	addu	$7,$31
	srl	$6,$24,27
	addu	$7,$25
	 xor	$21,$13
	xor	$25,$2,$3
	addu	$7,$6
	 xor	$21,$18
	sll	$30,$1,30
	xor	$25,$1
	 srl	$6,$21,31
	 addu	$21,$21
	srl	$1,$1,2
	addu	$7,$20
	 or	$21,$6
	or	$1,$30
	addu	$7,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$22,$8
	addu	$3,$31		# 77
	rotr	$6,$7,27
	 xor	$22,$14
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$22,$19
	xor	$25,$24
	addu	$3,$21
	 rotr	$22,$22,31
	rotr	$24,$24,2
	addu	$3,$25
#else
	 xor	$22,$8
	sll	$25,$7,5	# 77
	addu	$3,$31
	srl	$6,$7,27
	addu	$3,$25
	 xor	$22,$14
	xor	$25,$1,$2
	addu	$3,$6
	 xor	$22,$19
	sll	$30,$24,30
	xor	$25,$24
	 srl	$6,$22,31
	 addu	$22,$22
	srl	$24,$24,2
	addu	$3,$21
	 or	$22,$6
	or	$24,$30
	addu	$3,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 xor	$23,$9
	addu	$2,$31		# 78
	rotr	$6,$3,27
	 xor	$23,$15
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$23,$20
	xor	$25,$7
	addu	$2,$22
	 rotr	$23,$23,31
	rotr	$7,$7,2
	addu	$2,$25
#else
	 xor	$23,$9
	sll	$25,$3,5	# 78
	addu	$2,$31
	srl	$6,$3,27
	addu	$2,$25
	 xor	$23,$15
	xor	$25,$24,$1
	addu	$2,$6
	 xor	$23,$20
	sll	$30,$7,30
	xor	$25,$7
	 srl	$6,$23,31
	 addu	$23,$23
	srl	$7,$7,2
	addu	$2,$22
	 or	$23,$6
	or	$7,$30
	addu	$2,$25
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	 lw	$8,0($4)
	addu	$1,$31		# 79
	 lw	$9,4($4)
	rotr	$6,$2,27
	 lw	$10,8($4)
	xor	$25,$7,$24
	addu	$1,$6
	 lw	$11,12($4)
	xor	$25,$3
	addu	$1,$23
	 lw	$12,16($4)
	rotr	$3,$3,2
	addu	$1,$25
#else
	 lw	$8,0($4)
	sll	$25,$2,5	# 79
	addu	$1,$31
	 lw	$9,4($4)
	srl	$6,$2,27
	addu	$1,$25
	 lw	$10,8($4)
	xor	$25,$7,$24
	addu	$1,$6
	 lw	$11,12($4)
	sll	$30,$3,30
	xor	$25,$3
	 lw	$12,16($4)
	srl	$3,$3,2
	addu	$1,$23
	or	$3,$30
	addu	$1,$25
#endif
	daddu $5,64
	ld	$6,0($29)

	addu	$1,$8
	addu	$2,$9
	sw	$1,0($4)
	addu	$3,$10
	addu	$7,$11
	sw	$2,4($4)
	addu	$24,$12
	sw	$3,8($4)
	sw	$7,12($4)
	sw	$24,16($4)
	.set	noreorder
	bne	$5,$6,.Loop
	nop

	.set	noreorder
	ld	$31,(16-1)*8($29)
	ld	$30,(16-2)*8($29)
	ld	$23,(16-3)*8($29)
	ld	$22,(16-4)*8($29)
	ld	$21,(16-5)*8($29)
	ld	$20,(16-6)*8($29)
	ld	$19,(16-7)*8($29)
	ld	$18,(16-8)*8($29)
	ld	$17,(16-9)*8($29)
	ld	$16,(16-10)*8($29)
	jr	$31
	daddu $29,16*8
.end	sha1_block_data_order
.rdata
.asciiz	"SHA1 for MIPS, CRYPTOGAMS by <appro@openssl.org>"
