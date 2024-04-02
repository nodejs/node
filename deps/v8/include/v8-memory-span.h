// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_MEMORY_SPAN_H_
#define INCLUDE_V8_MEMORY_SPAN_H_

#include <stddef.h>

#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

/**
 * Points to an unowned continous buffer holding a known number of elements.
 *
 * This is similar to std::span (under consideration for C++20), but does not
 * require advanced C++ support. In the (far) future, this may be replaced with
 * or aliased to std::span.
 *
 * To facilitate future migration, this class exposes a subset of the interface
 * implemented by std::span.
 */
template <typename T>
class V8_EXPORT MemorySpan {
 public:
  /** The default constructor creates an empty span. */
  constexpr MemorySpan() = default;

  constexpr MemorySpan(T* data, size_t size) : data_(data), size_(size) {}

  /** Returns a pointer to the beginning of the buffer. */
  constexpr T* data() const { return data_; }
  /** Returns the number of elements that the buffer holds. */
  constexpr size_t size() const { return size_; }

 private:
  T* data_ = nullptr;
  size_t size_ = 0;
};

}  // namespace v8
#endif  // INCLUDE_V8_MEMORY_SPAN_H_
