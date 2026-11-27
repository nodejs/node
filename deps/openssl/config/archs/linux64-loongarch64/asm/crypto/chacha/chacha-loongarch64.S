#include "arch/loongarch_arch.h"

.text

.extern OPENSSL_loongarch_hwcap_P

.align 6
.Lsigma:
.ascii	"expand 32-byte k"
.Linc8x:
.long	0,1,2,3,4,5,6,7
.Linc4x:
.long	0,1,2,3

.globl	ChaCha20_ctr32
.type	ChaCha20_ctr32 function

.align 6
ChaCha20_ctr32:
	# $a0 = arg #1 (out pointer)
	# $a1 = arg #2 (inp pointer)
	# $a2 = arg #3 (len)
	# $a3 = arg #4 (key array)
	# $a4 = arg #5 (counter array)

	beqz		$a2,.Lno_data
	ori			$t3,$zero,64
	la.global	$t0,OPENSSL_loongarch_hwcap_P
	ld.w		$t0,$t0,0

	bleu		$a2,$t3,.LChaCha20_1x  # goto 1x when len <= 64

	andi		$t0,$t0,LOONGARCH_HWCAP_LASX | LOONGARCH_HWCAP_LSX
	beqz		$t0,.LChaCha20_1x

	addi.d		$sp,$sp,-64
	fst.d		$f24,$sp,0
	fst.d		$f25,$sp,8
	fst.d		$f26,$sp,16
	fst.d		$f27,$sp,24
	fst.d		$f28,$sp,32
	fst.d		$f29,$sp,40
	fst.d		$f30,$sp,48
	fst.d		$f31,$sp,56

	andi		$t1,$t0,LOONGARCH_HWCAP_LASX
	bnez		$t1,.LChaCha20_8x

	b		.LChaCha20_4x

.align 6
.LChaCha20_1x:
	addi.d		$sp,$sp,-256
	st.d		$s0,$sp,0
	st.d		$s1,$sp,8
	st.d		$s2,$sp,16
	st.d		$s3,$sp,24
	st.d		$s4,$sp,32
	st.d		$s5,$sp,40
	st.d		$s6,$sp,48
	st.d		$s7,$sp,56
	st.d		$s8,$sp,64

	# Save the initial block counter in $s8
	ld.w		$s8,$a4,0
	b			.Loop_outer_1x

.align 5
.Loop_outer_1x:
	# Load constants
	la.local	$t8,.Lsigma
	ld.w		$t0,$t8,0		  # 'expa'
	ld.w		$t1,$t8,4		  # 'nd 3'
	ld.w		$t2,$t8,8		  # '2-by'
	ld.w		$t3,$t8,12	  # 'te k'

	# Load key
	ld.w		$t4,$a3,4*0
	ld.w		$t5,$a3,4*1
	ld.w		$t6,$a3,4*2
	ld.w		$t7,$a3,4*3
	ld.w		$s0,$a3,4*4
	ld.w		$s1,$a3,4*5
	ld.w		$s2,$a3,4*6
	ld.w		$s3,$a3,4*7

	# Load block counter
	move		$s4,$s8

	# Load nonce
	ld.w		$s5,$a4,4*1
	ld.w		$s6,$a4,4*2
	ld.w		$s7,$a4,4*3

	# Update states in @x[*] for 20 rounds
	ori			$t8,$zero,10
	b			.Loop_1x

.align 5
.Loop_1x:
	add.w		$t0,$t0,$t4
	xor			$s4,$s4,$t0
	rotri.w		$s4,$s4,16      # rotate left 16 bits
	add.w		$t1,$t1,$t5
	xor			$s5,$s5,$t1
	rotri.w		$s5,$s5,16

	add.w		$s0,$s0,$s4
	xor			$t4,$t4,$s0
	rotri.w		$t4,$t4,20      # rotate left 12 bits
	add.w		$s1,$s1,$s5
	xor			$t5,$t5,$s1
	rotri.w		$t5,$t5,20

	add.w		$t0,$t0,$t4
	xor			$s4,$s4,$t0
	rotri.w		$s4,$s4,24      # rotate left 8 bits
	add.w		$t1,$t1,$t5
	xor			$s5,$s5,$t1
	rotri.w		$s5,$s5,24

	add.w		$s0,$s0,$s4
	xor			$t4,$t4,$s0
	rotri.w		$t4,$t4,25      # rotate left 7 bits
	add.w		$s1,$s1,$s5
	xor			$t5,$t5,$s1
	rotri.w		$t5,$t5,25

	add.w		$t2,$t2,$t6
	xor			$s6,$s6,$t2
	rotri.w		$s6,$s6,16
	add.w		$t3,$t3,$t7
	xor			$s7,$s7,$t3
	rotri.w		$s7,$s7,16

	add.w		$s2,$s2,$s6
	xor			$t6,$t6,$s2
	rotri.w		$t6,$t6,20
	add.w		$s3,$s3,$s7
	xor			$t7,$t7,$s3
	rotri.w		$t7,$t7,20

	add.w		$t2,$t2,$t6
	xor			$s6,$s6,$t2
	rotri.w		$s6,$s6,24
	add.w		$t3,$t3,$t7
	xor			$s7,$s7,$t3
	rotri.w		$s7,$s7,24

	add.w		$s2,$s2,$s6
	xor			$t6,$t6,$s2
	rotri.w		$t6,$t6,25
	add.w		$s3,$s3,$s7
	xor			$t7,$t7,$s3
	rotri.w		$t7,$t7,25

	add.w		$t0,$t0,$t5
	xor			$s7,$s7,$t0
	rotri.w		$s7,$s7,16      # rotate left 16 bits
	add.w		$t1,$t1,$t6
	xor			$s4,$s4,$t1
	rotri.w		$s4,$s4,16

	add.w		$s2,$s2,$s7
	xor			$t5,$t5,$s2
	rotri.w		$t5,$t5,20      # rotate left 12 bits
	add.w		$s3,$s3,$s4
	xor			$t6,$t6,$s3
	rotri.w		$t6,$t6,20

	add.w		$t0,$t0,$t5
	xor			$s7,$s7,$t0
	rotri.w		$s7,$s7,24      # rotate left 8 bits
	add.w		$t1,$t1,$t6
	xor			$s4,$s4,$t1
	rotri.w		$s4,$s4,24

	add.w		$s2,$s2,$s7
	xor			$t5,$t5,$s2
	rotri.w		$t5,$t5,25      # rotate left 7 bits
	add.w		$s3,$s3,$s4
	xor			$t6,$t6,$s3
	rotri.w		$t6,$t6,25

	add.w		$t2,$t2,$t7
	xor			$s5,$s5,$t2
	rotri.w		$s5,$s5,16
	add.w		$t3,$t3,$t4
	xor			$s6,$s6,$t3
	rotri.w		$s6,$s6,16

	add.w		$s0,$s0,$s5
	xor			$t7,$t7,$s0
	rotri.w		$t7,$t7,20
	add.w		$s1,$s1,$s6
	xor			$t4,$t4,$s1
	rotri.w		$t4,$t4,20

	add.w		$t2,$t2,$t7
	xor			$s5,$s5,$t2
	rotri.w		$s5,$s5,24
	add.w		$t3,$t3,$t4
	xor			$s6,$s6,$t3
	rotri.w		$s6,$s6,24

	add.w		$s0,$s0,$s5
	xor			$t7,$t7,$s0
	rotri.w		$t7,$t7,25
	add.w		$s1,$s1,$s6
	xor			$t4,$t4,$s1
	rotri.w		$t4,$t4,25

	addi.w		$t8,$t8,-1
	bnez		$t8,.Loop_1x

	# Get the final states by adding the initial states
	la.local	$t8,.Lsigma
	ld.w		$a7,$t8,4*0
	ld.w		$a6,$t8,4*1
	ld.w		$a5,$t8,4*2
	add.w		$t0,$t0,$a7
	add.w		$t1,$t1,$a6
	add.w		$t2,$t2,$a5
	ld.w		$a7,$t8,4*3
	add.w		$t3,$t3,$a7

	ld.w		$t8,$a3,4*0
	ld.w		$a7,$a3,4*1
	ld.w		$a6,$a3,4*2
	ld.w		$a5,$a3,4*3
	add.w		$t4,$t4,$t8
	add.w		$t5,$t5,$a7
	add.w		$t6,$t6,$a6
	add.w		$t7,$t7,$a5

	ld.w		$t8,$a3,4*4
	ld.w		$a7,$a3,4*5
	ld.w		$a6,$a3,4*6
	ld.w		$a5,$a3,4*7
	add.w		$s0,$s0,$t8
	add.w		$s1,$s1,$a7
	add.w		$s2,$s2,$a6
	add.w		$s3,$s3,$a5

	add.w		$s4,$s4,$s8

	ld.w		$t8,$a4,4*1
	ld.w		$a7,$a4,4*2
	ld.w		$a6,$a4,4*3
	add.w		$s5,$s5,$t8
	add.w		$s6,$s6,$a7
	add.w		$s7,$s7,$a6

	ori			$t8,$zero,64
	bltu		$a2,$t8,.Ltail_1x

	# Get the encrypted message by xor states with plaintext
	ld.w		$t8,$a1,4*0
	ld.w		$a7,$a1,4*1
	ld.w		$a6,$a1,4*2
	ld.w		$a5,$a1,4*3
	xor			$t8,$t8,$t0
	xor			$a7,$a7,$t1
	xor			$a6,$a6,$t2
	xor			$a5,$a5,$t3
	st.w		$t8,$a0,4*0
	st.w		$a7,$a0,4*1
	st.w		$a6,$a0,4*2
	st.w		$a5,$a0,4*3

	ld.w		$t8,$a1,4*4
	ld.w		$a7,$a1,4*5
	ld.w		$a6,$a1,4*6
	ld.w		$a5,$a1,4*7
	xor			$t8,$t8,$t4
	xor			$a7,$a7,$t5
	xor			$a6,$a6,$t6
	xor			$a5,$a5,$t7
	st.w		$t8,$a0,4*4
	st.w		$a7,$a0,4*5
	st.w		$a6,$a0,4*6
	st.w		$a5,$a0,4*7

	ld.w		$t8,$a1,4*8
	ld.w		$a7,$a1,4*9
	ld.w		$a6,$a1,4*10
	ld.w		$a5,$a1,4*11
	xor			$t8,$t8,$s0
	xor			$a7,$a7,$s1
	xor			$a6,$a6,$s2
	xor			$a5,$a5,$s3
	st.w		$t8,$a0,4*8
	st.w		$a7,$a0,4*9
	st.w		$a6,$a0,4*10
	st.w		$a5,$a0,4*11

	ld.w		$t8,$a1,4*12
	ld.w		$a7,$a1,4*13
	ld.w		$a6,$a1,4*14
	ld.w		$a5,$a1,4*15
	xor			$t8,$t8,$s4
	xor			$a7,$a7,$s5
	xor			$a6,$a6,$s6
	xor			$a5,$a5,$s7
	st.w		$t8,$a0,4*12
	st.w		$a7,$a0,4*13
	st.w		$a6,$a0,4*14
	st.w		$a5,$a0,4*15

	addi.d		$a2,$a2,-64
	beqz		$a2,.Ldone_1x
	addi.d		$a1,$a1,64
	addi.d		$a0,$a0,64
	addi.w		$s8,$s8,1
	b			.Loop_outer_1x

.align 4
.Ltail_1x:
	# Handle the tail for 1x (1 <= tail_len <= 63)
	addi.d		$a7,$sp,72
	st.w		$t0,$a7,4*0
	st.w		$t1,$a7,4*1
	st.w		$t2,$a7,4*2
	st.w		$t3,$a7,4*3
	st.w		$t4,$a7,4*4
	st.w		$t5,$a7,4*5
	st.w		$t6,$a7,4*6
	st.w		$t7,$a7,4*7
	st.w		$s0,$a7,4*8
	st.w		$s1,$a7,4*9
	st.w		$s2,$a7,4*10
	st.w		$s3,$a7,4*11
	st.w		$s4,$a7,4*12
	st.w		$s5,$a7,4*13
	st.w		$s6,$a7,4*14
	st.w		$s7,$a7,4*15

	move		$t8,$zero

.Loop_tail_1x:
	# Xor input with states byte by byte
	ldx.bu		$a6,$a1,$t8
	ldx.bu		$a5,$a7,$t8
	xor			$a6,$a6,$a5
	stx.b		$a6,$a0,$t8
	addi.w		$t8,$t8,1
	addi.d		$a2,$a2,-1
	bnez		$a2,.Loop_tail_1x
	b			.Ldone_1x

.Ldone_1x:
	ld.d		$s0,$sp,0
	ld.d		$s1,$sp,8
	ld.d		$s2,$sp,16
	ld.d		$s3,$sp,24
	ld.d		$s4,$sp,32
	ld.d		$s5,$sp,40
	ld.d		$s6,$sp,48
	ld.d		$s7,$sp,56
	ld.d		$s8,$sp,64
	addi.d		$sp,$sp,256

	b			.Lend

.align 6
.LChaCha20_4x:
	addi.d		$sp,$sp,-128

	# Save the initial block counter in $t4
	ld.w		$t4,$a4,0
	b			.Loop_outer_4x

.align 5
.Loop_outer_4x:
	# Load constant
	la.local		$t8,.Lsigma
	vldrepl.w		$vr0,$t8,4*0		# 'expa'
	vldrepl.w		$vr1,$t8,4*1		# 'nd 3'
	vldrepl.w		$vr2,$t8,4*2		# '2-by'
	vldrepl.w		$vr3,$t8,4*3		# 'te k'

	# Load key
	vldrepl.w		$vr4,$a3,4*0
	vldrepl.w		$vr5,$a3,4*1
	vldrepl.w		$vr6,$a3,4*2
	vldrepl.w		$vr7,$a3,4*3
	vldrepl.w		$vr8,$a3,4*4
	vldrepl.w		$vr9,$a3,4*5
	vldrepl.w		$vr10,$a3,4*6
	vldrepl.w		$vr11,$a3,4*7

	# Load block counter
	vreplgr2vr.w	$vr12,$t4

	# Load nonce
	vldrepl.w		$vr13,$a4,4*1
	vldrepl.w		$vr14,$a4,4*2
	vldrepl.w		$vr15,$a4,4*3

	# Get the correct block counter for each block
	la.local		$t8,.Linc4x
	vld				$vr16,$t8,0
	vadd.w			$vr12,$vr12,$vr16

	# Copy the initial states from @x[*] to @y[*]
	vori.b			$vr16,$vr0,0
	vori.b			$vr17,$vr1,0
	vori.b			$vr18,$vr2,0
	vori.b			$vr19,$vr3,0
	vori.b			$vr20,$vr4,0
	vori.b			$vr21,$vr5,0
	vori.b			$vr22,$vr6,0
	vori.b			$vr23,$vr7,0
	vori.b			$vr24,$vr8,0
	vori.b			$vr25,$vr9,0
	vori.b			$vr26,$vr10,0
	vori.b			$vr27,$vr11,0
	vori.b			$vr28,$vr12,0
	vori.b			$vr29,$vr13,0
	vori.b			$vr30,$vr14,0
	vori.b			$vr31,$vr15,0

	# Update states in @x[*] for 20 rounds
	ori				$t8,$zero,10
	b				.Loop_4x

.align 5
.Loop_4x:
	vadd.w		$vr0,$vr0,$vr4
	vxor.v		$vr12,$vr12,$vr0
	vrotri.w	$vr12,$vr12,16      # rotate left 16 bits
	vadd.w		$vr1,$vr1,$vr5
	vxor.v		$vr13,$vr13,$vr1
	vrotri.w	$vr13,$vr13,16

	vadd.w		$vr8,$vr8,$vr12
	vxor.v		$vr4,$vr4,$vr8
	vrotri.w	$vr4,$vr4,20      # rotate left 12 bits
	vadd.w		$vr9,$vr9,$vr13
	vxor.v		$vr5,$vr5,$vr9
	vrotri.w	$vr5,$vr5,20

	vadd.w		$vr0,$vr0,$vr4
	vxor.v		$vr12,$vr12,$vr0
	vrotri.w	$vr12,$vr12,24      # rotate left 8 bits
	vadd.w		$vr1,$vr1,$vr5
	vxor.v		$vr13,$vr13,$vr1
	vrotri.w	$vr13,$vr13,24

	vadd.w		$vr8,$vr8,$vr12
	vxor.v		$vr4,$vr4,$vr8
	vrotri.w	$vr4,$vr4,25      # rotate left 7 bits
	vadd.w		$vr9,$vr9,$vr13
	vxor.v		$vr5,$vr5,$vr9
	vrotri.w	$vr5,$vr5,25

	vadd.w		$vr2,$vr2,$vr6
	vxor.v		$vr14,$vr14,$vr2
	vrotri.w	$vr14,$vr14,16
	vadd.w		$vr3,$vr3,$vr7
	vxor.v		$vr15,$vr15,$vr3
	vrotri.w	$vr15,$vr15,16

	vadd.w		$vr10,$vr10,$vr14
	vxor.v		$vr6,$vr6,$vr10
	vrotri.w	$vr6,$vr6,20
	vadd.w		$vr11,$vr11,$vr15
	vxor.v		$vr7,$vr7,$vr11
	vrotri.w	$vr7,$vr7,20

	vadd.w		$vr2,$vr2,$vr6
	vxor.v		$vr14,$vr14,$vr2
	vrotri.w	$vr14,$vr14,24
	vadd.w		$vr3,$vr3,$vr7
	vxor.v		$vr15,$vr15,$vr3
	vrotri.w	$vr15,$vr15,24

	vadd.w		$vr10,$vr10,$vr14
	vxor.v		$vr6,$vr6,$vr10
	vrotri.w	$vr6,$vr6,25
	vadd.w		$vr11,$vr11,$vr15
	vxor.v		$vr7,$vr7,$vr11
	vrotri.w	$vr7,$vr7,25

	vadd.w		$vr0,$vr0,$vr5
	vxor.v		$vr15,$vr15,$vr0
	vrotri.w	$vr15,$vr15,16      # rotate left 16 bits
	vadd.w		$vr1,$vr1,$vr6
	vxor.v		$vr12,$vr12,$vr1
	vrotri.w	$vr12,$vr12,16

	vadd.w		$vr10,$vr10,$vr15
	vxor.v		$vr5,$vr5,$vr10
	vrotri.w	$vr5,$vr5,20      # rotate left 12 bits
	vadd.w		$vr11,$vr11,$vr12
	vxor.v		$vr6,$vr6,$vr11
	vrotri.w	$vr6,$vr6,20

	vadd.w		$vr0,$vr0,$vr5
	vxor.v		$vr15,$vr15,$vr0
	vrotri.w	$vr15,$vr15,24      # rotate left 8 bits
	vadd.w		$vr1,$vr1,$vr6
	vxor.v		$vr12,$vr12,$vr1
	vrotri.w	$vr12,$vr12,24

	vadd.w		$vr10,$vr10,$vr15
	vxor.v		$vr5,$vr5,$vr10
	vrotri.w	$vr5,$vr5,25      # rotate left 7 bits
	vadd.w		$vr11,$vr11,$vr12
	vxor.v		$vr6,$vr6,$vr11
	vrotri.w	$vr6,$vr6,25

	vadd.w		$vr2,$vr2,$vr7
	vxor.v		$vr13,$vr13,$vr2
	vrotri.w	$vr13,$vr13,16
	vadd.w		$vr3,$vr3,$vr4
	vxor.v		$vr14,$vr14,$vr3
	vrotri.w	$vr14,$vr14,16

	vadd.w		$vr8,$vr8,$vr13
	vxor.v		$vr7,$vr7,$vr8
	vrotri.w	$vr7,$vr7,20
	vadd.w		$vr9,$vr9,$vr14
	vxor.v		$vr4,$vr4,$vr9
	vrotri.w	$vr4,$vr4,20

	vadd.w		$vr2,$vr2,$vr7
	vxor.v		$vr13,$vr13,$vr2
	vrotri.w	$vr13,$vr13,24
	vadd.w		$vr3,$vr3,$vr4
	vxor.v		$vr14,$vr14,$vr3
	vrotri.w	$vr14,$vr14,24

	vadd.w		$vr8,$vr8,$vr13
	vxor.v		$vr7,$vr7,$vr8
	vrotri.w	$vr7,$vr7,25
	vadd.w		$vr9,$vr9,$vr14
	vxor.v		$vr4,$vr4,$vr9
	vrotri.w	$vr4,$vr4,25

	addi.w		$t8,$t8,-1
	bnez		$t8,.Loop_4x

	# Get the final states by adding the initial states
	vadd.w		$vr0,$vr0,$vr16
	vadd.w		$vr1,$vr1,$vr17
	vadd.w		$vr2,$vr2,$vr18
	vadd.w		$vr3,$vr3,$vr19
	vadd.w		$vr4,$vr4,$vr20
	vadd.w		$vr5,$vr5,$vr21
	vadd.w		$vr6,$vr6,$vr22
	vadd.w		$vr7,$vr7,$vr23
	vadd.w		$vr8,$vr8,$vr24
	vadd.w		$vr9,$vr9,$vr25
	vadd.w		$vr10,$vr10,$vr26
	vadd.w		$vr11,$vr11,$vr27
	vadd.w		$vr12,$vr12,$vr28
	vadd.w		$vr13,$vr13,$vr29
	vadd.w		$vr14,$vr14,$vr30
	vadd.w		$vr15,$vr15,$vr31

	# Get the transpose of @x[*] and save them in @x[*]
	vilvl.w		$vr16,$vr1,$vr0
	vilvh.w		$vr17,$vr1,$vr0
	vilvl.w		$vr18,$vr3,$vr2
	vilvh.w		$vr19,$vr3,$vr2
	vilvl.w		$vr20,$vr5,$vr4
	vilvh.w		$vr21,$vr5,$vr4
	vilvl.w		$vr22,$vr7,$vr6
	vilvh.w		$vr23,$vr7,$vr6
	vilvl.w		$vr24,$vr9,$vr8
	vilvh.w		$vr25,$vr9,$vr8
	vilvl.w		$vr26,$vr11,$vr10
	vilvh.w		$vr27,$vr11,$vr10
	vilvl.w		$vr28,$vr13,$vr12
	vilvh.w		$vr29,$vr13,$vr12
	vilvl.w		$vr30,$vr15,$vr14
	vilvh.w		$vr31,$vr15,$vr14

	vilvl.d		$vr0,$vr18,$vr16
	vilvh.d		$vr1,$vr18,$vr16
	vilvl.d		$vr2,$vr19,$vr17
	vilvh.d		$vr3,$vr19,$vr17
	vilvl.d		$vr4,$vr22,$vr20
	vilvh.d		$vr5,$vr22,$vr20
	vilvl.d		$vr6,$vr23,$vr21
	vilvh.d		$vr7,$vr23,$vr21
	vilvl.d		$vr8,$vr26,$vr24
	vilvh.d		$vr9,$vr26,$vr24
	vilvl.d		$vr10,$vr27,$vr25
	vilvh.d		$vr11,$vr27,$vr25
	vilvl.d		$vr12,$vr30,$vr28
	vilvh.d		$vr13,$vr30,$vr28
	vilvl.d		$vr14,$vr31,$vr29
	vilvh.d		$vr15,$vr31,$vr29
	ori			$t8,$zero,64*4
	bltu		$a2,$t8,.Ltail_4x

	# Get the encrypted message by xor states with plaintext
	vld			$vr16,$a1,16*0
	vld			$vr17,$a1,16*1
	vld			$vr18,$a1,16*2
	vld			$vr19,$a1,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$a0,16*0
	vst			$vr17,$a0,16*1
	vst			$vr18,$a0,16*2
	vst			$vr19,$a0,16*3

	vld			$vr16,$a1,16*4
	vld			$vr17,$a1,16*5
	vld			$vr18,$a1,16*6
	vld			$vr19,$a1,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$a0,16*4
	vst			$vr17,$a0,16*5
	vst			$vr18,$a0,16*6
	vst			$vr19,$a0,16*7

	vld			$vr16,$a1,16*8
	vld			$vr17,$a1,16*9
	vld			$vr18,$a1,16*10
	vld			$vr19,$a1,16*11
	vxor.v		$vr16,$vr16,$vr2
	vxor.v		$vr17,$vr17,$vr6
	vxor.v		$vr18,$vr18,$vr10
	vxor.v		$vr19,$vr19,$vr14
	vst			$vr16,$a0,16*8
	vst			$vr17,$a0,16*9
	vst			$vr18,$a0,16*10
	vst			$vr19,$a0,16*11

	vld			$vr16,$a1,16*12
	vld			$vr17,$a1,16*13
	vld			$vr18,$a1,16*14
	vld			$vr19,$a1,16*15
	vxor.v		$vr16,$vr16,$vr3
	vxor.v		$vr17,$vr17,$vr7
	vxor.v		$vr18,$vr18,$vr11
	vxor.v		$vr19,$vr19,$vr15
	vst			$vr16,$a0,16*12
	vst			$vr17,$a0,16*13
	vst			$vr18,$a0,16*14
	vst			$vr19,$a0,16*15

	addi.d		$a2,$a2,-64*4
	beqz		$a2,.Ldone_4x
	addi.d		$a1,$a1,64*4
	addi.d		$a0,$a0,64*4
	addi.w		$t4,$t4,4
	b			.Loop_outer_4x

.Ltail_4x:
	# Handle the tail for 4x (1 <= tail_len <= 255)
	ori			$t8,$zero,192
	bgeu		$a2,$t8,.L192_or_more4x
	ori			$t8,$zero,128
	bgeu		$a2,$t8,.L128_or_more4x
	ori			$t8,$zero,64
	bgeu		$a2,$t8,.L64_or_more4x

	vst			$vr0,$sp,16*0
	vst			$vr4,$sp,16*1
	vst			$vr8,$sp,16*2
	vst			$vr12,$sp,16*3
	move		$t8,$zero
	b			.Loop_tail_4x

.align 5
.L64_or_more4x:
	vld			$vr16,$a1,16*0
	vld			$vr17,$a1,16*1
	vld			$vr18,$a1,16*2
	vld			$vr19,$a1,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$a0,16*0
	vst			$vr17,$a0,16*1
	vst			$vr18,$a0,16*2
	vst			$vr19,$a0,16*3

	addi.d		$a2,$a2,-64
	beqz		$a2,.Ldone_4x
	addi.d		$a1,$a1,64
	addi.d		$a0,$a0,64
	vst			$vr1,$sp,16*0
	vst			$vr5,$sp,16*1
	vst			$vr9,$sp,16*2
	vst			$vr13,$sp,16*3
	move		$t8,$zero
	b			.Loop_tail_4x

.align 5
.L128_or_more4x:
	vld			$vr16,$a1,16*0
	vld			$vr17,$a1,16*1
	vld			$vr18,$a1,16*2
	vld			$vr19,$a1,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$a0,16*0
	vst			$vr17,$a0,16*1
	vst			$vr18,$a0,16*2
	vst			$vr19,$a0,16*3

	vld			$vr16,$a1,16*4
	vld			$vr17,$a1,16*5
	vld			$vr18,$a1,16*6
	vld			$vr19,$a1,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$a0,16*4
	vst			$vr17,$a0,16*5
	vst			$vr18,$a0,16*6
	vst			$vr19,$a0,16*7

	addi.d		$a2,$a2,-128
	beqz		$a2,.Ldone_4x
	addi.d		$a1,$a1,128
	addi.d		$a0,$a0,128
	vst			$vr2,$sp,16*0
	vst			$vr6,$sp,16*1
	vst			$vr10,$sp,16*2
	vst			$vr14,$sp,16*3
	move		$t8,$zero
	b			.Loop_tail_4x

.align 5
.L192_or_more4x:
	vld			$vr16,$a1,16*0
	vld			$vr17,$a1,16*1
	vld			$vr18,$a1,16*2
	vld			$vr19,$a1,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$a0,16*0
	vst			$vr17,$a0,16*1
	vst			$vr18,$a0,16*2
	vst			$vr19,$a0,16*3

	vld			$vr16,$a1,16*4
	vld			$vr17,$a1,16*5
	vld			$vr18,$a1,16*6
	vld			$vr19,$a1,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$a0,16*4
	vst			$vr17,$a0,16*5
	vst			$vr18,$a0,16*6
	vst			$vr19,$a0,16*7

	vld			$vr16,$a1,16*8
	vld			$vr17,$a1,16*9
	vld			$vr18,$a1,16*10
	vld			$vr19,$a1,16*11
	vxor.v		$vr16,$vr16,$vr2
	vxor.v		$vr17,$vr17,$vr6
	vxor.v		$vr18,$vr18,$vr10
	vxor.v		$vr19,$vr19,$vr14
	vst			$vr16,$a0,16*8
	vst			$vr17,$a0,16*9
	vst			$vr18,$a0,16*10
	vst			$vr19,$a0,16*11

	addi.d		$a2,$a2,-192
	beqz		$a2,.Ldone_4x
	addi.d		$a1,$a1,192
	addi.d		$a0,$a0,192
	vst			$vr3,$sp,16*0
	vst			$vr7,$sp,16*1
	vst			$vr11,$sp,16*2
	vst			$vr15,$sp,16*3
	move		$t8,$zero
	b			.Loop_tail_4x

.Loop_tail_4x:
	# Xor input with states byte by byte
	ldx.bu		$t5,$a1,$t8
	ldx.bu		$t6,$sp,$t8
	xor			$t5,$t5,$t6
	stx.b		$t5,$a0,$t8
	addi.w		$t8,$t8,1
	addi.d		$a2,$a2,-1
	bnez		$a2,.Loop_tail_4x
	b			.Ldone_4x

.Ldone_4x:
	addi.d		$sp,$sp,128
	b			.Lrestore_saved_fpr

.align 6
.LChaCha20_8x:
	addi.d		$sp,$sp,-128

	# Save the initial block counter in $t4
	ld.w		$t4,$a4,0
	b			.Loop_outer_8x

.align 5
.Loop_outer_8x:
	# Load constant
	la.local		$t8,.Lsigma
	xvldrepl.w		$xr0,$t8,4*0		# 'expa'
	xvldrepl.w		$xr1,$t8,4*1		# 'nd 3'
	xvldrepl.w		$xr2,$t8,4*2		# '2-by'
	xvldrepl.w		$xr3,$t8,4*3		# 'te k'

	# Load key
	xvldrepl.w		$xr4,$a3,4*0
	xvldrepl.w		$xr5,$a3,4*1
	xvldrepl.w		$xr6,$a3,4*2
	xvldrepl.w		$xr7,$a3,4*3
	xvldrepl.w		$xr8,$a3,4*4
	xvldrepl.w		$xr9,$a3,4*5
	xvldrepl.w		$xr10,$a3,4*6
	xvldrepl.w		$xr11,$a3,4*7

	# Load block counter
	xvreplgr2vr.w	$xr12,$t4

	# Load nonce
	xvldrepl.w		$xr13,$a4,4*1
	xvldrepl.w		$xr14,$a4,4*2
	xvldrepl.w		$xr15,$a4,4*3

	# Get the correct block counter for each block
	la.local		$t8,.Linc8x
	xvld			$xr16,$t8,0
	xvadd.w			$xr12,$xr12,$xr16

	# Copy the initial states from @x[*] to @y[*]
	xvori.b			$xr16,$xr0,0
	xvori.b			$xr17,$xr1,0
	xvori.b			$xr18,$xr2,0
	xvori.b			$xr19,$xr3,0
	xvori.b			$xr20,$xr4,0
	xvori.b			$xr21,$xr5,0
	xvori.b			$xr22,$xr6,0
	xvori.b			$xr23,$xr7,0
	xvori.b			$xr24,$xr8,0
	xvori.b			$xr25,$xr9,0
	xvori.b			$xr26,$xr10,0
	xvori.b			$xr27,$xr11,0
	xvori.b			$xr28,$xr12,0
	xvori.b			$xr29,$xr13,0
	xvori.b			$xr30,$xr14,0
	xvori.b			$xr31,$xr15,0

	# Update states in @x[*] for 20 rounds
	ori				$t8,$zero,10
	b				.Loop_8x

.align 5
.Loop_8x:
	xvadd.w		$xr0,$xr0,$xr4
	xvxor.v		$xr12,$xr12,$xr0
	xvrotri.w	$xr12,$xr12,16      # rotate left 16 bits
	xvadd.w		$xr1,$xr1,$xr5
	xvxor.v		$xr13,$xr13,$xr1
	xvrotri.w	$xr13,$xr13,16

	xvadd.w		$xr8,$xr8,$xr12
	xvxor.v		$xr4,$xr4,$xr8
	xvrotri.w	$xr4,$xr4,20      # rotate left 12 bits
	xvadd.w		$xr9,$xr9,$xr13
	xvxor.v		$xr5,$xr5,$xr9
	xvrotri.w	$xr5,$xr5,20

	xvadd.w		$xr0,$xr0,$xr4
	xvxor.v		$xr12,$xr12,$xr0
	xvrotri.w	$xr12,$xr12,24      # rotate left 8 bits
	xvadd.w		$xr1,$xr1,$xr5
	xvxor.v		$xr13,$xr13,$xr1
	xvrotri.w	$xr13,$xr13,24

	xvadd.w		$xr8,$xr8,$xr12
	xvxor.v		$xr4,$xr4,$xr8
	xvrotri.w	$xr4,$xr4,25      # rotate left 7 bits
	xvadd.w		$xr9,$xr9,$xr13
	xvxor.v		$xr5,$xr5,$xr9
	xvrotri.w	$xr5,$xr5,25

	xvadd.w		$xr2,$xr2,$xr6
	xvxor.v		$xr14,$xr14,$xr2
	xvrotri.w	$xr14,$xr14,16
	xvadd.w		$xr3,$xr3,$xr7
	xvxor.v		$xr15,$xr15,$xr3
	xvrotri.w	$xr15,$xr15,16

	xvadd.w		$xr10,$xr10,$xr14
	xvxor.v		$xr6,$xr6,$xr10
	xvrotri.w	$xr6,$xr6,20
	xvadd.w		$xr11,$xr11,$xr15
	xvxor.v		$xr7,$xr7,$xr11
	xvrotri.w	$xr7,$xr7,20

	xvadd.w		$xr2,$xr2,$xr6
	xvxor.v		$xr14,$xr14,$xr2
	xvrotri.w	$xr14,$xr14,24
	xvadd.w		$xr3,$xr3,$xr7
	xvxor.v		$xr15,$xr15,$xr3
	xvrotri.w	$xr15,$xr15,24

	xvadd.w		$xr10,$xr10,$xr14
	xvxor.v		$xr6,$xr6,$xr10
	xvrotri.w	$xr6,$xr6,25
	xvadd.w		$xr11,$xr11,$xr15
	xvxor.v		$xr7,$xr7,$xr11
	xvrotri.w	$xr7,$xr7,25

	xvadd.w		$xr0,$xr0,$xr5
	xvxor.v		$xr15,$xr15,$xr0
	xvrotri.w	$xr15,$xr15,16      # rotate left 16 bits
	xvadd.w		$xr1,$xr1,$xr6
	xvxor.v		$xr12,$xr12,$xr1
	xvrotri.w	$xr12,$xr12,16

	xvadd.w		$xr10,$xr10,$xr15
	xvxor.v		$xr5,$xr5,$xr10
	xvrotri.w	$xr5,$xr5,20      # rotate left 12 bits
	xvadd.w		$xr11,$xr11,$xr12
	xvxor.v		$xr6,$xr6,$xr11
	xvrotri.w	$xr6,$xr6,20

	xvadd.w		$xr0,$xr0,$xr5
	xvxor.v		$xr15,$xr15,$xr0
	xvrotri.w	$xr15,$xr15,24      # rotate left 8 bits
	xvadd.w		$xr1,$xr1,$xr6
	xvxor.v		$xr12,$xr12,$xr1
	xvrotri.w	$xr12,$xr12,24

	xvadd.w		$xr10,$xr10,$xr15
	xvxor.v		$xr5,$xr5,$xr10
	xvrotri.w	$xr5,$xr5,25      # rotate left 7 bits
	xvadd.w		$xr11,$xr11,$xr12
	xvxor.v		$xr6,$xr6,$xr11
	xvrotri.w	$xr6,$xr6,25

	xvadd.w		$xr2,$xr2,$xr7
	xvxor.v		$xr13,$xr13,$xr2
	xvrotri.w	$xr13,$xr13,16
	xvadd.w		$xr3,$xr3,$xr4
	xvxor.v		$xr14,$xr14,$xr3
	xvrotri.w	$xr14,$xr14,16

	xvadd.w		$xr8,$xr8,$xr13
	xvxor.v		$xr7,$xr7,$xr8
	xvrotri.w	$xr7,$xr7,20
	xvadd.w		$xr9,$xr9,$xr14
	xvxor.v		$xr4,$xr4,$xr9
	xvrotri.w	$xr4,$xr4,20

	xvadd.w		$xr2,$xr2,$xr7
	xvxor.v		$xr13,$xr13,$xr2
	xvrotri.w	$xr13,$xr13,24
	xvadd.w		$xr3,$xr3,$xr4
	xvxor.v		$xr14,$xr14,$xr3
	xvrotri.w	$xr14,$xr14,24

	xvadd.w		$xr8,$xr8,$xr13
	xvxor.v		$xr7,$xr7,$xr8
	xvrotri.w	$xr7,$xr7,25
	xvadd.w		$xr9,$xr9,$xr14
	xvxor.v		$xr4,$xr4,$xr9
	xvrotri.w	$xr4,$xr4,25

	addi.w		$t8,$t8,-1
	bnez		$t8,.Loop_8x

	# Get the final states by adding the initial states
	xvadd.w		$xr0,$xr0,$xr16
	xvadd.w		$xr1,$xr1,$xr17
	xvadd.w		$xr2,$xr2,$xr18
	xvadd.w		$xr3,$xr3,$xr19
	xvadd.w		$xr4,$xr4,$xr20
	xvadd.w		$xr5,$xr5,$xr21
	xvadd.w		$xr6,$xr6,$xr22
	xvadd.w		$xr7,$xr7,$xr23
	xvadd.w		$xr8,$xr8,$xr24
	xvadd.w		$xr9,$xr9,$xr25
	xvadd.w		$xr10,$xr10,$xr26
	xvadd.w		$xr11,$xr11,$xr27
	xvadd.w		$xr12,$xr12,$xr28
	xvadd.w		$xr13,$xr13,$xr29
	xvadd.w		$xr14,$xr14,$xr30
	xvadd.w		$xr15,$xr15,$xr31

	# Get the transpose of @x[*] and save them in @y[*]
	xvilvl.w	$xr16,$xr1,$xr0
	xvilvh.w	$xr17,$xr1,$xr0
	xvilvl.w	$xr18,$xr3,$xr2
	xvilvh.w	$xr19,$xr3,$xr2
	xvilvl.w	$xr20,$xr5,$xr4
	xvilvh.w	$xr21,$xr5,$xr4
	xvilvl.w	$xr22,$xr7,$xr6
	xvilvh.w	$xr23,$xr7,$xr6
	xvilvl.w	$xr24,$xr9,$xr8
	xvilvh.w	$xr25,$xr9,$xr8
	xvilvl.w	$xr26,$xr11,$xr10
	xvilvh.w	$xr27,$xr11,$xr10
	xvilvl.w	$xr28,$xr13,$xr12
	xvilvh.w	$xr29,$xr13,$xr12
	xvilvl.w	$xr30,$xr15,$xr14
	xvilvh.w	$xr31,$xr15,$xr14

	xvilvl.d	$xr0,$xr18,$xr16
	xvilvh.d	$xr1,$xr18,$xr16
	xvilvl.d	$xr2,$xr19,$xr17
	xvilvh.d	$xr3,$xr19,$xr17
	xvilvl.d	$xr4,$xr22,$xr20
	xvilvh.d	$xr5,$xr22,$xr20
	xvilvl.d	$xr6,$xr23,$xr21
	xvilvh.d	$xr7,$xr23,$xr21
	xvilvl.d	$xr8,$xr26,$xr24
	xvilvh.d	$xr9,$xr26,$xr24
	xvilvl.d	$xr10,$xr27,$xr25
	xvilvh.d	$xr11,$xr27,$xr25
	xvilvl.d	$xr12,$xr30,$xr28
	xvilvh.d	$xr13,$xr30,$xr28
	xvilvl.d	$xr14,$xr31,$xr29
	xvilvh.d	$xr15,$xr31,$xr29

	xvori.b		$xr16,$xr4,0
	xvpermi.q	$xr16,$xr0,0x20
	xvori.b		$xr17,$xr5,0
	xvpermi.q	$xr17,$xr1,0x20
	xvori.b		$xr18,$xr6,0
	xvpermi.q	$xr18,$xr2,0x20
	xvori.b		$xr19,$xr7,0
	xvpermi.q	$xr19,$xr3,0x20
	xvori.b		$xr20,$xr4,0
	xvpermi.q	$xr20,$xr0,0x31
	xvori.b		$xr21,$xr5,0
	xvpermi.q	$xr21,$xr1,0x31
	xvori.b		$xr22,$xr6,0
	xvpermi.q	$xr22,$xr2,0x31
	xvori.b		$xr23,$xr7,0
	xvpermi.q	$xr23,$xr3,0x31
	xvori.b		$xr24,$xr12,0
	xvpermi.q	$xr24,$xr8,0x20
	xvori.b		$xr25,$xr13,0
	xvpermi.q	$xr25,$xr9,0x20
	xvori.b		$xr26,$xr14,0
	xvpermi.q	$xr26,$xr10,0x20
	xvori.b		$xr27,$xr15,0
	xvpermi.q	$xr27,$xr11,0x20
	xvori.b		$xr28,$xr12,0
	xvpermi.q	$xr28,$xr8,0x31
	xvori.b		$xr29,$xr13,0
	xvpermi.q	$xr29,$xr9,0x31
	xvori.b		$xr30,$xr14,0
	xvpermi.q	$xr30,$xr10,0x31
	xvori.b		$xr31,$xr15,0
	xvpermi.q	$xr31,$xr11,0x31

	ori			$t8,$zero,64*8
	bltu		$a2,$t8,.Ltail_8x

	# Get the encrypted message by xor states with plaintext
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvld		$xr2,$a1,32*6
	xvld		$xr3,$a1,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5
	xvst		$xr2,$a0,32*6
	xvst		$xr3,$a0,32*7

	xvld		$xr0,$a1,32*8
	xvld		$xr1,$a1,32*9
	xvld		$xr2,$a1,32*10
	xvld		$xr3,$a1,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$a0,32*8
	xvst		$xr1,$a0,32*9
	xvst		$xr2,$a0,32*10
	xvst		$xr3,$a0,32*11

	xvld		$xr0,$a1,32*12
	xvld		$xr1,$a1,32*13
	xvld		$xr2,$a1,32*14
	xvld		$xr3,$a1,32*15
	xvxor.v		$xr0,$xr0,$xr22
	xvxor.v		$xr1,$xr1,$xr30
	xvxor.v		$xr2,$xr2,$xr23
	xvxor.v		$xr3,$xr3,$xr31
	xvst		$xr0,$a0,32*12
	xvst		$xr1,$a0,32*13
	xvst		$xr2,$a0,32*14
	xvst		$xr3,$a0,32*15

	addi.d		$a2,$a2,-64*8
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,64*8
	addi.d		$a0,$a0,64*8
	addi.w		$t4,$t4,8
	b			.Loop_outer_8x

.Ltail_8x:
	# Handle the tail for 8x (1 <= tail_len <= 511)
	ori			$t8,$zero,448
	bgeu		$a2,$t8,.L448_or_more8x
	ori			$t8,$zero,384
	bgeu		$a2,$t8,.L384_or_more8x
	ori			$t8,$zero,320
	bgeu		$a2,$t8,.L320_or_more8x
	ori			$t8,$zero,256
	bgeu		$a2,$t8,.L256_or_more8x
	ori			$t8,$zero,192
	bgeu		$a2,$t8,.L192_or_more8x
	ori			$t8,$zero,128
	bgeu		$a2,$t8,.L128_or_more8x
	ori			$t8,$zero,64
	bgeu		$a2,$t8,.L64_or_more8x

	xvst		$xr16,$sp,32*0
	xvst		$xr24,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L64_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1

	addi.d		$a2,$a2,-64
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,64
	addi.d		$a0,$a0,64
	xvst		$xr17,$sp,32*0
	xvst		$xr25,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L128_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	addi.d		$a2,$a2,-128
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,128
	addi.d		$a0,$a0,128
	xvst		$xr18,$sp,32*0
	xvst		$xr26,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L192_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5

	addi.d		$a2,$a2,-192
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,192
	addi.d		$a0,$a0,192
	xvst		$xr19,$sp,32*0
	xvst		$xr27,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L256_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvld		$xr2,$a1,32*6
	xvld		$xr3,$a1,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5
	xvst		$xr2,$a0,32*6
	xvst		$xr3,$a0,32*7

	addi.d		$a2,$a2,-256
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,256
	addi.d		$a0,$a0,256
	xvst		$xr20,$sp,32*0
	xvst		$xr28,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L320_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvld		$xr2,$a1,32*6
	xvld		$xr3,$a1,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5
	xvst		$xr2,$a0,32*6
	xvst		$xr3,$a0,32*7

	xvld		$xr0,$a1,32*8
	xvld		$xr1,$a1,32*9
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvst		$xr0,$a0,32*8
	xvst		$xr1,$a0,32*9

	addi.d		$a2,$a2,-320
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,320
	addi.d		$a0,$a0,320
	xvst		$xr21,$sp,32*0
	xvst		$xr29,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L384_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvld		$xr2,$a1,32*6
	xvld		$xr3,$a1,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5
	xvst		$xr2,$a0,32*6
	xvst		$xr3,$a0,32*7

	xvld		$xr0,$a1,32*8
	xvld		$xr1,$a1,32*9
	xvld		$xr2,$a1,32*10
	xvld		$xr3,$a1,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$a0,32*8
	xvst		$xr1,$a0,32*9
	xvst		$xr2,$a0,32*10
	xvst		$xr3,$a0,32*11

	addi.d		$a2,$a2,-384
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,384
	addi.d		$a0,$a0,384
	xvst		$xr22,$sp,32*0
	xvst		$xr30,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.align 5
.L448_or_more8x:
	xvld		$xr0,$a1,32*0
	xvld		$xr1,$a1,32*1
	xvld		$xr2,$a1,32*2
	xvld		$xr3,$a1,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$a0,32*0
	xvst		$xr1,$a0,32*1
	xvst		$xr2,$a0,32*2
	xvst		$xr3,$a0,32*3

	xvld		$xr0,$a1,32*4
	xvld		$xr1,$a1,32*5
	xvld		$xr2,$a1,32*6
	xvld		$xr3,$a1,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$a0,32*4
	xvst		$xr1,$a0,32*5
	xvst		$xr2,$a0,32*6
	xvst		$xr3,$a0,32*7

	xvld		$xr0,$a1,32*8
	xvld		$xr1,$a1,32*9
	xvld		$xr2,$a1,32*10
	xvld		$xr3,$a1,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$a0,32*8
	xvst		$xr1,$a0,32*9
	xvst		$xr2,$a0,32*10
	xvst		$xr3,$a0,32*11

	xvld		$xr0,$a1,32*12
	xvld		$xr1,$a1,32*13
	xvxor.v		$xr0,$xr0,$xr22
	xvxor.v		$xr1,$xr1,$xr30
	xvst		$xr0,$a0,32*12
	xvst		$xr1,$a0,32*13

	addi.d		$a2,$a2,-448
	beqz		$a2,.Ldone_8x
	addi.d		$a1,$a1,448
	addi.d		$a0,$a0,448
	xvst		$xr23,$sp,32*0
	xvst		$xr31,$sp,32*1
	move		$t8,$zero
	b			.Loop_tail_8x

.Loop_tail_8x:
	# Xor input with states byte by byte
	ldx.bu		$t5,$a1,$t8
	ldx.bu		$t6,$sp,$t8
	xor			$t5,$t5,$t6
	stx.b		$t5,$a0,$t8
	addi.w		$t8,$t8,1
	addi.d		$a2,$a2,-1
	bnez		$a2,.Loop_tail_8x
	b			.Ldone_8x

.Ldone_8x:
	addi.d		$sp,$sp,128
	b			.Lrestore_saved_fpr

.Lrestore_saved_fpr:
	fld.d		$f24,$sp,0
	fld.d		$f25,$sp,8
	fld.d		$f26,$sp,16
	fld.d		$f27,$sp,24
	fld.d		$f28,$sp,32
	fld.d		$f29,$sp,40
	fld.d		$f30,$sp,48
	fld.d		$f31,$sp,56
	addi.d		$sp,$sp,64
.Lno_data:
.Lend:
	jr	$ra
.size ChaCha20_ctr32,.-ChaCha20_ctr32
