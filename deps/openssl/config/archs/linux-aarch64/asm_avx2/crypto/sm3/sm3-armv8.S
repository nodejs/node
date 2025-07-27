// Copyright 2021-2025 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html
//
// This module implements support for Armv8 SM3 instructions

// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arm_arch.h"
.text
.globl	ossl_hwsm3_block_data_order
.type	ossl_hwsm3_block_data_order,%function
.align	5
ossl_hwsm3_block_data_order:
	AARCH64_VALID_CALL_TARGET
	// load state
	ld1	{v5.4s,v6.4s}, [x0]
	rev64	v5.4s, v5.4s
	rev64	v6.4s, v6.4s
	ext	v5.16b, v5.16b, v5.16b, #8
	ext	v6.16b, v6.16b, v6.16b, #8
	adrp	x8, .Tj
	add	x8, x8, #:lo12:.Tj
	ldp	s16, s17, [x8]

.Loop:
	// load input
	ld1	{v0.4s,v1.4s,v2.4s,v3.4s}, [x1], #64
	sub	w2, w2, #1

	mov	v18.16b, v5.16b
	mov	v19.16b, v6.16b

#ifndef __AARCH64EB__
	rev32	v0.16b, v0.16b
	rev32	v1.16b, v1.16b
	rev32	v2.16b, v2.16b
	rev32	v3.16b, v3.16b
#endif

	ext	v20.16b, v16.16b, v16.16b, #4
	// s4 = w7  | w8  | w9  | w10
	ext	v4.16b, v1.16b, v2.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v0.16b, v1.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v2.16b, v3.16b, #8
.inst	0xce63c004	//sm3partw1 v4.4s, v0.4s, v3.4s
.inst	0xce76c6e4	//sm3partw2 v4.4s, v23.4s, v22.4s
	eor	v22.16b, v0.16b, v1.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5682e5	//sm3tt1a v5.4s, v23.4s, v22.4s[0]
.inst	0xce408ae6	//sm3tt2a v6.4s, v23.4s, v0.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5692e5	//sm3tt1a v5.4s, v23.4s, v22.4s[1]
.inst	0xce409ae6	//sm3tt2a v6.4s, v23.4s, v0.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[2]
.inst	0xce40aae6	//sm3tt2a v6.4s, v23.4s, v0.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[3]
.inst	0xce40bae6	//sm3tt2a v6.4s, v23.4s, v0.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v0.16b, v2.16b, v3.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v1.16b, v2.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v3.16b, v4.16b, #8
.inst	0xce64c020	//sm3partw1 v0.4s, v1.4s, v4.4s
.inst	0xce76c6e0	//sm3partw2 v0.4s, v23.4s, v22.4s
	eor	v22.16b, v1.16b, v2.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5682e5	//sm3tt1a v5.4s, v23.4s, v22.4s[0]
.inst	0xce418ae6	//sm3tt2a v6.4s, v23.4s, v1.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5692e5	//sm3tt1a v5.4s, v23.4s, v22.4s[1]
.inst	0xce419ae6	//sm3tt2a v6.4s, v23.4s, v1.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[2]
.inst	0xce41aae6	//sm3tt2a v6.4s, v23.4s, v1.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[3]
.inst	0xce41bae6	//sm3tt2a v6.4s, v23.4s, v1.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v1.16b, v3.16b, v4.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v2.16b, v3.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v4.16b, v0.16b, #8
.inst	0xce60c041	//sm3partw1 v1.4s, v2.4s, v0.4s
.inst	0xce76c6e1	//sm3partw2 v1.4s, v23.4s, v22.4s
	eor	v22.16b, v2.16b, v3.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5682e5	//sm3tt1a v5.4s, v23.4s, v22.4s[0]
.inst	0xce428ae6	//sm3tt2a v6.4s, v23.4s, v2.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5692e5	//sm3tt1a v5.4s, v23.4s, v22.4s[1]
.inst	0xce429ae6	//sm3tt2a v6.4s, v23.4s, v2.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[2]
.inst	0xce42aae6	//sm3tt2a v6.4s, v23.4s, v2.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[3]
.inst	0xce42bae6	//sm3tt2a v6.4s, v23.4s, v2.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v2.16b, v4.16b, v0.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v3.16b, v4.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v0.16b, v1.16b, #8
.inst	0xce61c062	//sm3partw1 v2.4s, v3.4s, v1.4s
.inst	0xce76c6e2	//sm3partw2 v2.4s, v23.4s, v22.4s
	eor	v22.16b, v3.16b, v4.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5682e5	//sm3tt1a v5.4s, v23.4s, v22.4s[0]
.inst	0xce438ae6	//sm3tt2a v6.4s, v23.4s, v3.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5692e5	//sm3tt1a v5.4s, v23.4s, v22.4s[1]
.inst	0xce439ae6	//sm3tt2a v6.4s, v23.4s, v3.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[2]
.inst	0xce43aae6	//sm3tt2a v6.4s, v23.4s, v3.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b2e5	//sm3tt1a v5.4s, v23.4s, v22.4s[3]
.inst	0xce43bae6	//sm3tt2a v6.4s, v23.4s, v3.4s[3]
	ext	v20.16b, v17.16b, v17.16b, #4
	// s4 = w7  | w8  | w9  | w10
	ext	v3.16b, v0.16b, v1.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v4.16b, v0.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v1.16b, v2.16b, #8
.inst	0xce62c083	//sm3partw1 v3.4s, v4.4s, v2.4s
.inst	0xce76c6e3	//sm3partw2 v3.4s, v23.4s, v22.4s
	eor	v22.16b, v4.16b, v0.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce448ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce449ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce44aee6	//sm3tt2b v6.4s, v23.4s, v4.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce44bee6	//sm3tt2b v6.4s, v23.4s, v4.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v4.16b, v1.16b, v2.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v0.16b, v1.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v2.16b, v3.16b, #8
.inst	0xce63c004	//sm3partw1 v4.4s, v0.4s, v3.4s
.inst	0xce76c6e4	//sm3partw2 v4.4s, v23.4s, v22.4s
	eor	v22.16b, v0.16b, v1.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce408ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce409ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce40aee6	//sm3tt2b v6.4s, v23.4s, v0.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce40bee6	//sm3tt2b v6.4s, v23.4s, v0.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v0.16b, v2.16b, v3.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v1.16b, v2.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v3.16b, v4.16b, #8
.inst	0xce64c020	//sm3partw1 v0.4s, v1.4s, v4.4s
.inst	0xce76c6e0	//sm3partw2 v0.4s, v23.4s, v22.4s
	eor	v22.16b, v1.16b, v2.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce418ee6	//sm3tt2b v6.4s, v23.4s, v1.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce419ee6	//sm3tt2b v6.4s, v23.4s, v1.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce41aee6	//sm3tt2b v6.4s, v23.4s, v1.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce41bee6	//sm3tt2b v6.4s, v23.4s, v1.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v1.16b, v3.16b, v4.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v2.16b, v3.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v4.16b, v0.16b, #8
.inst	0xce60c041	//sm3partw1 v1.4s, v2.4s, v0.4s
.inst	0xce76c6e1	//sm3partw2 v1.4s, v23.4s, v22.4s
	eor	v22.16b, v2.16b, v3.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce428ee6	//sm3tt2b v6.4s, v23.4s, v2.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce429ee6	//sm3tt2b v6.4s, v23.4s, v2.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce42aee6	//sm3tt2b v6.4s, v23.4s, v2.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce42bee6	//sm3tt2b v6.4s, v23.4s, v2.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v2.16b, v4.16b, v0.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v3.16b, v4.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v0.16b, v1.16b, #8
.inst	0xce61c062	//sm3partw1 v2.4s, v3.4s, v1.4s
.inst	0xce76c6e2	//sm3partw2 v2.4s, v23.4s, v22.4s
	eor	v22.16b, v3.16b, v4.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce438ee6	//sm3tt2b v6.4s, v23.4s, v3.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce439ee6	//sm3tt2b v6.4s, v23.4s, v3.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce43aee6	//sm3tt2b v6.4s, v23.4s, v3.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce43bee6	//sm3tt2b v6.4s, v23.4s, v3.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v3.16b, v0.16b, v1.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v4.16b, v0.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v1.16b, v2.16b, #8
.inst	0xce62c083	//sm3partw1 v3.4s, v4.4s, v2.4s
.inst	0xce76c6e3	//sm3partw2 v3.4s, v23.4s, v22.4s
	eor	v22.16b, v4.16b, v0.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce448ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce449ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce44aee6	//sm3tt2b v6.4s, v23.4s, v4.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce44bee6	//sm3tt2b v6.4s, v23.4s, v4.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v4.16b, v1.16b, v2.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v0.16b, v1.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v2.16b, v3.16b, #8
.inst	0xce63c004	//sm3partw1 v4.4s, v0.4s, v3.4s
.inst	0xce76c6e4	//sm3partw2 v4.4s, v23.4s, v22.4s
	eor	v22.16b, v0.16b, v1.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce408ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce409ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce40aee6	//sm3tt2b v6.4s, v23.4s, v0.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce40bee6	//sm3tt2b v6.4s, v23.4s, v0.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v0.16b, v2.16b, v3.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v1.16b, v2.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v3.16b, v4.16b, #8
.inst	0xce64c020	//sm3partw1 v0.4s, v1.4s, v4.4s
.inst	0xce76c6e0	//sm3partw2 v0.4s, v23.4s, v22.4s
	eor	v22.16b, v1.16b, v2.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce418ee6	//sm3tt2b v6.4s, v23.4s, v1.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce419ee6	//sm3tt2b v6.4s, v23.4s, v1.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce41aee6	//sm3tt2b v6.4s, v23.4s, v1.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce41bee6	//sm3tt2b v6.4s, v23.4s, v1.4s[3]
	// s4 = w7  | w8  | w9  | w10
	ext	v1.16b, v3.16b, v4.16b, #12
	// vtmp1 = w3  | w4  | w5  | w6
	ext	v22.16b, v2.16b, v3.16b, #12
	// vtmp2 = w10 | w11 | w12 | w13
	ext	v23.16b, v4.16b, v0.16b, #8
.inst	0xce60c041	//sm3partw1 v1.4s, v2.4s, v0.4s
.inst	0xce76c6e1	//sm3partw2 v1.4s, v23.4s, v22.4s
	eor	v22.16b, v2.16b, v3.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce428ee6	//sm3tt2b v6.4s, v23.4s, v2.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce429ee6	//sm3tt2b v6.4s, v23.4s, v2.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce42aee6	//sm3tt2b v6.4s, v23.4s, v2.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce42bee6	//sm3tt2b v6.4s, v23.4s, v2.4s[3]
	eor	v22.16b, v3.16b, v4.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce438ee6	//sm3tt2b v6.4s, v23.4s, v3.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce439ee6	//sm3tt2b v6.4s, v23.4s, v3.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce43aee6	//sm3tt2b v6.4s, v23.4s, v3.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce43bee6	//sm3tt2b v6.4s, v23.4s, v3.4s[3]
	eor	v22.16b, v4.16b, v0.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce448ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce449ee6	//sm3tt2b v6.4s, v23.4s, v4.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce44aee6	//sm3tt2b v6.4s, v23.4s, v4.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce44bee6	//sm3tt2b v6.4s, v23.4s, v4.4s[3]
	eor	v22.16b, v0.16b, v1.16b
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce5686e5	//sm3tt1b v5.4s, v23.4s, v22.4s[0]
.inst	0xce408ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[0]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce5696e5	//sm3tt1b v5.4s, v23.4s, v22.4s[1]
.inst	0xce409ee6	//sm3tt2b v6.4s, v23.4s, v0.4s[1]
.inst	0xce5418b7	//sm3ss1 v23.4s, v5.4s, v20.4s, v6.4s
	shl	v21.4s, v20.4s, #1
	sri	v21.4s, v20.4s, #31
.inst	0xce56a6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[2]
.inst	0xce40aee6	//sm3tt2b v6.4s, v23.4s, v0.4s[2]
.inst	0xce5518b7	//sm3ss1 v23.4s, v5.4s, v21.4s, v6.4s
	shl	v20.4s, v21.4s, #1
	sri	v20.4s, v21.4s, #31
.inst	0xce56b6e5	//sm3tt1b v5.4s, v23.4s, v22.4s[3]
.inst	0xce40bee6	//sm3tt2b v6.4s, v23.4s, v0.4s[3]
	eor	v5.16b, v5.16b, v18.16b
	eor	v6.16b, v6.16b, v19.16b

	// any remained blocks?
	cbnz	w2, .Loop

	// save state
	rev64	v5.4s, v5.4s
	rev64	v6.4s, v6.4s
	ext	v5.16b, v5.16b, v5.16b, #8
	ext	v6.16b, v6.16b, v6.16b, #8
	st1	{v5.4s,v6.4s}, [x0]
	ret
.size	ossl_hwsm3_block_data_order,.-ossl_hwsm3_block_data_order
.section	.rodata

.type	_sm3_consts,%object
.align	3
_sm3_consts:
.Tj:
.word	0x79cc4519, 0x9d8a7a87
.size	_sm3_consts,.-_sm3_consts
.previous
