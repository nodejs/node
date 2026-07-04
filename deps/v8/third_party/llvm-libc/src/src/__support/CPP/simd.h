//===-- Portable SIMD library similar to stdx::simd -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides a generic interface into fixed-size SIMD instructions
// using the clang vector type. The API shares some similarities with the
// stdx::simd proposal, but instead chooses to use vectors as primitive types
// with several extra helper functions.
//
//===----------------------------------------------------------------------===//

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/tuple.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/utility/integer_sequence.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_SIMD_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_SIMD_H

#if LIBC_HAS_VECTOR_TYPE

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

namespace internal {

#if defined(LIBC_TARGET_CPU_HAS_AVX512F)
template <typename T>
LIBC_INLINE_VAR constexpr size_t native_vector_size = 64 / sizeof(T);
#elif defined(LIBC_TARGET_CPU_HAS_AVX2)
template <typename T>
LIBC_INLINE_VAR constexpr size_t native_vector_size = 32 / sizeof(T);
#elif defined(LIBC_TARGET_CPU_HAS_SSE2) || defined(LIBC_TARGET_CPU_HAS_ARM_NEON)
template <typename T>
LIBC_INLINE_VAR constexpr size_t native_vector_size = 16 / sizeof(T);
#else
template <typename T> LIBC_INLINE constexpr size_t native_vector_size = 1;
#endif

} // namespace internal

// Type aliases.
template <typename T, size_t N>
using fixed_size_simd = T [[clang::ext_vector_type(N)]];
template <typename T, size_t N = internal::native_vector_size<T>>
using simd = T [[clang::ext_vector_type(N)]];
template <typename T>
using simd_mask = simd<bool, internal::native_vector_size<T>>;

// Type trait helpers.
template <typename T>
struct simd_size : cpp::integral_constant<size_t, __builtin_vectorelements(T)> {
};
template <class T> constexpr size_t simd_size_v = simd_size<T>::value;

template <typename T> struct is_simd : cpp::integral_constant<bool, false> {};
template <typename T, unsigned N>
struct is_simd<simd<T, N>> : cpp::integral_constant<bool, true> {};
template <class T> constexpr bool is_simd_v = is_simd<T>::value;

template <typename T>
struct is_simd_mask : cpp::integral_constant<bool, false> {};
template <unsigned N>
struct is_simd_mask<simd<bool, N>> : cpp::integral_constant<bool, true> {};
template <class T> constexpr bool is_simd_mask_v = is_simd_mask<T>::value;

template <typename T> struct simd_element_type;
template <typename T, size_t N> struct simd_element_type<simd<T, N>> {
  using type = T;
};
template <typename T>
using simd_element_type_t = typename simd_element_type<T>::type;
namespace internal {

template <typename T>
using get_as_integer_type_t = unsigned _BitInt(sizeof(T) * CHAR_BIT);

template <typename T> LIBC_INLINE constexpr T poison() {
  return __builtin_nondeterministic_value(T());
}

template <typename T, size_t N, size_t OriginalSize, size_t... Indices>
LIBC_INLINE constexpr static cpp::simd<T, sizeof...(Indices)>
extend(cpp::simd<T, N> x, cpp::index_sequence<Indices...>) {
  return __builtin_shufflevector(
      x, x, (Indices < OriginalSize ? static_cast<int>(Indices) : -1)...);
}

template <typename T, size_t N, size_t TargetSize, size_t OriginalSize>
LIBC_INLINE constexpr static auto extend(cpp::simd<T, N> x) {
  // Recursively resize an input vector to the target size, increasing its size
  // by at most double the input size each step due to shufflevector limitation.
  if constexpr (N == TargetSize)
    return x;
  else if constexpr (TargetSize <= 2 * N)
    return extend<T, N, TargetSize>(x, cpp::make_index_sequence<TargetSize>{});
  else
    return extend<T, 2 * N, TargetSize, OriginalSize>(
        extend<T, N, 2 * N>(x, cpp::make_index_sequence<2 * N>{}));
}

template <typename T, size_t N, size_t M, size_t... Indices>
LIBC_INLINE constexpr static cpp::simd<T, N + M>
concat(cpp::simd<T, N> x, cpp::simd<T, M> y, cpp::index_sequence<Indices...>) {
  constexpr size_t Size = cpp::max(N, M);
  auto remap = [](size_t idx) -> int {
    if (idx < N)
      return static_cast<int>(idx);
    if (idx < N + M)
      return static_cast<int>((idx - N) + Size);
    return -1;
  };

  // Extend the input vectors until they are the same size, then use the indices
  // to shuffle in only the indices that correspond to the original values.
  auto x_ext = extend<T, N, Size, N>(x);
  auto y_ext = extend<T, M, Size, M>(y);
  return __builtin_shufflevector(x_ext, y_ext, remap(Indices)...);
}

template <typename T, size_t N, size_t Count, size_t Offset, size_t... Indices>
LIBC_INLINE constexpr static cpp::simd<T, Count>
slice(cpp::simd<T, N> x, cpp::index_sequence<Indices...>) {
  return __builtin_shufflevector(x, x, (Offset + Indices)...);
}

template <typename T, size_t N, size_t Offset, size_t Head, size_t... Tail>
LIBC_INLINE constexpr static auto split(cpp::simd<T, N> x) {
  // Recursively splits the input vector by walking the variadic template list,
  // increasing our current head each call.
  auto result = cpp::make_tuple(
      slice<T, N, Head, Offset>(x, cpp::make_index_sequence<Head>{}));
  if constexpr (sizeof...(Tail) > 0)
    return cpp::tuple_cat(result, split<T, N, Offset + Head, Tail...>(x));
  else
    return result;
}

// Helper trait
template <typename T>
using enable_if_integral_t = cpp::enable_if_t<cpp::is_integral_v<T>, T>;

template <typename T>
using enable_if_simd_t = cpp::enable_if_t<is_simd_v<T>, bool>;

} // namespace internal

// Casting.
template <typename To, typename From, size_t N>
LIBC_INLINE constexpr static simd<To, N> simd_cast(simd<From, N> v) {
  return __builtin_convertvector(v, simd<To, N>);
}

// SIMD mask operations.
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static bool all_of(simd<T, N> v) {
  return __builtin_reduce_and(simd_cast<bool>(v));
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static bool any_of(simd<T, N> v) {
  return __builtin_reduce_or(simd_cast<bool>(v));
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static bool none_of(simd<T, N> v) {
  return !any_of(v);
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static bool some_of(simd<T, N> v) {
  return any_of(v) && !all_of(v);
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static int popcount(simd<T, N> v) {
  return __builtin_popcountg(v);
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static int find_first_set(simd<T, N> v) {
  return __builtin_ctzg(simd_cast<bool>(v));
}
template <typename T, size_t N, internal::enable_if_integral_t<T> = 0>
LIBC_INLINE constexpr static int find_last_set(simd<T, N> v) {
  constexpr size_t size = simd_size_v<simd<T, N>>;
  return size - 1 - __builtin_clzg(simd_cast<bool>(v));
}

// Elementwise operations.
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> min(simd<T, N> x, simd<T, N> y) {
  return __builtin_elementwise_min(x, y);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> max(simd<T, N> x, simd<T, N> y) {
  return __builtin_elementwise_max(x, y);
}

template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> abs(simd<T, N> x) {
  return __builtin_elementwise_abs(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> fma(simd<T, N> x, simd<T, N> y,
                                            simd<T, N> z) {
  return __builtin_elementwise_fma(x, y, z);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> ceil(simd<T, N> x) {
  return __builtin_elementwise_ceil(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> floor(simd<T, N> x) {
  return __builtin_elementwise_floor(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> roundeven(simd<T, N> x) {
  return __builtin_elementwise_roundeven(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> round(simd<T, N> x) {
  return __builtin_elementwise_round(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> trunc(simd<T, N> x) {
  return __builtin_elementwise_trunc(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> nearbyint(simd<T, N> x) {
  return __builtin_elementwise_nearbyint(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> rint(simd<T, N> x) {
  return __builtin_elementwise_rint(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> canonicalize(simd<T, N> x) {
  return __builtin_elementwise_canonicalize(x);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> copysign(simd<T, N> x, simd<T, N> y) {
  return __builtin_elementwise_copysign(x, y);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> fmod(simd<T, N> x, simd<T, N> y) {
  return __builtin_elementwise_fmod(x, y);
}

// Reduction operations.
template <typename T, size_t N, typename Op = cpp::plus<>>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, Op op = {}) {
  return reduce(v, op);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, cpp::plus<>) {
  return __builtin_reduce_add(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, cpp::multiplies<>) {
  return __builtin_reduce_mul(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, cpp::bit_and<>) {
  return __builtin_reduce_and(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, cpp::bit_or<>) {
  return __builtin_reduce_or(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T reduce(simd<T, N> v, cpp::bit_xor<>) {
  return __builtin_reduce_xor(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T hmin(simd<T, N> v) {
  return __builtin_reduce_min(v);
}
template <typename T, size_t N>
LIBC_INLINE constexpr static T hmax(simd<T, N> v) {
  return __builtin_reduce_max(v);
}

// Accessor helpers.
template <typename T>
LIBC_INLINE T constexpr static load(const void *ptr, bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  T tmp;
  __builtin_memcpy_inline(
      &tmp, reinterpret_cast<const simd_element_type_t<T> *>(ptr), sizeof(T));
  return tmp;
}
template <typename T, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static void store(T v, void *ptr, bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  __builtin_memcpy_inline(ptr, &v, sizeof(T));
}
template <typename T, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static T
load_masked(simd<bool, simd_size_v<T>> mask, const void *ptr,
            T passthru = internal::poison<T>(), bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  return __builtin_masked_load(
      mask, reinterpret_cast<const simd_element_type_t<T> *>(ptr), passthru);
}
template <typename T, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static void store_masked(simd<bool, simd_size_v<T>> mask,
                                               T v, void *ptr,
                                               bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  __builtin_masked_store(mask, v,
                         reinterpret_cast<simd_element_type_t<T> *>(ptr));
}
template <typename T, typename Idx, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static T gather(simd<bool, simd_size_v<T>> mask, Idx idx,
                                      const void *base, bool aligned = false) {
  if (aligned)
    base = __builtin_assume_aligned(base, alignof(T));
  return __builtin_masked_gather(
      mask, idx, reinterpret_cast<const simd_element_type_t<T> *>(base));
}
template <typename T, typename Idx, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static void scatter(simd<bool, simd_size_v<T>> mask,
                                          Idx idx, T v, void *base,
                                          bool aligned = false) {
  if (aligned)
    base = __builtin_assume_aligned(base, alignof(T));
  __builtin_masked_scatter(mask, idx, v,
                           reinterpret_cast<simd_element_type_t<T> *>(base));
}
template <typename T, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static T
expand(simd<bool, simd_size_v<T>> mask, const void *ptr,
       T passthru = internal::poison<T>(), bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  return __builtin_masked_expand_load(
      mask, reinterpret_cast<const simd_element_type_t<T> *>(ptr), passthru);
}
template <typename T, internal::enable_if_simd_t<T> = 0>
LIBC_INLINE constexpr static void compress(simd<bool, simd_size_v<T>> mask, T v,
                                           void *ptr, bool aligned = false) {
  if (aligned)
    ptr = __builtin_assume_aligned(ptr, alignof(T));
  __builtin_masked_compress_store(
      mask, v, reinterpret_cast<simd_element_type_t<T> *>(ptr));
}

// Construction helpers.
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> splat(T v) {
  return simd<T, N>(v);
}
template <typename T> LIBC_INLINE constexpr static simd<T> splat(T v) {
  return splat<T, simd_size_v<simd<T>>>(v);
}
template <typename T, unsigned N>
LIBC_INLINE constexpr static simd<T, N> iota(T base = T(0), T step = T(1)) {
  simd<T, N> v{};
  for (unsigned i = 0; i < N; ++i)
    v[i] = base + T(i) * step;
  return v;
}
template <typename T>
LIBC_INLINE constexpr static simd<T> iota(T base = T(0), T step = T(1)) {
  return iota<T, simd_size_v<simd<T>>>(base, step);
}

// Conditional helpers.
template <typename T, size_t N>
LIBC_INLINE constexpr static simd<T, N> select(simd<bool, N> m, simd<T, N> x,
                                               simd<T, N> y) {
  return m ? x : y;
}

// Shuffling helpers.
template <typename T, size_t N, size_t M>
LIBC_INLINE constexpr static auto concat(cpp::simd<T, N> x, cpp::simd<T, M> y) {
  return internal::concat(x, y, make_index_sequence<N + M>{});
}
template <typename T, size_t N, size_t M, typename... Rest>
LIBC_INLINE constexpr static auto concat(cpp::simd<T, N> x, cpp::simd<T, M> y,
                                         Rest... rest) {
  auto xy = concat(x, y);
  if constexpr (sizeof...(Rest))
    return concat(xy, rest...);
  else
    return xy;
}
template <size_t... Sizes, typename T, size_t N> auto split(cpp::simd<T, N> x) {
  static_assert((... + Sizes) == N, "split sizes must sum to vector size");
  return internal::split<T, N, 0, Sizes...>(x);
}

// TODO: where expressions, scalar overloads, ABI types.

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_HAS_VECTOR_TYPE
#endif
