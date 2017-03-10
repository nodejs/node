.text
// Copyright 2009-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the OpenSSL license (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

.globl	OPENSSL_s390x_facilities
.type	OPENSSL_s390x_facilities,@function
.align	16
OPENSSL_s390x_facilities:
	lghi	%r0,0
	larl	%r4,OPENSSL_s390xcap_P
	stg	%r0,8(%r4)	# wipe capability vectors
	stg	%r0,16(%r4)
	stg	%r0,24(%r4)
	stg	%r0,32(%r4)
	stg	%r0,40(%r4)
	stg	%r0,48(%r4)
	stg	%r0,56(%r4)
	stg	%r0,64(%r4)
	stg	%r0,72(%r4)

	.long	0xb2b04000	# stfle	0(%r4)
	brc	8,.Ldone
	lghi	%r0,1
	.long	0xb2b04000	# stfle 0(%r4)
.Ldone:
	lmg	%r2,%r3,0(%r4)
	tmhl	%r2,0x4000	# check for message-security-assist
	jz	.Lret

	lghi	%r0,0		# query kimd capabilities
	la	%r1,16(%r4)
	.long	0xb93e0002	# kimd %r0,%r2

	lghi	%r0,0		# query km capability vector
	la	%r1,32(%r4)
	.long	0xb92e0042	# km %r4,%r2

	lghi	%r0,0		# query kmc capability vector
	la	%r1,48(%r4)
	.long	0xb92f0042	# kmc %r4,%r2

	tmhh	%r3,0x0004	# check for message-security-assist-4
	jz	.Lret

	lghi	%r0,0		# query kmctr capability vector
	la	%r1,64(%r4)
	.long	0xb92d2042	# kmctr %r4,%r2,%r2

.Lret:
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

.section	.init
	brasl	%r14,OPENSSL_cpuid_setup

.comm	OPENSSL_s390xcap_P,80,8
