// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#ifndef V8_BASE_NUMERICS_BASIC_OPS_IMPL_H_
#define V8_BASE_NUMERICS_BASIC_OPS_IMPL_H_

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace v8::base::internal {

// The correct type to perform math operations on given values of type `T`. This
// may be a larger type than `T` to avoid promotion to `int` which involves sign
// conversion!
template <class T>
  requires(std::is_integral_v<T>)
using MathType = std::conditional_t<
    sizeof(T) >= sizeof(int), T,
    std::conditional_t<std::is_signed_v<T>, int, unsigned int>>;

// Reverses the byte order of the integer.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr T SwapBytes(T value) {
  // MSVC intrinsics are not constexpr, so we provide our own constexpr
  // implementation. We provide it unconditionally so we can test it on all
  // platforms for correctness.
  if (std::is_constant_evaluated()) {
    if constexpr (sizeof(T) == 1u) {
      return value;
    } else if constexpr (sizeof(T) == 2u) {
      MathType<T> a = (MathType<T>(value) >> 0) & MathType<T>{0xff};
      MathType<T> b = (MathType<T>(value) >> 8) & MathType<T>{0xff};
      return static_cast<T>((a << 8) | (b << 0));
    } else if constexpr (sizeof(T) == 4u) {
      T a = (value >> 0) & T{0xff};
      T b = (value >> 8) & T{0xff};
      T c = (value >> 16) & T{0xff};
      T d = (value >> 24) & T{0xff};
      return (a << 24) | (b << 16) | (c << 8) | (d << 0);
    } else {
      static_assert(sizeof(T) == 8u);
      T a = (value >> 0) & T{0xff};
      T b = (value >> 8) & T{0xff};
      T c = (value >> 16) & T{0xff};
      T d = (value >> 24) & T{0xff};
      T e = (value >> 32) & T{0xff};
      T f = (value >> 40) & T{0xff};
      T g = (value >> 48) & T{0xff};
      T h = (value >> 56) & T{0xff};
      return (a << 56) | (b << 48) | (c << 40) | (d << 32) |  //
             (e << 24) | (f << 16) | (g << 8) | (h << 0);
    }
  }

#if _MSC_VER
  if constexpr (sizeof(T) == 1u) {
    return value;
    // NOLINTNEXTLINE(runtime/int)
  } else if constexpr (sizeof(T) == sizeof(unsigned short)) {
    using U = unsigned short;  // NOLINT(runtime/int)
    return _byteswap_ushort(U{value});
    // NOLINTNEXTLINE(runtime/int)
  } else if constexpr (sizeof(T) == sizeof(unsigned long)) {
    using U = unsigned long;  // NOLINT(runtime/int)
    return _byteswap_ulong(U{value});
  } else {
    static_assert(sizeof(T) == 8u);
    return _byteswap_uint64(value);
  }
#else
  if constexpr (sizeof(T) == 1u) {
    return value;
  } else if constexpr (sizeof(T) == 2u) {
    return __builtin_bswap16(uint16_t{value});
  } else if constexpr (sizeof(T) == 4u) {
    return __builtin_bswap32(value);
  } else {
    static_assert(sizeof(T) == 8u);
    return __builtin_bswap64(value);
  }
#endif
}

// Signed values are byte-swapped as unsigned values.
template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr T SwapBytes(T value) {
  return static_cast<T>(SwapBytes(static_cast<std::make_unsigned_t<T>>(value)));
}

// Converts from a byte array to an integer.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  T val;
  if (std::is_constant_evaluated()) {
    val = T{0};
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      // SAFETY: `i < sizeof(T)` (the number of bytes in T), so `(8 * i)` is
      // less than the number of bits in T.
      val |= MathType<T>(bytes[i]) << (8u * i);
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(&val, bytes.data(), sizeof(T));
  }
  return val;
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return static_cast<T>(FromLittleEndian<std::make_unsigned_t<T>>(bytes));
}

// Converts to a byte array from an integer.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  auto bytes = std::array<uint8_t, sizeof(T)>();
  if (std::is_constant_evaluated()) {
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      const auto last_byte = static_cast<uint8_t>(val & 0xff);
      // The low bytes go to the front of the array in little endian.
      bytes[i] = last_byte;
      // If `val` is one byte, this shift would be UB. But it's also not needed
      // since the loop will not run again.
      if constexpr (sizeof(T) > 1u) {
        val >>= 8u;
      }
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(bytes.data(), &val, sizeof(T));
  }
  return bytes;
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  return ToLittleEndian(static_cast<std::make_unsigned_t<T>>(val));
}
}  // namespace v8::base::internal

#endif  // V8_BASE_NUMERICS_BASIC_OPS_IMPL_H_
