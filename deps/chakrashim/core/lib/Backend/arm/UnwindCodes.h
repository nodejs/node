//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// ARMNT Unwind codes
//
// 7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   B3   B4   Opsize  Explanation
//------------------------------------------------------------------------------

// 0 x x x x x x x |                 |    |    | 16     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_7B,                           16,    0x00000000, 0x80000000, 1 )

// 1 0 L x x x x x | x x x x x x x x |    |    | 32     | pop {r0-r12, lr} (X=bitmask)
UNWIND_CODE( POP_BITMASK,                        32,    0x80000000, 0xc0000000, 2 )

// 1 1 0 0 x x x x |                 |    |    | 16     | mov sp, rX (X=0-15)
UNWIND_CODE( MOV_SP,                             16,    0xc0000000, 0xf0000000, 1 )

// 1 1 0 1 0 L x x |                 |    |    | 16     | pop {r4-rX, lr} (X=4-7)
UNWIND_CODE( POP_RANGE,                          16,    0xd0000000, 0xf8000000, 1 )

// 1 1 0 1 1 L x x |                 |    |    | 32     | pop {r4-rX, lr} (X=8-11)
UNWIND_CODE( POP_RANGE,                          32,    0xd8000000, 0xf8000000, 1 )

// 1 1 1 0 0 x x x |                 |    |    | 32     | vpop {d8-dX} (X=8-15)
UNWIND_CODE( VPOP,                               32,    0xe0000000, 0xf8000000, 1 )

// 1 1 1 0 1 0 x x | x x x x x x x x |    |    | 32     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_10B,                          32,    0xe8000000, 0xfc000000, 2 )

// 1 1 1 0 1 1 0 L | x x x x x x x x |    |    | 16     | pop {r0-r7,lr} (X=bitmask)
UNWIND_CODE( POP_BITMASK,                        16,    0xec000000, 0xfe000000, 2 )

// 1 1 1 0 1 1 1 0 | 0 0 0 0 x x x x |    |    | 16     | Microsoft-specific
// reserved

// 1 1 1 0 1 1 1 0 | x x x x y y y y |    |    | 16     | available (X = 1-15)
// reserved

// 1 1 1 0 1 1 1 1 | 0 0 0 0 x x x x |    |    | 32     | ldr lr, [sp], #X*4
UNWIND_CODE( POP_LR,                             32,    0xef000000, 0xfff00000, 2 )

// 1 1 1 0 1 1 1 1 | x x x x y y y y |    |    | 32     | available (X = 1-15)
// reserved

// 1 1 1 1 0 x x x |                 |    |    | --     | available (X = 0-4)
// reserved

// 1 1 1 1 0 1 0 1 | s s s s e e e e |    |    | 32     | vpop {dS-dE}
UNWIND_CODE( VPOP_RANGE,                         32,    0xf5000000, 0xff000000, 2 )

// 1 1 1 1 0 1 1 0 | s s s s e e e e |    |    | 32     | vpop {d(S+16)-d(E+16)}
UNWIND_CODE( VPOP_HIRANGE,                       32,    0xf6000000, 0xff000000, 2 )

// 1 1 1 1 0 1 1 1 | x x x x x x x x | x  |    | 16     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_16B,                          16,    0xf7000000, 0xff000000, 3 )

// 1 1 1 1 1 0 0 0 | x x x x x x x x | x  | x  | 16     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_24B,                          16,    0xf8000000, 0xff000000, 4 )

// 1 1 1 1 1 0 0 1 | x x x x x x x x | x  |    | 32     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_16B,                          32,    0xf9000000, 0xff000000, 3 )

// 1 1 1 1 1 0 1 0 | x x x x x x x x | x  | x  | 32     | add sp, sp, #X*4
UNWIND_CODE( ALLOC_24B,                          32,    0xfa000000, 0xff000000, 4 )

// 1 1 1 1 1 0 1 1 |                 |    |    | 16     | 16bit nop
UNWIND_CODE( NOP,                                16,    0xfb000000, 0xff000000, 1 )

// 1 1 1 1 1 1 0 0 |                 |    |    | 32     | 32bit nop
UNWIND_CODE( NOP,                                32,    0xfc000000, 0xff000000, 1 )

// 1 1 1 1 1 1 0 1 |                 |    |    | 16     | end + 16bit nop in epilogue
UNWIND_CODE( END_EX,                             16,    0xfd000000, 0xff000000, 1 )

// 1 1 1 1 1 1 1 0 |                 |    |    | 32     | end + 32bit nop in epilogue
UNWIND_CODE( END_EX,                             32,    0xfe000000, 0xff000000, 1 )

// 1 1 1 1 1 1 1 1 |                 |    |    | 00     | end
UNWIND_CODE( END,                                00,    0xff000000, 0xff000000, 1 )

// this is the last template
UNWIND_CODE( INVALID,                            00,    0xbaadf00d, 0xffffffff, 0 )


