/* Copyright (c) 2005 Hewlett-Packard Development Company, L.P.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

//	Common registers are assigned as follows:
//
//	COMMON
//
//	t0		Const Tbl Ptr	TPtr
//	t1		Round Constant	TRound
//	t4		Block residual	LenResid
//	t5		Residual Data	DTmp
//
//	{in,out}0	Block 0 Cycle	RotateM0
//	{in,out}1	Block Value 12	M12
//	{in,out}2	Block Value 8	M8
//	{in,out}3	Block Value 4	M4
//	{in,out}4	Block Value 0	M0
//	{in,out}5	Block 1 Cycle	RotateM1
//	{in,out}6	Block Value 13	M13
//	{in,out}7	Block Value 9	M9
//	{in,out}8	Block Value 5	M5
//	{in,out}9	Block Value 1	M1
//	{in,out}10	Block 2 Cycle	RotateM2
//	{in,out}11	Block Value 14	M14
//	{in,out}12	Block Value 10	M10
//	{in,out}13	Block Value 6	M6
//	{in,out}14	Block Value 2	M2
//	{in,out}15	Block 3 Cycle	RotateM3
//	{in,out}16	Block Value 15	M15
//	{in,out}17	Block Value 11	M11
//	{in,out}18	Block Value 7	M7
//	{in,out}19	Block Value 3	M3
//	{in,out}20	Scratch			Z
//	{in,out}21	Scratch			Y
//	{in,out}22	Scratch			X
//	{in,out}23	Scratch			W
//	{in,out}24	Digest A		A
//	{in,out}25	Digest B		B
//	{in,out}26	Digest C		C
//	{in,out}27	Digest D		D
//	{in,out}28	Active Data Ptr	DPtr
//	in28		Dummy Value		-
//	out28		Dummy Value		-
//	bt0			Coroutine Link	QUICK_RTN
//
///	These predicates are used for computing the padding block(s) and
///	are shared between the driver and digest co-routines
//
//	pt0			Extra Pad Block	pExtra
//	pt1			Load next word	pLoad
//	pt2			Skip next word	pSkip
//	pt3			Search for Pad	pNoPad
//	pt4			Pad Word 0		pPad0
//	pt5			Pad Word 1		pPad1
//	pt6			Pad Word 2		pPad2
//	pt7			Pad Word 3		pPad3

#define	DTmp		r19
#define	LenResid	r18
#define	QUICK_RTN	b6
#define	TPtr		r14
#define	TRound		r15
#define	pExtra		p6
#define	pLoad		p7
#define	pNoPad		p9
#define	pPad0		p10
#define	pPad1		p11
#define	pPad2		p12
#define	pPad3		p13
#define	pSkip		p8

#define	A_		out24
#define	B_		out25
#define	C_		out26
#define	D_		out27
#define	DPtr_		out28
#define	M0_		out4
#define	M1_		out9
#define	M10_		out12
#define	M11_		out17
#define	M12_		out1
#define	M13_		out6
#define	M14_		out11
#define	M15_		out16
#define	M2_		out14
#define	M3_		out19
#define	M4_		out3
#define	M5_		out8
#define	M6_		out13
#define	M7_		out18
#define	M8_		out2
#define	M9_		out7
#define	RotateM0_	out0
#define	RotateM1_	out5
#define	RotateM2_	out10
#define	RotateM3_	out15
#define	W_		out23
#define	X_		out22
#define	Y_		out21
#define	Z_		out20

#define	A		in24
#define	B		in25
#define	C		in26
#define	D		in27
#define	DPtr		in28
#define	M0		in4
#define	M1		in9
#define	M10		in12
#define	M11		in17
#define	M12		in1
#define	M13		in6
#define	M14		in11
#define	M15		in16
#define	M2		in14
#define	M3		in19
#define	M4		in3
#define	M5		in8
#define	M6		in13
#define	M7		in18
#define	M8		in2
#define	M9		in7
#define	RotateM0	in0
#define	RotateM1	in5
#define	RotateM2	in10
#define	RotateM3	in15
#define	W		in23
#define	X		in22
#define	Y		in21
#define	Z		in20

/* register stack configuration for md5_block_asm_data_order(): */
#define	MD5_NINP	3
#define	MD5_NLOC	0
#define MD5_NOUT	29
#define MD5_NROT	0

/* register stack configuration for helpers: */
#define	_NINPUTS	MD5_NOUT
#define	_NLOCALS	0
#define _NOUTPUT	0
#define	_NROTATE	24	/* this must be <= _NINPUTS */

#if defined(_HPUX_SOURCE) && !defined(_LP64)
#define	ADDP	addp4
#else
#define	ADDP	add
#endif

#if defined(_HPUX_SOURCE) || defined(B_ENDIAN)
#define HOST_IS_BIG_ENDIAN
#endif

//	Macros for getting the left and right portions of little-endian words

#define	GETLW(dst, src, align)	dep.z dst = src, 32 - 8 * align, 8 * align
#define	GETRW(dst, src, align)	extr.u dst = src, 8 * align, 32 - 8 * align

//	MD5 driver
//
//		Reads an input block, then calls the digest block
//		subroutine and adds the results to the accumulated
//		digest.  It allocates 32 outs which the subroutine
//		uses as it's inputs and rotating
//		registers. Initializes the round constant pointer and
//		takes care of saving/restoring ar.lc
//
///	INPUT
//
//	in0		Context Ptr		CtxPtr0
//	in1		Input Data Ptr		DPtrIn
//	in2		Integral Blocks		BlockCount
//	rp		Return Address		-
//
///	CODE
//
//	v2		Input Align		InAlign
//	t0		Shared w/digest		-
//	t1		Shared w/digest		-
//	t2		Shared w/digest		-
//	t3		Shared w/digest		-
//	t4		Shared w/digest		-
//	t5		Shared w/digest		-
//	t6		PFS Save		PFSSave
//	t7		ar.lc Save		LCSave
//	t8		Saved PR		PRSave
//	t9		2nd CtxPtr		CtxPtr1
//	t10		Table Base		CTable
//	t11		Table[0]		CTable0
//	t13		Accumulator A		AccumA
//	t14		Accumulator B		AccumB
//	t15		Accumulator C		AccumC
//	t16		Accumulator D		AccumD
//	pt0		Shared w/digest		-
//	pt1		Shared w/digest		-
//	pt2		Shared w/digest		-
//	pt3		Shared w/digest		-
//	pt4		Shared w/digest		-
//	pt5		Shared w/digest		-
//	pt6		Shared w/digest		-
//	pt7		Shared w/digest		-
//	pt8		Not Aligned		pOff
//	pt8		Blocks Left		pAgain

#define	AccumA		r27
#define	AccumB		r28
#define	AccumC		r29
#define	AccumD		r30
#define	CTable		r24
#define	CTable0		r25
#define	CtxPtr0		in0
#define	CtxPtr1		r23
#define	DPtrIn		in1
#define	BlockCount	in2
#define	InAlign		r10
#define	LCSave		r21
#define	PFSSave		r20
#define	PRSave		r22
#define	pAgain		p63
#define	pOff		p63

	.text

/* md5_block_asm_data_order(MD5_CTX *c, const void *data, size_t num)

     where:
      c: a pointer to a structure of this type:

	   typedef struct MD5state_st
	     {
	       MD5_LONG A,B,C,D;
	       MD5_LONG Nl,Nh;
	       MD5_LONG data[MD5_LBLOCK];
	       unsigned int num;
	     }
	   MD5_CTX;

      data: a pointer to the input data (may be misaligned)
      num:  the number of 16-byte blocks to hash (i.e., the length
            of DATA is 16*NUM.

   */

	.type	md5_block_asm_data_order, @function
	.global	md5_block_asm_data_order
	.align	32
	.proc	md5_block_asm_data_order
md5_block_asm_data_order:
.md5_block:
	.prologue
{	.mmi
	.save	ar.pfs, PFSSave
	alloc	PFSSave = ar.pfs, MD5_NINP, MD5_NLOC, MD5_NOUT, MD5_NROT
	ADDP	CtxPtr1 = 8, CtxPtr0
	mov	CTable = ip
}
{	.mmi
	ADDP	DPtrIn = 0, DPtrIn
	ADDP	CtxPtr0 = 0, CtxPtr0
	.save	ar.lc, LCSave
	mov	LCSave = ar.lc
}
;;
{	.mmi
	add	CTable = .md5_tbl_data_order#-.md5_block#, CTable
	and	InAlign = 0x3, DPtrIn
}

{	.mmi
	ld4	AccumA = [CtxPtr0], 4
	ld4	AccumC = [CtxPtr1], 4
	.save pr, PRSave
	mov	PRSave = pr
	.body
}
;;
{	.mmi
	ld4	AccumB = [CtxPtr0]
	ld4	AccumD = [CtxPtr1]
	dep	DPtr_ = 0, DPtrIn, 0, 2
} ;;
#ifdef HOST_IS_BIG_ENDIAN
	rum	psr.be;;	// switch to little-endian
#endif
{	.mmb
	ld4	CTable0 = [CTable], 4
	cmp.ne	pOff, p0 = 0, InAlign
(pOff)	br.cond.spnt.many .md5_unaligned
} ;;

//	The FF load/compute loop rotates values three times, so that
//	loading into M12 here produces the M0 value, M13 -> M1, etc.

.md5_block_loop0:
{	.mmi
	ld4	M12_ = [DPtr_], 4
	mov	TPtr = CTable
	mov	TRound = CTable0
} ;;
{	.mmi
	ld4	M13_ = [DPtr_], 4
	mov	A_ = AccumA
	mov	B_ = AccumB
} ;;
{	.mmi
	ld4	M14_ = [DPtr_], 4
	mov	C_ = AccumC
	mov	D_ = AccumD
} ;;
{	.mmb
	ld4	M15_ = [DPtr_], 4
	add	BlockCount = -1, BlockCount
	br.call.sptk.many QUICK_RTN = md5_digest_block0
} ;;

//	Now, we add the new digest values and do some clean-up
//	before checking if there's another full block to process

{	.mmi
	add	AccumA = AccumA, A_
	add	AccumB = AccumB, B_
	cmp.ne	pAgain, p0 = 0, BlockCount
}
{	.mib
	add	AccumC = AccumC, C_
	add	AccumD = AccumD, D_
(pAgain) br.cond.dptk.many .md5_block_loop0
} ;;

.md5_exit:
#ifdef HOST_IS_BIG_ENDIAN
	sum	psr.be;;	// switch back to big-endian mode
#endif
{	.mmi
	st4	[CtxPtr0] = AccumB, -4
	st4	[CtxPtr1] = AccumD, -4
	mov	pr = PRSave, 0x1ffff ;;
}
{	.mmi
	st4	[CtxPtr0] = AccumA
	st4	[CtxPtr1] = AccumC
	mov	ar.lc = LCSave
} ;;
{	.mib
	mov	ar.pfs = PFSSave
	br.ret.sptk.few	rp
} ;;

#define	MD5UNALIGNED(offset)						\
.md5_process##offset:							\
{	.mib ;								\
	nop	0x0	;						\
	GETRW(DTmp, DTmp, offset) ;					\
} ;;									\
.md5_block_loop##offset:						\
{	.mmi ;								\
	ld4	Y_ = [DPtr_], 4 ;					\
	mov	TPtr = CTable ;						\
	mov	TRound = CTable0 ;					\
} ;;									\
{	.mmi ;								\
	ld4	M13_ = [DPtr_], 4 ;					\
	mov	A_ = AccumA ;						\
	mov	B_ = AccumB ;						\
} ;;									\
{	.mii ;								\
	ld4	M14_ = [DPtr_], 4 ;					\
	GETLW(W_, Y_, offset) ;						\
	mov	C_ = AccumC ;						\
}									\
{	.mmi ;								\
	mov	D_ = AccumD ;;						\
	or	M12_ = W_, DTmp ;					\
	GETRW(DTmp, Y_, offset) ;					\
}									\
{	.mib ;								\
	ld4	M15_ = [DPtr_], 4 ;					\
	add	BlockCount = -1, BlockCount ;				\
	br.call.sptk.many QUICK_RTN = md5_digest_block##offset;		\
} ;;									\
{	.mmi ;								\
	add	AccumA = AccumA, A_ ;					\
	add	AccumB = AccumB, B_ ;					\
	cmp.ne	pAgain, p0 = 0, BlockCount ;				\
}									\
{	.mib ;								\
	add	AccumC = AccumC, C_ ;					\
	add	AccumD = AccumD, D_ ;					\
(pAgain) br.cond.dptk.many .md5_block_loop##offset ;			\
} ;;									\
{	.mib ;								\
	nop	0x0 ;							\
	nop	0x0 ;							\
	br.cond.sptk.many .md5_exit ;					\
} ;;

	.align	32
.md5_unaligned:
//
//	Because variable shifts are expensive, we special case each of
//	the four alignements. In practice, this won't hurt too much
//	since only one working set of code will be loaded.
//
{	.mib
	ld4	DTmp = [DPtr_], 4
	cmp.eq	pOff, p0 = 1, InAlign
(pOff)	br.cond.dpnt.many .md5_process1
} ;;
{	.mib
	cmp.eq	pOff, p0 = 2, InAlign
	nop	0x0
(pOff)	br.cond.dpnt.many .md5_process2
} ;;
	MD5UNALIGNED(3)
	MD5UNALIGNED(1)
	MD5UNALIGNED(2)

	.endp md5_block_asm_data_order


// MD5 Perform the F function and load
//
// Passed the first 4 words (M0 - M3) and initial (A, B, C, D) values,
// computes the FF() round of functions, then branches to the common
// digest code to finish up with GG(), HH, and II().
//
// INPUT
//
// rp Return Address -
//
// CODE
//
// v0 PFS bit bucket PFS
// v1 Loop Trip Count LTrip
// pt0 Load next word pMore

/* For F round: */
#define LTrip	r9
#define PFS	r8
#define pMore	p6

/* For GHI rounds: */
#define T	r9
#define U	r10
#define V	r11

#define COMPUTE(a, b, s, M, R)			\
{						\
	.mii ;					\
	ld4 TRound = [TPtr], 4 ;		\
	dep.z Y = Z, 32, 32 ;;			\
	shrp Z = Z, Y, 64 - s ;			\
} ;;						\
{						\
	.mmi ;					\
	add a = Z, b ;				\
	mov R = M ;				\
	nop 0x0 ;				\
} ;;

#define LOOP(a, b, s, M, R, label)		\
{	.mii ;					\
	ld4 TRound = [TPtr], 4 ;		\
	dep.z Y = Z, 32, 32 ;;			\
	shrp Z = Z, Y, 64 - s ;			\
} ;;						\
{	.mib ;					\
	add a = Z, b ;				\
	mov R = M ;				\
	br.ctop.sptk.many label ;		\
} ;;

// G(B, C, D) = (B & D) | (C & ~D)

#define G(a, b, c, d, M)			\
{	.mmi ;					\
	add Z = M, TRound ;			\
	and Y = b, d ;				\
	andcm X = c, d ;			\
} ;;						\
{	.mii ;					\
	add Z = Z, a ;				\
	or Y = Y, X ;;				\
	add Z = Z, Y ;				\
} ;;

// H(B, C, D) = B ^ C ^ D

#define H(a, b, c, d, M)			\
{	.mmi ;					\
	add Z = M, TRound ;			\
	xor Y = b, c ;				\
	nop 0x0 ;				\
} ;;						\
{	.mii ;					\
	add Z = Z, a ;				\
	xor Y = Y, d ;;				\
	add Z = Z, Y ;				\
} ;;

// I(B, C, D) = C ^ (B | ~D)
//
// However, since we have an andcm operator, we use the fact that
//
// Y ^ Z == ~Y ^ ~Z
//
// to rewrite the expression as
//
// I(B, C, D) = ~C ^ (~B & D)

#define I(a, b, c, d, M)			\
{	.mmi ;					\
	add Z = M, TRound ;			\
	andcm Y = d, b ;			\
	andcm X = -1, c ;			\
} ;;						\
{	.mii ;					\
	add Z = Z, a ;				\
	xor Y = Y, X ;;				\
	add Z = Z, Y ;				\
} ;;

#define GG4(label)				\
	G(A, B, C, D, M0)			\
	COMPUTE(A, B, 5, M0, RotateM0)		\
	G(D, A, B, C, M1)			\
	COMPUTE(D, A, 9, M1, RotateM1)		\
	G(C, D, A, B, M2)			\
	COMPUTE(C, D, 14, M2, RotateM2)		\
	G(B, C, D, A, M3)			\
	LOOP(B, C, 20, M3, RotateM3, label)

#define HH4(label)				\
	H(A, B, C, D, M0)			\
	COMPUTE(A, B, 4, M0, RotateM0)		\
	H(D, A, B, C, M1)			\
	COMPUTE(D, A, 11, M1, RotateM1)		\
	H(C, D, A, B, M2)			\
	COMPUTE(C, D, 16, M2, RotateM2)		\
	H(B, C, D, A, M3)			\
	LOOP(B, C, 23, M3, RotateM3, label)

#define II4(label)				\
	I(A, B, C, D, M0)			\
	COMPUTE(A, B, 6, M0, RotateM0)		\
	I(D, A, B, C, M1)			\
	COMPUTE(D, A, 10, M1, RotateM1)		\
	I(C, D, A, B, M2)			\
	COMPUTE(C, D, 15, M2, RotateM2)		\
	I(B, C, D, A, M3)			\
	LOOP(B, C, 21, M3, RotateM3, label)

#define FFLOAD(a, b, c, d, M, N, s)		\
{	.mii ;					\
(pMore) ld4 N = [DPtr], 4 ;			\
	add Z = M, TRound ;			\
	and Y = c, b ;				\
}						\
{	.mmi ;					\
	andcm X = d, b ;;			\
	add Z = Z, a ;				\
	or Y = Y, X ;				\
} ;;						\
{	.mii ;					\
	ld4 TRound = [TPtr], 4 ;		\
	add Z = Z, Y ;;				\
	dep.z Y = Z, 32, 32 ;			\
} ;;						\
{	.mii ;					\
	nop 0x0 ;				\
	shrp Z = Z, Y, 64 - s ;;		\
	add a = Z, b ;				\
} ;;

#define FFLOOP(a, b, c, d, M, N, s, dest)	\
{	.mii ;					\
(pMore)	ld4 N = [DPtr], 4 ;			\
	add Z = M, TRound ;			\
	and Y = c, b ;				\
}						\
{	.mmi ;					\
	andcm X = d, b ;;			\
	add Z = Z, a ;				\
	or Y = Y, X ;				\
} ;;						\
{	.mii ;					\
	ld4 TRound = [TPtr], 4 ;		\
	add Z = Z, Y ;;				\
	dep.z Y = Z, 32, 32 ;			\
} ;;						\
{	.mii ;					\
	nop 0x0 ;				\
	shrp Z = Z, Y, 64 - s ;;		\
	add a = Z, b ;				\
}						\
{	.mib ;					\
	cmp.ne pMore, p0 = 0, LTrip ;		\
	add LTrip = -1, LTrip ;			\
	br.ctop.dptk.many dest ;		\
} ;;

	.type md5_digest_block0, @function
	.align 32

	.proc md5_digest_block0
	.prologue
md5_digest_block0:
	.altrp QUICK_RTN
	.body
{	.mmi
	alloc PFS = ar.pfs, _NINPUTS, _NLOCALS, _NOUTPUT, _NROTATE
	mov LTrip = 2
	mov ar.lc = 3
} ;;
{	.mii
	cmp.eq pMore, p0 = r0, r0
	mov ar.ec = 0
	nop 0x0
} ;;

.md5_FF_round0:
	FFLOAD(A, B, C, D, M12, RotateM0, 7)
	FFLOAD(D, A, B, C, M13, RotateM1, 12)
	FFLOAD(C, D, A, B, M14, RotateM2, 17)
	FFLOOP(B, C, D, A, M15, RotateM3, 22, .md5_FF_round0)
	//
	// !!! Fall through to md5_digest_GHI
	//
	.endp md5_digest_block0

	.type md5_digest_GHI, @function
	.align 32

	.proc md5_digest_GHI
	.prologue
	.regstk _NINPUTS, _NLOCALS, _NOUTPUT, _NROTATE
md5_digest_GHI:
	.altrp QUICK_RTN
	.body
//
// The following sequence shuffles the block counstants round for the
// next round:
//
// 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
// 1 6 11 0 5 10 14 4 9 14 3 8 13 2 7 12
//
{	.mmi
	mov Z = M0
	mov Y = M15
	mov ar.lc = 3
}
{	.mmi
	mov X = M2
	mov W = M9
	mov V = M4
} ;;

{	.mmi
	mov M0 = M1
	mov M15 = M12
	mov ar.ec = 1
}
{	.mmi
	mov M2 = M11
	mov M9 = M14
	mov M4 = M5
} ;;

{	.mmi
	mov M1 = M6
	mov M12 = M13
	mov U = M3
}
{	.mmi
	mov M11 = M8
	mov M14 = M7
	mov M5 = M10
} ;;

{	.mmi
	mov M6 = Y
	mov M13 = X
	mov M3 = Z
}
{	.mmi
	mov M8 = W
	mov M7 = V
	mov M10 = U
} ;;

.md5_GG_round:
	GG4(.md5_GG_round)

// The following sequence shuffles the block constants round for the
// next round:
//
// 1 6 11 0 5 10 14 4 9 14 3 8 13 2 7 12
// 5 8 11 14 1 4 7 10 13 0 3 6 9 12 15 2

{	.mmi
	mov Z = M0
	mov Y = M1
	mov ar.lc = 3
}
{	.mmi
	mov X = M3
	mov W = M5
	mov V = M6
} ;;

{	.mmi
	mov M0 = M4
	mov M1 = M11
	mov ar.ec = 1
}
{	.mmi
	mov M3 = M9
	mov U = M8
	mov T = M13
} ;;

{	.mmi
	mov M4 = Z
	mov M11 = Y
	mov M5 = M7
}
{	.mmi
	mov M6 = M14
	mov M8 = M12
	mov M13 = M15
} ;;

{	.mmi
	mov M7 = W
	mov M14 = V
	nop 0x0
}
{	.mmi
	mov M9 = X
	mov M12 = U
	mov M15 = T
} ;;

.md5_HH_round:
	HH4(.md5_HH_round)

// The following sequence shuffles the block constants round for the
// next round:
//
// 5 8 11 14 1 4 7 10 13 0 3 6 9 12 15 2
// 0 7 14 5 12 3 10 1 8 15 6 13 4 11 2 9

{	.mmi
	mov Z = M0
	mov Y = M15
	mov ar.lc = 3
}
{	.mmi
	mov X = M10
	mov W = M1
	mov V = M4
} ;;

{	.mmi
	mov M0 = M9
	mov M15 = M12
	mov ar.ec = 1
}
{	.mmi
	mov M10 = M11
	mov M1 = M6
	mov M4 = M13
} ;;

{	.mmi
	mov M9 = M14
	mov M12 = M5
	mov U = M3
}
{	.mmi
	mov M11 = M8
	mov M6 = M7
	mov M13 = M2
} ;;

{	.mmi
	mov M14 = Y
	mov M5 = X
	mov M3 = Z
}
{	.mmi
	mov M8 = W
	mov M7 = V
	mov M2 = U
} ;;

.md5_II_round:
	II4(.md5_II_round)

{	.mib
	nop 0x0
	nop 0x0
	br.ret.sptk.many QUICK_RTN
} ;;

	.endp md5_digest_GHI

#define FFLOADU(a, b, c, d, M, P, N, s, offset)	\
{	.mii ;					\
(pMore) ld4 N = [DPtr], 4 ;			\
	add Z = M, TRound ;			\
	and Y = c, b ;				\
}						\
{	.mmi ;					\
	andcm X = d, b ;;			\
	add Z = Z, a ;				\
	or Y = Y, X ;				\
} ;;						\
{	.mii ;					\
	ld4 TRound = [TPtr], 4 ;		\
	GETLW(W, P, offset) ;			\
	add Z = Z, Y ;				\
} ;;						\
{	.mii ;					\
	or W = W, DTmp ;			\
	dep.z Y = Z, 32, 32 ;;			\
	shrp Z = Z, Y, 64 - s ;			\
} ;;						\
{	.mii ;					\
	add a = Z, b ;				\
	GETRW(DTmp, P, offset) ;		\
	mov P = W ;				\
} ;;

#define FFLOOPU(a, b, c, d, M, P, N, s, offset)		\
{	.mii ;						\
(pMore) ld4 N = [DPtr], 4 ;				\
	add Z = M, TRound ;				\
	and Y = c, b ;					\
}							\
{	.mmi ;						\
	andcm X = d, b ;;				\
	add Z = Z, a ;					\
	or Y = Y, X ;					\
} ;;							\
{	.mii ;						\
	ld4 TRound = [TPtr], 4 ;			\
(pMore) GETLW(W, P, offset) 	;			\
	add Z = Z, Y ;					\
} ;;							\
{	.mii ;						\
(pMore) or W = W, DTmp ;				\
	dep.z Y = Z, 32, 32 ;;				\
	shrp Z = Z, Y, 64 - s ;				\
} ;;							\
{	.mii ;						\
	add a = Z, b ;					\
(pMore) GETRW(DTmp, P, offset) 	;			\
(pMore) mov P = W ;					\
}							\
{	.mib ;						\
	cmp.ne pMore, p0 = 0, LTrip ;			\
	add LTrip = -1, LTrip ;				\
	br.ctop.sptk.many .md5_FF_round##offset ;	\
} ;;

#define MD5FBLOCK(offset)						\
	.type md5_digest_block##offset, @function ;			\
									\
	.align 32 ;							\
	.proc md5_digest_block##offset ;				\
	.prologue ;							\
	.altrp QUICK_RTN ;						\
	.body ;								\
md5_digest_block##offset:						\
{	.mmi ;								\
	alloc PFS = ar.pfs, _NINPUTS, _NLOCALS, _NOUTPUT, _NROTATE ;	\
	mov LTrip = 2 ;							\
	mov ar.lc = 3 ;							\
} ;;									\
{	.mii ;								\
	cmp.eq pMore, p0 = r0, r0 ;					\
	mov ar.ec = 0 ;							\
	nop 0x0 ;							\
} ;;									\
									\
	.pred.rel "mutex", pLoad, pSkip ;				\
.md5_FF_round##offset:							\
	FFLOADU(A, B, C, D, M12, M13, RotateM0, 7, offset)		\
	FFLOADU(D, A, B, C, M13, M14, RotateM1, 12, offset)		\
	FFLOADU(C, D, A, B, M14, M15, RotateM2, 17, offset)		\
	FFLOOPU(B, C, D, A, M15, RotateM0, RotateM3, 22, offset)	\
									\
{	.mib ;								\
	nop 0x0 ;							\
	nop 0x0 ;							\
	br.cond.sptk.many md5_digest_GHI ;				\
} ;;									\
	.endp md5_digest_block##offset

MD5FBLOCK(1)
MD5FBLOCK(2)
MD5FBLOCK(3)

	.align 64
	.type md5_constants, @object
md5_constants:
.md5_tbl_data_order:			// To ensure little-endian data
					// order, code as bytes.
	data1 0x78, 0xa4, 0x6a, 0xd7	//     0
	data1 0x56, 0xb7, 0xc7, 0xe8	//     1
	data1 0xdb, 0x70, 0x20, 0x24	//     2
	data1 0xee, 0xce, 0xbd, 0xc1	//     3
	data1 0xaf, 0x0f, 0x7c, 0xf5	//     4
	data1 0x2a, 0xc6, 0x87, 0x47	//     5
	data1 0x13, 0x46, 0x30, 0xa8	//     6
	data1 0x01, 0x95, 0x46, 0xfd	//     7
	data1 0xd8, 0x98, 0x80, 0x69	//     8
	data1 0xaf, 0xf7, 0x44, 0x8b	//     9
	data1 0xb1, 0x5b, 0xff, 0xff	//    10
	data1 0xbe, 0xd7, 0x5c, 0x89	//    11
	data1 0x22, 0x11, 0x90, 0x6b	//    12
	data1 0x93, 0x71, 0x98, 0xfd	//    13
	data1 0x8e, 0x43, 0x79, 0xa6	//    14
	data1 0x21, 0x08, 0xb4, 0x49	//    15
	data1 0x62, 0x25, 0x1e, 0xf6	//    16
	data1 0x40, 0xb3, 0x40, 0xc0	//    17
	data1 0x51, 0x5a, 0x5e, 0x26	//    18
	data1 0xaa, 0xc7, 0xb6, 0xe9	//    19
	data1 0x5d, 0x10, 0x2f, 0xd6	//    20
	data1 0x53, 0x14, 0x44, 0x02	//    21
	data1 0x81, 0xe6, 0xa1, 0xd8	//    22
	data1 0xc8, 0xfb, 0xd3, 0xe7	//    23
	data1 0xe6, 0xcd, 0xe1, 0x21	//    24
	data1 0xd6, 0x07, 0x37, 0xc3	//    25
	data1 0x87, 0x0d, 0xd5, 0xf4	//    26
	data1 0xed, 0x14, 0x5a, 0x45	//    27
	data1 0x05, 0xe9, 0xe3, 0xa9	//    28
	data1 0xf8, 0xa3, 0xef, 0xfc	//    29
	data1 0xd9, 0x02, 0x6f, 0x67	//    30
	data1 0x8a, 0x4c, 0x2a, 0x8d	//    31
	data1 0x42, 0x39, 0xfa, 0xff	//    32
	data1 0x81, 0xf6, 0x71, 0x87	//    33
	data1 0x22, 0x61, 0x9d, 0x6d	//    34
	data1 0x0c, 0x38, 0xe5, 0xfd	//    35
	data1 0x44, 0xea, 0xbe, 0xa4	//    36
	data1 0xa9, 0xcf, 0xde, 0x4b	//    37
	data1 0x60, 0x4b, 0xbb, 0xf6	//    38
	data1 0x70, 0xbc, 0xbf, 0xbe	//    39
	data1 0xc6, 0x7e, 0x9b, 0x28	//    40
	data1 0xfa, 0x27, 0xa1, 0xea	//    41
	data1 0x85, 0x30, 0xef, 0xd4	//    42
	data1 0x05, 0x1d, 0x88, 0x04	//    43
	data1 0x39, 0xd0, 0xd4, 0xd9	//    44
	data1 0xe5, 0x99, 0xdb, 0xe6	//    45
	data1 0xf8, 0x7c, 0xa2, 0x1f	//    46
	data1 0x65, 0x56, 0xac, 0xc4	//    47
	data1 0x44, 0x22, 0x29, 0xf4	//    48
	data1 0x97, 0xff, 0x2a, 0x43	//    49
	data1 0xa7, 0x23, 0x94, 0xab	//    50
	data1 0x39, 0xa0, 0x93, 0xfc	//    51
	data1 0xc3, 0x59, 0x5b, 0x65	//    52
	data1 0x92, 0xcc, 0x0c, 0x8f	//    53
	data1 0x7d, 0xf4, 0xef, 0xff	//    54
	data1 0xd1, 0x5d, 0x84, 0x85	//    55
	data1 0x4f, 0x7e, 0xa8, 0x6f	//    56
	data1 0xe0, 0xe6, 0x2c, 0xfe	//    57
	data1 0x14, 0x43, 0x01, 0xa3	//    58
	data1 0xa1, 0x11, 0x08, 0x4e	//    59
	data1 0x82, 0x7e, 0x53, 0xf7	//    60
	data1 0x35, 0xf2, 0x3a, 0xbd	//    61
	data1 0xbb, 0xd2, 0xd7, 0x2a	//    62
	data1 0x91, 0xd3, 0x86, 0xeb	//    63
.size	md5_constants#,64*4
