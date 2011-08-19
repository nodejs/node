// ====================================================================
// Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
// project.
//
// Rights for redistribution and usage in source and binary forms are
// granted according to the OpenSSL license. Warranty of any kind is
// disclaimed.
// ====================================================================

.ident  "rc4-ia64.S, Version 2.0"
.ident  "IA-64 ISA artwork by Andy Polyakov <appro@fy.chalmers.se>"

// What's wrong with compiler generated code? Because of the nature of
// C language, compiler doesn't [dare to] reorder load and stores. But
// being memory-bound, RC4 should benefit from reorder [on in-order-
// execution core such as IA-64]. But what can we reorder? At the very
// least we can safely reorder references to key schedule in respect
// to input and output streams. Secondly, from the first [close] glance
// it appeared that it's possible to pull up some references to
// elements of the key schedule itself. Original rationale ["prior
// loads are not safe only for "degenerated" key schedule, when some
// elements equal to the same value"] was kind of sloppy. I should have
// formulated as it really was: if we assume that pulling up reference
// to key[x+1] is not safe, then it would mean that key schedule would
// "degenerate," which is never the case. The problem is that this
// holds true in respect to references to key[x], but not to key[y].
// Legitimate "collisions" do occur within every 256^2 bytes window.
// Fortunately there're enough free instruction slots to keep prior
// reference to key[x+1], detect "collision" and compensate for it.
// All this without sacrificing a single clock cycle:-) Throughput is
// ~210MBps on 900MHz CPU, which is is >3x faster than gcc generated
// code and +30% - if compared to HP-UX C. Unrolling loop below should
// give >30% on top of that...

.text
.explicit

#if defined(_HPUX_SOURCE) && !defined(_LP64)
# define ADDP	addp4
#else
# define ADDP	add
#endif

#ifndef SZ
#define SZ	4	// this is set to sizeof(RC4_INT)
#endif
// SZ==4 seems to be optimal. At least SZ==8 is not any faster, not for
// assembler implementation, while SZ==1 code is ~30% slower.
#if SZ==1	// RC4_INT is unsigned char
# define	LDKEY	ld1
# define	STKEY	st1
# define	OFF	0
#elif SZ==4	// RC4_INT is unsigned int
# define	LDKEY	ld4
# define	STKEY	st4
# define	OFF	2
#elif SZ==8	// RC4_INT is unsigned long
# define	LDKEY	ld8
# define	STKEY	st8
# define	OFF	3
#endif

out=r8;		// [expanded] output pointer
inp=r9;		// [expanded] output pointer
prsave=r10;
key=r28;	// [expanded] pointer to RC4_KEY
ksch=r29;	// (key->data+255)[&~(sizeof(key->data)-1)]
xx=r30;
yy=r31;

// void RC4(RC4_KEY *key,size_t len,const void *inp,void *out);
.global	RC4#
.proc	RC4#
.align	32
.skip	16
RC4:
	.prologue
	.save   ar.pfs,r2
{ .mii;	alloc	r2=ar.pfs,4,12,0,16
	.save	pr,prsave
	mov	prsave=pr
	ADDP	key=0,in0		};;
{ .mib;	cmp.eq	p6,p0=0,in1			// len==0?
	.save	ar.lc,r3
	mov	r3=ar.lc
(p6)	br.ret.spnt.many	b0	};;	// emergency exit

	.body
	.rotr	dat[4],key_x[4],tx[2],rnd[2],key_y[2],ty[1];

{ .mib;	LDKEY	xx=[key],SZ			// load key->x
	add	in1=-1,in1			// adjust len for loop counter
	nop.b	0			}
{ .mib;	ADDP	inp=0,in2
	ADDP	out=0,in3
	brp.loop.imp	.Ltop,.Lexit-16	};;
{ .mmi;	LDKEY	yy=[key]			// load key->y
	add	ksch=SZ,key
	mov	ar.lc=in1		}
{ .mmi;	mov	key_y[1]=r0			// guarantee inequality
						// in first iteration
	add	xx=1,xx
	mov	pr.rot=1<<16		};;
{ .mii;	nop.m	0
	dep	key_x[1]=xx,r0,OFF,8
	mov	ar.ec=3			};;	// note that epilogue counter
						// is off by 1. I compensate
						// for this at exit...
.Ltop:
// The loop is scheduled for 4*(n+2) spin-rate on Itanium 2, which
// theoretically gives asymptotic performance of clock frequency
// divided by 4 bytes per seconds, or 400MBps on 1.6GHz CPU. This is
// for sizeof(RC4_INT)==4. For smaller RC4_INT STKEY inadvertently
// splits the last bundle and you end up with 5*n spin-rate:-(
// Originally the loop was scheduled for 3*n and relied on key
// schedule to be aligned at 256*sizeof(RC4_INT) boundary. But
// *(out++)=dat, which maps to st1, had same effect [inadvertent
// bundle split] and holded the loop back. Rescheduling for 4*n
// made it possible to eliminate dependence on specific alignment
// and allow OpenSSH keep "abusing" our API. Reaching for 3*n would
// require unrolling, sticking to variable shift instruction for
// collecting output [to avoid starvation for integer shifter] and
// copying of key schedule to controlled place in stack [so that
// deposit instruction can serve as substitute for whole
// key->data+((x&255)<<log2(sizeof(key->data[0])))]...
{ .mmi;	(p19)	st1	[out]=dat[3],1			// *(out++)=dat
	(p16)	add	xx=1,xx				// x++
	(p18)	dep	rnd[1]=rnd[1],r0,OFF,8	}	// ((tx+ty)&255)<<OFF
{ .mmi;	(p16)	add	key_x[1]=ksch,key_x[1]		// &key[xx&255]
	(p17)	add	key_y[1]=ksch,key_y[1]	};;	// &key[yy&255]	
{ .mmi;	(p16)	LDKEY	tx[0]=[key_x[1]]		// tx=key[xx]
	(p17)	LDKEY	ty[0]=[key_y[1]]		// ty=key[yy]	
	(p16)	dep	key_x[0]=xx,r0,OFF,8	}	// (xx&255)<<OFF
{ .mmi;	(p18)	add	rnd[1]=ksch,rnd[1]		// &key[(tx+ty)&255]
	(p16)	cmp.ne.unc p20,p21=key_x[1],key_y[1] };;
{ .mmi;	(p18)	LDKEY	rnd[1]=[rnd[1]]			// rnd=key[(tx+ty)&255]
	(p16)	ld1	dat[0]=[inp],1		}	// dat=*(inp++)
.pred.rel	"mutex",p20,p21
{ .mmi;	(p21)	add	yy=yy,tx[1]			// (p16)
	(p20)	add	yy=yy,tx[0]			// (p16) y+=tx
	(p21)	mov	tx[0]=tx[1]		};;	// (p16)
{ .mmi;	(p17)	STKEY	[key_y[1]]=tx[1]		// key[yy]=tx
	(p17)	STKEY	[key_x[2]]=ty[0]		// key[xx]=ty
	(p16)	dep	key_y[0]=yy,r0,OFF,8	}	// &key[yy&255]
{ .mmb;	(p17)	add	rnd[0]=tx[1],ty[0]		// tx+=ty
	(p18)	xor	dat[2]=dat[2],rnd[1]		// dat^=rnd
	br.ctop.sptk	.Ltop			};;
.Lexit:
{ .mib;	STKEY	[key]=yy,-SZ			// save key->y
	mov	pr=prsave,0x1ffff
	nop.b	0			}
{ .mib;	st1	[out]=dat[3],1			// compensate for truncated
						// epilogue counter
	add	xx=-1,xx
	nop.b	0			};;
{ .mib;	STKEY	[key]=xx			// save key->x
	mov	ar.lc=r3
	br.ret.sptk.many	b0	};;
.endp	RC4#
