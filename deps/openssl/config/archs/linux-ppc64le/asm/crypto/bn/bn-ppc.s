

















































































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
.abiversion	2
.text








.align	4
bn_sqr_comba4:
.localentry	bn_sqr_comba4,0
















	xor	0,0,0



	ld	5,0(4)
	mulld	9,5,5
	mulhdu	10,5,5




	std	9,0(3)

	ld	6,8(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	9,0


	addc	10,7,10
	addze	11,8
	addze	9,9

	std	10,8(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	ld	6,16(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	std	11,16(3)

	ld	6,24(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	11,0

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,8(4)
	ld	6,16(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	7,7,7
	adde	8,8,8
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	std	9,24(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0

	ld	6,24(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	std	10,32(3)

	ld	5,16(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	7,7,7
	adde	8,8,8
	addze	10,0

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	std	11,40(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	9,7,9
	adde	10,8,10

	std	9,48(3)
	std	10,56(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	bn_sqr_comba4,.-bn_sqr_comba4








.align	4
bn_sqr_comba8:
.localentry	bn_sqr_comba8,0




















	xor	0,0,0



	ld	5,0(4)
	mulld	9,5,5
	mulhdu	10,5,5
	std	9,0(3)

	ld	6,8(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,0
	addze	9,0

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	std	10,8(3)


	mulld	7,6,6
	mulhdu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	ld	6,16(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	std	11,16(3)

	ld	6,24(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,0

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,8(4)
	ld	6,16(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	std	9,24(3)

	mulld	7,6,6
	mulhdu	8,6,6

	addc	10,7,10
	adde	11,8,11
	addze	9,0

	ld	6,24(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	ld	5,0(4)
	ld	6,32(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	std	10,32(3)

	ld	6,40(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,0

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	ld	5,8(4)
	ld	6,32(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	ld	5,16(4)
	ld	6,24(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10
	std	11,40(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	9,7,9
	adde	10,8,10
	addze	11,0

	ld	6,32(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,8(4)
	ld	6,40(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,0(4)
	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	std	9,48(3)

	ld	6,56(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,0
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	ld	5,8(4)
	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	ld	5,16(4)
	ld	6,40(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	ld	5,24(4)
	ld	6,32(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	std	10,56(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0

	ld	6,40(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	ld	5,16(4)
	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	addc	11,7,11
	adde	9,8,9
	addze	10,10

	ld	5,8(4)
	ld	6,56(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	std	11,64(3)

	ld	5,16(4)
	mulld	7,5,6
	mulhdu	8,5,6

	addc	9,7,9
	adde	10,8,10
	addze	11,0
	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,24(4)
	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11

	ld	5,32(4)
	ld	6,40(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	std	9,72(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0

	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9

	ld	5,24(4)
	ld	6,56(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	std	10,80(3)

	ld	5,32(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,0
	addc	11,7,11
	adde	9,8,9
	addze	10,10

	ld	5,40(4)
	ld	6,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	addc	11,7,11
	adde	9,8,9
	addze	10,10
	std	11,88(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	9,7,9
	adde	10,8,10
	addze	11,0

	ld	6,56(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	addc	9,7,9
	adde	10,8,10
	addze	11,11
	std	9,96(3)


	ld	5,48(4)
	mulld	7,5,6
	mulhdu	8,5,6
	addc	10,7,10
	adde	11,8,11
	addze	9,0
	addc	10,7,10
	adde	11,8,11
	addze	9,9
	std	10,104(3)

	mulld	7,6,6
	mulhdu	8,6,6
	addc	11,7,11
	adde	9,8,9
	std	11,112(3)
	std	9, 120(3)


	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	bn_sqr_comba8,.-bn_sqr_comba8








.align	4
bn_mul_comba4:
.localentry	bn_mul_comba4,0












	xor	0,0,0

	ld	6,0(4)
	ld	7,0(5)
	mulld	10,6,7
	mulhdu	11,6,7
	std	10,0(3)

	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,8,11
	adde	12,9,0
	addze	10,0

	ld	6, 8(4)
	ld	7, 0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10
	std	11,8(3)

	ld	6,16(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,0

	ld	6,8(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11

	ld	6,0(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11
	std	12,16(3)

	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,0

	ld	6,8(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12

	ld	6,16(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12

	ld	6,24(4)
	ld	7,0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,8,10
	adde	11,9,11
	addze	12,12
	std	10,24(3)

	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,0

	ld	6,16(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10

	ld	6,8(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,8,11
	adde	12,9,12
	addze	10,10
	std	11,32(3)

	ld	6,16(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,0

	ld	6,24(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,8,12
	adde	10,9,10
	addze	11,11
	std	12,40(3)

	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,8,10
	adde	11,9,11

	std	10,48(3)
	std	11,56(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_mul_comba4,.-bn_mul_comba4








.align	4
bn_mul_comba8:
.localentry	bn_mul_comba8,0












	xor	0,0,0


	ld	6,0(4)
	ld	7,0(5)
	mulld	10,6,7
	mulhdu	11,6,7
	std	10,0(3)

	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	addze	12,9
	addze	10,0

	ld	6,8(4)
	ld	7,0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	std	11,8(3)

	ld	6,16(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	ld	6,8(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,0(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	std	12,16(3)

	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	ld	6,8(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12


	ld	6,16(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,24(4)
	ld	7,0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	std	10,24(3)

	ld	6,32(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	ld	6,24(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,16(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,8(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,0(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	std	11,32(3)

	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	ld	6,8(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,16(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,24(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,32(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,40(4)
	ld	7,0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	std	12,40(3)

	ld	6,48(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	ld	6,40(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,32(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,24(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,16(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,8(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,0(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	std	10,48(3)

	ld	7,56(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	ld	6,8(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,16(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,24(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,32(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,40(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,48(4)
	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,56(4)
	ld	7,0(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	std	11,56(3)

	ld	7,8(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	ld	6,48(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,40(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,32(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,24(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,16(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,8(4)
	ld	7,56(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	std	12,64(3)

	ld	6,16(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	ld	6,24(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,32(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,40(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,48(4)
	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,56(4)
	ld	7,16(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	std	10,72(3)

	ld	7,24(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	ld	6,48(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,40(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,32(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10

	ld	6,24(4)
	ld	7,56(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	std	11,80(3)

	ld	6,32(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,0

	ld	6,40(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,48(4)
	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11

	ld	6,56(4)
	ld	7,32(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	addze	11,11
	std	12,88(3)

	ld	7,40(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,0

	ld	6,48(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12

	ld	6,40(4)
	ld	7,56(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	10,10,8
	adde	11,11,9
	addze	12,12
	std	10,96(3)

	ld	6,48(4)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,0

	ld	6,56(4)
	ld	7,48(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	11,11,8
	adde	12,12,9
	addze	10,10
	std	11,104(3)

	ld	7,56(5)
	mulld	8,6,7
	mulhdu	9,6,7
	addc	12,12,8
	adde	10,10,9
	std	12,112(3)
	std	10,120(3)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_mul_comba8,.-bn_mul_comba8








.align	4
bn_sub_words:
.localentry	bn_sub_words,0














	xor	0,0,0



	subfc.	7,0,6


	beq	.Lppcasm_sub_adios
	addi	4,4,-8
	addi	3,3,-8
	addi	5,5,-8
	mtctr	6
.Lppcasm_sub_mainloop:
	ldu	7,8(4)
	ldu	8,8(5)
	subfe	6,8,7


	stdu	6,8(3)
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
.localentry	bn_add_words,0














	xor	0,0,0



	addic.	6,6,0
	beq	.Lppcasm_add_adios
	addi	4,4,-8
	addi	3,3,-8
	addi	5,5,-8
	mtctr	6
.Lppcasm_add_mainloop:
	ldu	7,8(4)
	ldu	8,8(5)
	adde	8,7,8
	stdu	8,8(3)
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
.localentry	bn_div_words,0












	cmpldi	0,5,0
	bne	.Lppcasm_div1
	li	3,-1
	blr	
.Lppcasm_div1:
	xor	0,0,0
	li	8,64
	cntlzd.	7,5
	beq	.Lppcasm_div2
	subf	8,7,8
	srd.	9,3,8
	td	16,9,0
.Lppcasm_div2:
	cmpld	0,3,5
	blt	.Lppcasm_div3
	subf	3,5,3
.Lppcasm_div3:
	cmpi	0,0,7,0
	beq	.Lppcasm_div4
	sld	3,3,7
	srd	8,4,8
	sld	5,5,7
	or	3,3,8
	sld	4,4,7
.Lppcasm_div4:
	srdi	9,5,32


	li	6,2
	mtctr	6
.Lppcasm_divouterloop:
	srdi	8,3,32
	srdi	11,4,32

	cmpld	0,8,9
	bne	.Lppcasm_div5

	li	8,-1
	clrldi	8,8,32
	b	.Lppcasm_div6
.Lppcasm_div5:
	divdu	8,3,9
.Lppcasm_div6:
	mulld	12,9,8
	clrldi	10,5,32
	mulld	6,8,10

.Lppcasm_divinnerloop:
	subf	10,12,3
	srdi	7,10,32
	addic.	7,7,0



	sldi	7,10,32
	or	7,7,11
	cmpld	1,6,7
	bne	.Lppcasm_divinnerexit
	ble	1,.Lppcasm_divinnerexit
	addi	8,8,-1
	subf	12,9,12
	clrldi	10,5,32
	subf	6,10,6
	b	.Lppcasm_divinnerloop
.Lppcasm_divinnerexit:
	srdi	10,6,32
	sldi	11,6,32
	cmpld	1,4,11
	add	12,12,10
	bge	1,.Lppcasm_div7
	addi	12,12,1
.Lppcasm_div7:
	subf	11,11,4
	cmpld	1,3,12
	bge	1,.Lppcasm_div8
	addi	8,8,-1
	add	3,5,3
.Lppcasm_div8:
	subf	12,12,3
	sldi	4,11,32



	insrdi	11,12,32,32
	rotldi	3,11,32
	bdz	.Lppcasm_div9
	sldi	0,8,32
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
.localentry	bn_sqr_words,0















	addic.	5,5,0
	beq	.Lppcasm_sqr_adios
	addi	4,4,-8
	addi	3,3,-8
	mtctr	5
.Lppcasm_sqr_mainloop:

	ldu	6,8(4)
	mulld	7,6,6
	mulhdu	8,6,6
	stdu	7,8(3)
	stdu	8,8(3)
	bdnz	.Lppcasm_sqr_mainloop
.Lppcasm_sqr_adios:
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.long	0
.size	bn_sqr_words,.-bn_sqr_words








.align	4
bn_mul_words:
.localentry	bn_mul_words,0








	xor	0,0,0
	xor	12,12,12
	rlwinm.	7,5,30,2,31
	beq	.Lppcasm_mw_REM
	mtctr	7
.Lppcasm_mw_LOOP:

	ld	8,0(4)
	mulld	9,6,8
	mulhdu	10,6,8
	addc	9,9,12




	std	9,0(3)

	ld	8,8(4)
	mulld	11,6,8
	mulhdu	12,6,8
	adde	11,11,10

	std	11,8(3)

	ld	8,16(4)
	mulld	9,6,8
	mulhdu	10,6,8
	adde	9,9,12

	std	9,16(3)

	ld	8,24(4)
	mulld	11,6,8
	mulhdu	12,6,8
	adde	11,11,10
	addze	12,12

	std	11,24(3)

	addi	3,3,32
	addi	4,4,32
	bdnz	.Lppcasm_mw_LOOP

.Lppcasm_mw_REM:
	andi.	5,5,0x3
	beq	.Lppcasm_mw_OVER

	ld	8,0(4)
	mulld	9,6,8
	mulhdu	10,6,8
	addc	9,9,12
	addze	10,10
	std	9,0(3)
	addi	12,10,0

	addi	5,5,-1
	cmpli	0,0,5,0
	beq	.Lppcasm_mw_OVER



	ld	8,8(4)
	mulld	9,6,8
	mulhdu	10,6,8
	addc	9,9,12
	addze	10,10
	std	9,8(3)
	addi	12,10,0

	addi	5,5,-1
	cmpli	0,0,5,0
	beq	.Lppcasm_mw_OVER


	ld	8,16(4)
	mulld	9,6,8
	mulhdu	10,6,8
	addc	9,9,12
	addze	10,10
	std	9,16(3)
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
.localentry	bn_mul_add_words,0











	xor	0,0,0
	xor	12,12,12
	rlwinm.	7,5,30,2,31
	beq	.Lppcasm_maw_leftover
	mtctr	7
.Lppcasm_maw_mainloop:

	ld	8,0(4)
	ld	11,0(3)
	mulld	9,6,8
	mulhdu	10,6,8
	addc	9,9,12
	addze	10,10
	addc	9,9,11






	std	9,0(3)


	ld	8,8(4)
	ld	9,8(3)
	mulld	11,6,8
	mulhdu	12,6,8
	adde	11,11,10
	addze	12,12
	addc	11,11,9

	std	11,8(3)


	ld	8,16(4)
	mulld	9,6,8
	ld	11,16(3)
	mulhdu	10,6,8
	adde	9,9,12
	addze	10,10
	addc	9,9,11

	std	9,16(3)


	ld	8,24(4)
	mulld	11,6,8
	ld	9,24(3)
	mulhdu	12,6,8
	adde	11,11,10
	addze	12,12
	addc	11,11,9
	addze	12,12
	std	11,24(3)
	addi	3,3,32
	addi	4,4,32
	bdnz	.Lppcasm_maw_mainloop

.Lppcasm_maw_leftover:
	andi.	5,5,0x3
	beq	.Lppcasm_maw_adios
	addi	3,3,-8
	addi	4,4,-8

	mtctr	5
	ldu	8,8(4)
	mulld	9,6,8
	mulhdu	10,6,8
	ldu	11,8(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	std	9,0(3)

	bdz	.Lppcasm_maw_adios

	ldu	8,8(4)
	mulld	9,6,8
	mulhdu	10,6,8
	ldu	11,8(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	std	9,0(3)

	bdz	.Lppcasm_maw_adios

	ldu	8,8(4)
	mulld	9,6,8
	mulhdu	10,6,8
	ldu	11,8(3)
	addc	9,9,11
	addze	10,10
	addc	9,9,12
	addze	12,10
	std	9,0(3)

.Lppcasm_maw_adios:
	addi	3,12,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.size	bn_mul_add_words,.-bn_mul_add_words
.align	4
