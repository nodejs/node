.text

.globl	OPENSSL_s390x_facilities
.type	OPENSSL_s390x_facilities,@function
.align	16
OPENSSL_s390x_facilities:
	lghi	%r0,0
	.long	0xb2b0f010	# stfle	16(%r15)
	lg	%r2,16(%r15)
	larl	%r1,OPENSSL_s390xcap_P
	stg	%r2,0(%r1)
	br	%r14
.size	OPENSSL_s390x_facilities,.-OPENSSL_s390x_facilities

.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.align	16
OPENSSL_rdtsc:
	stck	16(%r15)
	lg	%r2,16(%r15)
	br	%r14
.size	OPENSSL_rdtsc,.-OPENSSL_rdtsc

.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,@function
.align	16
OPENSSL_atomic_add:
	l	%r1,0(%r2)
.Lspin:	lr	%r0,%r1
	ar	%r0,%r3
	cs	%r1,%r0,0(%r2)
	brc	4,.Lspin
	lgfr	%r2,%r0		# OpenSSL expects the new value
	br	%r14
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,@function
.align	16
OPENSSL_wipe_cpu:
	xgr	%r0,%r0
	xgr	%r1,%r1
	lgr	%r2,%r15
	xgr	%r3,%r3
	xgr	%r4,%r4
	lzdr	%f0
	lzdr	%f1
	lzdr	%f2
	lzdr	%f3
	lzdr	%f4
	lzdr	%f5
	lzdr	%f6
	lzdr	%f7
	br	%r14
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu

.globl	OPENSSL_cleanse
.type	OPENSSL_cleanse,@function
.align	16
OPENSSL_cleanse:
	lghi	%r4,15
	lghi	%r0,0
	clgr	%r3,%r4
	jh	.Lot
	clgr	%r3,%r0
	bcr	8,%r14
.Little:
	stc	%r0,0(%r2)
	la	%r2,1(%r2)
	brctg	%r3,.Little
	br	%r14
.align	4
.Lot:	tmll	%r2,7
	jz	.Laligned
	stc	%r0,0(%r2)
	la	%r2,1(%r2)
	brctg	%r3,.Lot
.Laligned:
	srlg	%r4,%r3,3
.Loop:	stg	%r0,0(%r2)
	la	%r2,8(%r2)
	brctg	%r4,.Loop
	lghi	%r4,7
	ngr	%r3,%r4
	jnz	.Little
	br	%r14
.size	OPENSSL_cleanse,.-OPENSSL_cleanse

.section	.init
	brasl	%r14,OPENSSL_cpuid_setup

.comm	OPENSSL_s390xcap_P,8,8
