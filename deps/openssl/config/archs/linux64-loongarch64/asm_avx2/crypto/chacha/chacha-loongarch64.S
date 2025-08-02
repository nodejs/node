#include "loongarch_arch.h"

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
	# $r4 = arg #1 (out pointer)
	# $r5 = arg #2 (inp pointer)
	# $r6 = arg #3 (len)
	# $r7 = arg #4 (key array)
	# $r8 = arg #5 (counter array)

	beqz		$r6,.Lno_data
	ori			$r15,$r0,64
	la.global	$r12,OPENSSL_loongarch_hwcap_P
	ld.w		$r12,$r12,0

	bleu		$r6,$r15,.LChaCha20_1x  # goto 1x when len <= 64

	andi		$r12,$r12,LOONGARCH_HWCAP_LASX | LOONGARCH_HWCAP_LSX
	beqz		$r12,.LChaCha20_1x

	addi.d		$r3,$r3,-64
	fst.d		$f24,$r3,0
	fst.d		$f25,$r3,8
	fst.d		$f26,$r3,16
	fst.d		$f27,$r3,24
	fst.d		$f28,$r3,32
	fst.d		$f29,$r3,40
	fst.d		$f30,$r3,48
	fst.d		$f31,$r3,56

	andi		$r13,$r12,LOONGARCH_HWCAP_LASX
	bnez		$r13,.LChaCha20_8x

	b		.LChaCha20_4x

.align 6
.LChaCha20_1x:
	addi.d		$r3,$r3,-256
	st.d		$r23,$r3,0
	st.d		$r24,$r3,8
	st.d		$r25,$r3,16
	st.d		$r26,$r3,24
	st.d		$r27,$r3,32
	st.d		$r28,$r3,40
	st.d		$r29,$r3,48
	st.d		$r30,$r3,56
	st.d		$r31,$r3,64

	# Save the initial block counter in $r31
	ld.w		$r31,$r8,0
	b			.Loop_outer_1x

.align 5
.Loop_outer_1x:
	# Load constants
	la.local	$r20,.Lsigma
	ld.w		$r12,$r20,0		  # 'expa'
	ld.w		$r13,$r20,4		  # 'nd 3'
	ld.w		$r14,$r20,8		  # '2-by'
	ld.w		$r15,$r20,12	  # 'te k'

	# Load key
	ld.w		$r16,$r7,4*0
	ld.w		$r17,$r7,4*1
	ld.w		$r18,$r7,4*2
	ld.w		$r19,$r7,4*3
	ld.w		$r23,$r7,4*4
	ld.w		$r24,$r7,4*5
	ld.w		$r25,$r7,4*6
	ld.w		$r26,$r7,4*7

	# Load block counter
	move		$r27,$r31

	# Load nonce
	ld.w		$r28,$r8,4*1
	ld.w		$r29,$r8,4*2
	ld.w		$r30,$r8,4*3

	# Update states in @x[*] for 20 rounds
	ori			$r20,$r0,10
	b			.Loop_1x

.align 5
.Loop_1x:
	add.w		$r12,$r12,$r16
	xor			$r27,$r27,$r12
	rotri.w		$r27,$r27,16      # rotate left 16 bits
	add.w		$r13,$r13,$r17
	xor			$r28,$r28,$r13
	rotri.w		$r28,$r28,16

	add.w		$r23,$r23,$r27
	xor			$r16,$r16,$r23
	rotri.w		$r16,$r16,20      # rotate left 12 bits
	add.w		$r24,$r24,$r28
	xor			$r17,$r17,$r24
	rotri.w		$r17,$r17,20

	add.w		$r12,$r12,$r16
	xor			$r27,$r27,$r12
	rotri.w		$r27,$r27,24      # rotate left 8 bits
	add.w		$r13,$r13,$r17
	xor			$r28,$r28,$r13
	rotri.w		$r28,$r28,24

	add.w		$r23,$r23,$r27
	xor			$r16,$r16,$r23
	rotri.w		$r16,$r16,25      # rotate left 7 bits
	add.w		$r24,$r24,$r28
	xor			$r17,$r17,$r24
	rotri.w		$r17,$r17,25

	add.w		$r14,$r14,$r18
	xor			$r29,$r29,$r14
	rotri.w		$r29,$r29,16
	add.w		$r15,$r15,$r19
	xor			$r30,$r30,$r15
	rotri.w		$r30,$r30,16

	add.w		$r25,$r25,$r29
	xor			$r18,$r18,$r25
	rotri.w		$r18,$r18,20
	add.w		$r26,$r26,$r30
	xor			$r19,$r19,$r26
	rotri.w		$r19,$r19,20

	add.w		$r14,$r14,$r18
	xor			$r29,$r29,$r14
	rotri.w		$r29,$r29,24
	add.w		$r15,$r15,$r19
	xor			$r30,$r30,$r15
	rotri.w		$r30,$r30,24

	add.w		$r25,$r25,$r29
	xor			$r18,$r18,$r25
	rotri.w		$r18,$r18,25
	add.w		$r26,$r26,$r30
	xor			$r19,$r19,$r26
	rotri.w		$r19,$r19,25

	add.w		$r12,$r12,$r17
	xor			$r30,$r30,$r12
	rotri.w		$r30,$r30,16      # rotate left 16 bits
	add.w		$r13,$r13,$r18
	xor			$r27,$r27,$r13
	rotri.w		$r27,$r27,16

	add.w		$r25,$r25,$r30
	xor			$r17,$r17,$r25
	rotri.w		$r17,$r17,20      # rotate left 12 bits
	add.w		$r26,$r26,$r27
	xor			$r18,$r18,$r26
	rotri.w		$r18,$r18,20

	add.w		$r12,$r12,$r17
	xor			$r30,$r30,$r12
	rotri.w		$r30,$r30,24      # rotate left 8 bits
	add.w		$r13,$r13,$r18
	xor			$r27,$r27,$r13
	rotri.w		$r27,$r27,24

	add.w		$r25,$r25,$r30
	xor			$r17,$r17,$r25
	rotri.w		$r17,$r17,25      # rotate left 7 bits
	add.w		$r26,$r26,$r27
	xor			$r18,$r18,$r26
	rotri.w		$r18,$r18,25

	add.w		$r14,$r14,$r19
	xor			$r28,$r28,$r14
	rotri.w		$r28,$r28,16
	add.w		$r15,$r15,$r16
	xor			$r29,$r29,$r15
	rotri.w		$r29,$r29,16

	add.w		$r23,$r23,$r28
	xor			$r19,$r19,$r23
	rotri.w		$r19,$r19,20
	add.w		$r24,$r24,$r29
	xor			$r16,$r16,$r24
	rotri.w		$r16,$r16,20

	add.w		$r14,$r14,$r19
	xor			$r28,$r28,$r14
	rotri.w		$r28,$r28,24
	add.w		$r15,$r15,$r16
	xor			$r29,$r29,$r15
	rotri.w		$r29,$r29,24

	add.w		$r23,$r23,$r28
	xor			$r19,$r19,$r23
	rotri.w		$r19,$r19,25
	add.w		$r24,$r24,$r29
	xor			$r16,$r16,$r24
	rotri.w		$r16,$r16,25

	addi.w		$r20,$r20,-1
	bnez		$r20,.Loop_1x

	# Get the final states by adding the initial states
	la.local	$r20,.Lsigma
	ld.w		$r11,$r20,4*0
	ld.w		$r10,$r20,4*1
	ld.w		$r9,$r20,4*2
	add.w		$r12,$r12,$r11
	add.w		$r13,$r13,$r10
	add.w		$r14,$r14,$r9
	ld.w		$r11,$r20,4*3
	add.w		$r15,$r15,$r11

	ld.w		$r20,$r7,4*0
	ld.w		$r11,$r7,4*1
	ld.w		$r10,$r7,4*2
	ld.w		$r9,$r7,4*3
	add.w		$r16,$r16,$r20
	add.w		$r17,$r17,$r11
	add.w		$r18,$r18,$r10
	add.w		$r19,$r19,$r9

	ld.w		$r20,$r7,4*4
	ld.w		$r11,$r7,4*5
	ld.w		$r10,$r7,4*6
	ld.w		$r9,$r7,4*7
	add.w		$r23,$r23,$r20
	add.w		$r24,$r24,$r11
	add.w		$r25,$r25,$r10
	add.w		$r26,$r26,$r9

	add.w		$r27,$r27,$r31

	ld.w		$r20,$r8,4*1
	ld.w		$r11,$r8,4*2
	ld.w		$r10,$r8,4*3
	add.w		$r28,$r28,$r20
	add.w		$r29,$r29,$r11
	add.w		$r30,$r30,$r10

	ori			$r20,$r0,64
	bltu		$r6,$r20,.Ltail_1x

	# Get the encrypted message by xor states with plaintext
	ld.w		$r20,$r5,4*0
	ld.w		$r11,$r5,4*1
	ld.w		$r10,$r5,4*2
	ld.w		$r9,$r5,4*3
	xor			$r20,$r20,$r12
	xor			$r11,$r11,$r13
	xor			$r10,$r10,$r14
	xor			$r9,$r9,$r15
	st.w		$r20,$r4,4*0
	st.w		$r11,$r4,4*1
	st.w		$r10,$r4,4*2
	st.w		$r9,$r4,4*3

	ld.w		$r20,$r5,4*4
	ld.w		$r11,$r5,4*5
	ld.w		$r10,$r5,4*6
	ld.w		$r9,$r5,4*7
	xor			$r20,$r20,$r16
	xor			$r11,$r11,$r17
	xor			$r10,$r10,$r18
	xor			$r9,$r9,$r19
	st.w		$r20,$r4,4*4
	st.w		$r11,$r4,4*5
	st.w		$r10,$r4,4*6
	st.w		$r9,$r4,4*7

	ld.w		$r20,$r5,4*8
	ld.w		$r11,$r5,4*9
	ld.w		$r10,$r5,4*10
	ld.w		$r9,$r5,4*11
	xor			$r20,$r20,$r23
	xor			$r11,$r11,$r24
	xor			$r10,$r10,$r25
	xor			$r9,$r9,$r26
	st.w		$r20,$r4,4*8
	st.w		$r11,$r4,4*9
	st.w		$r10,$r4,4*10
	st.w		$r9,$r4,4*11

	ld.w		$r20,$r5,4*12
	ld.w		$r11,$r5,4*13
	ld.w		$r10,$r5,4*14
	ld.w		$r9,$r5,4*15
	xor			$r20,$r20,$r27
	xor			$r11,$r11,$r28
	xor			$r10,$r10,$r29
	xor			$r9,$r9,$r30
	st.w		$r20,$r4,4*12
	st.w		$r11,$r4,4*13
	st.w		$r10,$r4,4*14
	st.w		$r9,$r4,4*15

	addi.d		$r6,$r6,-64
	beqz		$r6,.Ldone_1x
	addi.d		$r5,$r5,64
	addi.d		$r4,$r4,64
	addi.w		$r31,$r31,1
	b			.Loop_outer_1x

.align 4
.Ltail_1x:
	# Handle the tail for 1x (1 <= tail_len <= 63)
	addi.d		$r11,$r3,72
	st.w		$r12,$r11,4*0
	st.w		$r13,$r11,4*1
	st.w		$r14,$r11,4*2
	st.w		$r15,$r11,4*3
	st.w		$r16,$r11,4*4
	st.w		$r17,$r11,4*5
	st.w		$r18,$r11,4*6
	st.w		$r19,$r11,4*7
	st.w		$r23,$r11,4*8
	st.w		$r24,$r11,4*9
	st.w		$r25,$r11,4*10
	st.w		$r26,$r11,4*11
	st.w		$r27,$r11,4*12
	st.w		$r28,$r11,4*13
	st.w		$r29,$r11,4*14
	st.w		$r30,$r11,4*15

	move		$r20,$r0

.Loop_tail_1x:
	# Xor input with states byte by byte
	ldx.bu		$r10,$r5,$r20
	ldx.bu		$r9,$r11,$r20
	xor			$r10,$r10,$r9
	stx.b		$r10,$r4,$r20
	addi.w		$r20,$r20,1
	addi.d		$r6,$r6,-1
	bnez		$r6,.Loop_tail_1x
	b			.Ldone_1x

.Ldone_1x:
	ld.d		$r23,$r3,0
	ld.d		$r24,$r3,8
	ld.d		$r25,$r3,16
	ld.d		$r26,$r3,24
	ld.d		$r27,$r3,32
	ld.d		$r28,$r3,40
	ld.d		$r29,$r3,48
	ld.d		$r30,$r3,56
	ld.d		$r31,$r3,64
	addi.d		$r3,$r3,256

	b			.Lend

.align 6
.LChaCha20_4x:
	addi.d		$r3,$r3,-128

	# Save the initial block counter in $r16
	ld.w		$r16,$r8,0
	b			.Loop_outer_4x

.align 5
.Loop_outer_4x:
	# Load constant
	la.local		$r20,.Lsigma
	vldrepl.w		$vr0,$r20,4*0		# 'expa'
	vldrepl.w		$vr1,$r20,4*1		# 'nd 3'
	vldrepl.w		$vr2,$r20,4*2		# '2-by'
	vldrepl.w		$vr3,$r20,4*3		# 'te k'

	# Load key
	vldrepl.w		$vr4,$r7,4*0
	vldrepl.w		$vr5,$r7,4*1
	vldrepl.w		$vr6,$r7,4*2
	vldrepl.w		$vr7,$r7,4*3
	vldrepl.w		$vr8,$r7,4*4
	vldrepl.w		$vr9,$r7,4*5
	vldrepl.w		$vr10,$r7,4*6
	vldrepl.w		$vr11,$r7,4*7

	# Load block counter
	vreplgr2vr.w	$vr12,$r16

	# Load nonce
	vldrepl.w		$vr13,$r8,4*1
	vldrepl.w		$vr14,$r8,4*2
	vldrepl.w		$vr15,$r8,4*3

	# Get the correct block counter for each block
	la.local		$r20,.Linc4x
	vld				$vr16,$r20,0
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
	ori				$r20,$r0,10
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

	addi.w		$r20,$r20,-1
	bnez		$r20,.Loop_4x

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
	ori			$r20,$r0,64*4
	bltu		$r6,$r20,.Ltail_4x

	# Get the encrypted message by xor states with plaintext
	vld			$vr16,$r5,16*0
	vld			$vr17,$r5,16*1
	vld			$vr18,$r5,16*2
	vld			$vr19,$r5,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$r4,16*0
	vst			$vr17,$r4,16*1
	vst			$vr18,$r4,16*2
	vst			$vr19,$r4,16*3

	vld			$vr16,$r5,16*4
	vld			$vr17,$r5,16*5
	vld			$vr18,$r5,16*6
	vld			$vr19,$r5,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$r4,16*4
	vst			$vr17,$r4,16*5
	vst			$vr18,$r4,16*6
	vst			$vr19,$r4,16*7

	vld			$vr16,$r5,16*8
	vld			$vr17,$r5,16*9
	vld			$vr18,$r5,16*10
	vld			$vr19,$r5,16*11
	vxor.v		$vr16,$vr16,$vr2
	vxor.v		$vr17,$vr17,$vr6
	vxor.v		$vr18,$vr18,$vr10
	vxor.v		$vr19,$vr19,$vr14
	vst			$vr16,$r4,16*8
	vst			$vr17,$r4,16*9
	vst			$vr18,$r4,16*10
	vst			$vr19,$r4,16*11

	vld			$vr16,$r5,16*12
	vld			$vr17,$r5,16*13
	vld			$vr18,$r5,16*14
	vld			$vr19,$r5,16*15
	vxor.v		$vr16,$vr16,$vr3
	vxor.v		$vr17,$vr17,$vr7
	vxor.v		$vr18,$vr18,$vr11
	vxor.v		$vr19,$vr19,$vr15
	vst			$vr16,$r4,16*12
	vst			$vr17,$r4,16*13
	vst			$vr18,$r4,16*14
	vst			$vr19,$r4,16*15

	addi.d		$r6,$r6,-64*4
	beqz		$r6,.Ldone_4x
	addi.d		$r5,$r5,64*4
	addi.d		$r4,$r4,64*4
	addi.w		$r16,$r16,4
	b			.Loop_outer_4x

.Ltail_4x:
	# Handle the tail for 4x (1 <= tail_len <= 255)
	ori			$r20,$r0,192
	bgeu		$r6,$r20,.L192_or_more4x
	ori			$r20,$r0,128
	bgeu		$r6,$r20,.L128_or_more4x
	ori			$r20,$r0,64
	bgeu		$r6,$r20,.L64_or_more4x

	vst			$vr0,$r3,16*0
	vst			$vr4,$r3,16*1
	vst			$vr8,$r3,16*2
	vst			$vr12,$r3,16*3
	move		$r20,$r0
	b			.Loop_tail_4x

.align 5
.L64_or_more4x:
	vld			$vr16,$r5,16*0
	vld			$vr17,$r5,16*1
	vld			$vr18,$r5,16*2
	vld			$vr19,$r5,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$r4,16*0
	vst			$vr17,$r4,16*1
	vst			$vr18,$r4,16*2
	vst			$vr19,$r4,16*3

	addi.d		$r6,$r6,-64
	beqz		$r6,.Ldone_4x
	addi.d		$r5,$r5,64
	addi.d		$r4,$r4,64
	vst			$vr1,$r3,16*0
	vst			$vr5,$r3,16*1
	vst			$vr9,$r3,16*2
	vst			$vr13,$r3,16*3
	move		$r20,$r0
	b			.Loop_tail_4x

.align 5
.L128_or_more4x:
	vld			$vr16,$r5,16*0
	vld			$vr17,$r5,16*1
	vld			$vr18,$r5,16*2
	vld			$vr19,$r5,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$r4,16*0
	vst			$vr17,$r4,16*1
	vst			$vr18,$r4,16*2
	vst			$vr19,$r4,16*3

	vld			$vr16,$r5,16*4
	vld			$vr17,$r5,16*5
	vld			$vr18,$r5,16*6
	vld			$vr19,$r5,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$r4,16*4
	vst			$vr17,$r4,16*5
	vst			$vr18,$r4,16*6
	vst			$vr19,$r4,16*7

	addi.d		$r6,$r6,-128
	beqz		$r6,.Ldone_4x
	addi.d		$r5,$r5,128
	addi.d		$r4,$r4,128
	vst			$vr2,$r3,16*0
	vst			$vr6,$r3,16*1
	vst			$vr10,$r3,16*2
	vst			$vr14,$r3,16*3
	move		$r20,$r0
	b			.Loop_tail_4x

.align 5
.L192_or_more4x:
	vld			$vr16,$r5,16*0
	vld			$vr17,$r5,16*1
	vld			$vr18,$r5,16*2
	vld			$vr19,$r5,16*3
	vxor.v		$vr16,$vr16,$vr0
	vxor.v		$vr17,$vr17,$vr4
	vxor.v		$vr18,$vr18,$vr8
	vxor.v		$vr19,$vr19,$vr12
	vst			$vr16,$r4,16*0
	vst			$vr17,$r4,16*1
	vst			$vr18,$r4,16*2
	vst			$vr19,$r4,16*3

	vld			$vr16,$r5,16*4
	vld			$vr17,$r5,16*5
	vld			$vr18,$r5,16*6
	vld			$vr19,$r5,16*7
	vxor.v		$vr16,$vr16,$vr1
	vxor.v		$vr17,$vr17,$vr5
	vxor.v		$vr18,$vr18,$vr9
	vxor.v		$vr19,$vr19,$vr13
	vst			$vr16,$r4,16*4
	vst			$vr17,$r4,16*5
	vst			$vr18,$r4,16*6
	vst			$vr19,$r4,16*7

	vld			$vr16,$r5,16*8
	vld			$vr17,$r5,16*9
	vld			$vr18,$r5,16*10
	vld			$vr19,$r5,16*11
	vxor.v		$vr16,$vr16,$vr2
	vxor.v		$vr17,$vr17,$vr6
	vxor.v		$vr18,$vr18,$vr10
	vxor.v		$vr19,$vr19,$vr14
	vst			$vr16,$r4,16*8
	vst			$vr17,$r4,16*9
	vst			$vr18,$r4,16*10
	vst			$vr19,$r4,16*11

	addi.d		$r6,$r6,-192
	beqz		$r6,.Ldone_4x
	addi.d		$r5,$r5,192
	addi.d		$r4,$r4,192
	vst			$vr3,$r3,16*0
	vst			$vr7,$r3,16*1
	vst			$vr11,$r3,16*2
	vst			$vr15,$r3,16*3
	move		$r20,$r0
	b			.Loop_tail_4x

.Loop_tail_4x:
	# Xor input with states byte by byte
	ldx.bu		$r17,$r5,$r20
	ldx.bu		$r18,$r3,$r20
	xor			$r17,$r17,$r18
	stx.b		$r17,$r4,$r20
	addi.w		$r20,$r20,1
	addi.d		$r6,$r6,-1
	bnez		$r6,.Loop_tail_4x
	b			.Ldone_4x

.Ldone_4x:
	addi.d		$r3,$r3,128
	b			.Lrestore_saved_fpr

.align 6
.LChaCha20_8x:
	addi.d		$r3,$r3,-128

	# Save the initial block counter in $r16
	ld.w		$r16,$r8,0
	b			.Loop_outer_8x

.align 5
.Loop_outer_8x:
	# Load constant
	la.local		$r20,.Lsigma
	xvldrepl.w		$xr0,$r20,4*0		# 'expa'
	xvldrepl.w		$xr1,$r20,4*1		# 'nd 3'
	xvldrepl.w		$xr2,$r20,4*2		# '2-by'
	xvldrepl.w		$xr3,$r20,4*3		# 'te k'

	# Load key
	xvldrepl.w		$xr4,$r7,4*0
	xvldrepl.w		$xr5,$r7,4*1
	xvldrepl.w		$xr6,$r7,4*2
	xvldrepl.w		$xr7,$r7,4*3
	xvldrepl.w		$xr8,$r7,4*4
	xvldrepl.w		$xr9,$r7,4*5
	xvldrepl.w		$xr10,$r7,4*6
	xvldrepl.w		$xr11,$r7,4*7

	# Load block counter
	xvreplgr2vr.w	$xr12,$r16

	# Load nonce
	xvldrepl.w		$xr13,$r8,4*1
	xvldrepl.w		$xr14,$r8,4*2
	xvldrepl.w		$xr15,$r8,4*3

	# Get the correct block counter for each block
	la.local		$r20,.Linc8x
	xvld			$xr16,$r20,0
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
	ori				$r20,$r0,10
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

	addi.w		$r20,$r20,-1
	bnez		$r20,.Loop_8x

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

	ori			$r20,$r0,64*8
	bltu		$r6,$r20,.Ltail_8x

	# Get the encrypted message by xor states with plaintext
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvld		$xr2,$r5,32*6
	xvld		$xr3,$r5,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5
	xvst		$xr2,$r4,32*6
	xvst		$xr3,$r4,32*7

	xvld		$xr0,$r5,32*8
	xvld		$xr1,$r5,32*9
	xvld		$xr2,$r5,32*10
	xvld		$xr3,$r5,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$r4,32*8
	xvst		$xr1,$r4,32*9
	xvst		$xr2,$r4,32*10
	xvst		$xr3,$r4,32*11

	xvld		$xr0,$r5,32*12
	xvld		$xr1,$r5,32*13
	xvld		$xr2,$r5,32*14
	xvld		$xr3,$r5,32*15
	xvxor.v		$xr0,$xr0,$xr22
	xvxor.v		$xr1,$xr1,$xr30
	xvxor.v		$xr2,$xr2,$xr23
	xvxor.v		$xr3,$xr3,$xr31
	xvst		$xr0,$r4,32*12
	xvst		$xr1,$r4,32*13
	xvst		$xr2,$r4,32*14
	xvst		$xr3,$r4,32*15

	addi.d		$r6,$r6,-64*8
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,64*8
	addi.d		$r4,$r4,64*8
	addi.w		$r16,$r16,8
	b			.Loop_outer_8x

.Ltail_8x:
	# Handle the tail for 8x (1 <= tail_len <= 511)
	ori			$r20,$r0,448
	bgeu		$r6,$r20,.L448_or_more8x
	ori			$r20,$r0,384
	bgeu		$r6,$r20,.L384_or_more8x
	ori			$r20,$r0,320
	bgeu		$r6,$r20,.L320_or_more8x
	ori			$r20,$r0,256
	bgeu		$r6,$r20,.L256_or_more8x
	ori			$r20,$r0,192
	bgeu		$r6,$r20,.L192_or_more8x
	ori			$r20,$r0,128
	bgeu		$r6,$r20,.L128_or_more8x
	ori			$r20,$r0,64
	bgeu		$r6,$r20,.L64_or_more8x

	xvst		$xr16,$r3,32*0
	xvst		$xr24,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L64_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1

	addi.d		$r6,$r6,-64
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,64
	addi.d		$r4,$r4,64
	xvst		$xr17,$r3,32*0
	xvst		$xr25,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L128_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	addi.d		$r6,$r6,-128
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,128
	addi.d		$r4,$r4,128
	xvst		$xr18,$r3,32*0
	xvst		$xr26,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L192_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5

	addi.d		$r6,$r6,-192
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,192
	addi.d		$r4,$r4,192
	xvst		$xr19,$r3,32*0
	xvst		$xr27,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L256_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvld		$xr2,$r5,32*6
	xvld		$xr3,$r5,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5
	xvst		$xr2,$r4,32*6
	xvst		$xr3,$r4,32*7

	addi.d		$r6,$r6,-256
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,256
	addi.d		$r4,$r4,256
	xvst		$xr20,$r3,32*0
	xvst		$xr28,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L320_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvld		$xr2,$r5,32*6
	xvld		$xr3,$r5,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5
	xvst		$xr2,$r4,32*6
	xvst		$xr3,$r4,32*7

	xvld		$xr0,$r5,32*8
	xvld		$xr1,$r5,32*9
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvst		$xr0,$r4,32*8
	xvst		$xr1,$r4,32*9

	addi.d		$r6,$r6,-320
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,320
	addi.d		$r4,$r4,320
	xvst		$xr21,$r3,32*0
	xvst		$xr29,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L384_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvld		$xr2,$r5,32*6
	xvld		$xr3,$r5,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5
	xvst		$xr2,$r4,32*6
	xvst		$xr3,$r4,32*7

	xvld		$xr0,$r5,32*8
	xvld		$xr1,$r5,32*9
	xvld		$xr2,$r5,32*10
	xvld		$xr3,$r5,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$r4,32*8
	xvst		$xr1,$r4,32*9
	xvst		$xr2,$r4,32*10
	xvst		$xr3,$r4,32*11

	addi.d		$r6,$r6,-384
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,384
	addi.d		$r4,$r4,384
	xvst		$xr22,$r3,32*0
	xvst		$xr30,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.align 5
.L448_or_more8x:
	xvld		$xr0,$r5,32*0
	xvld		$xr1,$r5,32*1
	xvld		$xr2,$r5,32*2
	xvld		$xr3,$r5,32*3
	xvxor.v		$xr0,$xr0,$xr16
	xvxor.v		$xr1,$xr1,$xr24
	xvxor.v		$xr2,$xr2,$xr17
	xvxor.v		$xr3,$xr3,$xr25
	xvst		$xr0,$r4,32*0
	xvst		$xr1,$r4,32*1
	xvst		$xr2,$r4,32*2
	xvst		$xr3,$r4,32*3

	xvld		$xr0,$r5,32*4
	xvld		$xr1,$r5,32*5
	xvld		$xr2,$r5,32*6
	xvld		$xr3,$r5,32*7
	xvxor.v		$xr0,$xr0,$xr18
	xvxor.v		$xr1,$xr1,$xr26
	xvxor.v		$xr2,$xr2,$xr19
	xvxor.v		$xr3,$xr3,$xr27
	xvst		$xr0,$r4,32*4
	xvst		$xr1,$r4,32*5
	xvst		$xr2,$r4,32*6
	xvst		$xr3,$r4,32*7

	xvld		$xr0,$r5,32*8
	xvld		$xr1,$r5,32*9
	xvld		$xr2,$r5,32*10
	xvld		$xr3,$r5,32*11
	xvxor.v		$xr0,$xr0,$xr20
	xvxor.v		$xr1,$xr1,$xr28
	xvxor.v		$xr2,$xr2,$xr21
	xvxor.v		$xr3,$xr3,$xr29
	xvst		$xr0,$r4,32*8
	xvst		$xr1,$r4,32*9
	xvst		$xr2,$r4,32*10
	xvst		$xr3,$r4,32*11

	xvld		$xr0,$r5,32*12
	xvld		$xr1,$r5,32*13
	xvxor.v		$xr0,$xr0,$xr22
	xvxor.v		$xr1,$xr1,$xr30
	xvst		$xr0,$r4,32*12
	xvst		$xr1,$r4,32*13

	addi.d		$r6,$r6,-448
	beqz		$r6,.Ldone_8x
	addi.d		$r5,$r5,448
	addi.d		$r4,$r4,448
	xvst		$xr23,$r3,32*0
	xvst		$xr31,$r3,32*1
	move		$r20,$r0
	b			.Loop_tail_8x

.Loop_tail_8x:
	# Xor input with states byte by byte
	ldx.bu		$r17,$r5,$r20
	ldx.bu		$r18,$r3,$r20
	xor			$r17,$r17,$r18
	stx.b		$r17,$r4,$r20
	addi.w		$r20,$r20,1
	addi.d		$r6,$r6,-1
	bnez		$r6,.Loop_tail_8x
	b			.Ldone_8x

.Ldone_8x:
	addi.d		$r3,$r3,128
	b			.Lrestore_saved_fpr

.Lrestore_saved_fpr:
	fld.d		$f24,$r3,0
	fld.d		$f25,$r3,8
	fld.d		$f26,$r3,16
	fld.d		$f27,$r3,24
	fld.d		$f28,$r3,32
	fld.d		$f29,$r3,40
	fld.d		$f30,$r3,48
	fld.d		$f31,$r3,56
	addi.d		$r3,$r3,64
.Lno_data:
.Lend:
	jr	$r1
.size ChaCha20_ctr32,.-ChaCha20_ctr32
