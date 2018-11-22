#include "s390x_arch.h"

.text

.globl	OPENSSL_s390x_facilities
.type	OPENSSL_s390x_facilities,@function
.align	16
OPENSSL_s390x_facilities:
	lghi	%r0,0
	larl	%r4,OPENSSL_s390xcap_P

	stg	%r0,S390X_STFLE+8(%r4)	# wipe capability vectors
	stg	%r0,S390X_STFLE+16(%r4)
	stg	%r0,S390X_STFLE+24(%r4)
	stg	%r0,S390X_KIMD(%r4)
	stg	%r0,S390X_KIMD+8(%r4)
	stg	%r0,S390X_KLMD(%r4)
	stg	%r0,S390X_KLMD+8(%r4)
	stg	%r0,S390X_KM(%r4)
	stg	%r0,S390X_KM+8(%r4)
	stg	%r0,S390X_KMC(%r4)
	stg	%r0,S390X_KMC+8(%r4)
	stg	%r0,S390X_KMAC(%r4)
	stg	%r0,S390X_KMAC+8(%r4)
	stg	%r0,S390X_KMCTR(%r4)
	stg	%r0,S390X_KMCTR+8(%r4)
	stg	%r0,S390X_KMO(%r4)
	stg	%r0,S390X_KMO+8(%r4)
	stg	%r0,S390X_KMF(%r4)
	stg	%r0,S390X_KMF+8(%r4)
	stg	%r0,S390X_PRNO(%r4)
	stg	%r0,S390X_PRNO+8(%r4)
	stg	%r0,S390X_KMA(%r4)
	stg	%r0,S390X_KMA+8(%r4)

	.long	0xb2b04000		# stfle	0(%r4)
	brc	8,.Ldone
	lghi	%r0,1
	.long	0xb2b04000		# stfle 0(%r4)
	brc	8,.Ldone
	lghi	%r0,2
	.long	0xb2b04000		# stfle 0(%r4)
.Ldone:
	lmg	%r2,%r3,S390X_STFLE(%r4)
	tmhl	%r2,0x4000		# check for message-security-assist
	jz	.Lret

	lghi	%r0,S390X_QUERY		# query kimd capabilities
	la	%r1,S390X_KIMD(%r4)
	.long	0xb93e0002		# kimd %r0,%r2

	lghi	%r0,S390X_QUERY		# query klmd capabilities
	la	%r1,S390X_KLMD(%r4)
	.long	0xb93f0002		# klmd %r0,%r2

	lghi	%r0,S390X_QUERY		# query km capability vector
	la	%r1,S390X_KM(%r4)
	.long	0xb92e0042		# km %r4,%r2

	lghi	%r0,S390X_QUERY		# query kmc capability vector
	la	%r1,S390X_KMC(%r4)
	.long	0xb92f0042		# kmc %r4,%r2

	lghi	%r0,S390X_QUERY		# query kmac capability vector
	la	%r1,S390X_KMAC(%r4)
	.long	0xb91e0042		# kmac %r4,%r2

	tmhh	%r3,0x0004		# check for message-security-assist-4
	jz	.Lret

	lghi	%r0,S390X_QUERY		# query kmctr capability vector
	la	%r1,S390X_KMCTR(%r4)
	.long	0xb92d2042		# kmctr %r4,%r2,%r2

	lghi	%r0,S390X_QUERY		# query kmo capability vector
	la	%r1,S390X_KMO(%r4)
	.long	0xb92b0042		# kmo %r4,%r2

	lghi	%r0,S390X_QUERY		# query kmf capability vector
	la	%r1,S390X_KMF(%r4)
	.long	0xb92a0042		# kmf %r4,%r2

	tml	%r2,0x40		# check for message-security-assist-5
	jz	.Lret

	lghi	%r0,S390X_QUERY		# query prno capability vector
	la	%r1,S390X_PRNO(%r4)
	.long	0xb93c0042		# prno %r4,%r2

	lg	%r2,S390X_STFLE+16(%r4)
	tmhl	%r2,0x2000		# check for message-security-assist-8
	jz	.Lret

	lghi	%r0,S390X_QUERY		# query kma capability vector
	la	%r1,S390X_KMA(%r4)
	.long	0xb9294022		# kma %r2,%r4,%r2

.Lret:
	br	%r14
.size	OPENSSL_s390x_facilities,.-OPENSSL_s390x_facilities

.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.align	16
OPENSSL_rdtsc:
	larl	%r4,OPENSSL_s390xcap_P
	tm	S390X_STFLE+3(%r4),0x40	# check for store-clock-fast facility
	jz	.Lstck

	.long	0xb27cf010	# stckf 16(%r15)
	lg	%r2,16(%r15)
	br	%r14
.Lstck:
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
#if !defined(__s390x__) && !defined(__s390x)
	llgfr	%r3,%r3
#endif
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

.globl	CRYPTO_memcmp
.type	CRYPTO_memcmp,@function
.align	16
CRYPTO_memcmp:
#if !defined(__s390x__) && !defined(__s390x)
	llgfr	%r4,%r4
#endif
	lghi	%r5,0
	clgr	%r4,%r5
	je	.Lno_data

.Loop_cmp:
	llgc	%r0,0(%r2)
	la	%r2,1(%r2)
	llgc	%r1,0(%r3)
	la	%r3,1(%r3)
	xr	%r1,%r0
	or	%r5,%r1
	brctg	%r4,.Loop_cmp

	lnr	%r5,%r5
	srl	%r5,31
.Lno_data:
	lgr	%r2,%r5
	br	%r14
.size	CRYPTO_memcmp,.-CRYPTO_memcmp

.globl	OPENSSL_instrument_bus
.type	OPENSSL_instrument_bus,@function
.align	16
OPENSSL_instrument_bus:
	lghi	%r2,0
	br	%r14
.size	OPENSSL_instrument_bus,.-OPENSSL_instrument_bus

.globl	OPENSSL_instrument_bus2
.type	OPENSSL_instrument_bus2,@function
.align	16
OPENSSL_instrument_bus2:
	lghi	%r2,0
	br	%r14
.size	OPENSSL_instrument_bus2,.-OPENSSL_instrument_bus2

.globl	OPENSSL_vx_probe
.type	OPENSSL_vx_probe,@function
.align	16
OPENSSL_vx_probe:
	.word	0xe700,0x0000,0x0044	# vzero %v0
	br	%r14
.size	OPENSSL_vx_probe,.-OPENSSL_vx_probe
.globl	s390x_kimd
.type	s390x_kimd,@function
.align	16
s390x_kimd:
	llgfr	%r0,%r4
	lgr	%r1,%r5

	.long	0xb93e0002	# kimd %r0,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_kimd,.-s390x_kimd
.globl	s390x_klmd
.type	s390x_klmd,@function
.align	32
s390x_klmd:
	llgfr	%r0,%r6
	l	%r1,96(%r15)

	.long	0xb93f0042	# klmd %r4,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_klmd,.-s390x_klmd
.globl	s390x_km
.type	s390x_km,@function
.align	16
s390x_km:
	lr	%r0,%r5
	lr	%r1,%r6

	.long	0xb92e0042	# km %r4,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_km,.-s390x_km
.globl	s390x_kmac
.type	s390x_kmac,@function
.align	16
s390x_kmac:
	lr	%r0,%r4
	lr	%r1,%r5

	.long	0xb91e0002	# kmac %r0,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_kmac,.-s390x_kmac
.globl	s390x_kmo
.type	s390x_kmo,@function
.align	16
s390x_kmo:
	lr	%r0,%r5
	lr	%r1,%r6

	.long	0xb92b0042	# kmo %r4,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_kmo,.-s390x_kmo
.globl	s390x_kmf
.type	s390x_kmf,@function
.align	16
s390x_kmf:
	lr	%r0,%r5
	lr	%r1,%r6

	.long	0xb92a0042	# kmf %r4,%r2
	brc	1,.-4		# pay attention to "partial completion"

	br	%r14
.size	s390x_kmf,.-s390x_kmf
.globl	s390x_kma
.type	s390x_kma,@function
.align	16
s390x_kma:
	st	%r6,6*4(%r15)
	lm	%r0,%r1,96(%r15)

	.long	0xb9292064	# kma %r6,%r2,%r4
	brc	1,.-4		# pay attention to "partial completion"

	l	%r6,6*4(%r15)
	br	%r14
.size	s390x_kma,.-s390x_kma
.section	.init
	brasl	%r14,OPENSSL_cpuid_setup
