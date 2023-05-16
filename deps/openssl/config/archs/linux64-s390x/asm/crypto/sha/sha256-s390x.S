#include "s390x_arch.h"

.text
.align	64
.type	K256,@object
K256:
	.long	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
	.long	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
	.long	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
	.long	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
	.long	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
	.long	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
	.long	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
	.long	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
	.long	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
	.long	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
	.long	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
	.long	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
	.long	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
	.long	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
	.long	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
	.long	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
.size	K256,.-K256
.globl	sha256_block_data_order
.type	sha256_block_data_order,@function
sha256_block_data_order:
	sllg	%r4,%r4,6
	larl	%r1,OPENSSL_s390xcap_P
	lg	%r0,S390X_KIMD(%r1)	# check kimd capabilities
	tmhh	%r0,8192
	jz	.Lsoftware
	lghi	%r0,2
	lgr	%r1,%r2
	lgr	%r2,%r3
	lgr	%r3,%r4
	.long	0xb93e0002	# kimd %r0,%r2
	brc	1,.-4		# pay attention to "partial completion"
	br	%r14
.align	16
.Lsoftware:
	lghi	%r1,-224
	la	%r4,0(%r4,%r3)
	stmg	%r2,%r15,16(%r15)
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)

	larl	%r13,K256
	llgf	%r5,0(%r2)
	llgf	%r6,4(%r2)
	llgf	%r7,8(%r2)
	llgf	%r8,12(%r2)
	llgf	%r9,16(%r2)
	llgf	%r10,20(%r2)
	llgf	%r11,24(%r2)
	llgf	%r12,28(%r2)

.Lloop:
	lghi	%r4,0
	llgf	%r14,0(%r3)	### 0
	rll	%r0,%r9,7
	rll	%r1,%r9,21
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r11
	st	%r14,160(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r12,%r5,10
	 xgr	%r2,%r11			# Ch(e,f,g)
	al	%r14,0(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r5,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rll	%r0,%r0,11
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	llgf	%r14,4(%r3)	### 1
	rll	%r0,%r8,7
	rll	%r1,%r8,21
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r10
	st	%r14,164(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r11,%r12,10
	 xgr	%r2,%r10			# Ch(e,f,g)
	al	%r14,4(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r12,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rll	%r0,%r0,11
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	llgf	%r14,8(%r3)	### 2
	rll	%r0,%r7,7
	rll	%r1,%r7,21
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r9
	st	%r14,168(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r10,%r11,10
	 xgr	%r2,%r9			# Ch(e,f,g)
	al	%r14,8(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r11,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rll	%r0,%r0,11
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	llgf	%r14,12(%r3)	### 3
	rll	%r0,%r6,7
	rll	%r1,%r6,21
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r8
	st	%r14,172(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r9,%r10,10
	 xgr	%r2,%r8			# Ch(e,f,g)
	al	%r14,12(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r10,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rll	%r0,%r0,11
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	llgf	%r14,16(%r3)	### 4
	rll	%r0,%r5,7
	rll	%r1,%r5,21
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r7
	st	%r14,176(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r8,%r9,10
	 xgr	%r2,%r7			# Ch(e,f,g)
	al	%r14,16(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r9,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rll	%r0,%r0,11
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	llgf	%r14,20(%r3)	### 5
	rll	%r0,%r12,7
	rll	%r1,%r12,21
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r6
	st	%r14,180(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r7,%r8,10
	 xgr	%r2,%r6			# Ch(e,f,g)
	al	%r14,20(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r8,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rll	%r0,%r0,11
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	llgf	%r14,24(%r3)	### 6
	rll	%r0,%r11,7
	rll	%r1,%r11,21
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r5
	st	%r14,184(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r6,%r7,10
	 xgr	%r2,%r5			# Ch(e,f,g)
	al	%r14,24(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r7,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rll	%r0,%r0,11
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	llgf	%r14,28(%r3)	### 7
	rll	%r0,%r10,7
	rll	%r1,%r10,21
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r12
	st	%r14,188(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r5,%r6,10
	 xgr	%r2,%r12			# Ch(e,f,g)
	al	%r14,28(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r6,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rll	%r0,%r0,11
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	llgf	%r14,32(%r3)	### 8
	rll	%r0,%r9,7
	rll	%r1,%r9,21
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r11
	st	%r14,192(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r12,%r5,10
	 xgr	%r2,%r11			# Ch(e,f,g)
	al	%r14,32(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r5,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rll	%r0,%r0,11
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	llgf	%r14,36(%r3)	### 9
	rll	%r0,%r8,7
	rll	%r1,%r8,21
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r10
	st	%r14,196(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r11,%r12,10
	 xgr	%r2,%r10			# Ch(e,f,g)
	al	%r14,36(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r12,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rll	%r0,%r0,11
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	llgf	%r14,40(%r3)	### 10
	rll	%r0,%r7,7
	rll	%r1,%r7,21
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r9
	st	%r14,200(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r10,%r11,10
	 xgr	%r2,%r9			# Ch(e,f,g)
	al	%r14,40(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r11,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rll	%r0,%r0,11
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	llgf	%r14,44(%r3)	### 11
	rll	%r0,%r6,7
	rll	%r1,%r6,21
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r8
	st	%r14,204(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r9,%r10,10
	 xgr	%r2,%r8			# Ch(e,f,g)
	al	%r14,44(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r10,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rll	%r0,%r0,11
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	llgf	%r14,48(%r3)	### 12
	rll	%r0,%r5,7
	rll	%r1,%r5,21
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r7
	st	%r14,208(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r8,%r9,10
	 xgr	%r2,%r7			# Ch(e,f,g)
	al	%r14,48(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r9,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rll	%r0,%r0,11
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	llgf	%r14,52(%r3)	### 13
	rll	%r0,%r12,7
	rll	%r1,%r12,21
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r6
	st	%r14,212(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r7,%r8,10
	 xgr	%r2,%r6			# Ch(e,f,g)
	al	%r14,52(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r8,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rll	%r0,%r0,11
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	llgf	%r14,56(%r3)	### 14
	rll	%r0,%r11,7
	rll	%r1,%r11,21
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r5
	st	%r14,216(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r6,%r7,10
	 xgr	%r2,%r5			# Ch(e,f,g)
	al	%r14,56(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r7,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rll	%r0,%r0,11
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	llgf	%r14,60(%r3)	### 15
	rll	%r0,%r10,7
	rll	%r1,%r10,21
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r12
	st	%r14,220(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r5,%r6,10
	 xgr	%r2,%r12			# Ch(e,f,g)
	al	%r14,60(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r6,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rll	%r0,%r0,11
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
.Lrounds_16_xx:
	llgf	%r14,164(%r15)	### 16
	llgf	%r1,216(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,160(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,196(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r9,7
	rll	%r1,%r9,21
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r11
	st	%r14,160(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r12,%r5,10
	 xgr	%r2,%r11			# Ch(e,f,g)
	al	%r14,64(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r5,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rll	%r0,%r0,11
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	llgf	%r14,168(%r15)	### 17
	llgf	%r1,220(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,164(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,200(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r8,7
	rll	%r1,%r8,21
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r10
	st	%r14,164(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r11,%r12,10
	 xgr	%r2,%r10			# Ch(e,f,g)
	al	%r14,68(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r12,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rll	%r0,%r0,11
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	llgf	%r14,172(%r15)	### 18
	llgf	%r1,160(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,168(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,204(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r7,7
	rll	%r1,%r7,21
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r9
	st	%r14,168(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r10,%r11,10
	 xgr	%r2,%r9			# Ch(e,f,g)
	al	%r14,72(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r11,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rll	%r0,%r0,11
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	llgf	%r14,176(%r15)	### 19
	llgf	%r1,164(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,172(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,208(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r6,7
	rll	%r1,%r6,21
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r8
	st	%r14,172(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r9,%r10,10
	 xgr	%r2,%r8			# Ch(e,f,g)
	al	%r14,76(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r10,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rll	%r0,%r0,11
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	llgf	%r14,180(%r15)	### 20
	llgf	%r1,168(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,176(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,212(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r5,7
	rll	%r1,%r5,21
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r7
	st	%r14,176(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r8,%r9,10
	 xgr	%r2,%r7			# Ch(e,f,g)
	al	%r14,80(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r9,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rll	%r0,%r0,11
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	llgf	%r14,184(%r15)	### 21
	llgf	%r1,172(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,180(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,216(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r12,7
	rll	%r1,%r12,21
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r6
	st	%r14,180(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r7,%r8,10
	 xgr	%r2,%r6			# Ch(e,f,g)
	al	%r14,84(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r8,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rll	%r0,%r0,11
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	llgf	%r14,188(%r15)	### 22
	llgf	%r1,176(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,184(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,220(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r11,7
	rll	%r1,%r11,21
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r5
	st	%r14,184(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r6,%r7,10
	 xgr	%r2,%r5			# Ch(e,f,g)
	al	%r14,88(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r7,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rll	%r0,%r0,11
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	llgf	%r14,192(%r15)	### 23
	llgf	%r1,180(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,188(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,160(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r10,7
	rll	%r1,%r10,21
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r12
	st	%r14,188(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r5,%r6,10
	 xgr	%r2,%r12			# Ch(e,f,g)
	al	%r14,92(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r6,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rll	%r0,%r0,11
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	llgf	%r14,196(%r15)	### 24
	llgf	%r1,184(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,192(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,164(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r9,7
	rll	%r1,%r9,21
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r11
	st	%r14,192(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r12,%r5,10
	 xgr	%r2,%r11			# Ch(e,f,g)
	al	%r14,96(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r5,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rll	%r0,%r0,11
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	llgf	%r14,200(%r15)	### 25
	llgf	%r1,188(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,196(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,168(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r8,7
	rll	%r1,%r8,21
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r10
	st	%r14,196(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r11,%r12,10
	 xgr	%r2,%r10			# Ch(e,f,g)
	al	%r14,100(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r12,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rll	%r0,%r0,11
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	llgf	%r14,204(%r15)	### 26
	llgf	%r1,192(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,200(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,172(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r7,7
	rll	%r1,%r7,21
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r9
	st	%r14,200(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r10,%r11,10
	 xgr	%r2,%r9			# Ch(e,f,g)
	al	%r14,104(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r11,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rll	%r0,%r0,11
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	llgf	%r14,208(%r15)	### 27
	llgf	%r1,196(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,204(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,176(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r6,7
	rll	%r1,%r6,21
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r8
	st	%r14,204(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r9,%r10,10
	 xgr	%r2,%r8			# Ch(e,f,g)
	al	%r14,108(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r10,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rll	%r0,%r0,11
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	llgf	%r14,212(%r15)	### 28
	llgf	%r1,200(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,208(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,180(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r5,7
	rll	%r1,%r5,21
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r7
	st	%r14,208(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r8,%r9,10
	 xgr	%r2,%r7			# Ch(e,f,g)
	al	%r14,112(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r9,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rll	%r0,%r0,11
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	llgf	%r14,216(%r15)	### 29
	llgf	%r1,204(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,212(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,184(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r12,7
	rll	%r1,%r12,21
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r6
	st	%r14,212(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r7,%r8,10
	 xgr	%r2,%r6			# Ch(e,f,g)
	al	%r14,116(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r8,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rll	%r0,%r0,11
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	llgf	%r14,220(%r15)	### 30
	llgf	%r1,208(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,216(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,188(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r11,7
	rll	%r1,%r11,21
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r5
	st	%r14,216(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r6,%r7,10
	 xgr	%r2,%r5			# Ch(e,f,g)
	al	%r14,120(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r7,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rll	%r0,%r0,11
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	llgf	%r14,160(%r15)	### 31
	llgf	%r1,212(%r15)
	rll	%r0,%r14,14
	srl	%r14,3
	rll	%r2,%r0,11
	xgr	%r14,%r0
	rll	%r0,%r1,13
	xgr	%r14,%r2					# sigma0(X[i+1])
	srl	%r1,10
	al	%r14,220(%r15)	# +=X[i]
	xgr	%r1,%r0
	rll	%r0,%r0,2
	al	%r14,192(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rll	%r0,%r10,7
	rll	%r1,%r10,21
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rll	%r1,%r1,5
	 xgr	%r2,%r12
	st	%r14,220(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rll	%r5,%r6,10
	 xgr	%r2,%r12			# Ch(e,f,g)
	al	%r14,124(%r4,%r13)	# T1+=K[i]
	rll	%r0,%r6,19
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rll	%r0,%r0,11
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	aghi	%r4,64
	lghi	%r0,192
	clgr	%r4,%r0
	jne	.Lrounds_16_xx

	lg	%r2,240(%r15)
	la	%r3,64(%r3)
	al	%r5,0(%r2)
	al	%r6,4(%r2)
	al	%r7,8(%r2)
	al	%r8,12(%r2)
	al	%r9,16(%r2)
	al	%r10,20(%r2)
	al	%r11,24(%r2)
	al	%r12,28(%r2)
	st	%r5,0(%r2)
	st	%r6,4(%r2)
	st	%r7,8(%r2)
	st	%r8,12(%r2)
	st	%r9,16(%r2)
	st	%r10,20(%r2)
	st	%r11,24(%r2)
	st	%r12,28(%r2)
	clg	%r3,256(%r15)
	jne	.Lloop

	lmg	%r6,%r15,272(%r15)
	br	%r14
.size	sha256_block_data_order,.-sha256_block_data_order
.string	"SHA256 block transform for s390x, CRYPTOGAMS by <appro@openssl.org>"
