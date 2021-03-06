

















































































.globl	bn_sqr_comba4
.type	bn_sqr_comba4,@function
.globl	bn_sqr_comba8
.type	bn_sqr_comba8,@function
.globl	bn_mul_comba4
.type	bn_mul_comba4,@function
.globl	bn_mul_comba8
.type	bn_mul_comba8,@function
.globl	bn_sub_words
.type	bn_sub_words,@function
.globl	bn_add_words
.type	bn_add_words,@function
.globl	bn_div_words
.type	bn_div_words,@function
.globl	bn_sqr_words
.type	bn_sqr_words,@function
.globl	bn_mul_words
.type	bn_mul_words,@function
.globl	bn_mul_add_words
.type	bn_mul_add_words,@function



.machine	"any"
.text








.align	4
bn_sqr_comba4:















	xor	0,0,0



	lwz	5,0(4)
	mullw	9,5,5
	mulhwu	10,5,5




	stw	9,0(3)

	lwz	6,4(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	9,0


	addc	10,7,10
	addze	11,8
	addze	9,9

	stw	10,4(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	lwz	6,8(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	stw	11,8(3)

	lwz	6,12(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	11,0

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,4(4)
	lwz	6,8(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	stw	9,12(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0

	lwz	6,12(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	stw	10,16(3)

	lwz	5,8(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	10,0

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	stw	11,20(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	9,7,9
	adde	10,8,10

	stw	9,24(3)
	stw	10,28(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	bn_sqr_comba4,.-bn_sqr_comba4








.align	4
bn_sqr_comba8:



















	xor	0,0,0



	lwz	5,0(4)
	mullw	9,5,5
	mulhwu	10,5,5
	stw	9,0(3)

	lwz	6,4(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,0
	addze	9,0

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	stw	10,4(3)


	mullw	7,6,6
	mulhwu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	lwz	6,8(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	stw	11,8(3)

	lwz	6,12(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,0

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,4(4)
	lwz	6,8(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	stw	9,12(3)

	mullw	7,6,6
	mulhwu	8,6,6

	addc	10,7,10
	adde	11,8,11
	addze	9,0

	lwz	6,12(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	lwz	5,0(4)
	lwz	6,16(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	stw	10,16(3)

	lwz	6,20(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,0

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	lwz	5,4(4)
	lwz	6,16(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	lwz	5,8(4)
	lwz	6,12(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	stw	11,20(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	9,7,9
	adde	10,8,10
	addze	11,0

	lwz	6,16(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,4(4)
	lwz	6,20(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,0(4)
	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	stw	9,24(3)

	lwz	6,28(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,0
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	lwz	5,4(4)
	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	lwz	5,8(4)
	lwz	6,20(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	lwz	5,12(4)
	lwz	6,16(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	stw	10,28(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	lwz	6,20(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	lwz	5,8(4)
	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	lwz	5,4(4)
	lwz	6,28(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	stw	11,32(3)

	lwz	5,8(4)
	mullw	7,5,6
	mulhwu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,0
	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,12(4)
	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11

	lwz	5,16(4)
	lwz	6,20(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	stw	9,36(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0

	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	lwz	5,12(4)
	lwz	6,28(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	stw	10,40(3)

	lwz	5,16(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	lwz	5,20(4)
	lwz	6,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	stw	11,44(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	9,7,9
	adde	10,8,10
	addze	11,0

	lwz	6,28(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	stw	9,48(3)


	lwz	5,24(4)
	mullw	7,5,6
	mulhwu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	stw	10,52(3)

	mullw	7,6,6
	mulhwu	8,6,6
	addc	11,7,11
	adde	9,8,9
	stw	11,56(3)
	stw	9, 60(3)


	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	bn_sqr_comba8,.-bn_sqr_comba8








.align	4
bn_mul_comba4:











	xor	0,0,0

	lwz	6,0(4)
	lwz	7,0(5)
	mullw	10,6,7
	mulhwu	11,6,7
	stw	10,0(3)

	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,8,11
	adde	12,9,0
	addze	10,0

	lwz	6, 4(4)
	lwz	7, 0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10
	stw	11,4(3)

	lwz	6,8(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,0

	lwz	6,4(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11

	lwz	6,0(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11
	stw	12,8(3)

	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,0

	lwz	6,4(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12

	lwz	6,8(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12

	lwz	6,12(4)
	lwz	7,0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12
	stw	10,12(3)

	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,0

	lwz	6,8(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10

	lwz	6,4(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10
	stw	11,16(3)

	lwz	6,8(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,0

	lwz	6,12(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11
	stw	12,20(3)

	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,8,10
	adde	11,9,11

	stw	10,24(3)
	stw	11,28(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_mul_comba4,.-bn_mul_comba4








.align	4
bn_mul_comba8:











	xor	0,0,0


	lwz	6,0(4)
	lwz	7,0(5)
	mullw	10,6,7
	mulhwu	11,6,7
	stw	10,0(3)

	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	addze	12,9
	addze	10,0

	lwz	6,4(4)
	lwz	7,0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	stw	11,4(3)

	lwz	6,8(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	lwz	6,4(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,0(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	stw	12,8(3)

	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	lwz	6,4(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12


	lwz	6,8(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,12(4)
	lwz	7,0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	stw	10,12(3)

	lwz	6,16(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	lwz	6,12(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,8(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,4(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,0(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	stw	11,16(3)

	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	lwz	6,4(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,8(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,12(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,16(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,20(4)
	lwz	7,0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	stw	12,20(3)

	lwz	6,24(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	lwz	6,20(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,16(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,12(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,8(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,4(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,0(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	stw	10,24(3)

	lwz	7,28(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	lwz	6,4(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,8(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,12(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,16(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,20(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,24(4)
	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,28(4)
	lwz	7,0(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	stw	11,28(3)

	lwz	7,4(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	lwz	6,24(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,20(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,16(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,12(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,8(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,4(4)
	lwz	7,28(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	stw	12,32(3)

	lwz	6,8(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	lwz	6,12(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,16(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,20(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,24(4)
	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,28(4)
	lwz	7,8(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	stw	10,36(3)

	lwz	7,12(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	lwz	6,24(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,20(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,16(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	lwz	6,12(4)
	lwz	7,28(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	stw	11,40(3)

	lwz	6,16(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	lwz	6,20(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,24(4)
	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	lwz	6,28(4)
	lwz	7,16(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	stw	12,44(3)

	lwz	7,20(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	lwz	6,24(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	lwz	6,20(4)
	lwz	7,28(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	stw	10,48(3)

	lwz	6,24(4)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	lwz	6,28(4)
	lwz	7,24(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	stw	11,52(3)

	lwz	7,28(5)
	mullw	8,6,7
	mulhwu	9,6,7
	addc	12,12,8
	adde	10,10,9
	stw	12,56(3)
	stw	10,60(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_mul_comba8,.-bn_mul_comba8








.align	4
bn_sub_words:













	xor	0,0,0



	subfc.	7,0,6


	beq	.Lppcasm_sub_adios
	addi	4,4,-4
	addi	3,3,-4
	addi	5,5,-4
	mtctr	6
.Lppcasm_sub_mainloop:
	lwzu	7,4(4)
	lwzu	8,4(5)
	subfe	6,8,7


	stwu	6,4(3)
	bdnz	.Lppcasm_sub_mainloop
.Lppcasm_sub_adios:
	subfze	3,0
	andi.	3,3,1
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.size	bn_sub_words,.-bn_sub_words








.align	4
bn_add_words:













	xor	0,0,0



	addic.	6,6,0
	beq	.Lppcasm_add_adios
	addi	4,4,-4
	addi	3,3,-4
	addi	5,5,-4
	mtctr	6
.Lppcasm_add_mainloop:
	lwzu	7,4(4)
	lwzu	8,4(5)
	adde	8,7,8
	stwu	8,4(3)
	bdnz	.Lppcasm_add_mainloop
.Lppcasm_add_adios:
	addze	3,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.size	bn_add_words,.-bn_add_words








.align	4
bn_div_words:











	cmplwi	0,5,0
	bne	.Lppcasm_div1
	li	3,-1
	blr	
.Lppcasm_div1:
	xor	0,0,0
	li	8,32
	cntlzw.	7,5
	beq	.Lppcasm_div2
	subf	8,7,8
	srw.	9,3,8
	tw	16,9,0
.Lppcasm_div2:
	.long	0x7c032840
	blt	.Lppcasm_div3
	subf	3,5,3
.Lppcasm_div3:
	cmpi	0,0,7,0
	beq	.Lppcasm_div4
	slw	3,3,7
	srw	8,4,8
	slw	5,5,7
	or	3,3,8
	slw	4,4,7
.Lppcasm_div4:
	srwi	9,5,16


	li	6,2
	mtctr	6
.Lppcasm_divouterloop:
	srwi	8,3,16
	srwi	11,4,16

	.long	0x7c084840
	bne	.Lppcasm_div5

	li	8,-1
	clrlwi	8,8,16
	b	.Lppcasm_div6
.Lppcasm_div5:
	divwu	8,3,9
.Lppcasm_div6:
	mullw	12,9,8
	clrlwi	10,5,16
	mullw	6,8,10

.Lppcasm_divinnerloop:
	subf	10,12,3
	srwi	7,10,16
	addic.	7,7,0



	slwi	7,10,16
	or	7,7,11
	.long	0x7c863840
	bne	.Lppcasm_divinnerexit
	ble	1,.Lppcasm_divinnerexit
	addi	8,8,-1
	subf	12,9,12
	clrlwi	10,5,16
	subf	6,10,6
	b	.Lppcasm_divinnerloop
.Lppcasm_divinnerexit:
	srwi	10,6,16
	slwi	11,6,16
	.long	0x7c845840
	add	12,12,10
	bge	1,.Lppcasm_div7
	addi	12,12,1
.Lppcasm_div7:
	subf	11,11,4
	.long	0x7c836040
	bge	1,.Lppcasm_div8
	addi	8,8,-1
	add	3,5,3
.Lppcasm_div8:
	subf	12,12,3
	slwi	4,11,16



	insrwi	11,12,16,16
	rotlwi	3,11,16
	bdz	.Lppcasm_div9
	slwi	0,8,16
	b	.Lppcasm_divouterloop
.Lppcasm_div9:
	or	3,8,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_div_words,.-bn_div_words







.align	4
bn_sqr_words:














	addic.	5,5,0
	beq	.Lppcasm_sqr_adios
	addi	4,4,-4
	addi	3,3,-4
	mtctr	5
.Lppcasm_sqr_mainloop:

	lwzu	6,4(4)
	mullw	7,6,6
	mulhwu	8,6,6
	stwu	7,4(3)
	stwu	8,4(3)
	bdnz	.Lppcasm_sqr_mainloop
.Lppcasm_sqr_adios:
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_sqr_words,.-bn_sqr_words








.align	4
bn_mul_words:







	xor	0,0,0
	xor	12,12,12
	rlwinm.	7,5,30,2,31
	beq	.Lppcasm_mw_REM
	mtctr	7
.Lppcasm_mw_LOOP:

	lwz	8,0(4)
	mullw	9,6,8
	mulhwu	10,6,8
	addc	9,9,12




	stw	9,0(3)

	lwz	8,4(4)
	mullw	11,6,8
	mulhwu	12,6,8
	adde	11,11,10

	stw	11,4(3)

	lwz	8,8(4)
	mullw	9,6,8
	mulhwu	10,6,8
	adde	9,9,12

	stw	9,8(3)

	lwz	8,12(4)
	mullw	11,6,8
	mulhwu	12,6,8
	adde	11,11,10
	addze	12,12

	stw	11,12(3)

	addi	3,3,16
	addi	4,4,16
	bdnz	.Lppcasm_mw_LOOP

.Lppcasm_mw_REM:
	andi.	5,5,0x3
	beq	.Lppcasm_mw_OVER

	lwz	8,0(4)
	mullw	9,6,8
	mulhwu	10,6,8
	addc	9,9,12
	addze	10,10
	stw	9,0(3)
	addi	12,10,0

	addi	5,5,-1
	cmpli	0,0,5,0
	beq	.Lppcasm_mw_OVER



	lwz	8,4(4)
	mullw	9,6,8
	mulhwu	10,6,8
	addc	9,9,12
	addze	10,10
	stw	9,4(3)
	addi	12,10,0

	addi	5,5,-1
	cmpli	0,0,5,0
	beq	.Lppcasm_mw_OVER


	lwz	8,8(4)
	mullw	9,6,8
	mulhwu	10,6,8
	addc	9,9,12
	addze	10,10
	stw	9,8(3)
	addi	12,10,0

.Lppcasm_mw_OVER:
	addi	3,12,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.size	bn_mul_words,.-bn_mul_words








.align	4
bn_mul_add_words:










	xor	0,0,0
	xor	12,12,12
	rlwinm.	7,5,30,2,31
	beq	.Lppcasm_maw_leftover
	mtctr	7
.Lppcasm_maw_mainloop:

	lwz	8,0(4)
	lwz	11,0(3)
	mullw	9,6,8
	mulhwu	10,6,8
	addc	9,9,12
	addze	10,10
	addc	9,9,11






	stw	9,0(3)


	lwz	8,4(4)
	lwz	9,4(3)
	mullw	11,6,8
	mulhwu	12,6,8
	adde	11,11,10
	addze	12,12
	addc	11,11,9

	stw	11,4(3)


	lwz	8,8(4)
	mullw	9,6,8
	lwz	11,8(3)
	mulhwu	10,6,8
	adde	9,9,12
	addze	10,10
	addc	9,9,11

	stw	9,8(3)


	lwz	8,12(4)
	mullw	11,6,8
	lwz	9,12(3)
	mulhwu	12,6,8
	adde	11,11,10
	addze	12,12
	addc	11,11,9
	addze	12,12
	stw	11,12(3)
	addi	3,3,16
	addi	4,4,16
	bdnz	.Lppcasm_maw_mainloop

.Lppcasm_maw_leftover:
	andi.	5,5,0x3
	beq	.Lppcasm_maw_adios
	addi	3,3,-4
	addi	4,4,-4

	mtctr	5
	lwzu	8,4(4)
	mullw	9,6,8
	mulhwu	10,6,8
	lwzu	11,4(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	stw	9,0(3)

	bdz	.Lppcasm_maw_adios

	lwzu	8,4(4)
	mullw	9,6,8
	mulhwu	10,6,8
	lwzu	11,4(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	stw	9,0(3)

	bdz	.Lppcasm_maw_adios

	lwzu	8,4(4)
	mullw	9,6,8
	mulhwu	10,6,8
	lwzu	11,4(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	stw	9,0(3)

.Lppcasm_maw_adios:
	addi	3,12,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.size	bn_mul_add_words,.-bn_mul_add_words
.align	4
