#include "s390x_arch.h"

.text

.globl	gcm_gmult_4bit
.align	32
gcm_gmult_4bit:
	stmg	%r6,%r14,6*8(%r15)

	aghi	%r2,-1
	lghi	%r5,1
	lghi	%r13,120
	larl	%r14,rem_4bit

	lg	%r1,8+1(%r2)		# Xi
	j	.Lgmult_shortcut
.type	gcm_gmult_4bit,@function
.size	gcm_gmult_4bit,(.-gcm_gmult_4bit)

.globl	gcm_ghash_4bit
.align	32
gcm_ghash_4bit:
	larl	%r1,OPENSSL_s390xcap_P
	lg	%r0,S390X_KIMD+8(%r1)	# load second word of kimd capabilities
					#  vector
	tmhh	%r0,0x4000	# check for function 65
	jz	.Lsoft_ghash
	lghi	%r0,S390X_GHASH	# function 65
	la	%r1,0(%r2)	# H lies right after Xi in gcm128_context
	.long	0xb93e0004	# kimd %r0,%r4
	brc	1,.-4		# pay attention to "partial completion"
	br	%r14
.align	32
.Lsoft_ghash:
	stmg	%r6,%r14,6*8(%r15)

	aghi	%r2,-1
	srlg	%r5,%r5,4
	lghi	%r13,120
	larl	%r14,rem_4bit

	lg	%r1,8+1(%r2)		# Xi
	lg	%r0,0+1(%r2)
	lghi	%r12,0
.Louter:
	xg	%r0,0(%r4)		# Xi ^= inp
	xg	%r1,8(%r4)
	xgr	%r0,%r12
	stg	%r1,8+1(%r2)
	stg	%r0,0+1(%r2)

.Lgmult_shortcut:
	lghi	%r12,0xf0
	sllg	%r8,%r1,4
	srlg	%r10,%r1,8		# extract second byte
	ngr	%r8,%r12
	lgr	%r9,%r1
	lghi	%r11,14
	ngr	%r9,%r12

	lg	%r1,8(%r8,%r3)
	lg	%r0,0(%r8,%r3)

	sllg	%r8,%r10,4
	sllg	%r6,%r1,3
	ngr	%r8,%r12
	ngr	%r6,%r13
	ngr	%r10,%r12

	sllg	%r12,%r0,60
	srlg	%r1,%r1,4
	srlg	%r0,%r0,4
	xg	%r1,8(%r9,%r3)
	xg	%r0,0(%r9,%r3)
	lgr	%r9,%r10
	sllg	%r7,%r1,3
	xgr	%r1,%r12
	ngr	%r7,%r13
	sllg	%r12,%r0,60
	j	.Lghash_inner
.align	16
.Lghash_inner:
	srlg	%r1,%r1,4
	srlg	%r0,%r0,4
	xg	%r1,8(%r8,%r3)
	llgc	%r10,0(%r11,%r2)
	xg	%r0,0(%r8,%r3)
	sllg	%r8,%r10,4
	xg	%r0,0(%r6,%r14)
	nill	%r8,0xf0
	sllg	%r6,%r1,3
	xgr	%r1,%r12
	ngr	%r6,%r13
	nill	%r10,0xf0

	sllg	%r12,%r0,60
	srlg	%r1,%r1,4
	srlg	%r0,%r0,4
	xg	%r1,8(%r9,%r3)
	xg	%r0,0(%r9,%r3)
	lgr	%r9,%r10
	xg	%r0,0(%r7,%r14)
	sllg	%r7,%r1,3
	xgr	%r1,%r12
	ngr	%r7,%r13
	sllg	%r12,%r0,60
	brct	%r11,.Lghash_inner

	srlg	%r1,%r1,4
	srlg	%r0,%r0,4
	xg	%r1,8(%r8,%r3)
	xg	%r0,0(%r8,%r3)
	sllg	%r10,%r1,3
	xg	%r0,0(%r6,%r14)
	xgr	%r1,%r12
	ngr	%r10,%r13

	sllg	%r12,%r0,60
	srlg	%r1,%r1,4
	srlg	%r0,%r0,4
	xg	%r1,8(%r9,%r3)
	xg	%r0,0(%r9,%r3)
	xgr	%r1,%r12
	xg	%r0,0(%r7,%r14)

	lg	%r12,0(%r10,%r14)
	la	%r4,16(%r4)
	sllg	%r12,%r12,4		# correct last rem_4bit[rem]
	brctg	%r5,.Louter

	xgr	%r0,%r12
	stg	%r1,8+1(%r2)
	stg	%r0,0+1(%r2)
	lmg	%r6,%r14,6*8(%r15)
	br	%r14
.type	gcm_ghash_4bit,@function
.size	gcm_ghash_4bit,(.-gcm_ghash_4bit)

.align	64
rem_4bit:
	.long	0,0,29491200,0,58982400,0,38141952,0
	.long	117964800,0,113901568,0,76283904,0,88997888,0
	.long	235929600,0,265420800,0,227803136,0,206962688,0
	.long	152567808,0,148504576,0,177995776,0,190709760,0
.type	rem_4bit,@object
.size	rem_4bit,(.-rem_4bit)
.string	"GHASH for s390x, CRYPTOGAMS by <appro@openssl.org>"
