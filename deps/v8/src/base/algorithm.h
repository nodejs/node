// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ALGORITHM_H_
#define V8_BASE_ALGORITHM_H_

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "src/base/memcopy.h"

namespace v8::base {
namespace internal {

template <class InputIt, class OutputIt>
constexpr bool can_use_memcpy_v =
    std::contiguous_iterator<InputIt> && std::contiguous_iterator<OutputIt> &&
    std::is_same_v<typename std::iterator_traits<InputIt>::value_type,
                   typename std::iterator_traits<OutputIt>::value_type> &&
    std::is_trivially_copyable_v<
        typename std::iterator_traits<InputIt>::value_type>;

}  // namespace internal

// Version of `std::copy()` that delegates to `base::MemCopy()` when possible.
// Unlike `std::copy()` we don't allow any overlaps as our implementation
// assumes `std::execution::par_unseq` execution policy under the hood.
template <class InputIt, class OutputIt>
OutputIt Copy(InputIt first, InputIt last, OutputIt d_first)
  requires(internal::can_use_memcpy_v<InputIt, OutputIt>)
{
  using T = typename std::iterator_traits<InputIt>::value_type;
  const auto count = std::distance(first, last);
  const T* src_ptr = std::to_address(first);
  T* dest_ptr = std::to_address(d_first);
  base::MemCopy(dest_ptr, src_ptr, count * sizeof(T));
  return d_first + count;
}

template <class InputIt, class OutputIt>
constexpr OutputIt Copy(InputIt first, InputIt last, OutputIt d_first)
  requires(!internal::can_use_memcpy_v<InputIt, OutputIt>)
{
  return std::copy(first, last, d_first);
}

}  // namespace v8::base

#endif  // V8_BASE_ALGORITHM_H_
