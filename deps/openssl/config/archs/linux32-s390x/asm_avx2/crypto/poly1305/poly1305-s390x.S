#include "s390x_arch.h"
.text
.globl	poly1305_init
.type	poly1305_init,@function
.align	16
poly1305_init:
	lghi	%r0,0
	lghi	%r1,-1
	stg	%r0,0(%r2)
	stg	%r0,8(%r2)
	stg	%r0,16(%r2)
	st	%r0,24(%r2)
	lgr	%r5,%r2
	lghi	%r2,0
	clr	%r3,%r0
	je	.Lno_key
	lrvg	%r2,0(%r3)
	lrvg	%r3,8(%r3)
	nihl	%r1,65472
	srlg	%r0,%r1,4
	srlg	%r1,%r1,4
	nill	%r1,65532
	ngr	%r2,%r0
	ngr	%r3,%r1
	stmg	%r2,%r3,32(%r5)
	larl	%r1,OPENSSL_s390xcap_P
	lg	%r0,16(%r1)
	srlg	%r0,%r0,62
	nill	%r0,1
	lcgr	%r0,%r0
	larl	%r1,.Lpoly1305_blocks
	larl	%r2,.Lpoly1305_blocks_vx
	larl	%r3,.Lpoly1305_emit
	xr	%r2,%r1
	nr	%r2,%r0
	xr	%r2,%r1
	stm	%r2,%r3,0(%r4)
	lghi	%r2,1
.Lno_key:
	br	%r14
.size	poly1305_init,.-poly1305_init
.globl	poly1305_blocks
.type	poly1305_blocks,@function
.align	16
poly1305_blocks:
.Lpoly1305_blocks:
	ltr	%r0,%r4
	jz	.Lno_data
	stm	%r6,%r14,6*4(%r15)
	lg	%r11,0(%r2)
	lg	%r13,8(%r2)
	lg	%r14,16(%r2)
.Lpoly1305_blocks_entry:
	srl	%r4,4
	llgfr	%r5,%r5
	lg	%r0,32(%r2)
	lg	%r1,40(%r2)
	st	%r2,2*4(%r15)
	srlg	%r2,%r1,2
	algr	%r2,%r1
	j	.Loop
.align	16
.Loop:
	lrvg	%r7,0(%r3)
	lrvg	%r9,8(%r3)
	la	%r3,16(%r3)
	algr	%r7,%r11
	alcgr	%r9,%r13
	alcgr	%r14,%r5
	lgr	%r11,%r7
	mlgr	%r6,%r0
	lgr	%r13,%r9
	mlgr	%r8,%r2
	mlgr	%r10,%r1
	mlgr	%r12,%r0
	algr	%r7,%r9
	lgr	%r9,%r14
	alcgr	%r6,%r8
	lghi	%r8,0
	algr	%r13,%r11
	alcgr	%r12,%r10
	msgr	%r9,%r2
	msgr	%r14,%r0
	algr	%r13,%r9
	alcgr	%r12,%r8
	algr	%r13,%r6
	alcgr	%r14,%r12
	lghi	%r11,-4
	ngr	%r11,%r14
	srlg	%r10,%r14,2
	algr	%r11,%r10
	lghi	%r12,3
	ngr	%r14,%r12
	algr	%r11,%r7
	alcgr	%r13,%r8
	alcgr	%r14,%r8
	brct	%r4,.Loop
	l	%r2,2*4(%r15)
	stg	%r11,0(%r2)
	stg	%r13,8(%r2)
	stg	%r14,16(%r2)
	lm	%r6,%r14,6*4(%r15)
.Lno_data:
	br	%r14
.size	poly1305_blocks,.-poly1305_blocks
.type	poly1305_blocks_vx,@function
.align	16
poly1305_blocks_vx:
.Lpoly1305_blocks_vx:
	.word	0xc24f,000000,0x0080	# clfi	%r4,128
	jhe	__poly1305_blocks_vx
	stm	%r6,%r14,6*4(%r15)
	lg	%r10,0(%r2)
	lg	%r12,8(%r2)
	lg	%r9,16(%r2)
	llgfr	%r0,%r10
	srlg	%r11,%r10,32
	llgfr	%r1,%r12
	srlg	%r13,%r12,32
	srlg	%r14,%r9,32
	sllg	%r0,%r0,26
	algr	%r11,%r0
	sllg	%r0,%r13,52
	srlg	%r13,%r13,12
	sllg	%r1,%r1,14
	algr	%r11,%r0
	alcgr	%r13,%r1
	sllg	%r0,%r14,40
	srlg	%r14,%r14,24
	lghi	%r1,0
	algr	%r13,%r0
	alcgr	%r14,%r1
	llgf	%r0,24(%r2)
	lcgr	%r0,%r0
	xgr	%r11,%r10
	xgr	%r13,%r12
	xgr	%r14,%r9
	ngr	%r11,%r0
	ngr	%r13,%r0
	ngr	%r14,%r0
	xgr	%r11,%r10
	xgr	%r13,%r12
	xgr	%r14,%r9
	lhi	%r0,0
	st	%r0,24(%r2)
	j	.Lpoly1305_blocks_entry
.size	poly1305_blocks_vx,.-poly1305_blocks_vx
.type	__poly1305_mul,@function
.align	16
__poly1305_mul:
	.word	0xe730,0xa000,0x28a5	# vmlo	%v19,%v0,%v10,2
	.word	0xe740,0xb000,0x28a5	# vmlo	%v20,%v0,%v11,2
	.word	0xe750,0xd000,0x28a5	# vmlo	%v21,%v0,%v13,2
	.word	0xe760,0xf000,0x28a5	# vmlo	%v22,%v0,%v15,2
	.word	0xe770,0x1000,0x2aa5	# vmlo	%v23,%v0,%v17,2
	.word	0xe731,0x2200,0x3bad	# vmalo	%v19,%v1,%v18,%v19,2
	.word	0xe741,0xa200,0x49ad	# vmalo	%v20,%v1,%v10,%v20,2
	.word	0xe751,0xb200,0x59ad	# vmalo	%v21,%v1,%v11,%v21,2
	.word	0xe761,0xd200,0x69ad	# vmalo	%v22,%v1,%v13,%v22,2
	.word	0xe771,0xf200,0x79ad	# vmalo	%v23,%v1,%v15,%v23,2
	.word	0xe732,0x0200,0x3bad	# vmalo	%v19,%v2,%v16,%v19,2
	.word	0xe742,0x2200,0x4bad	# vmalo	%v20,%v2,%v18,%v20,2
	.word	0xe752,0xa200,0x59ad	# vmalo	%v21,%v2,%v10,%v21,2
	.word	0xe762,0xb200,0x69ad	# vmalo	%v22,%v2,%v11,%v22,2
	.word	0xe772,0xd200,0x79ad	# vmalo	%v23,%v2,%v13,%v23,2
	.word	0xe733,0xe200,0x39ad	# vmalo	%v19,%v3,%v14,%v19,2
	.word	0xe743,0x0200,0x4bad	# vmalo	%v20,%v3,%v16,%v20,2
	.word	0xe753,0x2200,0x5bad	# vmalo	%v21,%v3,%v18,%v21,2
	.word	0xe763,0xa200,0x69ad	# vmalo	%v22,%v3,%v10,%v22,2
	.word	0xe773,0xb200,0x79ad	# vmalo	%v23,%v3,%v11,%v23,2
	.word	0xe734,0xc200,0x39ad	# vmalo	%v19,%v4,%v12,%v19,2
	.word	0xe744,0xe200,0x49ad	# vmalo	%v20,%v4,%v14,%v20,2
	.word	0xe754,0x0200,0x5bad	# vmalo	%v21,%v4,%v16,%v21,2
	.word	0xe764,0x2200,0x6bad	# vmalo	%v22,%v4,%v18,%v22,2
	.word	0xe774,0xa200,0x79ad	# vmalo	%v23,%v4,%v10,%v23,2
	.word	0xe746,0x001a,0x3438	# vesrl	%v4,%v22,26,3
	.word	0xe713,0x001a,0x3438	# vesrl	%v1,%v19,26,3
	.word	0xe736,0xc000,0x0668	# vn	%v3,%v22,%v28
	.word	0xe703,0xc000,0x0668	# vn	%v0,%v19,%v28
	.word	0xe744,0x7000,0x32f3	# va	%v4,%v4,%v23,3
	.word	0xe711,0x4000,0x32f3	# va	%v1,%v1,%v20,3
	.word	0xe774,0x001a,0x3838	# vesrl	%v23,%v4,26,3
	.word	0xe741,0x001a,0x3838	# vesrl	%v20,%v1,26,3
	.word	0xe744,0xc000,0x0268	# vn	%v4,%v4,%v28
	.word	0xe711,0xc000,0x0268	# vn	%v1,%v1,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe725,0x4000,0x36f3	# va	%v2,%v21,%v20,3
	.word	0xe777,0x0002,0x3c30	# vesl	%v23,%v23,2,3
	.word	0xe752,0x001a,0x3838	# vesrl	%v21,%v2,26,3
	.word	0xe722,0xc000,0x0268	# vn	%v2,%v2,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe733,0x5000,0x32f3	# va	%v3,%v3,%v21,3
	.word	0xe730,0x001a,0x3838	# vesrl	%v19,%v0,26,3
	.word	0xe763,0x001a,0x3838	# vesrl	%v22,%v3,26,3
	.word	0xe700,0xc000,0x0268	# vn	%v0,%v0,%v28
	.word	0xe733,0xc000,0x0268	# vn	%v3,%v3,%v28
	.word	0xe711,0x3000,0x32f3	# va	%v1,%v1,%v19,3
	.word	0xe744,0x6000,0x32f3	# va	%v4,%v4,%v22,3
	br	%r14
.size	__poly1305_mul,.-__poly1305_mul
.type	__poly1305_blocks_vx,@function
.align	16
__poly1305_blocks_vx:
	lr	%r0,%r15
	stm	%r10,%r15,10*4(%r15)
	std	%f4,16*4+2*8(%r15)
	std	%f6,16*4+3*8(%r15)
	ahi	%r15,-96
	st	%r0,0(%r15)
	llgfr	%r4,%r4
	larl	%r1,.Lconst
	.word	0xe7c0,0x263f,0x3846	# vgm	%v28,38,63,3
	.word	0xe7df,0x1010,0x0c36	# vlm	%v29,%v31,16(%r1)
	.word	0xe300,0x2018,0x0012	# lt	%r0,24(%r2)
	jnz	.Lskip_init
	lg	%r11,32(%r2)
	lg	%r13,40(%r2)
	.word	0xecab,0x26bf,0x2655	# risbg	%r10,%r11,38,191,38
	srlg	%r12,%r11,52
	.word	0xecbb,0x26bf,0x0055	# risbg	%r11,%r11,38,191,0
	.word	0xe7ab,000000,0x3022	# vlvg	%v10,%r11,0,3
	.word	0xeccd,0x2633,0x0c55	# risbg	%r12,%r13,38,51,12
	.word	0xe7ba,000000,0x3022	# vlvg	%v11,%r10,0,3
	.word	0xecad,0x263f,0x3255	# risbg	%r10,%r13,38,63,50
	.word	0xe7dc,000000,0x3022	# vlvg	%v13,%r12,0,3
	srlg	%r12,%r13,40
	.word	0xe7fa,000000,0x3022	# vlvg	%v15,%r10,0,3
	.word	0xe71c,000000,0x3822	# vlvg	%v17,%r12,0,3
	.word	0xe7cb,0x0002,0x3030	# vesl	%v12,%v11,2,3
	.word	0xe7ed,0x0002,0x3030	# vesl	%v14,%v13,2,3
	.word	0xe70f,0x0002,0x3830	# vesl	%v16,%v15,2,3
	.word	0xe721,0x0002,0x3c30	# vesl	%v18,%v17,2,3
	.word	0xe70a,000000,0x0056	# vlr	%v0,%v10
	.word	0xe71b,000000,0x0056	# vlr	%v1,%v11
	.word	0xe72d,000000,0x0056	# vlr	%v2,%v13
	.word	0xe73f,000000,0x0056	# vlr	%v3,%v15
	.word	0xe741,000000,0x0456	# vlr	%v4,%v17
	.word	0xe7cc,0xb000,0x30f3	# va	%v12,%v12,%v11,3
	.word	0xe7ee,0xd000,0x30f3	# va	%v14,%v14,%v13,3
	.word	0xe700,0xf000,0x3cf3	# va	%v16,%v16,%v15,3
	.word	0xe722,0x1000,0x3ef3	# va	%v18,%v18,%v17,3
	brasl	%r14,__poly1305_mul
	.word	0xe7a0,0xa000,0x0084	# vpdi	%v10,%v0,%v10,0
	.word	0xe7b1,0xb000,0x0084	# vpdi	%v11,%v1,%v11,0
	.word	0xe7d2,0xd000,0x0084	# vpdi	%v13,%v2,%v13,0
	.word	0xe7f3,0xf000,0x0084	# vpdi	%v15,%v3,%v15,0
	.word	0xe714,0x1000,0x0a84	# vpdi	%v17,%v4,%v17,0
	.word	0xe700,000000,0x0084	# vpdi	%v0,%v0,%v0,0
	.word	0xe711,0x1000,0x0084	# vpdi	%v1,%v1,%v1,0
	.word	0xe722,0x2000,0x0084	# vpdi	%v2,%v2,%v2,0
	.word	0xe733,0x3000,0x0084	# vpdi	%v3,%v3,%v3,0
	.word	0xe744,0x4000,0x0084	# vpdi	%v4,%v4,%v4,0
	.word	0xe7cb,0x0002,0x3030	# vesl	%v12,%v11,2,3
	.word	0xe7ed,0x0002,0x3030	# vesl	%v14,%v13,2,3
	.word	0xe70f,0x0002,0x3830	# vesl	%v16,%v15,2,3
	.word	0xe721,0x0002,0x3c30	# vesl	%v18,%v17,2,3
	.word	0xe7cc,0xb000,0x30f3	# va	%v12,%v12,%v11,3
	.word	0xe7ee,0xd000,0x30f3	# va	%v14,%v14,%v13,3
	.word	0xe700,0xf000,0x3cf3	# va	%v16,%v16,%v15,3
	.word	0xe722,0x1000,0x3ef3	# va	%v18,%v18,%v17,3
	brasl	%r14,__poly1305_mul
	.word	0xe750,0x1000,0x0006	# vl	%v5,0(%r1)
	.word	0xe7aa,000000,0x508c	# vperm	%v10,%v10,%v0,%v5
	.word	0xe7bb,0x1000,0x508c	# vperm	%v11,%v11,%v1,%v5
	.word	0xe7dd,0x2000,0x508c	# vperm	%v13,%v13,%v2,%v5
	.word	0xe7ff,0x3000,0x508c	# vperm	%v15,%v15,%v3,%v5
	.word	0xe711,0x4000,0x5c8c	# vperm	%v17,%v17,%v4,%v5
	.word	0xe7cb,0x0002,0x2030	# vesl	%v12,%v11,2,2
	.word	0xe7ed,0x0002,0x2030	# vesl	%v14,%v13,2,2
	.word	0xe70f,0x0002,0x2830	# vesl	%v16,%v15,2,2
	.word	0xe721,0x0002,0x2c30	# vesl	%v18,%v17,2,2
	.word	0xe7cc,0xb000,0x20f3	# va	%v12,%v12,%v11,2
	.word	0xe7ee,0xd000,0x20f3	# va	%v14,%v14,%v13,2
	.word	0xe700,0xf000,0x2cf3	# va	%v16,%v16,%v15,2
	.word	0xe722,0x1000,0x2ef3	# va	%v18,%v18,%v17,2
	lg	%r11,0(%r2)
	lg	%r13,8(%r2)
	lg	%r14,16(%r2)
	.word	0xe700,000000,0x0044	# vgbm	%v0,0
	.word	0xe710,000000,0x0044	# vgbm	%v1,0
	.word	0xe720,000000,0x0044	# vgbm	%v2,0
	.word	0xe730,000000,0x0044	# vgbm	%v3,0
	.word	0xe740,000000,0x0044	# vgbm	%v4,0
	.word	0xecab,0x26bf,0x2655	# risbg	%r10,%r11,38,191,38
	srlg	%r12,%r11,52
	.word	0xecbb,0x26bf,0x0055	# risbg	%r11,%r11,38,191,0
	.word	0xe70b,000000,0x3022	# vlvg	%v0,%r11,0,3
	.word	0xeccd,0x2633,0x0c55	# risbg	%r12,%r13,38,51,12
	.word	0xe71a,000000,0x3022	# vlvg	%v1,%r10,0,3
	.word	0xecad,0x263f,0x3255	# risbg	%r10,%r13,38,63,50
	.word	0xe72c,000000,0x3022	# vlvg	%v2,%r12,0,3
	srlg	%r12,%r13,40
	.word	0xe73a,000000,0x3022	# vlvg	%v3,%r10,0,3
	.word	0xecce,0x2527,0x1855	# risbg	%r12,%r14,37,39,24
	.word	0xe74c,000000,0x3022	# vlvg	%v4,%r12,0,3
	lhi	%r0,1
	st	%r0,24(%r2)
	.word	0xe7a2,0x2030,0x043e	# vstm	%v10,%v18,48(%r2)
	.word	0xe7aa,0xa000,0x0084	# vpdi	%v10,%v10,%v10,0
	.word	0xe7bb,0xb000,0x0084	# vpdi	%v11,%v11,%v11,0
	.word	0xe7cc,0xc000,0x0084	# vpdi	%v12,%v12,%v12,0
	.word	0xe7dd,0xd000,0x0084	# vpdi	%v13,%v13,%v13,0
	.word	0xe7ee,0xe000,0x0084	# vpdi	%v14,%v14,%v14,0
	.word	0xe7ff,0xf000,0x0084	# vpdi	%v15,%v15,%v15,0
	.word	0xe700,000000,0x0e84	# vpdi	%v16,%v16,%v16,0
	.word	0xe711,0x1000,0x0e84	# vpdi	%v17,%v17,%v17,0
	.word	0xe722,0x2000,0x0e84	# vpdi	%v18,%v18,%v18,0
	j	.Loaded_hash
.align	16
.Lskip_init:
	.word	0xe700,0x2000,0x2004	# vllez	%v0,0(%r2),2
	.word	0xe710,0x2004,0x2004	# vllez	%v1,4(%r2),2
	.word	0xe720,0x2008,0x2004	# vllez	%v2,8(%r2),2
	.word	0xe730,0x200c,0x2004	# vllez	%v3,12(%r2),2
	.word	0xe740,0x2010,0x2004	# vllez	%v4,16(%r2),2
	.word	0xe7a0,0x2030,0x3005	# vlrep	%v10,0x30(%r2),3
	.word	0xe7b0,0x2040,0x3005	# vlrep	%v11,0x40(%r2),3
	.word	0xe7c0,0x2050,0x3005	# vlrep	%v12,0x50(%r2),3
	.word	0xe7d0,0x2060,0x3005	# vlrep	%v13,0x60(%r2),3
	.word	0xe7e0,0x2070,0x3005	# vlrep	%v14,0x70(%r2),3
	.word	0xe7f0,0x2080,0x3005	# vlrep	%v15,0x80(%r2),3
	.word	0xe700,0x2090,0x3805	# vlrep	%v16,0x90(%r2),3
	.word	0xe710,0x20a0,0x3805	# vlrep	%v17,0xa0(%r2),3
	.word	0xe720,0x20b0,0x3805	# vlrep	%v18,0xb0(%r2),3
.Loaded_hash:
	.word	0xe760,000000,0x0044	# vgbm	%v6,0
	.word	0xe780,000000,0x0044	# vgbm	%v8,0
	.word	0xe78b,0x3000,0x0c36	# vlm	%v24,%v27,0x00(%r3)
	la	%r3,0x40(%r3)
	.word	0xe7c0,0x061f,0x3846	# vgm	%v28,6,31,3
	.word	0xe790,0x0505,0x2046	# vgm	%v9,5,5,2
	.word	0xe75a,0xb000,0xd78c	# vperm	%v5,%v26,%v27,%v29
	.word	0xe77a,0xb000,0xf78c	# vperm	%v7,%v26,%v27,%v31
	.word	0xe7aa,0xb000,0xef8c	# vperm	%v26,%v26,%v27,%v30
	.word	0xe765,0xc006,0x3272	# verim	%v6,%v5,%v28,6,3
	.word	0xe755,0x0020,0x3030	# vesl	%v5,%v5,32,3
	.word	0xe777,0x001c,0x3030	# vesl	%v7,%v7,28,3
	.word	0xe78a,0xc012,0x3672	# verim	%v8,%v26,%v28,18,3
	.word	0xe79a,0xc03a,0x3672	# verim	%v9,%v26,%v28,58,3
	.word	0xe755,0xc000,0x0268	# vn	%v5,%v5,%v28
	.word	0xe777,0xc000,0x0268	# vn	%v7,%v7,%v28
	.word	0xe799,0x0002,0x2038	# vesrl	%v9,%v9,2,2
	.word	0xe7c0,0x263f,0x3846	# vgm	%v28,38,63,3
	.word	0xe7a8,0x9000,0xdf8c	# vperm	%v26,%v24,%v25,%v29
	.word	0xe7b8,0x9000,0xef8c	# vperm	%v27,%v24,%v25,%v30
	.word	0xe798,0x9000,0xff8c	# vperm	%v25,%v24,%v25,%v31
	.word	0xe75a,0xc000,0x3672	# verim	%v5,%v26,%v28,0,3
	.word	0xe76a,0xc026,0x3672	# verim	%v6,%v26,%v28,38,3
	.word	0xe779,0xc03c,0x3672	# verim	%v7,%v25,%v28,60,3
	.word	0xe78b,0xc032,0x3672	# verim	%v8,%v27,%v28,50,3
	.word	0xe7bb,0x0028,0x3c38	# vesrl	%v27,%v27,40,3
	.word	0xe799,0xb000,0x026a	# vo	%v9,%v9,%v27
	srlg	%r0,%r4,6
	ahi	%r0,-1
.align	16
.Loop_vx:
	.word	0xe735,0xa000,0x28a4	# vmle	%v19,%v5,%v10,2
	.word	0xe745,0xb000,0x28a4	# vmle	%v20,%v5,%v11,2
	.word	0xe755,0xd000,0x28a4	# vmle	%v21,%v5,%v13,2
	.word	0xe765,0xf000,0x28a4	# vmle	%v22,%v5,%v15,2
	.word	0xe775,0x1000,0x2aa4	# vmle	%v23,%v5,%v17,2
	.word	0xe736,0x2200,0x3bac	# vmale	%v19,%v6,%v18,%v19,2
	.word	0xe746,0xa200,0x49ac	# vmale	%v20,%v6,%v10,%v20,2
	.word	0xe756,0xb200,0x59ac	# vmale	%v21,%v6,%v11,%v21,2
	.word	0xe766,0xd200,0x69ac	# vmale	%v22,%v6,%v13,%v22,2
	.word	0xe776,0xf200,0x79ac	# vmale	%v23,%v6,%v15,%v23,2
	.word	0xe722,0x7000,0x20f3	# va	%v2,%v2,%v7,2
	.word	0xe700,0x5000,0x20f3	# va	%v0,%v0,%v5,2
	.word	0xe733,0x8000,0x20f3	# va	%v3,%v3,%v8,2
	.word	0xe711,0x6000,0x20f3	# va	%v1,%v1,%v6,2
	.word	0xe744,0x9000,0x20f3	# va	%v4,%v4,%v9,2
	.word	0xe737,0x0200,0x3bac	# vmale	%v19,%v7,%v16,%v19,2
	.word	0xe747,0x2200,0x4bac	# vmale	%v20,%v7,%v18,%v20,2
	.word	0xe757,0xa200,0x59ac	# vmale	%v21,%v7,%v10,%v21,2
	.word	0xe767,0xb200,0x69ac	# vmale	%v22,%v7,%v11,%v22,2
	.word	0xe777,0xd200,0x79ac	# vmale	%v23,%v7,%v13,%v23,2
	.word	0xe78b,0x3000,0x0c36	# vlm	%v24,%v27,0x00(%r3)
	la	%r3,0x40(%r3)
	.word	0xe7c0,0x061f,0x3846	# vgm	%v28,6,31,3
	.word	0xe738,0xe200,0x39ac	# vmale	%v19,%v8,%v14,%v19,2
	.word	0xe748,0x0200,0x4bac	# vmale	%v20,%v8,%v16,%v20,2
	.word	0xe758,0x2200,0x5bac	# vmale	%v21,%v8,%v18,%v21,2
	.word	0xe768,0xa200,0x69ac	# vmale	%v22,%v8,%v10,%v22,2
	.word	0xe778,0xb200,0x79ac	# vmale	%v23,%v8,%v11,%v23,2
	.word	0xe75a,0xb000,0xd78c	# vperm	%v5,%v26,%v27,%v29
	.word	0xe77a,0xb000,0xf78c	# vperm	%v7,%v26,%v27,%v31
	.word	0xe7aa,0xb000,0xef8c	# vperm	%v26,%v26,%v27,%v30
	.word	0xe739,0xc200,0x39ac	# vmale	%v19,%v9,%v12,%v19,2
	.word	0xe749,0xe200,0x49ac	# vmale	%v20,%v9,%v14,%v20,2
	.word	0xe759,0x0200,0x5bac	# vmale	%v21,%v9,%v16,%v21,2
	.word	0xe769,0x2200,0x6bac	# vmale	%v22,%v9,%v18,%v22,2
	.word	0xe779,0xa200,0x79ac	# vmale	%v23,%v9,%v10,%v23,2
	.word	0xe765,0xc006,0x3272	# verim	%v6,%v5,%v28,6,3
	.word	0xe755,0x0020,0x3030	# vesl	%v5,%v5,32,3
	.word	0xe777,0x001c,0x3030	# vesl	%v7,%v7,28,3
	.word	0xe78a,0xc012,0x3672	# verim	%v8,%v26,%v28,18,3
	.word	0xe730,0xa200,0x39ad	# vmalo	%v19,%v0,%v10,%v19,2
	.word	0xe740,0xb200,0x49ad	# vmalo	%v20,%v0,%v11,%v20,2
	.word	0xe750,0xd200,0x59ad	# vmalo	%v21,%v0,%v13,%v21,2
	.word	0xe760,0xf200,0x69ad	# vmalo	%v22,%v0,%v15,%v22,2
	.word	0xe770,0x1200,0x7bad	# vmalo	%v23,%v0,%v17,%v23,2
	.word	0xe790,0x0505,0x2046	# vgm	%v9,5,5,2
	.word	0xe79a,0xc03a,0x3672	# verim	%v9,%v26,%v28,58,3
	.word	0xe755,0xc000,0x0268	# vn	%v5,%v5,%v28
	.word	0xe777,0xc000,0x0268	# vn	%v7,%v7,%v28
	.word	0xe799,0x0002,0x2038	# vesrl	%v9,%v9,2,2
	.word	0xe731,0x2200,0x3bad	# vmalo	%v19,%v1,%v18,%v19,2
	.word	0xe741,0xa200,0x49ad	# vmalo	%v20,%v1,%v10,%v20,2
	.word	0xe751,0xb200,0x59ad	# vmalo	%v21,%v1,%v11,%v21,2
	.word	0xe761,0xd200,0x69ad	# vmalo	%v22,%v1,%v13,%v22,2
	.word	0xe771,0xf200,0x79ad	# vmalo	%v23,%v1,%v15,%v23,2
	.word	0xe7c0,0x263f,0x3846	# vgm	%v28,38,63,3
	.word	0xe7a8,0x9000,0xdf8c	# vperm	%v26,%v24,%v25,%v29
	.word	0xe7b8,0x9000,0xef8c	# vperm	%v27,%v24,%v25,%v30
	.word	0xe798,0x9000,0xff8c	# vperm	%v25,%v24,%v25,%v31
	.word	0xe732,0x0200,0x3bad	# vmalo	%v19,%v2,%v16,%v19,2
	.word	0xe742,0x2200,0x4bad	# vmalo	%v20,%v2,%v18,%v20,2
	.word	0xe752,0xa200,0x59ad	# vmalo	%v21,%v2,%v10,%v21,2
	.word	0xe762,0xb200,0x69ad	# vmalo	%v22,%v2,%v11,%v22,2
	.word	0xe772,0xd200,0x79ad	# vmalo	%v23,%v2,%v13,%v23,2
	.word	0xe75a,0xc000,0x3672	# verim	%v5,%v26,%v28,0,3
	.word	0xe76a,0xc026,0x3672	# verim	%v6,%v26,%v28,38,3
	.word	0xe779,0xc03c,0x3672	# verim	%v7,%v25,%v28,60,3
	.word	0xe733,0xe200,0x39ad	# vmalo	%v19,%v3,%v14,%v19,2
	.word	0xe743,0x0200,0x4bad	# vmalo	%v20,%v3,%v16,%v20,2
	.word	0xe753,0x2200,0x5bad	# vmalo	%v21,%v3,%v18,%v21,2
	.word	0xe763,0xa200,0x69ad	# vmalo	%v22,%v3,%v10,%v22,2
	.word	0xe773,0xb200,0x79ad	# vmalo	%v23,%v3,%v11,%v23,2
	.word	0xe78b,0xc032,0x3672	# verim	%v8,%v27,%v28,50,3
	.word	0xe7bb,0x0028,0x3c38	# vesrl	%v27,%v27,40,3
	.word	0xe799,0xb000,0x026a	# vo	%v9,%v9,%v27
	.word	0xe734,0xc200,0x39ad	# vmalo	%v19,%v4,%v12,%v19,2
	.word	0xe744,0xe200,0x49ad	# vmalo	%v20,%v4,%v14,%v20,2
	.word	0xe754,0x0200,0x5bad	# vmalo	%v21,%v4,%v16,%v21,2
	.word	0xe764,0x2200,0x6bad	# vmalo	%v22,%v4,%v18,%v22,2
	.word	0xe774,0xa200,0x79ad	# vmalo	%v23,%v4,%v10,%v23,2
	.word	0xe746,0x001a,0x3438	# vesrl	%v4,%v22,26,3
	.word	0xe713,0x001a,0x3438	# vesrl	%v1,%v19,26,3
	.word	0xe736,0xc000,0x0668	# vn	%v3,%v22,%v28
	.word	0xe703,0xc000,0x0668	# vn	%v0,%v19,%v28
	.word	0xe744,0x7000,0x32f3	# va	%v4,%v4,%v23,3
	.word	0xe711,0x4000,0x32f3	# va	%v1,%v1,%v20,3
	.word	0xe774,0x001a,0x3838	# vesrl	%v23,%v4,26,3
	.word	0xe741,0x001a,0x3838	# vesrl	%v20,%v1,26,3
	.word	0xe744,0xc000,0x0268	# vn	%v4,%v4,%v28
	.word	0xe711,0xc000,0x0268	# vn	%v1,%v1,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe725,0x4000,0x36f3	# va	%v2,%v21,%v20,3
	.word	0xe777,0x0002,0x3c30	# vesl	%v23,%v23,2,3
	.word	0xe752,0x001a,0x3838	# vesrl	%v21,%v2,26,3
	.word	0xe722,0xc000,0x0268	# vn	%v2,%v2,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe733,0x5000,0x32f3	# va	%v3,%v3,%v21,3
	.word	0xe730,0x001a,0x3838	# vesrl	%v19,%v0,26,3
	.word	0xe763,0x001a,0x3838	# vesrl	%v22,%v3,26,3
	.word	0xe700,0xc000,0x0268	# vn	%v0,%v0,%v28
	.word	0xe733,0xc000,0x0268	# vn	%v3,%v3,%v28
	.word	0xe711,0x3000,0x32f3	# va	%v1,%v1,%v19,3
	.word	0xe744,0x6000,0x32f3	# va	%v4,%v4,%v22,3
	brct	%r0,.Loop_vx
	.word	0xe7a2,0x2030,0x0436	# vlm	%v10,%v18,48(%r2)
	lghi	%r0,48
	lcr	%r4,%r4
	nr	%r4,%r0
	slr	%r3,%r4
.Last:
	.word	0xe735,0xa000,0x28a4	# vmle	%v19,%v5,%v10,2
	.word	0xe745,0xb000,0x28a4	# vmle	%v20,%v5,%v11,2
	.word	0xe755,0xd000,0x28a4	# vmle	%v21,%v5,%v13,2
	.word	0xe765,0xf000,0x28a4	# vmle	%v22,%v5,%v15,2
	.word	0xe775,0x1000,0x2aa4	# vmle	%v23,%v5,%v17,2
	.word	0xe736,0x2200,0x3bac	# vmale	%v19,%v6,%v18,%v19,2
	.word	0xe746,0xa200,0x49ac	# vmale	%v20,%v6,%v10,%v20,2
	.word	0xe756,0xb200,0x59ac	# vmale	%v21,%v6,%v11,%v21,2
	.word	0xe766,0xd200,0x69ac	# vmale	%v22,%v6,%v13,%v22,2
	.word	0xe776,0xf200,0x79ac	# vmale	%v23,%v6,%v15,%v23,2
	.word	0xe700,0x5000,0x20f3	# va	%v0,%v0,%v5,2
	.word	0xe711,0x6000,0x20f3	# va	%v1,%v1,%v6,2
	.word	0xe722,0x7000,0x20f3	# va	%v2,%v2,%v7,2
	.word	0xe733,0x8000,0x20f3	# va	%v3,%v3,%v8,2
	.word	0xe744,0x9000,0x20f3	# va	%v4,%v4,%v9,2
	.word	0xe737,0x0200,0x3bac	# vmale	%v19,%v7,%v16,%v19,2
	.word	0xe747,0x2200,0x4bac	# vmale	%v20,%v7,%v18,%v20,2
	.word	0xe757,0xa200,0x59ac	# vmale	%v21,%v7,%v10,%v21,2
	.word	0xe767,0xb200,0x69ac	# vmale	%v22,%v7,%v11,%v22,2
	.word	0xe777,0xd200,0x79ac	# vmale	%v23,%v7,%v13,%v23,2
	.word	0xe738,0xe200,0x39ac	# vmale	%v19,%v8,%v14,%v19,2
	.word	0xe748,0x0200,0x4bac	# vmale	%v20,%v8,%v16,%v20,2
	.word	0xe758,0x2200,0x5bac	# vmale	%v21,%v8,%v18,%v21,2
	.word	0xe768,0xa200,0x69ac	# vmale	%v22,%v8,%v10,%v22,2
	.word	0xe778,0xb200,0x79ac	# vmale	%v23,%v8,%v11,%v23,2
	.word	0xe739,0xc200,0x39ac	# vmale	%v19,%v9,%v12,%v19,2
	.word	0xe749,0xe200,0x49ac	# vmale	%v20,%v9,%v14,%v20,2
	.word	0xe759,0x0200,0x5bac	# vmale	%v21,%v9,%v16,%v21,2
	.word	0xe769,0x2200,0x6bac	# vmale	%v22,%v9,%v18,%v22,2
	.word	0xe779,0xa200,0x79ac	# vmale	%v23,%v9,%v10,%v23,2
	.word	0xe730,0xa200,0x39ad	# vmalo	%v19,%v0,%v10,%v19,2
	.word	0xe740,0xb200,0x49ad	# vmalo	%v20,%v0,%v11,%v20,2
	.word	0xe750,0xd200,0x59ad	# vmalo	%v21,%v0,%v13,%v21,2
	.word	0xe760,0xf200,0x69ad	# vmalo	%v22,%v0,%v15,%v22,2
	.word	0xe770,0x1200,0x7bad	# vmalo	%v23,%v0,%v17,%v23,2
	.word	0xe731,0x2200,0x3bad	# vmalo	%v19,%v1,%v18,%v19,2
	.word	0xe741,0xa200,0x49ad	# vmalo	%v20,%v1,%v10,%v20,2
	.word	0xe751,0xb200,0x59ad	# vmalo	%v21,%v1,%v11,%v21,2
	.word	0xe761,0xd200,0x69ad	# vmalo	%v22,%v1,%v13,%v22,2
	.word	0xe771,0xf200,0x79ad	# vmalo	%v23,%v1,%v15,%v23,2
	.word	0xe732,0x0200,0x3bad	# vmalo	%v19,%v2,%v16,%v19,2
	.word	0xe742,0x2200,0x4bad	# vmalo	%v20,%v2,%v18,%v20,2
	.word	0xe752,0xa200,0x59ad	# vmalo	%v21,%v2,%v10,%v21,2
	.word	0xe762,0xb200,0x69ad	# vmalo	%v22,%v2,%v11,%v22,2
	.word	0xe772,0xd200,0x79ad	# vmalo	%v23,%v2,%v13,%v23,2
	.word	0xe733,0xe200,0x39ad	# vmalo	%v19,%v3,%v14,%v19,2
	.word	0xe743,0x0200,0x4bad	# vmalo	%v20,%v3,%v16,%v20,2
	.word	0xe753,0x2200,0x5bad	# vmalo	%v21,%v3,%v18,%v21,2
	.word	0xe763,0xa200,0x69ad	# vmalo	%v22,%v3,%v10,%v22,2
	.word	0xe773,0xb200,0x79ad	# vmalo	%v23,%v3,%v11,%v23,2
	.word	0xe734,0xc200,0x39ad	# vmalo	%v19,%v4,%v12,%v19,2
	.word	0xe744,0xe200,0x49ad	# vmalo	%v20,%v4,%v14,%v20,2
	.word	0xe754,0x0200,0x5bad	# vmalo	%v21,%v4,%v16,%v21,2
	.word	0xe764,0x2200,0x6bad	# vmalo	%v22,%v4,%v18,%v22,2
	.word	0xe774,0xa200,0x79ad	# vmalo	%v23,%v4,%v10,%v23,2
	.word	0xe700,000000,0x0044	# vgbm	%v0,0
	.word	0xe733,000000,0x3c67	# vsumq	%v19,%v19,%v0,3
	.word	0xe744,000000,0x3c67	# vsumq	%v20,%v20,%v0,3
	.word	0xe755,000000,0x3c67	# vsumq	%v21,%v21,%v0,3
	.word	0xe766,000000,0x3c67	# vsumq	%v22,%v22,%v0,3
	.word	0xe777,000000,0x3c67	# vsumq	%v23,%v23,%v0,3
	.word	0xe746,0x001a,0x3438	# vesrl	%v4,%v22,26,3
	.word	0xe713,0x001a,0x3438	# vesrl	%v1,%v19,26,3
	.word	0xe736,0xc000,0x0668	# vn	%v3,%v22,%v28
	.word	0xe703,0xc000,0x0668	# vn	%v0,%v19,%v28
	.word	0xe744,0x7000,0x32f3	# va	%v4,%v4,%v23,3
	.word	0xe711,0x4000,0x32f3	# va	%v1,%v1,%v20,3
	.word	0xe774,0x001a,0x3838	# vesrl	%v23,%v4,26,3
	.word	0xe741,0x001a,0x3838	# vesrl	%v20,%v1,26,3
	.word	0xe744,0xc000,0x0268	# vn	%v4,%v4,%v28
	.word	0xe711,0xc000,0x0268	# vn	%v1,%v1,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe725,0x4000,0x36f3	# va	%v2,%v21,%v20,3
	.word	0xe777,0x0002,0x3c30	# vesl	%v23,%v23,2,3
	.word	0xe752,0x001a,0x3838	# vesrl	%v21,%v2,26,3
	.word	0xe722,0xc000,0x0268	# vn	%v2,%v2,%v28
	.word	0xe700,0x7000,0x32f3	# va	%v0,%v0,%v23,3
	.word	0xe733,0x5000,0x32f3	# va	%v3,%v3,%v21,3
	.word	0xe730,0x001a,0x3838	# vesrl	%v19,%v0,26,3
	.word	0xe763,0x001a,0x3838	# vesrl	%v22,%v3,26,3
	.word	0xe700,0xc000,0x0268	# vn	%v0,%v0,%v28
	.word	0xe733,0xc000,0x0268	# vn	%v3,%v3,%v28
	.word	0xe711,0x3000,0x32f3	# va	%v1,%v1,%v19,3
	.word	0xe744,0x6000,0x32f3	# va	%v4,%v4,%v22,3
	.word	0xc24f,000000,000000	# clfi	%r4,0
	je	.Ldone
	.word	0xe78b,0x3000,0x0c36	# vlm	%v24,%v27,0x00(%r3)
	.word	0xe7c0,0x061f,0x3846	# vgm	%v28,6,31,3
	.word	0xe790,0x0505,0x2046	# vgm	%v9,5,5,2
	.word	0xe75a,0xb000,0xd78c	# vperm	%v5,%v26,%v27,%v29
	.word	0xe77a,0xb000,0xf78c	# vperm	%v7,%v26,%v27,%v31
	.word	0xe7aa,0xb000,0xef8c	# vperm	%v26,%v26,%v27,%v30
	.word	0xe734,0x1030,0x0806	# vl	%v19,0x30(%r4,%r1)
	.word	0xe744,0x1060,0x0806	# vl	%v20,0x60(%r4,%r1)
	.word	0xe765,0xc006,0x3272	# verim	%v6,%v5,%v28,6,3
	.word	0xe755,0x0020,0x3030	# vesl	%v5,%v5,32,3
	.word	0xe777,0x001c,0x3030	# vesl	%v7,%v7,28,3
	.word	0xe78a,0xc012,0x3672	# verim	%v8,%v26,%v28,18,3
	.word	0xe79a,0xc03a,0x3672	# verim	%v9,%v26,%v28,58,3
	.word	0xe755,0xc000,0x0268	# vn	%v5,%v5,%v28
	.word	0xe777,0xc000,0x0268	# vn	%v7,%v7,%v28
	.word	0xe799,0x0002,0x2038	# vesrl	%v9,%v9,2,2
	.word	0xe7c0,0x263f,0x3846	# vgm	%v28,38,63,3
	.word	0xe7a8,0x9000,0xdf8c	# vperm	%v26,%v24,%v25,%v29
	.word	0xe7b8,0x9000,0xef8c	# vperm	%v27,%v24,%v25,%v30
	.word	0xe798,0x9000,0xff8c	# vperm	%v25,%v24,%v25,%v31
	.word	0xe75a,0xc000,0x3672	# verim	%v5,%v26,%v28,0,3
	.word	0xe76a,0xc026,0x3672	# verim	%v6,%v26,%v28,38,3
	.word	0xe779,0xc03c,0x3672	# verim	%v7,%v25,%v28,60,3
	.word	0xe78b,0xc032,0x3672	# verim	%v8,%v27,%v28,50,3
	.word	0xe7bb,0x0028,0x3c38	# vesrl	%v27,%v27,40,3
	.word	0xe799,0xb000,0x026a	# vo	%v9,%v9,%v27
	.word	0xe700,000000,0x318c	# vperm	%v0,%v0,%v0,%v19
	.word	0xe755,0x4000,0x0268	# vn	%v5,%v5,%v20
	.word	0xe711,0x1000,0x318c	# vperm	%v1,%v1,%v1,%v19
	.word	0xe766,0x4000,0x0268	# vn	%v6,%v6,%v20
	.word	0xe722,0x2000,0x318c	# vperm	%v2,%v2,%v2,%v19
	.word	0xe777,0x4000,0x0268	# vn	%v7,%v7,%v20
	.word	0xe733,0x3000,0x318c	# vperm	%v3,%v3,%v3,%v19
	.word	0xe788,0x4000,0x0268	# vn	%v8,%v8,%v20
	.word	0xe744,0x4000,0x318c	# vperm	%v4,%v4,%v4,%v19
	.word	0xe799,0x4000,0x0268	# vn	%v9,%v9,%v20
	.word	0xe755,000000,0x20f3	# va	%v5,%v5,%v0,2
	.word	0xe700,000000,0x0044	# vgbm	%v0,0
	.word	0xe766,0x1000,0x20f3	# va	%v6,%v6,%v1,2
	.word	0xe710,000000,0x0044	# vgbm	%v1,0
	.word	0xe777,0x2000,0x20f3	# va	%v7,%v7,%v2,2
	.word	0xe720,000000,0x0044	# vgbm	%v2,0
	.word	0xe788,0x3000,0x20f3	# va	%v8,%v8,%v3,2
	.word	0xe730,000000,0x0044	# vgbm	%v3,0
	.word	0xe799,0x4000,0x20f3	# va	%v9,%v9,%v4,2
	.word	0xe740,000000,0x0044	# vgbm	%v4,0
	lhi	%r4,0
	j	.Last
.Ldone:
	.word	0xe700,0x2000,0x300b	# vstef	%v0,0(%r2),3
	.word	0xe710,0x2004,0x300b	# vstef	%v1,4(%r2),3
	.word	0xe720,0x2008,0x300b	# vstef	%v2,8(%r2),3
	.word	0xe730,0x200c,0x300b	# vstef	%v3,12(%r2),3
	.word	0xe740,0x2010,0x300b	# vstef	%v4,16(%r2),3
	ld	%f4,96+16*4+2*8(%r15)
	ld	%f6,96+16*4+3*8(%r15)
	lm	%r10,%r15,96+10*4(%r15)
	br	%r14
.size	__poly1305_blocks_vx,.-__poly1305_blocks_vx
.globl	poly1305_emit
.type	poly1305_emit,@function
.align	16
poly1305_emit:
.Lpoly1305_emit:
	stm	%r6,%r10,6*4(%r15)
	lg	%r8,0(%r2)
	lg	%r9,8(%r2)
	lg	%r10,16(%r2)
	llgfr	%r0,%r8
	srlg	%r5,%r8,32
	llgfr	%r1,%r9
	srlg	%r6,%r9,32
	srlg	%r7,%r10,32
	sllg	%r0,%r0,26
	algr	%r5,%r0
	sllg	%r0,%r6,52
	srlg	%r6,%r6,12
	sllg	%r1,%r1,14
	algr	%r5,%r0
	alcgr	%r6,%r1
	sllg	%r0,%r7,40
	srlg	%r7,%r7,24
	lghi	%r1,0
	algr	%r6,%r0
	alcgr	%r7,%r1
	llgf	%r0,24(%r2)
	lcgr	%r0,%r0
	xgr	%r5,%r8
	xgr	%r6,%r9
	xgr	%r7,%r10
	ngr	%r5,%r0
	ngr	%r6,%r0
	ngr	%r7,%r0
	xgr	%r5,%r8
	xgr	%r6,%r9
	xgr	%r7,%r10
	lghi	%r0,5
	lgr	%r8,%r5
	lgr	%r9,%r6
	algr	%r5,%r0
	alcgr	%r6,%r1
	alcgr	%r7,%r1
	srlg	%r7,%r7,2
	slgr	%r1,%r7
	lg	%r10,0(%r4)
	lg	%r2,8(%r4)
	xgr	%r5,%r8
	xgr	%r6,%r9
	ngr	%r5,%r1
	ngr	%r6,%r1
	xgr	%r5,%r8
	rllg	%r8,%r10,32
	xgr	%r6,%r9
	rllg	%r9,%r2,32
	algr	%r5,%r8
	alcgr	%r6,%r9
	strvg	%r5,0(%r3)
	strvg	%r6,8(%r3)
	lm	%r6,%r10,6*4(%r15)
	br	%r14
.size	poly1305_emit,.-poly1305_emit
.align	16
.Lconst:
.long	67438087,336926231,202182159,471670303
.long	117835012,50462976,387323156,319951120
.long	252579084,185207048,522067228,454695192
.long	0,151521030,0,421009174
.long	0,0,0,202182159
.long	202182159,0,0,0
.long	0,0,202182159,0
.long	4294967295,0,4294967295,4294967295
.long	4294967295,0,4294967295,0
.long	0,0,4294967295,0
.string	"Poly1305 for s390x, CRYPTOGAMS by <appro@openssl.org>"
