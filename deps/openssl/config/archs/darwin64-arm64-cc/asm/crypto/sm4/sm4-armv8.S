// Copyright 2022-2025 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

//
// This module implements support for SM4 hw support on aarch64
// Oct 2021
//

// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arm_arch.h"

.text

.section	__TEXT,__const

.align	6
_sm4_v8_consts:
Lck:
.long	0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269
.long	0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9
.long	0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249
.long	0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9
.long	0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229
.long	0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299
.long	0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209
.long	0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
Lfk:
.long	0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc

.previous

.globl	_sm4_v8_set_encrypt_key

.align	5
_sm4_v8_set_encrypt_key:
	AARCH64_VALID_CALL_TARGET
	ld1	{v0.4s},[x0]
	adrp	x2,Lfk@PAGE
	add	x2,x2,Lfk@PAGEOFF
	ld1	{v24.4s},[x2]
	adrp	x2,Lck@PAGE
	add	x2,x2,Lck@PAGEOFF
	ld1	{v16.4s,v17.4s,v18.4s,v19.4s},[x2],64
#ifndef __AARCH64EB__
	rev32	v0.16b,v0.16b
#endif
	ld1	{v20.4s,v21.4s,v22.4s,v23.4s},[x2]
	eor	v0.16b,v0.16b,v24.16b;
.long	0xce70c800	//sm4ekey v0.4S,v0.4S,v16.4S
.long	0xce71c801	//sm4ekey v1.4S,v0.4S,v17.4S
.long	0xce72c822	//sm4ekey v2.4S,v1.4S,v18.4S
.long	0xce73c843	//sm4ekey v3.4S,v2.4S,v19.4S
.long	0xce74c864	//sm4ekey v4.4S,v3.4S,v20.4S
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],64
.long	0xce75c885	//sm4ekey v5.4S,v4.4S,v21.4S
.long	0xce76c8a6	//sm4ekey v6.4S,v5.4S,v22.4S
.long	0xce77c8c7	//sm4ekey v7.4S,v6.4S,v23.4S
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1]
	ret

.globl	_sm4_v8_set_decrypt_key

.align	5
_sm4_v8_set_decrypt_key:
	AARCH64_VALID_CALL_TARGET
	ld1	{v7.4s},[x0]
	adrp	x2,Lfk@PAGE
	add	x2,x2,Lfk@PAGEOFF
	ld1	{v24.4s},[x2]
	adrp	x2,Lck@PAGE
	add	x2,x2,Lck@PAGEOFF
	ld1	{v16.4s,v17.4s,v18.4s,v19.4s},[x2],64
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	ld1	{v20.4s,v21.4s,v22.4s,v23.4s},[x2]
	eor	v7.16b, v7.16b,v24.16b;
.long	0xce70c8e7	//sm4ekey v7.4S,v7.4S,v16.4S
.long	0xce71c8e6	//sm4ekey v6.4S,v7.4S,v17.4S
.long	0xce72c8c5	//sm4ekey v5.4S,v6.4S,v18.4S
	rev64	v7.4s,v7.4s
	rev64	v6.4s,v6.4s
	ext	v7.16b,v7.16b,v7.16b,#8
	ext	v6.16b,v6.16b,v6.16b,#8
.long	0xce73c8a4	//sm4ekey v4.4S,v5.4S,v19.4S
.long	0xce74c883	//sm4ekey v3.4S,v4.4S,v20.4S
	rev64	v5.4s,v5.4s
	rev64	v4.4s,v4.4s
	ext	v5.16b,v5.16b,v5.16b,#8
	ext	v4.16b,v4.16b,v4.16b,#8
.long	0xce75c862	//sm4ekey v2.4S,v3.4S,v21.4S
.long	0xce76c841	//sm4ekey v1.4S,v2.4S,v22.4S
	rev64	v3.4s,v3.4s
	rev64	v2.4s,v2.4s
	ext	v3.16b,v3.16b,v3.16b,#8
	ext	v2.16b,v2.16b,v2.16b,#8
.long	0xce77c820	//sm4ekey v0.4S,v1.4S,v23.4S
	rev64	v1.4s, v1.4s
	rev64	v0.4s, v0.4s
	ext	v1.16b,v1.16b,v1.16b,#8
	ext	v0.16b,v0.16b,v0.16b,#8
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1]
	ret

.globl	_sm4_v8_encrypt

.align	5
_sm4_v8_encrypt:
	AARCH64_VALID_CALL_TARGET
	ld1	{v16.4s},[x0]
	ld1	{v0.4s,v1.4s,v2.4s,v3.4s},[x2],64
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x2]
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	st1	{v16.4s},[x1]
	ret

.globl	_sm4_v8_decrypt

.align	5
_sm4_v8_decrypt:
	AARCH64_VALID_CALL_TARGET
	ld1	{v16.4s},[x0]
	ld1	{v0.4s,v1.4s,v2.4s,v3.4s},[x2],64
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x2]
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	st1	{v16.4s},[x1]
	ret

.globl	_sm4_v8_ecb_encrypt

.align	5
_sm4_v8_ecb_encrypt:
	AARCH64_VALID_CALL_TARGET
	ld1	{v0.4s,v1.4s,v2.4s,v3.4s},[x3],#64
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x3]
1:
	cmp	x2,#64
	b.lt	1f
	ld1	{v16.4s,v17.4s,v18.4s,v19.4s},[x0],#64
	cmp	x2,#128
	b.lt	2f
	ld1	{v20.4s,v21.4s,v22.4s,v23.4s},[x0],#64
	// 8 blocks
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
#ifndef __AARCH64EB__
	rev32	v20.16b,v20.16b
#endif
#ifndef __AARCH64EB__
	rev32	v21.16b,v21.16b
#endif
#ifndef __AARCH64EB__
	rev32	v22.16b,v22.16b
#endif
#ifndef __AARCH64EB__
	rev32	v23.16b,v23.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
.long	0xcec08414	//sm4e v20.4s,v0.4s
.long	0xcec08415	//sm4e v21.4s,v0.4s
.long	0xcec08416	//sm4e v22.4s,v0.4s
.long	0xcec08417	//sm4e v23.4s,v0.4s

.long	0xcec08434	//sm4e v20.4s,v1.4s
.long	0xcec08435	//sm4e v21.4s,v1.4s
.long	0xcec08436	//sm4e v22.4s,v1.4s
.long	0xcec08437	//sm4e v23.4s,v1.4s

.long	0xcec08454	//sm4e v20.4s,v2.4s
.long	0xcec08455	//sm4e v21.4s,v2.4s
.long	0xcec08456	//sm4e v22.4s,v2.4s
.long	0xcec08457	//sm4e v23.4s,v2.4s

.long	0xcec08474	//sm4e v20.4s,v3.4s
.long	0xcec08475	//sm4e v21.4s,v3.4s
.long	0xcec08476	//sm4e v22.4s,v3.4s
.long	0xcec08477	//sm4e v23.4s,v3.4s

.long	0xcec08494	//sm4e v20.4s,v4.4s
.long	0xcec08495	//sm4e v21.4s,v4.4s
.long	0xcec08496	//sm4e v22.4s,v4.4s
.long	0xcec08497	//sm4e v23.4s,v4.4s

.long	0xcec084b4	//sm4e v20.4s,v5.4s
.long	0xcec084b5	//sm4e v21.4s,v5.4s
.long	0xcec084b6	//sm4e v22.4s,v5.4s
.long	0xcec084b7	//sm4e v23.4s,v5.4s

.long	0xcec084d4	//sm4e v20.4s,v6.4s
.long	0xcec084d5	//sm4e v21.4s,v6.4s
.long	0xcec084d6	//sm4e v22.4s,v6.4s
.long	0xcec084d7	//sm4e v23.4s,v6.4s

.long	0xcec084f4	//sm4e v20.4s,v7.4s
	rev64	v20.4S,v20.4S
.long	0xcec084f5	//sm4e v21.4s,v7.4s
	ext	v20.16b,v20.16b,v20.16b,#8
	rev64	v21.4S,v21.4S
.long	0xcec084f6	//sm4e v22.4s,v7.4s
	ext	v21.16b,v21.16b,v21.16b,#8
	rev64	v22.4S,v22.4S
.long	0xcec084f7	//sm4e v23.4s,v7.4s
	ext	v22.16b,v22.16b,v22.16b,#8
	rev64	v23.4S,v23.4S
	ext	v23.16b,v23.16b,v23.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
#ifndef __AARCH64EB__
	rev32	v20.16b,v20.16b
#endif
#ifndef __AARCH64EB__
	rev32	v21.16b,v21.16b
#endif
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
#ifndef __AARCH64EB__
	rev32	v22.16b,v22.16b
#endif
#ifndef __AARCH64EB__
	rev32	v23.16b,v23.16b
#endif
	st1	{v20.4s,v21.4s,v22.4s,v23.4s},[x1],#64
	subs	x2,x2,#128
	b.gt	1b
	ret
	// 4 blocks
2:
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	subs	x2,x2,#64
	b.gt	1b
1:
	subs	x2,x2,#16
	b.lt	1f
	ld1	{v16.4s},[x0],#16
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	st1	{v16.4s},[x1],#16
	b.ne	1b
1:
	ret

.globl	_sm4_v8_cbc_encrypt

.align	5
_sm4_v8_cbc_encrypt:
	AARCH64_VALID_CALL_TARGET
	stp	d8,d9,[sp, #-16]!

	ld1	{v0.4s,v1.4s,v2.4s,v3.4s},[x3],#64
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x3]
	ld1	{v8.4s},[x4]
	cmp	w5,#0
	b.eq	Ldec
1:
	cmp	x2, #64
	b.lt	1f
	ld1	{v16.4s,v17.4s,v18.4s,v19.4s},[x0],#64
	eor	v16.16b,v16.16b,v8.16b
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
	eor	v17.16b,v17.16b,v16.16b
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	rev64	v17.4S,v17.4S
	ext	v17.16b,v17.16b,v17.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	eor	v18.16b,v18.16b,v17.16b
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	rev64	v18.4S,v18.4S
	ext	v18.16b,v18.16b,v18.16b,#8
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
	eor	v19.16b,v19.16b,v18.16b
.long	0xcec08413	//sm4e v19.4s,v0.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
	mov	v8.16b,v19.16b
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	subs	x2,x2,#64
	b.ne	1b
1:
	subs	x2,x2,#16
	b.lt	3f
	ld1	{v16.4s},[x0],#16
	eor	v8.16b,v8.16b,v16.16b
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
.long	0xcec08408	//sm4e v8.4s,v0.4s
.long	0xcec08428	//sm4e v8.4s,v1.4s
.long	0xcec08448	//sm4e v8.4s,v2.4s
.long	0xcec08468	//sm4e v8.4s,v3.4s
.long	0xcec08488	//sm4e v8.4s,v4.4s
.long	0xcec084a8	//sm4e v8.4s,v5.4s
.long	0xcec084c8	//sm4e v8.4s,v6.4s
.long	0xcec084e8	//sm4e v8.4s,v7.4s
	rev64	v8.4S,v8.4S
	ext	v8.16b,v8.16b,v8.16b,#8
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	st1	{v8.4s},[x1],#16
	b.ne	1b
	b	3f
Ldec:
1:
	cmp	x2, #64
	b.lt	1f
	ld1	{v16.4s,v17.4s,v18.4s,v19.4s},[x0]
	ld1	{v24.4s,v25.4s,v26.4s,v27.4s},[x0],#64
	cmp	x2,#128
	b.lt	2f
	// 8 blocks mode
	ld1	{v20.4s,v21.4s,v22.4s,v23.4s},[x0]
	ld1	{v28.4s,v29.4s,v30.4s,v31.4s},[x0],#64
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
#ifndef __AARCH64EB__
	rev32	v20.16b,v20.16b
#endif
#ifndef __AARCH64EB__
	rev32	v21.16b,v21.16b
#endif
#ifndef __AARCH64EB__
	rev32	v22.16b,v22.16b
#endif
#ifndef __AARCH64EB__
	rev32	v23.16b,v23.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
.long	0xcec08414	//sm4e v20.4s,v0.4s
.long	0xcec08415	//sm4e v21.4s,v0.4s
.long	0xcec08416	//sm4e v22.4s,v0.4s
.long	0xcec08417	//sm4e v23.4s,v0.4s

.long	0xcec08434	//sm4e v20.4s,v1.4s
.long	0xcec08435	//sm4e v21.4s,v1.4s
.long	0xcec08436	//sm4e v22.4s,v1.4s
.long	0xcec08437	//sm4e v23.4s,v1.4s

.long	0xcec08454	//sm4e v20.4s,v2.4s
.long	0xcec08455	//sm4e v21.4s,v2.4s
.long	0xcec08456	//sm4e v22.4s,v2.4s
.long	0xcec08457	//sm4e v23.4s,v2.4s

.long	0xcec08474	//sm4e v20.4s,v3.4s
.long	0xcec08475	//sm4e v21.4s,v3.4s
.long	0xcec08476	//sm4e v22.4s,v3.4s
.long	0xcec08477	//sm4e v23.4s,v3.4s

.long	0xcec08494	//sm4e v20.4s,v4.4s
.long	0xcec08495	//sm4e v21.4s,v4.4s
.long	0xcec08496	//sm4e v22.4s,v4.4s
.long	0xcec08497	//sm4e v23.4s,v4.4s

.long	0xcec084b4	//sm4e v20.4s,v5.4s
.long	0xcec084b5	//sm4e v21.4s,v5.4s
.long	0xcec084b6	//sm4e v22.4s,v5.4s
.long	0xcec084b7	//sm4e v23.4s,v5.4s

.long	0xcec084d4	//sm4e v20.4s,v6.4s
.long	0xcec084d5	//sm4e v21.4s,v6.4s
.long	0xcec084d6	//sm4e v22.4s,v6.4s
.long	0xcec084d7	//sm4e v23.4s,v6.4s

.long	0xcec084f4	//sm4e v20.4s,v7.4s
	rev64	v20.4S,v20.4S
.long	0xcec084f5	//sm4e v21.4s,v7.4s
	ext	v20.16b,v20.16b,v20.16b,#8
	rev64	v21.4S,v21.4S
.long	0xcec084f6	//sm4e v22.4s,v7.4s
	ext	v21.16b,v21.16b,v21.16b,#8
	rev64	v22.4S,v22.4S
.long	0xcec084f7	//sm4e v23.4s,v7.4s
	ext	v22.16b,v22.16b,v22.16b,#8
	rev64	v23.4S,v23.4S
	ext	v23.16b,v23.16b,v23.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
#ifndef __AARCH64EB__
	rev32	v20.16b,v20.16b
#endif
#ifndef __AARCH64EB__
	rev32	v21.16b,v21.16b
#endif
#ifndef __AARCH64EB__
	rev32	v22.16b,v22.16b
#endif
#ifndef __AARCH64EB__
	rev32	v23.16b,v23.16b
#endif
	eor	v16.16b,v16.16b,v8.16b
	eor	v17.16b,v17.16b,v24.16b
	eor	v18.16b,v18.16b,v25.16b
	mov	v8.16b,v31.16b
	eor	v19.16b,v19.16b,v26.16b
	eor	v20.16b,v20.16b,v27.16b
	eor	v21.16b,v21.16b,v28.16b
	eor	v22.16b,v22.16b,v29.16b
	eor	v23.16b,v23.16b,v30.16b
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	st1	{v20.4s,v21.4s,v22.4s,v23.4s},[x1],#64
	subs	x2,x2,128
	b.gt	1b
	b	3f
	// 4 blocks mode
2:
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
	eor	v16.16b,v16.16b,v8.16b
	eor	v17.16b,v17.16b,v24.16b
	mov	v8.16b,v27.16b
	eor	v18.16b,v18.16b,v25.16b
	eor	v19.16b,v19.16b,v26.16b
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	subs	x2,x2,#64
	b.gt	1b
1:
	subs	x2,x2,#16
	b.lt	3f
	ld1	{v16.4s},[x0],#16
	mov	v24.16b,v16.16b
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	eor	v16.16b,v16.16b,v8.16b
	mov	v8.16b,v24.16b
	st1	{v16.4s},[x1],#16
	b.ne	1b
3:
	// save back IV
	st1	{v8.4s},[x4]
	ldp	d8,d9,[sp],#16
	ret

.globl	_sm4_v8_ctr32_encrypt_blocks

.align	5
_sm4_v8_ctr32_encrypt_blocks:
	AARCH64_VALID_CALL_TARGET
	stp	d8,d9,[sp, #-16]!

	ld1	{v8.4s},[x4]
	ld1	{v0.4s,v1.4s,v2.4s,v3.4s},[x3],64
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x3]
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	w5,v8.s[3]
1:
	cmp	x2,#4
	b.lt	1f
	ld1	{v24.4s,v25.4s,v26.4s,v27.4s},[x0],#64
	mov	v16.16b,v8.16b
	mov	v17.16b,v8.16b
	mov	v18.16b,v8.16b
	mov	v19.16b,v8.16b
	add	w5,w5,#1
	mov	v17.s[3],w5
	add	w5,w5,#1
	mov	v18.s[3],w5
	add	w5,w5,#1
	mov	v19.s[3],w5
	cmp	x2,#8
	b.lt	2f
	ld1	{v28.4s,v29.4s,v30.4s,v31.4s},[x0],#64
	mov	v20.16b,v8.16b
	mov	v21.16b,v8.16b
	mov	v22.16b,v8.16b
	mov	v23.16b,v8.16b
	add	w5,w5,#1
	mov	v20.s[3],w5
	add	w5,w5,#1
	mov	v21.s[3],w5
	add	w5,w5,#1
	mov	v22.s[3],w5
	add	w5,w5,#1
	mov	v23.s[3],w5
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
.long	0xcec08414	//sm4e v20.4s,v0.4s
.long	0xcec08415	//sm4e v21.4s,v0.4s
.long	0xcec08416	//sm4e v22.4s,v0.4s
.long	0xcec08417	//sm4e v23.4s,v0.4s

.long	0xcec08434	//sm4e v20.4s,v1.4s
.long	0xcec08435	//sm4e v21.4s,v1.4s
.long	0xcec08436	//sm4e v22.4s,v1.4s
.long	0xcec08437	//sm4e v23.4s,v1.4s

.long	0xcec08454	//sm4e v20.4s,v2.4s
.long	0xcec08455	//sm4e v21.4s,v2.4s
.long	0xcec08456	//sm4e v22.4s,v2.4s
.long	0xcec08457	//sm4e v23.4s,v2.4s

.long	0xcec08474	//sm4e v20.4s,v3.4s
.long	0xcec08475	//sm4e v21.4s,v3.4s
.long	0xcec08476	//sm4e v22.4s,v3.4s
.long	0xcec08477	//sm4e v23.4s,v3.4s

.long	0xcec08494	//sm4e v20.4s,v4.4s
.long	0xcec08495	//sm4e v21.4s,v4.4s
.long	0xcec08496	//sm4e v22.4s,v4.4s
.long	0xcec08497	//sm4e v23.4s,v4.4s

.long	0xcec084b4	//sm4e v20.4s,v5.4s
.long	0xcec084b5	//sm4e v21.4s,v5.4s
.long	0xcec084b6	//sm4e v22.4s,v5.4s
.long	0xcec084b7	//sm4e v23.4s,v5.4s

.long	0xcec084d4	//sm4e v20.4s,v6.4s
.long	0xcec084d5	//sm4e v21.4s,v6.4s
.long	0xcec084d6	//sm4e v22.4s,v6.4s
.long	0xcec084d7	//sm4e v23.4s,v6.4s

.long	0xcec084f4	//sm4e v20.4s,v7.4s
	rev64	v20.4S,v20.4S
.long	0xcec084f5	//sm4e v21.4s,v7.4s
	ext	v20.16b,v20.16b,v20.16b,#8
	rev64	v21.4S,v21.4S
.long	0xcec084f6	//sm4e v22.4s,v7.4s
	ext	v21.16b,v21.16b,v21.16b,#8
	rev64	v22.4S,v22.4S
.long	0xcec084f7	//sm4e v23.4s,v7.4s
	ext	v22.16b,v22.16b,v22.16b,#8
	rev64	v23.4S,v23.4S
	ext	v23.16b,v23.16b,v23.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
#ifndef __AARCH64EB__
	rev32	v20.16b,v20.16b
#endif
#ifndef __AARCH64EB__
	rev32	v21.16b,v21.16b
#endif
#ifndef __AARCH64EB__
	rev32	v22.16b,v22.16b
#endif
#ifndef __AARCH64EB__
	rev32	v23.16b,v23.16b
#endif
	eor	v16.16b,v16.16b,v24.16b
	eor	v17.16b,v17.16b,v25.16b
	eor	v18.16b,v18.16b,v26.16b
	eor	v19.16b,v19.16b,v27.16b
	eor	v20.16b,v20.16b,v28.16b
	eor	v21.16b,v21.16b,v29.16b
	eor	v22.16b,v22.16b,v30.16b
	eor	v23.16b,v23.16b,v31.16b
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	st1	{v20.4s,v21.4s,v22.4s,v23.4s},[x1],#64
	subs	x2,x2,#8
	b.eq	3f
	add	w5,w5,#1
	mov	v8.s[3],w5
	b	1b
2:
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08411	//sm4e v17.4s,v0.4s
.long	0xcec08412	//sm4e v18.4s,v0.4s
.long	0xcec08413	//sm4e v19.4s,v0.4s

.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08431	//sm4e v17.4s,v1.4s
.long	0xcec08432	//sm4e v18.4s,v1.4s
.long	0xcec08433	//sm4e v19.4s,v1.4s

.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08451	//sm4e v17.4s,v2.4s
.long	0xcec08452	//sm4e v18.4s,v2.4s
.long	0xcec08453	//sm4e v19.4s,v2.4s

.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08471	//sm4e v17.4s,v3.4s
.long	0xcec08472	//sm4e v18.4s,v3.4s
.long	0xcec08473	//sm4e v19.4s,v3.4s

.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec08491	//sm4e v17.4s,v4.4s
.long	0xcec08492	//sm4e v18.4s,v4.4s
.long	0xcec08493	//sm4e v19.4s,v4.4s

.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084b1	//sm4e v17.4s,v5.4s
.long	0xcec084b2	//sm4e v18.4s,v5.4s
.long	0xcec084b3	//sm4e v19.4s,v5.4s

.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084d1	//sm4e v17.4s,v6.4s
.long	0xcec084d2	//sm4e v18.4s,v6.4s
.long	0xcec084d3	//sm4e v19.4s,v6.4s

.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
.long	0xcec084f1	//sm4e v17.4s,v7.4s
	ext	v16.16b,v16.16b,v16.16b,#8
	rev64	v17.4S,v17.4S
.long	0xcec084f2	//sm4e v18.4s,v7.4s
	ext	v17.16b,v17.16b,v17.16b,#8
	rev64	v18.4S,v18.4S
.long	0xcec084f3	//sm4e v19.4s,v7.4s
	ext	v18.16b,v18.16b,v18.16b,#8
	rev64	v19.4S,v19.4S
	ext	v19.16b,v19.16b,v19.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
#ifndef __AARCH64EB__
	rev32	v17.16b,v17.16b
#endif
#ifndef __AARCH64EB__
	rev32	v18.16b,v18.16b
#endif
#ifndef __AARCH64EB__
	rev32	v19.16b,v19.16b
#endif
	eor	v16.16b,v16.16b,v24.16b
	eor	v17.16b,v17.16b,v25.16b
	eor	v18.16b,v18.16b,v26.16b
	eor	v19.16b,v19.16b,v27.16b
	st1	{v16.4s,v17.4s,v18.4s,v19.4s},[x1],#64
	subs	x2,x2,#4
	b.eq	3f
	add	w5,w5,#1
	mov	v8.s[3],w5
	b	1b
1:
	subs	x2,x2,#1
	b.lt	3f
	mov	v16.16b,v8.16b
	ld1	{v24.4s},[x0],#16
.long	0xcec08410	//sm4e v16.4s,v0.4s
.long	0xcec08430	//sm4e v16.4s,v1.4s
.long	0xcec08450	//sm4e v16.4s,v2.4s
.long	0xcec08470	//sm4e v16.4s,v3.4s
.long	0xcec08490	//sm4e v16.4s,v4.4s
.long	0xcec084b0	//sm4e v16.4s,v5.4s
.long	0xcec084d0	//sm4e v16.4s,v6.4s
.long	0xcec084f0	//sm4e v16.4s,v7.4s
	rev64	v16.4S,v16.4S
	ext	v16.16b,v16.16b,v16.16b,#8
#ifndef __AARCH64EB__
	rev32	v16.16b,v16.16b
#endif
	eor	v16.16b,v16.16b,v24.16b
	st1	{v16.4s},[x1],#16
	b.eq	3f
	add	w5,w5,#1
	mov	v8.s[3],w5
	b	1b
3:
	ldp	d8,d9,[sp],#16
	ret

