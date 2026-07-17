// Copyright 2020-2025 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

//
// This module implements SM4 with ASIMD on aarch64
//
// Feb 2022
//

// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arm_arch.h"
.arch	armv8-a
.text

.section	.rodata
.type	_vpsm4_consts,%object
.align	7
_vpsm4_consts:
.Lsbox:
.byte	0xD6,0x90,0xE9,0xFE,0xCC,0xE1,0x3D,0xB7,0x16,0xB6,0x14,0xC2,0x28,0xFB,0x2C,0x05
.byte	0x2B,0x67,0x9A,0x76,0x2A,0xBE,0x04,0xC3,0xAA,0x44,0x13,0x26,0x49,0x86,0x06,0x99
.byte	0x9C,0x42,0x50,0xF4,0x91,0xEF,0x98,0x7A,0x33,0x54,0x0B,0x43,0xED,0xCF,0xAC,0x62
.byte	0xE4,0xB3,0x1C,0xA9,0xC9,0x08,0xE8,0x95,0x80,0xDF,0x94,0xFA,0x75,0x8F,0x3F,0xA6
.byte	0x47,0x07,0xA7,0xFC,0xF3,0x73,0x17,0xBA,0x83,0x59,0x3C,0x19,0xE6,0x85,0x4F,0xA8
.byte	0x68,0x6B,0x81,0xB2,0x71,0x64,0xDA,0x8B,0xF8,0xEB,0x0F,0x4B,0x70,0x56,0x9D,0x35
.byte	0x1E,0x24,0x0E,0x5E,0x63,0x58,0xD1,0xA2,0x25,0x22,0x7C,0x3B,0x01,0x21,0x78,0x87
.byte	0xD4,0x00,0x46,0x57,0x9F,0xD3,0x27,0x52,0x4C,0x36,0x02,0xE7,0xA0,0xC4,0xC8,0x9E
.byte	0xEA,0xBF,0x8A,0xD2,0x40,0xC7,0x38,0xB5,0xA3,0xF7,0xF2,0xCE,0xF9,0x61,0x15,0xA1
.byte	0xE0,0xAE,0x5D,0xA4,0x9B,0x34,0x1A,0x55,0xAD,0x93,0x32,0x30,0xF5,0x8C,0xB1,0xE3
.byte	0x1D,0xF6,0xE2,0x2E,0x82,0x66,0xCA,0x60,0xC0,0x29,0x23,0xAB,0x0D,0x53,0x4E,0x6F
.byte	0xD5,0xDB,0x37,0x45,0xDE,0xFD,0x8E,0x2F,0x03,0xFF,0x6A,0x72,0x6D,0x6C,0x5B,0x51
.byte	0x8D,0x1B,0xAF,0x92,0xBB,0xDD,0xBC,0x7F,0x11,0xD9,0x5C,0x41,0x1F,0x10,0x5A,0xD8
.byte	0x0A,0xC1,0x31,0x88,0xA5,0xCD,0x7B,0xBD,0x2D,0x74,0xD0,0x12,0xB8,0xE5,0xB4,0xB0
.byte	0x89,0x69,0x97,0x4A,0x0C,0x96,0x77,0x7E,0x65,0xB9,0xF1,0x09,0xC5,0x6E,0xC6,0x84
.byte	0x18,0xF0,0x7D,0xEC,0x3A,0xDC,0x4D,0x20,0x79,0xEE,0x5F,0x3E,0xD7,0xCB,0x39,0x48
.Lck:
.long	0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269
.long	0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9
.long	0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249
.long	0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9
.long	0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229
.long	0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299
.long	0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209
.long	0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
.Lfk:
.quad	0x56aa3350a3b1bac6,0xb27022dc677d9197
.Lshuffles:
.quad	0x0B0A090807060504,0x030201000F0E0D0C
.Lxts_magic:
.quad	0x0101010101010187,0x0101010101010101

.size	_vpsm4_consts,.-_vpsm4_consts

.previous

.type	_vpsm4_set_key,%function
.align	4
_vpsm4_set_key:
	AARCH64_VALID_CALL_TARGET
	ld1	{v5.4s},[x0]
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
	adrp	x5,.Lshuffles
	add	x5,x5,#:lo12:.Lshuffles
	ld1	{v7.2d},[x5]
	adrp	x5,.Lfk
	add	x5,x5,#:lo12:.Lfk
	ld1	{v6.2d},[x5]
	eor	v5.16b,v5.16b,v6.16b
	mov	x6,#32
	adrp	x5,.Lck
	add	x5,x5,#:lo12:.Lck
	movi	v0.16b,#64
	cbnz	w2,1f
	add	x1,x1,124
1:
	mov	w7,v5.s[1]
	ldr	w8,[x5],#4
	eor	w8,w8,w7
	mov	w7,v5.s[2]
	eor	w8,w8,w7
	mov	w7,v5.s[3]
	eor	w8,w8,w7
	// sbox lookup
	mov	v4.s[0],w8
	tbl	v1.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v4.16b
	sub	v4.16b,v4.16b,v0.16b
	tbx	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v4.16b
	sub	v4.16b,v4.16b,v0.16b
	tbx	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v4.16b
	sub	v4.16b,v4.16b,v0.16b
	tbx	v1.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v4.16b
	mov	w7,v1.s[0]
	eor	w8,w7,w7,ror #19
	eor	w8,w8,w7,ror #9
	mov	w7,v5.s[0]
	eor	w8,w8,w7
	mov	v5.s[0],w8
	cbz	w2,2f
	str	w8,[x1],#4
	b	3f
2:
	str	w8,[x1],#-4
3:
	tbl	v5.16b,{v5.16b},v7.16b
	subs	x6,x6,#1
	b.ne	1b
	ret
.size	_vpsm4_set_key,.-_vpsm4_set_key
.type	_vpsm4_enc_4blks,%function
.align	4
_vpsm4_enc_4blks:
	AARCH64_VALID_CALL_TARGET
	mov	x10,x3
	mov	w11,#8
10:
	ldp	w7,w8,[x10],8
	dup	v12.4s,w7
	dup	v13.4s,w8

	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	v14.16b,v6.16b,v7.16b
	eor	v12.16b,v5.16b,v12.16b
	eor	v12.16b,v14.16b,v12.16b
	movi	v0.16b,#64
	movi	v1.16b,#128
	movi	v2.16b,#192
	sub	v0.16b,v12.16b,v0.16b
	sub	v1.16b,v12.16b,v1.16b
	sub	v2.16b,v12.16b,v2.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v0.2d,v0.2d,v1.2d
	add	v2.2d,v2.2d,v12.2d
	add	v12.2d,v0.2d,v2.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v12.4s,32-10
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v12.4s,10
	eor	v1.16b,v2.16b,v1.16b
	ushr	v0.4s,v12.4s,32-18
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v12.4s,32-24
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v12.4s,24
	eor	v12.16b,v2.16b,v1.16b
	eor	v4.16b,v4.16b,v12.16b

	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	v14.16b,v14.16b,v4.16b
	eor	v13.16b,v14.16b,v13.16b
	movi	v0.16b,#64
	movi	v1.16b,#128
	movi	v2.16b,#192
	sub	v0.16b,v13.16b,v0.16b
	sub	v1.16b,v13.16b,v1.16b
	sub	v2.16b,v13.16b,v2.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v0.2d,v0.2d,v1.2d
	add	v2.2d,v2.2d,v13.2d
	add	v13.2d,v0.2d,v2.2d

	ushr	v0.4s,v13.4s,32-2
	sli	v0.4s,v13.4s,2
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v13.16b
	sli	v2.4s,v13.4s,10
	eor	v1.16b,v2.16b,v1.16b
	ushr	v0.4s,v13.4s,32-18
	sli	v0.4s,v13.4s,18
	ushr	v2.4s,v13.4s,32-24
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v1.16b
	ldp	w7,w8,[x10],8
	eor	v5.16b,v5.16b,v13.16b

	dup	v12.4s,w7
	dup	v13.4s,w8

	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	v14.16b,v4.16b,v5.16b
	eor	v12.16b,v7.16b,v12.16b
	eor	v12.16b,v14.16b,v12.16b
	movi	v0.16b,#64
	movi	v1.16b,#128
	movi	v2.16b,#192
	sub	v0.16b,v12.16b,v0.16b
	sub	v1.16b,v12.16b,v1.16b
	sub	v2.16b,v12.16b,v2.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v0.2d,v0.2d,v1.2d
	add	v2.2d,v2.2d,v12.2d
	add	v12.2d,v0.2d,v2.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v12.4s,32-10
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v12.4s,10
	eor	v1.16b,v2.16b,v1.16b
	ushr	v0.4s,v12.4s,32-18
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v12.4s,32-24
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v12.4s,24
	eor	v12.16b,v2.16b,v1.16b
	eor	v6.16b,v6.16b,v12.16b

	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	v14.16b,v14.16b,v6.16b
	eor	v13.16b,v14.16b,v13.16b
	movi	v0.16b,#64
	movi	v1.16b,#128
	movi	v2.16b,#192
	sub	v0.16b,v13.16b,v0.16b
	sub	v1.16b,v13.16b,v1.16b
	sub	v2.16b,v13.16b,v2.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v0.2d,v0.2d,v1.2d
	add	v2.2d,v2.2d,v13.2d
	add	v13.2d,v0.2d,v2.2d

	ushr	v0.4s,v13.4s,32-2
	sli	v0.4s,v13.4s,2
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v13.16b
	sli	v2.4s,v13.4s,10
	eor	v1.16b,v2.16b,v1.16b
	ushr	v0.4s,v13.4s,32-18
	sli	v0.4s,v13.4s,18
	ushr	v2.4s,v13.4s,32-24
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v1.16b
	eor	v7.16b,v7.16b,v13.16b
	subs	w11,w11,#1
	b.ne	10b
#ifndef __AARCH64EB__
	rev32	v3.16b,v4.16b
#else
	mov	v3.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v2.16b,v5.16b
#else
	mov	v2.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v1.16b,v6.16b
#else
	mov	v1.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v0.16b,v7.16b
#else
	mov	v0.16b,v7.16b
#endif
	ret
.size	_vpsm4_enc_4blks,.-_vpsm4_enc_4blks
.type	_vpsm4_enc_8blks,%function
.align	4
_vpsm4_enc_8blks:
	AARCH64_VALID_CALL_TARGET
	mov	x10,x3
	mov	w11,#8
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	dup	v12.4s,w7
	eor	v14.16b,v6.16b,v7.16b
	eor	v15.16b,v10.16b,v11.16b
	eor	v0.16b,v5.16b,v12.16b
	eor	v1.16b,v9.16b,v12.16b
	eor	v12.16b,v14.16b,v0.16b
	eor	v13.16b,v15.16b,v1.16b
	movi	v3.16b,#64
	sub	v0.16b,v12.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v12.2d,v2.2d,v12.2d
	add	v12.2d,v1.2d,v12.2d

	sub	v0.16b,v13.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v13.2d,v2.2d,v13.2d
	add	v13.2d,v1.2d,v13.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v13.4s,32-2
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v13.4s,2

	ushr	v0.4s,v12.4s,32-10
	eor	v3.16b,v2.16b,v13.16b
	sli	v0.4s,v12.4s,10
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,10

	ushr	v0.4s,v12.4s,32-18
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v13.4s,32-18
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,18

	ushr	v0.4s,v12.4s,32-24
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,24
	ushr	v2.4s,v13.4s,32-24
	eor	v12.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v3.16b
	eor	v4.16b,v4.16b,v12.16b
	eor	v8.16b,v8.16b,v13.16b

	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	dup	v13.4s,w8
	eor	v14.16b,v14.16b,v4.16b
	eor	v15.16b,v15.16b,v8.16b
	eor	v12.16b,v14.16b,v13.16b
	eor	v13.16b,v15.16b,v13.16b
	movi	v3.16b,#64
	sub	v0.16b,v12.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v12.2d,v2.2d,v12.2d
	add	v12.2d,v1.2d,v12.2d

	sub	v0.16b,v13.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v13.2d,v2.2d,v13.2d
	add	v13.2d,v1.2d,v13.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v13.4s,32-2
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v13.4s,2

	ushr	v0.4s,v12.4s,32-10
	eor	v3.16b,v2.16b,v13.16b
	sli	v0.4s,v12.4s,10
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,10

	ushr	v0.4s,v12.4s,32-18
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v13.4s,32-18
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,18

	ushr	v0.4s,v12.4s,32-24
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,24
	ushr	v2.4s,v13.4s,32-24
	eor	v12.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v3.16b
	ldp	w7,w8,[x10],8
	eor	v5.16b,v5.16b,v12.16b
	eor	v9.16b,v9.16b,v13.16b

	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	dup	v12.4s,w7
	eor	v14.16b,v4.16b,v5.16b
	eor	v15.16b,v8.16b,v9.16b
	eor	v0.16b,v7.16b,v12.16b
	eor	v1.16b,v11.16b,v12.16b
	eor	v12.16b,v14.16b,v0.16b
	eor	v13.16b,v15.16b,v1.16b
	movi	v3.16b,#64
	sub	v0.16b,v12.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v12.2d,v2.2d,v12.2d
	add	v12.2d,v1.2d,v12.2d

	sub	v0.16b,v13.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v13.2d,v2.2d,v13.2d
	add	v13.2d,v1.2d,v13.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v13.4s,32-2
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v13.4s,2

	ushr	v0.4s,v12.4s,32-10
	eor	v3.16b,v2.16b,v13.16b
	sli	v0.4s,v12.4s,10
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,10

	ushr	v0.4s,v12.4s,32-18
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v13.4s,32-18
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,18

	ushr	v0.4s,v12.4s,32-24
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,24
	ushr	v2.4s,v13.4s,32-24
	eor	v12.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v3.16b
	eor	v6.16b,v6.16b,v12.16b
	eor	v10.16b,v10.16b,v13.16b

	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	dup	v13.4s,w8
	eor	v14.16b,v14.16b,v6.16b
	eor	v15.16b,v15.16b,v10.16b
	eor	v12.16b,v14.16b,v13.16b
	eor	v13.16b,v15.16b,v13.16b
	movi	v3.16b,#64
	sub	v0.16b,v12.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v12.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v12.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v12.2d,v2.2d,v12.2d
	add	v12.2d,v1.2d,v12.2d

	sub	v0.16b,v13.16b,v3.16b
	sub	v1.16b,v0.16b,v3.16b
	sub	v2.16b,v1.16b,v3.16b
	tbl	v13.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v13.16b
	tbl	v0.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v0.16b
	tbl	v1.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v1.16b
	tbl	v2.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v2.16b
	add	v1.2d,v0.2d,v1.2d
	add	v13.2d,v2.2d,v13.2d
	add	v13.2d,v1.2d,v13.2d

	ushr	v0.4s,v12.4s,32-2
	sli	v0.4s,v12.4s,2
	ushr	v2.4s,v13.4s,32-2
	eor	v1.16b,v0.16b,v12.16b
	sli	v2.4s,v13.4s,2

	ushr	v0.4s,v12.4s,32-10
	eor	v3.16b,v2.16b,v13.16b
	sli	v0.4s,v12.4s,10
	ushr	v2.4s,v13.4s,32-10
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,10

	ushr	v0.4s,v12.4s,32-18
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,18
	ushr	v2.4s,v13.4s,32-18
	eor	v1.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,18

	ushr	v0.4s,v12.4s,32-24
	eor	v3.16b,v2.16b,v3.16b
	sli	v0.4s,v12.4s,24
	ushr	v2.4s,v13.4s,32-24
	eor	v12.16b,v0.16b,v1.16b
	sli	v2.4s,v13.4s,24
	eor	v13.16b,v2.16b,v3.16b
	eor	v7.16b,v7.16b,v12.16b
	eor	v11.16b,v11.16b,v13.16b
	subs	w11,w11,#1
	b.ne	10b
#ifndef __AARCH64EB__
	rev32	v3.16b,v4.16b
#else
	mov	v3.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v2.16b,v5.16b
#else
	mov	v2.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v1.16b,v6.16b
#else
	mov	v1.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v0.16b,v7.16b
#else
	mov	v0.16b,v7.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v8.16b
#else
	mov	v7.16b,v8.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v9.16b
#else
	mov	v6.16b,v9.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v10.16b
#else
	mov	v5.16b,v10.16b
#endif
#ifndef __AARCH64EB__
	rev32	v4.16b,v11.16b
#else
	mov	v4.16b,v11.16b
#endif
	ret
.size	_vpsm4_enc_8blks,.-_vpsm4_enc_8blks
.globl	vpsm4_set_encrypt_key
.type	vpsm4_set_encrypt_key,%function
.align	5
vpsm4_set_encrypt_key:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-16]!
	mov	w2,1
	bl	_vpsm4_set_key
	ldp	x29,x30,[sp],#16
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_set_encrypt_key,.-vpsm4_set_encrypt_key
.globl	vpsm4_set_decrypt_key
.type	vpsm4_set_decrypt_key,%function
.align	5
vpsm4_set_decrypt_key:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-16]!
	mov	w2,0
	bl	_vpsm4_set_key
	ldp	x29,x30,[sp],#16
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_set_decrypt_key,.-vpsm4_set_decrypt_key
.globl	vpsm4_encrypt
.type	vpsm4_encrypt,%function
.align	5
vpsm4_encrypt:
	AARCH64_VALID_CALL_TARGET
	ld1	{v4.4s},[x0]
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x3,x2
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	st1	{v4.4s},[x1]
	ret
.size	vpsm4_encrypt,.-vpsm4_encrypt
.globl	vpsm4_decrypt
.type	vpsm4_decrypt,%function
.align	5
vpsm4_decrypt:
	AARCH64_VALID_CALL_TARGET
	ld1	{v4.4s},[x0]
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x3,x2
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	st1	{v4.4s},[x1]
	ret
.size	vpsm4_decrypt,.-vpsm4_decrypt
.globl	vpsm4_ecb_encrypt
.type	vpsm4_ecb_encrypt,%function
.align	5
vpsm4_ecb_encrypt:
	AARCH64_SIGN_LINK_REGISTER
	// convert length into blocks
	lsr	x2,x2,4
	stp	d8,d9,[sp,#-80]!
	stp	d10,d11,[sp,#16]
	stp	d12,d13,[sp,#32]
	stp	d14,d15,[sp,#48]
	stp	x29,x30,[sp,#64]
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
.Lecb_8_blocks_process:
	cmp	w2,#8
	b.lt	.Lecb_4_blocks_process
	ld4	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	ld4	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
#ifndef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifndef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
#ifndef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	bl	_vpsm4_enc_8blks
	st4	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	st4	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	w2,w2,#8
	b.gt	.Lecb_8_blocks_process
	b	100f
.Lecb_4_blocks_process:
	cmp	w2,#4
	b.lt	1f
	ld4	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	st4	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	sub	w2,w2,#4
1:
	// process last block
	cmp	w2,#1
	b.lt	100f
	b.gt	1f
	ld1	{v4.4s},[x0]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	st1	{v4.4s},[x1]
	b	100f
1:	//	process last 2 blocks
	ld4	{v4.s,v5.s,v6.s,v7.s}[0],[x0],#16
	ld4	{v4.s,v5.s,v6.s,v7.s}[1],[x0],#16
	cmp	w2,#2
	b.gt	1f
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	st4	{v0.s,v1.s,v2.s,v3.s}[0],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[1],[x1]
	b	100f
1:	//	process last 3 blocks
	ld4	{v4.s,v5.s,v6.s,v7.s}[2],[x0],#16
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	st4	{v0.s,v1.s,v2.s,v3.s}[0],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[1],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[2],[x1]
100:
	ldp	d10,d11,[sp,#16]
	ldp	d12,d13,[sp,#32]
	ldp	d14,d15,[sp,#48]
	ldp	x29,x30,[sp,#64]
	ldp	d8,d9,[sp],#80
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_ecb_encrypt,.-vpsm4_ecb_encrypt
.globl	vpsm4_cbc_encrypt
.type	vpsm4_cbc_encrypt,%function
.align	5
vpsm4_cbc_encrypt:
	AARCH64_VALID_CALL_TARGET
	lsr	x2,x2,4
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
	cbz	w5,.Ldec
	ld1	{v3.4s},[x4]
.Lcbc_4_blocks_enc:
	cmp	w2,#4
	b.lt	1f
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	eor	v4.16b,v4.16b,v3.16b
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
	eor	v5.16b,v5.16b,v4.16b
	mov	x10,x3
	mov	w11,#8
	mov	w12,v5.s[0]
	mov	w13,v5.s[1]
	mov	w14,v5.s[2]
	mov	w15,v5.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v5.s[0],w15
	mov	v5.s[1],w14
	mov	v5.s[2],w13
	mov	v5.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v6.16b,v6.16b,v5.16b
	mov	x10,x3
	mov	w11,#8
	mov	w12,v6.s[0]
	mov	w13,v6.s[1]
	mov	w14,v6.s[2]
	mov	w15,v6.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v6.s[0],w15
	mov	v6.s[1],w14
	mov	v6.s[2],w13
	mov	v6.s[3],w12
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
	eor	v7.16b,v7.16b,v6.16b
	mov	x10,x3
	mov	w11,#8
	mov	w12,v7.s[0]
	mov	w13,v7.s[1]
	mov	w14,v7.s[2]
	mov	w15,v7.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v7.s[0],w15
	mov	v7.s[1],w14
	mov	v7.s[2],w13
	mov	v7.s[3],w12
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	orr	v3.16b,v7.16b,v7.16b
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	w2,w2,#4
	b.ne	.Lcbc_4_blocks_enc
	b	2f
1:
	subs	w2,w2,#1
	b.lt	2f
	ld1	{v4.4s},[x0],#16
	eor	v3.16b,v3.16b,v4.16b
#ifndef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v3.s[0]
	mov	w13,v3.s[1]
	mov	w14,v3.s[2]
	mov	w15,v3.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v3.s[0],w15
	mov	v3.s[1],w14
	mov	v3.s[2],w13
	mov	v3.s[3],w12
#ifndef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	st1	{v3.4s},[x1],#16
	b	1b
2:
	// save back IV
	st1	{v3.4s},[x4]
	ret

.Ldec:
	// decryption mode starts
	AARCH64_SIGN_LINK_REGISTER
	stp	d8,d9,[sp,#-80]!
	stp	d10,d11,[sp,#16]
	stp	d12,d13,[sp,#32]
	stp	d14,d15,[sp,#48]
	stp	x29,x30,[sp,#64]
.Lcbc_8_blocks_dec:
	cmp	w2,#8
	b.lt	1f
	ld4	{v4.4s,v5.4s,v6.4s,v7.4s},[x0]
	add	x10,x0,#64
	ld4	{v8.4s,v9.4s,v10.4s,v11.4s},[x10]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
#ifndef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifndef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
#ifndef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	bl	_vpsm4_enc_8blks
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	zip1	v8.4s,v4.4s,v5.4s
	zip2	v9.4s,v4.4s,v5.4s
	zip1	v10.4s,v6.4s,v7.4s
	zip2	v11.4s,v6.4s,v7.4s
	zip1	v4.2d,v8.2d,v10.2d
	zip2	v5.2d,v8.2d,v10.2d
	zip1	v6.2d,v9.2d,v11.2d
	zip2	v7.2d,v9.2d,v11.2d
	ld1	{v15.4s},[x4]
	ld1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	// note ivec1 and vtmpx[3] are reusing the same register
	// care needs to be taken to avoid conflict
	eor	v0.16b,v0.16b,v15.16b
	ld1	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	eor	v1.16b,v1.16b,v8.16b
	eor	v2.16b,v2.16b,v9.16b
	eor	v3.16b,v3.16b,v10.16b
	// save back IV
	st1	{v15.4s}, [x4]
	eor	v4.16b,v4.16b,v11.16b
	eor	v5.16b,v5.16b,v12.16b
	eor	v6.16b,v6.16b,v13.16b
	eor	v7.16b,v7.16b,v14.16b
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	w2,w2,#8
	b.gt	.Lcbc_8_blocks_dec
	b.eq	100f
1:
	ld1	{v15.4s},[x4]
.Lcbc_4_blocks_dec:
	cmp	w2,#4
	b.lt	1f
	ld4	{v4.4s,v5.4s,v6.4s,v7.4s},[x0]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	eor	v0.16b,v0.16b,v15.16b
	eor	v1.16b,v1.16b,v4.16b
	orr	v15.16b,v7.16b,v7.16b
	eor	v2.16b,v2.16b,v5.16b
	eor	v3.16b,v3.16b,v6.16b
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	subs	w2,w2,#4
	b.gt	.Lcbc_4_blocks_dec
	// save back IV
	st1	{v7.4s}, [x4]
	b	100f
1:	//	last block
	subs	w2,w2,#1
	b.lt	100f
	b.gt	1f
	ld1	{v4.4s},[x0],#16
	// save back IV
	st1	{v4.4s}, [x4]
#ifndef __AARCH64EB__
	rev32	v8.16b,v4.16b
#else
	mov	v8.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v8.s[0]
	mov	w13,v8.s[1]
	mov	w14,v8.s[2]
	mov	w15,v8.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v8.s[0],w15
	mov	v8.s[1],w14
	mov	v8.s[2],w13
	mov	v8.s[3],w12
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	eor	v8.16b,v8.16b,v15.16b
	st1	{v8.4s},[x1],#16
	b	100f
1:	//	last two blocks
	ld4	{v4.s,v5.s,v6.s,v7.s}[0],[x0]
	add	x10,x0,#16
	ld4	{v4.s,v5.s,v6.s,v7.s}[1],[x10],#16
	subs	w2,w2,1
	b.gt	1f
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	ld1	{v4.4s,v5.4s},[x0],#32
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	eor	v0.16b,v0.16b,v15.16b
	eor	v1.16b,v1.16b,v4.16b
	st1	{v0.4s,v1.4s},[x1],#32
	// save back IV
	st1	{v5.4s}, [x4]
	b	100f
1:	//	last 3 blocks
	ld4	{v4.s,v5.s,v6.s,v7.s}[2],[x10]
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	bl	_vpsm4_enc_4blks
	ld1	{v4.4s,v5.4s,v6.4s},[x0],#48
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	eor	v0.16b,v0.16b,v15.16b
	eor	v1.16b,v1.16b,v4.16b
	eor	v2.16b,v2.16b,v5.16b
	st1	{v0.4s,v1.4s,v2.4s},[x1],#48
	// save back IV
	st1	{v6.4s}, [x4]
100:
	ldp	d10,d11,[sp,#16]
	ldp	d12,d13,[sp,#32]
	ldp	d14,d15,[sp,#48]
	ldp	x29,x30,[sp,#64]
	ldp	d8,d9,[sp],#80
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_cbc_encrypt,.-vpsm4_cbc_encrypt
.globl	vpsm4_ctr32_encrypt_blocks
.type	vpsm4_ctr32_encrypt_blocks,%function
.align	5
vpsm4_ctr32_encrypt_blocks:
	AARCH64_VALID_CALL_TARGET
	ld1	{v3.4s},[x4]
#ifndef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
	cmp	w2,#1
	b.ne	1f
	// fast processing for one single block without
	// context saving overhead
	mov	x10,x3
	mov	w11,#8
	mov	w12,v3.s[0]
	mov	w13,v3.s[1]
	mov	w14,v3.s[2]
	mov	w15,v3.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v3.s[0],w15
	mov	v3.s[1],w14
	mov	v3.s[2],w13
	mov	v3.s[3],w12
#ifndef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	ld1	{v4.4s},[x0]
	eor	v4.16b,v4.16b,v3.16b
	st1	{v4.4s},[x1]
	ret
1:
	AARCH64_SIGN_LINK_REGISTER
	stp	d8,d9,[sp,#-80]!
	stp	d10,d11,[sp,#16]
	stp	d12,d13,[sp,#32]
	stp	d14,d15,[sp,#48]
	stp	x29,x30,[sp,#64]
	mov	w12,v3.s[0]
	mov	w13,v3.s[1]
	mov	w14,v3.s[2]
	mov	w5,v3.s[3]
.Lctr32_4_blocks_process:
	cmp	w2,#4
	b.lt	1f
	dup	v4.4s,w12
	dup	v5.4s,w13
	dup	v6.4s,w14
	mov	v7.s[0],w5
	add	w5,w5,#1
	mov	v7.s[1],w5
	add	w5,w5,#1
	mov	v7.s[2],w5
	add	w5,w5,#1
	mov	v7.s[3],w5
	add	w5,w5,#1
	cmp	w2,#8
	b.ge	.Lctr32_8_blocks_process
	bl	_vpsm4_enc_4blks
	ld4	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	eor	v0.16b,v0.16b,v12.16b
	eor	v1.16b,v1.16b,v13.16b
	eor	v2.16b,v2.16b,v14.16b
	eor	v3.16b,v3.16b,v15.16b
	st4	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	subs	w2,w2,#4
	b.ne	.Lctr32_4_blocks_process
	b	100f
.Lctr32_8_blocks_process:
	dup	v8.4s,w12
	dup	v9.4s,w13
	dup	v10.4s,w14
	mov	v11.s[0],w5
	add	w5,w5,#1
	mov	v11.s[1],w5
	add	w5,w5,#1
	mov	v11.s[2],w5
	add	w5,w5,#1
	mov	v11.s[3],w5
	add	w5,w5,#1
	bl	_vpsm4_enc_8blks
	ld4	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	ld4	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	eor	v0.16b,v0.16b,v12.16b
	eor	v1.16b,v1.16b,v13.16b
	eor	v2.16b,v2.16b,v14.16b
	eor	v3.16b,v3.16b,v15.16b
	eor	v4.16b,v4.16b,v8.16b
	eor	v5.16b,v5.16b,v9.16b
	eor	v6.16b,v6.16b,v10.16b
	eor	v7.16b,v7.16b,v11.16b
	st4	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	st4	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	w2,w2,#8
	b.ne	.Lctr32_4_blocks_process
	b	100f
1:	//	last block processing
	subs	w2,w2,#1
	b.lt	100f
	b.gt	1f
	mov	v3.s[0],w12
	mov	v3.s[1],w13
	mov	v3.s[2],w14
	mov	v3.s[3],w5
	mov	x10,x3
	mov	w11,#8
	mov	w12,v3.s[0]
	mov	w13,v3.s[1]
	mov	w14,v3.s[2]
	mov	w15,v3.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v3.s[0],w15
	mov	v3.s[1],w14
	mov	v3.s[2],w13
	mov	v3.s[3],w12
#ifndef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	ld1	{v4.4s},[x0]
	eor	v4.16b,v4.16b,v3.16b
	st1	{v4.4s},[x1]
	b	100f
1:	//	last 2 blocks processing
	dup	v4.4s,w12
	dup	v5.4s,w13
	dup	v6.4s,w14
	mov	v7.s[0],w5
	add	w5,w5,#1
	mov	v7.s[1],w5
	subs	w2,w2,#1
	b.ne	1f
	bl	_vpsm4_enc_4blks
	ld4	{v12.s,v13.s,v14.s,v15.s}[0],[x0],#16
	ld4	{v12.s,v13.s,v14.s,v15.s}[1],[x0],#16
	eor	v0.16b,v0.16b,v12.16b
	eor	v1.16b,v1.16b,v13.16b
	eor	v2.16b,v2.16b,v14.16b
	eor	v3.16b,v3.16b,v15.16b
	st4	{v0.s,v1.s,v2.s,v3.s}[0],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[1],[x1],#16
	b	100f
1:	//	last 3 blocks processing
	add	w5,w5,#1
	mov	v7.s[2],w5
	bl	_vpsm4_enc_4blks
	ld4	{v12.s,v13.s,v14.s,v15.s}[0],[x0],#16
	ld4	{v12.s,v13.s,v14.s,v15.s}[1],[x0],#16
	ld4	{v12.s,v13.s,v14.s,v15.s}[2],[x0],#16
	eor	v0.16b,v0.16b,v12.16b
	eor	v1.16b,v1.16b,v13.16b
	eor	v2.16b,v2.16b,v14.16b
	eor	v3.16b,v3.16b,v15.16b
	st4	{v0.s,v1.s,v2.s,v3.s}[0],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[1],[x1],#16
	st4	{v0.s,v1.s,v2.s,v3.s}[2],[x1],#16
100:
	ldp	d10,d11,[sp,#16]
	ldp	d12,d13,[sp,#32]
	ldp	d14,d15,[sp,#48]
	ldp	x29,x30,[sp,#64]
	ldp	d8,d9,[sp],#80
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_ctr32_encrypt_blocks,.-vpsm4_ctr32_encrypt_blocks
.globl	vpsm4_xts_encrypt_gb
.type	vpsm4_xts_encrypt_gb,%function
.align	5
vpsm4_xts_encrypt_gb:
	AARCH64_SIGN_LINK_REGISTER
	stp	x15, x16, [sp, #-0x10]!
	stp	x17, x18, [sp, #-0x10]!
	stp	x19, x20, [sp, #-0x10]!
	stp	x21, x22, [sp, #-0x10]!
	stp	x23, x24, [sp, #-0x10]!
	stp	x25, x26, [sp, #-0x10]!
	stp	x27, x28, [sp, #-0x10]!
	stp	x29, x30, [sp, #-0x10]!
	stp	d8, d9, [sp, #-0x10]!
	stp	d10, d11, [sp, #-0x10]!
	stp	d12, d13, [sp, #-0x10]!
	stp	d14, d15, [sp, #-0x10]!
	mov	x26,x3
	mov	x27,x4
	mov	w28,w6
	ld1	{v8.4s}, [x5]
	mov	x3,x27
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v8.s[0]
	mov	w13,v8.s[1]
	mov	w14,v8.s[2]
	mov	w15,v8.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v8.s[0],w15
	mov	v8.s[1],w14
	mov	v8.s[2],w13
	mov	v8.s[3],w12
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x3,x26
	and	x29,x2,#0x0F
	// convert length into blocks
	lsr	x2,x2,4
	cmp	x2,#1
	b.lt	.return_gb

	cmp	x29,0
	// If the encryption/decryption Length is N times of 16,
	// the all blocks are encrypted/decrypted in .xts_encrypt_blocks_gb
	b.eq	.xts_encrypt_blocks_gb

	// If the encryption/decryption length is not N times of 16,
	// the last two blocks are encrypted/decrypted in .last_2blks_tweak_gb or .only_2blks_tweak_gb
	// the other blocks are encrypted/decrypted in .xts_encrypt_blocks_gb
	subs	x2,x2,#1
	b.eq	.only_2blks_tweak_gb
.xts_encrypt_blocks_gb:
	rbit	v8.16b,v8.16b
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x12,v8.d[0]
	mov	x13,v8.d[1]
	mov	w7,0x87
	extr	x9,x13,x13,#32
	extr	x15,x13,x12,#63
	and	w8,w7,w9,asr#31
	eor	x14,x8,x12,lsl#1
	mov	w7,0x87
	extr	x9,x15,x15,#32
	extr	x17,x15,x14,#63
	and	w8,w7,w9,asr#31
	eor	x16,x8,x14,lsl#1
	mov	w7,0x87
	extr	x9,x17,x17,#32
	extr	x19,x17,x16,#63
	and	w8,w7,w9,asr#31
	eor	x18,x8,x16,lsl#1
	mov	w7,0x87
	extr	x9,x19,x19,#32
	extr	x21,x19,x18,#63
	and	w8,w7,w9,asr#31
	eor	x20,x8,x18,lsl#1
	mov	w7,0x87
	extr	x9,x21,x21,#32
	extr	x23,x21,x20,#63
	and	w8,w7,w9,asr#31
	eor	x22,x8,x20,lsl#1
	mov	w7,0x87
	extr	x9,x23,x23,#32
	extr	x25,x23,x22,#63
	and	w8,w7,w9,asr#31
	eor	x24,x8,x22,lsl#1
	mov	w7,0x87
	extr	x9,x25,x25,#32
	extr	x27,x25,x24,#63
	and	w8,w7,w9,asr#31
	eor	x26,x8,x24,lsl#1
.Lxts_8_blocks_process_gb:
	cmp	x2,#8
	b.lt	.Lxts_4_blocks_process_gb
	mov	v0.d[0],x12
	mov	v0.d[1],x13
#ifdef __AARCH64EB__
	rev32	v0.16b,v0.16b
#endif
	mov	v1.d[0],x14
	mov	v1.d[1],x15
#ifdef __AARCH64EB__
	rev32	v1.16b,v1.16b
#endif
	mov	v2.d[0],x16
	mov	v2.d[1],x17
#ifdef __AARCH64EB__
	rev32	v2.16b,v2.16b
#endif
	mov	v3.d[0],x18
	mov	v3.d[1],x19
#ifdef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	mov	v12.d[0],x20
	mov	v12.d[1],x21
#ifdef __AARCH64EB__
	rev32	v12.16b,v12.16b
#endif
	mov	v13.d[0],x22
	mov	v13.d[1],x23
#ifdef __AARCH64EB__
	rev32	v13.16b,v13.16b
#endif
	mov	v14.d[0],x24
	mov	v14.d[1],x25
#ifdef __AARCH64EB__
	rev32	v14.16b,v14.16b
#endif
	mov	v15.d[0],x26
	mov	v15.d[1],x27
#ifdef __AARCH64EB__
	rev32	v15.16b,v15.16b
#endif
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	rbit	v0.16b,v0.16b
	rbit	v1.16b,v1.16b
	rbit	v2.16b,v2.16b
	rbit	v3.16b,v3.16b
	eor	v4.16b, v4.16b, v0.16b
	eor	v5.16b, v5.16b, v1.16b
	eor	v6.16b, v6.16b, v2.16b
	eor	v7.16b, v7.16b, v3.16b
	ld1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	rbit	v12.16b,v12.16b
	rbit	v13.16b,v13.16b
	rbit	v14.16b,v14.16b
	rbit	v15.16b,v15.16b
	eor	v8.16b, v8.16b, v12.16b
	eor	v9.16b, v9.16b, v13.16b
	eor	v10.16b, v10.16b, v14.16b
	eor	v11.16b, v11.16b, v15.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
#ifndef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifndef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
#ifndef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	zip1	v0.4s,v8.4s,v9.4s
	zip2	v1.4s,v8.4s,v9.4s
	zip1	v2.4s,v10.4s,v11.4s
	zip2	v3.4s,v10.4s,v11.4s
	zip1	v8.2d,v0.2d,v2.2d
	zip2	v9.2d,v0.2d,v2.2d
	zip1	v10.2d,v1.2d,v3.2d
	zip2	v11.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_8blks
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	zip1	v8.4s,v4.4s,v5.4s
	zip2	v9.4s,v4.4s,v5.4s
	zip1	v10.4s,v6.4s,v7.4s
	zip2	v11.4s,v6.4s,v7.4s
	zip1	v4.2d,v8.2d,v10.2d
	zip2	v5.2d,v8.2d,v10.2d
	zip1	v6.2d,v9.2d,v11.2d
	zip2	v7.2d,v9.2d,v11.2d
	mov	v12.d[0],x12
	mov	v12.d[1],x13
#ifdef __AARCH64EB__
	rev32	v12.16b,v12.16b
#endif
	mov	w7,0x87
	extr	x9,x27,x27,#32
	extr	x13,x27,x26,#63
	and	w8,w7,w9,asr#31
	eor	x12,x8,x26,lsl#1
	mov	v13.d[0],x14
	mov	v13.d[1],x15
#ifdef __AARCH64EB__
	rev32	v13.16b,v13.16b
#endif
	mov	w7,0x87
	extr	x9,x13,x13,#32
	extr	x15,x13,x12,#63
	and	w8,w7,w9,asr#31
	eor	x14,x8,x12,lsl#1
	mov	v14.d[0],x16
	mov	v14.d[1],x17
#ifdef __AARCH64EB__
	rev32	v14.16b,v14.16b
#endif
	mov	w7,0x87
	extr	x9,x15,x15,#32
	extr	x17,x15,x14,#63
	and	w8,w7,w9,asr#31
	eor	x16,x8,x14,lsl#1
	mov	v15.d[0],x18
	mov	v15.d[1],x19
#ifdef __AARCH64EB__
	rev32	v15.16b,v15.16b
#endif
	mov	w7,0x87
	extr	x9,x17,x17,#32
	extr	x19,x17,x16,#63
	and	w8,w7,w9,asr#31
	eor	x18,x8,x16,lsl#1
	mov	v8.d[0],x20
	mov	v8.d[1],x21
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	w7,0x87
	extr	x9,x19,x19,#32
	extr	x21,x19,x18,#63
	and	w8,w7,w9,asr#31
	eor	x20,x8,x18,lsl#1
	mov	v9.d[0],x22
	mov	v9.d[1],x23
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	w7,0x87
	extr	x9,x21,x21,#32
	extr	x23,x21,x20,#63
	and	w8,w7,w9,asr#31
	eor	x22,x8,x20,lsl#1
	mov	v10.d[0],x24
	mov	v10.d[1],x25
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	mov	w7,0x87
	extr	x9,x23,x23,#32
	extr	x25,x23,x22,#63
	and	w8,w7,w9,asr#31
	eor	x24,x8,x22,lsl#1
	mov	v11.d[0],x26
	mov	v11.d[1],x27
#ifdef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	mov	w7,0x87
	extr	x9,x25,x25,#32
	extr	x27,x25,x24,#63
	and	w8,w7,w9,asr#31
	eor	x26,x8,x24,lsl#1
	eor	v0.16b, v0.16b, v12.16b
	eor	v1.16b, v1.16b, v13.16b
	eor	v2.16b, v2.16b, v14.16b
	eor	v3.16b, v3.16b, v15.16b
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
	eor	v7.16b, v7.16b, v11.16b

	// save the last tweak
	st1	{v11.4s},[x5]
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	x2,x2,#8
	b.gt	.Lxts_8_blocks_process_gb
	b	100f
.Lxts_4_blocks_process_gb:
	mov	v8.d[0],x12
	mov	v8.d[1],x13
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	v9.d[0],x14
	mov	v9.d[1],x15
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	v10.d[0],x16
	mov	v10.d[1],x17
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	mov	v11.d[0],x18
	mov	v11.d[1],x19
#ifdef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	cmp	x2,#4
	b.lt	1f
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	rbit	v8.16b,v8.16b
	rbit	v9.16b,v9.16b
	rbit	v10.16b,v10.16b
	rbit	v11.16b,v11.16b
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
	eor	v7.16b, v7.16b, v11.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	eor	v2.16b, v2.16b, v10.16b
	eor	v3.16b, v3.16b, v11.16b
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	sub	x2,x2,#4
	mov	v8.d[0],x20
	mov	v8.d[1],x21
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	v9.d[0],x22
	mov	v9.d[1],x23
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	v10.d[0],x24
	mov	v10.d[1],x25
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	// save the last tweak
	st1	{v11.4s},[x5]
1:
	// process last block
	cmp	x2,#1
	b.lt	100f
	b.gt	1f
	ld1	{v4.4s},[x0],#16
	rbit	v8.16b,v8.16b
	eor	v4.16b, v4.16b, v8.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v8.16b
	st1	{v4.4s},[x1],#16
	// save the last tweak
	st1	{v8.4s},[x5]
	b	100f
1:	//	process last 2 blocks
	cmp	x2,#2
	b.gt	1f
	ld1	{v4.4s,v5.4s},[x0],#32
	rbit	v8.16b,v8.16b
	rbit	v9.16b,v9.16b
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	st1	{v0.4s,v1.4s},[x1],#32
	// save the last tweak
	st1	{v9.4s},[x5]
	b	100f
1:	//	process last 3 blocks
	ld1	{v4.4s,v5.4s,v6.4s},[x0],#48
	rbit	v8.16b,v8.16b
	rbit	v9.16b,v9.16b
	rbit	v10.16b,v10.16b
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	eor	v2.16b, v2.16b, v10.16b
	st1	{v0.4s,v1.4s,v2.4s},[x1],#48
	// save the last tweak
	st1	{v10.4s},[x5]
100:
	cmp	x29,0
	b.eq	.return_gb

// This branch calculates the last two tweaks, 
// while the encryption/decryption length is larger than 32
.last_2blks_tweak_gb:
	ld1	{v8.4s},[x5]
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	rbit	v2.16b,v8.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v9.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v9.16b, v9.16b, v1.16b
	rbit	v9.16b,v9.16b
	rbit	v2.16b,v9.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v10.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v10.16b, v10.16b, v1.16b
	rbit	v10.16b,v10.16b
	b	.check_dec_gb


// This branch calculates the last two tweaks, 
// while the encryption/decryption length is equal to 32, who only need two tweaks
.only_2blks_tweak_gb:
	mov	v9.16b,v8.16b
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	rbit	v2.16b,v9.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v10.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v10.16b, v10.16b, v1.16b
	rbit	v10.16b,v10.16b
	b	.check_dec_gb


// Determine whether encryption or decryption is required.
// The last two tweaks need to be swapped for decryption.
.check_dec_gb:
	// encryption:1 decryption:0
	cmp	w28,1
	b.eq	.process_last_2blks_gb
	mov	v0.16B,v9.16b
	mov	v9.16B,v10.16b
	mov	v10.16B,v0.16b

.process_last_2blks_gb:
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	ld1	{v4.4s},[x0],#16
	eor	v4.16b, v4.16b, v9.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v9.16b
	st1	{v4.4s},[x1],#16

	sub	x26,x1,16
.loop_gb:
	subs	x29,x29,1
	ldrb	w7,[x26,x29]
	ldrb	w8,[x0,x29]
	strb	w8,[x26,x29]
	strb	w7,[x1,x29]
	b.gt	.loop_gb
	ld1	{v4.4s}, [x26]
	eor	v4.16b, v4.16b, v10.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v10.16b
	st1	{v4.4s}, [x26]
.return_gb:
	ldp	d14, d15, [sp], #0x10
	ldp	d12, d13, [sp], #0x10
	ldp	d10, d11, [sp], #0x10
	ldp	d8, d9, [sp], #0x10
	ldp	x29, x30, [sp], #0x10
	ldp	x27, x28, [sp], #0x10
	ldp	x25, x26, [sp], #0x10
	ldp	x23, x24, [sp], #0x10
	ldp	x21, x22, [sp], #0x10
	ldp	x19, x20, [sp], #0x10
	ldp	x17, x18, [sp], #0x10
	ldp	x15, x16, [sp], #0x10
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_xts_encrypt_gb,.-vpsm4_xts_encrypt_gb
.globl	vpsm4_xts_encrypt
.type	vpsm4_xts_encrypt,%function
.align	5
vpsm4_xts_encrypt:
	AARCH64_SIGN_LINK_REGISTER
	stp	x15, x16, [sp, #-0x10]!
	stp	x17, x18, [sp, #-0x10]!
	stp	x19, x20, [sp, #-0x10]!
	stp	x21, x22, [sp, #-0x10]!
	stp	x23, x24, [sp, #-0x10]!
	stp	x25, x26, [sp, #-0x10]!
	stp	x27, x28, [sp, #-0x10]!
	stp	x29, x30, [sp, #-0x10]!
	stp	d8, d9, [sp, #-0x10]!
	stp	d10, d11, [sp, #-0x10]!
	stp	d12, d13, [sp, #-0x10]!
	stp	d14, d15, [sp, #-0x10]!
	mov	x26,x3
	mov	x27,x4
	mov	w28,w6
	ld1	{v8.4s}, [x5]
	mov	x3,x27
	adrp	x10,.Lsbox
	add	x10,x10,#:lo12:.Lsbox
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x10],#64
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x10],#64
	ld1	{v24.16b,v25.16b,v26.16b,v27.16b},[x10],#64
	ld1	{v28.16b,v29.16b,v30.16b,v31.16b},[x10]
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v8.s[0]
	mov	w13,v8.s[1]
	mov	w14,v8.s[2]
	mov	w15,v8.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v8.s[0],w15
	mov	v8.s[1],w14
	mov	v8.s[2],w13
	mov	v8.s[3],w12
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x3,x26
	and	x29,x2,#0x0F
	// convert length into blocks
	lsr	x2,x2,4
	cmp	x2,#1
	b.lt	.return

	cmp	x29,0
	// If the encryption/decryption Length is N times of 16,
	// the all blocks are encrypted/decrypted in .xts_encrypt_blocks
	b.eq	.xts_encrypt_blocks

	// If the encryption/decryption length is not N times of 16,
	// the last two blocks are encrypted/decrypted in .last_2blks_tweak or .only_2blks_tweak
	// the other blocks are encrypted/decrypted in .xts_encrypt_blocks
	subs	x2,x2,#1
	b.eq	.only_2blks_tweak
.xts_encrypt_blocks:
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	x12,v8.d[0]
	mov	x13,v8.d[1]
	mov	w7,0x87
	extr	x9,x13,x13,#32
	extr	x15,x13,x12,#63
	and	w8,w7,w9,asr#31
	eor	x14,x8,x12,lsl#1
	mov	w7,0x87
	extr	x9,x15,x15,#32
	extr	x17,x15,x14,#63
	and	w8,w7,w9,asr#31
	eor	x16,x8,x14,lsl#1
	mov	w7,0x87
	extr	x9,x17,x17,#32
	extr	x19,x17,x16,#63
	and	w8,w7,w9,asr#31
	eor	x18,x8,x16,lsl#1
	mov	w7,0x87
	extr	x9,x19,x19,#32
	extr	x21,x19,x18,#63
	and	w8,w7,w9,asr#31
	eor	x20,x8,x18,lsl#1
	mov	w7,0x87
	extr	x9,x21,x21,#32
	extr	x23,x21,x20,#63
	and	w8,w7,w9,asr#31
	eor	x22,x8,x20,lsl#1
	mov	w7,0x87
	extr	x9,x23,x23,#32
	extr	x25,x23,x22,#63
	and	w8,w7,w9,asr#31
	eor	x24,x8,x22,lsl#1
	mov	w7,0x87
	extr	x9,x25,x25,#32
	extr	x27,x25,x24,#63
	and	w8,w7,w9,asr#31
	eor	x26,x8,x24,lsl#1
.Lxts_8_blocks_process:
	cmp	x2,#8
	b.lt	.Lxts_4_blocks_process
	mov	v0.d[0],x12
	mov	v0.d[1],x13
#ifdef __AARCH64EB__
	rev32	v0.16b,v0.16b
#endif
	mov	v1.d[0],x14
	mov	v1.d[1],x15
#ifdef __AARCH64EB__
	rev32	v1.16b,v1.16b
#endif
	mov	v2.d[0],x16
	mov	v2.d[1],x17
#ifdef __AARCH64EB__
	rev32	v2.16b,v2.16b
#endif
	mov	v3.d[0],x18
	mov	v3.d[1],x19
#ifdef __AARCH64EB__
	rev32	v3.16b,v3.16b
#endif
	mov	v12.d[0],x20
	mov	v12.d[1],x21
#ifdef __AARCH64EB__
	rev32	v12.16b,v12.16b
#endif
	mov	v13.d[0],x22
	mov	v13.d[1],x23
#ifdef __AARCH64EB__
	rev32	v13.16b,v13.16b
#endif
	mov	v14.d[0],x24
	mov	v14.d[1],x25
#ifdef __AARCH64EB__
	rev32	v14.16b,v14.16b
#endif
	mov	v15.d[0],x26
	mov	v15.d[1],x27
#ifdef __AARCH64EB__
	rev32	v15.16b,v15.16b
#endif
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	eor	v4.16b, v4.16b, v0.16b
	eor	v5.16b, v5.16b, v1.16b
	eor	v6.16b, v6.16b, v2.16b
	eor	v7.16b, v7.16b, v3.16b
	ld1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	eor	v8.16b, v8.16b, v12.16b
	eor	v9.16b, v9.16b, v13.16b
	eor	v10.16b, v10.16b, v14.16b
	eor	v11.16b, v11.16b, v15.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
#ifndef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
#ifndef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifndef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
#ifndef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	zip1	v0.4s,v8.4s,v9.4s
	zip2	v1.4s,v8.4s,v9.4s
	zip1	v2.4s,v10.4s,v11.4s
	zip2	v3.4s,v10.4s,v11.4s
	zip1	v8.2d,v0.2d,v2.2d
	zip2	v9.2d,v0.2d,v2.2d
	zip1	v10.2d,v1.2d,v3.2d
	zip2	v11.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_8blks
	zip1	v8.4s,v0.4s,v1.4s
	zip2	v9.4s,v0.4s,v1.4s
	zip1	v10.4s,v2.4s,v3.4s
	zip2	v11.4s,v2.4s,v3.4s
	zip1	v0.2d,v8.2d,v10.2d
	zip2	v1.2d,v8.2d,v10.2d
	zip1	v2.2d,v9.2d,v11.2d
	zip2	v3.2d,v9.2d,v11.2d
	zip1	v8.4s,v4.4s,v5.4s
	zip2	v9.4s,v4.4s,v5.4s
	zip1	v10.4s,v6.4s,v7.4s
	zip2	v11.4s,v6.4s,v7.4s
	zip1	v4.2d,v8.2d,v10.2d
	zip2	v5.2d,v8.2d,v10.2d
	zip1	v6.2d,v9.2d,v11.2d
	zip2	v7.2d,v9.2d,v11.2d
	mov	v12.d[0],x12
	mov	v12.d[1],x13
#ifdef __AARCH64EB__
	rev32	v12.16b,v12.16b
#endif
	mov	w7,0x87
	extr	x9,x27,x27,#32
	extr	x13,x27,x26,#63
	and	w8,w7,w9,asr#31
	eor	x12,x8,x26,lsl#1
	mov	v13.d[0],x14
	mov	v13.d[1],x15
#ifdef __AARCH64EB__
	rev32	v13.16b,v13.16b
#endif
	mov	w7,0x87
	extr	x9,x13,x13,#32
	extr	x15,x13,x12,#63
	and	w8,w7,w9,asr#31
	eor	x14,x8,x12,lsl#1
	mov	v14.d[0],x16
	mov	v14.d[1],x17
#ifdef __AARCH64EB__
	rev32	v14.16b,v14.16b
#endif
	mov	w7,0x87
	extr	x9,x15,x15,#32
	extr	x17,x15,x14,#63
	and	w8,w7,w9,asr#31
	eor	x16,x8,x14,lsl#1
	mov	v15.d[0],x18
	mov	v15.d[1],x19
#ifdef __AARCH64EB__
	rev32	v15.16b,v15.16b
#endif
	mov	w7,0x87
	extr	x9,x17,x17,#32
	extr	x19,x17,x16,#63
	and	w8,w7,w9,asr#31
	eor	x18,x8,x16,lsl#1
	mov	v8.d[0],x20
	mov	v8.d[1],x21
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	w7,0x87
	extr	x9,x19,x19,#32
	extr	x21,x19,x18,#63
	and	w8,w7,w9,asr#31
	eor	x20,x8,x18,lsl#1
	mov	v9.d[0],x22
	mov	v9.d[1],x23
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	w7,0x87
	extr	x9,x21,x21,#32
	extr	x23,x21,x20,#63
	and	w8,w7,w9,asr#31
	eor	x22,x8,x20,lsl#1
	mov	v10.d[0],x24
	mov	v10.d[1],x25
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	mov	w7,0x87
	extr	x9,x23,x23,#32
	extr	x25,x23,x22,#63
	and	w8,w7,w9,asr#31
	eor	x24,x8,x22,lsl#1
	mov	v11.d[0],x26
	mov	v11.d[1],x27
#ifdef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	mov	w7,0x87
	extr	x9,x25,x25,#32
	extr	x27,x25,x24,#63
	and	w8,w7,w9,asr#31
	eor	x26,x8,x24,lsl#1
	eor	v0.16b, v0.16b, v12.16b
	eor	v1.16b, v1.16b, v13.16b
	eor	v2.16b, v2.16b, v14.16b
	eor	v3.16b, v3.16b, v15.16b
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
	eor	v7.16b, v7.16b, v11.16b

	// save the last tweak
	st1	{v11.4s},[x5]
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x1],#64
	subs	x2,x2,#8
	b.gt	.Lxts_8_blocks_process
	b	100f
.Lxts_4_blocks_process:
	mov	v8.d[0],x12
	mov	v8.d[1],x13
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	v9.d[0],x14
	mov	v9.d[1],x15
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	v10.d[0],x16
	mov	v10.d[1],x17
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	mov	v11.d[0],x18
	mov	v11.d[1],x19
#ifdef __AARCH64EB__
	rev32	v11.16b,v11.16b
#endif
	cmp	x2,#4
	b.lt	1f
	ld1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
	eor	v7.16b, v7.16b, v11.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
#ifndef __AARCH64EB__
	rev32	v7.16b,v7.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	eor	v2.16b, v2.16b, v10.16b
	eor	v3.16b, v3.16b, v11.16b
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x1],#64
	sub	x2,x2,#4
	mov	v8.d[0],x20
	mov	v8.d[1],x21
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	v9.d[0],x22
	mov	v9.d[1],x23
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	v10.d[0],x24
	mov	v10.d[1],x25
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	// save the last tweak
	st1	{v11.4s},[x5]
1:
	// process last block
	cmp	x2,#1
	b.lt	100f
	b.gt	1f
	ld1	{v4.4s},[x0],#16
	eor	v4.16b, v4.16b, v8.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v8.16b
	st1	{v4.4s},[x1],#16
	// save the last tweak
	st1	{v8.4s},[x5]
	b	100f
1:	//	process last 2 blocks
	cmp	x2,#2
	b.gt	1f
	ld1	{v4.4s,v5.4s},[x0],#32
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	st1	{v0.4s,v1.4s},[x1],#32
	// save the last tweak
	st1	{v9.4s},[x5]
	b	100f
1:	//	process last 3 blocks
	ld1	{v4.4s,v5.4s,v6.4s},[x0],#48
	eor	v4.16b, v4.16b, v8.16b
	eor	v5.16b, v5.16b, v9.16b
	eor	v6.16b, v6.16b, v10.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
#ifndef __AARCH64EB__
	rev32	v5.16b,v5.16b
#endif
#ifndef __AARCH64EB__
	rev32	v6.16b,v6.16b
#endif
	zip1	v0.4s,v4.4s,v5.4s
	zip2	v1.4s,v4.4s,v5.4s
	zip1	v2.4s,v6.4s,v7.4s
	zip2	v3.4s,v6.4s,v7.4s
	zip1	v4.2d,v0.2d,v2.2d
	zip2	v5.2d,v0.2d,v2.2d
	zip1	v6.2d,v1.2d,v3.2d
	zip2	v7.2d,v1.2d,v3.2d
	bl	_vpsm4_enc_4blks
	zip1	v4.4s,v0.4s,v1.4s
	zip2	v5.4s,v0.4s,v1.4s
	zip1	v6.4s,v2.4s,v3.4s
	zip2	v7.4s,v2.4s,v3.4s
	zip1	v0.2d,v4.2d,v6.2d
	zip2	v1.2d,v4.2d,v6.2d
	zip1	v2.2d,v5.2d,v7.2d
	zip2	v3.2d,v5.2d,v7.2d
	eor	v0.16b, v0.16b, v8.16b
	eor	v1.16b, v1.16b, v9.16b
	eor	v2.16b, v2.16b, v10.16b
	st1	{v0.4s,v1.4s,v2.4s},[x1],#48
	// save the last tweak
	st1	{v10.4s},[x5]
100:
	cmp	x29,0
	b.eq	.return

// This branch calculates the last two tweaks, 
// while the encryption/decryption length is larger than 32
.last_2blks_tweak:
	ld1	{v8.4s},[x5]
#ifdef __AARCH64EB__
	rev32	v8.16b,v8.16b
#endif
	mov	v2.16b,v8.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v9.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v9.16b, v9.16b, v1.16b
	mov	v2.16b,v9.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v10.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v10.16b, v10.16b, v1.16b
	b	.check_dec


// This branch calculates the last two tweaks, 
// while the encryption/decryption length is equal to 32, who only need two tweaks
.only_2blks_tweak:
	mov	v9.16b,v8.16b
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
	mov	v2.16b,v9.16b
	adrp	x10,.Lxts_magic
	ldr	q0, [x10, #:lo12:.Lxts_magic]
	shl	v10.16b, v2.16b, #1
	ext	v1.16b, v2.16b, v2.16b,#15
	ushr	v1.16b, v1.16b, #7
	mul	v1.16b, v1.16b, v0.16b
	eor	v10.16b, v10.16b, v1.16b
	b	.check_dec


// Determine whether encryption or decryption is required.
// The last two tweaks need to be swapped for decryption.
.check_dec:
	// encryption:1 decryption:0
	cmp	w28,1
	b.eq	.process_last_2blks
	mov	v0.16B,v9.16b
	mov	v9.16B,v10.16b
	mov	v10.16B,v0.16b

.process_last_2blks:
#ifdef __AARCH64EB__
	rev32	v9.16b,v9.16b
#endif
#ifdef __AARCH64EB__
	rev32	v10.16b,v10.16b
#endif
	ld1	{v4.4s},[x0],#16
	eor	v4.16b, v4.16b, v9.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v9.16b
	st1	{v4.4s},[x1],#16

	sub	x26,x1,16
.loop:
	subs	x29,x29,1
	ldrb	w7,[x26,x29]
	ldrb	w8,[x0,x29]
	strb	w8,[x26,x29]
	strb	w7,[x1,x29]
	b.gt	.loop
	ld1	{v4.4s}, [x26]
	eor	v4.16b, v4.16b, v10.16b
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	mov	x10,x3
	mov	w11,#8
	mov	w12,v4.s[0]
	mov	w13,v4.s[1]
	mov	w14,v4.s[2]
	mov	w15,v4.s[3]
10:
	ldp	w7,w8,[x10],8
	// B0 ^= SBOX(B1 ^ B2 ^ B3 ^ RK0)
	eor	w6,w14,w15
	eor	w9,w7,w13
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w12,w12,w6
	// B1 ^= SBOX(B0 ^ B2 ^ B3 ^ RK1)
	eor	w6,w14,w15
	eor	w9,w12,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	ldp	w7,w8,[x10],8
	eor	w13,w13,w6
	// B2 ^= SBOX(B0 ^ B1 ^ B3 ^ RK2)
	eor	w6,w12,w13
	eor	w9,w7,w15
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w14,w14,w6
	// B3 ^= SBOX(B0 ^ B1 ^ B2 ^ RK3)
	eor	w6,w12,w13
	eor	w9,w14,w8
	eor	w6,w6,w9
	movi	v1.16b,#64
	movi	v2.16b,#128
	movi	v3.16b,#192
	mov	v0.s[0],w6

	sub	v1.16b,v0.16b,v1.16b
	sub	v2.16b,v0.16b,v2.16b
	sub	v3.16b,v0.16b,v3.16b

	tbl	v0.16b,{v16.16b,v17.16b,v18.16b,v19.16b},v0.16b
	tbl	v1.16b,{v20.16b,v21.16b,v22.16b,v23.16b},v1.16b
	tbl	v2.16b,{v24.16b,v25.16b,v26.16b,v27.16b},v2.16b
	tbl	v3.16b,{v28.16b,v29.16b,v30.16b,v31.16b},v3.16b

	mov	w6,v0.s[0]
	mov	w7,v1.s[0]
	mov	w9,v2.s[0]
	add	w7,w6,w7
	mov	w6,v3.s[0]
	add	w7,w7,w9
	add	w7,w7,w6

	eor	w6,w7,w7,ror #32-2
	eor	w6,w6,w7,ror #32-10
	eor	w6,w6,w7,ror #32-18
	eor	w6,w6,w7,ror #32-24
	eor	w15,w15,w6
	subs	w11,w11,#1
	b.ne	10b
	mov	v4.s[0],w15
	mov	v4.s[1],w14
	mov	v4.s[2],w13
	mov	v4.s[3],w12
#ifndef __AARCH64EB__
	rev32	v4.16b,v4.16b
#endif
	eor	v4.16b, v4.16b, v10.16b
	st1	{v4.4s}, [x26]
.return:
	ldp	d14, d15, [sp], #0x10
	ldp	d12, d13, [sp], #0x10
	ldp	d10, d11, [sp], #0x10
	ldp	d8, d9, [sp], #0x10
	ldp	x29, x30, [sp], #0x10
	ldp	x27, x28, [sp], #0x10
	ldp	x25, x26, [sp], #0x10
	ldp	x23, x24, [sp], #0x10
	ldp	x21, x22, [sp], #0x10
	ldp	x19, x20, [sp], #0x10
	ldp	x17, x18, [sp], #0x10
	ldp	x15, x16, [sp], #0x10
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	vpsm4_xts_encrypt,.-vpsm4_xts_encrypt
