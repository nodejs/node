//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Illegal registers - must be first and have a value of 0

//           Internal Name
//          /      Listing Name
//         /      /        Machine Encode
//        /      /        /       Type
//       /      /        /       /             BitVec
//      /      /        /       /             /
REGDAT(NOREG, noreg,    0xf,    TyIllegal,    RA_DONTALLOCATE)

// 32 bit Integer Registers
REGDAT(EAX,  eax,       0,      TyInt32,      RA_BYTEABLE)
REGDAT(ECX,  ecx,       1,      TyInt32,      RA_BYTEABLE)
REGDAT(EDX,  edx,       2,      TyInt32,      RA_BYTEABLE)
REGDAT(EBX,  ebx,       3,      TyInt32,      RA_CALLEESAVE|RA_BYTEABLE)
REGDAT(ESP,  esp,       4,      TyInt32,      RA_DONTALLOCATE)
REGDAT(EBP,  ebp,       5,      TyInt32,      RA_DONTALLOCATE)
REGDAT(ESI,  esi,       6,      TyInt32,      RA_CALLEESAVE)
REGDAT(EDI,  edi,       7,      TyInt32,      RA_CALLEESAVE)

REGDAT(XMM0, xmm0,      0,      TyFloat64,    0)
REGDAT(XMM1, xmm1,      1,      TyFloat64,    0)
REGDAT(XMM2, xmm2,      2,      TyFloat64,    0)
REGDAT(XMM3, xmm3,      3,      TyFloat64,    0)
REGDAT(XMM4, xmm4,      4,      TyFloat64,    0)
REGDAT(XMM5, xmm5,      5,      TyFloat64,    0)
REGDAT(XMM6, xmm6,      6,      TyFloat64,    0)
REGDAT(XMM7, xmm7,      7,      TyFloat64,    0)
