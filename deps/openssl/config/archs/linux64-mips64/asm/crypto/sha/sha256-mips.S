#include "mips_arch.h"

.text
.set	noat
#if !defined(__mips_eabi) && (!defined(__vxworks) || defined(__pic__))
.option	pic2
#endif

.align	5
.globl	sha256_block_data_order
.ent	sha256_block_data_order
sha256_block_data_order:
	.frame	$29,192,$31
	.mask	0xc0ff0000,-8
	.set	noreorder
	dsubu $29,192
	sd	$31,192-1*8($29)
	sd	$30,192-2*8($29)
	sd	$23,192-3*8($29)
	sd	$22,192-4*8($29)
	sd	$21,192-5*8($29)
	sd	$20,192-6*8($29)
	sd	$19,192-7*8($29)
	sd	$18,192-8*8($29)
	sd	$17,192-9*8($29)
	sd	$16,192-10*8($29)
	dsll $23,$6,6
	.cplocal	$6
	.cpsetup	$25,$0,sha256_block_data_order
	.set	reorder
	dla	$6,K256		# PIC-ified 'load address'

	lw	$1,0*4($4)		# load context
	lw	$2,1*4($4)
	lw	$3,2*4($4)
	lw	$7,3*4($4)
	lw	$24,4*4($4)
	lw	$25,5*4($4)
	lw	$30,6*4($4)
	lw	$31,7*4($4)

	daddu $23,$5		# pointer to the end of input
	sd	$23,16*4($29)
	b	.Loop

.align	5
.Loop:
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$8,($5)
#else
	lwl	$8,3($5)
	lwr	$8,0($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$9,4($5)
#else
	lwl	$9,7($5)
	lwr	$9,4($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$8,$8		# byte swap(0)
	rotr	$8,$8,16
#else
	srl	$13,$8,24		# byte swap(0)
	srl	$14,$8,8
	andi	$15,$8,0xFF00
	sll	$8,$8,24
	andi	$14,0xFF00
	sll	$15,$15,8
	or	$8,$13
	or	$14,$15
	or	$8,$14
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$15,$25,$30			# 0
	rotr	$13,$24,6
	addu	$12,$8,$31
	rotr	$14,$24,11
	and	$15,$24
	rotr	$31,$24,25
	xor	$13,$14
	rotr	$14,$1,2
	xor	$15,$30			# Ch(e,f,g)
	xor	$13,$31			# Sigma1(e)

	rotr	$31,$1,13
	addu	$12,$15
	lw	$15,0($6)		# K[0]
	xor	$31,$14
	rotr	$14,$1,22
	addu	$12,$13
	and	$13,$2,$3
	xor	$31,$14			# Sigma0(a)
	xor	$14,$2,$3
#else
	addu	$12,$8,$31			# 0
	srl	$31,$24,6
	xor	$15,$25,$30
	sll	$14,$24,7
	and	$15,$24
	srl	$13,$24,11
	xor	$31,$14
	sll	$14,$24,21
	xor	$31,$13
	srl	$13,$24,25
	xor	$31,$14
	sll	$14,$24,26
	xor	$31,$13
	xor	$15,$30			# Ch(e,f,g)
	xor	$13,$14,$31			# Sigma1(e)

	srl	$31,$1,2
	addu	$12,$15
	lw	$15,0($6)		# K[0]
	sll	$14,$1,10
	addu	$12,$13
	srl	$13,$1,13
	xor	$31,$14
	sll	$14,$1,19
	xor	$31,$13
	srl	$13,$1,22
	xor	$31,$14
	sll	$14,$1,30
	xor	$31,$13
	and	$13,$2,$3
	xor	$31,$14			# Sigma0(a)
	xor	$14,$2,$3
#endif
	sw	$8,0($29)	# offload to ring buffer
	addu	$31,$13
	and	$14,$1
	addu	$12,$15			# +=K[0]
	addu	$31,$14			# +=Maj(a,b,c)
	addu	$7,$12
	addu	$31,$12
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$10,8($5)
#else
	lwl	$10,11($5)
	lwr	$10,8($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$9,$9		# byte swap(1)
	rotr	$9,$9,16
#else
	srl	$14,$9,24		# byte swap(1)
	srl	$15,$9,8
	andi	$16,$9,0xFF00
	sll	$9,$9,24
	andi	$15,0xFF00
	sll	$16,$16,8
	or	$9,$14
	or	$15,$16
	or	$9,$15
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$16,$24,$25			# 1
	rotr	$14,$7,6
	addu	$13,$9,$30
	rotr	$15,$7,11
	and	$16,$7
	rotr	$30,$7,25
	xor	$14,$15
	rotr	$15,$31,2
	xor	$16,$25			# Ch(e,f,g)
	xor	$14,$30			# Sigma1(e)

	rotr	$30,$31,13
	addu	$13,$16
	lw	$16,4($6)		# K[1]
	xor	$30,$15
	rotr	$15,$31,22
	addu	$13,$14
	and	$14,$1,$2
	xor	$30,$15			# Sigma0(a)
	xor	$15,$1,$2
#else
	addu	$13,$9,$30			# 1
	srl	$30,$7,6
	xor	$16,$24,$25
	sll	$15,$7,7
	and	$16,$7
	srl	$14,$7,11
	xor	$30,$15
	sll	$15,$7,21
	xor	$30,$14
	srl	$14,$7,25
	xor	$30,$15
	sll	$15,$7,26
	xor	$30,$14
	xor	$16,$25			# Ch(e,f,g)
	xor	$14,$15,$30			# Sigma1(e)

	srl	$30,$31,2
	addu	$13,$16
	lw	$16,4($6)		# K[1]
	sll	$15,$31,10
	addu	$13,$14
	srl	$14,$31,13
	xor	$30,$15
	sll	$15,$31,19
	xor	$30,$14
	srl	$14,$31,22
	xor	$30,$15
	sll	$15,$31,30
	xor	$30,$14
	and	$14,$1,$2
	xor	$30,$15			# Sigma0(a)
	xor	$15,$1,$2
#endif
	sw	$9,4($29)	# offload to ring buffer
	addu	$30,$14
	and	$15,$31
	addu	$13,$16			# +=K[1]
	addu	$30,$15			# +=Maj(a,b,c)
	addu	$3,$13
	addu	$30,$13
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$11,12($5)
#else
	lwl	$11,15($5)
	lwr	$11,12($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$10,$10		# byte swap(2)
	rotr	$10,$10,16
#else
	srl	$15,$10,24		# byte swap(2)
	srl	$16,$10,8
	andi	$17,$10,0xFF00
	sll	$10,$10,24
	andi	$16,0xFF00
	sll	$17,$17,8
	or	$10,$15
	or	$16,$17
	or	$10,$16
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$17,$7,$24			# 2
	rotr	$15,$3,6
	addu	$14,$10,$25
	rotr	$16,$3,11
	and	$17,$3
	rotr	$25,$3,25
	xor	$15,$16
	rotr	$16,$30,2
	xor	$17,$24			# Ch(e,f,g)
	xor	$15,$25			# Sigma1(e)

	rotr	$25,$30,13
	addu	$14,$17
	lw	$17,8($6)		# K[2]
	xor	$25,$16
	rotr	$16,$30,22
	addu	$14,$15
	and	$15,$31,$1
	xor	$25,$16			# Sigma0(a)
	xor	$16,$31,$1
#else
	addu	$14,$10,$25			# 2
	srl	$25,$3,6
	xor	$17,$7,$24
	sll	$16,$3,7
	and	$17,$3
	srl	$15,$3,11
	xor	$25,$16
	sll	$16,$3,21
	xor	$25,$15
	srl	$15,$3,25
	xor	$25,$16
	sll	$16,$3,26
	xor	$25,$15
	xor	$17,$24			# Ch(e,f,g)
	xor	$15,$16,$25			# Sigma1(e)

	srl	$25,$30,2
	addu	$14,$17
	lw	$17,8($6)		# K[2]
	sll	$16,$30,10
	addu	$14,$15
	srl	$15,$30,13
	xor	$25,$16
	sll	$16,$30,19
	xor	$25,$15
	srl	$15,$30,22
	xor	$25,$16
	sll	$16,$30,30
	xor	$25,$15
	and	$15,$31,$1
	xor	$25,$16			# Sigma0(a)
	xor	$16,$31,$1
#endif
	sw	$10,8($29)	# offload to ring buffer
	addu	$25,$15
	and	$16,$30
	addu	$14,$17			# +=K[2]
	addu	$25,$16			# +=Maj(a,b,c)
	addu	$2,$14
	addu	$25,$14
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$12,16($5)
#else
	lwl	$12,19($5)
	lwr	$12,16($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$11,$11		# byte swap(3)
	rotr	$11,$11,16
#else
	srl	$16,$11,24		# byte swap(3)
	srl	$17,$11,8
	andi	$18,$11,0xFF00
	sll	$11,$11,24
	andi	$17,0xFF00
	sll	$18,$18,8
	or	$11,$16
	or	$17,$18
	or	$11,$17
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$18,$3,$7			# 3
	rotr	$16,$2,6
	addu	$15,$11,$24
	rotr	$17,$2,11
	and	$18,$2
	rotr	$24,$2,25
	xor	$16,$17
	rotr	$17,$25,2
	xor	$18,$7			# Ch(e,f,g)
	xor	$16,$24			# Sigma1(e)

	rotr	$24,$25,13
	addu	$15,$18
	lw	$18,12($6)		# K[3]
	xor	$24,$17
	rotr	$17,$25,22
	addu	$15,$16
	and	$16,$30,$31
	xor	$24,$17			# Sigma0(a)
	xor	$17,$30,$31
#else
	addu	$15,$11,$24			# 3
	srl	$24,$2,6
	xor	$18,$3,$7
	sll	$17,$2,7
	and	$18,$2
	srl	$16,$2,11
	xor	$24,$17
	sll	$17,$2,21
	xor	$24,$16
	srl	$16,$2,25
	xor	$24,$17
	sll	$17,$2,26
	xor	$24,$16
	xor	$18,$7			# Ch(e,f,g)
	xor	$16,$17,$24			# Sigma1(e)

	srl	$24,$25,2
	addu	$15,$18
	lw	$18,12($6)		# K[3]
	sll	$17,$25,10
	addu	$15,$16
	srl	$16,$25,13
	xor	$24,$17
	sll	$17,$25,19
	xor	$24,$16
	srl	$16,$25,22
	xor	$24,$17
	sll	$17,$25,30
	xor	$24,$16
	and	$16,$30,$31
	xor	$24,$17			# Sigma0(a)
	xor	$17,$30,$31
#endif
	sw	$11,12($29)	# offload to ring buffer
	addu	$24,$16
	and	$17,$25
	addu	$15,$18			# +=K[3]
	addu	$24,$17			# +=Maj(a,b,c)
	addu	$1,$15
	addu	$24,$15
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$13,20($5)
#else
	lwl	$13,23($5)
	lwr	$13,20($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$12,$12		# byte swap(4)
	rotr	$12,$12,16
#else
	srl	$17,$12,24		# byte swap(4)
	srl	$18,$12,8
	andi	$19,$12,0xFF00
	sll	$12,$12,24
	andi	$18,0xFF00
	sll	$19,$19,8
	or	$12,$17
	or	$18,$19
	or	$12,$18
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$19,$2,$3			# 4
	rotr	$17,$1,6
	addu	$16,$12,$7
	rotr	$18,$1,11
	and	$19,$1
	rotr	$7,$1,25
	xor	$17,$18
	rotr	$18,$24,2
	xor	$19,$3			# Ch(e,f,g)
	xor	$17,$7			# Sigma1(e)

	rotr	$7,$24,13
	addu	$16,$19
	lw	$19,16($6)		# K[4]
	xor	$7,$18
	rotr	$18,$24,22
	addu	$16,$17
	and	$17,$25,$30
	xor	$7,$18			# Sigma0(a)
	xor	$18,$25,$30
#else
	addu	$16,$12,$7			# 4
	srl	$7,$1,6
	xor	$19,$2,$3
	sll	$18,$1,7
	and	$19,$1
	srl	$17,$1,11
	xor	$7,$18
	sll	$18,$1,21
	xor	$7,$17
	srl	$17,$1,25
	xor	$7,$18
	sll	$18,$1,26
	xor	$7,$17
	xor	$19,$3			# Ch(e,f,g)
	xor	$17,$18,$7			# Sigma1(e)

	srl	$7,$24,2
	addu	$16,$19
	lw	$19,16($6)		# K[4]
	sll	$18,$24,10
	addu	$16,$17
	srl	$17,$24,13
	xor	$7,$18
	sll	$18,$24,19
	xor	$7,$17
	srl	$17,$24,22
	xor	$7,$18
	sll	$18,$24,30
	xor	$7,$17
	and	$17,$25,$30
	xor	$7,$18			# Sigma0(a)
	xor	$18,$25,$30
#endif
	sw	$12,16($29)	# offload to ring buffer
	addu	$7,$17
	and	$18,$24
	addu	$16,$19			# +=K[4]
	addu	$7,$18			# +=Maj(a,b,c)
	addu	$31,$16
	addu	$7,$16
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$14,24($5)
#else
	lwl	$14,27($5)
	lwr	$14,24($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$13,$13		# byte swap(5)
	rotr	$13,$13,16
#else
	srl	$18,$13,24		# byte swap(5)
	srl	$19,$13,8
	andi	$20,$13,0xFF00
	sll	$13,$13,24
	andi	$19,0xFF00
	sll	$20,$20,8
	or	$13,$18
	or	$19,$20
	or	$13,$19
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$20,$1,$2			# 5
	rotr	$18,$31,6
	addu	$17,$13,$3
	rotr	$19,$31,11
	and	$20,$31
	rotr	$3,$31,25
	xor	$18,$19
	rotr	$19,$7,2
	xor	$20,$2			# Ch(e,f,g)
	xor	$18,$3			# Sigma1(e)

	rotr	$3,$7,13
	addu	$17,$20
	lw	$20,20($6)		# K[5]
	xor	$3,$19
	rotr	$19,$7,22
	addu	$17,$18
	and	$18,$24,$25
	xor	$3,$19			# Sigma0(a)
	xor	$19,$24,$25
#else
	addu	$17,$13,$3			# 5
	srl	$3,$31,6
	xor	$20,$1,$2
	sll	$19,$31,7
	and	$20,$31
	srl	$18,$31,11
	xor	$3,$19
	sll	$19,$31,21
	xor	$3,$18
	srl	$18,$31,25
	xor	$3,$19
	sll	$19,$31,26
	xor	$3,$18
	xor	$20,$2			# Ch(e,f,g)
	xor	$18,$19,$3			# Sigma1(e)

	srl	$3,$7,2
	addu	$17,$20
	lw	$20,20($6)		# K[5]
	sll	$19,$7,10
	addu	$17,$18
	srl	$18,$7,13
	xor	$3,$19
	sll	$19,$7,19
	xor	$3,$18
	srl	$18,$7,22
	xor	$3,$19
	sll	$19,$7,30
	xor	$3,$18
	and	$18,$24,$25
	xor	$3,$19			# Sigma0(a)
	xor	$19,$24,$25
#endif
	sw	$13,20($29)	# offload to ring buffer
	addu	$3,$18
	and	$19,$7
	addu	$17,$20			# +=K[5]
	addu	$3,$19			# +=Maj(a,b,c)
	addu	$30,$17
	addu	$3,$17
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$15,28($5)
#else
	lwl	$15,31($5)
	lwr	$15,28($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$14,$14		# byte swap(6)
	rotr	$14,$14,16
#else
	srl	$19,$14,24		# byte swap(6)
	srl	$20,$14,8
	andi	$21,$14,0xFF00
	sll	$14,$14,24
	andi	$20,0xFF00
	sll	$21,$21,8
	or	$14,$19
	or	$20,$21
	or	$14,$20
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$21,$31,$1			# 6
	rotr	$19,$30,6
	addu	$18,$14,$2
	rotr	$20,$30,11
	and	$21,$30
	rotr	$2,$30,25
	xor	$19,$20
	rotr	$20,$3,2
	xor	$21,$1			# Ch(e,f,g)
	xor	$19,$2			# Sigma1(e)

	rotr	$2,$3,13
	addu	$18,$21
	lw	$21,24($6)		# K[6]
	xor	$2,$20
	rotr	$20,$3,22
	addu	$18,$19
	and	$19,$7,$24
	xor	$2,$20			# Sigma0(a)
	xor	$20,$7,$24
#else
	addu	$18,$14,$2			# 6
	srl	$2,$30,6
	xor	$21,$31,$1
	sll	$20,$30,7
	and	$21,$30
	srl	$19,$30,11
	xor	$2,$20
	sll	$20,$30,21
	xor	$2,$19
	srl	$19,$30,25
	xor	$2,$20
	sll	$20,$30,26
	xor	$2,$19
	xor	$21,$1			# Ch(e,f,g)
	xor	$19,$20,$2			# Sigma1(e)

	srl	$2,$3,2
	addu	$18,$21
	lw	$21,24($6)		# K[6]
	sll	$20,$3,10
	addu	$18,$19
	srl	$19,$3,13
	xor	$2,$20
	sll	$20,$3,19
	xor	$2,$19
	srl	$19,$3,22
	xor	$2,$20
	sll	$20,$3,30
	xor	$2,$19
	and	$19,$7,$24
	xor	$2,$20			# Sigma0(a)
	xor	$20,$7,$24
#endif
	sw	$14,24($29)	# offload to ring buffer
	addu	$2,$19
	and	$20,$3
	addu	$18,$21			# +=K[6]
	addu	$2,$20			# +=Maj(a,b,c)
	addu	$25,$18
	addu	$2,$18
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$16,32($5)
#else
	lwl	$16,35($5)
	lwr	$16,32($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$15,$15		# byte swap(7)
	rotr	$15,$15,16
#else
	srl	$20,$15,24		# byte swap(7)
	srl	$21,$15,8
	andi	$22,$15,0xFF00
	sll	$15,$15,24
	andi	$21,0xFF00
	sll	$22,$22,8
	or	$15,$20
	or	$21,$22
	or	$15,$21
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$22,$30,$31			# 7
	rotr	$20,$25,6
	addu	$19,$15,$1
	rotr	$21,$25,11
	and	$22,$25
	rotr	$1,$25,25
	xor	$20,$21
	rotr	$21,$2,2
	xor	$22,$31			# Ch(e,f,g)
	xor	$20,$1			# Sigma1(e)

	rotr	$1,$2,13
	addu	$19,$22
	lw	$22,28($6)		# K[7]
	xor	$1,$21
	rotr	$21,$2,22
	addu	$19,$20
	and	$20,$3,$7
	xor	$1,$21			# Sigma0(a)
	xor	$21,$3,$7
#else
	addu	$19,$15,$1			# 7
	srl	$1,$25,6
	xor	$22,$30,$31
	sll	$21,$25,7
	and	$22,$25
	srl	$20,$25,11
	xor	$1,$21
	sll	$21,$25,21
	xor	$1,$20
	srl	$20,$25,25
	xor	$1,$21
	sll	$21,$25,26
	xor	$1,$20
	xor	$22,$31			# Ch(e,f,g)
	xor	$20,$21,$1			# Sigma1(e)

	srl	$1,$2,2
	addu	$19,$22
	lw	$22,28($6)		# K[7]
	sll	$21,$2,10
	addu	$19,$20
	srl	$20,$2,13
	xor	$1,$21
	sll	$21,$2,19
	xor	$1,$20
	srl	$20,$2,22
	xor	$1,$21
	sll	$21,$2,30
	xor	$1,$20
	and	$20,$3,$7
	xor	$1,$21			# Sigma0(a)
	xor	$21,$3,$7
#endif
	sw	$15,28($29)	# offload to ring buffer
	addu	$1,$20
	and	$21,$2
	addu	$19,$22			# +=K[7]
	addu	$1,$21			# +=Maj(a,b,c)
	addu	$24,$19
	addu	$1,$19
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$17,36($5)
#else
	lwl	$17,39($5)
	lwr	$17,36($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$16,$16		# byte swap(8)
	rotr	$16,$16,16
#else
	srl	$21,$16,24		# byte swap(8)
	srl	$22,$16,8
	andi	$23,$16,0xFF00
	sll	$16,$16,24
	andi	$22,0xFF00
	sll	$23,$23,8
	or	$16,$21
	or	$22,$23
	or	$16,$22
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$23,$25,$30			# 8
	rotr	$21,$24,6
	addu	$20,$16,$31
	rotr	$22,$24,11
	and	$23,$24
	rotr	$31,$24,25
	xor	$21,$22
	rotr	$22,$1,2
	xor	$23,$30			# Ch(e,f,g)
	xor	$21,$31			# Sigma1(e)

	rotr	$31,$1,13
	addu	$20,$23
	lw	$23,32($6)		# K[8]
	xor	$31,$22
	rotr	$22,$1,22
	addu	$20,$21
	and	$21,$2,$3
	xor	$31,$22			# Sigma0(a)
	xor	$22,$2,$3
#else
	addu	$20,$16,$31			# 8
	srl	$31,$24,6
	xor	$23,$25,$30
	sll	$22,$24,7
	and	$23,$24
	srl	$21,$24,11
	xor	$31,$22
	sll	$22,$24,21
	xor	$31,$21
	srl	$21,$24,25
	xor	$31,$22
	sll	$22,$24,26
	xor	$31,$21
	xor	$23,$30			# Ch(e,f,g)
	xor	$21,$22,$31			# Sigma1(e)

	srl	$31,$1,2
	addu	$20,$23
	lw	$23,32($6)		# K[8]
	sll	$22,$1,10
	addu	$20,$21
	srl	$21,$1,13
	xor	$31,$22
	sll	$22,$1,19
	xor	$31,$21
	srl	$21,$1,22
	xor	$31,$22
	sll	$22,$1,30
	xor	$31,$21
	and	$21,$2,$3
	xor	$31,$22			# Sigma0(a)
	xor	$22,$2,$3
#endif
	sw	$16,32($29)	# offload to ring buffer
	addu	$31,$21
	and	$22,$1
	addu	$20,$23			# +=K[8]
	addu	$31,$22			# +=Maj(a,b,c)
	addu	$7,$20
	addu	$31,$20
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$18,40($5)
#else
	lwl	$18,43($5)
	lwr	$18,40($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$17,$17		# byte swap(9)
	rotr	$17,$17,16
#else
	srl	$22,$17,24		# byte swap(9)
	srl	$23,$17,8
	andi	$8,$17,0xFF00
	sll	$17,$17,24
	andi	$23,0xFF00
	sll	$8,$8,8
	or	$17,$22
	or	$23,$8
	or	$17,$23
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$8,$24,$25			# 9
	rotr	$22,$7,6
	addu	$21,$17,$30
	rotr	$23,$7,11
	and	$8,$7
	rotr	$30,$7,25
	xor	$22,$23
	rotr	$23,$31,2
	xor	$8,$25			# Ch(e,f,g)
	xor	$22,$30			# Sigma1(e)

	rotr	$30,$31,13
	addu	$21,$8
	lw	$8,36($6)		# K[9]
	xor	$30,$23
	rotr	$23,$31,22
	addu	$21,$22
	and	$22,$1,$2
	xor	$30,$23			# Sigma0(a)
	xor	$23,$1,$2
#else
	addu	$21,$17,$30			# 9
	srl	$30,$7,6
	xor	$8,$24,$25
	sll	$23,$7,7
	and	$8,$7
	srl	$22,$7,11
	xor	$30,$23
	sll	$23,$7,21
	xor	$30,$22
	srl	$22,$7,25
	xor	$30,$23
	sll	$23,$7,26
	xor	$30,$22
	xor	$8,$25			# Ch(e,f,g)
	xor	$22,$23,$30			# Sigma1(e)

	srl	$30,$31,2
	addu	$21,$8
	lw	$8,36($6)		# K[9]
	sll	$23,$31,10
	addu	$21,$22
	srl	$22,$31,13
	xor	$30,$23
	sll	$23,$31,19
	xor	$30,$22
	srl	$22,$31,22
	xor	$30,$23
	sll	$23,$31,30
	xor	$30,$22
	and	$22,$1,$2
	xor	$30,$23			# Sigma0(a)
	xor	$23,$1,$2
#endif
	sw	$17,36($29)	# offload to ring buffer
	addu	$30,$22
	and	$23,$31
	addu	$21,$8			# +=K[9]
	addu	$30,$23			# +=Maj(a,b,c)
	addu	$3,$21
	addu	$30,$21
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$19,44($5)
#else
	lwl	$19,47($5)
	lwr	$19,44($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$18,$18		# byte swap(10)
	rotr	$18,$18,16
#else
	srl	$23,$18,24		# byte swap(10)
	srl	$8,$18,8
	andi	$9,$18,0xFF00
	sll	$18,$18,24
	andi	$8,0xFF00
	sll	$9,$9,8
	or	$18,$23
	or	$8,$9
	or	$18,$8
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$9,$7,$24			# 10
	rotr	$23,$3,6
	addu	$22,$18,$25
	rotr	$8,$3,11
	and	$9,$3
	rotr	$25,$3,25
	xor	$23,$8
	rotr	$8,$30,2
	xor	$9,$24			# Ch(e,f,g)
	xor	$23,$25			# Sigma1(e)

	rotr	$25,$30,13
	addu	$22,$9
	lw	$9,40($6)		# K[10]
	xor	$25,$8
	rotr	$8,$30,22
	addu	$22,$23
	and	$23,$31,$1
	xor	$25,$8			# Sigma0(a)
	xor	$8,$31,$1
#else
	addu	$22,$18,$25			# 10
	srl	$25,$3,6
	xor	$9,$7,$24
	sll	$8,$3,7
	and	$9,$3
	srl	$23,$3,11
	xor	$25,$8
	sll	$8,$3,21
	xor	$25,$23
	srl	$23,$3,25
	xor	$25,$8
	sll	$8,$3,26
	xor	$25,$23
	xor	$9,$24			# Ch(e,f,g)
	xor	$23,$8,$25			# Sigma1(e)

	srl	$25,$30,2
	addu	$22,$9
	lw	$9,40($6)		# K[10]
	sll	$8,$30,10
	addu	$22,$23
	srl	$23,$30,13
	xor	$25,$8
	sll	$8,$30,19
	xor	$25,$23
	srl	$23,$30,22
	xor	$25,$8
	sll	$8,$30,30
	xor	$25,$23
	and	$23,$31,$1
	xor	$25,$8			# Sigma0(a)
	xor	$8,$31,$1
#endif
	sw	$18,40($29)	# offload to ring buffer
	addu	$25,$23
	and	$8,$30
	addu	$22,$9			# +=K[10]
	addu	$25,$8			# +=Maj(a,b,c)
	addu	$2,$22
	addu	$25,$22
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$20,48($5)
#else
	lwl	$20,51($5)
	lwr	$20,48($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$19,$19		# byte swap(11)
	rotr	$19,$19,16
#else
	srl	$8,$19,24		# byte swap(11)
	srl	$9,$19,8
	andi	$10,$19,0xFF00
	sll	$19,$19,24
	andi	$9,0xFF00
	sll	$10,$10,8
	or	$19,$8
	or	$9,$10
	or	$19,$9
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$10,$3,$7			# 11
	rotr	$8,$2,6
	addu	$23,$19,$24
	rotr	$9,$2,11
	and	$10,$2
	rotr	$24,$2,25
	xor	$8,$9
	rotr	$9,$25,2
	xor	$10,$7			# Ch(e,f,g)
	xor	$8,$24			# Sigma1(e)

	rotr	$24,$25,13
	addu	$23,$10
	lw	$10,44($6)		# K[11]
	xor	$24,$9
	rotr	$9,$25,22
	addu	$23,$8
	and	$8,$30,$31
	xor	$24,$9			# Sigma0(a)
	xor	$9,$30,$31
#else
	addu	$23,$19,$24			# 11
	srl	$24,$2,6
	xor	$10,$3,$7
	sll	$9,$2,7
	and	$10,$2
	srl	$8,$2,11
	xor	$24,$9
	sll	$9,$2,21
	xor	$24,$8
	srl	$8,$2,25
	xor	$24,$9
	sll	$9,$2,26
	xor	$24,$8
	xor	$10,$7			# Ch(e,f,g)
	xor	$8,$9,$24			# Sigma1(e)

	srl	$24,$25,2
	addu	$23,$10
	lw	$10,44($6)		# K[11]
	sll	$9,$25,10
	addu	$23,$8
	srl	$8,$25,13
	xor	$24,$9
	sll	$9,$25,19
	xor	$24,$8
	srl	$8,$25,22
	xor	$24,$9
	sll	$9,$25,30
	xor	$24,$8
	and	$8,$30,$31
	xor	$24,$9			# Sigma0(a)
	xor	$9,$30,$31
#endif
	sw	$19,44($29)	# offload to ring buffer
	addu	$24,$8
	and	$9,$25
	addu	$23,$10			# +=K[11]
	addu	$24,$9			# +=Maj(a,b,c)
	addu	$1,$23
	addu	$24,$23
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$21,52($5)
#else
	lwl	$21,55($5)
	lwr	$21,52($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$20,$20		# byte swap(12)
	rotr	$20,$20,16
#else
	srl	$9,$20,24		# byte swap(12)
	srl	$10,$20,8
	andi	$11,$20,0xFF00
	sll	$20,$20,24
	andi	$10,0xFF00
	sll	$11,$11,8
	or	$20,$9
	or	$10,$11
	or	$20,$10
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$11,$2,$3			# 12
	rotr	$9,$1,6
	addu	$8,$20,$7
	rotr	$10,$1,11
	and	$11,$1
	rotr	$7,$1,25
	xor	$9,$10
	rotr	$10,$24,2
	xor	$11,$3			# Ch(e,f,g)
	xor	$9,$7			# Sigma1(e)

	rotr	$7,$24,13
	addu	$8,$11
	lw	$11,48($6)		# K[12]
	xor	$7,$10
	rotr	$10,$24,22
	addu	$8,$9
	and	$9,$25,$30
	xor	$7,$10			# Sigma0(a)
	xor	$10,$25,$30
#else
	addu	$8,$20,$7			# 12
	srl	$7,$1,6
	xor	$11,$2,$3
	sll	$10,$1,7
	and	$11,$1
	srl	$9,$1,11
	xor	$7,$10
	sll	$10,$1,21
	xor	$7,$9
	srl	$9,$1,25
	xor	$7,$10
	sll	$10,$1,26
	xor	$7,$9
	xor	$11,$3			# Ch(e,f,g)
	xor	$9,$10,$7			# Sigma1(e)

	srl	$7,$24,2
	addu	$8,$11
	lw	$11,48($6)		# K[12]
	sll	$10,$24,10
	addu	$8,$9
	srl	$9,$24,13
	xor	$7,$10
	sll	$10,$24,19
	xor	$7,$9
	srl	$9,$24,22
	xor	$7,$10
	sll	$10,$24,30
	xor	$7,$9
	and	$9,$25,$30
	xor	$7,$10			# Sigma0(a)
	xor	$10,$25,$30
#endif
	sw	$20,48($29)	# offload to ring buffer
	addu	$7,$9
	and	$10,$24
	addu	$8,$11			# +=K[12]
	addu	$7,$10			# +=Maj(a,b,c)
	addu	$31,$8
	addu	$7,$8
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$22,56($5)
#else
	lwl	$22,59($5)
	lwr	$22,56($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$21,$21		# byte swap(13)
	rotr	$21,$21,16
#else
	srl	$10,$21,24		# byte swap(13)
	srl	$11,$21,8
	andi	$12,$21,0xFF00
	sll	$21,$21,24
	andi	$11,0xFF00
	sll	$12,$12,8
	or	$21,$10
	or	$11,$12
	or	$21,$11
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$12,$1,$2			# 13
	rotr	$10,$31,6
	addu	$9,$21,$3
	rotr	$11,$31,11
	and	$12,$31
	rotr	$3,$31,25
	xor	$10,$11
	rotr	$11,$7,2
	xor	$12,$2			# Ch(e,f,g)
	xor	$10,$3			# Sigma1(e)

	rotr	$3,$7,13
	addu	$9,$12
	lw	$12,52($6)		# K[13]
	xor	$3,$11
	rotr	$11,$7,22
	addu	$9,$10
	and	$10,$24,$25
	xor	$3,$11			# Sigma0(a)
	xor	$11,$24,$25
#else
	addu	$9,$21,$3			# 13
	srl	$3,$31,6
	xor	$12,$1,$2
	sll	$11,$31,7
	and	$12,$31
	srl	$10,$31,11
	xor	$3,$11
	sll	$11,$31,21
	xor	$3,$10
	srl	$10,$31,25
	xor	$3,$11
	sll	$11,$31,26
	xor	$3,$10
	xor	$12,$2			# Ch(e,f,g)
	xor	$10,$11,$3			# Sigma1(e)

	srl	$3,$7,2
	addu	$9,$12
	lw	$12,52($6)		# K[13]
	sll	$11,$7,10
	addu	$9,$10
	srl	$10,$7,13
	xor	$3,$11
	sll	$11,$7,19
	xor	$3,$10
	srl	$10,$7,22
	xor	$3,$11
	sll	$11,$7,30
	xor	$3,$10
	and	$10,$24,$25
	xor	$3,$11			# Sigma0(a)
	xor	$11,$24,$25
#endif
	sw	$21,52($29)	# offload to ring buffer
	addu	$3,$10
	and	$11,$7
	addu	$9,$12			# +=K[13]
	addu	$3,$11			# +=Maj(a,b,c)
	addu	$30,$9
	addu	$3,$9
	lw	$8,0($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R6) || defined(_MIPS_ARCH_MIPS64R6)
	lw	$23,60($5)
#else
	lwl	$23,63($5)
	lwr	$23,60($5)
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$22,$22		# byte swap(14)
	rotr	$22,$22,16
#else
	srl	$11,$22,24		# byte swap(14)
	srl	$12,$22,8
	andi	$13,$22,0xFF00
	sll	$22,$22,24
	andi	$12,0xFF00
	sll	$13,$13,8
	or	$22,$11
	or	$12,$13
	or	$22,$12
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$13,$31,$1			# 14
	rotr	$11,$30,6
	addu	$10,$22,$2
	rotr	$12,$30,11
	and	$13,$30
	rotr	$2,$30,25
	xor	$11,$12
	rotr	$12,$3,2
	xor	$13,$1			# Ch(e,f,g)
	xor	$11,$2			# Sigma1(e)

	rotr	$2,$3,13
	addu	$10,$13
	lw	$13,56($6)		# K[14]
	xor	$2,$12
	rotr	$12,$3,22
	addu	$10,$11
	and	$11,$7,$24
	xor	$2,$12			# Sigma0(a)
	xor	$12,$7,$24
#else
	addu	$10,$22,$2			# 14
	srl	$2,$30,6
	xor	$13,$31,$1
	sll	$12,$30,7
	and	$13,$30
	srl	$11,$30,11
	xor	$2,$12
	sll	$12,$30,21
	xor	$2,$11
	srl	$11,$30,25
	xor	$2,$12
	sll	$12,$30,26
	xor	$2,$11
	xor	$13,$1			# Ch(e,f,g)
	xor	$11,$12,$2			# Sigma1(e)

	srl	$2,$3,2
	addu	$10,$13
	lw	$13,56($6)		# K[14]
	sll	$12,$3,10
	addu	$10,$11
	srl	$11,$3,13
	xor	$2,$12
	sll	$12,$3,19
	xor	$2,$11
	srl	$11,$3,22
	xor	$2,$12
	sll	$12,$3,30
	xor	$2,$11
	and	$11,$7,$24
	xor	$2,$12			# Sigma0(a)
	xor	$12,$7,$24
#endif
	sw	$22,56($29)	# offload to ring buffer
	addu	$2,$11
	and	$12,$3
	addu	$10,$13			# +=K[14]
	addu	$2,$12			# +=Maj(a,b,c)
	addu	$25,$10
	addu	$2,$10
	lw	$9,4($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	wsbh	$23,$23		# byte swap(15)
	rotr	$23,$23,16
#else
	srl	$12,$23,24		# byte swap(15)
	srl	$13,$23,8
	andi	$14,$23,0xFF00
	sll	$23,$23,24
	andi	$13,0xFF00
	sll	$14,$14,8
	or	$23,$12
	or	$13,$14
	or	$23,$13
#endif
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$14,$30,$31			# 15
	rotr	$12,$25,6
	addu	$11,$23,$1
	rotr	$13,$25,11
	and	$14,$25
	rotr	$1,$25,25
	xor	$12,$13
	rotr	$13,$2,2
	xor	$14,$31			# Ch(e,f,g)
	xor	$12,$1			# Sigma1(e)

	rotr	$1,$2,13
	addu	$11,$14
	lw	$14,60($6)		# K[15]
	xor	$1,$13
	rotr	$13,$2,22
	addu	$11,$12
	and	$12,$3,$7
	xor	$1,$13			# Sigma0(a)
	xor	$13,$3,$7
#else
	addu	$11,$23,$1			# 15
	srl	$1,$25,6
	xor	$14,$30,$31
	sll	$13,$25,7
	and	$14,$25
	srl	$12,$25,11
	xor	$1,$13
	sll	$13,$25,21
	xor	$1,$12
	srl	$12,$25,25
	xor	$1,$13
	sll	$13,$25,26
	xor	$1,$12
	xor	$14,$31			# Ch(e,f,g)
	xor	$12,$13,$1			# Sigma1(e)

	srl	$1,$2,2
	addu	$11,$14
	lw	$14,60($6)		# K[15]
	sll	$13,$2,10
	addu	$11,$12
	srl	$12,$2,13
	xor	$1,$13
	sll	$13,$2,19
	xor	$1,$12
	srl	$12,$2,22
	xor	$1,$13
	sll	$13,$2,30
	xor	$1,$12
	and	$12,$3,$7
	xor	$1,$13			# Sigma0(a)
	xor	$13,$3,$7
#endif
	sw	$23,60($29)	# offload to ring buffer
	addu	$1,$12
	and	$13,$2
	addu	$11,$14			# +=K[15]
	addu	$1,$13			# +=Maj(a,b,c)
	addu	$24,$11
	addu	$1,$11
	lw	$10,8($29)	# prefetch from ring buffer
	b	.L16_xx
.align	4
.L16_xx:
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$14,$9,3		# Xupdate(16)
	rotr	$12,$9,7
	addu	$8,$17			# +=X[i+9]
	xor	$14,$12
	rotr	$12,$9,18

	srl	$15,$22,10
	rotr	$13,$22,17
	xor	$14,$12			# sigma0(X[i+1])
	rotr	$12,$22,19
	xor	$15,$13
	addu	$8,$14
#else
	srl	$14,$9,3		# Xupdate(16)
	addu	$8,$17			# +=X[i+9]
	sll	$13,$9,14
	srl	$12,$9,7
	xor	$14,$13
	sll	$13,11
	xor	$14,$12
	srl	$12,$9,18
	xor	$14,$13

	srl	$15,$22,10
	xor	$14,$12			# sigma0(X[i+1])
	sll	$13,$22,13
	addu	$8,$14
	srl	$12,$22,17
	xor	$15,$13
	sll	$13,2
	xor	$15,$12
	srl	$12,$22,19
	xor	$15,$13
#endif
	xor	$15,$12			# sigma1(X[i+14])
	addu	$8,$15
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$15,$25,$30			# 16
	rotr	$13,$24,6
	addu	$12,$8,$31
	rotr	$14,$24,11
	and	$15,$24
	rotr	$31,$24,25
	xor	$13,$14
	rotr	$14,$1,2
	xor	$15,$30			# Ch(e,f,g)
	xor	$13,$31			# Sigma1(e)

	rotr	$31,$1,13
	addu	$12,$15
	lw	$15,64($6)		# K[16]
	xor	$31,$14
	rotr	$14,$1,22
	addu	$12,$13
	and	$13,$2,$3
	xor	$31,$14			# Sigma0(a)
	xor	$14,$2,$3
#else
	addu	$12,$8,$31			# 16
	srl	$31,$24,6
	xor	$15,$25,$30
	sll	$14,$24,7
	and	$15,$24
	srl	$13,$24,11
	xor	$31,$14
	sll	$14,$24,21
	xor	$31,$13
	srl	$13,$24,25
	xor	$31,$14
	sll	$14,$24,26
	xor	$31,$13
	xor	$15,$30			# Ch(e,f,g)
	xor	$13,$14,$31			# Sigma1(e)

	srl	$31,$1,2
	addu	$12,$15
	lw	$15,64($6)		# K[16]
	sll	$14,$1,10
	addu	$12,$13
	srl	$13,$1,13
	xor	$31,$14
	sll	$14,$1,19
	xor	$31,$13
	srl	$13,$1,22
	xor	$31,$14
	sll	$14,$1,30
	xor	$31,$13
	and	$13,$2,$3
	xor	$31,$14			# Sigma0(a)
	xor	$14,$2,$3
#endif
	sw	$8,0($29)	# offload to ring buffer
	addu	$31,$13
	and	$14,$1
	addu	$12,$15			# +=K[16]
	addu	$31,$14			# +=Maj(a,b,c)
	addu	$7,$12
	addu	$31,$12
	lw	$11,12($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$15,$10,3		# Xupdate(17)
	rotr	$13,$10,7
	addu	$9,$18			# +=X[i+9]
	xor	$15,$13
	rotr	$13,$10,18

	srl	$16,$23,10
	rotr	$14,$23,17
	xor	$15,$13			# sigma0(X[i+1])
	rotr	$13,$23,19
	xor	$16,$14
	addu	$9,$15
#else
	srl	$15,$10,3		# Xupdate(17)
	addu	$9,$18			# +=X[i+9]
	sll	$14,$10,14
	srl	$13,$10,7
	xor	$15,$14
	sll	$14,11
	xor	$15,$13
	srl	$13,$10,18
	xor	$15,$14

	srl	$16,$23,10
	xor	$15,$13			# sigma0(X[i+1])
	sll	$14,$23,13
	addu	$9,$15
	srl	$13,$23,17
	xor	$16,$14
	sll	$14,2
	xor	$16,$13
	srl	$13,$23,19
	xor	$16,$14
#endif
	xor	$16,$13			# sigma1(X[i+14])
	addu	$9,$16
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$16,$24,$25			# 17
	rotr	$14,$7,6
	addu	$13,$9,$30
	rotr	$15,$7,11
	and	$16,$7
	rotr	$30,$7,25
	xor	$14,$15
	rotr	$15,$31,2
	xor	$16,$25			# Ch(e,f,g)
	xor	$14,$30			# Sigma1(e)

	rotr	$30,$31,13
	addu	$13,$16
	lw	$16,68($6)		# K[17]
	xor	$30,$15
	rotr	$15,$31,22
	addu	$13,$14
	and	$14,$1,$2
	xor	$30,$15			# Sigma0(a)
	xor	$15,$1,$2
#else
	addu	$13,$9,$30			# 17
	srl	$30,$7,6
	xor	$16,$24,$25
	sll	$15,$7,7
	and	$16,$7
	srl	$14,$7,11
	xor	$30,$15
	sll	$15,$7,21
	xor	$30,$14
	srl	$14,$7,25
	xor	$30,$15
	sll	$15,$7,26
	xor	$30,$14
	xor	$16,$25			# Ch(e,f,g)
	xor	$14,$15,$30			# Sigma1(e)

	srl	$30,$31,2
	addu	$13,$16
	lw	$16,68($6)		# K[17]
	sll	$15,$31,10
	addu	$13,$14
	srl	$14,$31,13
	xor	$30,$15
	sll	$15,$31,19
	xor	$30,$14
	srl	$14,$31,22
	xor	$30,$15
	sll	$15,$31,30
	xor	$30,$14
	and	$14,$1,$2
	xor	$30,$15			# Sigma0(a)
	xor	$15,$1,$2
#endif
	sw	$9,4($29)	# offload to ring buffer
	addu	$30,$14
	and	$15,$31
	addu	$13,$16			# +=K[17]
	addu	$30,$15			# +=Maj(a,b,c)
	addu	$3,$13
	addu	$30,$13
	lw	$12,16($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$16,$11,3		# Xupdate(18)
	rotr	$14,$11,7
	addu	$10,$19			# +=X[i+9]
	xor	$16,$14
	rotr	$14,$11,18

	srl	$17,$8,10
	rotr	$15,$8,17
	xor	$16,$14			# sigma0(X[i+1])
	rotr	$14,$8,19
	xor	$17,$15
	addu	$10,$16
#else
	srl	$16,$11,3		# Xupdate(18)
	addu	$10,$19			# +=X[i+9]
	sll	$15,$11,14
	srl	$14,$11,7
	xor	$16,$15
	sll	$15,11
	xor	$16,$14
	srl	$14,$11,18
	xor	$16,$15

	srl	$17,$8,10
	xor	$16,$14			# sigma0(X[i+1])
	sll	$15,$8,13
	addu	$10,$16
	srl	$14,$8,17
	xor	$17,$15
	sll	$15,2
	xor	$17,$14
	srl	$14,$8,19
	xor	$17,$15
#endif
	xor	$17,$14			# sigma1(X[i+14])
	addu	$10,$17
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$17,$7,$24			# 18
	rotr	$15,$3,6
	addu	$14,$10,$25
	rotr	$16,$3,11
	and	$17,$3
	rotr	$25,$3,25
	xor	$15,$16
	rotr	$16,$30,2
	xor	$17,$24			# Ch(e,f,g)
	xor	$15,$25			# Sigma1(e)

	rotr	$25,$30,13
	addu	$14,$17
	lw	$17,72($6)		# K[18]
	xor	$25,$16
	rotr	$16,$30,22
	addu	$14,$15
	and	$15,$31,$1
	xor	$25,$16			# Sigma0(a)
	xor	$16,$31,$1
#else
	addu	$14,$10,$25			# 18
	srl	$25,$3,6
	xor	$17,$7,$24
	sll	$16,$3,7
	and	$17,$3
	srl	$15,$3,11
	xor	$25,$16
	sll	$16,$3,21
	xor	$25,$15
	srl	$15,$3,25
	xor	$25,$16
	sll	$16,$3,26
	xor	$25,$15
	xor	$17,$24			# Ch(e,f,g)
	xor	$15,$16,$25			# Sigma1(e)

	srl	$25,$30,2
	addu	$14,$17
	lw	$17,72($6)		# K[18]
	sll	$16,$30,10
	addu	$14,$15
	srl	$15,$30,13
	xor	$25,$16
	sll	$16,$30,19
	xor	$25,$15
	srl	$15,$30,22
	xor	$25,$16
	sll	$16,$30,30
	xor	$25,$15
	and	$15,$31,$1
	xor	$25,$16			# Sigma0(a)
	xor	$16,$31,$1
#endif
	sw	$10,8($29)	# offload to ring buffer
	addu	$25,$15
	and	$16,$30
	addu	$14,$17			# +=K[18]
	addu	$25,$16			# +=Maj(a,b,c)
	addu	$2,$14
	addu	$25,$14
	lw	$13,20($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$17,$12,3		# Xupdate(19)
	rotr	$15,$12,7
	addu	$11,$20			# +=X[i+9]
	xor	$17,$15
	rotr	$15,$12,18

	srl	$18,$9,10
	rotr	$16,$9,17
	xor	$17,$15			# sigma0(X[i+1])
	rotr	$15,$9,19
	xor	$18,$16
	addu	$11,$17
#else
	srl	$17,$12,3		# Xupdate(19)
	addu	$11,$20			# +=X[i+9]
	sll	$16,$12,14
	srl	$15,$12,7
	xor	$17,$16
	sll	$16,11
	xor	$17,$15
	srl	$15,$12,18
	xor	$17,$16

	srl	$18,$9,10
	xor	$17,$15			# sigma0(X[i+1])
	sll	$16,$9,13
	addu	$11,$17
	srl	$15,$9,17
	xor	$18,$16
	sll	$16,2
	xor	$18,$15
	srl	$15,$9,19
	xor	$18,$16
#endif
	xor	$18,$15			# sigma1(X[i+14])
	addu	$11,$18
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$18,$3,$7			# 19
	rotr	$16,$2,6
	addu	$15,$11,$24
	rotr	$17,$2,11
	and	$18,$2
	rotr	$24,$2,25
	xor	$16,$17
	rotr	$17,$25,2
	xor	$18,$7			# Ch(e,f,g)
	xor	$16,$24			# Sigma1(e)

	rotr	$24,$25,13
	addu	$15,$18
	lw	$18,76($6)		# K[19]
	xor	$24,$17
	rotr	$17,$25,22
	addu	$15,$16
	and	$16,$30,$31
	xor	$24,$17			# Sigma0(a)
	xor	$17,$30,$31
#else
	addu	$15,$11,$24			# 19
	srl	$24,$2,6
	xor	$18,$3,$7
	sll	$17,$2,7
	and	$18,$2
	srl	$16,$2,11
	xor	$24,$17
	sll	$17,$2,21
	xor	$24,$16
	srl	$16,$2,25
	xor	$24,$17
	sll	$17,$2,26
	xor	$24,$16
	xor	$18,$7			# Ch(e,f,g)
	xor	$16,$17,$24			# Sigma1(e)

	srl	$24,$25,2
	addu	$15,$18
	lw	$18,76($6)		# K[19]
	sll	$17,$25,10
	addu	$15,$16
	srl	$16,$25,13
	xor	$24,$17
	sll	$17,$25,19
	xor	$24,$16
	srl	$16,$25,22
	xor	$24,$17
	sll	$17,$25,30
	xor	$24,$16
	and	$16,$30,$31
	xor	$24,$17			# Sigma0(a)
	xor	$17,$30,$31
#endif
	sw	$11,12($29)	# offload to ring buffer
	addu	$24,$16
	and	$17,$25
	addu	$15,$18			# +=K[19]
	addu	$24,$17			# +=Maj(a,b,c)
	addu	$1,$15
	addu	$24,$15
	lw	$14,24($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$18,$13,3		# Xupdate(20)
	rotr	$16,$13,7
	addu	$12,$21			# +=X[i+9]
	xor	$18,$16
	rotr	$16,$13,18

	srl	$19,$10,10
	rotr	$17,$10,17
	xor	$18,$16			# sigma0(X[i+1])
	rotr	$16,$10,19
	xor	$19,$17
	addu	$12,$18
#else
	srl	$18,$13,3		# Xupdate(20)
	addu	$12,$21			# +=X[i+9]
	sll	$17,$13,14
	srl	$16,$13,7
	xor	$18,$17
	sll	$17,11
	xor	$18,$16
	srl	$16,$13,18
	xor	$18,$17

	srl	$19,$10,10
	xor	$18,$16			# sigma0(X[i+1])
	sll	$17,$10,13
	addu	$12,$18
	srl	$16,$10,17
	xor	$19,$17
	sll	$17,2
	xor	$19,$16
	srl	$16,$10,19
	xor	$19,$17
#endif
	xor	$19,$16			# sigma1(X[i+14])
	addu	$12,$19
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$19,$2,$3			# 20
	rotr	$17,$1,6
	addu	$16,$12,$7
	rotr	$18,$1,11
	and	$19,$1
	rotr	$7,$1,25
	xor	$17,$18
	rotr	$18,$24,2
	xor	$19,$3			# Ch(e,f,g)
	xor	$17,$7			# Sigma1(e)

	rotr	$7,$24,13
	addu	$16,$19
	lw	$19,80($6)		# K[20]
	xor	$7,$18
	rotr	$18,$24,22
	addu	$16,$17
	and	$17,$25,$30
	xor	$7,$18			# Sigma0(a)
	xor	$18,$25,$30
#else
	addu	$16,$12,$7			# 20
	srl	$7,$1,6
	xor	$19,$2,$3
	sll	$18,$1,7
	and	$19,$1
	srl	$17,$1,11
	xor	$7,$18
	sll	$18,$1,21
	xor	$7,$17
	srl	$17,$1,25
	xor	$7,$18
	sll	$18,$1,26
	xor	$7,$17
	xor	$19,$3			# Ch(e,f,g)
	xor	$17,$18,$7			# Sigma1(e)

	srl	$7,$24,2
	addu	$16,$19
	lw	$19,80($6)		# K[20]
	sll	$18,$24,10
	addu	$16,$17
	srl	$17,$24,13
	xor	$7,$18
	sll	$18,$24,19
	xor	$7,$17
	srl	$17,$24,22
	xor	$7,$18
	sll	$18,$24,30
	xor	$7,$17
	and	$17,$25,$30
	xor	$7,$18			# Sigma0(a)
	xor	$18,$25,$30
#endif
	sw	$12,16($29)	# offload to ring buffer
	addu	$7,$17
	and	$18,$24
	addu	$16,$19			# +=K[20]
	addu	$7,$18			# +=Maj(a,b,c)
	addu	$31,$16
	addu	$7,$16
	lw	$15,28($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$19,$14,3		# Xupdate(21)
	rotr	$17,$14,7
	addu	$13,$22			# +=X[i+9]
	xor	$19,$17
	rotr	$17,$14,18

	srl	$20,$11,10
	rotr	$18,$11,17
	xor	$19,$17			# sigma0(X[i+1])
	rotr	$17,$11,19
	xor	$20,$18
	addu	$13,$19
#else
	srl	$19,$14,3		# Xupdate(21)
	addu	$13,$22			# +=X[i+9]
	sll	$18,$14,14
	srl	$17,$14,7
	xor	$19,$18
	sll	$18,11
	xor	$19,$17
	srl	$17,$14,18
	xor	$19,$18

	srl	$20,$11,10
	xor	$19,$17			# sigma0(X[i+1])
	sll	$18,$11,13
	addu	$13,$19
	srl	$17,$11,17
	xor	$20,$18
	sll	$18,2
	xor	$20,$17
	srl	$17,$11,19
	xor	$20,$18
#endif
	xor	$20,$17			# sigma1(X[i+14])
	addu	$13,$20
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$20,$1,$2			# 21
	rotr	$18,$31,6
	addu	$17,$13,$3
	rotr	$19,$31,11
	and	$20,$31
	rotr	$3,$31,25
	xor	$18,$19
	rotr	$19,$7,2
	xor	$20,$2			# Ch(e,f,g)
	xor	$18,$3			# Sigma1(e)

	rotr	$3,$7,13
	addu	$17,$20
	lw	$20,84($6)		# K[21]
	xor	$3,$19
	rotr	$19,$7,22
	addu	$17,$18
	and	$18,$24,$25
	xor	$3,$19			# Sigma0(a)
	xor	$19,$24,$25
#else
	addu	$17,$13,$3			# 21
	srl	$3,$31,6
	xor	$20,$1,$2
	sll	$19,$31,7
	and	$20,$31
	srl	$18,$31,11
	xor	$3,$19
	sll	$19,$31,21
	xor	$3,$18
	srl	$18,$31,25
	xor	$3,$19
	sll	$19,$31,26
	xor	$3,$18
	xor	$20,$2			# Ch(e,f,g)
	xor	$18,$19,$3			# Sigma1(e)

	srl	$3,$7,2
	addu	$17,$20
	lw	$20,84($6)		# K[21]
	sll	$19,$7,10
	addu	$17,$18
	srl	$18,$7,13
	xor	$3,$19
	sll	$19,$7,19
	xor	$3,$18
	srl	$18,$7,22
	xor	$3,$19
	sll	$19,$7,30
	xor	$3,$18
	and	$18,$24,$25
	xor	$3,$19			# Sigma0(a)
	xor	$19,$24,$25
#endif
	sw	$13,20($29)	# offload to ring buffer
	addu	$3,$18
	and	$19,$7
	addu	$17,$20			# +=K[21]
	addu	$3,$19			# +=Maj(a,b,c)
	addu	$30,$17
	addu	$3,$17
	lw	$16,32($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$20,$15,3		# Xupdate(22)
	rotr	$18,$15,7
	addu	$14,$23			# +=X[i+9]
	xor	$20,$18
	rotr	$18,$15,18

	srl	$21,$12,10
	rotr	$19,$12,17
	xor	$20,$18			# sigma0(X[i+1])
	rotr	$18,$12,19
	xor	$21,$19
	addu	$14,$20
#else
	srl	$20,$15,3		# Xupdate(22)
	addu	$14,$23			# +=X[i+9]
	sll	$19,$15,14
	srl	$18,$15,7
	xor	$20,$19
	sll	$19,11
	xor	$20,$18
	srl	$18,$15,18
	xor	$20,$19

	srl	$21,$12,10
	xor	$20,$18			# sigma0(X[i+1])
	sll	$19,$12,13
	addu	$14,$20
	srl	$18,$12,17
	xor	$21,$19
	sll	$19,2
	xor	$21,$18
	srl	$18,$12,19
	xor	$21,$19
#endif
	xor	$21,$18			# sigma1(X[i+14])
	addu	$14,$21
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$21,$31,$1			# 22
	rotr	$19,$30,6
	addu	$18,$14,$2
	rotr	$20,$30,11
	and	$21,$30
	rotr	$2,$30,25
	xor	$19,$20
	rotr	$20,$3,2
	xor	$21,$1			# Ch(e,f,g)
	xor	$19,$2			# Sigma1(e)

	rotr	$2,$3,13
	addu	$18,$21
	lw	$21,88($6)		# K[22]
	xor	$2,$20
	rotr	$20,$3,22
	addu	$18,$19
	and	$19,$7,$24
	xor	$2,$20			# Sigma0(a)
	xor	$20,$7,$24
#else
	addu	$18,$14,$2			# 22
	srl	$2,$30,6
	xor	$21,$31,$1
	sll	$20,$30,7
	and	$21,$30
	srl	$19,$30,11
	xor	$2,$20
	sll	$20,$30,21
	xor	$2,$19
	srl	$19,$30,25
	xor	$2,$20
	sll	$20,$30,26
	xor	$2,$19
	xor	$21,$1			# Ch(e,f,g)
	xor	$19,$20,$2			# Sigma1(e)

	srl	$2,$3,2
	addu	$18,$21
	lw	$21,88($6)		# K[22]
	sll	$20,$3,10
	addu	$18,$19
	srl	$19,$3,13
	xor	$2,$20
	sll	$20,$3,19
	xor	$2,$19
	srl	$19,$3,22
	xor	$2,$20
	sll	$20,$3,30
	xor	$2,$19
	and	$19,$7,$24
	xor	$2,$20			# Sigma0(a)
	xor	$20,$7,$24
#endif
	sw	$14,24($29)	# offload to ring buffer
	addu	$2,$19
	and	$20,$3
	addu	$18,$21			# +=K[22]
	addu	$2,$20			# +=Maj(a,b,c)
	addu	$25,$18
	addu	$2,$18
	lw	$17,36($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$21,$16,3		# Xupdate(23)
	rotr	$19,$16,7
	addu	$15,$8			# +=X[i+9]
	xor	$21,$19
	rotr	$19,$16,18

	srl	$22,$13,10
	rotr	$20,$13,17
	xor	$21,$19			# sigma0(X[i+1])
	rotr	$19,$13,19
	xor	$22,$20
	addu	$15,$21
#else
	srl	$21,$16,3		# Xupdate(23)
	addu	$15,$8			# +=X[i+9]
	sll	$20,$16,14
	srl	$19,$16,7
	xor	$21,$20
	sll	$20,11
	xor	$21,$19
	srl	$19,$16,18
	xor	$21,$20

	srl	$22,$13,10
	xor	$21,$19			# sigma0(X[i+1])
	sll	$20,$13,13
	addu	$15,$21
	srl	$19,$13,17
	xor	$22,$20
	sll	$20,2
	xor	$22,$19
	srl	$19,$13,19
	xor	$22,$20
#endif
	xor	$22,$19			# sigma1(X[i+14])
	addu	$15,$22
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$22,$30,$31			# 23
	rotr	$20,$25,6
	addu	$19,$15,$1
	rotr	$21,$25,11
	and	$22,$25
	rotr	$1,$25,25
	xor	$20,$21
	rotr	$21,$2,2
	xor	$22,$31			# Ch(e,f,g)
	xor	$20,$1			# Sigma1(e)

	rotr	$1,$2,13
	addu	$19,$22
	lw	$22,92($6)		# K[23]
	xor	$1,$21
	rotr	$21,$2,22
	addu	$19,$20
	and	$20,$3,$7
	xor	$1,$21			# Sigma0(a)
	xor	$21,$3,$7
#else
	addu	$19,$15,$1			# 23
	srl	$1,$25,6
	xor	$22,$30,$31
	sll	$21,$25,7
	and	$22,$25
	srl	$20,$25,11
	xor	$1,$21
	sll	$21,$25,21
	xor	$1,$20
	srl	$20,$25,25
	xor	$1,$21
	sll	$21,$25,26
	xor	$1,$20
	xor	$22,$31			# Ch(e,f,g)
	xor	$20,$21,$1			# Sigma1(e)

	srl	$1,$2,2
	addu	$19,$22
	lw	$22,92($6)		# K[23]
	sll	$21,$2,10
	addu	$19,$20
	srl	$20,$2,13
	xor	$1,$21
	sll	$21,$2,19
	xor	$1,$20
	srl	$20,$2,22
	xor	$1,$21
	sll	$21,$2,30
	xor	$1,$20
	and	$20,$3,$7
	xor	$1,$21			# Sigma0(a)
	xor	$21,$3,$7
#endif
	sw	$15,28($29)	# offload to ring buffer
	addu	$1,$20
	and	$21,$2
	addu	$19,$22			# +=K[23]
	addu	$1,$21			# +=Maj(a,b,c)
	addu	$24,$19
	addu	$1,$19
	lw	$18,40($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$22,$17,3		# Xupdate(24)
	rotr	$20,$17,7
	addu	$16,$9			# +=X[i+9]
	xor	$22,$20
	rotr	$20,$17,18

	srl	$23,$14,10
	rotr	$21,$14,17
	xor	$22,$20			# sigma0(X[i+1])
	rotr	$20,$14,19
	xor	$23,$21
	addu	$16,$22
#else
	srl	$22,$17,3		# Xupdate(24)
	addu	$16,$9			# +=X[i+9]
	sll	$21,$17,14
	srl	$20,$17,7
	xor	$22,$21
	sll	$21,11
	xor	$22,$20
	srl	$20,$17,18
	xor	$22,$21

	srl	$23,$14,10
	xor	$22,$20			# sigma0(X[i+1])
	sll	$21,$14,13
	addu	$16,$22
	srl	$20,$14,17
	xor	$23,$21
	sll	$21,2
	xor	$23,$20
	srl	$20,$14,19
	xor	$23,$21
#endif
	xor	$23,$20			# sigma1(X[i+14])
	addu	$16,$23
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$23,$25,$30			# 24
	rotr	$21,$24,6
	addu	$20,$16,$31
	rotr	$22,$24,11
	and	$23,$24
	rotr	$31,$24,25
	xor	$21,$22
	rotr	$22,$1,2
	xor	$23,$30			# Ch(e,f,g)
	xor	$21,$31			# Sigma1(e)

	rotr	$31,$1,13
	addu	$20,$23
	lw	$23,96($6)		# K[24]
	xor	$31,$22
	rotr	$22,$1,22
	addu	$20,$21
	and	$21,$2,$3
	xor	$31,$22			# Sigma0(a)
	xor	$22,$2,$3
#else
	addu	$20,$16,$31			# 24
	srl	$31,$24,6
	xor	$23,$25,$30
	sll	$22,$24,7
	and	$23,$24
	srl	$21,$24,11
	xor	$31,$22
	sll	$22,$24,21
	xor	$31,$21
	srl	$21,$24,25
	xor	$31,$22
	sll	$22,$24,26
	xor	$31,$21
	xor	$23,$30			# Ch(e,f,g)
	xor	$21,$22,$31			# Sigma1(e)

	srl	$31,$1,2
	addu	$20,$23
	lw	$23,96($6)		# K[24]
	sll	$22,$1,10
	addu	$20,$21
	srl	$21,$1,13
	xor	$31,$22
	sll	$22,$1,19
	xor	$31,$21
	srl	$21,$1,22
	xor	$31,$22
	sll	$22,$1,30
	xor	$31,$21
	and	$21,$2,$3
	xor	$31,$22			# Sigma0(a)
	xor	$22,$2,$3
#endif
	sw	$16,32($29)	# offload to ring buffer
	addu	$31,$21
	and	$22,$1
	addu	$20,$23			# +=K[24]
	addu	$31,$22			# +=Maj(a,b,c)
	addu	$7,$20
	addu	$31,$20
	lw	$19,44($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$23,$18,3		# Xupdate(25)
	rotr	$21,$18,7
	addu	$17,$10			# +=X[i+9]
	xor	$23,$21
	rotr	$21,$18,18

	srl	$8,$15,10
	rotr	$22,$15,17
	xor	$23,$21			# sigma0(X[i+1])
	rotr	$21,$15,19
	xor	$8,$22
	addu	$17,$23
#else
	srl	$23,$18,3		# Xupdate(25)
	addu	$17,$10			# +=X[i+9]
	sll	$22,$18,14
	srl	$21,$18,7
	xor	$23,$22
	sll	$22,11
	xor	$23,$21
	srl	$21,$18,18
	xor	$23,$22

	srl	$8,$15,10
	xor	$23,$21			# sigma0(X[i+1])
	sll	$22,$15,13
	addu	$17,$23
	srl	$21,$15,17
	xor	$8,$22
	sll	$22,2
	xor	$8,$21
	srl	$21,$15,19
	xor	$8,$22
#endif
	xor	$8,$21			# sigma1(X[i+14])
	addu	$17,$8
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$8,$24,$25			# 25
	rotr	$22,$7,6
	addu	$21,$17,$30
	rotr	$23,$7,11
	and	$8,$7
	rotr	$30,$7,25
	xor	$22,$23
	rotr	$23,$31,2
	xor	$8,$25			# Ch(e,f,g)
	xor	$22,$30			# Sigma1(e)

	rotr	$30,$31,13
	addu	$21,$8
	lw	$8,100($6)		# K[25]
	xor	$30,$23
	rotr	$23,$31,22
	addu	$21,$22
	and	$22,$1,$2
	xor	$30,$23			# Sigma0(a)
	xor	$23,$1,$2
#else
	addu	$21,$17,$30			# 25
	srl	$30,$7,6
	xor	$8,$24,$25
	sll	$23,$7,7
	and	$8,$7
	srl	$22,$7,11
	xor	$30,$23
	sll	$23,$7,21
	xor	$30,$22
	srl	$22,$7,25
	xor	$30,$23
	sll	$23,$7,26
	xor	$30,$22
	xor	$8,$25			# Ch(e,f,g)
	xor	$22,$23,$30			# Sigma1(e)

	srl	$30,$31,2
	addu	$21,$8
	lw	$8,100($6)		# K[25]
	sll	$23,$31,10
	addu	$21,$22
	srl	$22,$31,13
	xor	$30,$23
	sll	$23,$31,19
	xor	$30,$22
	srl	$22,$31,22
	xor	$30,$23
	sll	$23,$31,30
	xor	$30,$22
	and	$22,$1,$2
	xor	$30,$23			# Sigma0(a)
	xor	$23,$1,$2
#endif
	sw	$17,36($29)	# offload to ring buffer
	addu	$30,$22
	and	$23,$31
	addu	$21,$8			# +=K[25]
	addu	$30,$23			# +=Maj(a,b,c)
	addu	$3,$21
	addu	$30,$21
	lw	$20,48($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$8,$19,3		# Xupdate(26)
	rotr	$22,$19,7
	addu	$18,$11			# +=X[i+9]
	xor	$8,$22
	rotr	$22,$19,18

	srl	$9,$16,10
	rotr	$23,$16,17
	xor	$8,$22			# sigma0(X[i+1])
	rotr	$22,$16,19
	xor	$9,$23
	addu	$18,$8
#else
	srl	$8,$19,3		# Xupdate(26)
	addu	$18,$11			# +=X[i+9]
	sll	$23,$19,14
	srl	$22,$19,7
	xor	$8,$23
	sll	$23,11
	xor	$8,$22
	srl	$22,$19,18
	xor	$8,$23

	srl	$9,$16,10
	xor	$8,$22			# sigma0(X[i+1])
	sll	$23,$16,13
	addu	$18,$8
	srl	$22,$16,17
	xor	$9,$23
	sll	$23,2
	xor	$9,$22
	srl	$22,$16,19
	xor	$9,$23
#endif
	xor	$9,$22			# sigma1(X[i+14])
	addu	$18,$9
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$9,$7,$24			# 26
	rotr	$23,$3,6
	addu	$22,$18,$25
	rotr	$8,$3,11
	and	$9,$3
	rotr	$25,$3,25
	xor	$23,$8
	rotr	$8,$30,2
	xor	$9,$24			# Ch(e,f,g)
	xor	$23,$25			# Sigma1(e)

	rotr	$25,$30,13
	addu	$22,$9
	lw	$9,104($6)		# K[26]
	xor	$25,$8
	rotr	$8,$30,22
	addu	$22,$23
	and	$23,$31,$1
	xor	$25,$8			# Sigma0(a)
	xor	$8,$31,$1
#else
	addu	$22,$18,$25			# 26
	srl	$25,$3,6
	xor	$9,$7,$24
	sll	$8,$3,7
	and	$9,$3
	srl	$23,$3,11
	xor	$25,$8
	sll	$8,$3,21
	xor	$25,$23
	srl	$23,$3,25
	xor	$25,$8
	sll	$8,$3,26
	xor	$25,$23
	xor	$9,$24			# Ch(e,f,g)
	xor	$23,$8,$25			# Sigma1(e)

	srl	$25,$30,2
	addu	$22,$9
	lw	$9,104($6)		# K[26]
	sll	$8,$30,10
	addu	$22,$23
	srl	$23,$30,13
	xor	$25,$8
	sll	$8,$30,19
	xor	$25,$23
	srl	$23,$30,22
	xor	$25,$8
	sll	$8,$30,30
	xor	$25,$23
	and	$23,$31,$1
	xor	$25,$8			# Sigma0(a)
	xor	$8,$31,$1
#endif
	sw	$18,40($29)	# offload to ring buffer
	addu	$25,$23
	and	$8,$30
	addu	$22,$9			# +=K[26]
	addu	$25,$8			# +=Maj(a,b,c)
	addu	$2,$22
	addu	$25,$22
	lw	$21,52($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$9,$20,3		# Xupdate(27)
	rotr	$23,$20,7
	addu	$19,$12			# +=X[i+9]
	xor	$9,$23
	rotr	$23,$20,18

	srl	$10,$17,10
	rotr	$8,$17,17
	xor	$9,$23			# sigma0(X[i+1])
	rotr	$23,$17,19
	xor	$10,$8
	addu	$19,$9
#else
	srl	$9,$20,3		# Xupdate(27)
	addu	$19,$12			# +=X[i+9]
	sll	$8,$20,14
	srl	$23,$20,7
	xor	$9,$8
	sll	$8,11
	xor	$9,$23
	srl	$23,$20,18
	xor	$9,$8

	srl	$10,$17,10
	xor	$9,$23			# sigma0(X[i+1])
	sll	$8,$17,13
	addu	$19,$9
	srl	$23,$17,17
	xor	$10,$8
	sll	$8,2
	xor	$10,$23
	srl	$23,$17,19
	xor	$10,$8
#endif
	xor	$10,$23			# sigma1(X[i+14])
	addu	$19,$10
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$10,$3,$7			# 27
	rotr	$8,$2,6
	addu	$23,$19,$24
	rotr	$9,$2,11
	and	$10,$2
	rotr	$24,$2,25
	xor	$8,$9
	rotr	$9,$25,2
	xor	$10,$7			# Ch(e,f,g)
	xor	$8,$24			# Sigma1(e)

	rotr	$24,$25,13
	addu	$23,$10
	lw	$10,108($6)		# K[27]
	xor	$24,$9
	rotr	$9,$25,22
	addu	$23,$8
	and	$8,$30,$31
	xor	$24,$9			# Sigma0(a)
	xor	$9,$30,$31
#else
	addu	$23,$19,$24			# 27
	srl	$24,$2,6
	xor	$10,$3,$7
	sll	$9,$2,7
	and	$10,$2
	srl	$8,$2,11
	xor	$24,$9
	sll	$9,$2,21
	xor	$24,$8
	srl	$8,$2,25
	xor	$24,$9
	sll	$9,$2,26
	xor	$24,$8
	xor	$10,$7			# Ch(e,f,g)
	xor	$8,$9,$24			# Sigma1(e)

	srl	$24,$25,2
	addu	$23,$10
	lw	$10,108($6)		# K[27]
	sll	$9,$25,10
	addu	$23,$8
	srl	$8,$25,13
	xor	$24,$9
	sll	$9,$25,19
	xor	$24,$8
	srl	$8,$25,22
	xor	$24,$9
	sll	$9,$25,30
	xor	$24,$8
	and	$8,$30,$31
	xor	$24,$9			# Sigma0(a)
	xor	$9,$30,$31
#endif
	sw	$19,44($29)	# offload to ring buffer
	addu	$24,$8
	and	$9,$25
	addu	$23,$10			# +=K[27]
	addu	$24,$9			# +=Maj(a,b,c)
	addu	$1,$23
	addu	$24,$23
	lw	$22,56($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$10,$21,3		# Xupdate(28)
	rotr	$8,$21,7
	addu	$20,$13			# +=X[i+9]
	xor	$10,$8
	rotr	$8,$21,18

	srl	$11,$18,10
	rotr	$9,$18,17
	xor	$10,$8			# sigma0(X[i+1])
	rotr	$8,$18,19
	xor	$11,$9
	addu	$20,$10
#else
	srl	$10,$21,3		# Xupdate(28)
	addu	$20,$13			# +=X[i+9]
	sll	$9,$21,14
	srl	$8,$21,7
	xor	$10,$9
	sll	$9,11
	xor	$10,$8
	srl	$8,$21,18
	xor	$10,$9

	srl	$11,$18,10
	xor	$10,$8			# sigma0(X[i+1])
	sll	$9,$18,13
	addu	$20,$10
	srl	$8,$18,17
	xor	$11,$9
	sll	$9,2
	xor	$11,$8
	srl	$8,$18,19
	xor	$11,$9
#endif
	xor	$11,$8			# sigma1(X[i+14])
	addu	$20,$11
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$11,$2,$3			# 28
	rotr	$9,$1,6
	addu	$8,$20,$7
	rotr	$10,$1,11
	and	$11,$1
	rotr	$7,$1,25
	xor	$9,$10
	rotr	$10,$24,2
	xor	$11,$3			# Ch(e,f,g)
	xor	$9,$7			# Sigma1(e)

	rotr	$7,$24,13
	addu	$8,$11
	lw	$11,112($6)		# K[28]
	xor	$7,$10
	rotr	$10,$24,22
	addu	$8,$9
	and	$9,$25,$30
	xor	$7,$10			# Sigma0(a)
	xor	$10,$25,$30
#else
	addu	$8,$20,$7			# 28
	srl	$7,$1,6
	xor	$11,$2,$3
	sll	$10,$1,7
	and	$11,$1
	srl	$9,$1,11
	xor	$7,$10
	sll	$10,$1,21
	xor	$7,$9
	srl	$9,$1,25
	xor	$7,$10
	sll	$10,$1,26
	xor	$7,$9
	xor	$11,$3			# Ch(e,f,g)
	xor	$9,$10,$7			# Sigma1(e)

	srl	$7,$24,2
	addu	$8,$11
	lw	$11,112($6)		# K[28]
	sll	$10,$24,10
	addu	$8,$9
	srl	$9,$24,13
	xor	$7,$10
	sll	$10,$24,19
	xor	$7,$9
	srl	$9,$24,22
	xor	$7,$10
	sll	$10,$24,30
	xor	$7,$9
	and	$9,$25,$30
	xor	$7,$10			# Sigma0(a)
	xor	$10,$25,$30
#endif
	sw	$20,48($29)	# offload to ring buffer
	addu	$7,$9
	and	$10,$24
	addu	$8,$11			# +=K[28]
	addu	$7,$10			# +=Maj(a,b,c)
	addu	$31,$8
	addu	$7,$8
	lw	$23,60($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$11,$22,3		# Xupdate(29)
	rotr	$9,$22,7
	addu	$21,$14			# +=X[i+9]
	xor	$11,$9
	rotr	$9,$22,18

	srl	$12,$19,10
	rotr	$10,$19,17
	xor	$11,$9			# sigma0(X[i+1])
	rotr	$9,$19,19
	xor	$12,$10
	addu	$21,$11
#else
	srl	$11,$22,3		# Xupdate(29)
	addu	$21,$14			# +=X[i+9]
	sll	$10,$22,14
	srl	$9,$22,7
	xor	$11,$10
	sll	$10,11
	xor	$11,$9
	srl	$9,$22,18
	xor	$11,$10

	srl	$12,$19,10
	xor	$11,$9			# sigma0(X[i+1])
	sll	$10,$19,13
	addu	$21,$11
	srl	$9,$19,17
	xor	$12,$10
	sll	$10,2
	xor	$12,$9
	srl	$9,$19,19
	xor	$12,$10
#endif
	xor	$12,$9			# sigma1(X[i+14])
	addu	$21,$12
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$12,$1,$2			# 29
	rotr	$10,$31,6
	addu	$9,$21,$3
	rotr	$11,$31,11
	and	$12,$31
	rotr	$3,$31,25
	xor	$10,$11
	rotr	$11,$7,2
	xor	$12,$2			# Ch(e,f,g)
	xor	$10,$3			# Sigma1(e)

	rotr	$3,$7,13
	addu	$9,$12
	lw	$12,116($6)		# K[29]
	xor	$3,$11
	rotr	$11,$7,22
	addu	$9,$10
	and	$10,$24,$25
	xor	$3,$11			# Sigma0(a)
	xor	$11,$24,$25
#else
	addu	$9,$21,$3			# 29
	srl	$3,$31,6
	xor	$12,$1,$2
	sll	$11,$31,7
	and	$12,$31
	srl	$10,$31,11
	xor	$3,$11
	sll	$11,$31,21
	xor	$3,$10
	srl	$10,$31,25
	xor	$3,$11
	sll	$11,$31,26
	xor	$3,$10
	xor	$12,$2			# Ch(e,f,g)
	xor	$10,$11,$3			# Sigma1(e)

	srl	$3,$7,2
	addu	$9,$12
	lw	$12,116($6)		# K[29]
	sll	$11,$7,10
	addu	$9,$10
	srl	$10,$7,13
	xor	$3,$11
	sll	$11,$7,19
	xor	$3,$10
	srl	$10,$7,22
	xor	$3,$11
	sll	$11,$7,30
	xor	$3,$10
	and	$10,$24,$25
	xor	$3,$11			# Sigma0(a)
	xor	$11,$24,$25
#endif
	sw	$21,52($29)	# offload to ring buffer
	addu	$3,$10
	and	$11,$7
	addu	$9,$12			# +=K[29]
	addu	$3,$11			# +=Maj(a,b,c)
	addu	$30,$9
	addu	$3,$9
	lw	$8,0($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$12,$23,3		# Xupdate(30)
	rotr	$10,$23,7
	addu	$22,$15			# +=X[i+9]
	xor	$12,$10
	rotr	$10,$23,18

	srl	$13,$20,10
	rotr	$11,$20,17
	xor	$12,$10			# sigma0(X[i+1])
	rotr	$10,$20,19
	xor	$13,$11
	addu	$22,$12
#else
	srl	$12,$23,3		# Xupdate(30)
	addu	$22,$15			# +=X[i+9]
	sll	$11,$23,14
	srl	$10,$23,7
	xor	$12,$11
	sll	$11,11
	xor	$12,$10
	srl	$10,$23,18
	xor	$12,$11

	srl	$13,$20,10
	xor	$12,$10			# sigma0(X[i+1])
	sll	$11,$20,13
	addu	$22,$12
	srl	$10,$20,17
	xor	$13,$11
	sll	$11,2
	xor	$13,$10
	srl	$10,$20,19
	xor	$13,$11
#endif
	xor	$13,$10			# sigma1(X[i+14])
	addu	$22,$13
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$13,$31,$1			# 30
	rotr	$11,$30,6
	addu	$10,$22,$2
	rotr	$12,$30,11
	and	$13,$30
	rotr	$2,$30,25
	xor	$11,$12
	rotr	$12,$3,2
	xor	$13,$1			# Ch(e,f,g)
	xor	$11,$2			# Sigma1(e)

	rotr	$2,$3,13
	addu	$10,$13
	lw	$13,120($6)		# K[30]
	xor	$2,$12
	rotr	$12,$3,22
	addu	$10,$11
	and	$11,$7,$24
	xor	$2,$12			# Sigma0(a)
	xor	$12,$7,$24
#else
	addu	$10,$22,$2			# 30
	srl	$2,$30,6
	xor	$13,$31,$1
	sll	$12,$30,7
	and	$13,$30
	srl	$11,$30,11
	xor	$2,$12
	sll	$12,$30,21
	xor	$2,$11
	srl	$11,$30,25
	xor	$2,$12
	sll	$12,$30,26
	xor	$2,$11
	xor	$13,$1			# Ch(e,f,g)
	xor	$11,$12,$2			# Sigma1(e)

	srl	$2,$3,2
	addu	$10,$13
	lw	$13,120($6)		# K[30]
	sll	$12,$3,10
	addu	$10,$11
	srl	$11,$3,13
	xor	$2,$12
	sll	$12,$3,19
	xor	$2,$11
	srl	$11,$3,22
	xor	$2,$12
	sll	$12,$3,30
	xor	$2,$11
	and	$11,$7,$24
	xor	$2,$12			# Sigma0(a)
	xor	$12,$7,$24
#endif
	sw	$22,56($29)	# offload to ring buffer
	addu	$2,$11
	and	$12,$3
	addu	$10,$13			# +=K[30]
	addu	$2,$12			# +=Maj(a,b,c)
	addu	$25,$10
	addu	$2,$10
	lw	$9,4($29)	# prefetch from ring buffer
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	srl	$13,$8,3		# Xupdate(31)
	rotr	$11,$8,7
	addu	$23,$16			# +=X[i+9]
	xor	$13,$11
	rotr	$11,$8,18

	srl	$14,$21,10
	rotr	$12,$21,17
	xor	$13,$11			# sigma0(X[i+1])
	rotr	$11,$21,19
	xor	$14,$12
	addu	$23,$13
#else
	srl	$13,$8,3		# Xupdate(31)
	addu	$23,$16			# +=X[i+9]
	sll	$12,$8,14
	srl	$11,$8,7
	xor	$13,$12
	sll	$12,11
	xor	$13,$11
	srl	$11,$8,18
	xor	$13,$12

	srl	$14,$21,10
	xor	$13,$11			# sigma0(X[i+1])
	sll	$12,$21,13
	addu	$23,$13
	srl	$11,$21,17
	xor	$14,$12
	sll	$12,2
	xor	$14,$11
	srl	$11,$21,19
	xor	$14,$12
#endif
	xor	$14,$11			# sigma1(X[i+14])
	addu	$23,$14
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS64R2)
	xor	$14,$30,$31			# 31
	rotr	$12,$25,6
	addu	$11,$23,$1
	rotr	$13,$25,11
	and	$14,$25
	rotr	$1,$25,25
	xor	$12,$13
	rotr	$13,$2,2
	xor	$14,$31			# Ch(e,f,g)
	xor	$12,$1			# Sigma1(e)

	rotr	$1,$2,13
	addu	$11,$14
	lw	$14,124($6)		# K[31]
	xor	$1,$13
	rotr	$13,$2,22
	addu	$11,$12
	and	$12,$3,$7
	xor	$1,$13			# Sigma0(a)
	xor	$13,$3,$7
#else
	addu	$11,$23,$1			# 31
	srl	$1,$25,6
	xor	$14,$30,$31
	sll	$13,$25,7
	and	$14,$25
	srl	$12,$25,11
	xor	$1,$13
	sll	$13,$25,21
	xor	$1,$12
	srl	$12,$25,25
	xor	$1,$13
	sll	$13,$25,26
	xor	$1,$12
	xor	$14,$31			# Ch(e,f,g)
	xor	$12,$13,$1			# Sigma1(e)

	srl	$1,$2,2
	addu	$11,$14
	lw	$14,124($6)		# K[31]
	sll	$13,$2,10
	addu	$11,$12
	srl	$12,$2,13
	xor	$1,$13
	sll	$13,$2,19
	xor	$1,$12
	srl	$12,$2,22
	xor	$1,$13
	sll	$13,$2,30
	xor	$1,$12
	and	$12,$3,$7
	xor	$1,$13			# Sigma0(a)
	xor	$13,$3,$7
#endif
	sw	$23,60($29)	# offload to ring buffer
	addu	$1,$12
	and	$13,$2
	addu	$11,$14			# +=K[31]
	addu	$1,$13			# +=Maj(a,b,c)
	addu	$24,$11
	addu	$1,$11
	lw	$10,8($29)	# prefetch from ring buffer
	and	$14,0xfff
	li	$15,2290
	.set	noreorder
	bne	$14,$15,.L16_xx
	daddu $6,16*4		# Ktbl+=16

	ld	$23,16*4($29)	# restore pointer to the end of input
	lw	$8,0*4($4)
	lw	$9,1*4($4)
	lw	$10,2*4($4)
	daddu $5,16*4
	lw	$11,3*4($4)
	addu	$1,$8
	lw	$12,4*4($4)
	addu	$2,$9
	lw	$13,5*4($4)
	addu	$3,$10
	lw	$14,6*4($4)
	addu	$7,$11
	lw	$15,7*4($4)
	addu	$24,$12
	sw	$1,0*4($4)
	addu	$25,$13
	sw	$2,1*4($4)
	addu	$30,$14
	sw	$3,2*4($4)
	addu	$31,$15
	sw	$7,3*4($4)
	sw	$24,4*4($4)
	sw	$25,5*4($4)
	sw	$30,6*4($4)
	sw	$31,7*4($4)

	bne	$5,$23,.Loop
	dsubu $6,192	# rewind $6

	ld	$31,192-1*8($29)
	ld	$30,192-2*8($29)
	ld	$23,192-3*8($29)
	ld	$22,192-4*8($29)
	ld	$21,192-5*8($29)
	ld	$20,192-6*8($29)
	ld	$19,192-7*8($29)
	ld	$18,192-8*8($29)
	ld	$17,192-9*8($29)
	ld	$16,192-10*8($29)
	jr	$31
	daddu $29,192
.end	sha256_block_data_order

.rdata
.align	5
K256:
	.word	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
	.word	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
	.word	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
	.word	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
	.word	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
	.word	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
	.word	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
	.word	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
	.word	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
	.word	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
	.word	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
	.word	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
	.word	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
	.word	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
	.word	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
	.word	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
.asciiz	"SHA256 for MIPS, CRYPTOGAMS by <appro@openssl.org>"
.align	5

