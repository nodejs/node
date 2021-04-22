// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_IA32_SSE_INSTR_H_
#define V8_CODEGEN_IA32_SSE_INSTR_H_

#define SSE2_INSTRUCTION_LIST(V) \
  V(packsswb, 66, 0F, 63)        \
  V(packssdw, 66, 0F, 6B)        \
  V(packuswb, 66, 0F, 67)        \
  V(pmaddwd, 66, 0F, F5)         \
  V(paddb, 66, 0F, FC)           \
  V(paddw, 66, 0F, FD)           \
  V(paddd, 66, 0F, FE)           \
  V(paddq, 66, 0F, D4)           \
  V(paddsb, 66, 0F, EC)          \
  V(paddsw, 66, 0F, ED)          \
  V(paddusb, 66, 0F, DC)         \
  V(paddusw, 66, 0F, DD)         \
  V(pand, 66, 0F, DB)            \
  V(pandn, 66, 0F, DF)           \
  V(pcmpeqb, 66, 0F, 74)         \
  V(pcmpeqw, 66, 0F, 75)         \
  V(pcmpeqd, 66, 0F, 76)         \
  V(pcmpgtb, 66, 0F, 64)         \
  V(pcmpgtw, 66, 0F, 65)         \
  V(pcmpgtd, 66, 0F, 66)         \
  V(pmaxsw, 66, 0F, EE)          \
  V(pmaxub, 66, 0F, DE)          \
  V(pminsw, 66, 0F, EA)          \
  V(pminub, 66, 0F, DA)          \
  V(pmullw, 66, 0F, D5)          \
  V(por, 66, 0F, EB)             \
  V(psllw, 66, 0F, F1)           \
  V(pslld, 66, 0F, F2)           \
  V(psllq, 66, 0F, F3)           \
  V(pmuludq, 66, 0F, F4)         \
  V(pavgb, 66, 0F, E0)           \
  V(psraw, 66, 0F, E1)           \
  V(psrad, 66, 0F, E2)           \
  V(pavgw, 66, 0F, E3)           \
  V(pmulhuw, 66, 0F, E4)         \
  V(pmulhw, 66, 0F, E5)          \
  V(psrlw, 66, 0F, D1)           \
  V(psrld, 66, 0F, D2)           \
  V(psrlq, 66, 0F, D3)           \
  V(psubb, 66, 0F, F8)           \
  V(psubw, 66, 0F, F9)           \
  V(psubd, 66, 0F, FA)           \
  V(psubq, 66, 0F, FB)           \
  V(psubsb, 66, 0F, E8)          \
  V(psubsw, 66, 0F, E9)          \
  V(psubusb, 66, 0F, D8)         \
  V(psubusw, 66, 0F, D9)         \
  V(punpcklbw, 66, 0F, 60)       \
  V(punpcklwd, 66, 0F, 61)       \
  V(punpckldq, 66, 0F, 62)       \
  V(punpcklqdq, 66, 0F, 6C)      \
  V(punpckhbw, 66, 0F, 68)       \
  V(punpckhwd, 66, 0F, 69)       \
  V(punpckhdq, 66, 0F, 6A)       \
  V(punpckhqdq, 66, 0F, 6D)      \
  V(pxor, 66, 0F, EF)

#define SSSE3_INSTRUCTION_LIST(V) \
  V(pshufb, 66, 0F, 38, 00)       \
  V(phaddw, 66, 0F, 38, 01)       \
  V(phaddd, 66, 0F, 38, 02)       \
  V(pmaddubsw, 66, 0F, 38, 04)    \
  V(psignb, 66, 0F, 38, 08)       \
  V(psignw, 66, 0F, 38, 09)       \
  V(psignd, 66, 0F, 38, 0A)       \
  V(pmulhrsw, 66, 0F, 38, 0B)

// SSSE3 instructions whose AVX version has two operands.
#define SSSE3_UNOP_INSTRUCTION_LIST(V) \
  V(pabsb, 66, 0F, 38, 1C)             \
  V(pabsw, 66, 0F, 38, 1D)             \
  V(pabsd, 66, 0F, 38, 1E)

#define SSE4_INSTRUCTION_LIST(V) \
  V(pmuldq, 66, 0F, 38, 28)      \
  V(pcmpeqq, 66, 0F, 38, 29)     \
  V(packusdw, 66, 0F, 38, 2B)    \
  V(pminsb, 66, 0F, 38, 38)      \
  V(pminsd, 66, 0F, 38, 39)      \
  V(pminuw, 66, 0F, 38, 3A)      \
  V(pminud, 66, 0F, 38, 3B)      \
  V(pmaxsb, 66, 0F, 38, 3C)      \
  V(pmaxsd, 66, 0F, 38, 3D)      \
  V(pmaxuw, 66, 0F, 38, 3E)      \
  V(pmaxud, 66, 0F, 38, 3F)      \
  V(pmulld, 66, 0F, 38, 40)

#define SSE4_RM_INSTRUCTION_LIST(V) \
  V(pmovsxbw, 66, 0F, 38, 20)       \
  V(pmovsxwd, 66, 0F, 38, 23)       \
  V(pmovsxdq, 66, 0F, 38, 25)       \
  V(pmovzxbw, 66, 0F, 38, 30)       \
  V(pmovzxwd, 66, 0F, 38, 33)       \
  V(pmovzxdq, 66, 0F, 38, 35)       \
  V(ptest, 66, 0F, 38, 17)

#endif  // V8_CODEGEN_IA32_SSE_INSTR_H_
