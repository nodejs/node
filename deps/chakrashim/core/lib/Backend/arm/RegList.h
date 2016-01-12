//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Internal  Listing  Machine
// Name      Name     Encode    Type        BitVec
//------------------------------------------------------------------------

// Illegal registers - must be first and have a value of 0

//           Internal Name
//          /      Listing Name
//         /      /        Machine Encode
//        /      /        /       Type
//       /      /        /       /           BitVec
//      /      /        /       /           /
REGDAT(NOREG, noreg,    0xf,   TyIllegal,  RA_DONTALLOCATE)

// 32 bit Integer Registers
REGDAT(R0,      r0,       0,   TyInt32,    0)
REGDAT(R1,      r1,       1,   TyInt32,    0)
REGDAT(R2,      r2,       2,   TyInt32,    0)
REGDAT(R3,      r3,       3,   TyInt32,    0)
REGDAT(R4,      r4,       4,   TyInt32,    RA_CALLEESAVE)
REGDAT(R5,      r5,       5,   TyInt32,    RA_CALLEESAVE)
REGDAT(R6,      r6,       6,   TyInt32,    RA_CALLEESAVE)
REGDAT(R7,      r7,       7,   TyInt32,    RA_CALLEESAVE)
REGDAT(R8,      r8,       8,   TyInt32,    RA_CALLEESAVE)
REGDAT(R9,      r9,       9,   TyInt32,    RA_CALLEESAVE)
REGDAT(R10,     r10,      10,  TyInt32,    RA_CALLEESAVE)
REGDAT(R11,     r11,      11,  TyInt32,    RA_DONTALLOCATE) //Frame Pointer
REGDAT(R12,     r12,      12,  TyInt32,    RA_DONTALLOCATE) // ?? Why is this marked RA_DONTTRACK in UTC?
REGDAT(SP,      sp,       13,  TyInt32,    RA_DONTALLOCATE)
REGDAT(LR,      lr,       14,  TyInt32,    0)
REGDAT(PC,      pc,       15,  TyInt32,    RA_DONTALLOCATE)

// VFP Floating Point Registers
REGDAT(D0,      d0,       0,    TyFloat64,  0)
REGDAT(D1,      d1,       1,    TyFloat64,  0)
REGDAT(D2,      d2,       2,    TyFloat64,  0)
REGDAT(D3,      d3,       3,    TyFloat64,  0)
REGDAT(D4,      d4,       4,    TyFloat64,  0)
REGDAT(D5,      d5,       5,    TyFloat64,  0)
REGDAT(D6,      d6,       6,    TyFloat64,  0)
REGDAT(D7,      d7,       7,    TyFloat64,  0)
REGDAT(D8,      d8,       8,    TyFloat64,  RA_CALLEESAVE)
REGDAT(D9,      d9,       9,    TyFloat64,  RA_CALLEESAVE)
REGDAT(D10,     d10,      10,   TyFloat64,  RA_CALLEESAVE)
REGDAT(D11,     d11,      11,   TyFloat64,  RA_CALLEESAVE)
REGDAT(D12,     d12,      12,   TyFloat64,  RA_CALLEESAVE)
REGDAT(D13,     d13,      13,   TyFloat64,  RA_CALLEESAVE)
REGDAT(D14,     d14,      14,   TyFloat64,  RA_CALLEESAVE)
REGDAT(D15,     d15,      15,   TyFloat64,  RA_CALLEESAVE)

#define FIRST_DOUBLE_ARG_REG RegD0
#define LAST_DOUBLE_REG RegD15
#define LAST_DOUBLE_REG_NUM 15
#define LAST_FLOAT_REG_NUM 30
#define FIRST_DOUBLE_CALLEE_SAVED_REG_NUM 8
#define LAST_DOUBLE_CALLEE_SAVED_REG_NUM 15
#define VFP_REGCOUNT 16

#define REGNUM_ISVFPREG(r) ((r) >= RegD0 && (r) <= LAST_DOUBLE_REG)

