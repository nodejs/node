#include "s390x_arch.h"

.text
.align	64
.type	K512,@object
K512:
	.quad	0x428a2f98d728ae22,0x7137449123ef65cd
	.quad	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	.quad	0x3956c25bf348b538,0x59f111f1b605d019
	.quad	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	.quad	0xd807aa98a3030242,0x12835b0145706fbe
	.quad	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	.quad	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	.quad	0x9bdc06a725c71235,0xc19bf174cf692694
	.quad	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	.quad	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	.quad	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	.quad	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	.quad	0x983e5152ee66dfab,0xa831c66d2db43210
	.quad	0xb00327c898fb213f,0xbf597fc7beef0ee4
	.quad	0xc6e00bf33da88fc2,0xd5a79147930aa725
	.quad	0x06ca6351e003826f,0x142929670a0e6e70
	.quad	0x27b70a8546d22ffc,0x2e1b21385c26c926
	.quad	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	.quad	0x650a73548baf63de,0x766a0abb3c77b2a8
	.quad	0x81c2c92e47edaee6,0x92722c851482353b
	.quad	0xa2bfe8a14cf10364,0xa81a664bbc423001
	.quad	0xc24b8b70d0f89791,0xc76c51a30654be30
	.quad	0xd192e819d6ef5218,0xd69906245565a910
	.quad	0xf40e35855771202a,0x106aa07032bbd1b8
	.quad	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	.quad	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	.quad	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	.quad	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	.quad	0x748f82ee5defb2fc,0x78a5636f43172f60
	.quad	0x84c87814a1f0ab72,0x8cc702081a6439ec
	.quad	0x90befffa23631e28,0xa4506cebde82bde9
	.quad	0xbef9a3f7b2c67915,0xc67178f2e372532b
	.quad	0xca273eceea26619c,0xd186b8c721c0c207
	.quad	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	.quad	0x06f067aa72176fba,0x0a637dc5a2c898a6
	.quad	0x113f9804bef90dae,0x1b710b35131c471b
	.quad	0x28db77f523047d84,0x32caab7b40c72493
	.quad	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	.quad	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	.quad	0x5fcb6fab3ad6faec,0x6c44198c4a475817
.size	K512,.-K512
.globl	sha512_block_data_order
.type	sha512_block_data_order,@function
sha512_block_data_order:
	sllg	%r4,%r4,7
	larl	%r1,OPENSSL_s390xcap_P
	lg	%r0,S390X_KIMD(%r1)	# check kimd capabilities
	tmhh	%r0,4096
	jz	.Lsoftware
	lghi	%r0,3
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
	stm	%r2,%r15,8(%r15)
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	st	%r0,0(%r15)

	larl	%r13,K512
	lg	%r5,0(%r2)
	lg	%r6,8(%r2)
	lg	%r7,16(%r2)
	lg	%r8,24(%r2)
	lg	%r9,32(%r2)
	lg	%r10,40(%r2)
	lg	%r11,48(%r2)
	lg	%r12,56(%r2)

.Lloop:
	lghi	%r4,0
	lg	%r14,0(%r3)	### 0
	rllg	%r0,%r9,23
	rllg	%r1,%r9,46
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r11
	stg	%r14,96(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r12,%r5,25
	 xgr	%r2,%r11			# Ch(e,f,g)
	alg	%r14,0(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r5,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rllg	%r0,%r0,6
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	lg	%r14,8(%r3)	### 1
	rllg	%r0,%r8,23
	rllg	%r1,%r8,46
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r10
	stg	%r14,104(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r11,%r12,25
	 xgr	%r2,%r10			# Ch(e,f,g)
	alg	%r14,8(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r12,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rllg	%r0,%r0,6
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	lg	%r14,16(%r3)	### 2
	rllg	%r0,%r7,23
	rllg	%r1,%r7,46
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r9
	stg	%r14,112(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r10,%r11,25
	 xgr	%r2,%r9			# Ch(e,f,g)
	alg	%r14,16(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r11,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rllg	%r0,%r0,6
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	lg	%r14,24(%r3)	### 3
	rllg	%r0,%r6,23
	rllg	%r1,%r6,46
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r8
	stg	%r14,120(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r9,%r10,25
	 xgr	%r2,%r8			# Ch(e,f,g)
	alg	%r14,24(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r10,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rllg	%r0,%r0,6
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	lg	%r14,32(%r3)	### 4
	rllg	%r0,%r5,23
	rllg	%r1,%r5,46
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r7
	stg	%r14,128(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r8,%r9,25
	 xgr	%r2,%r7			# Ch(e,f,g)
	alg	%r14,32(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r9,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rllg	%r0,%r0,6
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	lg	%r14,40(%r3)	### 5
	rllg	%r0,%r12,23
	rllg	%r1,%r12,46
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r6
	stg	%r14,136(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r7,%r8,25
	 xgr	%r2,%r6			# Ch(e,f,g)
	alg	%r14,40(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r8,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rllg	%r0,%r0,6
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	lg	%r14,48(%r3)	### 6
	rllg	%r0,%r11,23
	rllg	%r1,%r11,46
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r5
	stg	%r14,144(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r6,%r7,25
	 xgr	%r2,%r5			# Ch(e,f,g)
	alg	%r14,48(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r7,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rllg	%r0,%r0,6
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	lg	%r14,56(%r3)	### 7
	rllg	%r0,%r10,23
	rllg	%r1,%r10,46
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r12
	stg	%r14,152(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r5,%r6,25
	 xgr	%r2,%r12			# Ch(e,f,g)
	alg	%r14,56(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r6,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rllg	%r0,%r0,6
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	lg	%r14,64(%r3)	### 8
	rllg	%r0,%r9,23
	rllg	%r1,%r9,46
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r11
	stg	%r14,160(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r12,%r5,25
	 xgr	%r2,%r11			# Ch(e,f,g)
	alg	%r14,64(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r5,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rllg	%r0,%r0,6
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	lg	%r14,72(%r3)	### 9
	rllg	%r0,%r8,23
	rllg	%r1,%r8,46
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r10
	stg	%r14,168(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r11,%r12,25
	 xgr	%r2,%r10			# Ch(e,f,g)
	alg	%r14,72(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r12,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rllg	%r0,%r0,6
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	lg	%r14,80(%r3)	### 10
	rllg	%r0,%r7,23
	rllg	%r1,%r7,46
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r9
	stg	%r14,176(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r10,%r11,25
	 xgr	%r2,%r9			# Ch(e,f,g)
	alg	%r14,80(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r11,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rllg	%r0,%r0,6
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	lg	%r14,88(%r3)	### 11
	rllg	%r0,%r6,23
	rllg	%r1,%r6,46
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r8
	stg	%r14,184(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r9,%r10,25
	 xgr	%r2,%r8			# Ch(e,f,g)
	alg	%r14,88(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r10,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rllg	%r0,%r0,6
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	lg	%r14,96(%r3)	### 12
	rllg	%r0,%r5,23
	rllg	%r1,%r5,46
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r7
	stg	%r14,192(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r8,%r9,25
	 xgr	%r2,%r7			# Ch(e,f,g)
	alg	%r14,96(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r9,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rllg	%r0,%r0,6
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	lg	%r14,104(%r3)	### 13
	rllg	%r0,%r12,23
	rllg	%r1,%r12,46
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r6
	stg	%r14,200(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r7,%r8,25
	 xgr	%r2,%r6			# Ch(e,f,g)
	alg	%r14,104(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r8,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rllg	%r0,%r0,6
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	lg	%r14,112(%r3)	### 14
	rllg	%r0,%r11,23
	rllg	%r1,%r11,46
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r5
	stg	%r14,208(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r6,%r7,25
	 xgr	%r2,%r5			# Ch(e,f,g)
	alg	%r14,112(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r7,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rllg	%r0,%r0,6
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	lg	%r14,120(%r3)	### 15
	rllg	%r0,%r10,23
	rllg	%r1,%r10,46
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r12
	stg	%r14,216(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r5,%r6,25
	 xgr	%r2,%r12			# Ch(e,f,g)
	alg	%r14,120(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r6,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rllg	%r0,%r0,6
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
.Lrounds_16_xx:
	lg	%r14,104(%r15)	### 16
	lg	%r1,208(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,96(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,168(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r9,23
	rllg	%r1,%r9,46
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r11
	stg	%r14,96(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r12,%r5,25
	 xgr	%r2,%r11			# Ch(e,f,g)
	alg	%r14,128(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r5,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rllg	%r0,%r0,6
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	lg	%r14,112(%r15)	### 17
	lg	%r1,216(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,104(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,176(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r8,23
	rllg	%r1,%r8,46
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r10
	stg	%r14,104(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r11,%r12,25
	 xgr	%r2,%r10			# Ch(e,f,g)
	alg	%r14,136(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r12,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rllg	%r0,%r0,6
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	lg	%r14,120(%r15)	### 18
	lg	%r1,96(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,112(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,184(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r7,23
	rllg	%r1,%r7,46
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r9
	stg	%r14,112(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r10,%r11,25
	 xgr	%r2,%r9			# Ch(e,f,g)
	alg	%r14,144(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r11,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rllg	%r0,%r0,6
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	lg	%r14,128(%r15)	### 19
	lg	%r1,104(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,120(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,192(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r6,23
	rllg	%r1,%r6,46
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r8
	stg	%r14,120(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r9,%r10,25
	 xgr	%r2,%r8			# Ch(e,f,g)
	alg	%r14,152(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r10,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rllg	%r0,%r0,6
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	lg	%r14,136(%r15)	### 20
	lg	%r1,112(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,128(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,200(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r5,23
	rllg	%r1,%r5,46
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r7
	stg	%r14,128(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r8,%r9,25
	 xgr	%r2,%r7			# Ch(e,f,g)
	alg	%r14,160(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r9,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rllg	%r0,%r0,6
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	lg	%r14,144(%r15)	### 21
	lg	%r1,120(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,136(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,208(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r12,23
	rllg	%r1,%r12,46
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r6
	stg	%r14,136(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r7,%r8,25
	 xgr	%r2,%r6			# Ch(e,f,g)
	alg	%r14,168(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r8,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rllg	%r0,%r0,6
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	lg	%r14,152(%r15)	### 22
	lg	%r1,128(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,144(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,216(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r11,23
	rllg	%r1,%r11,46
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r5
	stg	%r14,144(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r6,%r7,25
	 xgr	%r2,%r5			# Ch(e,f,g)
	alg	%r14,176(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r7,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rllg	%r0,%r0,6
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	lg	%r14,160(%r15)	### 23
	lg	%r1,136(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,152(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,96(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r10,23
	rllg	%r1,%r10,46
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r12
	stg	%r14,152(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r5,%r6,25
	 xgr	%r2,%r12			# Ch(e,f,g)
	alg	%r14,184(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r6,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rllg	%r0,%r0,6
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	lg	%r14,168(%r15)	### 24
	lg	%r1,144(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,160(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,104(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r9,23
	rllg	%r1,%r9,46
	 lgr	%r2,%r10
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r11
	stg	%r14,160(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r12			# T1+=h
	 ngr	%r2,%r9
	 lgr	%r1,%r5
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r12,%r5,25
	 xgr	%r2,%r11			# Ch(e,f,g)
	alg	%r14,192(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r5,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r6
	xgr	%r12,%r0
	 lgr	%r2,%r5
	 ngr	%r1,%r7
	rllg	%r0,%r0,6
	xgr	%r12,%r0			# h=Sigma0(a)
	 ngr	%r2,%r6
	algr	%r12,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r8,%r14			# d+=T1
	algr	%r12,%r2			# h+=Maj(a,b,c)
	lg	%r14,176(%r15)	### 25
	lg	%r1,152(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,168(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,112(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r8,23
	rllg	%r1,%r8,46
	 lgr	%r2,%r9
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r10
	stg	%r14,168(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r11			# T1+=h
	 ngr	%r2,%r8
	 lgr	%r1,%r12
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r11,%r12,25
	 xgr	%r2,%r10			# Ch(e,f,g)
	alg	%r14,200(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r12,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r5
	xgr	%r11,%r0
	 lgr	%r2,%r12
	 ngr	%r1,%r6
	rllg	%r0,%r0,6
	xgr	%r11,%r0			# h=Sigma0(a)
	 ngr	%r2,%r5
	algr	%r11,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r7,%r14			# d+=T1
	algr	%r11,%r2			# h+=Maj(a,b,c)
	lg	%r14,184(%r15)	### 26
	lg	%r1,160(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,176(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,120(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r7,23
	rllg	%r1,%r7,46
	 lgr	%r2,%r8
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r9
	stg	%r14,176(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r10			# T1+=h
	 ngr	%r2,%r7
	 lgr	%r1,%r11
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r10,%r11,25
	 xgr	%r2,%r9			# Ch(e,f,g)
	alg	%r14,208(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r11,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r12
	xgr	%r10,%r0
	 lgr	%r2,%r11
	 ngr	%r1,%r5
	rllg	%r0,%r0,6
	xgr	%r10,%r0			# h=Sigma0(a)
	 ngr	%r2,%r12
	algr	%r10,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r6,%r14			# d+=T1
	algr	%r10,%r2			# h+=Maj(a,b,c)
	lg	%r14,192(%r15)	### 27
	lg	%r1,168(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,184(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,128(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r6,23
	rllg	%r1,%r6,46
	 lgr	%r2,%r7
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r8
	stg	%r14,184(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r9			# T1+=h
	 ngr	%r2,%r6
	 lgr	%r1,%r10
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r9,%r10,25
	 xgr	%r2,%r8			# Ch(e,f,g)
	alg	%r14,216(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r10,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r11
	xgr	%r9,%r0
	 lgr	%r2,%r10
	 ngr	%r1,%r12
	rllg	%r0,%r0,6
	xgr	%r9,%r0			# h=Sigma0(a)
	 ngr	%r2,%r11
	algr	%r9,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r5,%r14			# d+=T1
	algr	%r9,%r2			# h+=Maj(a,b,c)
	lg	%r14,200(%r15)	### 28
	lg	%r1,176(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,192(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,136(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r5,23
	rllg	%r1,%r5,46
	 lgr	%r2,%r6
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r7
	stg	%r14,192(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r8			# T1+=h
	 ngr	%r2,%r5
	 lgr	%r1,%r9
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r8,%r9,25
	 xgr	%r2,%r7			# Ch(e,f,g)
	alg	%r14,224(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r9,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r10
	xgr	%r8,%r0
	 lgr	%r2,%r9
	 ngr	%r1,%r11
	rllg	%r0,%r0,6
	xgr	%r8,%r0			# h=Sigma0(a)
	 ngr	%r2,%r10
	algr	%r8,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r12,%r14			# d+=T1
	algr	%r8,%r2			# h+=Maj(a,b,c)
	lg	%r14,208(%r15)	### 29
	lg	%r1,184(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,200(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,144(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r12,23
	rllg	%r1,%r12,46
	 lgr	%r2,%r5
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r6
	stg	%r14,200(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r7			# T1+=h
	 ngr	%r2,%r12
	 lgr	%r1,%r8
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r7,%r8,25
	 xgr	%r2,%r6			# Ch(e,f,g)
	alg	%r14,232(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r8,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r9
	xgr	%r7,%r0
	 lgr	%r2,%r8
	 ngr	%r1,%r10
	rllg	%r0,%r0,6
	xgr	%r7,%r0			# h=Sigma0(a)
	 ngr	%r2,%r9
	algr	%r7,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r11,%r14			# d+=T1
	algr	%r7,%r2			# h+=Maj(a,b,c)
	lg	%r14,216(%r15)	### 30
	lg	%r1,192(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,208(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,152(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r11,23
	rllg	%r1,%r11,46
	 lgr	%r2,%r12
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r5
	stg	%r14,208(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r6			# T1+=h
	 ngr	%r2,%r11
	 lgr	%r1,%r7
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r6,%r7,25
	 xgr	%r2,%r5			# Ch(e,f,g)
	alg	%r14,240(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r7,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r8
	xgr	%r6,%r0
	 lgr	%r2,%r7
	 ngr	%r1,%r9
	rllg	%r0,%r0,6
	xgr	%r6,%r0			# h=Sigma0(a)
	 ngr	%r2,%r8
	algr	%r6,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r10,%r14			# d+=T1
	algr	%r6,%r2			# h+=Maj(a,b,c)
	lg	%r14,96(%r15)	### 31
	lg	%r1,200(%r15)
	rllg	%r0,%r14,56
	srlg	%r14,%r14,7
	rllg	%r2,%r0,7
	xgr	%r14,%r0
	rllg	%r0,%r1,3
	xgr	%r14,%r2					# sigma0(X[i+1])
	srlg	%r1,%r1,6
	alg	%r14,216(%r15)	# +=X[i]
	xgr	%r1,%r0
	rllg	%r0,%r0,42
	alg	%r14,160(%r15)	# +=X[i+9]
	xgr	%r1,%r0				# sigma1(X[i+14])
	algr	%r14,%r1				# +=sigma1(X[i+14])
	rllg	%r0,%r10,23
	rllg	%r1,%r10,46
	 lgr	%r2,%r11
	xgr	%r0,%r1
	rllg	%r1,%r1,4
	 xgr	%r2,%r12
	stg	%r14,216(%r15)
	xgr	%r0,%r1			# Sigma1(e)
	algr	%r14,%r5			# T1+=h
	 ngr	%r2,%r10
	 lgr	%r1,%r6
	algr	%r14,%r0			# T1+=Sigma1(e)
	rllg	%r5,%r6,25
	 xgr	%r2,%r12			# Ch(e,f,g)
	alg	%r14,248(%r4,%r13)	# T1+=K[i]
	rllg	%r0,%r6,30
	algr	%r14,%r2			# T1+=Ch(e,f,g)
	 ogr	%r1,%r7
	xgr	%r5,%r0
	 lgr	%r2,%r6
	 ngr	%r1,%r8
	rllg	%r0,%r0,6
	xgr	%r5,%r0			# h=Sigma0(a)
	 ngr	%r2,%r7
	algr	%r5,%r14			# h+=T1
	 ogr	%r2,%r1			# Maj(a,b,c)
	algr	%r9,%r14			# d+=T1
	algr	%r5,%r2			# h+=Maj(a,b,c)
	aghi	%r4,128
	lghi	%r0,512
	clgr	%r4,%r0
	jne	.Lrounds_16_xx

	l	%r2,232(%r15)
	la	%r3,128(%r3)
	alg	%r5,0(%r2)
	alg	%r6,8(%r2)
	alg	%r7,16(%r2)
	alg	%r8,24(%r2)
	alg	%r9,32(%r2)
	alg	%r10,40(%r2)
	alg	%r11,48(%r2)
	alg	%r12,56(%r2)
	stg	%r5,0(%r2)
	stg	%r6,8(%r2)
	stg	%r7,16(%r2)
	stg	%r8,24(%r2)
	stg	%r9,32(%r2)
	stg	%r10,40(%r2)
	stg	%r11,48(%r2)
	stg	%r12,56(%r2)
	cl	%r3,240(%r15)
	jne	.Lloop

	lm	%r6,%r15,248(%r15)
	br	%r14
.size	sha512_block_data_order,.-sha512_block_data_order
.string	"SHA512 block transform for s390x, CRYPTOGAMS by <appro@openssl.org>"
