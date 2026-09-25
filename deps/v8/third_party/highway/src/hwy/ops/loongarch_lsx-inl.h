// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#ifndef __loongarch_sx
// If LSX is to be runtime dispatched (instead of in baseline), we need
// to enable it *and* define __loongarch_sx or the intrinsic header will
// fail to compile.
//
// We cannot simply move lsxintrin.h after HWY_BEFORE_NAMESPACE because
// doing so may cause the first (the only effective) inclusion of
// lsxintrin.h to be compiled with both LSX and LASX enabled.  Then when
// we call the inline functions in the header with only LSX enabled,
// we'll get an "always_inline function requires lasx but would be inlined
// into a function that is compiled without suport for lasx" error.
HWY_PUSH_ATTRIBUTES("lsx")
#define __loongarch_sx
#include <lsxintrin.h>
#undef __loongarch_sx
// Prevent "unused push_attribute" warning from Clang.
HWY_MAYBE_UNUSED static void HWY_CONCAT(hwy_lsx_dummy, __COUNTER__) () {}
HWY_POP_ATTRIBUTES
#else
#include <lsxintrin.h>
#endif

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

// Enable generic functions for whichever of (f16, bf16) are not supported.
#define HWY_LSX_IF_EMULATED_D(D) HWY_IF_SPECIAL_FLOAT_D(D)

template <typename T>
struct Raw128 {
  using type = __m128i;
};
template <>
struct Raw128<float> {
  using type = __m128;
};
template <>
struct Raw128<double> {
  using type = __m128d;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;

 public:
  using PrivateT = T;                     // only for DFromV
  static constexpr size_t kPrivateN = N;  // only for DFromV

  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator%=(const Vec128 other) {
    return *this = (*this % other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

template <typename T>
using Vec64 = Vec128<T, 8 / sizeof(T)>;

template <typename T>
using Vec32 = Vec128<T, 4 / sizeof(T)>;

template <typename T>
using Vec16 = Vec128<T, 2 / sizeof(T)>;

namespace detail {

template <typename T>
using RawMask128 = typename Raw128<T>::type;

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  using Raw = typename detail::RawMask128<T>;

  using PrivateT = T;                     // only for DFromM
  static constexpr size_t kPrivateN = N;  // only for DFromM

  Raw raw;
};

template <class V>
using DFromV = Simd<typename V::PrivateT, V::kPrivateN, 0>;

template <class M>
using DFromM = Simd<typename M::PrivateT, M::kPrivateN, 0>;

template <class V>
using TFromV = typename V::PrivateT;

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m128i BitCastToInteger(__m128i v) { return v; }
HWY_INLINE __m128i BitCastToInteger(__m128 v) {
  return reinterpret_cast<__m128i>(v);
}
HWY_INLINE __m128i BitCastToInteger(__m128d v) {
  return reinterpret_cast<__m128i>(v);
}

template <typename T, size_t N>
HWY_INLINE Vec128<uint8_t, N * sizeof(T)> BitCastToByte(Vec128<T, N> v) {
  return Vec128<uint8_t, N * sizeof(T)>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger128 {
  HWY_INLINE __m128i operator()(__m128i v) { return v; }
};
template <>
struct BitCastFromInteger128<float> {
  HWY_INLINE __m128 operator()(__m128i v) {
    return reinterpret_cast<__m128>(v);
  }
};
template <>
struct BitCastFromInteger128<double> {
  HWY_INLINE __m128d operator()(__m128i v) {
    return reinterpret_cast<__m128d>(v);
  }
};

}  // namespace detail

// ------------------------------ Zero

// Use HWY_MAX_LANES_D here because VFromD is defined in terms of Zero.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_FLOAT3264_D(D)>
HWY_API Vec128<TFromD<D>, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  return Vec128<TFromD<D>, HWY_MAX_LANES_D(D)>{(__lsx_vreplgr2vr_w(0))};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_FLOAT3264_D(D)>
HWY_API Vec128<TFromD<D>, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  return Vec128<TFromD<D>, HWY_MAX_LANES_D(D)>{
      detail::BitCastFromInteger128<TFromD<D>>()(__lsx_vreplgr2vr_w(0))};
}

template <class D>
using VFromD = decltype(Zero(D()));

namespace detail {

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_INLINE VFromD<D> BitCastFromByte(D /* tag */,
                                     Vec128<uint8_t, D().MaxBytes()> v) {
  return VFromD<D>{BitCastFromInteger128<TFromD<D>>()(v.raw)};
}

}  // namespace detail

template <class D, typename FromT, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> BitCast(D d,
                          Vec128<FromT, Repartition<FromT, D>().MaxLanes()> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lsx_vreplgr2vr_b(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI16_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lsx_vreplgr2vr_h(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lsx_vreplgr2vr_w(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lsx_vreplgr2vr_d(static_cast<long int>(t))};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> Set(D d, float t) {
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vldrepl_w(&t, 0)});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> Set(D d, double t) {
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vldrepl_d(&t, 0)});
}

// Generic for all vector lengths.
template <class D, HWY_LSX_IF_EMULATED_D(D)>
HWY_API VFromD<D> Set(D df, TFromD<D> t) {
  const RebindToUnsigned<decltype(df)> du;
  static_assert(sizeof(TFromD<D>) == 2, "Expecting [b]f16");
  uint16_t bits;
  CopyBytes<2>(&t, &bits);
  return BitCast(df, Set(du, bits));
}

// ------------------------------ Undefined

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <class D>
HWY_API VFromD<D> Undefined(D /* tag */) {
  VFromD<D> v;
  return v;
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ GetLane

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(__lsx_vpickve2gr_b(v.raw, 0));
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(__lsx_vpickve2gr_h(v.raw, 0));
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(__lsx_vpickve2gr_w(v.raw, 0));
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(__lsx_vpickve2gr_d(v.raw, 0));
}
template <size_t N>
HWY_API float GetLane(const Vec128<float, N> v) {
  float f32;
  int32_t i32 = __lsx_vpickve2gr_w(reinterpret_cast<__m128i>(v.raw), 0);
  CopyBytes<4>(&i32, &f32);
  return f32;
}
template <size_t N>
HWY_API double GetLane(const Vec128<double, N> v) {
  double f64;
  int64_t i64 = __lsx_vpickve2gr_d(reinterpret_cast<__m128i>(v.raw), 0);
  CopyBytes<8>(&i64, &f64);
  return f64;
}

// ------------------------------ ResizeBitCast

template <class D, class FromV, HWY_IF_V_SIZE_LE_V(FromV, 16),
          HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, VFromD<decltype(du8)>{detail::BitCastToInteger(v.raw)});
}

// ------------------------------ Dup128VecFromValues

template <class D, HWY_IF_UI8_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3, TFromD<D> t4,
                                      TFromD<D> t5, TFromD<D> t6, TFromD<D> t7,
                                      TFromD<D> t8, TFromD<D> t9, TFromD<D> t10,
                                      TFromD<D> t11, TFromD<D> t12,
                                      TFromD<D> t13, TFromD<D> t14,
                                      TFromD<D> t15) {
  typedef int8_t GccI8RawVectType __attribute__((__vector_size__(16)));
  const GccI8RawVectType raw = {
      static_cast<int8_t>(t0),  static_cast<int8_t>(t1),
      static_cast<int8_t>(t2),  static_cast<int8_t>(t3),
      static_cast<int8_t>(t4),  static_cast<int8_t>(t5),
      static_cast<int8_t>(t6),  static_cast<int8_t>(t7),
      static_cast<int8_t>(t8),  static_cast<int8_t>(t9),
      static_cast<int8_t>(t10), static_cast<int8_t>(t11),
      static_cast<int8_t>(t12), static_cast<int8_t>(t13),
      static_cast<int8_t>(t14), static_cast<int8_t>(t15)};
  return VFromD<D>{reinterpret_cast<__m128i>(raw)};
}

template <class D, HWY_IF_UI16_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3, TFromD<D> t4,
                                      TFromD<D> t5, TFromD<D> t6,
                                      TFromD<D> t7) {
  typedef int16_t GccI16RawVectType __attribute__((__vector_size__(16)));
  const GccI16RawVectType raw = {
      static_cast<int16_t>(t0), static_cast<int16_t>(t1),
      static_cast<int16_t>(t2), static_cast<int16_t>(t3),
      static_cast<int16_t>(t4), static_cast<int16_t>(t5),
      static_cast<int16_t>(t6), static_cast<int16_t>(t7)};
  return VFromD<D>{reinterpret_cast<__m128i>(raw)};
}

template <class D, HWY_IF_SPECIAL_FLOAT_D(D)>
HWY_API VFromD<D> Dup128VecFromValues(D d, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3, TFromD<D> t4,
                                      TFromD<D> t5, TFromD<D> t6,
                                      TFromD<D> t7) {
  const RebindToSigned<decltype(d)> di;
  return BitCast(d,
                 Dup128VecFromValues(
                     di, BitCastScalar<int16_t>(t0), BitCastScalar<int16_t>(t1),
                     BitCastScalar<int16_t>(t2), BitCastScalar<int16_t>(t3),
                     BitCastScalar<int16_t>(t4), BitCastScalar<int16_t>(t5),
                     BitCastScalar<int16_t>(t6), BitCastScalar<int16_t>(t7)));
}

template <class D, HWY_IF_UI32_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3) {
  typedef int32_t GccI32RawVectType __attribute__((__vector_size__(16)));
  const GccI32RawVectType raw = {
      static_cast<int32_t>(t0), static_cast<int32_t>(t1),
      static_cast<int32_t>(t2), static_cast<int32_t>(t3)};
  return VFromD<D>{reinterpret_cast<__m128i>(raw)};
}
template <class D, HWY_IF_UI64_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1) {
  typedef int64_t GccI64RawVectType __attribute__((__vector_size__(16)));
  const GccI64RawVectType raw = {static_cast<int64_t>(t0),
                                 static_cast<int64_t>(t1)};
  return VFromD<D>{reinterpret_cast<__m128i>(raw)};
}
template <class D, HWY_IF_F32_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3) {
  typedef float GccF32RawVectType __attribute__((__vector_size__(16)));
  const GccF32RawVectType raw = {t0, t1, t2, t3};
  return VFromD<D>{reinterpret_cast<__m128>(raw)};
}
template <class D, HWY_IF_F64_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1) {
  typedef double GccF64RawVectType __attribute__((__vector_size__(16)));
  const GccF64RawVectType raw = {t0, t1};
  return VFromD<D>{reinterpret_cast<__m128d>(raw)};
}

// ================================================== LOGICAL

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{
                        __lsx_vand_v(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  const DFromV<decltype(mask)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vandn_v(
                        BitCast(du, not_mask).raw, BitCast(du, mask).raw)});
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{
                        __lsx_vor_v(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{
                        __lsx_vxor_v(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ Not
template <typename T, size_t N>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{
                        __lsx_vnor_v(BitCast(du, v).raw, BitCast(du, v).raw)});
}

// ------------------------------ Xor3
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor3(Vec128<T, N> x1, Vec128<T, N> x2, Vec128<T, N> x3) {
  return Xor(x1, Xor(x2, x3));
}

// ------------------------------ Or3
template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd
template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return Mask128<T, N>{v.raw};
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

// Generic for all vector lengths.
template <class D>
HWY_API VFromD<D> VecFromMask(D /* tag */, MFromD<D> v) {
  return VecFromMask(v);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  const DFromV<decltype(yes)> d;
  RebindToSigned<decltype(d)> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vbitsel_v(
                        BitCast(di, no).raw, BitCast(di, yes).raw,
                        RebindMask(di, mask).raw)});
}

// ------------------------------ IfVecThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ BitwiseIfThenElse

#ifdef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#undef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#else
#define HWY_NATIVE_BITWISE_IF_THEN_ELSE
#endif

template <class V>
HWY_API V BitwiseIfThenElse(V mask, V yes, V no) {
  return IfVecThenElse(mask, yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<1> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vpcnt_b(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<2> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vpcnt_h(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<4> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vpcnt_w(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<8> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vpcnt_d(v.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> PopulationCount(Vec128<T, N> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

// ================================================== SIGN

// ------------------------------ Neg

template <typename T, size_t N, HWY_IF_FLOAT_OR_SPECIAL(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Xor(v, SignBit(DFromV<decltype(v)>()));
}

template <typename T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vneg_b(v.raw)};
}

template <typename T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vneg_h(v.raw)};
}

template <typename T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vneg_w(v.raw)};
}

template <typename T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vneg_d(v.raw)};
}

// ------------------------------ Floating-point Abs
// Generic for all vector lengths
template <class V, HWY_IF_FLOAT(TFromV<V>)>
HWY_API V Abs(V v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  using TI = TFromD<decltype(di)>;
  return v & BitCast(d, Set(di, static_cast<TI>(~SignMask<TI>())));
}

// ------------------------------ CopySign
// Generic for all vector lengths.
template <class V>
HWY_API V CopySign(const V magn, const V sign) {
  static_assert(IsFloat<TFromV<V>>(), "Only makes sense for floating-point");

  const DFromV<decltype(magn)> d;
  const auto msb = SignBit(d);
  return BitwiseIfThenElse(msb, sign, magn);
}

// ------------------------------ CopySignToAbs
// Generic for all vector lengths.
template <class V>
HWY_API V CopySignToAbs(const V abs, const V sign) {
  const DFromV<decltype(abs)> d;
  return OrAnd(abs, SignBit(d), sign);
}

// ------------------------------ IfThenElseZero

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return yes & VecFromMask(DFromV<decltype(yes)>(), mask);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return AndNot(VecFromMask(DFromV<decltype(no)>(), mask), no);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Not(VecFromMask(d, m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

// ------------------------------ ExclusiveNeither

template <typename T, size_t N>
HWY_API Mask128<T, N> ExclusiveNeither(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ------------------------------ ShiftLeft

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftLeft(const Vec128<uint8_t, N> v) {
  return Vec128<uint8_t, N>{__lsx_vslli_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeft(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{__lsx_vslli_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeft(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{__lsx_vslli_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeft(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{__lsx_vslli_d(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftLeft(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{__lsx_vslli_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftLeft(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{__lsx_vslli_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftLeft(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{__lsx_vslli_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftLeft(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{__lsx_vslli_d(v.raw, kBits)};
}

// ------------------------------ ShiftRight

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftRight(Vec128<uint8_t, N> v) {
  return Vec128<uint8_t, N>{__lsx_vsrli_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftRight(Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{__lsx_vsrli_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftRight(Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{__lsx_vsrli_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftRight(Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{__lsx_vsrli_d(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftRight(Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{__lsx_vsrai_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftRight(Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{__lsx_vsrai_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftRight(Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{__lsx_vsrai_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftRight(Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{__lsx_vsrai_d(v.raw, kBits)};
}

// ------------------------------ RoundingShiftRight

#ifdef HWY_NATIVE_ROUNDING_SHR
#undef HWY_NATIVE_ROUNDING_SHR
#else
#define HWY_NATIVE_ROUNDING_SHR
#endif

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> RoundingShiftRight(Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{__lsx_vsrari_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> RoundingShiftRight(Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{__lsx_vsrari_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> RoundingShiftRight(Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{__lsx_vsrari_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> RoundingShiftRight(Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{__lsx_vsrari_d(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> RoundingShiftRight(Vec128<uint8_t, N> v) {
  return Vec128<uint8_t, N>{__lsx_vsrlri_b(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> RoundingShiftRight(Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{__lsx_vsrlri_h(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> RoundingShiftRight(Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{__lsx_vsrlri_w(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> RoundingShiftRight(Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{__lsx_vsrlri_d(v.raw, kBits)};
}

// ------------------------------ RoundingShr

template <size_t N>
HWY_API Vec128<int8_t, N> RoundingShr(Vec128<int8_t, N> v,
                                      Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>{__lsx_vsrar_b(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> RoundingShr(Vec128<int16_t, N> v,
                                       Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>{__lsx_vsrar_h(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> RoundingShr(Vec128<int32_t, N> v,
                                       Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>{__lsx_vsrar_w(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> RoundingShr(Vec128<int64_t, N> v,
                                       Vec128<int64_t, N> bits) {
  return Vec128<int64_t, N>{__lsx_vsrar_d(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> RoundingShr(Vec128<uint8_t, N> v,
                                       Vec128<uint8_t, N> bits) {
  return Vec128<uint8_t, N>{__lsx_vsrlr_b(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> RoundingShr(Vec128<uint16_t, N> v,
                                        Vec128<uint16_t, N> bits) {
  return Vec128<uint16_t, N>{__lsx_vsrlr_h(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> RoundingShr(Vec128<uint32_t, N> v,
                                        Vec128<uint32_t, N> bits) {
  return Vec128<uint32_t, N>{__lsx_vsrlr_w(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> RoundingShr(Vec128<uint64_t, N> v,
                                        Vec128<uint64_t, N> bits) {
  return Vec128<uint64_t, N>{__lsx_vsrlr_d(v.raw, bits.raw)};
}

// ------------------------------ RoundingShiftRightSame (RoundingShr)

template <typename T, size_t N>
HWY_API Vec128<T, N> RoundingShiftRightSame(const Vec128<T, N> v, int bits) {
  return RoundingShr(v, Set(DFromV<decltype(v)>(), static_cast<T>(bits)));
}

// ================================================== MEMORY (1)

// ------------------------------ Load 128

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> Load(D d, const T* HWY_RESTRICT aligned) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vld(aligned, 0)});
}

// Partial
template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> Load(D d, const TFromD<D>* HWY_RESTRICT p) {
  VFromD<D> v;
  CopyBytes<d.MaxBytes()>(p, &v);
  return v;
}

// LoadU == Load
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ MaskedLoad

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d,
                             const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

// ------------------------------ MaskedLoadOr

template <class D>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D d,
                               const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElse(m, LoadU(d, p), v);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> LoadDup128(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ Store 128

template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API void Store(VFromD<D> v, D /* tag */, void* HWY_RESTRICT aligned) {
  __lsx_vst(v.raw, aligned, 0);
}

// ------------------------------ Store 64

template <class D, HWY_IF_V_SIZE_D(D, 8)>
HWY_API void Store(VFromD<D> v, D /* tag */, void* HWY_RESTRICT aligned) {
  __lsx_vstelm_d(v.raw, aligned, 0, 0);
}

// ------------------------------ Store 32

template <class D, HWY_IF_V_SIZE_D(D, 4)>
HWY_API void Store(VFromD<D> v, D /* tag */, void* HWY_RESTRICT aligned) {
  __lsx_vstelm_w(v.raw, aligned, 0, 0);
}

// ------------------------------ Store 16

template <class D, HWY_IF_V_SIZE_D(D, 2)>
HWY_API void Store(VFromD<D> v, D /* tag */, void* HWY_RESTRICT aligned) {
  __lsx_vstelm_h(v.raw, aligned, 0, 0);
}

// ------------------------------ Store 8

template <class D, HWY_IF_V_SIZE_D(D, 1)>
HWY_API void Store(VFromD<D> v, D /* tag */, void* HWY_RESTRICT aligned) {
  __lsx_vstelm_b(v.raw, aligned, 0, 0);
}

template <class D>
HWY_API void StoreU(VFromD<D> v, D d, void* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ================================================== SWIZZLE (1)

// ------------------------------ TableLookupBytes
template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T, N> bytes,
                                        const Vec128<TI, NI> from) {
  const DFromV<decltype(from)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const DFromV<decltype(bytes)> d_bytes;
  const Repartition<uint8_t, decltype(d_bytes)> du8_bytes;
  return BitCast(
      d, VFromD<decltype(du8)>{__lsx_vshuf_b(BitCast(du8_bytes, bytes).raw,
                                             BitCast(du8_bytes, bytes).raw,
                                             (BitCast(du8, from).raw))});
}

// ------------------------------ TableLookupBytesOr0
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
  const DFromV<VI> d;
  const Repartition<int8_t, decltype(d)> di8;
  return BitCast(d,
                 IfThenZeroElse(Lt(BitCast(di8, from), Zero(di8)),
                                BitCast(di8, TableLookupBytes(bytes, from))));
}

// ------------------------------ Shuffles (ShiftRight, TableLookupBytes)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vshuf4i_w(
                        detail::BitCastToInteger(v.raw), 0xB1)});
}

namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo2301(const Vec32<T> a, const Vec32<T> b) {
  const int8_t _data_idx[] = {1, 0, 19, 18};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  return Vec32<T>{__lsx_vshuf_b(b.raw, a.raw, shuffle_idx)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo2301(const Vec64<T> a, const Vec64<T> b) {
  const int16_t _data_idx[] = {9, 8, 3, 2};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  return Vec64<T>{__lsx_vshuf_h(shuffle_idx, a.raw, b.raw)};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo2301(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, Vec128<int32_t>{__lsx_vpermi_w(BitCast(di, b).raw,
                                                   BitCast(di, a).raw, 0xB1)});
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo1230(const Vec32<T> a, const Vec32<T> b) {
  const int8_t _data_idx[] = {0, 3, 18, 17};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  return Vec32<T>{__lsx_vshuf_b(b.raw, a.raw, shuffle_idx)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo1230(const Vec64<T> a, const Vec64<T> b) {
  const int16_t _data_idx[] = {10, 11, 2, 1};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  auto t0 = __lsx_vshuf_h(shuffle_idx, a.raw, b.raw);
  return Vec64<T>{t0};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo1230(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, Vec128<int32_t>{__lsx_vpermi_w(BitCast(di, b).raw,
                                                   BitCast(di, a).raw, 0x6C)});
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo3012(const Vec32<T> a, const Vec32<T> b) {
  const int8_t _data_idx[] = {2, 1, 16, 19};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  return Vec32<T>{__lsx_vshuf_b(b.raw, a.raw, shuffle_idx)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo3012(const Vec64<T> a, const Vec64<T> b) {
  const int16_t _data_idx[] = {8, 9, 0, 3};
  __m128i shuffle_idx = __lsx_vld(_data_idx, 0);
  return Vec64<T>{__lsx_vshuf_h(shuffle_idx, a.raw, b.raw)};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo3012(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, Vec128<int32_t>{__lsx_vpermi_w(BitCast(di, b).raw,
                                                   BitCast(di, a).raw, 0xC6)});
}

}  // namespace detail

// Swap 64-bit halves
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle1032(const Vec128<T> v) {
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<uint32_t>{__lsx_vshuf4i_w(
                        reinterpret_cast<__m128i>(v.raw), 0x4E)});
}
HWY_API Vec128<uint64_t> Shuffle01(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>{__lsx_vshuf4i_w(v.raw, 0x4E)};
}
HWY_API Vec128<int64_t> Shuffle01(const Vec128<int64_t> v) {
  return Vec128<int64_t>{__lsx_vshuf4i_w(v.raw, 0x4E)};
}
HWY_API Vec128<double> Shuffle01(const Vec128<double> v) {
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<uint64_t>{__lsx_vshuf4i_d(
                        reinterpret_cast<__m128i>(v.raw),
                        reinterpret_cast<__m128i>(v.raw), 0x1)});
}

// Rotate right 32 bits
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle0321(const Vec128<T> v) {
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<uint32_t>{__lsx_vshuf4i_w(
                        reinterpret_cast<__m128i>(v.raw), 0x39)});
}
// Rotate left 32 bits
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle2103(const Vec128<T> v) {
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<uint32_t>{__lsx_vshuf4i_w(
                        reinterpret_cast<__m128i>(v.raw), 0x93)});
}
// Reverse
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle0123(const Vec128<T> v) {
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<uint32_t>{__lsx_vshuf4i_w(
                        reinterpret_cast<__m128i>(v.raw), 0x1B)});
}

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <class DTo, typename TFrom, size_t NFrom, HWY_IF_V_SIZE_LE_D(DTo, 16)>
HWY_API MFromD<DTo> RebindMask(DTo dto, Mask128<TFrom, NFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TFromD<DTo>), "Must have same size");
  const Simd<TFrom, NFrom, 0> d;
  return MaskFromVec(BitCast(dto, VecFromMask(d, m)));
}

// ================================================== COMPARE

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator==(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{__lsx_vseq_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator==(Vec128<uint16_t, N> a,
                                        Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{__lsx_vseq_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator==(Vec128<uint32_t, N> a,
                                        Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{__lsx_vseq_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{__lsx_vseq_d(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator==(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{__lsx_vseq_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator==(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{__lsx_vseq_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator==(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{__lsx_vseq_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{__lsx_vseq_d(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator==(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_ceq_s(a.raw, b.raw))};
}
template <size_t N>
HWY_API Mask128<double, N> operator==(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{
      reinterpret_cast<__m128d>(__lsx_vfcmp_ceq_d(a.raw, b.raw))};
}

// ------------------------------ Inequality

// This cannot have T as a template argument, otherwise it is not more
// specialized than rewritten operator== in C++20, leading to compile
// errors: https://gcc.godbolt.org/z/xsrPhPvPT.
template <size_t N>
HWY_API Mask128<uint8_t, N> operator!=(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator!=(Vec128<uint16_t, N> a,
                                        Vec128<uint16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator!=(Vec128<uint32_t, N> a,
                                        Vec128<uint32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator!=(Vec128<uint64_t, N> a,
                                        Vec128<uint64_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int8_t, N> operator!=(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator!=(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator!=(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator!=(Vec128<int64_t, N> a,
                                       Vec128<int64_t, N> b) {
  return Not(a == b);
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_cune_s(a.raw, b.raw))};
}
template <size_t N>
HWY_API Mask128<double, N> operator!=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{
      reinterpret_cast<__m128d>(__lsx_vfcmp_cune_d(a.raw, b.raw))};
}

// ------------------------------ Strict inequality

namespace detail {

template <size_t N>
HWY_INLINE Mask128<int8_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int8_t, N> a,
                                 Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{__lsx_vslt_b(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int16_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int16_t, N> a,
                                  Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{__lsx_vslt_h(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int32_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int32_t, N> a,
                                  Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{__lsx_vslt_w(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int64_t, N> Gt(hwy::SignedTag /*tag*/,
                                  const Vec128<int64_t, N> a,
                                  const Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{__lsx_vslt_d(b.raw, a.raw)};
}

template <size_t N>
HWY_INLINE Mask128<uint8_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<uint8_t, N> a,
                                  Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{__lsx_vslt_b(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<uint16_t, N> Gt(hwy::SignedTag /*tag*/,
                                   Vec128<uint16_t, N> a,
                                   Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{__lsx_vslt_h(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<uint32_t, N> Gt(hwy::SignedTag /*tag*/,
                                   Vec128<uint32_t, N> a,
                                   Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{__lsx_vslt_w(b.raw, a.raw)};
}
template <size_t N>
HWY_INLINE Mask128<uint64_t, N> Gt(hwy::SignedTag /*tag*/,
                                   const Vec128<uint64_t, N> a,
                                   const Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{__lsx_vslt_d(b.raw, a.raw)};
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Gt(hwy::UnsignedTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  const Vec128<T, N> msb = Set(du, (LimitsMax<T>() >> 1) + 1);
  const auto sa = BitCast(di, Xor(a, msb));
  const auto sb = BitCast(di, Xor(b, msb));
  return RebindMask(du, Gt(hwy::SignedTag(), sa, sb));
}

template <size_t N>
HWY_INLINE Mask128<float, N> Gt(hwy::FloatTag /*tag*/, Vec128<float, N> a,
                                Vec128<float, N> b) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_clt_s(b.raw, a.raw))};
}
template <size_t N>
HWY_INLINE Mask128<double, N> Gt(hwy::FloatTag /*tag*/, Vec128<double, N> a,
                                 Vec128<double, N> b) {
  return Mask128<double, N>{
      reinterpret_cast<__m128d>(__lsx_vfcmp_clt_d(b.raw, a.raw))};
}

}  // namespace detail

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Gt(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Weak inequality

namespace detail {
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Ge(hwy::SignedTag tag, Vec128<T, N> a,
                            Vec128<T, N> b) {
  return Not(Gt(tag, b, a));
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Ge(hwy::UnsignedTag tag, Vec128<T, N> a,
                            Vec128<T, N> b) {
  return Not(Gt(tag, b, a));
}

template <size_t N>
HWY_INLINE Mask128<float, N> Ge(hwy::FloatTag /*tag*/, Vec128<float, N> a,
                                Vec128<float, N> b) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_cle_s(b.raw, a.raw))};
}
template <size_t N>
HWY_INLINE Mask128<double, N> Ge(hwy::FloatTag /*tag*/, Vec128<double, N> a,
                                 Vec128<double, N> b) {
  return Mask128<double, N>{
      reinterpret_cast<__m128d>(__lsx_vfcmp_cle_d(b.raw, a.raw))};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Ge(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Reversed comparisons

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(Vec128<T, N> a, Vec128<T, N> b) {
  return b > a;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<=(Vec128<T, N> a, Vec128<T, N> b) {
  return b >= a;
}

// ------------------------------ Iota (Load)

namespace detail {

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<D> Iota0(D d) {
  return Dup128VecFromValues(
      d, TFromD<D>{0}, TFromD<D>{1}, TFromD<D>{2}, TFromD<D>{3}, TFromD<D>{4},
      TFromD<D>{5}, TFromD<D>{6}, TFromD<D>{7}, TFromD<D>{8}, TFromD<D>{9},
      TFromD<D>{10}, TFromD<D>{11}, TFromD<D>{12}, TFromD<D>{13}, TFromD<D>{14},
      TFromD<D>{15});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI16_D(D)>
HWY_INLINE VFromD<D> Iota0(D d) {
  return Dup128VecFromValues(d, TFromD<D>{0}, TFromD<D>{1}, TFromD<D>{2},
                             TFromD<D>{3}, TFromD<D>{4}, TFromD<D>{5},
                             TFromD<D>{6}, TFromD<D>{7});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<D> Iota0(D d) {
  return Dup128VecFromValues(
      d, static_cast<TFromD<D>>(0), static_cast<TFromD<D>>(1),
      static_cast<TFromD<D>>(2), static_cast<TFromD<D>>(3));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> Iota0(D d) {
  return Dup128VecFromValues(d, static_cast<TFromD<D>>(0),
                             static_cast<TFromD<D>>(1));
}

}  // namespace detail

template <class D, typename T2, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Iota(D d, const T2 first) {
  const auto result_iota =
      detail::Iota0(d) + Set(d, static_cast<TFromD<D>>(first));
  return result_iota;
}

// ------------------------------ FirstN (Iota, Lt)

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API MFromD<D> FirstN(D d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  using TI = TFromD<decltype(di)>;
  return RebindMask(d, detail::Iota0(di) < Set(di, static_cast<TI>(num)));
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vilvl_b(b.raw, a.raw)};
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vilvl_h(b.raw, a.raw)};
}
template <typename T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vilvl_w(b.raw, a.raw)};
}
template <typename T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vilvl_d(b.raw, a.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> InterleaveLower(Vec128<float, N> a,
                                         Vec128<float, N> b) {
  return Vec128<float, N>{reinterpret_cast<__m128>(__lsx_vilvl_w(
      reinterpret_cast<__m128i>(b.raw), reinterpret_cast<__m128i>(a.raw)))};
}
template <size_t N>
HWY_API Vec128<double, N> InterleaveLower(Vec128<double, N> a,
                                          Vec128<double, N> b) {
  return Vec128<double, N>{reinterpret_cast<__m128d>(__lsx_vilvl_d(
      reinterpret_cast<__m128i>(b.raw), reinterpret_cast<__m128i>(a.raw)))};
}

// Generic for all vector lengths.
template <class D>
HWY_API VFromD<D> InterleaveLower(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return InterleaveLower(a, b);
}

// ------------------------------ BlendedStore

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  StoreU(IfThenElse(m, v, LoadU(d, p)), d, p);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator+(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vadd_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator+(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vadd_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator+(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vadd_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator+(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vadd_d(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator+(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vadd_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator+(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vadd_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator+(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vadd_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator+(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vadd_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator+(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfadd_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator+(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfadd_d(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator-(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vsub_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator-(Vec128<uint16_t, N> a,
                                      Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vsub_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator-(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vsub_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator-(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vsub_d(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator-(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vsub_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator-(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vsub_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator-(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vsub_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator-(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vsub_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator-(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfsub_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator-(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfsub_d(a.raw, b.raw)};
}

// ------------------------------ SumsOf2
namespace detail {

template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::SignedTag, hwy::SizeTag<1> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_h_b(v.raw, v.raw)};
}
template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::UnsignedTag, hwy::SizeTag<1> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_hu_bu(v.raw, v.raw)};
}
template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::SignedTag, hwy::SizeTag<2> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_w_h(v.raw, v.raw)};
}
template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::UnsignedTag, hwy::SizeTag<2> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_wu_hu(v.raw, v.raw)};
}
template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::SignedTag, hwy::SizeTag<4> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_d_w(v.raw, v.raw)};
}
template <class V, HWY_IF_V_SIZE_LE_V(V, 16)>
HWY_INLINE VFromD<RepartitionToWide<DFromV<V>>> SumsOf2(
    hwy::UnsignedTag, hwy::SizeTag<4> /*lane_size_tag*/, V v) {
  return VFromD<RepartitionToWide<DFromV<V>>>{__lsx_vhaddw_du_wu(v.raw, v.raw)};
}

}  // namespace detail

// ------------------------------ SumsOf8
template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8(const Vec128<uint8_t, N> v) {
  __m128i temp = __lsx_vhaddw_hu_bu(v.raw, v.raw);
  temp = __lsx_vhaddw_wu_hu(temp, temp);
  return Vec128<uint64_t, N / 8>{__lsx_vhaddw_du_wu(temp, temp)};
}
template <size_t N>
HWY_API Vec128<int64_t, N / 8> SumsOf8(const Vec128<int8_t, N> v) {
  __m128i temp = __lsx_vhaddw_h_b(v.raw, v.raw);
  temp = __lsx_vhaddw_w_h(temp, temp);
  return Vec128<int64_t, N / 8>{__lsx_vhaddw_d_w(temp, temp)};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

#ifdef HWY_NATIVE_I32_SATURATED_ADDSUB
#undef HWY_NATIVE_I32_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I32_SATURATED_ADDSUB
#endif

#ifdef HWY_NATIVE_I64_SATURATED_ADDSUB
#undef HWY_NATIVE_I64_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I64_SATURATED_ADDSUB
#endif

#ifdef HWY_NATIVE_U32_SATURATED_ADDSUB
#undef HWY_NATIVE_U32_SATURATED_ADDSUB
#else
#define HWY_NATIVE_U32_SATURATED_ADDSUB
#endif

#ifdef HWY_NATIVE_U64_SATURATED_ADDSUB
#undef HWY_NATIVE_U64_SATURATED_ADDSUB
#else
#define HWY_NATIVE_U64_SATURATED_ADDSUB
#endif

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedAdd(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vsadd_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedAdd(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vsadd_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> SaturatedAdd(const Vec128<uint32_t, N> a,
                                         const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vsadd_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> SaturatedAdd(const Vec128<uint64_t, N> a,
                                         const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vsadd_du(a.raw, b.raw)};
}

// signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedAdd(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vsadd_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedAdd(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vsadd_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> SaturatedAdd(const Vec128<int32_t, N> a,
                                        const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vsadd_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> SaturatedAdd(const Vec128<int64_t, N> a,
                                        const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vsadd_d(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedSub(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vssub_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedSub(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vssub_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> SaturatedSub(const Vec128<uint32_t, N> a,
                                         const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vssub_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> SaturatedSub(const Vec128<uint64_t, N> a,
                                         const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vssub_du(a.raw, b.raw)};
}

// signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedSub(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vssub_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedSub(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vssub_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> SaturatedSub(const Vec128<int32_t, N> a,
                                        const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vssub_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> SaturatedSub(const Vec128<int64_t, N> a,
                                        const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vssub_d(a.raw, b.raw)};
}

// ------------------------------ AverageRound

// Returns (a + b + 1) / 2

#ifdef HWY_NATIVE_AVERAGE_ROUND_UI32
#undef HWY_NATIVE_AVERAGE_ROUND_UI32
#else
#define HWY_NATIVE_AVERAGE_ROUND_UI32
#endif

#ifdef HWY_NATIVE_AVERAGE_ROUND_UI64
#undef HWY_NATIVE_AVERAGE_ROUND_UI64
#else
#define HWY_NATIVE_AVERAGE_ROUND_UI64
#endif

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> AverageRound(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vavgr_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AverageRound(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vavgr_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> AverageRound(const Vec128<uint32_t, N> a,
                                         const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vavgr_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> AverageRound(const Vec128<uint64_t, N> a,
                                         const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vavgr_du(a.raw, b.raw)};
}

// signed
template <size_t N>
HWY_API Vec128<int8_t, N> AverageRound(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vavgr_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> AverageRound(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vavgr_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> AverageRound(const Vec128<int32_t, N> a,
                                        const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vavgr_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> AverageRound(const Vec128<int64_t, N> a,
                                        const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vavgr_d(a.raw, b.raw)};
}

// ------------------------------ Integer/Float multiplication

// Per-target flags to prevent generic_ops-inl.h defining 8/64-bit operator*.
#ifdef HWY_NATIVE_MUL_8
#undef HWY_NATIVE_MUL_8
#else
#define HWY_NATIVE_MUL_8
#endif
#ifdef HWY_NATIVE_MUL_64
#undef HWY_NATIVE_MUL_64
#else
#define HWY_NATIVE_MUL_64
#endif

template <typename T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> operator*(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vmul_b(a.raw, b.raw)};
}
template <typename T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> operator*(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vmul_h(a.raw, b.raw)};
}
template <typename T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> operator*(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vmul_w(a.raw, b.raw)};
}
template <typename T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> operator*(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vmul_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator*(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfmul_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator*(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfmul_d(a.raw, b.raw)};
}

// ------------------------------ MulHigh

// Usigned
template <size_t N>
HWY_API Vec128<uint8_t, N> MulHigh(const Vec128<uint8_t, N> a,
                                   const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vmuh_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vmuh_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> MulHigh(const Vec128<uint32_t, N> a,
                                    const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vmuh_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> MulHigh(const Vec128<uint64_t, N> a,
                                    const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vmuh_du(a.raw, b.raw)};
}

// signed
template <size_t N>
HWY_API Vec128<int8_t, N> MulHigh(const Vec128<int8_t, N> a,
                                  const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vmuh_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vmuh_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> MulHigh(const Vec128<int32_t, N> a,
                                   const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vmuh_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> MulHigh(const Vec128<int64_t, N> a,
                                   const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vmuh_d(a.raw, b.raw)};
}

// ------------------------------ MulEven

template <size_t N>
HWY_API Vec128<int16_t, (N + 1) / 2> MulEven(Vec128<int8_t, N> a,
                                             Vec128<int8_t, N> b) {
  return Vec128<int16_t, (N + 1) / 2>{__lsx_vmulwev_h_b(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint16_t, (N + 1) / 2> MulEven(Vec128<uint8_t, N> a,
                                              Vec128<uint8_t, N> b) {
  return Vec128<uint16_t, (N + 1) / 2>{__lsx_vmulwev_h_bu(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, (N + 1) / 2> MulEven(Vec128<int16_t, N> a,
                                             Vec128<int16_t, N> b) {
  return Vec128<int32_t, (N + 1) / 2>{__lsx_vmulwev_w_h(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, (N + 1) / 2> MulEven(Vec128<uint16_t, N> a,
                                              Vec128<uint16_t, N> b) {
  return Vec128<uint32_t, (N + 1) / 2>{__lsx_vmulwev_w_hu(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(Vec128<int32_t, N> a,
                                             Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{__lsx_vmulwev_d_w(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(Vec128<uint32_t, N> a,
                                              Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{__lsx_vmulwev_d_wu(a.raw, b.raw)};
}

template <typename T, HWY_IF_I64(T)>
HWY_API Vec128<T> MulEven(Vec128<T> a, Vec128<T> b) {
  return Vec128<T>{__lsx_vmulwev_q_d(a.raw, b.raw)};
}

template <typename T, HWY_IF_U64(T)>
HWY_API Vec128<T> MulEven(Vec128<T> a, Vec128<T> b) {
  return Vec128<T>{__lsx_vmulwev_q_du(a.raw, b.raw)};
}

// ------------------------------ MulOdd

template <size_t N>
HWY_API Vec128<int16_t, (N + 1) / 2> MulOdd(Vec128<int8_t, N> a,
                                            Vec128<int8_t, N> b) {
  return Vec128<int16_t, (N + 1) / 2>{__lsx_vmulwod_h_b(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint16_t, (N + 1) / 2> MulOdd(Vec128<uint8_t, N> a,
                                             Vec128<uint8_t, N> b) {
  return Vec128<uint16_t, (N + 1) / 2>{__lsx_vmulwod_h_bu(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, (N + 1) / 2> MulOdd(Vec128<int16_t, N> a,
                                            Vec128<int16_t, N> b) {
  return Vec128<int32_t, (N + 1) / 2>{__lsx_vmulwod_w_h(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, (N + 1) / 2> MulOdd(Vec128<uint16_t, N> a,
                                             Vec128<uint16_t, N> b) {
  return Vec128<uint32_t, (N + 1) / 2>{__lsx_vmulwod_w_hu(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulOdd(Vec128<int32_t, N> a,
                                            Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{__lsx_vmulwod_d_w(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulOdd(Vec128<uint32_t, N> a,
                                             Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{__lsx_vmulwod_d_wu(a.raw, b.raw)};
}

template <typename T, HWY_IF_I64(T)>
HWY_API Vec128<T> MulOdd(Vec128<T> a, Vec128<T> b) {
  return Vec128<T>{__lsx_vmulwod_q_d(a.raw, b.raw)};
}

template <typename T, HWY_IF_U64(T)>
HWY_API Vec128<T> MulOdd(Vec128<T> a, Vec128<T> b) {
  return Vec128<T>{__lsx_vmulwod_q_du(a.raw, b.raw)};
}

// ------------------------------ RotateRight (ShiftRight, Or)

template <int kBits, typename T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vrotri_b(v.raw, kBits)};
}
template <int kBits, typename T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vrotri_h(v.raw, kBits)};
}
template <int kBits, typename T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vrotri_w(v.raw, kBits)};
}
template <int kBits, typename T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  return Vec128<T, N>{__lsx_vrotri_d(v.raw, kBits)};
}

// ------------------------------ Ror
#ifdef HWY_NATIVE_ROL_ROR_8
#undef HWY_NATIVE_ROL_ROR_8
#else
#define HWY_NATIVE_ROL_ROR_8
#endif

#ifdef HWY_NATIVE_ROL_ROR_16
#undef HWY_NATIVE_ROL_ROR_16
#else
#define HWY_NATIVE_ROL_ROR_16
#endif

#ifdef HWY_NATIVE_ROL_ROR_32_64
#undef HWY_NATIVE_ROL_ROR_32_64
#else
#define HWY_NATIVE_ROL_ROR_32_64
#endif

template <class T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> Ror(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vrotr_b(a.raw, b.raw)};
}

template <class T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> Ror(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vrotr_h(a.raw, b.raw)};
}

template <class T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> Ror(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vrotr_w(a.raw, b.raw)};
}

template <class T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> Ror(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{__lsx_vrotr_d(a.raw, b.raw)};
}

// Rol is generic for all vector lengths
template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V Rol(V a, V b) {
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;

  return Ror(a, BitCast(d, Neg(BitCast(di, b))));
}

// ------------------------------ RotateLeftSame/RotateRightSame

#ifdef HWY_NATIVE_ROL_ROR_SAME_8
#undef HWY_NATIVE_ROL_ROR_SAME_8
#else
#define HWY_NATIVE_ROL_ROR_SAME_8
#endif

#ifdef HWY_NATIVE_ROL_ROR_SAME_16
#undef HWY_NATIVE_ROL_ROR_SAME_16
#else
#define HWY_NATIVE_ROL_ROR_SAME_16
#endif

#ifdef HWY_NATIVE_ROL_ROR_SAME_32_64
#undef HWY_NATIVE_ROL_ROR_SAME_32_64
#else
#define HWY_NATIVE_ROL_ROR_SAME_32_64
#endif

// RotateLeftSame/RotateRightSame are generic for all vector lengths
template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V RotateLeftSame(V v, int bits) {
  using T = TFromV<V>;
  const DFromV<decltype(v)> d;
  return Rol(v, Set(d, static_cast<T>(bits)));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V RotateRightSame(V v, int bits) {
  using T = TFromV<V>;
  const DFromV<decltype(v)> d;
  return Ror(v, Set(d, static_cast<T>(bits)));
}

// ------------------------------ BroadcastSignBit

template <typename T, size_t N, HWY_IF_SIGNED(T)>
HWY_API Vec128<T, N> BroadcastSignBit(const Vec128<T, N> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}

// ------------------------------ Integer Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <size_t N>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{__lsx_vabsd_b(v.raw, __lsx_vreplgr2vr_b(0))};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{__lsx_vabsd_h(v.raw, __lsx_vreplgr2vr_b(0))};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{__lsx_vabsd_w(v.raw, __lsx_vreplgr2vr_b(0))};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Abs(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{__lsx_vabsd_d(v.raw, __lsx_vreplgr2vr_b(0))};
}

// ------------------------------ SaturatedAbs

#ifdef HWY_NATIVE_SATURATED_ABS
#undef HWY_NATIVE_SATURATED_ABS
#else
#define HWY_NATIVE_SATURATED_ABS
#endif

template <class V, HWY_IF_I8(TFromV<V>)>
HWY_API V SaturatedAbs(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Min(BitCast(du, v), BitCast(du, SaturatedSub(Zero(d), v))));
}
template <class V, HWY_IF_I16(TFromV<V>)>
HWY_API V SaturatedAbs(V v) {
  return Max(v, SaturatedSub(Zero(DFromV<V>()), v));
}
template <class V, HWY_IF_I32(TFromV<V>)>
HWY_API V SaturatedAbs(V v) {
  const auto abs_v = Abs(v);
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Min(BitCast(du, abs_v),
                        Set(du, static_cast<uint32_t>(LimitsMax<int32_t>()))));
}
template <class V, HWY_IF_I64(TFromV<V>)>
HWY_API V SaturatedAbs(V v) {
  const auto abs_v = Abs(v);
  return Add(abs_v, BroadcastSignBit(abs_v));
}

// ------------------------------ IfNegativeThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(no)> d;
  const RebindToSigned<decltype(d)> di;

  Mask128<T, N> m = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(m, yes, no);
}

// ------------------------------ IfNegativeThenNegOrUndefIfZero

#ifdef HWY_NATIVE_INTEGER_IF_NEGATIVE_THEN_NEG
#undef HWY_NATIVE_INTEGER_IF_NEGATIVE_THEN_NEG
#else
#define HWY_NATIVE_INTEGER_IF_NEGATIVE_THEN_NEG
#endif

template <size_t N>
HWY_API Vec128<int8_t, N> IfNegativeThenNegOrUndefIfZero(Vec128<int8_t, N> mask,
                                                         Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{__lsx_vsigncov_b(mask.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int16_t, N> IfNegativeThenNegOrUndefIfZero(
    Vec128<int16_t, N> mask, Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{__lsx_vsigncov_h(mask.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> IfNegativeThenNegOrUndefIfZero(
    Vec128<int32_t, N> mask, Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{__lsx_vsigncov_w(mask.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, N> IfNegativeThenNegOrUndefIfZero(
    Vec128<int64_t, N> mask, Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{__lsx_vsigncov_d(mask.raw, v.raw)};
}

// ------------------------------ ShiftLeftSame/ShiftRightSame

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, int bits) {
  return v << Set(DFromV<decltype(v)>(), static_cast<T>(bits));
}
template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightSame(const Vec128<T, N> v, int bits) {
  return v >> Set(DFromV<decltype(v)>(), static_cast<T>(bits));
}

// ------------------------------ Integer/Float Div

#ifdef HWY_NATIVE_INT_DIV
#undef HWY_NATIVE_INT_DIV
#else
#define HWY_NATIVE_INT_DIV
#endif

template <size_t N>
HWY_API Vec128<int8_t, N> operator/(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int8_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vdiv.b %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int8_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> operator/(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vdiv.bu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint8_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int16_t, N> operator/(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int16_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vdiv.h %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int16_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint16_t, N> operator/(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vdiv.hu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint16_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator/(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int32_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vdiv.w %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int32_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator/(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vdiv.wu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint32_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int64_t, N> operator/(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int64_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vdiv.d %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int64_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint64_t, N> operator/(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vdiv.du %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint64_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfdiv_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator/(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfdiv_d(a.raw, b.raw)};
}

// ------------------------------ Integer Mod

template <size_t N>
HWY_API Vec128<int8_t, N> operator%(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int8_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vmod.b %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int8_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> operator%(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vmod.bu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint8_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int16_t, N> operator%(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int16_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vmod.h %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int16_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint16_t, N> operator%(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vmod.hu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint16_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator%(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int32_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vmod.w %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int32_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator%(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vmod.wu %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint32_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<int64_t, N> operator%(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  // or a[i] == LimitsMin<int64_t>() && b[i] == -1
  __m128i raw_result;
  __asm__("vmod.d %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<int64_t, N>{raw_result};
}

template <size_t N>
HWY_API Vec128<uint64_t, N> operator%(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  // Use inline assembly to avoid undefined behavior if any lanes of b are zero
  __m128i raw_result;
  __asm__("vmod.du %w0,%w1,%w2" : "=f"(raw_result) : "f"(a.raw), "f"(b.raw) :);
  return Vec128<uint64_t, N>{raw_result};
}

// ------------------------------ ApproximateReciprocal

#ifdef HWY_NATIVE_F64_APPROX_RECIP
#undef HWY_NATIVE_F64_APPROX_RECIP
#else
#define HWY_NATIVE_F64_APPROX_RECIP
#endif

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrecip_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> ApproximateReciprocal(const Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrecip_d(v.raw)};
}

// ------------------------------ Absolute value of difference

#ifdef HWY_NATIVE_INTEGER_ABS_DIFF
#undef HWY_NATIVE_INTEGER_ABS_DIFF
#else
#define HWY_NATIVE_INTEGER_ABS_DIFF
#endif

template <size_t N>
HWY_API Vec128<int8_t, N> AbsDiff(const Vec128<int8_t, N> a,
                                  Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vabsd_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> AbsDiff(const Vec128<int16_t, N> a,
                                   Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vabsd_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> AbsDiff(const Vec128<int32_t, N> a,
                                   Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vabsd_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> AbsDiff(const Vec128<int64_t, N> a,
                                   Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vabsd_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> AbsDiff(const Vec128<uint8_t, N> a,
                                   Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vabsd_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AbsDiff(const Vec128<uint16_t, N> a,
                                    Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vabsd_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> AbsDiff(const Vec128<uint32_t, N> a,
                                    Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vabsd_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> AbsDiff(const Vec128<uint64_t, N> a,
                                    Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vabsd_du(a.raw, b.raw)};
}

// Generic for all vector lengths.
template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V AbsDiff(V a, V b) {
  return Abs(a - b);
}

// ------------------------------ Integer/Float multiply-add

#ifdef HWY_NATIVE_INT_FMA
#undef HWY_NATIVE_INT_FMA
#else
#define HWY_NATIVE_INT_FMA
#endif

template <size_t N>
HWY_API Vec128<int8_t, N> MulAdd(Vec128<int8_t, N> mul, Vec128<int8_t, N> x,
                                 Vec128<int8_t, N> add) {
  return Vec128<int8_t, N>{__lsx_vmadd_b(add.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulAdd(Vec128<int16_t, N> mul, Vec128<int16_t, N> x,
                                  Vec128<int16_t, N> add) {
  return Vec128<int16_t, N>{__lsx_vmadd_h(add.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> MulAdd(Vec128<int32_t, N> mul, Vec128<int32_t, N> x,
                                  Vec128<int32_t, N> add) {
  return Vec128<int32_t, N>{__lsx_vmadd_w(add.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> MulAdd(Vec128<int64_t, N> mul, Vec128<int64_t, N> x,
                                  Vec128<int64_t, N> add) {
  return Vec128<int64_t, N>{__lsx_vmadd_d(add.raw, mul.raw, x.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> MulAdd(Vec128<float, N> mul, Vec128<float, N> x,
                                Vec128<float, N> add) {
  return Vec128<float, N>{__lsx_vfmadd_s(mul.raw, x.raw, add.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> MulAdd(Vec128<double, N> mul, Vec128<double, N> x,
                                 Vec128<double, N> add) {
  return Vec128<double, N>{__lsx_vfmadd_d(mul.raw, x.raw, add.raw)};
}

// Unsinged
template <typename T, size_t N, HWY_IF_UNSIGNED(T)>
HWY_API Vec128<T, N> MulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> add) {
  return mul * x + add;
}

// ------------------------------ Integer/Float NegMulAdd

template <size_t N>
HWY_API Vec128<int8_t, N> NegMulAdd(Vec128<int8_t, N> mul, Vec128<int8_t, N> x,
                                    Vec128<int8_t, N> add) {
  return Vec128<int8_t, N>{__lsx_vmsub_b(add.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> NegMulAdd(Vec128<int16_t, N> mul,
                                     Vec128<int16_t, N> x,
                                     Vec128<int16_t, N> add) {
  return Vec128<int16_t, N>{__lsx_vmsub_h(add.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> NegMulAdd(Vec128<int32_t, N> mul,
                                     Vec128<int32_t, N> x,
                                     Vec128<int32_t, N> sub) {
  return Vec128<int32_t, N>{__lsx_vmsub_w(sub.raw, mul.raw, x.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> NegMulAdd(Vec128<int64_t, N> mul,
                                     Vec128<int64_t, N> x,
                                     Vec128<int64_t, N> sub) {
  return Vec128<int64_t, N>{__lsx_vmsub_d(sub.raw, mul.raw, x.raw)};
}

// Float/unsigned
template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> NegMulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> add) {
  return add - mul * x;
}

// ------------------------------ Float MulSub

// float
template <size_t N>
HWY_API Vec128<float, N> MulSub(Vec128<float, N> mul, Vec128<float, N> x,
                                Vec128<float, N> sub) {
  return Vec128<float, N>{__lsx_vfmsub_s(x.raw, mul.raw, sub.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> MulSub(Vec128<double, N> mul, Vec128<double, N> x,
                                 Vec128<double, N> sub) {
  return Vec128<double, N>{__lsx_vfmsub_d(x.raw, mul.raw, sub.raw)};
}

// unsigned
template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> MulSub(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> sub) {
  return mul * x - sub;
}

// ------------------------------ Float NegMulSub

// float/unsigned
template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> NegMulSub(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> sub) {
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

template <size_t N>
HWY_API Vec128<float, N> Sqrt(Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfsqrt_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Sqrt(Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfsqrt_d(v.raw)};
}

// ------------------------------ ApproximateReciprocalSqrt
#ifdef HWY_NATIVE_F64_APPROX_RSQRT
#undef HWY_NATIVE_F64_APPROX_RSQRT
#else
#define HWY_NATIVE_F64_APPROX_RSQRT
#endif

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrsqrt_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> ApproximateReciprocalSqrt(Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrsqrt_d(v.raw)};
}

// ------------------------------ Min

template <size_t N>
HWY_API Vec128<uint8_t, N> Min(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vmin_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Min(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vmin_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Min(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vmin_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Min(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vmin_du(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> Min(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vmin_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Min(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vmin_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Min(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vmin_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Min(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vmin_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Min(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfmin_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Min(Vec128<double, N> a, Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfmin_d(a.raw, b.raw)};
}

// ------------------------------ Max

template <size_t N>
HWY_API Vec128<uint8_t, N> Max(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{__lsx_vmax_bu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Max(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{__lsx_vmax_hu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Max(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{__lsx_vmax_wu(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Max(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{__lsx_vmax_du(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> Max(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{__lsx_vmax_b(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Max(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{__lsx_vmax_h(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Max(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{__lsx_vmax_w(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Max(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{__lsx_vmax_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Max(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfmax_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Max(Vec128<double, N> a, Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfmax_d(a.raw, b.raw)};
}

// ------------------------------ MinMagnitude and MaxMagnitude

#ifdef HWY_NATIVE_FLOAT_MIN_MAX_MAGNITUDE
#undef HWY_NATIVE_FLOAT_MIN_MAX_MAGNITUDE
#else
#define HWY_NATIVE_FLOAT_MIN_MAX_MAGNITUDE
#endif

template <size_t N>
HWY_API Vec128<float, N> MinMagnitude(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfmina_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> MinMagnitude(Vec128<double, N> a,
                                       Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfmina_d(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> MaxMagnitude(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{__lsx_vfmaxa_s(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> MaxMagnitude(Vec128<double, N> a,
                                       Vec128<double, N> b) {
  return Vec128<double, N>{__lsx_vfmaxa_d(a.raw, b.raw)};
}

// ------------------------------ Non-temporal stores

// Same as aligned stores on non-x86.

template <class D>
HWY_API void Stream(const VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT aligned) {
  __builtin_prefetch(aligned, 1, 0);
  Store(v, d, aligned);
}

// ------------------------------ Scatter in generic_ops-inl.h
// ------------------------------ Gather in generic_ops-inl.h

// ================================================== SWIZZLE (2)

// ------------------------------ LowerHalf

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> LowerHalf(D /* tag */, VFromD<Twice<D>> v) {
  return VFromD<D>{v.raw};
}
template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftLeftBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  if (kBytes == 0) return v;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lsx_vbsll_v(BitCast(du, v).raw, kBytes)});
}

// Generic for all vector lengths.
template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

// Generic for all vector lengths.
template <int kLanes, class D>
HWY_API VFromD<D> ShiftLeftLanes(D d, const VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(TFromD<D>)>(BitCast(d8, v)));
}

// Generic for all vector lengths.
template <int kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftRightBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  if (kBytes == 0) return v;
  const RebindToUnsigned<decltype(d)> du;
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (d.MaxBytes() != 16) {
    const Full128<TFromD<D>> dfull;
    const VFromD<decltype(dfull)> vfull{v.raw};
    v = VFromD<D>{IfThenElseZero(FirstN(dfull, MaxLanes(d)), vfull).raw};
  }
  return BitCast(
      d, VFromD<decltype(du)>{__lsx_vbsrl_v(BitCast(du, v).raw, kBytes)});
}

// ------------------------------ ShiftRightLanes
// Generic for all vector lengths.
template <int kLanes, class D>
HWY_API VFromD<D> ShiftRightLanes(D d, const VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr size_t kBytes = kLanes * sizeof(TFromD<D>);
  return BitCast(d, ShiftRightBytes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

template <class D, HWY_IF_V_SIZE_D(D, 8)>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  const Twice<RebindToUnsigned<decltype(d)>> dut;
  using VUT = VFromD<decltype(dut)>;  // for float16_t
  const VUT vut = BitCast(dut, v);
  return BitCast(d, LowerHalf(VUT{__lsx_vilvh_d(vut.raw, vut.raw)}));
}

// Partial
template <class D, HWY_IF_V_SIZE_LE_D(D, 4)>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  return LowerHalf(d, ShiftRightBytes<d.MaxBytes()>(Twice<D>(), v));
}

// ------------------------------ ExtractLane (UpperHalf)

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  return static_cast<T>(__lsx_vpickve2gr_b(v.raw, kLane) & 0xFF);
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const uint16_t lane = static_cast<uint16_t>(
      __lsx_vpickve2gr_hu(BitCast(du, v).raw, kLane) & 0xFFFF);
  return BitCastScalar<T>(lane);
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  return static_cast<T>(__lsx_vpickve2gr_w(v.raw, kLane));
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  return static_cast<T>(__lsx_vpickve2gr_d(v.raw, kLane));
}

template <size_t kLane, size_t N>
HWY_INLINE float ExtractLane(const Vec128<float, N> v) {
  float f32;
  int32_t i32 = __lsx_vpickve2gr_w(reinterpret_cast<__m128i>(v.raw), kLane);
  CopyBytes<4>(&i32, &f32);
  return f32;
}
template <size_t kLane, size_t N>
HWY_INLINE double ExtractLane(const Vec128<double, N> v) {
  double f64;
  int64_t i64 = __lsx_vpickve2gr_d(reinterpret_cast<__m128i>(v.raw), kLane);
  CopyBytes<8>(&i64, &f64);
  return f64;
}

}  // namespace detail

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 1> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return GetLane(v);
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 2> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
    }
  }
#endif
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 4> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
    }
  }
#endif
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 8> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
    }
  }
#endif
  alignas(16) T lanes[8];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 16> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
      case 8:
        return detail::ExtractLane<8>(v);
      case 9:
        return detail::ExtractLane<9>(v);
      case 10:
        return detail::ExtractLane<10>(v);
      case 11:
        return detail::ExtractLane<11>(v);
      case 12:
        return detail::ExtractLane<12>(v);
      case 13:
        return detail::ExtractLane<13>(v);
      case 14:
        return detail::ExtractLane<14>(v);
      case 15:
        return detail::ExtractLane<15>(v);
    }
  }
#endif
  alignas(16) T lanes[16];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (UpperHalf)

namespace detail {

template <class V>
HWY_INLINE V InsertLaneUsingBroadcastAndBlend(V v, size_t i, TFromV<V> t) {
  const DFromV<decltype(v)> d;

#if HWY_TARGET <= HWY_AVX3
  using RawMask = decltype(MaskFromVec(VFromD<decltype(d)>()).raw);
  const auto mask = MFromD<decltype(d)>{static_cast<RawMask>(uint64_t{1} << i)};
#else
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const auto mask = RebindMask(d, Iota(du, 0) == Set(du, static_cast<TU>(i)));
#endif

  return IfThenElse(mask, Set(d, t), v);
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{__lsx_vinsgr2vr_b(v.raw, t, kLane)};
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const uint16_t bits = BitCastScalar<uint16_t>(t);
  return BitCast(d, VFromD<decltype(du)>{
                        __lsx_vinsgr2vr_h(BitCast(du, v).raw, bits, kLane)});
}
template <size_t kLane, typename T, size_t N, HWY_IF_UI32(T)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{__lsx_vinsgr2vr_w(v.raw, t, kLane)};
}
template <size_t kLane, typename T, size_t N, HWY_IF_UI64(T)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{__lsx_vinsgr2vr_d(v.raw, t, kLane)};
}

template <size_t kLane, size_t N>
HWY_INLINE Vec128<float, N> InsertLane(const Vec128<float, N> v, float t) {
  static_assert(kLane < N, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  int ti = BitCastScalar<int>(t);
  RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vinsgr2vr_w(
                        reinterpret_cast<__m128i>(v.raw), ti, kLane)});
}

template <size_t kLane>
HWY_INLINE Vec128<double> InsertLane(const Vec128<double> v, double t) {
  static_assert(kLane < 2, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  long int ti = BitCastScalar<long int>(t);
  RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vinsgr2vr_d(
                        reinterpret_cast<__m128i>(v.raw), ti, kLane)});
}

}  // namespace detail

template <typename T>
HWY_API Vec128<T, 1> InsertLane(const Vec128<T, 1> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  return Set(DFromV<decltype(v)>(), t);
}

template <typename T>
HWY_API Vec128<T, 2> InsertLane(const Vec128<T, 2> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 4> InsertLane(const Vec128<T, 4> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 8> InsertLane(const Vec128<T, 8> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 16> InsertLane(const Vec128<T, 16> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
      case 8:
        return detail::InsertLane<8>(v, t);
      case 9:
        return detail::InsertLane<9>(v, t);
      case 10:
        return detail::InsertLane<10>(v, t);
      case 11:
        return detail::InsertLane<11>(v, t);
      case 12:
        return detail::InsertLane<12>(v, t);
      case 13:
        return detail::InsertLane<13>(v, t);
      case 14:
        return detail::InsertLane<14>(v, t);
      case 15:
        return detail::InsertLane<15>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

// ------------------------------ CombineShiftRightBytes
template <int kBytes, class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  static_assert(0 < kBytes && kBytes < 16, "kBytes invalid");
  return Or(ShiftRightBytes<kBytes>(d, lo), ShiftLeftBytes<16 - kBytes>(d, hi));
}
template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  constexpr size_t kSize = d.MaxBytes();
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");

  const Twice<decltype(d)> dt;
  return VFromD<D>{ShiftRightBytes<kBytes>(dt, Combine(dt, hi, lo)).raw};
}

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{__lsx_vreplvei_b(v.raw, kLane)};
}
template <int kLane, typename T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{__lsx_vreplvei_h(v.raw, kLane)};
}
template <int kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<int32_t, N>{__lsx_vreplvei_w(
                        reinterpret_cast<__m128i>(v.raw), kLane)});
}
template <int kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec128<int64_t, N>{__lsx_vreplvei_d(
                        reinterpret_cast<__m128i>(v.raw), kLane)});
}

// ------------------------------ TableLookupLanes (Shuffle01)

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T, size_t N = 16 / sizeof(T)>
struct Indices128 {
  __m128i raw;
};

namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  return Iota(d8, 0);
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14};
  return Load(d8, kBroadcastLaneBytes);
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
  return Load(d8, kBroadcastLaneBytes);
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
  return Load(d8, kBroadcastLaneBytes);
}

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  return Zero(d8);
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kByteOffsets[16] = {
      0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
  return Load(d8, kByteOffsets);
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kByteOffsets[16] = {
      0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
  return Load(d8, kByteOffsets);
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t kByteOffsets[16] = {
      0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
  return Load(d8, kByteOffsets);
}

}  // namespace detail

template <class D, typename TI, HWY_IF_T_SIZE_D(D, 1)>
HWY_API Indices128<TFromD<D>, MaxLanes(D())> IndicesFromVec(
    D d, Vec128<TI, MaxLanes(D())> vec) {
  using T = TFromD<D>;
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  HWY_DASSERT(AllTrue(
      du, Lt(BitCast(du, vec), Set(du, static_cast<TU>(MaxLanes(d) * 2)))));
#endif

  (void)d;
  return Indices128<TFromD<D>, MaxLanes(D())>{BitCast(d, vec).raw};
}

template <class D, typename TI,
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 2) | (1 << 4) | (1 << 8))>
HWY_API Indices128<TFromD<D>, MaxLanes(D())> IndicesFromVec(
    D d, Vec128<TI, MaxLanes(D())> vec) {
  using T = TFromD<D>;
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  HWY_DASSERT(AllTrue(
      du, Lt(BitCast(du, vec), Set(du, static_cast<TU>(MaxLanes(d) * 2)))));
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;

  // Broadcast each lane index to all bytes of T and shift to bytes
  const V8 lane_indices = TableLookupBytes(
      BitCast(d8, vec), detail::IndicesFromVecBroadcastLaneBytes(d));
  constexpr int kIndexShiftAmt = static_cast<int>(FloorLog2(sizeof(T)));
  const V8 byte_indices = ShiftLeft<kIndexShiftAmt>(lane_indices);
  const V8 sum = Add(byte_indices, detail::IndicesFromVecByteOffsets(d));
  return Indices128<TFromD<D>, MaxLanes(D())>{sum.raw};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename TI>
HWY_API Indices128<TFromD<D>, MaxLanes(D())> SetTableIndices(D d,
                                                             const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  using TI = MakeSigned<T>;
  const DFromV<decltype(v)> d;
  const Rebind<TI, decltype(d)> di;
  auto t1 = TableLookupBytes(BitCast(di, v), Vec128<TI, N>{idx.raw});
  return BitCast(d, t1);
}

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> TableLookupLanes(Vec128<T, 1> v,
                                      Indices128<T, 1> /* idx */) {
  return v;
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ReverseBlocks(D /* tag */, VFromD<D> v) {
  return v;
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301)

// Single lane: no change
template <class D, HWY_IF_LANES_D(D, 1)>
HWY_API VFromD<D> Reverse(D /* tag */, VFromD<D> v) {
  return v;
}
// 32-bit x2: shuffle
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse(D /* tag */, const VFromD<D> v) {
  return VFromD<D>{Shuffle2301(Vec128<TFromD<D>>{v.raw}).raw};
}
// 64-bit x2: shuffle
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse(D /* tag */, const VFromD<D> v) {
  return Shuffle01(v);
}
// 32-bit x4: shuffle
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse(D /* tag */, const VFromD<D> v) {
  return Shuffle0123(v);
}

// 16-bit
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2),
          HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const VU vu = BitCast(du, v);
  constexpr size_t kN = MaxLanes(d);
  if (kN == 1) return v;
  if (kN == 2) {
    return BitCast(d, VU{__lsx_vshuf4i_h(vu.raw, 0x11)});
  }
  if (kN == 4) {
    return BitCast(d, VU{__lsx_vshuf4i_h(vu.raw, 0x1B)});
  }
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> shuffle = Dup128VecFromValues(
      di, 0x0F0E, 0x0D0C, 0x0B0A, 0x0908, 0x0706, 0x0504, 0x0302, 0x0100);
  return BitCast(d, TableLookupBytes(v, shuffle));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1),
          HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  static constexpr int kN = static_cast<int>(MaxLanes(d));
  if (kN == 1) return v;
  alignas(16) static constexpr int8_t _tmp_data[] = {
      kN - 1, kN - 2,  kN - 3,  kN - 4,  kN - 5,  kN - 6,  kN - 7,  kN - 8,
      kN - 9, kN - 10, kN - 11, kN - 12, kN - 13, kN - 14, kN - 15, kN - 16};
  return VFromD<D>{__lsx_vshuf_b(v.raw, v.raw, __lsx_vld(_tmp_data, 0))};
}

// ------------------------------ Reverse2

// Single lane: no change
template <class D, HWY_IF_LANES_D(D, 1)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return v;
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> dw;
  return BitCast(d, RotateRight<16>(BitCast(dw, v)));
}

// Generic for all vector lengths.
template <class D, HWY_IF_T_SIZE_D(D, 4), HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return Shuffle2301(v);
}

// Generic for all vector lengths.
template <class D, HWY_IF_T_SIZE_D(D, 8), HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse4(D /* tag */, VFromD<D> v) {
  return VFromD<D>{__lsx_vshuf4i_h(v.raw, 0x1B)};
}

// Generic for all vector lengths.
template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse4(D /* tag */, const VFromD<D> v) {
  return Shuffle0123(v);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse4(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 4 u64 lanes
}

// ------------------------------ Reverse8

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
  const RepartitionToWide<decltype(d)> dw;
  return Reverse2(d, BitCast(d, Shuffle0123(BitCast(dw, v))));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> Reverse8(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 8 lanes if larger than 16-bit
}

// ------------------------------ InterleaveUpper (UpperHalf)

// Full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vilvh_b(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vilvh_h(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<decltype(d)> df;
  return BitCast(d, VFromD<decltype(df)>{
                        __lsx_vilvh_w(BitCast(df, b).raw, BitCast(df, a).raw)});
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<decltype(d)> dd;
  return BitCast(d, VFromD<decltype(dd)>{
                        __lsx_vilvh_d(BitCast(dd, b).raw, BitCast(dd, a).raw)});
}

// Partial
template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, VFromD<D>{UpperHalf(d2, a).raw},
                         VFromD<D>{UpperHalf(d2, b).raw});
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== CONVERT (1)

// ------------------------------ PromoteTo unsigned
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint8_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_hu_bu(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_wu_hu(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_du_wu(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint8_t, D>> v) {
  const __m128i u16 = __lsx_vsllwil_hu_bu(v.raw, 0);
  return VFromD<D>{__lsx_vsllwil_wu_hu(u16, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<uint8_t, D>> v) {
  const Rebind<uint32_t, decltype(d)> du32;
  return PromoteTo(d, PromoteTo(du32, v));
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /*tag*/, VFromD<Rebind<uint16_t, D>> v) {
  const __m128i u32 = __lsx_vsllwil_wu_hu(v.raw, 0);
  return VFromD<D>{__lsx_vsllwil_du_wu(u32, 0)};
}

// Unsigned to signed: same plus cast.
template <class D, class V, HWY_IF_SIGNED_D(D), HWY_IF_UNSIGNED_V(V),
          HWY_IF_LANES_GT(sizeof(TFromD<D>), sizeof(TFromV<V>)),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_V(V))>
HWY_API VFromD<D> PromoteTo(D di, V v) {
  const RebindToUnsigned<decltype(di)> du;
  return BitCast(di, PromoteTo(du, v));
}

// signed
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int8_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_h_b(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_w_h(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{__lsx_vsllwil_d_w(v.raw, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int8_t, D>> v) {
  const __m128i i16 = __lsx_vsllwil_h_b(v.raw, 0);
  return VFromD<D>{__lsx_vsllwil_w_h(i16, 0)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<int8_t, D>> v) {
  const Rebind<int32_t, decltype(d)> di32;
  return PromoteTo(d, PromoteTo(di32, v));
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /*tag*/, VFromD<Rebind<int16_t, D>> v) {
  const __m128i i32 = __lsx_vsllwil_w_h(v.raw, 0);
  return VFromD<D>{__lsx_vsllwil_d_w(i32, 0)};
}

// -------------------- PromoteTo float

#ifdef HWY_NATIVE_F16C
#undef HWY_NATIVE_F16C
#else
#define HWY_NATIVE_F16C
#endif

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<hwy::float16_t, D>> v) {
  return VFromD<D>{__lsx_vfcvtl_s_h(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{__lsx_vfcvtl_d_s(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{__lsx_vffintl_d_w(v.raw)};
}

template <class D, HWY_IF_F64_D(D), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> PromoteTo(D df64, VFromD<Rebind<uint32_t, D>> v) {
  const Rebind<int32_t, decltype(df64)> di32;
  const auto i32_to_f64_result = PromoteTo(df64, BitCast(di32, v));
  return i32_to_f64_result + IfNegativeThenElse(i32_to_f64_result,
                                                Set(df64, 4294967296.0),
                                                Zero(df64));
}

template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<hwy::bfloat16_t, D>> v) {
  const RebindToSigned<decltype(d)> di32;
  const Rebind<uint16_t, decltype(d)> du16;
  return BitCast(d, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ------------------------------ Per4LaneBlockShuffle

namespace detail {

#ifdef HWY_NATIVE_PER4LANEBLKSHUF_DUP32
#undef HWY_NATIVE_PER4LANEBLKSHUF_DUP32
#else
#define HWY_NATIVE_PER4LANEBLKSHUF_DUP32
#endif

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_INLINE VFromD<D> Per4LaneBlkShufDupSet4xU32(D d, const uint32_t x3,
                                                const uint32_t x2,
                                                const uint32_t x1,
                                                const uint32_t x0) {
  typedef uint32_t GccU32RawVectType __attribute__((__vector_size__(16)));
  const GccU32RawVectType raw = {x0, x1, x2, x3};
  return ResizeBitCast(d, Vec128<uint32_t>{reinterpret_cast<__m128i>(raw)});
}

template <size_t kIdx3210, size_t kVectSize, class V,
          HWY_IF_LANES_LE(kVectSize, 16)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<1> /*lane_size_tag*/,
                                  hwy::SizeTag<kVectSize> /*vect_size_tag*/,
                                  V v) {
  constexpr int kShuffle = static_cast<int>(kIdx3210 & 0xFF);
  return V{__lsx_vshuf4i_b(v.raw, kShuffle)};
}

template <size_t kIdx3210, size_t kVectSize, class V,
          HWY_IF_LANES_LE(kVectSize, 16)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<2> /*lane_size_tag*/,
                                  hwy::SizeTag<kVectSize> /*vect_size_tag*/,
                                  V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;  // for float16_t
  constexpr int kShuffle = static_cast<int>(kIdx3210 & 0xFF);
  return BitCast(
      d, VFromD<decltype(du)>{__lsx_vshuf4i_h(BitCast(du, v).raw, kShuffle)});
}

template <size_t kIdx3210, class V>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<4> /*lane_size_tag*/,
                                  hwy::SizeTag<16> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  constexpr int kShuffle = static_cast<int>(kIdx3210 & 0xFF);
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vshuf4i_w(
                        reinterpret_cast<__m128i>(v.raw), kShuffle)});
}

}  // namespace detail

// ------------------------------ SlideUpLanes

namespace detail {

template <class V, HWY_IF_V_SIZE_LE_V(V, 8)>
HWY_INLINE V SlideUpLanes(V v, size_t amt) {
  const DFromV<decltype(v)> d;
  const Full64<uint64_t> du64;
  const auto vu64 = ResizeBitCast(du64, v);
  return ResizeBitCast(
      d, ShiftLeftSame(vu64, static_cast<int>(amt * sizeof(TFromV<V>) * 8)));
}

template <class V, HWY_IF_V_SIZE_V(V, 16)>
HWY_INLINE V SlideUpLanes(V v, size_t amt) {
  const DFromV<decltype(v)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const auto idx =
      Iota(du8, static_cast<uint8_t>(size_t{0} - amt * sizeof(TFromV<V>)));
  return BitCast(d, TableLookupBytesOr0(BitCast(du8, v), idx));
}

}  // namespace detail

template <class D, HWY_IF_LANES_D(D, 1)>
HWY_API VFromD<D> SlideUpLanes(D /*d*/, VFromD<D> v, size_t /*amt*/) {
  return v;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 2)>
HWY_API VFromD<D> SlideUpLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftLeftLanes<1>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideUpLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 4)>
HWY_API VFromD<D> SlideUpLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftLeftLanes<1>(d, v);
      case 2:
        return ShiftLeftLanes<2>(d, v);
      case 3:
        return ShiftLeftLanes<3>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideUpLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 8)>
HWY_API VFromD<D> SlideUpLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftLeftLanes<1>(d, v);
      case 2:
        return ShiftLeftLanes<2>(d, v);
      case 3:
        return ShiftLeftLanes<3>(d, v);
      case 4:
        return ShiftLeftLanes<4>(d, v);
      case 5:
        return ShiftLeftLanes<5>(d, v);
      case 6:
        return ShiftLeftLanes<6>(d, v);
      case 7:
        return ShiftLeftLanes<7>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideUpLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_LANES_D(D, 16)>
HWY_API VFromD<D> SlideUpLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftLeftLanes<1>(d, v);
      case 2:
        return ShiftLeftLanes<2>(d, v);
      case 3:
        return ShiftLeftLanes<3>(d, v);
      case 4:
        return ShiftLeftLanes<4>(d, v);
      case 5:
        return ShiftLeftLanes<5>(d, v);
      case 6:
        return ShiftLeftLanes<6>(d, v);
      case 7:
        return ShiftLeftLanes<7>(d, v);
      case 8:
        return ShiftLeftLanes<8>(d, v);
      case 9:
        return ShiftLeftLanes<9>(d, v);
      case 10:
        return ShiftLeftLanes<10>(d, v);
      case 11:
        return ShiftLeftLanes<11>(d, v);
      case 12:
        return ShiftLeftLanes<12>(d, v);
      case 13:
        return ShiftLeftLanes<13>(d, v);
      case 14:
        return ShiftLeftLanes<14>(d, v);
      case 15:
        return ShiftLeftLanes<15>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideUpLanes(v, amt);
}

// ------------------------------ SlideDownLanes

namespace detail {

template <class V, HWY_IF_V_SIZE_LE_V(V, 8)>
HWY_INLINE V SlideDownLanes(V v, size_t amt) {
  const DFromV<decltype(v)> d;
  const Repartition<UnsignedFromSize<d.MaxBytes()>, decltype(d)> dv;
  return BitCast(d,
                 ShiftRightSame(BitCast(dv, v),
                                static_cast<int>(amt * sizeof(TFromV<V>) * 8)));
}

template <class V, HWY_IF_V_SIZE_V(V, 16)>
HWY_INLINE V SlideDownLanes(V v, size_t amt) {
  const DFromV<decltype(v)> d;
  const Repartition<int8_t, decltype(d)> di8;
  auto idx = Iota(di8, static_cast<int8_t>(amt * sizeof(TFromV<V>)));
  idx = Or(idx, VecFromMask(di8, idx > Set(di8, int8_t{15})));
  return BitCast(d, TableLookupBytesOr0(BitCast(di8, v), idx));
}

}  // namespace detail

template <class D, HWY_IF_LANES_D(D, 1)>
HWY_API VFromD<D> SlideDownLanes(D /*d*/, VFromD<D> v, size_t /*amt*/) {
  return v;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 2)>
HWY_API VFromD<D> SlideDownLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftRightLanes<1>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideDownLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 4)>
HWY_API VFromD<D> SlideDownLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftRightLanes<1>(d, v);
      case 2:
        return ShiftRightLanes<2>(d, v);
      case 3:
        return ShiftRightLanes<3>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideDownLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_LANES_D(D, 8)>
HWY_API VFromD<D> SlideDownLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftRightLanes<1>(d, v);
      case 2:
        return ShiftRightLanes<2>(d, v);
      case 3:
        return ShiftRightLanes<3>(d, v);
      case 4:
        return ShiftRightLanes<4>(d, v);
      case 5:
        return ShiftRightLanes<5>(d, v);
      case 6:
        return ShiftRightLanes<6>(d, v);
      case 7:
        return ShiftRightLanes<7>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideDownLanes(v, amt);
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_LANES_D(D, 16)>
HWY_API VFromD<D> SlideDownLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(amt)) {
    switch (amt) {
      case 0:
        return v;
      case 1:
        return ShiftRightLanes<1>(d, v);
      case 2:
        return ShiftRightLanes<2>(d, v);
      case 3:
        return ShiftRightLanes<3>(d, v);
      case 4:
        return ShiftRightLanes<4>(d, v);
      case 5:
        return ShiftRightLanes<5>(d, v);
      case 6:
        return ShiftRightLanes<6>(d, v);
      case 7:
        return ShiftRightLanes<7>(d, v);
      case 8:
        return ShiftRightLanes<8>(d, v);
      case 9:
        return ShiftRightLanes<9>(d, v);
      case 10:
        return ShiftRightLanes<10>(d, v);
      case 11:
        return ShiftRightLanes<11>(d, v);
      case 12:
        return ShiftRightLanes<12>(d, v);
      case 13:
        return ShiftRightLanes<13>(d, v);
      case 14:
        return ShiftRightLanes<14>(d, v);
      case 15:
        return ShiftRightLanes<15>(d, v);
    }
  }
#else
  (void)d;
#endif

  return detail::SlideDownLanes(v, amt);
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// N = N/2 + N/2 (upper half undefined)
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), class VH = VFromD<Half<D>>>
HWY_API VFromD<D> Combine(D d, VH hi_half, VH lo_half) {
  const Half<decltype(d)> dh;
  const RebindToUnsigned<decltype(dh)> duh;
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<dh.MaxBytes()>, 2>;
  const VU lo{BitCast(duh, lo_half).raw};
  const VU hi{BitCast(duh, hi_half).raw};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine)
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ZeroExtendVector(D d, VFromD<Half<D>> lo) {
  return Combine(d, Zero(Half<decltype(d)>()), lo);
}

// ------------------------------ Concat full (InterleaveLower)

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ConcatLowerLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveLower(BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ConcatUpperUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveUpper(d64, BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves)
template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ConcatLowerUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  return CombineShiftRightBytes<8>(d, hi, lo);
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  return BitCast(d, Vec128<uint8_t>{__lsx_vshuf4i_d(
                        reinterpret_cast<__m128i>(lo.raw),
                        reinterpret_cast<__m128i>(hi.raw), 0xC)});
}

// ------------------------------ Concat partial (Combine, LowerHalf)

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatLowerLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), LowerHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatUpperUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), UpperHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatLowerUpper(D d, const VFromD<D> hi,
                                   const VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), UpperHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), LowerHalf(d2, lo));
}

// ------------------------------ ConcatOdd

// 8-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatOdd(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  return VFromD<D>{__lsx_vpickod_b(hi.raw, lo.raw)};
}
// 8-bit x8
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatOdd(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickod_b(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_w(_tmp, _tmp, 0x12)};
}
// 8-bit x4
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatOdd(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickod_b(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_h(_tmp, _tmp, 0x14)};
}

// 16-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatOdd(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  return VFromD<D>{__lsx_vpickod_h(hi.raw, lo.raw)};
}
// 16-bit x4
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatOdd(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickod_h(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_w(_tmp, _tmp, 0x12)};
}

// 32-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  return BitCast(
      d, Vec128<uint8_t>{__lsx_vpickod_w(reinterpret_cast<__m128i>(hi.raw),
                                         reinterpret_cast<__m128i>(lo.raw))});
}

// Any T x2
template <class D, HWY_IF_LANES_D(D, 2)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven

// 8-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatEven(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  return VFromD<D>{__lsx_vpickev_b(hi.raw, lo.raw)};
}
// 8-bit x8
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatEven(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickev_b(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_w(_tmp, _tmp, 0x12)};
}
// 8-bit x4
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatEven(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickev_b(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_h(_tmp, _tmp, 0x14)};
}

// 16-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatEven(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  return VFromD<D>{__lsx_vpickev_h(hi.raw, lo.raw)};
}
// 16-bit x4
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatEven(D /* tag */, VFromD<D> hi, VFromD<D> lo) {
  __m128i _tmp = __lsx_vpickev_h(hi.raw, lo.raw);
  return VFromD<D>{__lsx_vextrins_w(_tmp, _tmp, 0x12)};
}

// 32-bit full
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  return BitCast(
      d, Vec128<uint8_t>{__lsx_vpickev_w(reinterpret_cast<__m128i>(hi.raw),
                                         reinterpret_cast<__m128i>(lo.raw))});
}

// Any T x2
template <class D, HWY_IF_LANES_D(D, 2)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  return InterleaveLower(d, lo, hi);
}

template <size_t N>
HWY_INLINE Vec128<float16_t, N> ConcatEven(Vec128<float16_t, N> hi,
                                           Vec128<float16_t, N> lo) {
  const DFromV<decltype(hi)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, ConcatEven(BitCast(du, hi), BitCast(du, lo)));
}
// ------------------------------ DupEven (InterleaveLower)

template <typename T>
HWY_API Vec128<T, 1> DupEven(const Vec128<T, 1> v) {
  return v;
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  __m128i _tmp = __lsx_vpickev_b(v.raw, v.raw);
  return Vec128<T, N>{__lsx_vilvl_b(_tmp, _tmp)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;  // for float16_t
  __m128i _tmp = __lsx_vpickev_h(BitCast(du, v).raw, BitCast(du, v).raw);
  return BitCast(d, VFromD<decltype(du)>{__lsx_vilvl_h(_tmp, _tmp)});
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  __m128i _tmp = detail::BitCastToInteger(v.raw);
  __m128i _tmp1 = __lsx_vpickev_w(_tmp, _tmp);
  return BitCast(d, Vec128<uint32_t, N>{__lsx_vilvl_w(_tmp1, _tmp1)});
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return InterleaveLower(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, 1> DupOdd(Vec128<T, 1> v) {
  return v;
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  __m128i _tmp = __lsx_vpickod_b(v.raw, v.raw);
  return Vec128<T, N>{__lsx_vilvl_b(_tmp, _tmp)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  __m128i _tmp = __lsx_vpickod_h(v.raw, v.raw);
  return Vec128<T, N>{__lsx_vilvl_h(_tmp, _tmp)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  __m128i _tmp = detail::BitCastToInteger(v.raw);
  __m128i _tmp1 = __lsx_vpickod_w(_tmp, _tmp);
  return BitCast(d, Vec128<uint32_t, N>{__lsx_vilvl_w(_tmp1, _tmp1)});
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return InterleaveUpper(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ TwoTablesLookupLanes (DupEven)

template <typename T, size_t N, HWY_IF_V_SIZE_LE(T, N, 8)>
HWY_API Vec128<T, N> TwoTablesLookupLanes(Vec128<T, N> a, Vec128<T, N> b,
                                          Indices128<T, N> idx) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  const Repartition<uint8_t, decltype(dt)> dt_u8;
// TableLookupLanes currently requires table and index vectors to be the same
// size, though a half-length index vector would be sufficient here.
#if HWY_IS_MSAN
  const Vec128<T, N> idx_vec{idx.raw};
  const Indices128<T, N * 2> idx2{Combine(dt, idx_vec, idx_vec).raw};
#else
  // We only keep LowerHalf of the result, which is valid in idx.
  const Indices128<T, N * 2> idx2{idx.raw};
#endif
  return LowerHalf(
      d, TableLookupBytes(Combine(dt, b, a),
                          BitCast(dt, VFromD<decltype(dt_u8)>{idx2.raw})));
}

template <typename T, HWY_IF_UI8(T)>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
  return Vec128<T>{__lsx_vshuf_b(b.raw, a.raw, idx.raw)};
}

template <typename T, HWY_IF_T_SIZE_ONE_OF(T, ((1 << 2) | (1 << 4) | (1 << 8)))>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, TwoTablesLookupLanes(BitCast(du8, a), BitCast(du8, b),
                                         Indices128<uint8_t>{idx.raw}));
}

// ------------------------------ OddEven

template <typename T, size_t N, HWY_IF_UI8(T)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  __m128i t0 = __lsx_vpackod_b(a.raw, a.raw);
  return Vec128<T, N>{__lsx_vpackev_b(t0, b.raw)};
}
template <typename T, size_t N, HWY_IF_UI16(T)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  __m128i t0 = __lsx_vpackod_h(a.raw, a.raw);
  return Vec128<T, N>{__lsx_vpackev_h(t0, b.raw)};
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  __m128i t0 = __lsx_vpackod_w(BitCast(du, a).raw, BitCast(du, a).raw);
  return BitCast(d,
                 VFromD<decltype(du)>{__lsx_vpackev_w(t0, BitCast(du, b).raw)});
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lsx_vextrins_d(
                        BitCast(du, b).raw, BitCast(du, a).raw, 0x11)});
}

// -------------------------- InterleaveEven

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> InterleaveEven(D /*d*/, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vpackev_b(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> InterleaveEven(D /*d*/, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vpackev_h(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> InterleaveEven(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<D> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vpackev_w(BitCast(di, b).raw,
                                                         BitCast(di, a).raw)});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> InterleaveEven(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<D> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vpackev_d(BitCast(di, b).raw,
                                                         BitCast(di, a).raw)});
}

// -------------------------- InterleaveOdd

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> InterleaveOdd(D /*d*/, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vpackod_b(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> InterleaveOdd(D /*d*/, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lsx_vpackod_h(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> InterleaveOdd(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<D> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vpackod_w(BitCast(di, b).raw,
                                                         BitCast(di, a).raw)});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> InterleaveOdd(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToSigned<D> di;
  return BitCast(d, VFromD<decltype(di)>{__lsx_vpackod_d(BitCast(di, b).raw,
                                                         BitCast(di, a).raw)});
}

// ------------------------------ OddEvenBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ InterleaveEvenBlocks
template <class D, class V = VFromD<D>, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API V InterleaveEvenBlocks(D, V a, V /*b*/) {
  return a;
}
// ------------------------------ InterleaveOddBlocks
template <class D, class V = VFromD<D>, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API V InterleaveOddBlocks(D, V a, V /*b*/) {
  return a;
}

// ------------------------------ Shl

template <typename T, size_t N, HWY_IF_UI8(T)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return Vec128<T, N>{__lsx_vsll_b(v.raw, bits.raw)};
}

template <typename T, size_t N, HWY_IF_UI16(T)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return Vec128<T, N>{__lsx_vsll_h(v.raw, bits.raw)};
}

template <typename T, size_t N, HWY_IF_UI32(T)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return Vec128<T, N>{__lsx_vsll_w(v.raw, bits.raw)};
}

template <typename T, size_t N, HWY_IF_UI64(T)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return Vec128<T, N>{__lsx_vsll_d(v.raw, bits.raw)};
}

// ------------------------------ Shr

namespace detail {

template <size_t N>
HWY_API Vec128<uint8_t, N> Shr(Vec128<uint8_t, N> v, Vec128<uint8_t, N> bits) {
  return Vec128<uint8_t, N>{__lsx_vsrl_b(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Shr(Vec128<uint16_t, N> v,
                                Vec128<uint16_t, N> bits) {
  return Vec128<uint16_t, N>{__lsx_vsrl_h(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Shr(Vec128<uint32_t, N> v,
                                Vec128<uint32_t, N> bits) {
  return Vec128<uint32_t, N>{__lsx_vsrl_w(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Shr(Vec128<uint64_t, N> v,
                                Vec128<uint64_t, N> bits) {
  return Vec128<uint64_t, N>{__lsx_vsrl_d(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> Shr(Vec128<int8_t, N> v, Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>{__lsx_vsra_b(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Shr(Vec128<int16_t, N> v, Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>{__lsx_vsra_h(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Shr(Vec128<int32_t, N> v, Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>{__lsx_vsra_w(v.raw, bits.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Shr(Vec128<int64_t, N> v, Vec128<int64_t, N> bits) {
  return Vec128<int64_t, N>{__lsx_vsra_d(v.raw, bits.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, Vec128<T, N> bits) {
  return detail::Shr(v, bits);
}

// ================================================== CONVERT (2)

// ------------------------------ PromoteEvenTo/PromoteOddTo
#include "hwy/ops/inside-inl.h"

// Generic for all vector lengths.
template <class DF, HWY_IF_F32_D(DF),
          class VBF = VFromD<Repartition<bfloat16_t, DF>>>
HWY_API VFromD<DF> WidenMulPairwiseAdd(DF df, VBF a, VBF b) {
  return MulAdd(PromoteEvenTo(df, a), PromoteEvenTo(df, b),
                Mul(PromoteOddTo(df, a), PromoteOddTo(df, b)));
}

template <class D32, HWY_IF_I32_D(D32), HWY_IF_V_SIZE_LE_D(D32, 16),
          class V16 = VFromD<RepartitionToNarrow<D32>>>
HWY_API VFromD<D32> WidenMulPairwiseAdd(D32 /* tag */, V16 a, V16 b) {
  __m128i _tmp = __lsx_vmulwev_w_h(a.raw, b.raw);
  return VFromD<D32>{__lsx_vmaddwod_w_h(_tmp, a.raw, b.raw)};
}

template <class DU32, HWY_IF_U32_D(DU32), HWY_IF_V_SIZE_LE_D(DU32, 16),
          class VU16 = VFromD<RepartitionToNarrow<DU32>>>
HWY_API VFromD<DU32> WidenMulPairwiseAdd(DU32 /* tag */, VU16 a, VU16 b) {
  __m128i _tmp = __lsx_vmulwev_w_hu(a.raw, b.raw);
  return VFromD<DU32>{__lsx_vmaddwod_w_hu(_tmp, a.raw, b.raw)};
}

// ------------------------------ ReorderWidenMulAccumulate

template <class D32, HWY_IF_I32_D(D32), HWY_IF_V_SIZE_LE_D(D32, 16),
          class V16 = VFromD<RepartitionToNarrow<D32>>>
HWY_API VFromD<D32> ReorderWidenMulAccumulate(D32 /* tag */, V16 a, V16 b,
                                              const VFromD<D32> sum0,
                                              VFromD<D32>& /* sum1 */) {
  return VFromD<D32>{__lsx_vmaddwev_w_h(
      __lsx_vmaddwod_w_h(sum0.raw, a.raw, b.raw), a.raw, b.raw)};
}

template <class DU32, HWY_IF_U32_D(DU32),
          class VU16 = VFromD<RepartitionToNarrow<DU32>>>
HWY_API VFromD<DU32> ReorderWidenMulAccumulate(DU32 /* tag */, VU16 a, VU16 b,
                                               const VFromD<DU32> sum0,
                                               VFromD<DU32>& /* sum1 */) {
  return VFromD<DU32>{__lsx_vmaddwev_w_hu(
      __lsx_vmaddwod_w_hu(sum0.raw, a.raw, b.raw), a.raw, b.raw)};
}

// ------------------------------ RearrangeToOddPlusEven
template <size_t N>
HWY_API Vec128<int32_t, N> RearrangeToOddPlusEven(const Vec128<int32_t, N> sum0,
                                                  Vec128<int32_t, N> /*sum1*/) {
  return sum0;  // invariant already holds
}

template <size_t N>
HWY_API Vec128<uint32_t, N> RearrangeToOddPlusEven(
    const Vec128<uint32_t, N> sum0, Vec128<uint32_t, N> /*sum1*/) {
  return sum0;  // invariant already holds
}

template <class VW>
HWY_API VW RearrangeToOddPlusEven(const VW sum0, const VW sum1) {
  return Add(sum0, sum1);
}

// ------------------------------ Demotions

// NOTE: hwy::EnableIf<!hwy::IsSame<V, V>()>* = nullptr is used instead of
// hwy::EnableIf<false>* = nullptr to avoid compiler errors since
// !hwy::IsSame<V, V>() is always false and as !hwy::IsSame<V, V>() will cause
// SFINAE to occur instead of a hard error due to a dependency on the V template
// argument
#undef HWY_IF_U2I_DEMOTE_FROM_LANE_SIZE_V
#define HWY_IF_U2I_DEMOTE_FROM_LANE_SIZE_V(V) \
  hwy::EnableIf<!hwy::IsSame<V, V>()>* = nullptr

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_b_h(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_bu_h(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_b_h(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_bu_h(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_h_w(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_hu_w(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_h_w(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_hu_w(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_w_d(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{__lsx_vssrani_wu_d(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_w_d(v.raw, v.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{__lsx_vssrlni_wu_d(v.raw, v.raw, 0)};
}

// UI->UI DemoteTo for the case where
// sizeof(TFromD<D>) <= sizeof(TFromV<V>) / 4 is generic for all vector lengths
template <class DN, class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(DN),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_LE_D(DN, sizeof(TFromV<V>) / 4)>
HWY_API VFromD<DN> DemoteTo(DN dn, V v) {
  using T = TFromV<V>;
  using TN = TFromD<DN>;

  using TDemoteTo =
      MakeNarrow<If<IsSigned<T>() && IsSigned<TN>(), T, MakeUnsigned<T>>>;
  return DemoteTo(dn, DemoteTo(Rebind<TDemoteTo, DN>(), v));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_F16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{__lsx_vfcvt_h_s(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<double, D>> v) {
  return VFromD<D>{__lsx_vfcvt_s_d(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<double, D>> v) {
  return VFromD<D>{__lsx_vftintrz_w_d(
      reinterpret_cast<__m128d>(__lsx_vreplgr2vr_w(0)), v.raw)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D du32, VFromD<Rebind<double, D>> v) {
  const Rebind<uint64_t, decltype(du32)> du64;
  return DemoteTo(du32, ConvertTo(du64, v));
}

template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D df32, VFromD<Rebind<int64_t, D>> v) {
  const Rebind<double, decltype(df32)> df64;
  const RebindToUnsigned<decltype(df64)> du64;
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;

  const auto k2p64_63 = Set(df64, 27670116110564327424.0);
  const auto f64_hi52 =
      Xor(BitCast(df64, ShiftRight<12>(BitCast(du64, v))), k2p64_63) - k2p64_63;
  const auto f64_lo12 =
      PromoteTo(df64, BitCast(di32, And(TruncateTo(du32, BitCast(du64, v)),
                                        Set(du32, uint32_t{0x00000FFF}))));

  const auto f64_sum = f64_hi52 + f64_lo12;
  const auto f64_carry = (f64_hi52 - f64_sum) + f64_lo12;

  const auto f64_sum_is_inexact =
      ShiftRight<63>(BitCast(du64, VecFromMask(df64, f64_carry != Zero(df64))));
  const auto f64_bits_decrement =
      And(ShiftRight<63>(BitCast(du64, Xor(f64_sum, f64_carry))),
          f64_sum_is_inexact);

  const auto adj_f64_val = BitCast(
      df64,
      Or(BitCast(du64, f64_sum) - f64_bits_decrement, f64_sum_is_inexact));

  return DemoteTo(df32, adj_f64_val);
}

template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D df32, VFromD<Rebind<uint64_t, D>> v) {
  const Rebind<double, decltype(df32)> df64;
  const RebindToUnsigned<decltype(df64)> du64;
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;

  const auto k2p64 = Set(df64, 18446744073709551616.0);
  const auto f64_hi52 = Or(BitCast(df64, ShiftRight<12>(v)), k2p64) - k2p64;
  const auto f64_lo12 =
      PromoteTo(df64, BitCast(di32, And(TruncateTo(du32, BitCast(du64, v)),
                                        Set(du32, uint32_t{0x00000FFF}))));

  const auto f64_sum = f64_hi52 + f64_lo12;
  const auto f64_carry = (f64_hi52 - f64_sum) + f64_lo12;
  const auto f64_sum_is_inexact =
      ShiftRight<63>(BitCast(du64, VecFromMask(df64, f64_carry != Zero(df64))));

  const auto adj_f64_val = BitCast(
      df64,
      Or(BitCast(du64, f64_sum) - ShiftRight<63>(BitCast(du64, f64_carry)),
         f64_sum_is_inexact));

  return DemoteTo(df32, adj_f64_val);
}

// ------------------------------ ReorderDemote2To

// ReorderDemote2To for 8-byte UI64->UI32, <= 4-byte UI32->UI16,
// and <= 4-byte UI16->UI8
template <class DN, class V,
          HWY_IF_V_SIZE_LE_D(DN, ((sizeof(TFromD<DN>) <= 2 ? 4 : 8))),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(DN), HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int16_t> a,
                                   Vec128<int16_t> b) {
  return VFromD<D>{__lsx_vssrani_b_h(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int16_t> a,
                                   Vec128<int16_t> b) {
  return VFromD<D>{__lsx_vssrani_bu_h(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint16_t> a,
                                   Vec128<uint16_t> b) {
  return VFromD<D>{__lsx_vssrlni_b_h(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint16_t> a,
                                   Vec128<uint16_t> b) {
  return VFromD<D>{__lsx_vssrlni_bu_h(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int32_t> a,
                                   Vec128<int32_t> b) {
  return VFromD<D>{__lsx_vssrani_h_w(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int32_t> a,
                                   Vec128<int32_t> b) {
  return VFromD<D>{__lsx_vssrani_hu_w(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint32_t> a,
                                   Vec128<uint32_t> b) {
  return VFromD<D>{__lsx_vssrlni_h_w(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint32_t> a,
                                   Vec128<uint32_t> b) {
  return VFromD<D>{__lsx_vssrlni_hu_w(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int64_t> a,
                                   Vec128<int64_t> b) {
  return VFromD<D>{__lsx_vssrani_w_d(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<int64_t> a,
                                   Vec128<int64_t> b) {
  return VFromD<D>{__lsx_vssrani_wu_d(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint64_t> a,
                                   Vec128<uint64_t> b) {
  return VFromD<D>{__lsx_vssrlni_w_d(b.raw, a.raw, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /* tag */, Vec128<uint64_t> a,
                                   Vec128<uint64_t> b) {
  return VFromD<D>{__lsx_vssrlni_wu_d(b.raw, a.raw, 0)};
}

// 8-byte UI32->UI16 and UI16->UI8 ReorderDemote2To
template <class DN, class V, HWY_IF_V_SIZE_D(DN, 8),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(DN), HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_LE_D(DN, 2), HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Twice<DFromV<V>> dt;
  const Twice<decltype(dn)> dt_n;

  const auto demote2_result =
      ReorderDemote2To(dt_n, ResizeBitCast(dt, a), ResizeBitCast(dt, b));
  return VFromD<DN>{__lsx_vshuf4i_w(demote2_result.raw, 0x88)};
}

template <class D, class V, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  return ReorderDemote2To(d, a, b);
}

template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const DFromV<decltype(v)> du32;
  const Rebind<uint8_t, decltype(du32)> du8;
  return DemoteTo(du8, BitCast(du32, v));
}

// ------------------------------ F32->UI64 PromoteTo

// f32 ->i64
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /*di64*/, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{__lsx_vftintrzl_l_s(v.raw)};
}

// F32->U64 PromoteTo generic for all vector lengths
template <class D, HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D du64, VFromD<Rebind<float, D>> v) {
  const RebindToFloat<decltype(du64)> df64;
  return ConvertTo(du64, PromoteTo(df64, v));
}

// ------------------------------ MulFixedPoint15

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(const Vec128<int16_t, N> a,
                                           const Vec128<int16_t, N> b) {
  __m128i temp_ev = __lsx_vmulwev_w_h(a.raw, b.raw);
  __m128i temp_od = __lsx_vmulwod_w_h(a.raw, b.raw);
  __m128i temp1 = __lsx_vilvl_w(temp_od, temp_ev);
  __m128i temp2 = __lsx_vilvh_w(temp_od, temp_ev);
  return Vec128<int16_t, N>{__lsx_vssrarni_h_w(temp2, temp1, 15)};
}

// ------------------------------ Truncations

template <typename From, class DTo, HWY_IF_LANES_D(DTo, 1)>
HWY_API VFromD<DTo> TruncateTo(DTo /* tag */, Vec128<From, 1> v) {
  const Repartition<TFromD<DTo>, DFromV<decltype(v)>> dto;
  return VFromD<DTo>{BitCast(dto, v).raw};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec16<uint8_t> TruncateTo(D /* tag */, Vec128<uint64_t> v) {
  return Vec16<uint8_t>{__lsx_vextrins_b(v.raw, v.raw, 0x18)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec32<uint16_t> TruncateTo(D /* tag */, Vec128<uint64_t> v) {
  return Vec32<uint16_t>{__lsx_vextrins_h(v.raw, v.raw, 0x14)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec64<uint32_t> TruncateTo(D /* tag */, Vec128<uint64_t> v) {
  return Vec64<uint32_t>{__lsx_vpickev_w(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  __m128i v_ev = __lsx_vpickev_b(v.raw, v.raw);
  return VFromD<D>{__lsx_vpickev_b(v_ev, v_ev)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{__lsx_vpickev_h(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
  return VFromD<D>{__lsx_vpickev_b(v.raw, v.raw)};
}

// ------------------------------ int -> float ConvertTo

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{__lsx_vffint_s_w(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{__lsx_vffint_s_wu(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{__lsx_vffint_d_l(v.raw)};
}

// ------------------------------ float -> int ConvertTo

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{__lsx_vffint_d_lu(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{__lsx_vftintrz_w_s(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{__lsx_vftintrz_wu_s(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<double, D>> v) {
  return VFromD<D>{__lsx_vftintrz_l_d(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<double, D>> v) {
  return VFromD<D>{__lsx_vftintrz_lu_d(v.raw)};
}

// ------------------------------ NearestInt (Round)

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  return Vec128<int32_t, N>{__lsx_vftintrne_w_s(v.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, N> NearestInt(const Vec128<double, N> v) {
  return Vec128<int64_t, N>{__lsx_vftintrne_l_d(v.raw)};
}

template <class DI32, HWY_IF_I32_D(DI32)>
HWY_API VFromD<DI32> DemoteToNearestInt(DI32 di32,
                                        VFromD<Rebind<double, DI32>> v) {
  return DemoteTo(di32, NearestInt(v));
}

// ------------------------------ Floating-point rounding

template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrintrne_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Round(const Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrintrne_d(v.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrintrz_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Trunc(const Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrintrz_d(v.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrintrp_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Ceil(const Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrintrp_d(v.raw)};
}
// Toward -infinity, aka floor
template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  return Vec128<float, N>{__lsx_vfrintrm_s(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Floor(const Vec128<double, N> v) {
  return Vec128<double, N>{__lsx_vfrintrm_d(v.raw)};
}

// ------------------------------ Floating-point classification

// FIXME: disable gcc-14 tree-based loop optimizations to prevent
// 'HighwayTestGroup/HighwayTest.TestAllIsNaN/LSX' failures
#if HWY_COMPILER_GCC && !HWY_COMPILER_CLANG
#pragma GCC push_options
#pragma GCC optimize("-fno-tree-loop-optimize")
#endif

template <size_t N>
HWY_API Mask128<float, N> IsNaN(const Vec128<float, N> v) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_cune_s(v.raw, v.raw))};
}

template <size_t N>
HWY_API Mask128<double, N> IsNaN(const Vec128<double, N> v) {
  return Mask128<double, N>{
      reinterpret_cast<__m128d>(__lsx_vfcmp_cune_d(v.raw, v.raw))};
}

#if HWY_COMPILER_GCC && !HWY_COMPILER_CLANG
#pragma GCC pop_options
#endif

#ifdef HWY_NATIVE_IS_EITHER_NAN
#undef HWY_NATIVE_IS_EITHER_NAN
#else
#define HWY_NATIVE_IS_EITHER_NAN
#endif

template <size_t N>
HWY_API Mask128<float, N> IsEitherNaN(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{
      reinterpret_cast<__m128>(__lsx_vfcmp_cun_s(a.raw, b.raw))};
}

template <size_t N>
HWY_API Mask128<double, N> IsEitherNaN(Vec128<double, N> a,
                                       Vec128<double, N> b) {
  __m128i _tmp = __lsx_vor_v(__lsx_vfcmp_cune_d(a.raw, a.raw),
                             __lsx_vfcmp_cune_d(b.raw, b.raw));
  return Mask128<double, N>{reinterpret_cast<__m128d>(_tmp)};
}

#ifdef HWY_NATIVE_ISINF
#undef HWY_NATIVE_ISINF
#else
#define HWY_NATIVE_ISINF
#endif

template <class V>
HWY_API MFromD<DFromV<V>> IsInf(V v) {
  using T = TFromV<V>;

  static_assert(IsFloat<T>(), "Only for float");
  using TU = MakeUnsigned<T>;
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and
  // mantissa=0.
  return RebindMask(
      d,
      Eq(Add(vu, vu), Set(du, static_cast<TU>(hwy::MaxExponentTimes2<T>()))));
}

// Returns whether normal/subnormal/zero.
template <class V>
HWY_API MFromD<DFromV<V>> IsFinite(V v) {
  using T = TFromV<V>;

  static_assert(IsFloat<T>(), "Only for float");
  using TU = MakeUnsigned<T>;
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent<max.
  return RebindMask(
      d,
      Lt(Add(vu, vu), Set(du, static_cast<TU>(hwy::MaxExponentTimes2<T>()))));
}

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE MFromD<D> LoadMaskBits(D d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const VFromD<D> vbits{__lsx_vreplgr2vr_w(static_cast<int32_t>(bits))};

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) static constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                    1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vbits, Load(du, kRep8));

  alignas(16) static constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                                   1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE MFromD<D> LoadMaskBits(D d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(
      d, TestBit(Set(du, static_cast<uint16_t>(bits)), Load(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE MFromD<D> LoadMaskBits(D d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(
      d, TestBit(Set(du, static_cast<uint32_t>(bits)), Load(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE MFromD<D> LoadMaskBits(D d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, bits), Load(du, kBit)));
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  CopyBytes<(d.MaxLanes() + 7) / 8>(bits, &mask_bits);
  return detail::LoadMaskBits(d, mask_bits);
}

// ------------------------------ Dup128MaskFromMaskBits

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API MFromD<D> Dup128MaskFromMaskBits(D d, unsigned mask_bits) {
  constexpr size_t kN = MaxLanes(d);
  if (kN < 8) mask_bits &= (1u << kN) - 1;
  return detail::LoadMaskBits(d, mask_bits);
}

template <typename T>
struct CompressIsPartition {
  enum { value = (sizeof(T) != 1) };
};

// ------------------------------ BitsFromMask

namespace detail {

template <class D>
constexpr uint64_t OnlyActive(D d, uint64_t mask_bits) {
  return (d.MaxBytes() >= 16) ? mask_bits
                              : mask_bits & ((1ull << d.MaxLanes()) - 1);
}

constexpr HWY_INLINE uint64_t U64FromInt(int mask_bits) {
  return static_cast<uint64_t>(static_cast<unsigned>(mask_bits));
}

}  // namespace detail

template <class D, HWY_IF_T_SIZE_D(D, 1), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  return detail::OnlyActive(
      d, detail::U64FromInt(__lsx_vpickve2gr_w(__lsx_vmskltz_b(mask.raw), 0)));
}

template <class D, HWY_IF_T_SIZE_D(D, 2), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  return detail::OnlyActive(
      d, detail::U64FromInt(__lsx_vpickve2gr_w(__lsx_vmskltz_h(mask.raw), 0)));
}

template <class D, HWY_IF_T_SIZE_D(D, 4), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  return detail::OnlyActive(
      d, detail::U64FromInt(__lsx_vpickve2gr_w(
             __lsx_vmskltz_w(reinterpret_cast<__m128i>(mask.raw)), 0)));
}

template <class D, HWY_IF_T_SIZE_D(D, 8), HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  return detail::OnlyActive(
      d, detail::U64FromInt(__lsx_vpickve2gr_w(
             __lsx_vmskltz_d(reinterpret_cast<__m128i>(mask.raw)), 0)));
}

// ------------------------------ StoreMaskBits
// `p` points to at least 8 writable bytes.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t StoreMaskBits(D d, MFromD<D> mask, uint8_t* bits) {
  constexpr size_t kNumBytes = (MaxLanes(d) + 7) / 8;
  const uint64_t mask_bits = BitsFromMask(d, mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  return BitsFromMask(d, mask) == 0;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  constexpr uint64_t kAllBits = (1ull << kN) - 1;
  return BitsFromMask(d, mask) == kAllBits;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t CountTrue(D d, MFromD<D> mask) {
  return PopCount(BitsFromMask(d, mask));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownFirstTrue(D d, MFromD<D> mask) {
  return Num0BitsBelowLS1Bit_Nonzero64(BitsFromMask(d, mask));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindFirstTrue(D d, MFromD<D> mask) {
  const uint64_t mask_bits = BitsFromMask(d, mask);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask_bits)) : -1;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownLastTrue(D d, MFromD<D> mask) {
  return 31 - Num0BitsAboveMS1Bit_Nonzero32(
                  static_cast<uint32_t>(BitsFromMask(d, mask)));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindLastTrue(D d, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(BitsFromMask(d, mask));
  return mask_bits ? intptr_t(31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits))
                   : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

// Also works for N < 8 because the first 16 4-tuples only reference bytes 0-6.
template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Twice<decltype(d8)> d8t;
  const RebindToUnsigned<decltype(d)> du;

  alignas(16) static constexpr uint8_t table[2048] = {
      // PrintCompress16x8Tables
      0,  2,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      2,  0,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      4,  0,  2,  6,  8,  10, 12, 14, /**/ 0, 4,  2,  6,  8,  10, 12, 14,  //
      2,  4,  0,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      6,  0,  2,  4,  8,  10, 12, 14, /**/ 0, 6,  2,  4,  8,  10, 12, 14,  //
      2,  6,  0,  4,  8,  10, 12, 14, /**/ 0, 2,  6,  4,  8,  10, 12, 14,  //
      4,  6,  0,  2,  8,  10, 12, 14, /**/ 0, 4,  6,  2,  8,  10, 12, 14,  //
      2,  4,  6,  0,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      8,  0,  2,  4,  6,  10, 12, 14, /**/ 0, 8,  2,  4,  6,  10, 12, 14,  //
      2,  8,  0,  4,  6,  10, 12, 14, /**/ 0, 2,  8,  4,  6,  10, 12, 14,  //
      4,  8,  0,  2,  6,  10, 12, 14, /**/ 0, 4,  8,  2,  6,  10, 12, 14,  //
      2,  4,  8,  0,  6,  10, 12, 14, /**/ 0, 2,  4,  8,  6,  10, 12, 14,  //
      6,  8,  0,  2,  4,  10, 12, 14, /**/ 0, 6,  8,  2,  4,  10, 12, 14,  //
      2,  6,  8,  0,  4,  10, 12, 14, /**/ 0, 2,  6,  8,  4,  10, 12, 14,  //
      4,  6,  8,  0,  2,  10, 12, 14, /**/ 0, 4,  6,  8,  2,  10, 12, 14,  //
      2,  4,  6,  8,  0,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      10, 0,  2,  4,  6,  8,  12, 14, /**/ 0, 10, 2,  4,  6,  8,  12, 14,  //
      2,  10, 0,  4,  6,  8,  12, 14, /**/ 0, 2,  10, 4,  6,  8,  12, 14,  //
      4,  10, 0,  2,  6,  8,  12, 14, /**/ 0, 4,  10, 2,  6,  8,  12, 14,  //
      2,  4,  10, 0,  6,  8,  12, 14, /**/ 0, 2,  4,  10, 6,  8,  12, 14,  //
      6,  10, 0,  2,  4,  8,  12, 14, /**/ 0, 6,  10, 2,  4,  8,  12, 14,  //
      2,  6,  10, 0,  4,  8,  12, 14, /**/ 0, 2,  6,  10, 4,  8,  12, 14,  //
      4,  6,  10, 0,  2,  8,  12, 14, /**/ 0, 4,  6,  10, 2,  8,  12, 14,  //
      2,  4,  6,  10, 0,  8,  12, 14, /**/ 0, 2,  4,  6,  10, 8,  12, 14,  //
      8,  10, 0,  2,  4,  6,  12, 14, /**/ 0, 8,  10, 2,  4,  6,  12, 14,  //
      2,  8,  10, 0,  4,  6,  12, 14, /**/ 0, 2,  8,  10, 4,  6,  12, 14,  //
      4,  8,  10, 0,  2,  6,  12, 14, /**/ 0, 4,  8,  10, 2,  6,  12, 14,  //
      2,  4,  8,  10, 0,  6,  12, 14, /**/ 0, 2,  4,  8,  10, 6,  12, 14,  //
      6,  8,  10, 0,  2,  4,  12, 14, /**/ 0, 6,  8,  10, 2,  4,  12, 14,  //
      2,  6,  8,  10, 0,  4,  12, 14, /**/ 0, 2,  6,  8,  10, 4,  12, 14,  //
      4,  6,  8,  10, 0,  2,  12, 14, /**/ 0, 4,  6,  8,  10, 2,  12, 14,  //
      2,  4,  6,  8,  10, 0,  12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      12, 0,  2,  4,  6,  8,  10, 14, /**/ 0, 12, 2,  4,  6,  8,  10, 14,  //
      2,  12, 0,  4,  6,  8,  10, 14, /**/ 0, 2,  12, 4,  6,  8,  10, 14,  //
      4,  12, 0,  2,  6,  8,  10, 14, /**/ 0, 4,  12, 2,  6,  8,  10, 14,  //
      2,  4,  12, 0,  6,  8,  10, 14, /**/ 0, 2,  4,  12, 6,  8,  10, 14,  //
      6,  12, 0,  2,  4,  8,  10, 14, /**/ 0, 6,  12, 2,  4,  8,  10, 14,  //
      2,  6,  12, 0,  4,  8,  10, 14, /**/ 0, 2,  6,  12, 4,  8,  10, 14,  //
      4,  6,  12, 0,  2,  8,  10, 14, /**/ 0, 4,  6,  12, 2,  8,  10, 14,  //
      2,  4,  6,  12, 0,  8,  10, 14, /**/ 0, 2,  4,  6,  12, 8,  10, 14,  //
      8,  12, 0,  2,  4,  6,  10, 14, /**/ 0, 8,  12, 2,  4,  6,  10, 14,  //
      2,  8,  12, 0,  4,  6,  10, 14, /**/ 0, 2,  8,  12, 4,  6,  10, 14,  //
      4,  8,  12, 0,  2,  6,  10, 14, /**/ 0, 4,  8,  12, 2,  6,  10, 14,  //
      2,  4,  8,  12, 0,  6,  10, 14, /**/ 0, 2,  4,  8,  12, 6,  10, 14,  //
      6,  8,  12, 0,  2,  4,  10, 14, /**/ 0, 6,  8,  12, 2,  4,  10, 14,  //
      2,  6,  8,  12, 0,  4,  10, 14, /**/ 0, 2,  6,  8,  12, 4,  10, 14,  //
      4,  6,  8,  12, 0,  2,  10, 14, /**/ 0, 4,  6,  8,  12, 2,  10, 14,  //
      2,  4,  6,  8,  12, 0,  10, 14, /**/ 0, 2,  4,  6,  8,  12, 10, 14,  //
      10, 12, 0,  2,  4,  6,  8,  14, /**/ 0, 10, 12, 2,  4,  6,  8,  14,  //
      2,  10, 12, 0,  4,  6,  8,  14, /**/ 0, 2,  10, 12, 4,  6,  8,  14,  //
      4,  10, 12, 0,  2,  6,  8,  14, /**/ 0, 4,  10, 12, 2,  6,  8,  14,  //
      2,  4,  10, 12, 0,  6,  8,  14, /**/ 0, 2,  4,  10, 12, 6,  8,  14,  //
      6,  10, 12, 0,  2,  4,  8,  14, /**/ 0, 6,  10, 12, 2,  4,  8,  14,  //
      2,  6,  10, 12, 0,  4,  8,  14, /**/ 0, 2,  6,  10, 12, 4,  8,  14,  //
      4,  6,  10, 12, 0,  2,  8,  14, /**/ 0, 4,  6,  10, 12, 2,  8,  14,  //
      2,  4,  6,  10, 12, 0,  8,  14, /**/ 0, 2,  4,  6,  10, 12, 8,  14,  //
      8,  10, 12, 0,  2,  4,  6,  14, /**/ 0, 8,  10, 12, 2,  4,  6,  14,  //
      2,  8,  10, 12, 0,  4,  6,  14, /**/ 0, 2,  8,  10, 12, 4,  6,  14,  //
      4,  8,  10, 12, 0,  2,  6,  14, /**/ 0, 4,  8,  10, 12, 2,  6,  14,  //
      2,  4,  8,  10, 12, 0,  6,  14, /**/ 0, 2,  4,  8,  10, 12, 6,  14,  //
      6,  8,  10, 12, 0,  2,  4,  14, /**/ 0, 6,  8,  10, 12, 2,  4,  14,  //
      2,  6,  8,  10, 12, 0,  4,  14, /**/ 0, 2,  6,  8,  10, 12, 4,  14,  //
      4,  6,  8,  10, 12, 0,  2,  14, /**/ 0, 4,  6,  8,  10, 12, 2,  14,  //
      2,  4,  6,  8,  10, 12, 0,  14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      14, 0,  2,  4,  6,  8,  10, 12, /**/ 0, 14, 2,  4,  6,  8,  10, 12,  //
      2,  14, 0,  4,  6,  8,  10, 12, /**/ 0, 2,  14, 4,  6,  8,  10, 12,  //
      4,  14, 0,  2,  6,  8,  10, 12, /**/ 0, 4,  14, 2,  6,  8,  10, 12,  //
      2,  4,  14, 0,  6,  8,  10, 12, /**/ 0, 2,  4,  14, 6,  8,  10, 12,  //
      6,  14, 0,  2,  4,  8,  10, 12, /**/ 0, 6,  14, 2,  4,  8,  10, 12,  //
      2,  6,  14, 0,  4,  8,  10, 12, /**/ 0, 2,  6,  14, 4,  8,  10, 12,  //
      4,  6,  14, 0,  2,  8,  10, 12, /**/ 0, 4,  6,  14, 2,  8,  10, 12,  //
      2,  4,  6,  14, 0,  8,  10, 12, /**/ 0, 2,  4,  6,  14, 8,  10, 12,  //
      8,  14, 0,  2,  4,  6,  10, 12, /**/ 0, 8,  14, 2,  4,  6,  10, 12,  //
      2,  8,  14, 0,  4,  6,  10, 12, /**/ 0, 2,  8,  14, 4,  6,  10, 12,  //
      4,  8,  14, 0,  2,  6,  10, 12, /**/ 0, 4,  8,  14, 2,  6,  10, 12,  //
      2,  4,  8,  14, 0,  6,  10, 12, /**/ 0, 2,  4,  8,  14, 6,  10, 12,  //
      6,  8,  14, 0,  2,  4,  10, 12, /**/ 0, 6,  8,  14, 2,  4,  10, 12,  //
      2,  6,  8,  14, 0,  4,  10, 12, /**/ 0, 2,  6,  8,  14, 4,  10, 12,  //
      4,  6,  8,  14, 0,  2,  10, 12, /**/ 0, 4,  6,  8,  14, 2,  10, 12,  //
      2,  4,  6,  8,  14, 0,  10, 12, /**/ 0, 2,  4,  6,  8,  14, 10, 12,  //
      10, 14, 0,  2,  4,  6,  8,  12, /**/ 0, 10, 14, 2,  4,  6,  8,  12,  //
      2,  10, 14, 0,  4,  6,  8,  12, /**/ 0, 2,  10, 14, 4,  6,  8,  12,  //
      4,  10, 14, 0,  2,  6,  8,  12, /**/ 0, 4,  10, 14, 2,  6,  8,  12,  //
      2,  4,  10, 14, 0,  6,  8,  12, /**/ 0, 2,  4,  10, 14, 6,  8,  12,  //
      6,  10, 14, 0,  2,  4,  8,  12, /**/ 0, 6,  10, 14, 2,  4,  8,  12,  //
      2,  6,  10, 14, 0,  4,  8,  12, /**/ 0, 2,  6,  10, 14, 4,  8,  12,  //
      4,  6,  10, 14, 0,  2,  8,  12, /**/ 0, 4,  6,  10, 14, 2,  8,  12,  //
      2,  4,  6,  10, 14, 0,  8,  12, /**/ 0, 2,  4,  6,  10, 14, 8,  12,  //
      8,  10, 14, 0,  2,  4,  6,  12, /**/ 0, 8,  10, 14, 2,  4,  6,  12,  //
      2,  8,  10, 14, 0,  4,  6,  12, /**/ 0, 2,  8,  10, 14, 4,  6,  12,  //
      4,  8,  10, 14, 0,  2,  6,  12, /**/ 0, 4,  8,  10, 14, 2,  6,  12,  //
      2,  4,  8,  10, 14, 0,  6,  12, /**/ 0, 2,  4,  8,  10, 14, 6,  12,  //
      6,  8,  10, 14, 0,  2,  4,  12, /**/ 0, 6,  8,  10, 14, 2,  4,  12,  //
      2,  6,  8,  10, 14, 0,  4,  12, /**/ 0, 2,  6,  8,  10, 14, 4,  12,  //
      4,  6,  8,  10, 14, 0,  2,  12, /**/ 0, 4,  6,  8,  10, 14, 2,  12,  //
      2,  4,  6,  8,  10, 14, 0,  12, /**/ 0, 2,  4,  6,  8,  10, 14, 12,  //
      12, 14, 0,  2,  4,  6,  8,  10, /**/ 0, 12, 14, 2,  4,  6,  8,  10,  //
      2,  12, 14, 0,  4,  6,  8,  10, /**/ 0, 2,  12, 14, 4,  6,  8,  10,  //
      4,  12, 14, 0,  2,  6,  8,  10, /**/ 0, 4,  12, 14, 2,  6,  8,  10,  //
      2,  4,  12, 14, 0,  6,  8,  10, /**/ 0, 2,  4,  12, 14, 6,  8,  10,  //
      6,  12, 14, 0,  2,  4,  8,  10, /**/ 0, 6,  12, 14, 2,  4,  8,  10,  //
      2,  6,  12, 14, 0,  4,  8,  10, /**/ 0, 2,  6,  12, 14, 4,  8,  10,  //
      4,  6,  12, 14, 0,  2,  8,  10, /**/ 0, 4,  6,  12, 14, 2,  8,  10,  //
      2,  4,  6,  12, 14, 0,  8,  10, /**/ 0, 2,  4,  6,  12, 14, 8,  10,  //
      8,  12, 14, 0,  2,  4,  6,  10, /**/ 0, 8,  12, 14, 2,  4,  6,  10,  //
      2,  8,  12, 14, 0,  4,  6,  10, /**/ 0, 2,  8,  12, 14, 4,  6,  10,  //
      4,  8,  12, 14, 0,  2,  6,  10, /**/ 0, 4,  8,  12, 14, 2,  6,  10,  //
      2,  4,  8,  12, 14, 0,  6,  10, /**/ 0, 2,  4,  8,  12, 14, 6,  10,  //
      6,  8,  12, 14, 0,  2,  4,  10, /**/ 0, 6,  8,  12, 14, 2,  4,  10,  //
      2,  6,  8,  12, 14, 0,  4,  10, /**/ 0, 2,  6,  8,  12, 14, 4,  10,  //
      4,  6,  8,  12, 14, 0,  2,  10, /**/ 0, 4,  6,  8,  12, 14, 2,  10,  //
      2,  4,  6,  8,  12, 14, 0,  10, /**/ 0, 2,  4,  6,  8,  12, 14, 10,  //
      10, 12, 14, 0,  2,  4,  6,  8,  /**/ 0, 10, 12, 14, 2,  4,  6,  8,   //
      2,  10, 12, 14, 0,  4,  6,  8,  /**/ 0, 2,  10, 12, 14, 4,  6,  8,   //
      4,  10, 12, 14, 0,  2,  6,  8,  /**/ 0, 4,  10, 12, 14, 2,  6,  8,   //
      2,  4,  10, 12, 14, 0,  6,  8,  /**/ 0, 2,  4,  10, 12, 14, 6,  8,   //
      6,  10, 12, 14, 0,  2,  4,  8,  /**/ 0, 6,  10, 12, 14, 2,  4,  8,   //
      2,  6,  10, 12, 14, 0,  4,  8,  /**/ 0, 2,  6,  10, 12, 14, 4,  8,   //
      4,  6,  10, 12, 14, 0,  2,  8,  /**/ 0, 4,  6,  10, 12, 14, 2,  8,   //
      2,  4,  6,  10, 12, 14, 0,  8,  /**/ 0, 2,  4,  6,  10, 12, 14, 8,   //
      8,  10, 12, 14, 0,  2,  4,  6,  /**/ 0, 8,  10, 12, 14, 2,  4,  6,   //
      2,  8,  10, 12, 14, 0,  4,  6,  /**/ 0, 2,  8,  10, 12, 14, 4,  6,   //
      4,  8,  10, 12, 14, 0,  2,  6,  /**/ 0, 4,  8,  10, 12, 14, 2,  6,   //
      2,  4,  8,  10, 12, 14, 0,  6,  /**/ 0, 2,  4,  8,  10, 12, 14, 6,   //
      6,  8,  10, 12, 14, 0,  2,  4,  /**/ 0, 6,  8,  10, 12, 14, 2,  4,   //
      2,  6,  8,  10, 12, 14, 0,  4,  /**/ 0, 2,  6,  8,  10, 12, 14, 4,   //
      4,  6,  8,  10, 12, 14, 0,  2,  /**/ 0, 4,  6,  8,  10, 12, 14, 2,   //
      2,  4,  6,  8,  10, 12, 14, 0,  /**/ 0, 2,  4,  6,  8,  10, 12, 14};

  const VFromD<decltype(d8t)> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const VFromD<decltype(du)> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Twice<decltype(d8)> d8t;
  const RebindToUnsigned<decltype(d)> du;

  alignas(16) static constexpr uint8_t table[2048] = {
      // PrintCompressNot16x8Tables
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 14, 0,   //
      0, 4,  6,  8,  10, 12, 14, 2,  /**/ 4,  6,  8,  10, 12, 14, 0,  2,   //
      0, 2,  6,  8,  10, 12, 14, 4,  /**/ 2,  6,  8,  10, 12, 14, 0,  4,   //
      0, 6,  8,  10, 12, 14, 2,  4,  /**/ 6,  8,  10, 12, 14, 0,  2,  4,   //
      0, 2,  4,  8,  10, 12, 14, 6,  /**/ 2,  4,  8,  10, 12, 14, 0,  6,   //
      0, 4,  8,  10, 12, 14, 2,  6,  /**/ 4,  8,  10, 12, 14, 0,  2,  6,   //
      0, 2,  8,  10, 12, 14, 4,  6,  /**/ 2,  8,  10, 12, 14, 0,  4,  6,   //
      0, 8,  10, 12, 14, 2,  4,  6,  /**/ 8,  10, 12, 14, 0,  2,  4,  6,   //
      0, 2,  4,  6,  10, 12, 14, 8,  /**/ 2,  4,  6,  10, 12, 14, 0,  8,   //
      0, 4,  6,  10, 12, 14, 2,  8,  /**/ 4,  6,  10, 12, 14, 0,  2,  8,   //
      0, 2,  6,  10, 12, 14, 4,  8,  /**/ 2,  6,  10, 12, 14, 0,  4,  8,   //
      0, 6,  10, 12, 14, 2,  4,  8,  /**/ 6,  10, 12, 14, 0,  2,  4,  8,   //
      0, 2,  4,  10, 12, 14, 6,  8,  /**/ 2,  4,  10, 12, 14, 0,  6,  8,   //
      0, 4,  10, 12, 14, 2,  6,  8,  /**/ 4,  10, 12, 14, 0,  2,  6,  8,   //
      0, 2,  10, 12, 14, 4,  6,  8,  /**/ 2,  10, 12, 14, 0,  4,  6,  8,   //
      0, 10, 12, 14, 2,  4,  6,  8,  /**/ 10, 12, 14, 0,  2,  4,  6,  8,   //
      0, 2,  4,  6,  8,  12, 14, 10, /**/ 2,  4,  6,  8,  12, 14, 0,  10,  //
      0, 4,  6,  8,  12, 14, 2,  10, /**/ 4,  6,  8,  12, 14, 0,  2,  10,  //
      0, 2,  6,  8,  12, 14, 4,  10, /**/ 2,  6,  8,  12, 14, 0,  4,  10,  //
      0, 6,  8,  12, 14, 2,  4,  10, /**/ 6,  8,  12, 14, 0,  2,  4,  10,  //
      0, 2,  4,  8,  12, 14, 6,  10, /**/ 2,  4,  8,  12, 14, 0,  6,  10,  //
      0, 4,  8,  12, 14, 2,  6,  10, /**/ 4,  8,  12, 14, 0,  2,  6,  10,  //
      0, 2,  8,  12, 14, 4,  6,  10, /**/ 2,  8,  12, 14, 0,  4,  6,  10,  //
      0, 8,  12, 14, 2,  4,  6,  10, /**/ 8,  12, 14, 0,  2,  4,  6,  10,  //
      0, 2,  4,  6,  12, 14, 8,  10, /**/ 2,  4,  6,  12, 14, 0,  8,  10,  //
      0, 4,  6,  12, 14, 2,  8,  10, /**/ 4,  6,  12, 14, 0,  2,  8,  10,  //
      0, 2,  6,  12, 14, 4,  8,  10, /**/ 2,  6,  12, 14, 0,  4,  8,  10,  //
      0, 6,  12, 14, 2,  4,  8,  10, /**/ 6,  12, 14, 0,  2,  4,  8,  10,  //
      0, 2,  4,  12, 14, 6,  8,  10, /**/ 2,  4,  12, 14, 0,  6,  8,  10,  //
      0, 4,  12, 14, 2,  6,  8,  10, /**/ 4,  12, 14, 0,  2,  6,  8,  10,  //
      0, 2,  12, 14, 4,  6,  8,  10, /**/ 2,  12, 14, 0,  4,  6,  8,  10,  //
      0, 12, 14, 2,  4,  6,  8,  10, /**/ 12, 14, 0,  2,  4,  6,  8,  10,  //
      0, 2,  4,  6,  8,  10, 14, 12, /**/ 2,  4,  6,  8,  10, 14, 0,  12,  //
      0, 4,  6,  8,  10, 14, 2,  12, /**/ 4,  6,  8,  10, 14, 0,  2,  12,  //
      0, 2,  6,  8,  10, 14, 4,  12, /**/ 2,  6,  8,  10, 14, 0,  4,  12,  //
      0, 6,  8,  10, 14, 2,  4,  12, /**/ 6,  8,  10, 14, 0,  2,  4,  12,  //
      0, 2,  4,  8,  10, 14, 6,  12, /**/ 2,  4,  8,  10, 14, 0,  6,  12,  //
      0, 4,  8,  10, 14, 2,  6,  12, /**/ 4,  8,  10, 14, 0,  2,  6,  12,  //
      0, 2,  8,  10, 14, 4,  6,  12, /**/ 2,  8,  10, 14, 0,  4,  6,  12,  //
      0, 8,  10, 14, 2,  4,  6,  12, /**/ 8,  10, 14, 0,  2,  4,  6,  12,  //
      0, 2,  4,  6,  10, 14, 8,  12, /**/ 2,  4,  6,  10, 14, 0,  8,  12,  //
      0, 4,  6,  10, 14, 2,  8,  12, /**/ 4,  6,  10, 14, 0,  2,  8,  12,  //
      0, 2,  6,  10, 14, 4,  8,  12, /**/ 2,  6,  10, 14, 0,  4,  8,  12,  //
      0, 6,  10, 14, 2,  4,  8,  12, /**/ 6,  10, 14, 0,  2,  4,  8,  12,  //
      0, 2,  4,  10, 14, 6,  8,  12, /**/ 2,  4,  10, 14, 0,  6,  8,  12,  //
      0, 4,  10, 14, 2,  6,  8,  12, /**/ 4,  10, 14, 0,  2,  6,  8,  12,  //
      0, 2,  10, 14, 4,  6,  8,  12, /**/ 2,  10, 14, 0,  4,  6,  8,  12,  //
      0, 10, 14, 2,  4,  6,  8,  12, /**/ 10, 14, 0,  2,  4,  6,  8,  12,  //
      0, 2,  4,  6,  8,  14, 10, 12, /**/ 2,  4,  6,  8,  14, 0,  10, 12,  //
      0, 4,  6,  8,  14, 2,  10, 12, /**/ 4,  6,  8,  14, 0,  2,  10, 12,  //
      0, 2,  6,  8,  14, 4,  10, 12, /**/ 2,  6,  8,  14, 0,  4,  10, 12,  //
      0, 6,  8,  14, 2,  4,  10, 12, /**/ 6,  8,  14, 0,  2,  4,  10, 12,  //
      0, 2,  4,  8,  14, 6,  10, 12, /**/ 2,  4,  8,  14, 0,  6,  10, 12,  //
      0, 4,  8,  14, 2,  6,  10, 12, /**/ 4,  8,  14, 0,  2,  6,  10, 12,  //
      0, 2,  8,  14, 4,  6,  10, 12, /**/ 2,  8,  14, 0,  4,  6,  10, 12,  //
      0, 8,  14, 2,  4,  6,  10, 12, /**/ 8,  14, 0,  2,  4,  6,  10, 12,  //
      0, 2,  4,  6,  14, 8,  10, 12, /**/ 2,  4,  6,  14, 0,  8,  10, 12,  //
      0, 4,  6,  14, 2,  8,  10, 12, /**/ 4,  6,  14, 0,  2,  8,  10, 12,  //
      0, 2,  6,  14, 4,  8,  10, 12, /**/ 2,  6,  14, 0,  4,  8,  10, 12,  //
      0, 6,  14, 2,  4,  8,  10, 12, /**/ 6,  14, 0,  2,  4,  8,  10, 12,  //
      0, 2,  4,  14, 6,  8,  10, 12, /**/ 2,  4,  14, 0,  6,  8,  10, 12,  //
      0, 4,  14, 2,  6,  8,  10, 12, /**/ 4,  14, 0,  2,  6,  8,  10, 12,  //
      0, 2,  14, 4,  6,  8,  10, 12, /**/ 2,  14, 0,  4,  6,  8,  10, 12,  //
      0, 14, 2,  4,  6,  8,  10, 12, /**/ 14, 0,  2,  4,  6,  8,  10, 12,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 0,  14,  //
      0, 4,  6,  8,  10, 12, 2,  14, /**/ 4,  6,  8,  10, 12, 0,  2,  14,  //
      0, 2,  6,  8,  10, 12, 4,  14, /**/ 2,  6,  8,  10, 12, 0,  4,  14,  //
      0, 6,  8,  10, 12, 2,  4,  14, /**/ 6,  8,  10, 12, 0,  2,  4,  14,  //
      0, 2,  4,  8,  10, 12, 6,  14, /**/ 2,  4,  8,  10, 12, 0,  6,  14,  //
      0, 4,  8,  10, 12, 2,  6,  14, /**/ 4,  8,  10, 12, 0,  2,  6,  14,  //
      0, 2,  8,  10, 12, 4,  6,  14, /**/ 2,  8,  10, 12, 0,  4,  6,  14,  //
      0, 8,  10, 12, 2,  4,  6,  14, /**/ 8,  10, 12, 0,  2,  4,  6,  14,  //
      0, 2,  4,  6,  10, 12, 8,  14, /**/ 2,  4,  6,  10, 12, 0,  8,  14,  //
      0, 4,  6,  10, 12, 2,  8,  14, /**/ 4,  6,  10, 12, 0,  2,  8,  14,  //
      0, 2,  6,  10, 12, 4,  8,  14, /**/ 2,  6,  10, 12, 0,  4,  8,  14,  //
      0, 6,  10, 12, 2,  4,  8,  14, /**/ 6,  10, 12, 0,  2,  4,  8,  14,  //
      0, 2,  4,  10, 12, 6,  8,  14, /**/ 2,  4,  10, 12, 0,  6,  8,  14,  //
      0, 4,  10, 12, 2,  6,  8,  14, /**/ 4,  10, 12, 0,  2,  6,  8,  14,  //
      0, 2,  10, 12, 4,  6,  8,  14, /**/ 2,  10, 12, 0,  4,  6,  8,  14,  //
      0, 10, 12, 2,  4,  6,  8,  14, /**/ 10, 12, 0,  2,  4,  6,  8,  14,  //
      0, 2,  4,  6,  8,  12, 10, 14, /**/ 2,  4,  6,  8,  12, 0,  10, 14,  //
      0, 4,  6,  8,  12, 2,  10, 14, /**/ 4,  6,  8,  12, 0,  2,  10, 14,  //
      0, 2,  6,  8,  12, 4,  10, 14, /**/ 2,  6,  8,  12, 0,  4,  10, 14,  //
      0, 6,  8,  12, 2,  4,  10, 14, /**/ 6,  8,  12, 0,  2,  4,  10, 14,  //
      0, 2,  4,  8,  12, 6,  10, 14, /**/ 2,  4,  8,  12, 0,  6,  10, 14,  //
      0, 4,  8,  12, 2,  6,  10, 14, /**/ 4,  8,  12, 0,  2,  6,  10, 14,  //
      0, 2,  8,  12, 4,  6,  10, 14, /**/ 2,  8,  12, 0,  4,  6,  10, 14,  //
      0, 8,  12, 2,  4,  6,  10, 14, /**/ 8,  12, 0,  2,  4,  6,  10, 14,  //
      0, 2,  4,  6,  12, 8,  10, 14, /**/ 2,  4,  6,  12, 0,  8,  10, 14,  //
      0, 4,  6,  12, 2,  8,  10, 14, /**/ 4,  6,  12, 0,  2,  8,  10, 14,  //
      0, 2,  6,  12, 4,  8,  10, 14, /**/ 2,  6,  12, 0,  4,  8,  10, 14,  //
      0, 6,  12, 2,  4,  8,  10, 14, /**/ 6,  12, 0,  2,  4,  8,  10, 14,  //
      0, 2,  4,  12, 6,  8,  10, 14, /**/ 2,  4,  12, 0,  6,  8,  10, 14,  //
      0, 4,  12, 2,  6,  8,  10, 14, /**/ 4,  12, 0,  2,  6,  8,  10, 14,  //
      0, 2,  12, 4,  6,  8,  10, 14, /**/ 2,  12, 0,  4,  6,  8,  10, 14,  //
      0, 12, 2,  4,  6,  8,  10, 14, /**/ 12, 0,  2,  4,  6,  8,  10, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 0,  12, 14,  //
      0, 4,  6,  8,  10, 2,  12, 14, /**/ 4,  6,  8,  10, 0,  2,  12, 14,  //
      0, 2,  6,  8,  10, 4,  12, 14, /**/ 2,  6,  8,  10, 0,  4,  12, 14,  //
      0, 6,  8,  10, 2,  4,  12, 14, /**/ 6,  8,  10, 0,  2,  4,  12, 14,  //
      0, 2,  4,  8,  10, 6,  12, 14, /**/ 2,  4,  8,  10, 0,  6,  12, 14,  //
      0, 4,  8,  10, 2,  6,  12, 14, /**/ 4,  8,  10, 0,  2,  6,  12, 14,  //
      0, 2,  8,  10, 4,  6,  12, 14, /**/ 2,  8,  10, 0,  4,  6,  12, 14,  //
      0, 8,  10, 2,  4,  6,  12, 14, /**/ 8,  10, 0,  2,  4,  6,  12, 14,  //
      0, 2,  4,  6,  10, 8,  12, 14, /**/ 2,  4,  6,  10, 0,  8,  12, 14,  //
      0, 4,  6,  10, 2,  8,  12, 14, /**/ 4,  6,  10, 0,  2,  8,  12, 14,  //
      0, 2,  6,  10, 4,  8,  12, 14, /**/ 2,  6,  10, 0,  4,  8,  12, 14,  //
      0, 6,  10, 2,  4,  8,  12, 14, /**/ 6,  10, 0,  2,  4,  8,  12, 14,  //
      0, 2,  4,  10, 6,  8,  12, 14, /**/ 2,  4,  10, 0,  6,  8,  12, 14,  //
      0, 4,  10, 2,  6,  8,  12, 14, /**/ 4,  10, 0,  2,  6,  8,  12, 14,  //
      0, 2,  10, 4,  6,  8,  12, 14, /**/ 2,  10, 0,  4,  6,  8,  12, 14,  //
      0, 10, 2,  4,  6,  8,  12, 14, /**/ 10, 0,  2,  4,  6,  8,  12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  0,  10, 12, 14,  //
      0, 4,  6,  8,  2,  10, 12, 14, /**/ 4,  6,  8,  0,  2,  10, 12, 14,  //
      0, 2,  6,  8,  4,  10, 12, 14, /**/ 2,  6,  8,  0,  4,  10, 12, 14,  //
      0, 6,  8,  2,  4,  10, 12, 14, /**/ 6,  8,  0,  2,  4,  10, 12, 14,  //
      0, 2,  4,  8,  6,  10, 12, 14, /**/ 2,  4,  8,  0,  6,  10, 12, 14,  //
      0, 4,  8,  2,  6,  10, 12, 14, /**/ 4,  8,  0,  2,  6,  10, 12, 14,  //
      0, 2,  8,  4,  6,  10, 12, 14, /**/ 2,  8,  0,  4,  6,  10, 12, 14,  //
      0, 8,  2,  4,  6,  10, 12, 14, /**/ 8,  0,  2,  4,  6,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  0,  8,  10, 12, 14,  //
      0, 4,  6,  2,  8,  10, 12, 14, /**/ 4,  6,  0,  2,  8,  10, 12, 14,  //
      0, 2,  6,  4,  8,  10, 12, 14, /**/ 2,  6,  0,  4,  8,  10, 12, 14,  //
      0, 6,  2,  4,  8,  10, 12, 14, /**/ 6,  0,  2,  4,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  0,  6,  8,  10, 12, 14,  //
      0, 4,  2,  6,  8,  10, 12, 14, /**/ 4,  0,  2,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  0,  4,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 0,  2,  4,  6,  8,  10, 12, 14};

  const VFromD<decltype(d8t)> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const VFromD<decltype(du)> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[256] = {
      // PrintCompress32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      8,  9,  10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15,  //
      0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  //
      0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  8,  9,  10, 11,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,   //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,   //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,   //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[256] = {
      // PrintCompressNot32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
      14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
      12, 13, 14, 15, 8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[64] = {
      // PrintCompress64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[64] = {
      // PrintCompressNot64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v, uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressNotBits(Vec128<T, N> v, uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromNotBits128(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

}  // namespace detail

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 1 and mask[0] = 0, then swap both halves, else keep.
  const DFromV<decltype(v)> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskL, maskH);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case, 2 or 4 bytes
template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  const DFromV<decltype(v)> d;
  return detail::CompressBits(v, BitsFromMask(d, mask));
}

// ------------------------------ CompressNot

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 0 and mask[0] = 1, then swap both halves, else keep.
  const DFromV<decltype(v)> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  const DFromV<decltype(v)> d;
  // For partial vectors, we cannot pull the Not() into the table because
  // BitsFromMask clears the upper bits.
  if (N < 16 / sizeof(T)) {
    return detail::CompressBits(v, BitsFromMask(d, Not(mask)));
  }
  return detail::CompressNotBits(v, BitsFromMask(d, mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::CompressBits(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> m, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = BitsFromMask(d, m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = BitsFromMask(d, m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  uint64_t mask_bits = 0;
  constexpr size_t kN = MaxLanes(d);
  constexpr size_t kNumBytes = (kN + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);

  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// ------------------------------ StoreInterleaved2/3/4

// HWY_NATIVE_LOAD_STORE_INTERLEAVED not set, hence defined in
// generic_ops-inl.h.

// ------------------------------ Additional mask logical operations

template <class T>
HWY_API Mask128<T, 1> SetAtOrAfterFirst(Mask128<T, 1> mask) {
  return mask;
}
template <class T>
HWY_API Mask128<T, 2> SetAtOrAfterFirst(Mask128<T, 2> mask) {
  const FixedTag<T, 2> d;
  const auto vmask = VecFromMask(d, mask);
  return MaskFromVec(Or(vmask, InterleaveLower(vmask, vmask)));
}
template <class T, size_t N, HWY_IF_LANES_GT(N, 2), HWY_IF_V_SIZE_LE(T, N, 8)>
HWY_API Mask128<T, N> SetAtOrAfterFirst(Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const auto vmask = VecFromMask(d, mask);
  const auto neg_vmask =
      ResizeBitCast(d, Neg(ResizeBitCast(Full64<int64_t>(), vmask)));
  return MaskFromVec(Or(vmask, neg_vmask));
}
template <class T, HWY_IF_NOT_T_SIZE(T, 8)>
HWY_API Mask128<T> SetAtOrAfterFirst(Mask128<T> mask) {
  const Full128<T> d;
  const Repartition<int64_t, decltype(d)> di64;

  auto vmask = BitCast(di64, VecFromMask(d, mask));
  VFromD<decltype(di64)> neg_vmask{__lsx_vsub_q(Zero(di64).raw, vmask.raw)};

  return MaskFromVec(BitCast(d, Or(vmask, neg_vmask)));
}

template <class T, size_t N>
HWY_API Mask128<T, N> SetBeforeFirst(Mask128<T, N> mask) {
  return Not(SetAtOrAfterFirst(mask));
}

template <class T>
HWY_API Mask128<T, 1> SetOnlyFirst(Mask128<T, 1> mask) {
  return mask;
}
template <class T>
HWY_API Mask128<T, 2> SetOnlyFirst(Mask128<T, 2> mask) {
  const FixedTag<T, 2> d;
  const RebindToSigned<decltype(d)> di;

  const auto vmask = BitCast(di, VecFromMask(d, mask));
  const auto zero = Zero(di);
  const auto vmask2 = VecFromMask(di, InterleaveLower(zero, vmask) == zero);
  return MaskFromVec(BitCast(d, And(vmask, vmask2)));
}
template <class T, size_t N, HWY_IF_LANES_GT(N, 2), HWY_IF_V_SIZE_LE(T, N, 8)>
HWY_API Mask128<T, N> SetOnlyFirst(Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;

  const auto vmask = ResizeBitCast(Full64<int64_t>(), VecFromMask(d, mask));
  const auto only_first_vmask =
      BitCast(d, Neg(ResizeBitCast(di, And(vmask, Neg(vmask)))));
  return MaskFromVec(only_first_vmask);
}
template <class T, HWY_IF_NOT_T_SIZE(T, 8)>
HWY_API Mask128<T> SetOnlyFirst(Mask128<T> mask) {
  const Full128<T> d;
  const RebindToSigned<decltype(d)> di;

  auto vmask = BitCast(di, VecFromMask(d, mask));
  VFromD<decltype(di)> neg_vmask{__lsx_vsub_q(Zero(di).raw, vmask.raw)};

  return MaskFromVec(BitCast(d, Neg(And(vmask, neg_vmask))));
}

template <class T>
HWY_API Mask128<T, 1> SetAtOrBeforeFirst(Mask128<T, 1> /*mask*/) {
  const FixedTag<T, 1> d;
  const RebindToSigned<decltype(d)> di;
  using TI = MakeSigned<T>;

  return RebindMask(d, MaskFromVec(Set(di, TI(-1))));
}
template <class T, size_t N, HWY_IF_LANES_GT(N, 1)>
HWY_API Mask128<T, N> SetAtOrBeforeFirst(Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  return SetBeforeFirst(MaskFromVec(ShiftLeftLanes<1>(VecFromMask(d, mask))));
}

// ------------------------------ Reductions
#undef HWY_IF_SUM_OF_LANES_D
#define HWY_IF_SUM_OF_LANES_D(D)                                        \
  HWY_IF_LANES_GT_D(D, 1),                                              \
      hwy::EnableIf<!hwy::IsSame<TFromD<D>, uint8_t>() ||               \
                    (HWY_V_SIZE_D(D) != 8 && HWY_V_SIZE_D(D) != 16)>* = \
          nullptr
// ------------------------------ SumOfLanes

template <class D, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 8)>
HWY_API VFromD<D> SumOfLanes(D d, VFromD<D> v) {
  return Set(d, static_cast<uint8_t>(GetLane(SumsOf8(v)) & 0xFF));
}
template <class D, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 16)>
HWY_API VFromD<D> SumOfLanes(D d, VFromD<D> v) {
  const Repartition<uint64_t, decltype(d)> d64;
  VFromD<decltype(d64)> sums = SumsOf8(v);
  sums = SumOfLanes(d64, sums);
  return Broadcast<0>(BitCast(d, sums));
}

// ------------------------------ Lt128

namespace detail {

// Returns vector-mask for Lt128. Generic for all vector lengths.
template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Lt128Vec(const D d, VFromD<D> a, VFromD<D> b) {
  // Truth table of Eq and Lt for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const auto eqHL = Eq(a, b);
  const VFromD<D> ltHL = VecFromMask(d, Lt(a, b));
  const VFromD<D> ltLX = ShiftLeftLanes<1>(ltHL);
  const VFromD<D> vecHx = IfThenElse(eqHL, ltLX, ltHL);
  return InterleaveUpper(d, vecHx, vecHx);
}

// Returns vector-mask for Eq128. Generic for all vector lengths.
template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Eq128Vec(D d, VFromD<D> a, VFromD<D> b) {
  const auto eqHL = VecFromMask(d, Eq(a, b));
  const auto eqLH = Reverse2(d, eqHL);
  return And(eqHL, eqLH);
}

template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Ne128Vec(D d, VFromD<D> a, VFromD<D> b) {
  const auto neHL = VecFromMask(d, Ne(a, b));
  const auto neLH = Reverse2(d, neHL);
  return Or(neHL, neLH);
}

template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Lt128UpperVec(D d, VFromD<D> a, VFromD<D> b) {
  const VFromD<D> ltHL = VecFromMask(d, Lt(a, b));
  return InterleaveUpper(d, ltHL, ltHL);
}

template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Eq128UpperVec(D d, VFromD<D> a, VFromD<D> b) {
  const VFromD<D> eqHL = VecFromMask(d, Eq(a, b));
  return InterleaveUpper(d, eqHL, eqHL);
}

template <class D, HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> Ne128UpperVec(D d, VFromD<D> a, VFromD<D> b) {
  const VFromD<D> neHL = VecFromMask(d, Ne(a, b));
  return InterleaveUpper(d, neHL, neHL);
}

}  // namespace detail

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Lt128(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Lt128Vec(d, a, b));
}

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Eq128(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Eq128Vec(d, a, b));
}

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Ne128(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Ne128Vec(d, a, b));
}

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Lt128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Lt128UpperVec(d, a, b));
}

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Eq128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Eq128UpperVec(d, a, b));
}

template <class D, HWY_IF_U64_D(D)>
HWY_API MFromD<D> Ne128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return MaskFromVec(detail::Ne128UpperVec(d, a, b));
}

// ------------------------------ Min128, Max128 (Lt128)

// Avoids the extra MaskFromVec in Lt128.
template <class D, HWY_IF_U64_D(D)>
HWY_API VFromD<D> Min128(D d, VFromD<D> a, VFromD<D> b) {
  return IfVecThenElse(detail::Lt128Vec(d, a, b), a, b);
}

template <class D, HWY_IF_U64_D(D)>
HWY_API VFromD<D> Max128(D d, VFromD<D> a, VFromD<D> b) {
  return IfVecThenElse(detail::Lt128Vec(d, b, a), a, b);
}

template <class D, HWY_IF_U64_D(D)>
HWY_API VFromD<D> Min128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, a, b), a, b);
}

template <class D, HWY_IF_U64_D(D)>
HWY_API VFromD<D> Max128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, b, a), a, b);
}

// -------------------- LeadingZeroCount, TrailingZeroCount,
//                      HighestSetBitIndex

#ifdef HWY_NATIVE_LEADING_ZERO_COUNT
#undef HWY_NATIVE_LEADING_ZERO_COUNT
#else
#define HWY_NATIVE_LEADING_ZERO_COUNT
#endif

template <class V, HWY_IF_UI8_D(DFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lsx_vclz_b(v.raw)};
}

template <class V, HWY_IF_UI16_D(DFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lsx_vclz_h(v.raw)};
}

template <class V, HWY_IF_UI32_D(DFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lsx_vclz_w(v.raw)};
}

template <class V, HWY_IF_UI64_D(DFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lsx_vclz_d(v.raw)};
}

template <class V, HWY_IF_V_SIZE_LE_V(V, 16), HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  using T = TFromD<decltype(d)>;
  return BitCast(d, Set(d, T{sizeof(T) * 8 - 1}) - LeadingZeroCount(v));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V TrailingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  using T = TFromD<decltype(d)>;

  const auto lsb = And(v, BitCast(d, Neg(BitCast(di, v))));
  return IfThenElse(Eq(v, Zero(d)), Set(d, T{sizeof(T) * 8}),
                    HighestSetBitIndex(lsb));
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy

HWY_AFTER_NAMESPACE();

#undef HWY_LSX_IF_EMULATED_D
