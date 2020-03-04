// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRDTP_SPAN_H_
#define V8_CRDTP_SPAN_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace v8_crdtp {
// =============================================================================
// span - sequence of bytes
// =============================================================================

// This template is similar to std::span, which will be included in C++20.
template <typename T>
class span {
 public:
  using index_type = size_t;

  span() : data_(nullptr), size_(0) {}
  span(const T* data, index_type size) : data_(data), size_(size) {}

  const T* data() const { return data_; }

  const T* begin() const { return data_; }
  const T* end() const { return data_ + size_; }

  const T& operator[](index_type idx) const { return data_[idx]; }

  span<T> subspan(index_type offset, index_type count) const {
    return span(data_ + offset, count);
  }

  span<T> subspan(index_type offset) const {
    return span(data_ + offset, size_ - offset);
  }

  bool empty() const { return size_ == 0; }

  index_type size() const { return size_; }
  index_type size_bytes() const { return size_ * sizeof(T); }

 private:
  const T* data_;
  index_type size_;
};

template <typename T>
span<T> SpanFrom(const std::vector<T>& v) {
  return span<T>(v.data(), v.size());
}

template <size_t N>
span<uint8_t> SpanFrom(const char (&str)[N]) {
  return span<uint8_t>(reinterpret_cast<const uint8_t*>(str), N - 1);
}

inline span<uint8_t> SpanFrom(const char* str) {
  return str ? span<uint8_t>(reinterpret_cast<const uint8_t*>(str), strlen(str))
             : span<uint8_t>();
}

inline span<uint8_t> SpanFrom(const std::string& v) {
  return span<uint8_t>(reinterpret_cast<const uint8_t*>(v.data()), v.size());
}

// Less than / equality comparison functions for sorting / searching for byte
// spans. These are similar to absl::string_view's < and == operators.
inline bool SpanLessThan(span<uint8_t> x, span<uint8_t> y) noexcept {
  auto min_size = std::min(x.size(), y.size());
  const int r = min_size == 0 ? 0 : memcmp(x.data(), y.data(), min_size);
  return (r < 0) || (r == 0 && x.size() < y.size());
}

inline bool SpanEquals(span<uint8_t> x, span<uint8_t> y) noexcept {
  auto len = x.size();
  if (len != y.size())
    return false;
  return x.data() == y.data() || len == 0 ||
         std::memcmp(x.data(), y.data(), len) == 0;
}
}  // namespace v8_crdtp

#endif  // V8_CRDTP_SPAN_H_
