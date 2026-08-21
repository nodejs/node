// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRDTP_SPAN_H_
#define V8_CRDTP_SPAN_H_

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>

#include "export.h"

namespace v8_crdtp {

// crdtp::span is a std::span which always holds const elements.
template <typename T>
using span = std::span<const T>;

template <size_t N>
constexpr span<char> MakeSpan(const char (&str)[N]) {
  return span<char>(str, N - 1);
}

template <size_t N>
constexpr span<uint8_t> SpanFrom(const char (&str)[N]) {
  return span<uint8_t>(reinterpret_cast<const uint8_t*>(str), N - 1);
}

constexpr inline span<uint8_t> SpanFrom(const char* str) {
  return str ? span<uint8_t>(reinterpret_cast<const uint8_t*>(str), strlen(str))
             : span<uint8_t>();
}

inline span<uint8_t> SpanFrom(const std::string& v) {
  return span<uint8_t>(reinterpret_cast<const uint8_t*>(v.data()), v.size());
}

// This SpanFrom routine works for std::vector<uint8_t> and
// std::vector<uint16_t>, but also for base::span<const uint8_t> in Chromium.
template <typename C,
          typename = std::enable_if_t<
              std::is_unsigned<typename C::value_type>{} &&
              std::is_member_function_pointer<decltype(&C::size)>{}>>
inline span<typename C::value_type> SpanFrom(const C& v) {
  return span<typename C::value_type>(v.data(), v.size());
}

// Less than / equality comparison functions for sorting / searching for byte
// spans.
bool SpanLessThan(span<uint8_t> x, span<uint8_t> y) noexcept;
bool SpanEquals(span<uint8_t> x, span<uint8_t> y) noexcept;

// Less than / equality comparison functions for sorting / searching for byte
// spans.
bool SpanLessThan(span<char> x, span<char> y) noexcept;
bool SpanEquals(span<char> x, span<char> y) noexcept;

struct SpanLt {
  bool operator()(span<uint8_t> l, span<uint8_t> r) const {
    return SpanLessThan(l, r);
  }
};
}  // namespace v8_crdtp

#endif  // V8_CRDTP_SPAN_H_
