#include "arm_arch.h"

.section	__TEXT,__const

.align	8	// strategic alignment and padding that allows to use
		// address value as loop termination condition...
.quad	0,0,0,0,0,0,0,0

iotas:
.quad	0x0000000000000001
.quad	0x0000000000008082
.quad	0x800000000000808a
.quad	0x8000000080008000
.quad	0x000000000000808b
.quad	0x0000000080000001
.quad	0x8000000080008081
.quad	0x8000000000008009
.quad	0x000000000000008a
.quad	0x0000000000000088
.quad	0x0000000080008009
.quad	0x000000008000000a
.quad	0x000000008000808b
.quad	0x800000000000008b
.quad	0x8000000000008089
.quad	0x8000000000008003
.quad	0x8000000000008002
.quad	0x8000000000000080
.quad	0x000000000000800a
.quad	0x800000008000000a
.quad	0x8000000080008081
.quad	0x8000000000008080
.quad	0x0000000080000001
.quad	0x8000000080008008

.text


.align	5
KeccakF1600_int:
	AARCH64_SIGN_LINK_REGISTER
	adrp	x28,iotas@PAGE
	add	x28,x28,iotas@PAGEOFF
	stp	x28,x30,[sp,#16]		// 32 bytes on top are mine
	b	Loop
.align	4
Loop:
	////////////////////////////////////////// Theta
	eor	x26,x0,x5
	stp	x4,x9,[sp,#0]	// offload pair...
	eor	x27,x1,x6
	eor	x28,x2,x7
	eor	x30,x3,x8
	eor	x4,x4,x9
	eor	x26,x26,x10
	eor	x27,x27,x11
	eor	x28,x28,x12
	eor	x30,x30,x13
	eor	x4,x4,x14
	eor	x26,x26,x15
	eor	x27,x27,x16
	eor	x28,x28,x17
	eor	x30,x30,x25
	eor	x4,x4,x19
	eor	x26,x26,x20
	eor	x28,x28,x22
	eor	x27,x27,x21
	eor	x30,x30,x23
	eor	x4,x4,x24

	eor	x9,x26,x28,ror#63

	eor	x1,x1,x9
	eor	x6,x6,x9
	eor	x11,x11,x9
	eor	x16,x16,x9
	eor	x21,x21,x9

	eor	x9,x27,x30,ror#63
	eor	x28,x28,x4,ror#63
	eor	x30,x30,x26,ror#63
	eor	x4,x4,x27,ror#63

	eor	x27,   x2,x9		// mov	x27,x2
	eor	x7,x7,x9
	eor	x12,x12,x9
	eor	x17,x17,x9
	eor	x22,x22,x9

	eor	x0,x0,x4
	eor	x5,x5,x4
	eor	x10,x10,x4
	eor	x15,x15,x4
	eor	x20,x20,x4
	ldp	x4,x9,[sp,#0]	// re-load offloaded data
	eor	x26,   x3,x28		// mov	x26,x3
	eor	x8,x8,x28
	eor	x13,x13,x28
	eor	x25,x25,x28
	eor	x23,x23,x28

	eor	x28,   x4,x30		// mov	x28,x4
	eor	x9,x9,x30
	eor	x14,x14,x30
	eor	x19,x19,x30
	eor	x24,x24,x30

	////////////////////////////////////////// Rho+Pi
	mov	x30,x1
	ror	x1,x6,#64-44
	//mov	x27,x2
	ror	x2,x12,#64-43
	//mov	x26,x3
	ror	x3,x25,#64-21
	//mov	x28,x4
	ror	x4,x24,#64-14

	ror	x6,x9,#64-20
	ror	x12,x13,#64-25
	ror	x25,x17,#64-15
	ror	x24,x21,#64-2

	ror	x9,x22,#64-61
	ror	x13,x19,#64-8
	ror	x17,x11,#64-10
	ror	x21,x8,#64-55

	ror	x22,x14,#64-39
	ror	x19,x23,#64-56
	ror	x11,x7,#64-6
	ror	x8,x16,#64-45

	ror	x14,x20,#64-18
	ror	x23,x15,#64-41
	ror	x7,x10,#64-3
	ror	x16,x5,#64-36

	ror	x5,x26,#64-28
	ror	x10,x30,#64-1
	ror	x15,x28,#64-27
	ror	x20,x27,#64-62

	////////////////////////////////////////// Chi+Iota
	bic	x26,x2,x1
	bic	x27,x3,x2
	bic	x28,x0,x4
	bic	x30,x1,x0
	eor	x0,x0,x26
	bic	x26,x4,x3
	eor	x1,x1,x27
	ldr	x27,[sp,#16]
	eor	x3,x3,x28
	eor	x4,x4,x30
	eor	x2,x2,x26
	ldr	x30,[x27],#8		// Iota[i++]

	bic	x26,x7,x6
	tst	x27,#255			// are we done?
	str	x27,[sp,#16]
	bic	x27,x8,x7
	bic	x28,x5,x9
	eor	x0,x0,x30		// A[0][0] ^= Iota
	bic	x30,x6,x5
	eor	x5,x5,x26
	bic	x26,x9,x8
	eor	x6,x6,x27
	eor	x8,x8,x28
	eor	x9,x9,x30
	eor	x7,x7,x26

	bic	x26,x12,x11
	bic	x27,x13,x12
	bic	x28,x10,x14
	bic	x30,x11,x10
	eor	x10,x10,x26
	bic	x26,x14,x13
	eor	x11,x11,x27
	eor	x13,x13,x28
	eor	x14,x14,x30
	eor	x12,x12,x26

	bic	x26,x17,x16
	bic	x27,x25,x17
	bic	x28,x15,x19
	bic	x30,x16,x15
	eor	x15,x15,x26
	bic	x26,x19,x25
	eor	x16,x16,x27
	eor	x25,x25,x28
	eor	x19,x19,x30
	eor	x17,x17,x26

	bic	x26,x22,x21
	bic	x27,x23,x22
	bic	x28,x20,x24
	bic	x30,x21,x20
	eor	x20,x20,x26
	bic	x26,x24,x23
	eor	x21,x21,x27
	eor	x23,x23,x28
	eor	x24,x24,x30
	eor	x22,x22,x26

	bne	Loop

	ldr	x30,[sp,#24]
	AARCH64_VALIDATE_LINK_REGISTER
	ret



.align	5
KeccakF1600:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-128]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]
	stp	x27,x28,[sp,#80]
	sub	sp,sp,#48

	str	x0,[sp,#32]			// offload argument
	mov	x26,x0
	ldp	x0,x1,[x0,#16*0]
	ldp	x2,x3,[x26,#16*1]
	ldp	x4,x5,[x26,#16*2]
	ldp	x6,x7,[x26,#16*3]
	ldp	x8,x9,[x26,#16*4]
	ldp	x10,x11,[x26,#16*5]
	ldp	x12,x13,[x26,#16*6]
	ldp	x14,x15,[x26,#16*7]
	ldp	x16,x17,[x26,#16*8]
	ldp	x25,x19,[x26,#16*9]
	ldp	x20,x21,[x26,#16*10]
	ldp	x22,x23,[x26,#16*11]
	ldr	x24,[x26,#16*12]

	bl	KeccakF1600_int

	ldr	x26,[sp,#32]
	stp	x0,x1,[x26,#16*0]
	stp	x2,x3,[x26,#16*1]
	stp	x4,x5,[x26,#16*2]
	stp	x6,x7,[x26,#16*3]
	stp	x8,x9,[x26,#16*4]
	stp	x10,x11,[x26,#16*5]
	stp	x12,x13,[x26,#16*6]
	stp	x14,x15,[x26,#16*7]
	stp	x16,x17,[x26,#16*8]
	stp	x25,x19,[x26,#16*9]
	stp	x20,x21,[x26,#16*10]
	stp	x22,x23,[x26,#16*11]
	str	x24,[x26,#16*12]

	ldp	x19,x20,[x29,#16]
	add	sp,sp,#48
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#128
	AARCH64_VALIDATE_LINK_REGISTER
	ret


.globl	_SHA3_absorb

.align	5
_SHA3_absorb:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-128]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]
	stp	x27,x28,[sp,#80]
	sub	sp,sp,#64

	stp	x0,x1,[sp,#32]			// offload arguments
	stp	x2,x3,[sp,#48]

	mov	x26,x0			// uint64_t A[5][5]
	mov	x27,x1			// const void *inp
	mov	x28,x2			// size_t len
	mov	x30,x3			// size_t bsz
	ldp	x0,x1,[x26,#16*0]
	ldp	x2,x3,[x26,#16*1]
	ldp	x4,x5,[x26,#16*2]
	ldp	x6,x7,[x26,#16*3]
	ldp	x8,x9,[x26,#16*4]
	ldp	x10,x11,[x26,#16*5]
	ldp	x12,x13,[x26,#16*6]
	ldp	x14,x15,[x26,#16*7]
	ldp	x16,x17,[x26,#16*8]
	ldp	x25,x19,[x26,#16*9]
	ldp	x20,x21,[x26,#16*10]
	ldp	x22,x23,[x26,#16*11]
	ldr	x24,[x26,#16*12]
	b	Loop_absorb

.align	4
Loop_absorb:
	subs	x26,x28,x30		// len - bsz
	blo	Labsorbed

	str	x26,[sp,#48]			// save len - bsz
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x0,x0,x26
	cmp	x30,#8*(0+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x1,x1,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x2,x2,x26
	cmp	x30,#8*(2+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x3,x3,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x4,x4,x26
	cmp	x30,#8*(4+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x5,x5,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x6,x6,x26
	cmp	x30,#8*(6+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x7,x7,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x8,x8,x26
	cmp	x30,#8*(8+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x9,x9,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x10,x10,x26
	cmp	x30,#8*(10+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x11,x11,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x12,x12,x26
	cmp	x30,#8*(12+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x13,x13,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x14,x14,x26
	cmp	x30,#8*(14+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x15,x15,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x16,x16,x26
	cmp	x30,#8*(16+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x17,x17,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x25,x25,x26
	cmp	x30,#8*(18+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x19,x19,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x20,x20,x26
	cmp	x30,#8*(20+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x21,x21,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x22,x22,x26
	cmp	x30,#8*(22+2)
	blo	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x23,x23,x26
	beq	Lprocess_block
	ldr	x26,[x27],#8		// *inp++
#ifdef	__AARCH64EB__
	rev	x26,x26
#endif
	eor	x24,x24,x26

Lprocess_block:
	str	x27,[sp,#40]			// save inp

	bl	KeccakF1600_int

	ldr	x27,[sp,#40]			// restore arguments
	ldp	x28,x30,[sp,#48]
	b	Loop_absorb

.align	4
Labsorbed:
	ldr	x27,[sp,#32]
	stp	x0,x1,[x27,#16*0]
	stp	x2,x3,[x27,#16*1]
	stp	x4,x5,[x27,#16*2]
	stp	x6,x7,[x27,#16*3]
	stp	x8,x9,[x27,#16*4]
	stp	x10,x11,[x27,#16*5]
	stp	x12,x13,[x27,#16*6]
	stp	x14,x15,[x27,#16*7]
	stp	x16,x17,[x27,#16*8]
	stp	x25,x19,[x27,#16*9]
	stp	x20,x21,[x27,#16*10]
	stp	x22,x23,[x27,#16*11]
	str	x24,[x27,#16*12]

	mov	x0,x28			// return value
	ldp	x19,x20,[x29,#16]
	add	sp,sp,#64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#128
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_SHA3_squeeze

.align	5
_SHA3_squeeze:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-48]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]

	mov	x19,x0			// put aside arguments
	mov	x20,x1
	mov	x21,x2
	mov	x22,x3
	cmp	w4, #0				// w4 = 'next' argument
	bne	Lnext_block

Loop_squeeze:
	ldr	x4,[x0],#8
	cmp	x21,#8
	blo	Lsqueeze_tail
#ifdef	__AARCH64EB__
	rev	x4,x4
#endif
	str	x4,[x20],#8
	subs	x21,x21,#8
	beq	Lsqueeze_done

	subs	x3,x3,#8
	bhi	Loop_squeeze
Lnext_block:
	mov	x0,x19
	bl	KeccakF1600
	mov	x0,x19
	mov	x3,x22
	b	Loop_squeeze

.align	4
Lsqueeze_tail:
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1
	lsr	x4,x4,#8
	subs	x21,x21,#1
	beq	Lsqueeze_done
	strb	w4,[x20],#1

Lsqueeze_done:
	ldp	x19,x20,[sp,#16]
	ldp	x21,x22,[sp,#32]
	ldp	x29,x30,[sp],#48
	AARCH64_VALIDATE_LINK_REGISTER
	ret


.align	5
KeccakF1600_ce:
	mov	x9,#24
	adrp	x10,iotas@PAGE
	add	x10,x10,iotas@PAGEOFF
	b	Loop_ce
.align	4
Loop_ce:
	////////////////////////////////////////////////// Theta
.long	0xce0f2a99	//eor3 v25.16b,v20.16b,v15.16b,v10.16b
.long	0xce102eba	//eor3 v26.16b,v21.16b,v16.16b,v11.16b
.long	0xce1132db	//eor3 v27.16b,v22.16b,v17.16b,v12.16b
.long	0xce1236fc	//eor3 v28.16b,v23.16b,v18.16b,v13.16b
.long	0xce133b1d	//eor3 v29.16b,v24.16b,v19.16b,v14.16b
.long	0xce050339	//eor3 v25.16b,v25.16b,   v5.16b,v0.16b
.long	0xce06075a	//eor3 v26.16b,v26.16b,   v6.16b,v1.16b
.long	0xce070b7b	//eor3 v27.16b,v27.16b,   v7.16b,v2.16b
.long	0xce080f9c	//eor3 v28.16b,v28.16b,   v8.16b,v3.16b
.long	0xce0913bd	//eor3 v29.16b,v29.16b,   v9.16b,v4.16b

.long	0xce7b8f3e	//rax1 v30.16b,v25.16b,v27.16b			// D[1]
.long	0xce7c8f5f	//rax1 v31.16b,v26.16b,v28.16b			// D[2]
.long	0xce7d8f7b	//rax1 v27.16b,v27.16b,v29.16b			// D[3]
.long	0xce798f9c	//rax1 v28.16b,v28.16b,v25.16b			// D[4]
.long	0xce7a8fbd	//rax1 v29.16b,v29.16b,v26.16b			// D[0]

	////////////////////////////////////////////////// Theta+Rho+Pi
.long	0xce9efc39	//xar v25.16b,   v1.16b,v30.16b,#64-1 // C[0]=A[2][0]

.long	0xce9e50c1	//xar v1.16b,v6.16b,v30.16b,#64-44
.long	0xce9cb126	//xar v6.16b,v9.16b,v28.16b,#64-20
.long	0xce9f0ec9	//xar v9.16b,v22.16b,v31.16b,#64-61
.long	0xce9c65d6	//xar v22.16b,v14.16b,v28.16b,#64-39
.long	0xce9dba8e	//xar v14.16b,v20.16b,v29.16b,#64-18

.long	0xce9f085a	//xar v26.16b,   v2.16b,v31.16b,#64-62 // C[1]=A[4][0]

.long	0xce9f5582	//xar v2.16b,v12.16b,v31.16b,#64-43
.long	0xce9b9dac	//xar v12.16b,v13.16b,v27.16b,#64-25
.long	0xce9ce26d	//xar v13.16b,v19.16b,v28.16b,#64-8
.long	0xce9b22f3	//xar v19.16b,v23.16b,v27.16b,#64-56
.long	0xce9d5df7	//xar v23.16b,v15.16b,v29.16b,#64-41

.long	0xce9c948f	//xar v15.16b,v4.16b,v28.16b,#64-27

.long	0xce9ccb1c	//xar v28.16b,   v24.16b,v28.16b,#64-14 // D[4]=A[0][4]
.long	0xce9efab8	//xar v24.16b,v21.16b,v30.16b,#64-2
.long	0xce9b2508	//xar v8.16b,v8.16b,v27.16b,#64-55 // A[1][3]=A[4][1]
.long	0xce9e4e04	//xar v4.16b,v16.16b,v30.16b,#64-45 // A[0][4]=A[1][3]
.long	0xce9d70b0	//xar v16.16b,v5.16b,v29.16b,#64-36

.long	0xce9b9065	//xar v5.16b,v3.16b,v27.16b,#64-28

	eor	v0.16b,v0.16b,v29.16b

.long	0xce9bae5b	//xar v27.16b,   v18.16b,v27.16b,#64-21 // D[3]=A[0][3]
.long	0xce9fc623	//xar v3.16b,v17.16b,v31.16b,#64-15 // A[0][3]=A[3][3]
.long	0xce9ed97e	//xar v30.16b,   v11.16b,v30.16b,#64-10 // D[1]=A[3][2]
.long	0xce9fe8ff	//xar v31.16b,   v7.16b,v31.16b,#64-6 // D[2]=A[2][1]
.long	0xce9df55d	//xar v29.16b,   v10.16b,v29.16b,#64-3 // D[0]=A[1][2]

	////////////////////////////////////////////////// Chi+Iota
.long	0xce362354	//bcax v20.16b,v26.16b,   v22.16b,v8.16b	// A[1][3]=A[4][1]
.long	0xce375915	//bcax v21.16b,v8.16b,v23.16b,v22.16b	// A[1][3]=A[4][1]
.long	0xce385ed6	//bcax v22.16b,v22.16b,v24.16b,v23.16b
.long	0xce3a62f7	//bcax v23.16b,v23.16b,v26.16b,   v24.16b
.long	0xce286b18	//bcax v24.16b,v24.16b,v8.16b,v26.16b	// A[1][3]=A[4][1]

	ld1r	{v26.2d},[x10],#8

.long	0xce330fd1	//bcax v17.16b,v30.16b,   v19.16b,v3.16b	// A[0][3]=A[3][3]
.long	0xce2f4c72	//bcax v18.16b,v3.16b,v15.16b,v19.16b	// A[0][3]=A[3][3]
.long	0xce303e73	//bcax v19.16b,v19.16b,v16.16b,v15.16b
.long	0xce3e41ef	//bcax v15.16b,v15.16b,v30.16b,   v16.16b
.long	0xce237a10	//bcax v16.16b,v16.16b,v3.16b,v30.16b	// A[0][3]=A[3][3]

.long	0xce2c7f2a	//bcax v10.16b,v25.16b,   v12.16b,v31.16b
.long	0xce2d33eb	//bcax v11.16b,v31.16b,   v13.16b,v12.16b
.long	0xce2e358c	//bcax v12.16b,v12.16b,v14.16b,v13.16b
.long	0xce3939ad	//bcax v13.16b,v13.16b,v25.16b,   v14.16b
.long	0xce3f65ce	//bcax v14.16b,v14.16b,v31.16b,   v25.16b

.long	0xce2913a7	//bcax v7.16b,v29.16b,   v9.16b,v4.16b	// A[0][4]=A[1][3]
.long	0xce252488	//bcax v8.16b,v4.16b,v5.16b,v9.16b	// A[0][4]=A[1][3]
.long	0xce261529	//bcax v9.16b,v9.16b,v6.16b,v5.16b
.long	0xce3d18a5	//bcax v5.16b,v5.16b,v29.16b,   v6.16b
.long	0xce2474c6	//bcax v6.16b,v6.16b,v4.16b,v29.16b	// A[0][4]=A[1][3]

.long	0xce207363	//bcax v3.16b,v27.16b,   v0.16b,v28.16b
.long	0xce210384	//bcax v4.16b,v28.16b,   v1.16b,v0.16b
.long	0xce220400	//bcax v0.16b,v0.16b,v2.16b,v1.16b
.long	0xce3b0821	//bcax v1.16b,v1.16b,v27.16b,   v2.16b
.long	0xce3c6c42	//bcax v2.16b,v2.16b,v28.16b,   v27.16b

	eor	v0.16b,v0.16b,v26.16b

	subs	x9,x9,#1
	bne	Loop_ce

	ret



.align	5
KeccakF1600_cext:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-80]!
	add	x29,sp,#0
	stp	d8,d9,[sp,#16]		// per ABI requirement
	stp	d10,d11,[sp,#32]
	stp	d12,d13,[sp,#48]
	stp	d14,d15,[sp,#64]
	ldp	d0,d1,[x0,#8*0]
	ldp	d2,d3,[x0,#8*2]
	ldp	d4,d5,[x0,#8*4]
	ldp	d6,d7,[x0,#8*6]
	ldp	d8,d9,[x0,#8*8]
	ldp	d10,d11,[x0,#8*10]
	ldp	d12,d13,[x0,#8*12]
	ldp	d14,d15,[x0,#8*14]
	ldp	d16,d17,[x0,#8*16]
	ldp	d18,d19,[x0,#8*18]
	ldp	d20,d21,[x0,#8*20]
	ldp	d22,d23,[x0,#8*22]
	ldr	d24,[x0,#8*24]
	bl	KeccakF1600_ce
	ldr	x30,[sp,#8]
	stp	d0,d1,[x0,#8*0]
	stp	d2,d3,[x0,#8*2]
	stp	d4,d5,[x0,#8*4]
	stp	d6,d7,[x0,#8*6]
	stp	d8,d9,[x0,#8*8]
	stp	d10,d11,[x0,#8*10]
	stp	d12,d13,[x0,#8*12]
	stp	d14,d15,[x0,#8*14]
	stp	d16,d17,[x0,#8*16]
	stp	d18,d19,[x0,#8*18]
	stp	d20,d21,[x0,#8*20]
	stp	d22,d23,[x0,#8*22]
	str	d24,[x0,#8*24]

	ldp	d8,d9,[sp,#16]
	ldp	d10,d11,[sp,#32]
	ldp	d12,d13,[sp,#48]
	ldp	d14,d15,[sp,#64]
	ldr	x29,[sp],#80
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_SHA3_absorb_cext

.align	5
_SHA3_absorb_cext:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-80]!
	add	x29,sp,#0
	stp	d8,d9,[sp,#16]		// per ABI requirement
	stp	d10,d11,[sp,#32]
	stp	d12,d13,[sp,#48]
	stp	d14,d15,[sp,#64]
	ldp	d0,d1,[x0,#8*0]
	ldp	d2,d3,[x0,#8*2]
	ldp	d4,d5,[x0,#8*4]
	ldp	d6,d7,[x0,#8*6]
	ldp	d8,d9,[x0,#8*8]
	ldp	d10,d11,[x0,#8*10]
	ldp	d12,d13,[x0,#8*12]
	ldp	d14,d15,[x0,#8*14]
	ldp	d16,d17,[x0,#8*16]
	ldp	d18,d19,[x0,#8*18]
	ldp	d20,d21,[x0,#8*20]
	ldp	d22,d23,[x0,#8*22]
	ldr	d24,[x0,#8*24]
	b	Loop_absorb_ce

.align	4
Loop_absorb_ce:
	subs	x2,x2,x3		// len - bsz
	blo	Labsorbed_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v0.16b,v0.16b,v31.16b
	cmp	x3,#8*(0+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v1.16b,v1.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v2.16b,v2.16b,v31.16b
	cmp	x3,#8*(2+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v3.16b,v3.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v4.16b,v4.16b,v31.16b
	cmp	x3,#8*(4+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v5.16b,v5.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v6.16b,v6.16b,v31.16b
	cmp	x3,#8*(6+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v7.16b,v7.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v8.16b,v8.16b,v31.16b
	cmp	x3,#8*(8+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v9.16b,v9.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v10.16b,v10.16b,v31.16b
	cmp	x3,#8*(10+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v11.16b,v11.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v12.16b,v12.16b,v31.16b
	cmp	x3,#8*(12+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v13.16b,v13.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v14.16b,v14.16b,v31.16b
	cmp	x3,#8*(14+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v15.16b,v15.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v16.16b,v16.16b,v31.16b
	cmp	x3,#8*(16+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v17.16b,v17.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v18.16b,v18.16b,v31.16b
	cmp	x3,#8*(18+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v19.16b,v19.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v20.16b,v20.16b,v31.16b
	cmp	x3,#8*(20+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v21.16b,v21.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v22.16b,v22.16b,v31.16b
	cmp	x3,#8*(22+2)
	blo	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v23.16b,v23.16b,v31.16b
	beq	Lprocess_block_ce
	ldr	d31,[x1],#8		// *inp++
#ifdef	__AARCH64EB__
	rev64	v31.16b,v31.16b
#endif
	eor	v24.16b,v24.16b,v31.16b

Lprocess_block_ce:

	bl	KeccakF1600_ce

	b	Loop_absorb_ce

.align	4
Labsorbed_ce:
	stp	d0,d1,[x0,#8*0]
	stp	d2,d3,[x0,#8*2]
	stp	d4,d5,[x0,#8*4]
	stp	d6,d7,[x0,#8*6]
	stp	d8,d9,[x0,#8*8]
	stp	d10,d11,[x0,#8*10]
	stp	d12,d13,[x0,#8*12]
	stp	d14,d15,[x0,#8*14]
	stp	d16,d17,[x0,#8*16]
	stp	d18,d19,[x0,#8*18]
	stp	d20,d21,[x0,#8*20]
	stp	d22,d23,[x0,#8*22]
	str	d24,[x0,#8*24]
	add	x0,x2,x3		// return value

	ldp	d8,d9,[sp,#16]
	ldp	d10,d11,[sp,#32]
	ldp	d12,d13,[sp,#48]
	ldp	d14,d15,[sp,#64]
	ldp	x29,x30,[sp],#80
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_SHA3_squeeze_cext

.align	5
_SHA3_squeeze_cext:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	mov	x9,x0
	mov	x10,x3

Loop_squeeze_ce:
	ldr	x4,[x9],#8
	cmp	x2,#8
	blo	Lsqueeze_tail_ce
#ifdef	__AARCH64EB__
	rev	x4,x4
#endif
	str	x4,[x1],#8
	beq	Lsqueeze_done_ce

	sub	x2,x2,#8
	subs	x10,x10,#8
	bhi	Loop_squeeze_ce

	bl	KeccakF1600_cext
	ldr	x30,[sp,#8]
	mov	x9,x0
	mov	x10,x3
	b	Loop_squeeze_ce

.align	4
Lsqueeze_tail_ce:
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1
	lsr	x4,x4,#8
	subs	x2,x2,#1
	beq	Lsqueeze_done_ce
	strb	w4,[x1],#1

Lsqueeze_done_ce:
	ldr	x29,[sp],#16
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.byte	75,101,99,99,97,107,45,49,54,48,48,32,97,98,115,111,114,98,32,97,110,100,32,115,113,117,101,101,122,101,32,102,111,114,32,65,82,77,118,56,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
