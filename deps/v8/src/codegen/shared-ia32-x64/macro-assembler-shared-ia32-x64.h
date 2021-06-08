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

  template <typename Dst, typename... Args>
  struct AvxHelper {
    Assembler* assm;
    base::Optional<CpuFeature> feature = base::nullopt;
    // Call a method where the AVX version expects the dst argument to be
    // duplicated.
    template <void (Assembler::*avx)(Dst, Dst, Args...),
              void (Assembler::*no_avx)(Dst, Args...)>
    void emit(Dst dst, Args... args) {
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope scope(assm, AVX);
        (assm->*avx)(dst, dst, args...);
      } else if (feature.has_value()) {
        DCHECK(CpuFeatures::IsSupported(*feature));
        CpuFeatureScope scope(assm, *feature);
        (assm->*no_avx)(dst, args...);
      } else {
        (assm->*no_avx)(dst, args...);
      }
    }

    // Call a method where the AVX version expects no duplicated dst argument.
    template <void (Assembler::*avx)(Dst, Args...),
              void (Assembler::*no_avx)(Dst, Args...)>
    void emit(Dst dst, Args... args) {
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope scope(assm, AVX);
        (assm->*avx)(dst, args...);
      } else if (feature.has_value()) {
        DCHECK(CpuFeatures::IsSupported(*feature));
        CpuFeatureScope scope(assm, *feature);
        (assm->*no_avx)(dst, args...);
      } else {
        (assm->*no_avx)(dst, args...);
      }
    }
  };

#define AVX_OP(macro_name, name)                                             \
  template <typename Dst, typename... Args>                                  \
  void macro_name(Dst dst, Args... args) {                                   \
    AvxHelper<Dst, Args...>{this}                                            \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, args...); \
  }

#define AVX_OP_SSE3(macro_name, name)                                        \
  template <typename Dst, typename... Args>                                  \
  void macro_name(Dst dst, Args... args) {                                   \
    AvxHelper<Dst, Args...>{this, base::Optional<CpuFeature>(SSE3)}          \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, args...); \
  }

#define AVX_OP_SSSE3(macro_name, name)                                       \
  template <typename Dst, typename... Args>                                  \
  void macro_name(Dst dst, Args... args) {                                   \
    AvxHelper<Dst, Args...>{this, base::Optional<CpuFeature>(SSSE3)}         \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, args...); \
  }

#define AVX_OP_SSE4_1(macro_name, name)                                      \
  template <typename Dst, typename... Args>                                  \
  void macro_name(Dst dst, Args... args) {                                   \
    AvxHelper<Dst, Args...>{this, base::Optional<CpuFeature>(SSE4_1)}        \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, args...); \
  }

#define AVX_OP_SSE4_2(macro_name, name)                                      \
  template <typename Dst, typename... Args>                                  \
  void macro_name(Dst dst, Args... args) {                                   \
    AvxHelper<Dst, Args...>{this, base::Optional<CpuFeature>(SSE4_2)}        \
        .template emit<&Assembler::v##name, &Assembler::name>(dst, args...); \
  }

  AVX_OP(Cvtdq2pd, cvtdq2pd)
  AVX_OP(Cvtdq2ps, cvtdq2ps)
  AVX_OP(Cvtps2pd, cvtps2pd)
  AVX_OP(Cvtpd2ps, cvtpd2ps)
  AVX_OP(Cvttps2dq, cvttps2dq)
  AVX_OP(Movaps, movaps)
  AVX_OP(Movd, movd)
  AVX_OP(Movhps, movhps)
  AVX_OP(Movlps, movlps)
  AVX_OP(Movmskpd, movmskpd)
  AVX_OP(Movmskps, movmskps)
  AVX_OP(Movss, movss)
  AVX_OP(Movsd, movsd)
  AVX_OP(Movupd, movupd)
  AVX_OP(Movups, movups)
  AVX_OP(Pmovmskb, pmovmskb)
  AVX_OP(Pmullw, pmullw)
  AVX_OP(Pshuflw, pshuflw)
  AVX_OP(Pshufhw, pshufhw)
  AVX_OP(Pshufd, pshufd)
  AVX_OP(Rcpps, rcpps)
  AVX_OP(Rsqrtps, rsqrtps)
  AVX_OP(Sqrtps, sqrtps)
  AVX_OP(Sqrtpd, sqrtpd)
  AVX_OP_SSE3(Movddup, movddup)
  AVX_OP_SSE3(Movshdup, movshdup)
  AVX_OP_SSSE3(Pabsb, pabsb)
  AVX_OP_SSSE3(Pabsw, pabsw)
  AVX_OP_SSSE3(Pabsd, pabsd)
  AVX_OP_SSE4_1(Extractps, extractps)
  AVX_OP_SSE4_1(Pextrb, pextrb)
  AVX_OP_SSE4_1(Pextrw, pextrw)
  AVX_OP_SSE4_1(Pmovsxbw, pmovsxbw)
  AVX_OP_SSE4_1(Pmovsxwd, pmovsxwd)
  AVX_OP_SSE4_1(Pmovsxdq, pmovsxdq)
  AVX_OP_SSE4_1(Pmovzxbw, pmovzxbw)
  AVX_OP_SSE4_1(Pmovzxwd, pmovzxwd)
  AVX_OP_SSE4_1(Pmovzxdq, pmovzxdq)
  AVX_OP_SSE4_1(Ptest, ptest)
  AVX_OP_SSE4_1(Roundps, roundps)
  AVX_OP_SSE4_1(Roundpd, roundpd)

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
  // Requires dst == mask when AVX is not supported.
  void S128Select(XMMRegister dst, XMMRegister mask, XMMRegister src1,
                  XMMRegister src2, XMMRegister scratch);
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_SHARED_IA32_X64_MACRO_ASSEMBLER_SHARED_IA32_X64_H_
