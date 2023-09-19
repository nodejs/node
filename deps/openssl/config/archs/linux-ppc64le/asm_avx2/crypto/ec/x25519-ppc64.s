.abiversion	2
.text

.globl	x25519_fe51_mul
.type	x25519_fe51_mul,@function
.type	x25519_fe51_mul,@function
.align	5
x25519_fe51_mul:
.localentry	x25519_fe51_mul,0

	stdu	1,-144(1)
	std	21,56(1)
	std	22,64(1)
	std	23,72(1)
	std	24,80(1)
	std	25,88(1)
	std	26,96(1)
	std	27,104(1)
	std	28,112(1)
	std	29,120(1)
	std	30,128(1)
	std	31,136(1)

	ld	6,0(5)
	ld	7,0(4)
	ld	8,8(4)
	ld	9,16(4)
	ld	10,24(4)
	ld	11,32(4)

	mulld	22,7,6
	mulhdu	23,7,6

	mulld	24,8,6
	mulhdu	25,8,6

	mulld	30,11,6
	mulhdu	31,11,6
	ld	4,8(5)
	mulli	11,11,19

	mulld	26,9,6
	mulhdu	27,9,6

	mulld	28,10,6
	mulhdu	29,10,6
	mulld	12,11,4
	mulhdu	21,11,4
	addc	22,22,12
	adde	23,23,21

	mulld	12,7,4
	mulhdu	21,7,4
	addc	24,24,12
	adde	25,25,21

	mulld	12,10,4
	mulhdu	21,10,4
	ld	6,16(5)
	mulli	10,10,19
	addc	30,30,12
	adde	31,31,21

	mulld	12,8,4
	mulhdu	21,8,4
	addc	26,26,12
	adde	27,27,21

	mulld	12,9,4
	mulhdu	21,9,4
	addc	28,28,12
	adde	29,29,21
	mulld	12,10,6
	mulhdu	21,10,6
	addc	22,22,12
	adde	23,23,21

	mulld	12,11,6
	mulhdu	21,11,6
	addc	24,24,12
	adde	25,25,21

	mulld	12,9,6
	mulhdu	21,9,6
	ld	4,24(5)
	mulli	9,9,19
	addc	30,30,12
	adde	31,31,21

	mulld	12,7,6
	mulhdu	21,7,6
	addc	26,26,12
	adde	27,27,21

	mulld	12,8,6
	mulhdu	21,8,6
	addc	28,28,12
	adde	29,29,21
	mulld	12,9,4
	mulhdu	21,9,4
	addc	22,22,12
	adde	23,23,21

	mulld	12,10,4
	mulhdu	21,10,4
	addc	24,24,12
	adde	25,25,21

	mulld	12,8,4
	mulhdu	21,8,4
	ld	6,32(5)
	mulli	8,8,19
	addc	30,30,12
	adde	31,31,21

	mulld	12,11,4
	mulhdu	21,11,4
	addc	26,26,12
	adde	27,27,21

	mulld	12,7,4
	mulhdu	21,7,4
	addc	28,28,12
	adde	29,29,21
	mulld	12,8,6
	mulhdu	21,8,6
	addc	22,22,12
	adde	23,23,21

	mulld	12,9,6
	mulhdu	21,9,6
	addc	24,24,12
	adde	25,25,21

	mulld	12,10,6
	mulhdu	21,10,6
	addc	26,26,12
	adde	27,27,21

	mulld	12,11,6
	mulhdu	21,11,6
	addc	28,28,12
	adde	29,29,21

	mulld	12,7,6
	mulhdu	21,7,6
	addc	30,30,12
	adde	31,31,21

.Lfe51_reduce:
	li	0,-1
	srdi	0,0,13

	srdi	12,26,51
	and	9,26,0
	insrdi	12,27,51,0
	srdi	21,22,51
	and	7,22,0
	insrdi	21,23,51,0
	addc	28,28,12
	addze	29,29
	addc	24,24,21
	addze	25,25

	srdi	12,28,51
	and	10,28,0
	insrdi	12,29,51,0
	srdi	21,24,51
	and	8,24,0
	insrdi	21,25,51,0
	addc	30,30,12
	addze	31,31
	add	9,9,21

	srdi	12,30,51
	and	11,30,0
	insrdi	12,31,51,0
	mulli	12,12,19

	add	7,7,12

	srdi	21,9,51
	and	9,9,0
	add	10,10,21

	srdi	12,7,51
	and	7,7,0
	add	8,8,12

	std	9,16(3)
	std	10,24(3)
	std	11,32(3)
	std	7,0(3)
	std	8,8(3)

	ld	21,56(1)
	ld	22,64(1)
	ld	23,72(1)
	ld	24,80(1)
	ld	25,88(1)
	ld	26,96(1)
	ld	27,104(1)
	ld	28,112(1)
	ld	29,120(1)
	ld	30,128(1)
	ld	31,136(1)
	addi	1,1,144
	blr	
.long	0
.byte	0,12,4,0,0x80,11,3,0
.long	0
.size	x25519_fe51_mul,.-x25519_fe51_mul
.globl	x25519_fe51_sqr
.type	x25519_fe51_sqr,@function
.type	x25519_fe51_sqr,@function
.align	5
x25519_fe51_sqr:
.localentry	x25519_fe51_sqr,0

	stdu	1,-144(1)
	std	21,56(1)
	std	22,64(1)
	std	23,72(1)
	std	24,80(1)
	std	25,88(1)
	std	26,96(1)
	std	27,104(1)
	std	28,112(1)
	std	29,120(1)
	std	30,128(1)
	std	31,136(1)

	ld	7,0(4)
	ld	8,8(4)
	ld	9,16(4)
	ld	10,24(4)
	ld	11,32(4)

	add	6,7,7
	mulli	21,11,19

	mulld	22,7,7
	mulhdu	23,7,7
	mulld	24,8,6
	mulhdu	25,8,6
	mulld	26,9,6
	mulhdu	27,9,6
	mulld	28,10,6
	mulhdu	29,10,6
	mulld	30,11,6
	mulhdu	31,11,6
	add	6,8,8
	mulld	12,11,21
	mulhdu	11,11,21
	addc	28,28,12
	adde	29,29,11

	mulli	5,10,19

	mulld	12,8,8
	mulhdu	11,8,8
	addc	26,26,12
	adde	27,27,11
	mulld	12,9,6
	mulhdu	11,9,6
	addc	28,28,12
	adde	29,29,11
	mulld	12,10,6
	mulhdu	11,10,6
	addc	30,30,12
	adde	31,31,11
	mulld	12,21,6
	mulhdu	11,21,6
	add	6,10,10
	addc	22,22,12
	adde	23,23,11
	mulld	12,10,5
	mulhdu	10,10,5
	addc	24,24,12
	adde	25,25,10
	mulld	12,6,21
	mulhdu	10,6,21
	add	6,9,9
	addc	26,26,12
	adde	27,27,10

	mulld	12,9,9
	mulhdu	10,9,9
	addc	30,30,12
	adde	31,31,10
	mulld	12,5,6
	mulhdu	10,5,6
	addc	22,22,12
	adde	23,23,10
	mulld	12,21,6
	mulhdu	10,21,6
	addc	24,24,12
	adde	25,25,10

	b	.Lfe51_reduce
.long	0
.byte	0,12,4,0,0x80,11,2,0
.long	0
.size	x25519_fe51_sqr,.-x25519_fe51_sqr
.globl	x25519_fe51_mul121666
.type	x25519_fe51_mul121666,@function
.type	x25519_fe51_mul121666,@function
.align	5
x25519_fe51_mul121666:
.localentry	x25519_fe51_mul121666,0

	stdu	1,-144(1)
	std	21,56(1)
	std	22,64(1)
	std	23,72(1)
	std	24,80(1)
	std	25,88(1)
	std	26,96(1)
	std	27,104(1)
	std	28,112(1)
	std	29,120(1)
	std	30,128(1)
	std	31,136(1)

	lis	6,1
	ori	6,6,56130
	ld	7,0(4)
	ld	8,8(4)
	ld	9,16(4)
	ld	10,24(4)
	ld	11,32(4)

	mulld	22,7,6
	mulhdu	23,7,6
	mulld	24,8,6
	mulhdu	25,8,6
	mulld	26,9,6
	mulhdu	27,9,6
	mulld	28,10,6
	mulhdu	29,10,6
	mulld	30,11,6
	mulhdu	31,11,6

	b	.Lfe51_reduce
.long	0
.byte	0,12,4,0,0x80,11,2,0
.long	0
.size	x25519_fe51_mul121666,.-x25519_fe51_mul121666
