// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_FUNCTIONAL_H_
#define V8_BASE_FUNCTIONAL_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstring>
#include <functional>
#include <utility>

#include "src/base/base-export.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// base::hash is an implementation of the hash function object specified by
// C++11. It was designed to be compatible with std::hash (in C++11) and
// boost:hash (which in turn is based on the hash function object specified by
// the Draft Technical Report on C++ Library Extensions (TR1)).
//
// base::hash is implemented by calling the hash_value function. The namespace
// isn't specified so that it can detect overloads via argument dependent
// lookup. So if there is a free function hash_value in the same namespace as a
// custom type, it will get called.
//
// If users are asked to implement a hash function for their own types with no
// guidance, they generally write bad hash functions. Instead, we provide  a
// simple function base::hash_combine to pass hash-relevant member variables
// into, in order to define a decent hash function. base::hash_combine is
// declared as:
//
//   template<typename T, typename... Ts>
//   size_t hash_combine(const T& v, const Ts& ...vs);
//
// Consider the following example:
//
//   namespace v8 {
//   namespace bar {
//     struct Point { int x; int y; };
//     size_t hash_value(Point const& p) {
//       return base::hash_combine(p.x, p.y);
//     }
//   }
//
//   namespace foo {
//     void DoSomeWork(bar::Point const& p) {
//       base::hash<bar::Point> h;
//       ...
//       size_t hash_code = h(p);  // calls bar::hash_value(Point const&)
//       ...
//     }
//   }
//   }
//
// Based on the "Hashing User-Defined Types in C++1y" proposal from Jeffrey
// Yasskin and Chandler Carruth, see
// http://www.open-std.org/Jtc1/sc22/wg21/docs/papers/2012/n3333.html.

template <typename>
struct hash;


V8_INLINE size_t hash_combine() { return 0u; }
V8_INLINE size_t hash_combine(size_t seed) { return seed; }
V8_BASE_EXPORT size_t hash_combine(size_t seed, size_t value);
template <typename T, typename... Ts>
V8_INLINE size_t hash_combine(T const& v, Ts const&... vs) {
  return hash_combine(hash_combine(vs...), hash<T>()(v));
}


template <typename Iterator>
V8_INLINE size_t hash_range(Iterator first, Iterator last) {
  size_t seed = 0;
  for (; first != last; ++first) {
    seed = hash_combine(seed, *first);
  }
  return seed;
}


#define V8_BASE_HASH_VALUE_TRIVIAL(type) \
  V8_INLINE size_t hash_value(type v) { return static_cast<size_t>(v); }
V8_BASE_HASH_VALUE_TRIVIAL(bool)
V8_BASE_HASH_VALUE_TRIVIAL(unsigned char)
V8_BASE_HASH_VALUE_TRIVIAL(unsigned short)  // NOLINT(runtime/int)
#undef V8_BASE_HASH_VALUE_TRIVIAL

V8_BASE_EXPORT size_t hash_value(unsigned int);
V8_BASE_EXPORT size_t hash_value(unsigned long);       // NOLINT(runtime/int)
V8_BASE_EXPORT size_t hash_value(unsigned long long);  // NOLINT(runtime/int)

#define V8_BASE_HASH_VALUE_SIGNED(type)            \
  V8_INLINE size_t hash_value(signed type v) {     \
    return hash_value(bit_cast<unsigned type>(v)); \
  }
V8_BASE_HASH_VALUE_SIGNED(char)
V8_BASE_HASH_VALUE_SIGNED(short)      // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(int)        // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(long)       // NOLINT(runtime/int)
V8_BASE_HASH_VALUE_SIGNED(long long)  // NOLINT(runtime/int)
#undef V8_BASE_HASH_VALUE_SIGNED

V8_INLINE size_t hash_value(float v) {
  // 0 and -0 both hash to zero.
  return v != 0.0f ? hash_value(bit_cast<uint32_t>(v)) : 0;
}

V8_INLINE size_t hash_value(double v) {
  // 0 and -0 both hash to zero.
  return v != 0.0 ? hash_value(bit_cast<uint64_t>(v)) : 0;
}

template <typename T, size_t N>
V8_INLINE size_t hash_value(const T (&v)[N]) {
  return hash_range(v, v + N);
}

template <typename T, size_t N>
V8_INLINE size_t hash_value(T (&v)[N]) {
  return hash_range(v, v + N);
}

template <typename T>
V8_INLINE size_t hash_value(T* const& v) {
  return hash_value(bit_cast<uintptr_t>(v));
}

template <typename T1, typename T2>
V8_INLINE size_t hash_value(std::pair<T1, T2> const& v) {
  return hash_combine(v.first, v.second);
}

template <typename T>
struct hash {
  V8_INLINE size_t operator()(T const& v) const { return hash_value(v); }
};

#define V8_BASE_HASH_SPECIALIZE(type)                 \
  template <>                                         \
  struct hash<type> {                                 \
    V8_INLINE size_t operator()(type const v) const { \
      return ::v8::base::hash_value(v);               \
    }                                                 \
  };
V8_BASE_HASH_SPECIALIZE(bool)
V8_BASE_HASH_SPECIALIZE(signed char)
V8_BASE_HASH_SPECIALIZE(unsigned char)
V8_BASE_HASH_SPECIALIZE(short)           // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(unsigned short)  // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(int)
V8_BASE_HASH_SPECIALIZE(unsigned int)
V8_BASE_HASH_SPECIALIZE(long)                // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(unsigned long)       // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(long long)           // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(unsigned long long)  // NOLINT(runtime/int)
V8_BASE_HASH_SPECIALIZE(float)
V8_BASE_HASH_SPECIALIZE(double)
#undef V8_BASE_HASH_SPECIALIZE

template <typename T>
struct hash<T*> {
  V8_INLINE size_t operator()(T* const v) const {
    return ::v8::base::hash_value(v);
  }
};

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

#define V8_BASE_BIT_SPECIALIZE_BIT_CAST(type, btype)       \
  template <>                                              \
  struct bit_equal_to<type> {                              \
    V8_INLINE bool operator()(type lhs, type rhs) const {  \
      return bit_cast<btype>(lhs) == bit_cast<btype>(rhs); \
    }                                                      \
  };                                                       \
  template <>                                              \
  struct bit_hash<type> {                                  \
    V8_INLINE size_t operator()(type v) const {            \
      hash<btype> h;                                       \
      return h(bit_cast<btype>(v));                        \
    }                                                      \
  };
V8_BASE_BIT_SPECIALIZE_BIT_CAST(float, uint32_t)
V8_BASE_BIT_SPECIALIZE_BIT_CAST(double, uint64_t)
#undef V8_BASE_BIT_SPECIALIZE_BIT_CAST

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_FUNCTIONAL_H_
