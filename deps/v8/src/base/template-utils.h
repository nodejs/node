// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>

namespace v8 {
namespace base {

namespace detail {

// make_array_helper statically iteratively creates the index list 0 .. Size-1.
// A specialization for the base case (first index is 0) finally constructs the
// array.
// TODO(clemensh): Use std::index_sequence once we have C++14 support.
template <class Function, std::size_t... Indexes>
struct make_array_helper;

template <class Function, std::size_t... Indexes>
struct make_array_helper<Function, 0, Indexes...> {
  constexpr static auto make_array(Function f)
      -> std::array<decltype(f(std::size_t{0})), sizeof...(Indexes) + 1> {
    return {{f(0), f(Indexes)...}};
  }
};

template <class Function, std::size_t FirstIndex, std::size_t... Indexes>
struct make_array_helper<Function, FirstIndex, Indexes...>
    : make_array_helper<Function, FirstIndex - 1, FirstIndex, Indexes...> {};

}  // namespace detail

// base::make_array: Create an array of fixed length, initialized by a function.
// The content of the array is created by calling the function with 0 .. Size-1.
// Example usage to create the array {0, 2, 4}:
//   std::array<int, 3> arr = base::make_array<3>(
//       [](std::size_t i) { return static_cast<int>(2 * i); });
// The resulting array will be constexpr if the passed function is constexpr.
template <std::size_t Size, class Function>
constexpr auto make_array(Function f)
    -> std::array<decltype(f(std::size_t{0})), Size> {
  static_assert(Size > 0, "Can only create non-empty arrays");
  return detail::make_array_helper<Function, Size - 1>::make_array(f);
}

// base::make_unique<T>: Construct an object of type T and wrap it in a
// std::unique_ptr.
// Replacement for C++14's std::make_unique.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace base
}  // namespace v8
