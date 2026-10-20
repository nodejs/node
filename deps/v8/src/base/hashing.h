// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_HASHING_H_
#define V8_BASE_HASHING_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#include "src/base/base-export.h"
#include "src/base/bits.h"
#include "src/base/macros.h"
#include "third_party/rapidhash-v8/rapidhash.h"
#include "third_party/rapidhash-v8/secret.h"

namespace v8::base {

// base::hash is an implementation of the hash function object specified by
// C++11. It was designed to be compatible with std::hash (in C++11) and
// boost:hash (which in turn is based on the hash function object specified by
// the Draft Technical Report on C++ Library Extensions (TR1)).
//
// base::hash is implemented by calling either the hash_value function or the
// hash_value member function. In the first case, the namespace is not specified
// so that it can detect overloads via argument dependent lookup. So if there is
// a free function hash_value in the same namespace as a custom type, it will
// get called.
//
// If users are asked to implement a hash function for their own types with no
// guidance, they generally write bad hash functions. Instead, we provide a
// base::Hasher class to pass hash-relevant member variables into, in order to
// define a decent hash function.
//
// Consider the following example:
//
//   namespace v8 {
//   namespace bar {
//     struct Coordinate {
//       int val;
//       size_t hash_value() const { return hash_value(val); }
//     };
//     struct Point {
//       Coordinate x;
//       Coordinate y;
//     };
//     size_t hash_value(Point const& p) {
//       return base::Hasher::Combine(p.x, p.y);
//     }
//   }
//
//   namespace foo {
//     void DoSomeWork(bar::Point const& p) {
//       base::hash<bar::Point> h;
//       ...
//       size_t hash = h(p);  // calls bar::hash_value(Point const&), which
//                            // calls p.x.hash_value() and p.y.hash_value().
//       ...
//     }
//   }
//   }
//
// This header also provides implementations of hash_value for basic types.
//
// Based on the "Hashing User-Defined Types in C++1y" proposal from Jeffrey
// Yasskin and Chandler Carruth, see
// http://www.open-std.org/Jtc1/sc22/wg21/docs/papers/2012/n3333.html.

template <typename>
struct hash;

// Combine two hash values together. This code was taken from MurmurHash.
V8_INLINE size_t hash_combine(size_t seed, size_t hash) {
#if V8_HOST_ARCH_32_BIT
  const uint32_t c1 = 0xCC9E2D51;
  const uint32_t c2 = 0x1B873593;

  hash *= c1;
  hash = bits::RotateRight32(hash, 15);
  hash *= c2;

  seed ^= hash;
  seed = bits::RotateRight32(seed, 13);
  seed = seed * 5 + 0xE6546B64;
#else
  const uint64_t m = uint64_t{0xC6A4A7935BD1E995};
  const uint32_t r = 47;

  hash *= m;
  hash ^= hash >> r;
  hash *= m;

  seed ^= hash;
  seed *= m;
#endif  // V8_HOST_ARCH_32_BIT
  return seed;
}

#if V8_HASHES_COLLIDE
constexpr int kCollidingHash = 1234;
#endif  // V8_HASHES_COLLIDE

// base::Hasher makes it easier to combine multiple fields into one hash and
// avoids the ambiguity of the different {hash_combine} methods.
class Hasher {
 public:
  constexpr Hasher() = default;
  constexpr explicit Hasher(size_t seed) : hash_(seed) {}

  // Retrieve the current hash.
  constexpr size_t hash() const { return hash_; }

  // Combine an existing hash value into this hasher's hash.
  Hasher& AddHash(size_t other_hash) {
    hash_ = hash_combine(hash_, other_hash);
    return *this;
  }

  // Hash a value {t} and combine its hash into this hasher's hash.
  template <typename T>
  Hasher& Add(const T& t) {
    return AddHash(base::hash<T>{}(t));
  }

  // Hash a range of values and combine the hashes into this hasher's hash.
  template <typename Iterator>
  Hasher& AddRange(Iterator first, Iterator last) {
    // TODO(clemensb): If the iterator returns an integral or POD value smaller
    // than size_t we can combine multiple elements together to get better
    // hashing performance.
    for (; first != last; ++first) Add(*first);
    return *this;
  }

  // Hash a collection of values and combine the hashes into this hasher's hash.
  template <typename C>
  auto AddRange(C collection)
      -> decltype(AddRange(std::begin(collection), std::end(collection))) {
    return AddRange(std::begin(collection), std::end(collection));
  }

  // Hash multiple values and combine their hashes.
  template <typename... T>
  constexpr static size_t Combine(const T&... ts) {
    Hasher hasher;
    (..., hasher.Add(ts));
    return hasher.hash();
  }

 private:
  size_t hash_ = 0;
};

// rapidhash "mum" mixer secrets. Shared with the CSA / Turboshaft
// integer-hash emitters in builtins-collections-gen.cc and
// machine-lowering-reducer-inl.h; all implementations must agree
// bit-for-bit per-target so Map/Set and Dictionary lookups stay consistent
// between C++ and generated code. Aliased from the vendored rapidhash
// default secret so there is a single source of truth.
constexpr uint64_t kRapidhashSecret1 = RAPIDHASH_DEFAULT_SECRET[0];
constexpr uint64_t kRapidhashSecret2 = RAPIDHASH_DEFAULT_SECRET[1];

// Canonical integer hash. Uses the rapidhash "mum" mixer
// ((key ^ s1) * (key ^ s2) -> high64 ^ low64) on 64-bit targets. On 32-bit
// targets we fall back to Thomas Wang's integer hash functions to avoid a
// software 64x64->128 multiply. Distinct names per input width so the
// caller picks deliberately and no implicit-widening overload binding can
// silently change the mixer. The CSA mirror in
// CollectionsBuiltinsAssembler::ComputeUnseededHash and the Turboshaft mirror
// in MachineLoweringReducer::ComputeUnseededHash must stay in sync with
// hash32 followed by the kSmiHashMask mask (see SmiHash32 in utils.h).
V8_INLINE uint64_t hash64(uint64_t key) {
#if V8_TARGET_ARCH_64_BIT
  return rapid_mix(key ^ kRapidhashSecret1, key ^ kRapidhashSecret2);
#else
  // Thomas Wang, "64 bit to 32 bit Hash Functions".
  // http://www.concentric.net/~Ttwang/tech/inthash.htm`
  uint64_t hash = key;
  hash = ~hash + (hash << 18);  // hash = (hash << 18) - hash - 1;
  hash = hash ^ (hash >> 31);
  hash = hash * 21;  // hash = (hash + (hash << 2)) + (hash << 4);
  hash = hash ^ (hash >> 11);
  hash = hash + (hash << 6);
  hash = hash ^ (hash >> 22);
  return hash;
#endif
}

V8_INLINE uint32_t hash32(uint32_t key) {
#if V8_TARGET_ARCH_64_BIT
  return static_cast<uint32_t>(hash64(key));
#else
  // Thomas Wang, "32 bit Mix Functions".
  // http://www.concentric.net/~Ttwang/tech/inthash.htm`
  uint32_t hash = key;
  hash = ~hash + (hash << 15);  // hash = (hash << 15) - hash - 1;
  hash = hash ^ (hash >> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >> 4);
  hash = hash * 2057;  // hash = (hash + (hash << 3)) + (hash << 11);
  hash = hash ^ (hash >> 16);
  return hash;
#endif
}

// Block implicit narrowing from 64-bit to hash32. Widening into hash64 is
// fine and stays implicit.
uint32_t hash32(uint64_t) = delete;

#define V8_BASE_HASH_VALUE_TRIVIAL(type) \
  V8_INLINE size_t hash_value(type v) { return static_cast<size_t>(v); }
V8_BASE_HASH_VALUE_TRIVIAL(bool)
V8_BASE_HASH_VALUE_TRIVIAL(unsigned char)
V8_BASE_HASH_VALUE_TRIVIAL(unsigned short)  // NOLINT(runtime/int)
#undef V8_BASE_HASH_VALUE_TRIVIAL

V8_INLINE size_t hash_value(unsigned int v) { return hash32(v); }

V8_INLINE size_t hash_value(unsigned long v) {  // NOLINT(runtime/int)
  if constexpr (sizeof(v) == 4) {
    return hash32(static_cast<uint32_t>(v));
  } else {
    return hash64(v);
  }
}

V8_INLINE size_t hash_value(unsigned long long v) {  // NOLINT(runtime/int)
  return hash64(v);
}

#define V8_BASE_HASH_VALUE_SIGNED(type)                  \
  V8_INLINE size_t hash_value(signed type v) {           \
    return hash_value(base::bit_cast<unsigned type>(v)); \
  }
V8_BASE_HASH_VALUE_SIGNED(char)
V8_BASE_HASH_VALUE_SIGNED(short)      // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(int)        // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(long)       // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(long long)  // NOLINT(runtime/int)
#undef V8_BASE_HASH_VALUE_SIGNED

V8_INLINE size_t hash_value(float v) {
  // 0 and -0 both hash to zero.
  return v != 0.0f ? hash_value(base::bit_cast<uint32_t>(v)) : 0;
}

V8_INLINE size_t hash_value(double v) {
  // 0 and -0 both hash to zero.
  return v != 0.0 ? hash_value(base::bit_cast<uint64_t>(v)) : 0;
}

template <typename T, size_t N>
V8_INLINE size_t hash_value(const T (&v)[N]) {
  return Hasher{}.AddRange(v, v + N).hash();
}

template <typename T, size_t N>
V8_INLINE size_t hash_value(T (&v)[N]) {
  return Hasher{}.AddRange(v, v + N).hash();
}

template <typename T>
V8_INLINE size_t hash_value(T* const& v) {
  return hash_value(reinterpret_cast<uintptr_t>(v));
}

template <typename T1, typename T2>
V8_INLINE size_t hash_value(std::pair<T1, T2> const& v) {
  return Hasher::Combine(v.first, v.second);
}

template <typename... T, size_t... I>
V8_INLINE size_t hash_value_impl(std::tuple<T...> const& v,
                                 std::index_sequence<I...>) {
  return Hasher::Combine(std::get<I>(v)...);
}

template <typename... T>
V8_INLINE size_t hash_value(std::tuple<T...> const& v) {
  return hash_value_impl(v, std::make_index_sequence<sizeof...(T)>());
}

template <typename T>
V8_INLINE size_t hash_value(T v)
  requires std::is_enum_v<T>
{
  return hash_value(static_cast<std::underlying_type_t<T>>(v));
}

// Provide a hash_value function for each T with a hash_value member function.
template <typename T>
  requires requires(const T& t) {
    { t.hash_value() } -> std::convertible_to<size_t>;
  }
V8_INLINE size_t hash_value(const T& v) {
  return v.hash_value();
}

template <typename T>
concept Hashable = requires(const T& t) {
  { hash_value(t) } -> std::convertible_to<size_t>;
};

// Define base::hash to call the hash_value function.
template <Hashable T>
struct hash<T> {
  V8_INLINE constexpr size_t operator()(const T& v) const {
    return hash_value(v);
  }
};

// TODO(clemensb): Depending on the types in this template the compiler might
// pick {hash_combine(size_t, size_t)} instead. Thus remove this template and
// switch callers to {Hasher::Combine}.
template <typename... Ts>
V8_INLINE size_t hash_combine(Ts const&... vs) {
  return Hasher{}.Combine(vs...);
}

// TODO(clemensb): Switch users to {Hasher{}.AddRange(first, last).hash()}.
template <typename Iterator>
V8_INLINE size_t hash_range(Iterator first, Iterator last) {
  return Hasher{}.AddRange(first, last).hash();
}

// base::bit_equal_to is a function object class for bitwise equality
// comparison, similar to std::equal_to, except that the comparison is performed
// on the bit representation of the operands.
//
// base::bit_hash is a function object class for bitwise hashing, similar to
// base::hash. It can be used together with base::bit_equal_to to implement a
// hash data structure based on the bitwise representation of types.

template <typename T>
struct bit_equal_to {};

template <typename T>
struct bit_hash {};

#define V8_BASE_BIT_SPECIALIZE_TRIVIAL(type)                 \
  template <>                                                \
  struct bit_equal_to<type> : public std::equal_to<type> {}; \
  template <>                                                \
  struct bit_hash<type> : public hash<type> {};
V8_BASE_BIT_SPECIALIZE_TRIVIAL(signed char)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(unsigned char)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(short)           // NOLINT(runtime/int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(unsigned short)  // NOLINT(runtime/int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(unsigned int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(long)                // NOLINT(runtime/int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(unsigned long)       // NOLINT(runtime/int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(long long)           // NOLINT(runtime/int)
V8_BASE_BIT_SPECIALIZE_TRIVIAL(unsigned long long)  // NOLINT(runtime/int)
#undef V8_BASE_BIT_SPECIALIZE_TRIVIAL

#define V8_BASE_BIT_SPECIALIZE_BIT_CAST(type, btype)                   \
  template <>                                                          \
  struct bit_equal_to<type> {                                          \
    V8_INLINE bool operator()(type lhs, type rhs) const {              \
      return base::bit_cast<btype>(lhs) == base::bit_cast<btype>(rhs); \
    }                                                                  \
  };                                                                   \
  template <>                                                          \
  struct bit_hash<type> {                                              \
    V8_INLINE size_t operator()(type v) const {                        \
      hash<btype> h;                                                   \
      return h(base::bit_cast<btype>(v));                              \
    }                                                                  \
  };
V8_BASE_BIT_SPECIALIZE_BIT_CAST(float, uint32_t)
V8_BASE_BIT_SPECIALIZE_BIT_CAST(double, uint64_t)
#undef V8_BASE_BIT_SPECIALIZE_BIT_CAST

}  // namespace v8::base

#endif  // V8_BASE_HASHING_H_
