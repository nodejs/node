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

// 64 bit Integer Registers
REGDAT(X0,      x0,       0,   TyInt64,    0)
REGDAT(X1,      x1,       1,   TyInt64,    0)
REGDAT(X2,      x2,       2,   TyInt64,    0)
REGDAT(X3,      x3,       3,   TyInt64,    0)
REGDAT(X4,      x4,       4,   TyInt64,    0)
REGDAT(X5,      x5,       5,   TyInt64,    0)
REGDAT(X6,      x6,       6,   TyInt64,    0)
REGDAT(X7,      x7,       7,   TyInt64,    0)
REGDAT(X8,      x8,       8,   TyInt64,    0)
REGDAT(X9,      x9,       9,   TyInt64,    0)
REGDAT(X10,     x10,      10,  TyInt64,    0)
REGDAT(X11,     x11,      11,  TyInt64,    0)
REGDAT(X12,     x12,      12,  TyInt64,    0)
REGDAT(X13,     x13,      13,  TyInt64,    0)
REGDAT(X14,     x14,      14,  TyInt64,    0)
REGDAT(X15,     x15,      15,  TyInt64,    0)
REGDAT(X16,     x16,      16,  TyInt64,    RA_DONTALLOCATE) // ?? Why is this marked RA_DONTTRACK in UTC?
REGDAT(X17,     x17,      17,  TyInt64,    RA_DONTALLOCATE) // ?? Why is this marked RA_DONTTRACK in UTC?
REGDAT(X18,     x18,      18,  TyInt64,    RA_DONTALLOCATE) //Platform register (TEB pointer)
REGDAT(X19,     x19,      19,  TyInt64,    RA_CALLEESAVE)
REGDAT(X20,     x20,      20,  TyInt64,    RA_CALLEESAVE)
REGDAT(X21,     x21,      21,  TyInt64,    RA_CALLEESAVE)
REGDAT(X22,     x22,      22,  TyInt64,    RA_CALLEESAVE)
REGDAT(X23,     x23,      23,  TyInt64,    RA_CALLEESAVE)
REGDAT(X24,     x24,      24,  TyInt64,    RA_CALLEESAVE)
REGDAT(X25,     x25,      25,  TyInt64,    RA_CALLEESAVE)
REGDAT(X26,     x26,      26,  TyInt64,    RA_CALLEESAVE)
REGDAT(X27,     x27,      27,  TyInt64,    RA_CALLEESAVE)
REGDAT(X28,     x28,      28,  TyInt64,    RA_CALLEESAVE)
REGDAT(FP,      fp,       29,  TyInt64,    RA_DONTALLOCATE) //Frame Pointer
REGDAT(LR,      lr,       30,  TyInt64,    0)
REGDAT(SP,      sp,       31,  TyInt64,    RA_DONTALLOCATE)

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
REGDAT(D16,     d16,      16,   TyFloat64,  0)
REGDAT(D17,     d17,      17,   TyFloat64,  0)
REGDAT(D18,     d18,      18,   TyFloat64,  0)
REGDAT(D19,     d19,      19,   TyFloat64,  0)
REGDAT(D20,     d20,      20,   TyFloat64,  0)
REGDAT(D21,     d21,      21,   TyFloat64,  0)
REGDAT(D22,     d22,      22,   TyFloat64,  0)
REGDAT(D23,     d23,      23,   TyFloat64,  0)
REGDAT(D24,     d24,      24,   TyFloat64,  0)
REGDAT(D25,     d25,      25,   TyFloat64,  0)
REGDAT(D26,     d26,      26,   TyFloat64,  0)
REGDAT(D27,     d27,      27,   TyFloat64,  0)
REGDAT(D28,     d28,      28,   TyFloat64,  0)
REGDAT(D29,     d29,      29,   TyFloat64,  0)
REGDAT(D30,     d30,      30,   TyFloat64,  0)
REGDAT(D31,     d31,      31,   TyFloat64,  0)

#define FIRST_DOUBLE_ARG_REG RegD0
#define LAST_DOUBLE_REG RegD31
#define LAST_DOUBLE_REG_NUM 31
#define LAST_FLOAT_REG_NUM 31
#define FIRST_DOUBLE_CALLEE_SAVED_REG_NUM 8
#define LAST_DOUBLE_CALLEE_SAVED_REG_NUM 15
#define VFP_REGCOUNT 31

#define REGNUM_ISVFPREG(r) ((r) >= RegD0 && (r) <= LAST_DOUBLE_REG)

