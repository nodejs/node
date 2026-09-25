#include "s390x_arch.h"

.text
.align	64
.type	Ktable,@object
Ktable: .long	0x5a827999,0x6ed9eba1,0x8f1bbcdc,0xca62c1d6
	.skip	48	#.long	0,0,0,0,0,0,0,0,0,0,0,0
.size	Ktable,.-Ktable
.globl	sha1_block_data_order
.type	sha1_block_data_order,@function
sha1_block_data_order:
	larl	%r1,OPENSSL_s390xcap_P
	lg	%r0,S390X_KIMD(%r1)	# check kimd capabilities
	tmhh	%r0,16384
	jz	.Lsoftware
	lghi	%r0,1
	lgr	%r1,%r2
	lgr	%r2,%r3
	sllg	%r3,%r4,6
	.long	0xb93e0002	# kimd %r0,%r2
	brc	1,.-4		# pay attention to "partial completion"
	br	%r14
.align	16
.Lsoftware:
	lghi	%r1,-224
	stg	%r2,16(%r15)
	stmg	%r6,%r15,48(%r15)
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)

	larl	%r10,Ktable
	llgf	%r5,0(%r2)
	llgf	%r6,4(%r2)
	llgf	%r7,8(%r2)
	llgf	%r8,12(%r2)
	llgf	%r9,16(%r2)

	lg	%r0,0(%r10)
	lg	%r1,8(%r10)

.Lloop:
	rllg	%r0,%r0,32
	lg	%r12,0(%r3)	### Xload(0)
	rllg	%r13,%r12,32
	stg	%r12,160(%r15)
	alr	%r9,%r0		### 0
	rll	%r11,%r5,5
	lr	%r10,%r8
	xr	%r10,%r7
	alr	%r9,%r11
	nr	%r10,%r6
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r0		### 1
	rll	%r11,%r9,5
	lr	%r10,%r7
	xr	%r10,%r6
	alr	%r8,%r11
	nr	%r10,%r5
	alr	%r8,%r12
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	lg	%r14,8(%r3)	### Xload(2)
	rllg	%r12,%r14,32
	stg	%r14,168(%r15)
	alr	%r7,%r0		### 2
	rll	%r11,%r8,5
	lr	%r10,%r6
	xr	%r10,%r5
	alr	%r7,%r11
	nr	%r10,%r9
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r0		### 3
	rll	%r11,%r7,5
	lr	%r10,%r5
	xr	%r10,%r9
	alr	%r6,%r11
	nr	%r10,%r8
	alr	%r6,%r14
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	lg	%r13,16(%r3)	### Xload(4)
	rllg	%r14,%r13,32
	stg	%r13,176(%r15)
	alr	%r5,%r0		### 4
	rll	%r11,%r6,5
	lr	%r10,%r9
	xr	%r10,%r8
	alr	%r5,%r11
	nr	%r10,%r7
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r0		### 5
	rll	%r11,%r5,5
	lr	%r10,%r8
	xr	%r10,%r7
	alr	%r9,%r11
	nr	%r10,%r6
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	lg	%r12,24(%r3)	### Xload(6)
	rllg	%r13,%r12,32
	stg	%r12,184(%r15)
	alr	%r8,%r0		### 6
	rll	%r11,%r9,5
	lr	%r10,%r7
	xr	%r10,%r6
	alr	%r8,%r11
	nr	%r10,%r5
	alr	%r8,%r13
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r0		### 7
	rll	%r11,%r8,5
	lr	%r10,%r6
	xr	%r10,%r5
	alr	%r7,%r11
	nr	%r10,%r9
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	lg	%r14,32(%r3)	### Xload(8)
	rllg	%r12,%r14,32
	stg	%r14,192(%r15)
	alr	%r6,%r0		### 8
	rll	%r11,%r7,5
	lr	%r10,%r5
	xr	%r10,%r9
	alr	%r6,%r11
	nr	%r10,%r8
	alr	%r6,%r12
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r0		### 9
	rll	%r11,%r6,5
	lr	%r10,%r9
	xr	%r10,%r8
	alr	%r5,%r11
	nr	%r10,%r7
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	lg	%r13,40(%r3)	### Xload(10)
	rllg	%r14,%r13,32
	stg	%r13,200(%r15)
	alr	%r9,%r0		### 10
	rll	%r11,%r5,5
	lr	%r10,%r8
	xr	%r10,%r7
	alr	%r9,%r11
	nr	%r10,%r6
	alr	%r9,%r14
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r0		### 11
	rll	%r11,%r9,5
	lr	%r10,%r7
	xr	%r10,%r6
	alr	%r8,%r11
	nr	%r10,%r5
	alr	%r8,%r13
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	lg	%r12,48(%r3)	### Xload(12)
	rllg	%r13,%r12,32
	stg	%r12,208(%r15)
	alr	%r7,%r0		### 12
	rll	%r11,%r8,5
	lr	%r10,%r6
	xr	%r10,%r5
	alr	%r7,%r11
	nr	%r10,%r9
	alr	%r7,%r13
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r0		### 13
	rll	%r11,%r7,5
	lr	%r10,%r5
	xr	%r10,%r9
	alr	%r6,%r11
	nr	%r10,%r8
	alr	%r6,%r12
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	lg	%r14,56(%r3)	### Xload(14)
	rllg	%r12,%r14,32
	stg	%r14,216(%r15)
	alr	%r5,%r0		### 14
	rll	%r11,%r6,5
	lr	%r10,%r9
	xr	%r10,%r8
	alr	%r5,%r11
	nr	%r10,%r7
	alr	%r5,%r12
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	lg	%r2,160(%r15)	### Xupdate(16) warm-up
	lr	%r13,%r12
	alr	%r9,%r0		### 15
	rll	%r11,%r5,5
	lr	%r10,%r8
	xr	%r10,%r7
	alr	%r9,%r11
	nr	%r10,%r6
	alr	%r9,%r14
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r13,%r2		### Xupdate(16)
	lg	%r2,168(%r15)
	xg	%r13,192(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,160(%r15)
	alr	%r8,%r0		### 16
	rll	%r11,%r9,5
	lr	%r10,%r7
	xr	%r10,%r6
	alr	%r8,%r11
	nr	%r10,%r5
	alr	%r8,%r14
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r0		### 17
	rll	%r11,%r8,5
	lr	%r10,%r6
	xr	%r10,%r5
	alr	%r7,%r11
	nr	%r10,%r9
	alr	%r7,%r13
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r12,%r2		### Xupdate(18)
	lg	%r2,176(%r15)
	xg	%r12,200(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,168(%r15)
	alr	%r6,%r0		### 18
	rll	%r11,%r7,5
	lr	%r10,%r5
	xr	%r10,%r9
	alr	%r6,%r11
	nr	%r10,%r8
	alr	%r6,%r13
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r0		### 19
	rll	%r11,%r6,5
	lr	%r10,%r9
	xr	%r10,%r8
	alr	%r5,%r11
	nr	%r10,%r7
	alr	%r5,%r12
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	rllg	%r0,%r0,32
	xgr	%r14,%r2		### Xupdate(20)
	lg	%r2,184(%r15)
	xg	%r14,208(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,176(%r15)
	alr	%r9,%r0		### 20
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r12
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r0		### 21
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r14
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r13,%r2		### Xupdate(22)
	lg	%r2,192(%r15)
	xg	%r13,216(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,184(%r15)
	alr	%r7,%r0		### 22
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r14
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r0		### 23
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r13
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r12,%r2		### Xupdate(24)
	lg	%r2,200(%r15)
	xg	%r12,160(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,192(%r15)
	alr	%r5,%r0		### 24
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r13
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r0		### 25
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r12
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r14,%r2		### Xupdate(26)
	lg	%r2,208(%r15)
	xg	%r14,168(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,200(%r15)
	alr	%r8,%r0		### 26
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r12
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r0		### 27
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r14
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r13,%r2		### Xupdate(28)
	lg	%r2,216(%r15)
	xg	%r13,176(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,208(%r15)
	alr	%r6,%r0		### 28
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r14
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r0		### 29
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r13
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	xgr	%r12,%r2		### Xupdate(30)
	lg	%r2,160(%r15)
	xg	%r12,184(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,216(%r15)
	alr	%r9,%r0		### 30
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r0		### 31
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r12
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r14,%r2		### Xupdate(32)
	lg	%r2,168(%r15)
	xg	%r14,192(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,160(%r15)
	alr	%r7,%r0		### 32
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r0		### 33
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r14
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r13,%r2		### Xupdate(34)
	lg	%r2,176(%r15)
	xg	%r13,200(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,168(%r15)
	alr	%r5,%r0		### 34
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r0		### 35
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r12,%r2		### Xupdate(36)
	lg	%r2,184(%r15)
	xg	%r12,208(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,176(%r15)
	alr	%r8,%r0		### 36
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r13
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r0		### 37
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r14,%r2		### Xupdate(38)
	lg	%r2,192(%r15)
	xg	%r14,216(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,184(%r15)
	alr	%r6,%r0		### 38
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r12
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r0		### 39
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	rllg	%r1,%r1,32
	xgr	%r13,%r2		### Xupdate(40)
	lg	%r2,200(%r15)
	xg	%r13,160(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,192(%r15)
	alr	%r9,%r1		### 40
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	or	%r10,%r7
	lr	%r11,%r6
	nr	%r10,%r8
	nr	%r11,%r7
	alr	%r9,%r14
	or	%r10,%r11
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r1		### 41
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	or	%r10,%r6
	lr	%r11,%r5
	nr	%r10,%r7
	nr	%r11,%r6
	alr	%r8,%r13
	or	%r10,%r11
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r12,%r2		### Xupdate(42)
	lg	%r2,208(%r15)
	xg	%r12,168(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,200(%r15)
	alr	%r7,%r1		### 42
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	or	%r10,%r5
	lr	%r11,%r9
	nr	%r10,%r6
	nr	%r11,%r5
	alr	%r7,%r13
	or	%r10,%r11
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r1		### 43
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	or	%r10,%r9
	lr	%r11,%r8
	nr	%r10,%r5
	nr	%r11,%r9
	alr	%r6,%r12
	or	%r10,%r11
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r14,%r2		### Xupdate(44)
	lg	%r2,216(%r15)
	xg	%r14,176(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,208(%r15)
	alr	%r5,%r1		### 44
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	or	%r10,%r8
	lr	%r11,%r7
	nr	%r10,%r9
	nr	%r11,%r8
	alr	%r5,%r12
	or	%r10,%r11
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r1		### 45
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	or	%r10,%r7
	lr	%r11,%r6
	nr	%r10,%r8
	nr	%r11,%r7
	alr	%r9,%r14
	or	%r10,%r11
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r13,%r2		### Xupdate(46)
	lg	%r2,160(%r15)
	xg	%r13,184(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,216(%r15)
	alr	%r8,%r1		### 46
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	or	%r10,%r6
	lr	%r11,%r5
	nr	%r10,%r7
	nr	%r11,%r6
	alr	%r8,%r14
	or	%r10,%r11
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r1		### 47
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	or	%r10,%r5
	lr	%r11,%r9
	nr	%r10,%r6
	nr	%r11,%r5
	alr	%r7,%r13
	or	%r10,%r11
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r12,%r2		### Xupdate(48)
	lg	%r2,168(%r15)
	xg	%r12,192(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,160(%r15)
	alr	%r6,%r1		### 48
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	or	%r10,%r9
	lr	%r11,%r8
	nr	%r10,%r5
	nr	%r11,%r9
	alr	%r6,%r13
	or	%r10,%r11
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r1		### 49
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	or	%r10,%r8
	lr	%r11,%r7
	nr	%r10,%r9
	nr	%r11,%r8
	alr	%r5,%r12
	or	%r10,%r11
	rll	%r7,%r7,30
	alr	%r5,%r10
	xgr	%r14,%r2		### Xupdate(50)
	lg	%r2,176(%r15)
	xg	%r14,200(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,168(%r15)
	alr	%r9,%r1		### 50
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	or	%r10,%r7
	lr	%r11,%r6
	nr	%r10,%r8
	nr	%r11,%r7
	alr	%r9,%r12
	or	%r10,%r11
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r1		### 51
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	or	%r10,%r6
	lr	%r11,%r5
	nr	%r10,%r7
	nr	%r11,%r6
	alr	%r8,%r14
	or	%r10,%r11
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r13,%r2		### Xupdate(52)
	lg	%r2,184(%r15)
	xg	%r13,208(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,176(%r15)
	alr	%r7,%r1		### 52
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	or	%r10,%r5
	lr	%r11,%r9
	nr	%r10,%r6
	nr	%r11,%r5
	alr	%r7,%r14
	or	%r10,%r11
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r1		### 53
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	or	%r10,%r9
	lr	%r11,%r8
	nr	%r10,%r5
	nr	%r11,%r9
	alr	%r6,%r13
	or	%r10,%r11
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r12,%r2		### Xupdate(54)
	lg	%r2,192(%r15)
	xg	%r12,216(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,184(%r15)
	alr	%r5,%r1		### 54
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	or	%r10,%r8
	lr	%r11,%r7
	nr	%r10,%r9
	nr	%r11,%r8
	alr	%r5,%r13
	or	%r10,%r11
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r1		### 55
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	or	%r10,%r7
	lr	%r11,%r6
	nr	%r10,%r8
	nr	%r11,%r7
	alr	%r9,%r12
	or	%r10,%r11
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r14,%r2		### Xupdate(56)
	lg	%r2,200(%r15)
	xg	%r14,160(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,192(%r15)
	alr	%r8,%r1		### 56
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	or	%r10,%r6
	lr	%r11,%r5
	nr	%r10,%r7
	nr	%r11,%r6
	alr	%r8,%r12
	or	%r10,%r11
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r1		### 57
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	or	%r10,%r5
	lr	%r11,%r9
	nr	%r10,%r6
	nr	%r11,%r5
	alr	%r7,%r14
	or	%r10,%r11
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r13,%r2		### Xupdate(58)
	lg	%r2,208(%r15)
	xg	%r13,168(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,200(%r15)
	alr	%r6,%r1		### 58
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	or	%r10,%r9
	lr	%r11,%r8
	nr	%r10,%r5
	nr	%r11,%r9
	alr	%r6,%r14
	or	%r10,%r11
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r1		### 59
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	or	%r10,%r8
	lr	%r11,%r7
	nr	%r10,%r9
	nr	%r11,%r8
	alr	%r5,%r13
	or	%r10,%r11
	rll	%r7,%r7,30
	alr	%r5,%r10
	rllg	%r1,%r1,32
	xgr	%r12,%r2		### Xupdate(60)
	lg	%r2,216(%r15)
	xg	%r12,176(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,208(%r15)
	alr	%r9,%r1		### 60
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r1		### 61
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r12
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r14,%r2		### Xupdate(62)
	lg	%r2,160(%r15)
	xg	%r14,184(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,216(%r15)
	alr	%r7,%r1		### 62
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r1		### 63
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r14
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r13,%r2		### Xupdate(64)
	lg	%r2,168(%r15)
	xg	%r13,192(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,160(%r15)
	alr	%r5,%r1		### 64
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r1		### 65
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r13
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r12,%r2		### Xupdate(66)
	lg	%r2,176(%r15)
	xg	%r12,200(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	stg	%r12,168(%r15)
	alr	%r8,%r1		### 66
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r13
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r1		### 67
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r12
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r14,%r2		### Xupdate(68)
	lg	%r2,184(%r15)
	xg	%r14,208(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	stg	%r14,176(%r15)
	alr	%r6,%r1		### 68
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r12
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r1		### 69
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r14
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	xgr	%r13,%r2		### Xupdate(70)
	lg	%r2,192(%r15)
	xg	%r13,216(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	stg	%r13,184(%r15)
	alr	%r9,%r1		### 70
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r14
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	alr	%r8,%r1		### 71
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r13
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	xgr	%r12,%r2		### Xupdate(72)
	lg	%r2,200(%r15)
	xg	%r12,160(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	alr	%r7,%r1		### 72
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r13
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	alr	%r6,%r1		### 73
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r12
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	xgr	%r14,%r2		### Xupdate(74)
	lg	%r2,208(%r15)
	xg	%r14,168(%r15)
	xgr	%r14,%r2
	rll	%r14,%r14,1
	rllg	%r12,%r14,32
	rll	%r12,%r12,1
	rllg	%r14,%r12,32
	lr	%r13,%r12		# feedback
	alr	%r5,%r1		### 74
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r12
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10
	alr	%r9,%r1		### 75
	rll	%r11,%r5,5
	lr	%r10,%r6
	alr	%r9,%r11
	xr	%r10,%r7
	alr	%r9,%r14
	xr	%r10,%r8
	rll	%r6,%r6,30
	alr	%r9,%r10
	xgr	%r13,%r2		### Xupdate(76)
	lg	%r2,216(%r15)
	xg	%r13,176(%r15)
	xgr	%r13,%r2
	rll	%r13,%r13,1
	rllg	%r14,%r13,32
	rll	%r14,%r14,1
	rllg	%r13,%r14,32
	lr	%r12,%r14		# feedback
	alr	%r8,%r1		### 76
	rll	%r11,%r9,5
	lr	%r10,%r5
	alr	%r8,%r11
	xr	%r10,%r6
	alr	%r8,%r14
	xr	%r10,%r7
	rll	%r5,%r5,30
	alr	%r8,%r10
	alr	%r7,%r1		### 77
	rll	%r11,%r8,5
	lr	%r10,%r9
	alr	%r7,%r11
	xr	%r10,%r5
	alr	%r7,%r13
	xr	%r10,%r6
	rll	%r9,%r9,30
	alr	%r7,%r10
	xgr	%r12,%r2		### Xupdate(78)
	lg	%r2,160(%r15)
	xg	%r12,184(%r15)
	xgr	%r12,%r2
	rll	%r12,%r12,1
	rllg	%r13,%r12,32
	rll	%r13,%r13,1
	rllg	%r12,%r13,32
	lr	%r14,%r13		# feedback
	alr	%r6,%r1		### 78
	rll	%r11,%r7,5
	lr	%r10,%r8
	alr	%r6,%r11
	xr	%r10,%r9
	alr	%r6,%r13
	xr	%r10,%r5
	rll	%r8,%r8,30
	alr	%r6,%r10
	alr	%r5,%r1		### 79
	rll	%r11,%r6,5
	lr	%r10,%r7
	alr	%r5,%r11
	xr	%r10,%r8
	alr	%r5,%r12
	xr	%r10,%r9
	rll	%r7,%r7,30
	alr	%r5,%r10

	lg	%r2,240(%r15)
	la	%r3,64(%r3)
	al	%r5,0(%r2)
	al	%r6,4(%r2)
	al	%r7,8(%r2)
	al	%r8,12(%r2)
	al	%r9,16(%r2)
	st	%r5,0(%r2)
	st	%r6,4(%r2)
	st	%r7,8(%r2)
	st	%r8,12(%r2)
	st	%r9,16(%r2)
	brctg %r4,.Lloop

	lmg	%r6,%r15,272(%r15)
	br	%r14
.size	sha1_block_data_order,.-sha1_block_data_order
.string	"SHA1 block transform for s390x, CRYPTOGAMS by <appro@openssl.org>"
