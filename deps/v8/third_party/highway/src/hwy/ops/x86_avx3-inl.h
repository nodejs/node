// Copyright 2019 Google LLC
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

// External include guard in highway.h - see comment there.

// For AVX3/AVX10 targets that support 512-byte vectors. Already includes base.h
// and shared-inl.h.
#include "hwy/ops/x86_512-inl.h"

// AVX3/AVX10 ops that have dependencies on ops defined in x86_512-inl.h if
// HWY_MAX_BYTES >= 64 is true are defined below

// Avoid uninitialized warnings in GCC's avx512fintrin.h - see
// https://github.com/google/highway/issues/710)
HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_GCC_ACTUAL
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
HWY_DIAGNOSTICS_OFF(disable : 4701 4703 6001 26494,
                    ignored "-Wmaybe-uninitialized")
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

#if HWY_TARGET <= HWY_AVX3_DL

// ------------------------------ ShiftLeft

// Generic for all vector lengths. Must be defined after all GaloisAffine.
template <int kBits, class V, HWY_IF_T_SIZE_V(V, 1)>
HWY_API V ShiftLeft(const V v) {
  const Repartition<uint64_t, DFromV<V>> du64;
  if (kBits == 0) return v;
  if (kBits == 1) return v + v;
  constexpr uint64_t kMatrix = (0x0102040810204080ULL >> kBits) &
                               (0x0101010101010101ULL * (0xFF >> kBits));
  return detail::GaloisAffine(v, Set(du64, kMatrix));
}

// ------------------------------ ShiftRight

// Generic for all vector lengths. Must be defined after all GaloisAffine.
template <int kBits, class V, HWY_IF_U8_D(DFromV<V>)>
HWY_API V ShiftRight(const V v) {
  const Repartition<uint64_t, DFromV<V>> du64;
  if (kBits == 0) return v;
  constexpr uint64_t kMatrix =
      (0x0102040810204080ULL << kBits) &
      (0x0101010101010101ULL * ((0xFF << kBits) & 0xFF));
  return detail::GaloisAffine(v, Set(du64, kMatrix));
}

// Generic for all vector lengths. Must be defined after all GaloisAffine.
template <int kBits, class V, HWY_IF_I8_D(DFromV<V>)>
HWY_API V ShiftRight(const V v) {
  const Repartition<uint64_t, DFromV<V>> du64;
  if (kBits == 0) return v;
  constexpr uint64_t kShift =
      (0x0102040810204080ULL << kBits) &
      (0x0101010101010101ULL * ((0xFF << kBits) & 0xFF));
  constexpr uint64_t kSign =
      kBits == 0 ? 0 : (0x8080808080808080ULL >> (64 - (8 * kBits)));
  return detail::GaloisAffine(v, Set(du64, kShift | kSign));
}

// ------------------------------ RotateRight

// U8 RotateRight is generic for all vector lengths on AVX3_DL
template <int kBits, class V, HWY_IF_U8(TFromV<V>)>
HWY_API V RotateRight(V v) {
  static_assert(0 <= kBits && kBits < 8, "Invalid shift count");

  const Repartition<uint64_t, DFromV<V>> du64;
  if (kBits == 0) return v;

  constexpr uint64_t kShrMatrix =
      (0x0102040810204080ULL << kBits) &
      (0x0101010101010101ULL * ((0xFF << kBits) & 0xFF));
  constexpr int kShlBits = (-kBits) & 7;
  constexpr uint64_t kShlMatrix = (0x0102040810204080ULL >> kShlBits) &
                                  (0x0101010101010101ULL * (0xFF >> kShlBits));
  constexpr uint64_t kMatrix = kShrMatrix | kShlMatrix;

  return detail::GaloisAffine(v, Set(du64, kMatrix));
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

// ------------------------------ Compress

#pragma push_macro("HWY_X86_SLOW_COMPRESS_STORE")

#ifndef HWY_X86_SLOW_COMPRESS_STORE  // allow override
// Slow on Zen4 and SPR, faster if we emulate via Compress().
#if HWY_TARGET == HWY_AVX3_ZEN4 || HWY_TARGET == HWY_AVX3_SPR
#define HWY_X86_SLOW_COMPRESS_STORE 1
#else
#define HWY_X86_SLOW_COMPRESS_STORE 0
#endif
#endif  // HWY_X86_SLOW_COMPRESS_STORE

// Always implement 8-bit here even if we lack VBMI2 because we can do better
// than generic_ops (8 at a time) via the native 32-bit compress (16 at a time).
#ifdef HWY_NATIVE_COMPRESS8
#undef HWY_NATIVE_COMPRESS8
#else
#define HWY_NATIVE_COMPRESS8
#endif

namespace detail {

#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
template <size_t N>
HWY_INLINE Vec128<uint8_t, N> NativeCompress(const Vec128<uint8_t, N> v,
                                             const Mask128<uint8_t, N> mask) {
  return Vec128<uint8_t, N>{_mm_maskz_compress_epi8(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint8_t> NativeCompress(const Vec256<uint8_t> v,
                                          const Mask256<uint8_t> mask) {
  return Vec256<uint8_t>{_mm256_maskz_compress_epi8(mask.raw, v.raw)};
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE Vec512<uint8_t> NativeCompress(const Vec512<uint8_t> v,
                                          const Mask512<uint8_t> mask) {
  return Vec512<uint8_t>{_mm512_maskz_compress_epi8(mask.raw, v.raw)};
}
#endif

template <size_t N>
HWY_INLINE Vec128<uint16_t, N> NativeCompress(const Vec128<uint16_t, N> v,
                                              const Mask128<uint16_t, N> mask) {
  return Vec128<uint16_t, N>{_mm_maskz_compress_epi16(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint16_t> NativeCompress(const Vec256<uint16_t> v,
                                           const Mask256<uint16_t> mask) {
  return Vec256<uint16_t>{_mm256_maskz_compress_epi16(mask.raw, v.raw)};
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE Vec512<uint16_t> NativeCompress(const Vec512<uint16_t> v,
                                           const Mask512<uint16_t> mask) {
  return Vec512<uint16_t>{_mm512_maskz_compress_epi16(mask.raw, v.raw)};
}
#endif

// Do not even define these to prevent accidental usage.
#if !HWY_X86_SLOW_COMPRESS_STORE

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint8_t, N> v,
                                    Mask128<uint8_t, N> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint8_t> v, Mask256<uint8_t> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<uint8_t> v, Mask512<uint8_t> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}
#endif

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint16_t, N> v,
                                    Mask128<uint16_t, N> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint16_t> v, Mask256<uint16_t> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<uint16_t> v, Mask512<uint16_t> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}
#endif  // HWY_MAX_BYTES >= 64

#endif  // HWY_X86_SLOW_COMPRESS_STORE

#endif  // HWY_TARGET <= HWY_AVX3_DL

template <size_t N>
HWY_INLINE Vec128<uint32_t, N> NativeCompress(Vec128<uint32_t, N> v,
                                              Mask128<uint32_t, N> mask) {
  return Vec128<uint32_t, N>{_mm_maskz_compress_epi32(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint32_t> NativeCompress(Vec256<uint32_t> v,
                                           Mask256<uint32_t> mask) {
  return Vec256<uint32_t>{_mm256_maskz_compress_epi32(mask.raw, v.raw)};
}

#if HWY_MAX_BYTES >= 64
HWY_INLINE Vec512<uint32_t> NativeCompress(Vec512<uint32_t> v,
                                           Mask512<uint32_t> mask) {
  return Vec512<uint32_t>{_mm512_maskz_compress_epi32(mask.raw, v.raw)};
}
#endif
// We use table-based compress for 64-bit lanes, see CompressIsPartition.

// Do not even define these to prevent accidental usage.
#if !HWY_X86_SLOW_COMPRESS_STORE

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint32_t, N> v,
                                    Mask128<uint32_t, N> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint32_t> v, Mask256<uint32_t> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<uint32_t> v, Mask512<uint32_t> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}
#endif

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint64_t, N> v,
                                    Mask128<uint64_t, N> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint64_t> v, Mask256<uint64_t> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<uint64_t> v, Mask512<uint64_t> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}
#endif

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<float, N> v, Mask128<float, N> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<float> v, Mask256<float> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<float> v, Mask512<float> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}
#endif

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<double, N> v,
                                    Mask128<double, N> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<double> v, Mask256<double> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}
#if HWY_MAX_BYTES >= 64
HWY_INLINE void NativeCompressStore(Vec512<double> v, Mask512<double> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}
#endif

#endif  // HWY_X86_SLOW_COMPRESS_STORE

// For u8x16 and <= u16x16 we can avoid store+load for Compress because there is
// only a single compressed vector (u32x16). Other EmuCompress are implemented
// after the EmuCompressStore they build upon.
template <class V, HWY_IF_U8(TFromV<V>),
          HWY_IF_LANES_LE_D(DFromV<V>, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V EmuCompress(V v, MFromD<DFromV<V>> mask) {
  const DFromV<decltype(v)> d;
  const Rebind<uint32_t, decltype(d)> d32;
  const VFromD<decltype(d32)> v0 = PromoteTo(d32, v);

  using M32 = MFromD<decltype(d32)>;
  const M32 m0 = PromoteMaskTo(d32, d, mask);
  return TruncateTo(d, Compress(v0, m0));
}

template <class V, HWY_IF_U16(TFromV<V>),
          HWY_IF_LANES_LE_D(DFromV<V>, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V EmuCompress(V v, MFromD<DFromV<V>> mask) {
  const DFromV<decltype(v)> d;
  const Rebind<int32_t, decltype(d)> di32;
  const RebindToUnsigned<decltype(di32)> du32;

  const MFromD<decltype(du32)> mask32 = PromoteMaskTo(du32, d, mask);
  // DemoteTo is 2 ops, but likely lower latency than TruncateTo on SKX.
  // Only i32 -> u16 is supported, whereas NativeCompress expects u32.
  const VFromD<decltype(du32)> v32 = PromoteTo(du32, v);
  return DemoteTo(d, BitCast(di32, NativeCompress(v32, mask32)));
}

// See above - small-vector EmuCompressStore are implemented via EmuCompress.
template <class D, HWY_IF_UNSIGNED_D(D),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_LE_D(D, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED void EmuCompressStore(
    VFromD<D> v, MFromD<D> mask, D d, TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(EmuCompress(v, mask), d, unaligned);
}

// Main emulation logic for wider vector, starting with EmuCompressStore because
// it is most convenient to merge pieces using memory (concatenating vectors at
// byte offsets is difficult).
template <class D, HWY_IF_UNSIGNED_D(D),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_GT_D(D, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED void EmuCompressStore(
    VFromD<D> v, MFromD<D> mask, D d, TFromD<D>* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;

  const MFromD<decltype(dh)> m0 = LowerHalfOfMask(dh, mask);
  const MFromD<decltype(dh)> m1 = UpperHalfOfMask(dh, mask);

  const VFromD<decltype(dh)> v0 = LowerHalf(dh, v);
  const VFromD<decltype(dh)> v1 = UpperHalf(dh, v);

  EmuCompressStore(v0, m0, dh, unaligned);
  EmuCompressStore(v1, m1, dh, unaligned + CountTrue(dh, m0));
}

// Finally, the remaining EmuCompress for wide vectors, using EmuCompressStore.
template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_GT_D(DFromV<V>, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V EmuCompress(V v, MFromD<DFromV<V>> mask) {
  using D = DFromV<decltype(v)>;
  using T = TFromD<D>;
  const D d;

  alignas(HWY_MAX_LANES_D(D) * sizeof(T)) T buf[2 * HWY_MAX_LANES_D(D)];
  EmuCompressStore(v, mask, d, buf);
  return Load(d, buf);
}

}  // namespace detail

template <class V, class M, HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V Compress(V v, const M mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  return BitCast(d, detail::NativeCompress(BitCast(du, v), mu));
#else
  return BitCast(d, detail::EmuCompress(BitCast(du, v), mu));
#endif
}

template <class V, class M, HWY_IF_T_SIZE_V(V, 4)>
HWY_API V Compress(V v, const M mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeCompress(BitCast(du, v), mu));
}

// ------------------------------ CompressNot

template <class V, class M, HWY_IF_NOT_T_SIZE_V(V, 8)>
HWY_API V CompressNot(V v, const M mask) {
  return Compress(v, Not(mask));
}

// uint64_t lanes. Only implement for 256 and 512-bit vectors because this is a
// no-op for 128-bit.
template <class V, class M, HWY_IF_V_SIZE_GT_D(DFromV<V>, 16)>
HWY_API V CompressBlocksNot(V v, M mask) {
  return CompressNot(v, mask);
}

// ------------------------------ CompressBits
template <class V>
HWY_API V CompressBits(V v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(DFromV<V>(), bits));
}

// ------------------------------ CompressStore

// Generic for all vector lengths.

template <class D, HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_X86_SLOW_COMPRESS_STORE
  StoreU(Compress(v, mask), d, unaligned);
#else
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  auto pu = reinterpret_cast<TFromD<decltype(du)> * HWY_RESTRICT>(unaligned);

#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  detail::NativeCompressStore(BitCast(du, v), mu, pu);
#else
  detail::EmuCompressStore(BitCast(du, v), mu, du, pu);
#endif
#endif  // HWY_X86_SLOW_COMPRESS_STORE
  const size_t count = CountTrue(d, mask);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_NOT_FLOAT_D(D),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_X86_SLOW_COMPRESS_STORE
  StoreU(Compress(v, mask), d, unaligned);
#else
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  using TU = TFromD<decltype(du)>;
  TU* HWY_RESTRICT pu = reinterpret_cast<TU*>(unaligned);
  detail::NativeCompressStore(BitCast(du, v), mu, pu);
#endif  // HWY_X86_SLOW_COMPRESS_STORE
  const size_t count = CountTrue(d, mask);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// Additional overloads to avoid casting to uint32_t (delay?).
template <class D, HWY_IF_FLOAT3264_D(D)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_X86_SLOW_COMPRESS_STORE
  StoreU(Compress(v, mask), d, unaligned);
#else
  (void)d;
  detail::NativeCompressStore(v, mask, unaligned);
#endif  // HWY_X86_SLOW_COMPRESS_STORE
  const size_t count = PopCount(uint64_t{mask.raw});
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// ------------------------------ CompressBlendedStore
template <class D>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  // Native CompressStore already does the blending at no extra cost (latency
  // 11, rthroughput 2 - same as compress plus store).

  HWY_IF_CONSTEXPR(HWY_MAX_LANES_D(D) < (16 / sizeof(TFromD<D>))) {
    m = And(m, FirstN(d, HWY_MAX_LANES_D(D)));
  }

  HWY_IF_CONSTEXPR(!HWY_X86_SLOW_COMPRESS_STORE &&
                   (HWY_TARGET <= HWY_AVX3_DL || sizeof(TFromD<D>) > 2)) {
    return CompressStore(v, m, d, unaligned);
  }
  else {
    const size_t count = CountTrue(d, m);
    StoreN(Compress(v, m), d, unaligned, count);
    detail::MaybeUnpoison(unaligned, count);
    return count;
  }
}

// ------------------------------ CompressBitsStore
// Generic for all vector lengths.
template <class D>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

#pragma pop_macro("HWY_X86_SLOW_COMPRESS_STORE")

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
