.text

.global	bn_mul_mont
.type	bn_mul_mont,%function

.align	2
bn_mul_mont:
	stmdb	sp!,{r0,r2}		@ sp points at argument block
	ldr	r0,[sp,#3*4]		@ load num
	cmp	r0,#2
	movlt	r0,#0
	addlt	sp,sp,#2*4
	blt	.Labrt

	stmdb	sp!,{r4-r12,lr}		@ save 10 registers

	mov	r0,r0,lsl#2		@ rescale r0 for byte count
	sub	sp,sp,r0		@ alloca(4*num)
	sub	sp,sp,#4		@ +extra dword
	sub	r0,r0,#4		@ "num=num-1"
	add	r4,r2,r0		@ &bp[num-1]

	add	r0,sp,r0		@ r0 to point at &tp[num-1]
	ldr	r8,[r0,#14*4]		@ &n0
	ldr	r2,[r2]		@ bp[0]
	ldr	r5,[r1],#4		@ ap[0],ap++
	ldr	r6,[r3],#4		@ np[0],np++
	ldr	r8,[r8]		@ *n0
	str	r4,[r0,#15*4]		@ save &bp[num]

	umull	r10,r11,r5,r2	@ ap[0]*bp[0]
	str	r8,[r0,#14*4]		@ save n0 value
	mul	r8,r10,r8		@ "tp[0]"*n0
	mov	r12,#0
	umlal	r10,r12,r6,r8	@ np[0]*n0+"t[0]"
	mov	r4,sp

.L1st:
	ldr	r5,[r1],#4		@ ap[j],ap++
	mov	r10,r11
	ldr	r6,[r3],#4		@ np[j],np++
	mov	r11,#0
	umlal	r10,r11,r5,r2	@ ap[j]*bp[0]
	mov	r14,#0
	umlal	r12,r14,r6,r8	@ np[j]*n0
	adds	r12,r12,r10
	str	r12,[r4],#4		@ tp[j-1]=,tp++
	adc	r12,r14,#0
	cmp	r4,r0
	bne	.L1st

	adds	r12,r12,r11
	ldr	r4,[r0,#13*4]		@ restore bp
	mov	r14,#0
	ldr	r8,[r0,#14*4]		@ restore n0
	adc	r14,r14,#0
	str	r12,[r0]		@ tp[num-1]=
	str	r14,[r0,#4]		@ tp[num]=


.Louter:
	sub	r7,r0,sp		@ "original" r0-1 value
	sub	r1,r1,r7		@ "rewind" ap to &ap[1]
	ldr	r2,[r4,#4]!		@ *(++bp)
	sub	r3,r3,r7		@ "rewind" np to &np[1]
	ldr	r5,[r1,#-4]		@ ap[0]
	ldr	r10,[sp]		@ tp[0]
	ldr	r6,[r3,#-4]		@ np[0]
	ldr	r7,[sp,#4]		@ tp[1]

	mov	r11,#0
	umlal	r10,r11,r5,r2	@ ap[0]*bp[i]+tp[0]
	str	r4,[r0,#13*4]		@ save bp
	mul	r8,r10,r8
	mov	r12,#0
	umlal	r10,r12,r6,r8	@ np[0]*n0+"tp[0]"
	mov	r4,sp

.Linner:
	ldr	r5,[r1],#4		@ ap[j],ap++
	adds	r10,r11,r7		@ +=tp[j]
	ldr	r6,[r3],#4		@ np[j],np++
	mov	r11,#0
	umlal	r10,r11,r5,r2	@ ap[j]*bp[i]
	mov	r14,#0
	umlal	r12,r14,r6,r8	@ np[j]*n0
	adc	r11,r11,#0
	ldr	r7,[r4,#8]		@ tp[j+1]
	adds	r12,r12,r10
	str	r12,[r4],#4		@ tp[j-1]=,tp++
	adc	r12,r14,#0
	cmp	r4,r0
	bne	.Linner

	adds	r12,r12,r11
	mov	r14,#0
	ldr	r4,[r0,#13*4]		@ restore bp
	adc	r14,r14,#0
	ldr	r8,[r0,#14*4]		@ restore n0
	adds	r12,r12,r7
	ldr	r7,[r0,#15*4]		@ restore &bp[num]
	adc	r14,r14,#0
	str	r12,[r0]		@ tp[num-1]=
	str	r14,[r0,#4]		@ tp[num]=

	cmp	r4,r7
	bne	.Louter


	ldr	r2,[r0,#12*4]		@ pull rp
	add	r0,r0,#4		@ r0 to point at &tp[num]
	sub	r5,r0,sp		@ "original" num value
	mov	r4,sp			@ "rewind" r4
	mov	r1,r4			@ "borrow" r1
	sub	r3,r3,r5		@ "rewind" r3 to &np[0]

	subs	r7,r7,r7		@ "clear" carry flag
.Lsub:	ldr	r7,[r4],#4
	ldr	r6,[r3],#4
	sbcs	r7,r7,r6		@ tp[j]-np[j]
	str	r7,[r2],#4		@ rp[j]=
	teq	r4,r0		@ preserve carry
	bne	.Lsub
	sbcs	r14,r14,#0		@ upmost carry
	mov	r4,sp			@ "rewind" r4
	sub	r2,r2,r5		@ "rewind" r2

	and	r1,r4,r14
	bic	r3,r2,r14
	orr	r1,r1,r3		@ ap=borrow?tp:rp

.Lcopy:	ldr	r7,[r1],#4		@ copy or in-place refresh
	str	sp,[r4],#4		@ zap tp
	str	r7,[r2],#4
	cmp	r4,r0
	bne	.Lcopy

	add	sp,r0,#4		@ skip over tp[num+1]
	ldmia	sp!,{r4-r12,lr}		@ restore registers
	add	sp,sp,#2*4		@ skip over {r0,r2}
	mov	r0,#1
.Labrt:	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
.size	bn_mul_mont,.-bn_mul_mont
.asciz	"Montgomery multiplication for ARMv4, CRYPTOGAMS by <appro@openssl.org>"
.align	2
