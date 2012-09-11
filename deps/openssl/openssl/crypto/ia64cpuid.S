// Works on all IA-64 platforms: Linux, HP-UX, Win64i...
// On Win64i compile with ias.exe.
.text

.global	OPENSSL_cpuid_setup#
.proc	OPENSSL_cpuid_setup#
OPENSSL_cpuid_setup:
{ .mib;	br.ret.sptk.many	b0		};;
.endp	OPENSSL_cpuid_setup#

.global	OPENSSL_rdtsc#
.proc	OPENSSL_rdtsc#
OPENSSL_rdtsc:
{ .mib;	mov			r8=ar.itc
	br.ret.sptk.many	b0		};;
.endp   OPENSSL_rdtsc#

.global	OPENSSL_atomic_add#
.proc	OPENSSL_atomic_add#
.align	32
OPENSSL_atomic_add:
{ .mii;	ld4		r2=[r32]
	nop.i		0
	nop.i		0		};;
.Lspin:
{ .mii;	mov		ar.ccv=r2
	add		r8=r2,r33
	mov		r3=r2		};;
{ .mmi;	mf;;
	cmpxchg4.acq	r2=[r32],r8,ar.ccv
	nop.i		0		};;
{ .mib;	cmp.ne		p6,p0=r2,r3
	nop.i		0
(p6)	br.dpnt		.Lspin		};;
{ .mib;	nop.m		0
	sxt4		r8=r8
	br.ret.sptk.many	b0	};;
.endp	OPENSSL_atomic_add#

// Returns a structure comprising pointer to the top of stack of
// the caller and pointer beyond backing storage for the current
// register frame. The latter is required, because it might be
// insufficient to wipe backing storage for the current frame
// (as this procedure does), one might have to go further, toward
// higher addresses to reach for whole "retroactively" saved
// context...
.global	OPENSSL_wipe_cpu#
.proc	OPENSSL_wipe_cpu#
.align	32
OPENSSL_wipe_cpu:
	.prologue
	.fframe	0
	.save	ar.pfs,r2
	.save	ar.lc,r3
{ .mib;	alloc		r2=ar.pfs,0,96,0,96
	mov		r3=ar.lc
	brp.loop.imp	.L_wipe_top,.L_wipe_end-16
					};;
{ .mii;	mov		r9=ar.bsp
	mov		r8=pr
	mov		ar.lc=96	};;
	.body
{ .mii;	add		r9=96*8-8,r9
	mov		ar.ec=1		};;

// One can sweep double as fast, but then we can't quarantee
// that backing storage is wiped...
.L_wipe_top:
{ .mfi;	st8		[r9]=r0,-8
	mov		f127=f0
	mov		r127=r0		}
{ .mfb;	nop.m		0
	nop.f		0
	br.ctop.sptk	.L_wipe_top	};;
.L_wipe_end:

{ .mfi;	mov		r11=r0
	mov		f6=f0
	mov		r14=r0		}
{ .mfi;	mov		r15=r0
	mov		f7=f0
	mov		r16=r0		}
{ .mfi;	mov		r17=r0
	mov		f8=f0
	mov		r18=r0		}
{ .mfi;	mov		r19=r0
	mov		f9=f0
	mov		r20=r0		}
{ .mfi;	mov		r21=r0
	mov		f10=f0
	mov		r22=r0		}
{ .mfi;	mov		r23=r0
	mov		f11=f0
	mov		r24=r0		}
{ .mfi;	mov		r25=r0
	mov		f12=f0
	mov		r26=r0		}
{ .mfi;	mov		r27=r0
	mov		f13=f0
	mov		r28=r0		}
{ .mfi;	mov		r29=r0
	mov		f14=f0
	mov		r30=r0		}
{ .mfi;	mov		r31=r0
	mov		f15=f0
	nop.i		0		}
{ .mfi;	mov		f16=f0		}
{ .mfi;	mov		f17=f0		}
{ .mfi;	mov		f18=f0		}
{ .mfi;	mov		f19=f0		}
{ .mfi;	mov		f20=f0		}
{ .mfi;	mov		f21=f0		}
{ .mfi;	mov		f22=f0		}
{ .mfi;	mov		f23=f0		}
{ .mfi;	mov		f24=f0		}
{ .mfi;	mov		f25=f0		}
{ .mfi;	mov		f26=f0		}
{ .mfi;	mov		f27=f0		}
{ .mfi;	mov		f28=f0		}
{ .mfi;	mov		f29=f0		}
{ .mfi;	mov		f30=f0		}
{ .mfi;	add		r9=96*8+8,r9
	mov		f31=f0
	mov		pr=r8,0x1ffff	}
{ .mib;	mov		r8=sp
	mov		ar.lc=r3
	br.ret.sptk	b0		};;
.endp	OPENSSL_wipe_cpu#

.global	OPENSSL_cleanse#
.proc	OPENSSL_cleanse#
OPENSSL_cleanse:
{ .mib;	cmp.eq		p6,p0=0,r33	    // len==0
#if defined(_HPUX_SOURCE) && !defined(_LP64)
	addp4		r32=0,r32
#endif
(p6)	br.ret.spnt	b0		};;
{ .mib;	and		r2=7,r32
	cmp.leu		p6,p0=15,r33	    // len>=15
(p6)	br.cond.dptk	.Lot		};;

.Little:
{ .mib;	st1		[r32]=r0,1
	cmp.ltu		p6,p7=1,r33	}  // len>1
{ .mbb;	add		r33=-1,r33	   // len--
(p6)	br.cond.dptk	.Little
(p7)	br.ret.sptk.many	b0	};;

.Lot:
{ .mib;	cmp.eq		p6,p0=0,r2
(p6)	br.cond.dptk	.Laligned	};;
{ .mmi;	st1		[r32]=r0,1;;
	and		r2=7,r32	}
{ .mib;	add		r33=-1,r33
	br		.Lot		};;

.Laligned:
{ .mmi;	st8		[r32]=r0,8
	and		r2=-8,r33	    // len&~7
	add		r33=-8,r33	};; // len-=8
{ .mib;	cmp.ltu		p6,p0=8,r2	    // ((len+8)&~7)>8
(p6)	br.cond.dptk	.Laligned	};;

{ .mbb;	cmp.eq		p6,p7=r0,r33
(p7)	br.cond.dpnt	.Little
(p6)	br.ret.sptk.many	b0	};;
.endp	OPENSSL_cleanse#
