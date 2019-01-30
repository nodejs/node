// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_SPAN_H_
#define INSPECTOR_PROTOCOL_ENCODING_SPAN_H_

#include <cstddef>

namespace inspector_protocol {
// This template is similar to std::span, which will be included in C++20.  Like
// std::span it uses ptrdiff_t, which is signed (and thus a bit annoying
// sometimes when comparing with size_t), but other than this it's much simpler.
template <typename T>
class span {
 public:
  using index_type = std::ptrdiff_t;

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
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_SPAN_H_
