#include "arm_arch.h"

.text
.code	32

.align	5
.global	_armv7_neon_probe
.type	_armv7_neon_probe,%function
_armv7_neon_probe:
	.word	0xf26ee1fe	@ vorr	q15,q15,q15
	.word	0xe12fff1e	@ bx	lr
.size	_armv7_neon_probe,.-_armv7_neon_probe

.global	_armv7_tick
.type	_armv7_tick,%function
_armv7_tick:
	mrc	p15,0,r0,c9,c13,0
	.word	0xe12fff1e	@ bx	lr
.size	_armv7_tick,.-_armv7_tick

.global	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,%function
OPENSSL_atomic_add:
#if __ARM_ARCH__>=6
.Ladd:	ldrex	r2,[r0]
	add	r3,r2,r1
	strex	r2,r3,[r0]
	cmp	r2,#0
	bne	.Ladd
	mov	r0,r3
	.word	0xe12fff1e	@ bx	lr
#else
	stmdb	sp!,{r4-r6,lr}
	ldr	r2,.Lspinlock
	adr	r3,.Lspinlock
	mov	r4,r0
	mov	r5,r1
	add	r6,r3,r2	@ &spinlock
	b	.+8
.Lspin:	bl	sched_yield
	mov	r0,#-1
	swp	r0,r0,[r6]
	cmp	r0,#0
	bne	.Lspin

	ldr	r2,[r4]
	add	r2,r2,r5
	str	r2,[r4]
	str	r0,[r6]		@ release spinlock
	ldmia	sp!,{r4-r6,lr}
	tst	lr,#1
	moveq	pc,lr
	.word	0xe12fff1e	@ bx	lr
#endif
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.global	OPENSSL_cleanse
.type	OPENSSL_cleanse,%function
OPENSSL_cleanse:
	eor	ip,ip,ip
	cmp	r1,#7
	subhs	r1,r1,#4
	bhs	.Lot
	cmp	r1,#0
	beq	.Lcleanse_done
.Little:
	strb	ip,[r0],#1
	subs	r1,r1,#1
	bhi	.Little
	b	.Lcleanse_done

.Lot:	tst	r0,#3
	beq	.Laligned
	strb	ip,[r0],#1
	sub	r1,r1,#1
	b	.Lot
.Laligned:
	str	ip,[r0],#4
	subs	r1,r1,#4
	bhs	.Laligned
	adds	r1,r1,#4
	bne	.Little
.Lcleanse_done:
	tst	lr,#1
	moveq	pc,lr
	.word	0xe12fff1e	@ bx	lr
.size	OPENSSL_cleanse,.-OPENSSL_cleanse

.global	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,%function
OPENSSL_wipe_cpu:
	ldr	r0,.LOPENSSL_armcap
	adr	r1,.LOPENSSL_armcap
	ldr	r0,[r1,r0]
	eor	r2,r2,r2
	eor	r3,r3,r3
	eor	ip,ip,ip
	tst	r0,#1
	beq	.Lwipe_done
	.word	0xf3000150	@ veor    q0, q0, q0
	.word	0xf3022152	@ veor    q1, q1, q1
	.word	0xf3044154	@ veor    q2, q2, q2
	.word	0xf3066156	@ veor    q3, q3, q3
	.word	0xf34001f0	@ veor    q8, q8, q8
	.word	0xf34221f2	@ veor    q9, q9, q9
	.word	0xf34441f4	@ veor    q10, q10, q10
	.word	0xf34661f6	@ veor    q11, q11, q11
	.word	0xf34881f8	@ veor    q12, q12, q12
	.word	0xf34aa1fa	@ veor    q13, q13, q13
	.word	0xf34cc1fc	@ veor    q14, q14, q14
	.word	0xf34ee1fe	@ veor    q15, q15, q15
.Lwipe_done:
	mov	r0,sp
	tst	lr,#1
	moveq	pc,lr
	.word	0xe12fff1e	@ bx	lr
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu

.global	OPENSSL_instrument_bus
.type	OPENSSL_instrument_bus,%function
OPENSSL_instrument_bus:
	eor	r0,r0,r0
	tst	lr,#1
	moveq	pc,lr
	.word	0xe12fff1e	@ bx	lr
.size	OPENSSL_instrument_bus,.-OPENSSL_instrument_bus

.global	OPENSSL_instrument_bus2
.type	OPENSSL_instrument_bus2,%function
OPENSSL_instrument_bus2:
	eor	r0,r0,r0
	tst	lr,#1
	moveq	pc,lr
	.word	0xe12fff1e	@ bx	lr
.size	OPENSSL_instrument_bus2,.-OPENSSL_instrument_bus2

.align	5
.LOPENSSL_armcap:
.word	OPENSSL_armcap_P-.LOPENSSL_armcap
#if __ARM_ARCH__>=6
.align	5
#else
.Lspinlock:
.word	atomic_add_spinlock-.Lspinlock
.align	5

.data
.align	2
atomic_add_spinlock:
.word	0
#endif

.comm	OPENSSL_armcap_P,4,4
.hidden	OPENSSL_armcap_P
