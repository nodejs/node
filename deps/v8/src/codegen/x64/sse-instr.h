// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_SSE_INSTR_H_
#define V8_CODEGEN_X64_SSE_INSTR_H_

// SSE instructions whose AVX version has two operands.
#define SSE_UNOP_INSTRUCTION_LIST(V) \
  V(sqrtps, 0F, 51)                  \
  V(rsqrtps, 0F, 52)                 \
  V(rcpps, 0F, 53)                   \
  V(cvtdq2ps, 0F, 5B)

// SSE instructions whose AVX version has three operands.
#define SSE_BINOP_INSTRUCTION_LIST(V) \
  V(andps, 0F, 54)                    \
  V(andnps, 0F, 55)                   \
  V(orps, 0F, 56)                     \
  V(xorps, 0F, 57)                    \
  V(addps, 0F, 58)                    \
  V(mulps, 0F, 59)                    \
  V(subps, 0F, 5C)                    \
  V(minps, 0F, 5D)                    \
  V(divps, 0F, 5E)                    \
  V(maxps, 0F, 5F)

// Instructions dealing with scalar single-precision values.
#define SSE_INSTRUCTION_LIST_SS(V) \
  V(sqrtss, F3, 0F, 51)            \
  V(addss, F3, 0F, 58)             \
  V(mulss, F3, 0F, 59)             \
  V(subss, F3, 0F, 5C)             \
  V(minss, F3, 0F, 5D)             \
  V(divss, F3, 0F, 5E)             \
  V(maxss, F3, 0F, 5F)

#define SSE2_INSTRUCTION_LIST(V) \
  V(andpd, 66, 0F, 54)           \
  V(andnpd, 66, 0F, 55)          \
  V(orpd, 66, 0F, 56)            \
  V(xorpd, 66, 0F, 57)           \
  V(addpd, 66, 0F, 58)           \
  V(mulpd, 66, 0F, 59)           \
  V(subpd, 66, 0F, 5C)           \
  V(minpd, 66, 0F, 5D)           \
  V(maxpd, 66, 0F, 5F)           \
  V(divpd, 66, 0F, 5E)           \
  V(punpcklbw, 66, 0F, 60)       \
  V(punpcklwd, 66, 0F, 61)       \
  V(punpckldq, 66, 0F, 62)       \
  V(packsswb, 66, 0F, 63)        \
  V(packuswb, 66, 0F, 67)        \
  V(punpckhbw, 66, 0F, 68)       \
  V(punpckhwd, 66, 0F, 69)       \
  V(punpckhdq, 66, 0F, 6A)       \
  V(packssdw, 66, 0F, 6B)        \
  V(punpcklqdq, 66, 0F, 6C)      \
  V(punpckhqdq, 66, 0F, 6D)      \
  V(paddb, 66, 0F, FC)           \
  V(paddw, 66, 0F, FD)           \
  V(paddd, 66, 0F, FE)           \
  V(paddq, 66, 0F, D4)           \
  V(paddsb, 66, 0F, EC)          \
  V(paddsw, 66, 0F, ED)          \
  V(paddusb, 66, 0F, DC)         \
  V(paddusw, 66, 0F, DD)         \
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
  V(pmuludq, 66, 0F, F4)         \
  V(psllw, 66, 0F, F1)           \
  V(pslld, 66, 0F, F2)           \
  V(psllq, 66, 0F, F3)           \
  V(pavgb, 66, 0F, E0)           \
  V(psraw, 66, 0F, E1)           \
  V(psrad, 66, 0F, E2)           \
  V(pavgw, 66, 0F, E3)           \
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
  V(pand, 66, 0F, DB)            \
  V(por, 66, 0F, EB)             \
  V(pxor, 66, 0F, EF)

// SSE2 instructions whose AVX version has two operands.
#define SSE2_UNOP_INSTRUCTION_LIST(V) \
  V(sqrtpd, 66, 0F, 51)               \
  V(cvtps2dq, 66, 0F, 5B)

// SSE2 shift instructions with an immediate operand. The last element is the
// extension to the opcode.
#define SSE2_INSTRUCTION_LIST_SHIFT_IMM(V) \
  V(psrlw, 66, 0F, 71, 2)                  \
  V(psrld, 66, 0F, 72, 2)                  \
  V(psrlq, 66, 0F, 73, 2)                  \
  V(psraw, 66, 0F, 71, 4)                  \
  V(psrad, 66, 0F, 72, 4)                  \
  V(psllw, 66, 0F, 71, 6)                  \
  V(pslld, 66, 0F, 72, 6)                  \
  V(psllq, 66, 0F, 73, 6)

// Instructions dealing with scalar double-precision values.
#define SSE2_INSTRUCTION_LIST_SD(V) \
  V(sqrtsd, F2, 0F, 51)             \
  V(addsd, F2, 0F, 58)              \
  V(mulsd, F2, 0F, 59)              \
  V(cvtsd2ss, F2, 0F, 5A)           \
  V(subsd, F2, 0F, 5C)              \
  V(minsd, F2, 0F, 5D)              \
  V(divsd, F2, 0F, 5E)              \
  V(maxsd, F2, 0F, 5F)

#define SSSE3_INSTRUCTION_LIST(V) \
  V(phaddd, 66, 0F, 38, 02)       \
  V(phaddw, 66, 0F, 38, 01)       \
  V(pshufb, 66, 0F, 38, 00)       \
  V(psignb, 66, 0F, 38, 08)       \
  V(psignw, 66, 0F, 38, 09)       \
  V(psignd, 66, 0F, 38, 0A)

// SSSE3 instructions whose AVX version has two operands.
#define SSSE3_UNOP_INSTRUCTION_LIST(V) \
  V(pabsb, 66, 0F, 38, 1C)             \
  V(pabsw, 66, 0F, 38, 1D)             \
  V(pabsd, 66, 0F, 38, 1E)

#define SSE4_INSTRUCTION_LIST(V) \
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

// SSE instructions whose AVX version has two operands.
#define SSE4_UNOP_INSTRUCTION_LIST(V) \
  V(ptest, 66, 0F, 38, 17)            \
  V(pmovsxbw, 66, 0F, 38, 20)         \
  V(pmovsxwd, 66, 0F, 38, 23)         \
  V(pmovsxdq, 66, 0F, 38, 25)         \
  V(pmovzxbw, 66, 0F, 38, 30)         \
  V(pmovzxwd, 66, 0F, 38, 33)         \
  V(pmovzxdq, 66, 0F, 38, 35)

#define SSE4_EXTRACT_INSTRUCTION_LIST(V) \
  V(extractps, 66, 0F, 3A, 17)           \
  V(pextrb, 66, 0F, 3A, 14)              \
  V(pextrw, 66, 0F, 3A, 15)              \
  V(pextrd, 66, 0F, 3A, 16)

#define SSE4_2_INSTRUCTION_LIST(V) V(pcmpgtq, 66, 0F, 38, 37)

#endif  // V8_CODEGEN_X64_SSE_INSTR_H_
