// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_SHARED_IA32_X64_MACRO_ASSEMBLER_SHARED_IA32_X64_H_
#define V8_CODEGEN_SHARED_IA32_X64_MACRO_ASSEMBLER_SHARED_IA32_X64_H_

#include "src/base/macros.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/turbo-assembler.h"

#if V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/register-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/codegen/x64/register-x64.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {
class Assembler;

// For WebAssembly we care about the full floating point register. If we are not
// running Wasm, we can get away with saving half of those registers.
#if V8_ENABLE_WEBASSEMBLY
constexpr int kStackSavedSavedFPSize = 2 * kDoubleSize;
#else
constexpr int kStackSavedSavedFPSize = kDoubleSize;
#endif  // V8_ENABLE_WEBASSEMBLY

class V8_EXPORT_PRIVATE SharedTurboAssembler : public TurboAssemblerBase {
 public:
  using TurboAssemblerBase::TurboAssemblerBase;

  void Movapd(XMMRegister dst, XMMRegister src);

  template <typename Dst, typename Src>
  void Movdqu(Dst dst, Src src) {
    if (CpuFeatures::IsSupported(AVX)) {
      CpuFeatureScope avx_scope(this, AVX);
      vmovdqu(dst, src);
    } else {
      // movups is 1 byte shorter than movdqu. On most SSE systems, this incurs
      // no delay moving between integer and floating-point domain.
      movups(dst, src);
    }
  }

  // Shufps that will mov src1 into dst if AVX is not supported.
  void Shufps(XMMRegister dst, XMMRegister src1, XMMRegister src2,
              uint8_t imm8);

  // Helper struct to implement functions that check for AVX support and
  // dispatch to the appropriate AVX/SSE instruction.
  template <typename Dst, typename Arg, typename... Args>
  struct AvxHelper {
    Assembler* assm;
    base::Optional<CpuFeature> feature = base::nullopt;
    // Call a method where the AVX version expects the dst argument to be
    // duplicated.
    // E.g. Andps(x, y) -> vandps(x, x, y)
    //                  -> andps(x, y)
    template <void (Assembler::*avx)(Dst, Dst, Arg, Args...),
              void (Assembler::*no_avx)(Dst, Arg, Args...)>
    void emit(Dst dst, Arg arg, Args... args) {
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope scope(assm, AVX);
        (assm->*avx)(dst, dst, arg, args...);
      } else if (feature.has_value()) {
        DCHECK(CpuFeatures::IsSupported(*feature));
        CpuFeatureScope scope(assm, *feature);
        (assm->*no_avx)(dst, arg, args...);
      } else {
        (assm->*no_avx)(dst, arg, args...);
      }
    }

    // Call a method in the AVX form (one more operand), but if unsupported will
    // check that dst == first src.
    // E.g. Andps(x, y, z) -> vandps(x, y, z)
    //                     -> andps(x, z) and check that x == y
    template <void (Assembler::*avx)(Dst, Arg, Args...),
              void (Assembler::*no_avx)(Dst, Args...)>
    void emit(Dst dst, Arg arg, Args... args) {
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope scope(assm, AVX);
        (assm->*avx)(dst, arg, args...);
      } else if (feature.has_value()) {
        DCHECK_EQ(dst, arg);
        DCHECK(CpuFeatures::IsSupported(*feature));
        CpuFeatureScope scope(assm, *feature);
        (assm->*no_avx)(dst, args...);
      } else {
        DCHECK_EQ(dst, arg);
        (assm->*no_avx)(dst, args...);
      }
    }

    // Call a method where the AVX version expects no duplicated dst argument.
    // E.g. Movddup(x, y) -> vmovddup(x, y)
    //                    -> movddup(x, y)
    template <void (Assembler::*avx)(Dst, Arg, Args...),
              void (Assembler::*no_avx)(Dst, Arg, Args...)>
    void emit(Dst dst, Arg arg, Args... args) {
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope scope(assm, AVX);
        (assm->*avx)(dst, arg, args...);
      } else if (feature.has_value()) {
        DCHECK(CpuFeatures::IsSupported(*feature));
        CpuFeatureScope scope(assm, *feature);
        (assm->*no_avx)(dst, arg, args...);
      } else {
        (assm->*no_avx)(dst, arg, args...);
      }
    }
  };

#define AVX_OP(macro_name, name)                                        \
  template <typename Dst, typename Arg, typename... Args>               \
  void macro_name(Dst dst, Arg arg, Args... args) {                     \
    AvxHelper<Dst, Arg, Args...>{this}                                  \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, arg, \
                                                              args...); \
  }

#define AVX_OP_SSE3(macro_name, name)                                    \
  template <typename Dst, typename Arg, typename... Args>                \
  void macro_name(Dst dst, Arg arg, Args... args) {                      \
    AvxHelper<Dst, Arg, Args...>{this, base::Optional<CpuFeature>(SSE3)} \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, arg,  \
                                                              args...);  \
  }

#define AVX_OP_SSSE3(macro_name, name)                                    \
  template <typename Dst, typename Arg, typename... Args>                 \
  void macro_name(Dst dst, Arg arg, Args... args) {                       \
    AvxHelper<Dst, Arg, Args...>{this, base::Optional<CpuFeature>(SSSE3)} \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, arg,   \
                                                              args...);   \
  }

#define AVX_OP_SSE4_1(macro_name, name)                                    \
  template <typename Dst, typename Arg, typename... Args>                  \
  void macro_name(Dst dst, Arg arg, Args... args) {                        \
    AvxHelper<Dst, Arg, Args...>{this, base::Optional<CpuFeature>(SSE4_1)} \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, arg,    \
                                                              args...);    \
  }

#define AVX_OP_SSE4_2(macro_name, name)                                    \
  template <typename Dst, typename Arg, typename... Args>                  \
  void macro_name(Dst dst, Arg arg, Args... args) {                        \
    AvxHelper<Dst, Arg, Args...>{this, base::Optional<CpuFeature>(SSE4_2)} \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, arg,    \
                                                              args...);    \
  }

  // Keep this list sorted by required extension, then instruction name.
  AVX_OP(Addpd, addpd)
  AVX_OP(Addps, addps)
  AVX_OP(Andnpd, andnpd)
  AVX_OP(Andnps, andnps)
  AVX_OP(Andpd, andpd)
  AVX_OP(Andps, andps)
  AVX_OP(Cmpeqpd, cmpeqpd)
  AVX_OP(Cmplepd, cmplepd)
  AVX_OP(Cmpleps, cmpleps)
  AVX_OP(Cmpltpd, cmpltpd)
  AVX_OP(Cmpneqpd, cmpneqpd)
  AVX_OP(Cmpunordpd, cmpunordpd)
  AVX_OP(Cmpunordps, cmpunordps)
  AVX_OP(Cvtdq2pd, cvtdq2pd)
  AVX_OP(Cvtdq2ps, cvtdq2ps)
  AVX_OP(Cvtpd2ps, cvtpd2ps)
  AVX_OP(Cvtps2pd, cvtps2pd)
  AVX_OP(Cvttps2dq, cvttps2dq)
  AVX_OP(Divpd, divpd)
  AVX_OP(Divps, divps)
  AVX_OP(Maxpd, maxpd)
  AVX_OP(Maxps, maxps)
  AVX_OP(Minpd, minpd)
  AVX_OP(Minps, minps)
  AVX_OP(Movaps, movaps)
  AVX_OP(Movd, movd)
  AVX_OP(Movhlps, movhlps)
  AVX_OP(Movhps, movhps)
  AVX_OP(Movlps, movlps)
  AVX_OP(Movmskpd, movmskpd)
  AVX_OP(Movmskps, movmskps)
  AVX_OP(Movsd, movsd)
  AVX_OP(Movss, movss)
  AVX_OP(Movupd, movupd)
  AVX_OP(Movups, movups)
  AVX_OP(Mulpd, mulpd)
  AVX_OP(Mulps, mulps)
  AVX_OP(Orpd, orpd)
  AVX_OP(Orps, orps)
  AVX_OP(Packssdw, packssdw)
  AVX_OP(Packsswb, packsswb)
  AVX_OP(Packuswb, packuswb)
  AVX_OP(Paddb, paddb)
  AVX_OP(Paddd, paddd)
  AVX_OP(Paddq, paddq)
  AVX_OP(Paddsb, paddsb)
  AVX_OP(Paddusb, paddusb)
  AVX_OP(Paddusw, paddusw)
  AVX_OP(Paddw, paddw)
  AVX_OP(Pand, pand)
  AVX_OP(Pavgb, pavgb)
  AVX_OP(Pavgw, pavgw)
  AVX_OP(Pcmpgtb, pcmpgtb)
  AVX_OP(Pcmpeqd, pcmpeqd)
  AVX_OP(Pmaxub, pmaxub)
  AVX_OP(Pminub, pminub)
  AVX_OP(Pmovmskb, pmovmskb)
  AVX_OP(Pmullw, pmullw)
  AVX_OP(Pmuludq, pmuludq)
  AVX_OP(Por, por)
  AVX_OP(Pshufd, pshufd)
  AVX_OP(Pshufhw, pshufhw)
  AVX_OP(Pshuflw, pshuflw)
  AVX_OP(Pslld, pslld)
  AVX_OP(Psllq, psllq)
  AVX_OP(Psllw, psllw)
  AVX_OP(Psrad, psrad)
  AVX_OP(Psraw, psraw)
  AVX_OP(Psrld, psrld)
  AVX_OP(Psrlq, psrlq)
  AVX_OP(Psrlw, psrlw)
  AVX_OP(Psubb, psubb)
  AVX_OP(Psubd, psubd)
  AVX_OP(Psubq, psubq)
  AVX_OP(Psubsb, psubsb)
  AVX_OP(Psubusb, psubusb)
  AVX_OP(Psubw, psubw)
  AVX_OP(Punpckhbw, punpckhbw)
  AVX_OP(Punpckhdq, punpckhdq)
  AVX_OP(Punpckhqdq, punpckhqdq)
  AVX_OP(Punpckhwd, punpckhwd)
  AVX_OP(Punpcklbw, punpcklbw)
  AVX_OP(Punpckldq, punpckldq)
  AVX_OP(Punpcklqdq, punpcklqdq)
  AVX_OP(Punpcklwd, punpcklwd)
  AVX_OP(Pxor, pxor)
  AVX_OP(Rcpps, rcpps)
  AVX_OP(Rsqrtps, rsqrtps)
  AVX_OP(Sqrtpd, sqrtpd)
  AVX_OP(Sqrtps, sqrtps)
  AVX_OP(Sqrtsd, sqrtsd)
  AVX_OP(Sqrtss, sqrtss)
  AVX_OP(Subpd, subpd)
  AVX_OP(Subps, subps)
  AVX_OP(Unpcklps, unpcklps)
  AVX_OP(Xorpd, xorpd)
  AVX_OP(Xorps, xorps)

  AVX_OP_SSE3(Haddps, haddps)
  AVX_OP_SSE3(Movddup, movddup)
  AVX_OP_SSE3(Movshdup, movshdup)

  AVX_OP_SSSE3(Pabsb, pabsb)
  AVX_OP_SSSE3(Pabsd, pabsd)
  AVX_OP_SSSE3(Pabsw, pabsw)
  AVX_OP_SSSE3(Palignr, palignr)
  AVX_OP_SSSE3(Psignb, psignb)
  AVX_OP_SSSE3(Psignd, psignd)
  AVX_OP_SSSE3(Psignw, psignw)

  AVX_OP_SSE4_1(Extractps, extractps)
  AVX_OP_SSE4_1(Pblendw, pblendw)
  AVX_OP_SSE4_1(Pextrb, pextrb)
  AVX_OP_SSE4_1(Pextrw, pextrw)
  AVX_OP_SSE4_1(Pmaxsb, pmaxsb)
  AVX_OP_SSE4_1(Pmaxsd, pmaxsd)
  AVX_OP_SSE4_1(Pminsb, pminsb)
  AVX_OP_SSE4_1(Pmovsxbw, pmovsxbw)
  AVX_OP_SSE4_1(Pmovsxdq, pmovsxdq)
  AVX_OP_SSE4_1(Pmovsxwd, pmovsxwd)
  AVX_OP_SSE4_1(Pmovzxbw, pmovzxbw)
  AVX_OP_SSE4_1(Pmovzxdq, pmovzxdq)
  AVX_OP_SSE4_1(Pmovzxwd, pmovzxwd)
  AVX_OP_SSE4_1(Ptest, ptest)
  AVX_OP_SSE4_1(Roundpd, roundpd)
  AVX_OP_SSE4_1(Roundps, roundps)

  void F64x2ExtractLane(DoubleRegister dst, XMMRegister src, uint8_t lane);
  void F64x2ReplaceLane(XMMRegister dst, XMMRegister src, DoubleRegister rep,
                        uint8_t lane);
  void F64x2Min(XMMRegister dst, XMMRegister lhs, XMMRegister rhs,
                XMMRegister scratch);
  void F64x2Max(XMMRegister dst, XMMRegister lhs, XMMRegister rhs,
                XMMRegister scratch);
  void F32x4Splat(XMMRegister dst, DoubleRegister src);
  void F32x4ExtractLane(FloatRegister dst, XMMRegister src, uint8_t lane);
  void S128Store32Lane(Operand dst, XMMRegister src, uint8_t laneidx);
  void I16x8ExtMulLow(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                      XMMRegister scrat, bool is_signed);
  void I16x8ExtMulHighS(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                        XMMRegister scratch);
  void I16x8ExtMulHighU(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                        XMMRegister scratch);
  void I16x8SConvertI8x16High(XMMRegister dst, XMMRegister src);
  void I16x8UConvertI8x16High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  // Requires that dst == src1 if AVX is not supported.
  void I32x4ExtMul(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                   XMMRegister scratch, bool low, bool is_signed);
  void I32x4SConvertI16x8High(XMMRegister dst, XMMRegister src);
  void I32x4UConvertI16x8High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  void I64x2Neg(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  void I64x2Abs(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  void I64x2GtS(XMMRegister dst, XMMRegister src0, XMMRegister src1,
                XMMRegister scratch);
  void I64x2GeS(XMMRegister dst, XMMRegister src0, XMMRegister src1,
                XMMRegister scratch);
  void I64x2ExtMul(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                   XMMRegister scratch, bool low, bool is_signed);
  void I64x2SConvertI32x4High(XMMRegister dst, XMMRegister src);
  void I64x2UConvertI32x4High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  void S128Not(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  // Requires dst == mask when AVX is not supported.
  void S128Select(XMMRegister dst, XMMRegister mask, XMMRegister src1,
                  XMMRegister src2, XMMRegister scratch);
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_SHARED_IA32_X64_MACRO_ASSEMBLER_SHARED_IA32_X64_H_
