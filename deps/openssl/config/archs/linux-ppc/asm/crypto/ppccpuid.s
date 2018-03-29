.machine	"any"
.text

.globl	OPENSSL_fpu_probe
.type	OPENSSL_fpu_probe,@function
.align	4
OPENSSL_fpu_probe:
	fmr	0,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_fpu_probe,.-OPENSSL_fpu_probe
.globl	OPENSSL_ppc64_probe
.type	OPENSSL_ppc64_probe,@function
.align	4
OPENSSL_ppc64_probe:
	fcfid	1,1
	rldicl	0,0,32,32
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_ppc64_probe,.-OPENSSL_ppc64_probe

.globl	OPENSSL_altivec_probe
.type	OPENSSL_altivec_probe,@function
.align	4
OPENSSL_altivec_probe:
.long	0x10000484
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_altivec_probe,.-OPENSSL_altivec_probe

.globl	OPENSSL_crypto207_probe
.type	OPENSSL_crypto207_probe,@function
.align	4
OPENSSL_crypto207_probe:
	.long	0x7C000E99
	.long	0x10000508
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_crypto207_probe,.-OPENSSL_crypto207_probe

.globl	OPENSSL_madd300_probe
.type	OPENSSL_madd300_probe,@function
.align	4
OPENSSL_madd300_probe:
	xor	0,0,0
	.long	0x10600033
	.long	0x10600031
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,@function
.align	4
OPENSSL_wipe_cpu:
	xor	0,0,0
	fmr	0,31
	fmr	1,31
	fmr	2,31
	mr	3,1
	fmr	3,31
	xor	4,4,4
	fmr	4,31
	xor	5,5,5
	fmr	5,31
	xor	6,6,6
	fmr	6,31
	xor	7,7,7
	fmr	7,31
	xor	8,8,8
	fmr	8,31
	xor	9,9,9
	fmr	9,31
	xor	10,10,10
	fmr	10,31
	xor	11,11,11
	fmr	11,31
	xor	12,12,12
	fmr	12,31
	fmr	13,31
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu

.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,@function
.align	4
OPENSSL_atomic_add:
.Ladd:	lwarx	5,0,3
	add	0,4,5
	stwcx.	0,0,3
	bne-	.Ladd
	mr	3,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.align	4
OPENSSL_rdtsc:
.Loop_rdtsc:
	mftbu	5
	mftb	3
	mftbu	4
	.long	0x7c042840
	bne	.Loop_rdtsc
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	OPENSSL_rdtsc,.-OPENSSL_rdtsc

.globl	OPENSSL_cleanse
.type	OPENSSL_cleanse,@function
.align	4
OPENSSL_cleanse:
	cmplwi	4,7
	li	0,0
	bge	.Lot
	cmplwi	4,0
	.long	0x4DC20020
.Little:	mtctr	4
	stb	0,0(3)
	addi	3,3,1
	bdnz	$-8
	blr	
.Lot:	andi.	5,3,3
	beq	.Laligned
	stb	0,0(3)
	subi	4,4,1
	addi	3,3,1
	b	.Lot
.Laligned:
	srwi	5,4,2
	mtctr	5
	stw	0,0(3)
	addi	3,3,4
	bdnz	$-8
	andi.	4,4,3
	bne	.Little
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	OPENSSL_cleanse,.-OPENSSL_cleanse

.globl	CRYPTO_memcmp
.type	CRYPTO_memcmp,@function
.align	4
CRYPTO_memcmp:
	cmplwi	5,0
	li	0,0
	beq	.Lno_data
	mtctr	5
.Loop_cmp:
	lbz	6,0(3)
	addi	3,3,1
	lbz	7,0(4)
	addi	4,4,1
	xor	6,6,7
	or	0,0,6
	bdnz	.Loop_cmp

.Lno_data:
	li	3,0
	sub	3,3,0
	extrwi	3,3,1,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	CRYPTO_memcmp,.-CRYPTO_memcmp
.globl	OPENSSL_instrument_bus
.type	OPENSSL_instrument_bus,@function
.align	4
OPENSSL_instrument_bus:
	mtctr	4

	mftb	7
	li	8,0

	dcbf	0,3
	lwarx	6,0,3
	add	6,6,8
	stwcx.	6,0,3
	stwx	6,0,3

.Loop:	mftb	6
	sub	8,6,7
	mr	7,6
	dcbf	0,3
	lwarx	6,0,3
	add	6,6,8
	stwcx.	6,0,3
	stwx	6,0,3
	addi	3,3,4
	bdnz	.Loop

	mr	3,4
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	OPENSSL_instrument_bus,.-OPENSSL_instrument_bus

.globl	OPENSSL_instrument_bus2
.type	OPENSSL_instrument_bus2,@function
.align	4
OPENSSL_instrument_bus2:
	mr	0,4
	slwi	4,4,2

	mftb	7
	li	8,0

	dcbf	0,3
	lwarx	6,0,3
	add	6,6,8
	stwcx.	6,0,3
	stwx	6,0,3

	mftb	6
	sub	8,6,7
	mr	7,6
	mr	9,8
.Loop2:
	dcbf	0,3
	lwarx	6,0,3
	add	6,6,8
	stwcx.	6,0,3
	stwx	6,0,3

	addic.	5,5,-1
	beq	.Ldone2

	mftb	6
	sub	8,6,7
	mr	7,6
	.long	0x7f884840
	mr	9,8

	mfcr	6
	not	6,6
	rlwinm	6,6,1,29,29

	sub.	4,4,6
	add	3,3,6
	bne	.Loop2

.Ldone2:
	srwi	4,4,2
	sub	3,0,4
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	OPENSSL_instrument_bus2,.-OPENSSL_instrument_bus2
