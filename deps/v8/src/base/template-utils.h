// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_TEMPLATE_UTILS_H_
#define V8_BASE_TEMPLATE_UTILS_H_

#include <array>
#include <type_traits>
#include <utility>

namespace v8 {
namespace base {

namespace detail {

template <typename Function, std::size_t... Indexes>
constexpr inline auto make_array_helper(Function f,
                                        std::index_sequence<Indexes...>)
    -> std::array<decltype(f(0)), sizeof...(Indexes)> {
  return {{f(Indexes)...}};
}

}  // namespace detail

// base::make_array: Create an array of fixed length, initialized by a function.
// The content of the array is created by calling the function with 0 .. Size-1.
// Example usage to create the array {0, 2, 4}:
//   std::array<int, 3> arr = base::make_array<3>(
//       [](std::size_t i) { return static_cast<int>(2 * i); });
// The resulting array will be constexpr if the passed function is constexpr.
template <std::size_t Size, class Function>
constexpr auto make_array(Function f) {
  return detail::make_array_helper(f, std::make_index_sequence<Size>{});
}

// Helper to determine how to pass values: Pass scalars and arrays by value,
// others by const reference (even if it was a non-const ref before; this is
// disallowed by the style guide anyway).
// The default is to also remove array extends (int[5] -> int*), but this can be
// disabled by setting {remove_array_extend} to false.
template <typename T, bool remove_array_extend = true>
struct pass_value_or_ref {
  using noref_t = typename std::remove_reference<T>::type;
  using decay_t = typename std::conditional<
      std::is_array<noref_t>::value && !remove_array_extend, noref_t,
      typename std::decay<noref_t>::type>::type;
  using type = typename std::conditional<std::is_scalar<decay_t>::value ||
                                             std::is_array<decay_t>::value,
                                         decay_t, const decay_t&>::type;
};

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct has_output_operator : std::false_type {};
template <typename T>
struct has_output_operator<T, decltype(void(std::declval<std::ostream&>()
                                            << std::declval<T>()))>
    : std::true_type {};

// Fold all arguments from left to right with a given function.
template <typename Func, typename T>
constexpr auto fold(Func func, T&& t) {
  return std::forward<T>(t);
}

template <typename Func, typename T1, typename T2, typename... Ts>
constexpr auto fold(Func func, T1&& first, T2&& second, Ts&&... more) {
  auto&& folded = func(std::forward<T1>(first), std::forward<T2>(second));
  return fold(std::move(func), std::forward<decltype(folded)>(folded),
              std::forward<Ts>(more)...);
}

// {is_same<Ts...>::value} is true if all Ts are the same, false otherwise.
template <typename... Ts>
struct is_same : public std::false_type {};
template <>
struct is_same<> : public std::true_type {};
template <typename T>
struct is_same<T> : public std::true_type {};
template <typename T, typename... Ts>
struct is_same<T, T, Ts...> : public is_same<T, Ts...> {};

// Returns true, iff all values (implicitly converted to bool) are trueish.
template <typename... Args>
constexpr bool all(Args... rest) {
  return fold(std::logical_and<>{}, true, rest...);
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_TEMPLATE_UTILS_H_
