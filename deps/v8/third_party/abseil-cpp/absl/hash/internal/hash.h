// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: hash.h
// -----------------------------------------------------------------------------
//
#ifndef ABSL_HASH_INTERNAL_HASH_H_
#define ABSL_HASH_INTERNAL_HASH_H_

#ifdef __APPLE__
#include <Availability.h>
#include <TargetConditionals.h>
#endif

// We include config.h here to make sure that ABSL_INTERNAL_CPLUSPLUS_LANG is
// defined.
#include "absl/base/config.h"

// GCC15 warns that <ciso646> is deprecated in C++17 and suggests using
// <version> instead, even though <version> is not available in C++17 mode prior
// to GCC9.
#if defined(__has_include)
#if __has_include(<version>)
#define ABSL_INTERNAL_VERSION_HEADER_AVAILABLE 1
#endif
#endif

// For feature testing and determining which headers can be included.
#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L || \
    defined(ABSL_INTERNAL_VERSION_HEADER_AVAILABLE)
#include <version>
#else
#include <ciso646>
#endif

#undef ABSL_INTERNAL_VERSION_HEADER_AVAILABLE

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <forward_list>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/internal/unaligned_access.h"
#include "absl/base/optimization.h"
#include "absl/base/port.h"
#include "absl/container/fixed_array.h"
#include "absl/hash/internal/city.h"
#include "absl/hash/internal/weakly_mixed_integer.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"

#if defined(__cpp_lib_filesystem) && __cpp_lib_filesystem >= 201703L && \
    !defined(__XTENSA__)
#include <filesystem>  // NOLINT
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

class HashState;

namespace hash_internal {

// Internal detail: Large buffers are hashed in smaller chunks.  This function
// returns the size of these chunks.
constexpr size_t PiecewiseChunkSize() { return 1024; }

// PiecewiseCombiner is an internal-only helper class for hashing a piecewise
// buffer of `char` or `unsigned char` as though it were contiguous.  This class
// provides two methods:
//
//   H add_buffer(state, data, size)
//   H finalize(state)
//
// `add_buffer` can be called zero or more times, followed by a single call to
// `finalize`.  This will produce the same hash expansion as concatenating each
// buffer piece into a single contiguous buffer, and passing this to
// `H::combine_contiguous`.
//
//  Example usage:
//    PiecewiseCombiner combiner;
//    for (const auto& piece : pieces) {
//      state = combiner.add_buffer(std::move(state), piece.data, piece.size);
//    }
//    return combiner.finalize(std::move(state));
class PiecewiseCombiner {
 public:
  PiecewiseCombiner() = default;
  PiecewiseCombiner(const PiecewiseCombiner&) = delete;
  PiecewiseCombiner& operator=(const PiecewiseCombiner&) = delete;

  // Appends the given range of bytes to the sequence to be hashed, which may
  // modify the provided hash state.
  template <typename H>
  H add_buffer(H state, const unsigned char* data, size_t size);
  template <typename H>
  H add_buffer(H state, const char* data, size_t size) {
    return add_buffer(std::move(state),
                      reinterpret_cast<const unsigned char*>(data), size);
  }

  // Finishes combining the hash sequence, which may may modify the provided
  // hash state.
  //
  // Once finalize() is called, add_buffer() may no longer be called. The
  // resulting hash state will be the same as if the pieces passed to
  // add_buffer() were concatenated into a single flat buffer, and then provided
  // to H::combine_contiguous().
  template <typename H>
  H finalize(H state);

 private:
  unsigned char buf_[PiecewiseChunkSize()];
  size_t position_ = 0;
  bool added_something_ = false;
};

// Trait class which returns true if T is hashable by the absl::Hash framework.
// Used for the AbslHashValue implementations for composite types below.
template <typename T>
struct is_hashable;

// HashStateBase is an internal implementation detail that contains common
// implementation details for all of the "hash state objects" objects generated
// by Abseil.  This is not a public API; users should not create classes that
// inherit from this.
//
// A hash state object is the template argument `H` passed to `AbslHashValue`.
// It represents an intermediate state in the computation of an unspecified hash
// algorithm. `HashStateBase` provides a CRTP style base class for hash state
// implementations. Developers adding type support for `absl::Hash` should not
// rely on any parts of the state object other than the following member
// functions:
//
//   * HashStateBase::combine()
//   * HashStateBase::combine_contiguous()
//   * HashStateBase::combine_unordered()
//
// A derived hash state class of type `H` must provide a public member function
// with a signature similar to the following:
//
//    `static H combine_contiguous(H state, const unsigned char*, size_t)`.
//
// It must also provide a private template method named RunCombineUnordered.
//
// A "consumer" is a 1-arg functor returning void.  Its argument is a reference
// to an inner hash state object, and it may be called multiple times.  When
// called, the functor consumes the entropy from the provided state object,
// and resets that object to its empty state.
//
// A "combiner" is a stateless 2-arg functor returning void.  Its arguments are
// an inner hash state object and an ElementStateConsumer functor.  A combiner
// uses the provided inner hash state object to hash each element of the
// container, passing the inner hash state object to the consumer after hashing
// each element.
//
// Given these definitions, a derived hash state class of type H
// must provide a private template method with a signature similar to the
// following:
//
//    `template <typename CombinerT>`
//    `static H RunCombineUnordered(H outer_state, CombinerT combiner)`
//
// This function is responsible for constructing the inner state object and
// providing a consumer to the combiner.  It uses side effects of the consumer
// and combiner to mix the state of each element in an order-independent manner,
// and uses this to return an updated value of `outer_state`.
//
// This inside-out approach generates efficient object code in the normal case,
// but allows us to use stack storage to implement the absl::HashState type
// erasure mechanism (avoiding heap allocations while hashing).
//
// `HashStateBase` will provide a complete implementation for a hash state
// object in terms of these two methods.
//
// Example:
//
//   // Use CRTP to define your derived class.
//   struct MyHashState : HashStateBase<MyHashState> {
//       static H combine_contiguous(H state, const unsigned char*, size_t);
//       using MyHashState::HashStateBase::combine;
//       using MyHashState::HashStateBase::combine_contiguous;
//       using MyHashState::HashStateBase::combine_unordered;
//     private:
//       template <typename CombinerT>
//       static H RunCombineUnordered(H state, CombinerT combiner);
//   };
template <typename H>
class HashStateBase {
 public:
  // Combines an arbitrary number of values into a hash state, returning the
  // updated state.
  //
  // Each of the value types `T` must be separately hashable by the Abseil
  // hashing framework.
  //
  // NOTE:
  //
  //   state = H::combine(std::move(state), value1, value2, value3);
  //
  // is guaranteed to produce the same hash expansion as:
  //
  //   state = H::combine(std::move(state), value1);
  //   state = H::combine(std::move(state), value2);
  //   state = H::combine(std::move(state), value3);
  template <typename T, typename... Ts>
  static H combine(H state, const T& value, const Ts&... values);
  static H combine(H state) { return state; }

  // Combines a contiguous array of `size` elements into a hash state, returning
  // the updated state.
  //
  // NOTE:
  //
  //   state = H::combine_contiguous(std::move(state), data, size);
  //
  // is NOT guaranteed to produce the same hash expansion as a for-loop (it may
  // perform internal optimizations).  If you need this guarantee, use the
  // for-loop instead.
  template <typename T>
  static H combine_contiguous(H state, const T* data, size_t size);

  template <typename I>
  static H combine_unordered(H state, I begin, I end);

  using AbslInternalPiecewiseCombiner = PiecewiseCombiner;

  template <typename T>
  using is_hashable = absl::hash_internal::is_hashable<T>;

 private:
  // Common implementation of the iteration step of a "combiner", as described
  // above.
  template <typename I>
  struct CombineUnorderedCallback {
    I begin;
    I end;

    template <typename InnerH, typename ElementStateConsumer>
    void operator()(InnerH inner_state, ElementStateConsumer cb) {
      for (; begin != end; ++begin) {
        inner_state = H::combine(std::move(inner_state), *begin);
        cb(inner_state);
      }
    }
  };
};

// `is_uniquely_represented<T>` is a trait class that indicates whether `T`
// is uniquely represented.
//
// A type is "uniquely represented" if two equal values of that type are
// guaranteed to have the same bytes in their underlying storage. In other
// words, if `a == b`, then `memcmp(&a, &b, sizeof(T))` is guaranteed to be
// zero. This property cannot be detected automatically, so this trait is false
// by default, but can be specialized by types that wish to assert that they are
// uniquely represented. This makes them eligible for certain optimizations.
//
// If you have any doubt whatsoever, do not specialize this template.
// The default is completely safe, and merely disables some optimizations
// that will not matter for most types. Specializing this template,
// on the other hand, can be very hazardous.
//
// To be uniquely represented, a type must not have multiple ways of
// representing the same value; for example, float and double are not
// uniquely represented, because they have distinct representations for
// +0 and -0. Furthermore, the type's byte representation must consist
// solely of user-controlled data, with no padding bits and no compiler-
// controlled data such as vptrs or sanitizer metadata. This is usually
// very difficult to guarantee, because in most cases the compiler can
// insert data and padding bits at its own discretion.
//
// If you specialize this template for a type `T`, you must do so in the file
// that defines that type (or in this file). If you define that specialization
// anywhere else, `is_uniquely_represented<T>` could have different meanings
// in different places.
//
// The Enable parameter is meaningless; it is provided as a convenience,
// to support certain SFINAE techniques when defining specializations.
template <typename T, typename Enable = void>
struct is_uniquely_represented : std::false_type {};

// unsigned char is a synonym for "byte", so it is guaranteed to be
// uniquely represented.
template <>
struct is_uniquely_represented<unsigned char> : std::true_type {};

// is_uniquely_represented for non-standard integral types
//
// Integral types other than bool should be uniquely represented on any
// platform that this will plausibly be ported to.
template <typename Integral>
struct is_uniquely_represented<
    Integral, typename std::enable_if<std::is_integral<Integral>::value>::type>
    : std::true_type {};

template <>
struct is_uniquely_represented<bool> : std::false_type {};

#ifdef ABSL_HAVE_INTRINSIC_INT128
// Specialize the trait for GNU extension types.
template <>
struct is_uniquely_represented<__int128> : std::true_type {};
template <>
struct is_uniquely_represented<unsigned __int128> : std::true_type {};
#endif  // ABSL_HAVE_INTRINSIC_INT128

template <typename T>
struct FitsIn64Bits : std::integral_constant<bool, sizeof(T) <= 8> {};

struct CombineRaw {
  template <typename H>
  H operator()(H state, uint64_t value) const {
    return H::combine_raw(std::move(state), value);
  }
};

// For use in `raw_hash_set` to pass a seed to the hash function.
struct HashWithSeed {
  template <typename Hasher, typename T>
  size_t hash(const Hasher& hasher, const T& value, size_t seed) const {
    // NOLINTNEXTLINE(clang-diagnostic-sign-conversion)
    return hasher.hash_with_seed(value, seed);
  }
};

// Convenience function that combines `hash_state` with the byte representation
// of `value`.
template <typename H, typename T,
          absl::enable_if_t<FitsIn64Bits<T>::value, int> = 0>
H hash_bytes(H hash_state, const T& value) {
  const unsigned char* start = reinterpret_cast<const unsigned char*>(&value);
  uint64_t v;
  if constexpr (sizeof(T) == 1) {
    v = *start;
  } else if constexpr (sizeof(T) == 2) {
    v = absl::base_internal::UnalignedLoad16(start);
  } else if constexpr (sizeof(T) == 4) {
    v = absl::base_internal::UnalignedLoad32(start);
  } else {
    static_assert(sizeof(T) == 8);
    v = absl::base_internal::UnalignedLoad64(start);
  }
  return CombineRaw()(std::move(hash_state), v);
}
template <typename H, typename T,
          absl::enable_if_t<!FitsIn64Bits<T>::value, int> = 0>
H hash_bytes(H hash_state, const T& value) {
  const unsigned char* start = reinterpret_cast<const unsigned char*>(&value);
  return H::combine_contiguous(std::move(hash_state), start, sizeof(value));
}

template <typename H>
H hash_weakly_mixed_integer(H hash_state, WeaklyMixedInteger value) {
  return H::combine_weakly_mixed_integer(std::move(hash_state), value);
}

// -----------------------------------------------------------------------------
// AbslHashValue for Basic Types
// -----------------------------------------------------------------------------

// Note: Default `AbslHashValue` implementations live in `hash_internal`. This
// allows us to block lexical scope lookup when doing an unqualified call to
// `AbslHashValue` below. User-defined implementations of `AbslHashValue` can
// only be found via ADL.

// AbslHashValue() for hashing bool values
//
// We use SFINAE to ensure that this overload only accepts bool, not types that
// are convertible to bool.
template <typename H, typename B>
typename std::enable_if<std::is_same<B, bool>::value, H>::type AbslHashValue(
    H hash_state, B value) {
  // We use ~size_t{} instead of 1 so that all bits are different between
  // true/false instead of only 1.
  return H::combine(std::move(hash_state),
                    static_cast<size_t>(value ? ~size_t{} : 0));
}

// AbslHashValue() for hashing enum values
template <typename H, typename Enum>
typename std::enable_if<std::is_enum<Enum>::value, H>::type AbslHashValue(
    H hash_state, Enum e) {
  // In practice, we could almost certainly just invoke hash_bytes directly,
  // but it's possible that a sanitizer might one day want to
  // store data in the unused bits of an enum. To avoid that risk, we
  // convert to the underlying type before hashing. Hopefully this will get
  // optimized away; if not, we can reopen discussion with c-toolchain-team.
  return H::combine(std::move(hash_state),
                    static_cast<typename std::underlying_type<Enum>::type>(e));
}
// AbslHashValue() for hashing floating-point values
template <typename H, typename Float>
typename std::enable_if<std::is_same<Float, float>::value ||
                            std::is_same<Float, double>::value,
                        H>::type
AbslHashValue(H hash_state, Float value) {
  return hash_internal::hash_bytes(std::move(hash_state),
                                   value == 0 ? 0 : value);
}

// Long double has the property that it might have extra unused bytes in it.
// For example, in x86 sizeof(long double)==16 but it only really uses 80-bits
// of it. This means we can't use hash_bytes on a long double and have to
// convert it to something else first.
template <typename H, typename LongDouble>
typename std::enable_if<std::is_same<LongDouble, long double>::value, H>::type
AbslHashValue(H hash_state, LongDouble value) {
  const int category = std::fpclassify(value);
  switch (category) {
    case FP_INFINITE:
      // Add the sign bit to differentiate between +Inf and -Inf
      hash_state = H::combine(std::move(hash_state), std::signbit(value));
      break;

    case FP_NAN:
    case FP_ZERO:
    default:
      // Category is enough for these.
      break;

    case FP_NORMAL:
    case FP_SUBNORMAL:
      // We can't convert `value` directly to double because this would have
      // undefined behavior if the value is out of range.
      // std::frexp gives us a value in the range (-1, -.5] or [.5, 1) that is
      // guaranteed to be in range for `double`. The truncation is
      // implementation defined, but that works as long as it is deterministic.
      int exp;
      auto mantissa = static_cast<double>(std::frexp(value, &exp));
      hash_state = H::combine(std::move(hash_state), mantissa, exp);
  }

  return H::combine(std::move(hash_state), category);
}

// Without this overload, an array decays to a pointer and we hash that, which
// is not likely to be what the caller intended.
template <typename H, typename T, size_t N>
H AbslHashValue(H hash_state, T (&)[N]) {
  static_assert(
      sizeof(T) == -1,
      "Hashing C arrays is not allowed. For string literals, wrap the literal "
      "in absl::string_view(). To hash the array contents, use "
      "absl::MakeSpan() or make the array an std::array. To hash the array "
      "address, use &array[0].");
  return hash_state;
}

// AbslHashValue() for hashing pointers
template <typename H, typename T>
std::enable_if_t<std::is_pointer<T>::value, H> AbslHashValue(H hash_state,
                                                             T ptr) {
  auto v = reinterpret_cast<uintptr_t>(ptr);
  // Due to alignment, pointers tend to have low bits as zero, and the next few
  // bits follow a pattern since they are also multiples of some base value.
  // The PointerAlignment test verifies that our mixing is good enough to handle
  // these cases.
  return H::combine(std::move(hash_state), v);
}

// AbslHashValue() for hashing nullptr_t
template <typename H>
H AbslHashValue(H hash_state, std::nullptr_t) {
  return H::combine(std::move(hash_state), static_cast<void*>(nullptr));
}

// AbslHashValue() for hashing pointers-to-member
template <typename H, typename T, typename C>
H AbslHashValue(H hash_state, T C::*ptr) {
  auto salient_ptm_size = [](std::size_t n) -> std::size_t {
#if defined(_MSC_VER)
    // Pointers-to-member-function on MSVC consist of one pointer plus 0, 1, 2,
    // or 3 ints. In 64-bit mode, they are 8-byte aligned and thus can contain
    // padding (namely when they have 1 or 3 ints). The value below is a lower
    // bound on the number of salient, non-padding bytes that we use for
    // hashing.
    if constexpr (alignof(T C::*) == alignof(int)) {
      // No padding when all subobjects have the same size as the total
      // alignment. This happens in 32-bit mode.
      return n;
    } else {
      // Padding for 1 int (size 16) or 3 ints (size 24).
      // With 2 ints, the size is 16 with no padding, which we pessimize.
      return n == 24 ? 20 : n == 16 ? 12 : n;
    }
#else
  // On other platforms, we assume that pointers-to-members do not have
  // padding.
#ifdef __cpp_lib_has_unique_object_representations
    static_assert(std::has_unique_object_representations<T C::*>::value);
#endif  // __cpp_lib_has_unique_object_representations
    return n;
#endif
  };
  return H::combine_contiguous(std::move(hash_state),
                               reinterpret_cast<unsigned char*>(&ptr),
                               salient_ptm_size(sizeof ptr));
}

// -----------------------------------------------------------------------------
// AbslHashValue for Composite Types
// -----------------------------------------------------------------------------

// AbslHashValue() for hashing pairs
template <typename H, typename T1, typename T2>
typename std::enable_if<is_hashable<T1>::value && is_hashable<T2>::value,
                        H>::type
AbslHashValue(H hash_state, const std::pair<T1, T2>& p) {
  return H::combine(std::move(hash_state), p.first, p.second);
}

// Helper function for hashing a tuple. The third argument should
// be an index_sequence running from 0 to tuple_size<Tuple> - 1.
template <typename H, typename Tuple, size_t... Is>
H hash_tuple(H hash_state, const Tuple& t, absl::index_sequence<Is...>) {
  return H::combine(std::move(hash_state), std::get<Is>(t)...);
}

// AbslHashValue for hashing tuples
template <typename H, typename... Ts>
#if defined(_MSC_VER)
// This SFINAE gets MSVC confused under some conditions. Let's just disable it
// for now.
H
#else   // _MSC_VER
typename std::enable_if<absl::conjunction<is_hashable<Ts>...>::value, H>::type
#endif  // _MSC_VER
AbslHashValue(H hash_state, const std::tuple<Ts...>& t) {
  return hash_internal::hash_tuple(std::move(hash_state), t,
                                   absl::make_index_sequence<sizeof...(Ts)>());
}

// -----------------------------------------------------------------------------
// AbslHashValue for Pointers
// -----------------------------------------------------------------------------

// AbslHashValue for hashing unique_ptr
template <typename H, typename T, typename D>
H AbslHashValue(H hash_state, const std::unique_ptr<T, D>& ptr) {
  return H::combine(std::move(hash_state), ptr.get());
}

// AbslHashValue for hashing shared_ptr
template <typename H, typename T>
H AbslHashValue(H hash_state, const std::shared_ptr<T>& ptr) {
  return H::combine(std::move(hash_state), ptr.get());
}

// -----------------------------------------------------------------------------
// AbslHashValue for String-Like Types
// -----------------------------------------------------------------------------

// AbslHashValue for hashing strings
//
// All the string-like types supported here provide the same hash expansion for
// the same character sequence. These types are:
//
//  - `absl::Cord`
//  - `std::string` (and std::basic_string<T, std::char_traits<T>, A> for
//      any allocator A and any T in {char, wchar_t, char16_t, char32_t})
//  - `absl::string_view`, `std::string_view`, `std::wstring_view`,
//    `std::u16string_view`, and `std::u32_string_view`.
//
// For simplicity, we currently support only strings built on `char`, `wchar_t`,
// `char16_t`, or `char32_t`. This support may be broadened, if necessary, but
// with some caution - this overload would misbehave in cases where the traits'
// `eq()` member isn't equivalent to `==` on the underlying character type.
template <typename H>
H AbslHashValue(H hash_state, absl::string_view str) {
  return H::combine_contiguous(std::move(hash_state), str.data(), str.size());
}

// Support std::wstring, std::u16string and std::u32string.
template <typename Char, typename Alloc, typename H,
          typename = absl::enable_if_t<std::is_same<Char, wchar_t>::value ||
                                       std::is_same<Char, char16_t>::value ||
                                       std::is_same<Char, char32_t>::value>>
H AbslHashValue(
    H hash_state,
    const std::basic_string<Char, std::char_traits<Char>, Alloc>& str) {
  return H::combine_contiguous(std::move(hash_state), str.data(), str.size());
}

// Support std::wstring_view, std::u16string_view and std::u32string_view.
template <typename Char, typename H,
          typename = absl::enable_if_t<std::is_same<Char, wchar_t>::value ||
                                       std::is_same<Char, char16_t>::value ||
                                       std::is_same<Char, char32_t>::value>>
H AbslHashValue(H hash_state, std::basic_string_view<Char> str) {
  return H::combine_contiguous(std::move(hash_state), str.data(), str.size());
}

#if defined(__cpp_lib_filesystem) && __cpp_lib_filesystem >= 201703L && \
    (!defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) ||        \
     __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 130000) &&       \
    (!defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) ||         \
     __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500) &&        \
    (!defined(__XTENSA__))

#define ABSL_INTERNAL_STD_FILESYSTEM_PATH_HASH_AVAILABLE 1

// Support std::filesystem::path. The SFINAE is required because some string
// types are implicitly convertible to std::filesystem::path.
template <typename Path, typename H,
          typename = absl::enable_if_t<
              std::is_same_v<Path, std::filesystem::path>>>
H AbslHashValue(H hash_state, const Path& path) {
  // This is implemented by deferring to the standard library to compute the
  // hash.  The standard library requires that for two paths, `p1 == p2`, then
  // `hash_value(p1) == hash_value(p2)`. `AbslHashValue` has the same
  // requirement. Since `operator==` does platform specific matching, deferring
  // to the standard library is the simplest approach.
  return H::combine(std::move(hash_state), std::filesystem::hash_value(path));
}

#endif  // ABSL_INTERNAL_STD_FILESYSTEM_PATH_HASH_AVAILABLE

// -----------------------------------------------------------------------------
// AbslHashValue for Sequence Containers
// -----------------------------------------------------------------------------

// AbslHashValue for hashing std::array
template <typename H, typename T, size_t N>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, const std::array<T, N>& array) {
  return H::combine_contiguous(std::move(hash_state), array.data(),
                               array.size());
}

// AbslHashValue for hashing std::deque
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, const std::deque<T, Allocator>& deque) {
  // TODO(gromer): investigate a more efficient implementation taking
  // advantage of the chunk structure.
  for (const auto& t : deque) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{deque.size()});
}

// AbslHashValue for hashing std::forward_list
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, const std::forward_list<T, Allocator>& list) {
  size_t size = 0;
  for (const T& t : list) {
    hash_state = H::combine(std::move(hash_state), t);
    ++size;
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{size});
}

// AbslHashValue for hashing std::list
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, const std::list<T, Allocator>& list) {
  for (const auto& t : list) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{list.size()});
}

// AbslHashValue for hashing std::vector
//
// Do not use this for vector<bool> on platforms that have a working
// implementation of std::hash. It does not have a .data(), and a fallback for
// std::hash<> is most likely faster.
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value && !std::is_same<T, bool>::value,
                        H>::type
AbslHashValue(H hash_state, const std::vector<T, Allocator>& vector) {
  return H::combine_contiguous(std::move(hash_state), vector.data(),
                               vector.size());
}

// AbslHashValue special cases for hashing std::vector<bool>

#if defined(ABSL_IS_BIG_ENDIAN) && \
    (defined(__GLIBCXX__) || defined(__GLIBCPP__))

// std::hash in libstdc++ does not work correctly with vector<bool> on Big
// Endian platforms therefore we need to implement a custom AbslHashValue for
// it. More details on the bug:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102531
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value && std::is_same<T, bool>::value,
                        H>::type
AbslHashValue(H hash_state, const std::vector<T, Allocator>& vector) {
  typename H::AbslInternalPiecewiseCombiner combiner;
  for (const auto& i : vector) {
    unsigned char c = static_cast<unsigned char>(i);
    hash_state = combiner.add_buffer(std::move(hash_state), &c, sizeof(c));
  }
  return H::combine(combiner.finalize(std::move(hash_state)),
                    WeaklyMixedInteger{vector.size()});
}
#else
// When not working around the libstdc++ bug above, we still have to contend
// with the fact that std::hash<vector<bool>> is often poor quality, hashing
// directly on the internal words and on no other state.  On these platforms,
// vector<bool>{1, 1} and vector<bool>{1, 1, 0} hash to the same value.
//
// Mixing in the size (as we do in our other vector<> implementations) on top
// of the library-provided hash implementation avoids this QOI issue.
template <typename H, typename T, typename Allocator>
typename std::enable_if<is_hashable<T>::value && std::is_same<T, bool>::value,
                        H>::type
AbslHashValue(H hash_state, const std::vector<T, Allocator>& vector) {
  return H::combine(std::move(hash_state),
                    std::hash<std::vector<T, Allocator>>{}(vector),
                    WeaklyMixedInteger{vector.size()});
}
#endif

// -----------------------------------------------------------------------------
// AbslHashValue for Ordered Associative Containers
// -----------------------------------------------------------------------------

// AbslHashValue for hashing std::map
template <typename H, typename Key, typename T, typename Compare,
          typename Allocator>
typename std::enable_if<is_hashable<Key>::value && is_hashable<T>::value,
                        H>::type
AbslHashValue(H hash_state, const std::map<Key, T, Compare, Allocator>& map) {
  for (const auto& t : map) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{map.size()});
}

// AbslHashValue for hashing std::multimap
template <typename H, typename Key, typename T, typename Compare,
          typename Allocator>
typename std::enable_if<is_hashable<Key>::value && is_hashable<T>::value,
                        H>::type
AbslHashValue(H hash_state,
              const std::multimap<Key, T, Compare, Allocator>& map) {
  for (const auto& t : map) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{map.size()});
}

// AbslHashValue for hashing std::set
template <typename H, typename Key, typename Compare, typename Allocator>
typename std::enable_if<is_hashable<Key>::value, H>::type AbslHashValue(
    H hash_state, const std::set<Key, Compare, Allocator>& set) {
  for (const auto& t : set) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{set.size()});
}

// AbslHashValue for hashing std::multiset
template <typename H, typename Key, typename Compare, typename Allocator>
typename std::enable_if<is_hashable<Key>::value, H>::type AbslHashValue(
    H hash_state, const std::multiset<Key, Compare, Allocator>& set) {
  for (const auto& t : set) {
    hash_state = H::combine(std::move(hash_state), t);
  }
  return H::combine(std::move(hash_state), WeaklyMixedInteger{set.size()});
}

// -----------------------------------------------------------------------------
// AbslHashValue for Unordered Associative Containers
// -----------------------------------------------------------------------------

// AbslHashValue for hashing std::unordered_set
template <typename H, typename Key, typename Hash, typename KeyEqual,
          typename Alloc>
typename std::enable_if<is_hashable<Key>::value, H>::type AbslHashValue(
    H hash_state, const std::unordered_set<Key, Hash, KeyEqual, Alloc>& s) {
  return H::combine(
      H::combine_unordered(std::move(hash_state), s.begin(), s.end()),
      WeaklyMixedInteger{s.size()});
}

// AbslHashValue for hashing std::unordered_multiset
template <typename H, typename Key, typename Hash, typename KeyEqual,
          typename Alloc>
typename std::enable_if<is_hashable<Key>::value, H>::type AbslHashValue(
    H hash_state,
    const std::unordered_multiset<Key, Hash, KeyEqual, Alloc>& s) {
  return H::combine(
      H::combine_unordered(std::move(hash_state), s.begin(), s.end()),
      WeaklyMixedInteger{s.size()});
}

// AbslHashValue for hashing std::unordered_set
template <typename H, typename Key, typename T, typename Hash,
          typename KeyEqual, typename Alloc>
typename std::enable_if<is_hashable<Key>::value && is_hashable<T>::value,
                        H>::type
AbslHashValue(H hash_state,
              const std::unordered_map<Key, T, Hash, KeyEqual, Alloc>& s) {
  return H::combine(
      H::combine_unordered(std::move(hash_state), s.begin(), s.end()),
      WeaklyMixedInteger{s.size()});
}

// AbslHashValue for hashing std::unordered_multiset
template <typename H, typename Key, typename T, typename Hash,
          typename KeyEqual, typename Alloc>
typename std::enable_if<is_hashable<Key>::value && is_hashable<T>::value,
                        H>::type
AbslHashValue(H hash_state,
              const std::unordered_multimap<Key, T, Hash, KeyEqual, Alloc>& s) {
  return H::combine(
      H::combine_unordered(std::move(hash_state), s.begin(), s.end()),
      WeaklyMixedInteger{s.size()});
}

// -----------------------------------------------------------------------------
// AbslHashValue for Wrapper Types
// -----------------------------------------------------------------------------

// AbslHashValue for hashing std::reference_wrapper
template <typename H, typename T>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, std::reference_wrapper<T> opt) {
  return H::combine(std::move(hash_state), opt.get());
}

// AbslHashValue for hashing absl::optional
template <typename H, typename T>
typename std::enable_if<is_hashable<T>::value, H>::type AbslHashValue(
    H hash_state, const absl::optional<T>& opt) {
  if (opt) hash_state = H::combine(std::move(hash_state), *opt);
  return H::combine(std::move(hash_state), opt.has_value());
}

template <typename H>
struct VariantVisitor {
  H&& hash_state;
  template <typename T>
  H operator()(const T& t) const {
    return H::combine(std::move(hash_state), t);
  }
};

// AbslHashValue for hashing absl::variant
template <typename H, typename... T>
typename std::enable_if<conjunction<is_hashable<T>...>::value, H>::type
AbslHashValue(H hash_state, const absl::variant<T...>& v) {
  if (!v.valueless_by_exception()) {
    hash_state = absl::visit(VariantVisitor<H>{std::move(hash_state)}, v);
  }
  return H::combine(std::move(hash_state), v.index());
}

// -----------------------------------------------------------------------------
// AbslHashValue for Other Types
// -----------------------------------------------------------------------------

// AbslHashValue for hashing std::bitset is not defined on Little Endian
// platforms, for the same reason as for vector<bool> (see std::vector above):
// It does not expose the raw bytes, and a fallback to std::hash<> is most
// likely faster.

#if defined(ABSL_IS_BIG_ENDIAN) && \
    (defined(__GLIBCXX__) || defined(__GLIBCPP__))
// AbslHashValue for hashing std::bitset
//
// std::hash in libstdc++ does not work correctly with std::bitset on Big Endian
// platforms therefore we need to implement a custom AbslHashValue for it. More
// details on the bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102531
template <typename H, size_t N>
H AbslHashValue(H hash_state, const std::bitset<N>& set) {
  typename H::AbslInternalPiecewiseCombiner combiner;
  for (size_t i = 0; i < N; i++) {
    unsigned char c = static_cast<unsigned char>(set[i]);
    hash_state = combiner.add_buffer(std::move(hash_state), &c, sizeof(c));
  }
  return H::combine(combiner.finalize(std::move(hash_state)), N);
}
#endif

// -----------------------------------------------------------------------------

// Mixes all values in the range [data, data+size) into the hash state.
// This overload accepts only uniquely-represented types, and hashes them by
// hashing the entire range of bytes.
template <typename H, typename T>
typename std::enable_if<is_uniquely_represented<T>::value, H>::type
hash_range_or_bytes(H hash_state, const T* data, size_t size) {
  const auto* bytes = reinterpret_cast<const unsigned char*>(data);
  return H::combine_contiguous(std::move(hash_state), bytes, sizeof(T) * size);
}

template <typename H, typename T>
typename std::enable_if<!is_uniquely_represented<T>::value, H>::type
hash_range_or_bytes(H hash_state, const T* data, size_t size) {
  for (const auto end = data + size; data < end; ++data) {
    hash_state = H::combine(std::move(hash_state), *data);
  }
  return H::combine(std::move(hash_state),
                    hash_internal::WeaklyMixedInteger{size});
}

inline constexpr uint64_t kMul = uint64_t{0x79d5f9e0de1e8cf5};

// Random data taken from the hexadecimal digits of Pi's fractional component.
// https://en.wikipedia.org/wiki/Nothing-up-my-sleeve_number
ABSL_CACHELINE_ALIGNED inline constexpr uint64_t kStaticRandomData[] = {
    0x243f'6a88'85a3'08d3, 0x1319'8a2e'0370'7344, 0xa409'3822'299f'31d0,
    0x082e'fa98'ec4e'6c89, 0x4528'21e6'38d0'1377,
};

// Extremely weak mixture of length that is mixed into the state before
// combining the data. It is used only for small strings. This also ensures that
// we have high entropy in all bits of the state.
inline uint64_t PrecombineLengthMix(uint64_t state, size_t len) {
  ABSL_ASSUME(len + sizeof(uint64_t) <= sizeof(kStaticRandomData));
  uint64_t data = absl::base_internal::UnalignedLoad64(
      reinterpret_cast<const unsigned char*>(&kStaticRandomData[0]) + len);
  return state ^ data;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t Mix(uint64_t lhs, uint64_t rhs) {
  // Though the 128-bit product needs multiple instructions on non-x86-64
  // platforms, it is still a good balance between speed and hash quality.
  absl::uint128 m = lhs;
  m *= rhs;
  return Uint128High64(m) ^ Uint128Low64(m);
}

// Reads 8 bytes from p.
inline uint64_t Read8(const unsigned char* p) {
// Suppress erroneous array bounds errors on GCC.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
  return absl::base_internal::UnalignedLoad64(p);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// Reads 9 to 16 bytes from p.
// The first 8 bytes are in .first, and the rest of the bytes are in .second
// along with duplicated bytes from .first if len<16.
inline std::pair<uint64_t, uint64_t> Read9To16(const unsigned char* p,
                                               size_t len) {
  return {Read8(p), Read8(p + len - 8)};
}

// Reads 4 to 8 bytes from p.
// Bytes are permuted and some input bytes may be duplicated in output.
inline uint64_t Read4To8(const unsigned char* p, size_t len) {
  // If `len < 8`, we duplicate bytes. We always put low memory at the end.
  // E.g., on little endian platforms:
  // `ABCD` will be read as `ABCDABCD`.
  // `ABCDE` will be read as `BCDEABCD`.
  // `ABCDEF` will be read as `CDEFABCD`.
  // `ABCDEFG` will be read as `DEFGABCD`.
  // `ABCDEFGH` will be read as `EFGHABCD`.
  // We also do not care about endianness. On big-endian platforms, bytes will
  // be permuted differently. We always shift low memory by 32, because that
  // can be pipelined earlier. Reading high memory requires computing
  // `p + len - 4`.
  uint64_t most_significant =
      static_cast<uint64_t>(absl::base_internal::UnalignedLoad32(p)) << 32;
  uint64_t least_significant =
      absl::base_internal::UnalignedLoad32(p + len - 4);
  return most_significant | least_significant;
}

// Reads 1 to 3 bytes from p. Some input bytes may be duplicated in output.
inline uint32_t Read1To3(const unsigned char* p, size_t len) {
  // The trick used by this implementation is to avoid branches.
  // We always read three bytes by duplicating.
  // E.g.,
  // `A` is read as `AAA`.
  // `AB` is read as `ABB`.
  // `ABC` is read as `ABC`.
  // We always shift `p[0]` so that it can be pipelined better.
  // Other bytes require extra computation to find indices.
  uint32_t mem0 = (static_cast<uint32_t>(p[0]) << 16) | p[len - 1];
  uint32_t mem1 = static_cast<uint32_t>(p[len / 2]) << 8;
  return mem0 | mem1;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t CombineRawImpl(uint64_t state,
                                                            uint64_t value) {
  return Mix(state ^ value, kMul);
}

// Slow dispatch path for calls to CombineContiguousImpl with a size argument
// larger than inlined size. Has the same effect as calling
// CombineContiguousImpl() repeatedly with the chunk stride size.
uint64_t CombineLargeContiguousImplOn32BitLengthGt8(const unsigned char* first,
                                                    size_t len, uint64_t state);
uint64_t CombineLargeContiguousImplOn64BitLengthGt32(const unsigned char* first,
                                                     size_t len,
                                                     uint64_t state);

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t CombineSmallContiguousImpl(
    uint64_t state, const unsigned char* first, size_t len) {
  ABSL_ASSUME(len <= 8);
  uint64_t v;
  if (len >= 4) {
    v = Read4To8(first, len);
  } else if (len > 0) {
    v = Read1To3(first, len);
  } else {
    // Empty string must modify the state.
    v = 0x57;
  }
  return CombineRawImpl(state, v);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t CombineContiguousImpl9to16(
    uint64_t state, const unsigned char* first, size_t len) {
  ABSL_ASSUME(len >= 9);
  ABSL_ASSUME(len <= 16);
  // Note: any time one half of the mix function becomes zero it will fail to
  // incorporate any bits from the other half. However, there is exactly 1 in
  // 2^64 values for each side that achieve this, and only when the size is
  // exactly 16 -- for smaller sizes there is an overlapping byte that makes
  // this impossible unless the seed is *also* incredibly unlucky.
  auto p = Read9To16(first, len);
  return Mix(state ^ p.first, kMul ^ p.second);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t CombineContiguousImpl17to32(
    uint64_t state, const unsigned char* first, size_t len) {
  ABSL_ASSUME(len >= 17);
  ABSL_ASSUME(len <= 32);
  // Do two mixes of overlapping 16-byte ranges in parallel to minimize
  // latency.
  const uint64_t m0 =
      Mix(Read8(first) ^ kStaticRandomData[1], Read8(first + 8) ^ state);

  const unsigned char* tail_16b_ptr = first + (len - 16);
  const uint64_t m1 = Mix(Read8(tail_16b_ptr) ^ kStaticRandomData[3],
                          Read8(tail_16b_ptr + 8) ^ state);
  return m0 ^ m1;
}

// Implementation of the base case for combine_contiguous where we actually
// mix the bytes into the state.
// Dispatch to different implementations of combine_contiguous depending
// on the value of `sizeof(size_t)`.
inline uint64_t CombineContiguousImpl(
    uint64_t state, const unsigned char* first, size_t len,
    std::integral_constant<int, 4> /* sizeof_size_t */) {
  // For large values we use CityHash, for small ones we use custom low latency
  // hash.
  if (len <= 8) {
    return CombineSmallContiguousImpl(PrecombineLengthMix(state, len), first,
                                      len);
  }
  return CombineLargeContiguousImplOn32BitLengthGt8(first, len, state);
}

inline uint64_t CombineContiguousImpl(
    uint64_t state, const unsigned char* first, size_t len,
    std::integral_constant<int, 8> /* sizeof_size_t */) {
  // For large values we use LowLevelHash or CityHash depending on the platform,
  // for small ones we use custom low latency hash.
  if (len <= 8) {
    return CombineSmallContiguousImpl(PrecombineLengthMix(state, len), first,
                                      len);
  }
  if (len <= 16) {
    return CombineContiguousImpl9to16(PrecombineLengthMix(state, len), first,
                                      len);
  }
  if (len <= 32) {
    return CombineContiguousImpl17to32(PrecombineLengthMix(state, len), first,
                                       len);
  }
  // We must not mix length into the state here because calling
  // CombineContiguousImpl twice with PiecewiseChunkSize() must be equivalent
  // to calling CombineLargeContiguousImpl once with 2 * PiecewiseChunkSize().
  return CombineLargeContiguousImplOn64BitLengthGt32(first, len, state);
}

#if defined(ABSL_INTERNAL_LEGACY_HASH_NAMESPACE) && \
    ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
#define ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_ 1
#else
#define ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_ 0
#endif

// Type trait to select the appropriate hash implementation to use.
// HashSelect::type<T> will give the proper hash implementation, to be invoked
// as:
//   HashSelect::type<T>::Invoke(state, value)
// Also, HashSelect::type<T>::value is a boolean equal to `true` if there is a
// valid `Invoke` function. Types that are not hashable will have a ::value of
// `false`.
struct HashSelect {
 private:
  struct WeaklyMixedIntegerProbe {
    template <typename H>
    static H Invoke(H state, WeaklyMixedInteger value) {
      return hash_internal::hash_weakly_mixed_integer(std::move(state), value);
    }
  };

  struct State : HashStateBase<State> {
    static State combine_contiguous(State hash_state, const unsigned char*,
                                    size_t);
    using State::HashStateBase::combine_contiguous;
    static State combine_raw(State state, uint64_t value);
    static State combine_weakly_mixed_integer(State hash_state,
                                              WeaklyMixedInteger value);
  };

  struct UniquelyRepresentedProbe {
    template <typename H, typename T>
    static auto Invoke(H state, const T& value)
        -> absl::enable_if_t<is_uniquely_represented<T>::value, H> {
      return hash_internal::hash_bytes(std::move(state), value);
    }
  };

  struct HashValueProbe {
    template <typename H, typename T>
    static auto Invoke(H state, const T& value) -> absl::enable_if_t<
        std::is_same<H,
                     decltype(AbslHashValue(std::move(state), value))>::value,
        H> {
      return AbslHashValue(std::move(state), value);
    }
  };

  struct LegacyHashProbe {
#if ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_
    template <typename H, typename T>
    static auto Invoke(H state, const T& value) -> absl::enable_if_t<
        std::is_convertible<
            decltype(ABSL_INTERNAL_LEGACY_HASH_NAMESPACE::hash<T>()(value)),
            size_t>::value,
        H> {
      return hash_internal::hash_bytes(
          std::move(state),
          ABSL_INTERNAL_LEGACY_HASH_NAMESPACE::hash<T>{}(value));
    }
#endif  // ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_
  };

  struct StdHashProbe {
    template <typename H, typename T>
    static auto Invoke(H state, const T& value)
        -> absl::enable_if_t<type_traits_internal::IsHashable<T>::value, H> {
      return hash_internal::hash_bytes(std::move(state), std::hash<T>{}(value));
    }
  };

  template <typename Hash, typename T>
  struct Probe : Hash {
   private:
    template <typename H, typename = decltype(H::Invoke(
                              std::declval<State>(), std::declval<const T&>()))>
    static std::true_type Test(int);
    template <typename U>
    static std::false_type Test(char);

   public:
    static constexpr bool value = decltype(Test<Hash>(0))::value;
  };

 public:
  // Probe each implementation in order.
  // disjunction provides short circuiting wrt instantiation.
  template <typename T>
  using Apply = absl::disjunction<         //
      Probe<WeaklyMixedIntegerProbe, T>,   //
      Probe<UniquelyRepresentedProbe, T>,  //
      Probe<HashValueProbe, T>,            //
      Probe<LegacyHashProbe, T>,           //
      Probe<StdHashProbe, T>,              //
      std::false_type>;
};

template <typename T>
struct is_hashable
    : std::integral_constant<bool, HashSelect::template Apply<T>::value> {};

class ABSL_DLL MixingHashState : public HashStateBase<MixingHashState> {
  template <typename T>
  using IntegralFastPath =
      conjunction<std::is_integral<T>, is_uniquely_represented<T>,
                  FitsIn64Bits<T>>;

 public:
  // Move only
  MixingHashState(MixingHashState&&) = default;
  MixingHashState& operator=(MixingHashState&&) = default;

  // Fundamental base case for hash recursion: mixes the given range of bytes
  // into the hash state.
  static MixingHashState combine_contiguous(MixingHashState hash_state,
                                            const unsigned char* first,
                                            size_t size) {
    return MixingHashState(
        CombineContiguousImpl(hash_state.state_, first, size,
                              std::integral_constant<int, sizeof(size_t)>{}));
  }
  using MixingHashState::HashStateBase::combine_contiguous;

  template <typename T>
  static size_t hash(const T& value) {
    return hash_with_seed(value, Seed());
  }

  // For performance reasons in non-opt mode, we specialize this for
  // integral types.
  // Otherwise we would be instantiating and calling dozens of functions for
  // something that is just one multiplication and a couple xor's.
  // The result should be the same as running the whole algorithm, but faster.
  template <typename T, absl::enable_if_t<IntegralFastPath<T>::value, int> = 0>
  static size_t hash_with_seed(T value, size_t seed) {
    return static_cast<size_t>(
        CombineRawImpl(seed, static_cast<std::make_unsigned_t<T>>(value)));
  }

  template <typename T, absl::enable_if_t<!IntegralFastPath<T>::value, int> = 0>
  static size_t hash_with_seed(const T& value, size_t seed) {
    return static_cast<size_t>(combine(MixingHashState{seed}, value).state_);
  }

 private:
  friend class MixingHashState::HashStateBase;
  template <typename H>
  friend H absl::hash_internal::hash_weakly_mixed_integer(H,
                                                          WeaklyMixedInteger);
  // Allow the HashState type-erasure implementation to invoke
  // RunCombinedUnordered() directly.
  friend class absl::HashState;
  friend struct CombineRaw;

  // For use in Seed().
  static const void* const kSeed;

  // Invoked only once for a given argument; that plus the fact that this is
  // move-only ensures that there is only one non-moved-from object.
  MixingHashState() : state_(Seed()) {}

  // Workaround for MSVC bug.
  // We make the type copyable to fix the calling convention, even though we
  // never actually copy it. Keep it private to not affect the public API of the
  // type.
  MixingHashState(const MixingHashState&) = default;

  explicit MixingHashState(uint64_t state) : state_(state) {}

  // Combines a raw value from e.g. integrals/floats/pointers/etc. This allows
  // us to be consistent with IntegralFastPath when combining raw types, but
  // optimize Read1To3 and Read4To8 differently for the string case.
  static MixingHashState combine_raw(MixingHashState hash_state,
                                     uint64_t value) {
    return MixingHashState(CombineRawImpl(hash_state.state_, value));
  }

  static MixingHashState combine_weakly_mixed_integer(
      MixingHashState hash_state, WeaklyMixedInteger value) {
    // Some transformation for the value is needed to make an empty
    // string/container change the mixing hash state.
    // We use constant smaller than 8 bits to make compiler use
    // `add` with an immediate operand with 1 byte value.
    return MixingHashState{hash_state.state_ + (0x57 + value.value)};
  }

  template <typename CombinerT>
  static MixingHashState RunCombineUnordered(MixingHashState state,
                                             CombinerT combiner) {
    uint64_t unordered_state = 0;
    combiner(MixingHashState{}, [&](MixingHashState& inner_state) {
      // Add the hash state of the element to the running total, but mix the
      // carry bit back into the low bit.  This in intended to avoid losing
      // entropy to overflow, especially when unordered_multisets contain
      // multiple copies of the same value.
      auto element_state = inner_state.state_;
      unordered_state += element_state;
      if (unordered_state < element_state) {
        ++unordered_state;
      }
      inner_state = MixingHashState{};
    });
    return MixingHashState::combine(std::move(state), unordered_state);
  }

  // A non-deterministic seed.
  //
  // The current purpose of this seed is to generate non-deterministic results
  // and prevent having users depend on the particular hash values.
  // It is not meant as a security feature right now, but it leaves the door
  // open to upgrade it to a true per-process random seed. A true random seed
  // costs more and we don't need to pay for that right now.
  //
  // On platforms with ASLR, we take advantage of it to make a per-process
  // random value.
  // See https://en.wikipedia.org/wiki/Address_space_layout_randomization
  //
  // On other platforms this is still going to be non-deterministic but most
  // probably per-build and not per-process.
  ABSL_ATTRIBUTE_ALWAYS_INLINE static size_t Seed() {
#if (!defined(__clang__) || __clang_major__ > 11) && \
    (!defined(__apple_build_version__) ||            \
     __apple_build_version__ >= 19558921)  // Xcode 12
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(&kSeed));
#else
    // Workaround the absence of
    // https://github.com/llvm/llvm-project/commit/bc15bf66dcca76cc06fe71fca35b74dc4d521021.
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(kSeed));
#endif
  }

  uint64_t state_;
};

struct AggregateBarrier {};

// Add a private base class to make sure this type is not an aggregate.
// Aggregates can be aggregate initialized even if the default constructor is
// deleted.
struct PoisonedHash : private AggregateBarrier {
  PoisonedHash() = delete;
  PoisonedHash(const PoisonedHash&) = delete;
  PoisonedHash& operator=(const PoisonedHash&) = delete;
};

template <typename T>
struct HashImpl {
  size_t operator()(const T& value) const {
    return MixingHashState::hash(value);
  }

 private:
  friend struct HashWithSeed;

  size_t hash_with_seed(const T& value, size_t seed) const {
    return MixingHashState::hash_with_seed(value, seed);
  }
};

template <typename T>
struct Hash
    : absl::conditional_t<is_hashable<T>::value, HashImpl<T>, PoisonedHash> {};

template <typename H>
template <typename T, typename... Ts>
H HashStateBase<H>::combine(H state, const T& value, const Ts&... values) {
  return H::combine(hash_internal::HashSelect::template Apply<T>::Invoke(
                        std::move(state), value),
                    values...);
}

template <typename H>
template <typename T>
H HashStateBase<H>::combine_contiguous(H state, const T* data, size_t size) {
  return hash_internal::hash_range_or_bytes(std::move(state), data, size);
}

template <typename H>
template <typename I>
H HashStateBase<H>::combine_unordered(H state, I begin, I end) {
  return H::RunCombineUnordered(std::move(state),
                                CombineUnorderedCallback<I>{begin, end});
}

template <typename H>
H PiecewiseCombiner::add_buffer(H state, const unsigned char* data,
                                size_t size) {
  if (position_ + size < PiecewiseChunkSize()) {
    // This partial chunk does not fill our existing buffer
    memcpy(buf_ + position_, data, size);
    position_ += size;
    return state;
  }
  added_something_ = true;
  // If the buffer is partially filled we need to complete the buffer
  // and hash it.
  if (position_ != 0) {
    const size_t bytes_needed = PiecewiseChunkSize() - position_;
    memcpy(buf_ + position_, data, bytes_needed);
    state = H::combine_contiguous(std::move(state), buf_, PiecewiseChunkSize());
    data += bytes_needed;
    size -= bytes_needed;
  }

  // Hash whatever chunks we can without copying
  while (size >= PiecewiseChunkSize()) {
    state = H::combine_contiguous(std::move(state), data, PiecewiseChunkSize());
    data += PiecewiseChunkSize();
    size -= PiecewiseChunkSize();
  }
  // Fill the buffer with the remainder
  memcpy(buf_, data, size);
  position_ = size;
  return state;
}

template <typename H>
H PiecewiseCombiner::finalize(H state) {
  // Do not call combine_contiguous with empty remainder since it is modifying
  // state.
  if (added_something_ && position_ == 0) {
    return state;
  }
  // We still call combine_contiguous for the entirely empty buffer.
  return H::combine_contiguous(std::move(state), buf_, position_);
}

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_HASH_INTERNAL_HASH_H_
