#if defined(__SUNPRO_C) && defined(__sparcv9)
# define ABI64  /* They've said -xarch=v9 at command line */
#elif defined(__GNUC__) && defined(__arch64__)
# define ABI64  /* They've said -m64 at command line */
#endif

#ifdef ABI64
  .register	%g2,#scratch
  .register	%g3,#scratch
# define	FRAME	-192
# define	BIAS	2047
#else
# define	FRAME	-96
# define	BIAS	0
#endif

.text
.align	32
.global	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,#function
! Keep in mind that this does not excuse us from wiping the stack!
! This routine wipes registers, but not the backing store [which
! resides on the stack, toward lower addresses]. To facilitate for
! stack wiping I return pointer to the top of stack of the *caller*.
OPENSSL_wipe_cpu:
	save	%sp,FRAME,%sp
	nop
#ifdef __sun
#include <sys/trap.h>
	ta	ST_CLEAN_WINDOWS
#else
	call	.walk.reg.wins
#endif
	nop
	call	.PIC.zero.up
	mov	.zero-(.-4),%o0
	ldd	[%o0],%f0

	subcc	%g0,1,%o0
	! Following is V9 "rd %ccr,%o0" instruction. However! V8
	! specification says that it ("rd %asr2,%o0" in V8 terms) does
	! not cause illegal_instruction trap. It therefore can be used
	! to determine if the CPU the code is executing on is V8- or
	! V9-compliant, as V9 returns a distinct value of 0x99,
	! "negative" and "borrow" bits set in both %icc and %xcc.
	.word	0x91408000	!rd	%ccr,%o0
	cmp	%o0,0x99
	bne	.v8
	nop
			! Even though we do not use %fp register bank,
			! we wipe it as memcpy might have used it...
			.word	0xbfa00040	!fmovd	%f0,%f62
			.word	0xbba00040	!...
			.word	0xb7a00040
			.word	0xb3a00040
			.word	0xafa00040
			.word	0xaba00040
			.word	0xa7a00040
			.word	0xa3a00040
			.word	0x9fa00040
			.word	0x9ba00040
			.word	0x97a00040
			.word	0x93a00040
			.word	0x8fa00040
			.word	0x8ba00040
			.word	0x87a00040
			.word	0x83a00040	!fmovd	%f0,%f32
.v8:			fmovs	%f1,%f31
	clr	%o0
			fmovs	%f0,%f30
	clr	%o1
			fmovs	%f1,%f29
	clr	%o2
			fmovs	%f0,%f28
	clr	%o3
			fmovs	%f1,%f27
	clr	%o4
			fmovs	%f0,%f26
	clr	%o5
			fmovs	%f1,%f25
	clr	%o7
			fmovs	%f0,%f24
	clr	%l0
			fmovs	%f1,%f23
	clr	%l1
			fmovs	%f0,%f22
	clr	%l2
			fmovs	%f1,%f21
	clr	%l3
			fmovs	%f0,%f20
	clr	%l4
			fmovs	%f1,%f19
	clr	%l5
			fmovs	%f0,%f18
	clr	%l6
			fmovs	%f1,%f17
	clr	%l7
			fmovs	%f0,%f16
	clr	%i0
			fmovs	%f1,%f15
	clr	%i1
			fmovs	%f0,%f14
	clr	%i2
			fmovs	%f1,%f13
	clr	%i3
			fmovs	%f0,%f12
	clr	%i4
			fmovs	%f1,%f11
	clr	%i5
			fmovs	%f0,%f10
	clr	%g1
			fmovs	%f1,%f9
	clr	%g2
			fmovs	%f0,%f8
	clr	%g3
			fmovs	%f1,%f7
	clr	%g4
			fmovs	%f0,%f6
	clr	%g5
			fmovs	%f1,%f5
			fmovs	%f0,%f4
			fmovs	%f1,%f3
			fmovs	%f0,%f2

	add	%fp,BIAS,%i0	! return pointer to caller´s top of stack

	ret
	restore

.zero:	.long	0x0,0x0
.PIC.zero.up:
	retl
	add	%o0,%o7,%o0
#ifdef DEBUG
.global	walk_reg_wins
.type	walk_reg_wins,#function
walk_reg_wins:
#endif
.walk.reg.wins:
	save	%sp,FRAME,%sp
	cmp	%i7,%o7
	be	2f
	clr	%o0
	cmp	%o7,0	! compiler never cleans %o7...
	be	1f	! could have been a leaf function...
	clr	%o1
	call	.walk.reg.wins
	nop
1:	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o7
	clr	%l0
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	add	%o0,1,%i0	! used for debugging
2:	ret
	restore
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu

.global	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,#function
OPENSSL_atomic_add:
#ifndef ABI64
	subcc	%g0,1,%o2
	.word	0x95408000	!rd	%ccr,%o2, see comment above
	cmp	%o2,0x99
	be	.v9
	nop
	save	%sp,FRAME,%sp
	ba	.enter
	nop
#ifdef __sun
! Note that you don't have to link with libthread to call thr_yield,
! as libc provides a stub, which is overloaded the moment you link
! with *either* libpthread or libthread...
#define	YIELD_CPU	thr_yield
#else
! applies at least to Linux and FreeBSD... Feedback expected...
#define	YIELD_CPU	sched_yield
#endif
.spin:	call	YIELD_CPU
	nop
.enter:	ld	[%i0],%i2
	cmp	%i2,-4096
	be	.spin
	mov	-1,%i2
	swap	[%i0],%i2
	cmp	%i2,-1
	be	.spin
	add	%i2,%i1,%i2
	stbar
	st	%i2,[%i0]
	sra	%i2,%g0,%i0
	ret
	restore
.v9:
#endif
	ld	[%o0],%o2
1:	add	%o1,%o2,%o3
	.word	0xd7e2100a	!cas [%o0],%o2,%o3, compare [%o0] with %o2 and swap %o3
	cmp	%o2,%o3
	bne	1b
	mov	%o3,%o2		! cas is always fetching to dest. register
	add	%o1,%o2,%o0	! OpenSSL expects the new value
	retl
	sra	%o0,%g0,%o0	! we return signed int, remember?
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.global	OPENSSL_rdtsc
	subcc	%g0,1,%o0
	.word	0x91408000	!rd	%ccr,%o0
	cmp	%o0,0x99
	bne	.notsc
	xor	%o0,%o0,%o0
	save	%sp,FRAME-16,%sp
	mov	513,%o0		!SI_PLATFORM
	add	%sp,BIAS+16,%o1
	call	sysinfo
	mov	256,%o2

	add	%sp,BIAS-16,%o1
	ld	[%o1],%l0
	ld	[%o1+4],%l1
	ld	[%o1+8],%l2
	mov	%lo('SUNW'),%l3
	ret
	restore
.notsc:
	retl
	nop
.type	OPENSSL_rdtsc,#function
.size	OPENSSL_rdtsc,.-OPENSSL_atomic_add
