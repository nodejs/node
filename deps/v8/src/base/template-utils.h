// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_TEMPLATE_UTILS_H
#define V8_BASE_TEMPLATE_UTILS_H

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
  constexpr static std::array<typename std::result_of<Function(size_t)>::type,
                              sizeof...(Indexes) + 1>
  make_array(Function f) {
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
constexpr std::array<typename std::result_of<Function(size_t)>::type, Size>
make_array(Function f) {
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

// implicit_cast<A>(x) triggers an implicit cast from {x} to type {A}. This is
// useful in situations where static_cast<A>(x) would do too much.
template <class A>
A implicit_cast(A x) {
  return x;
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

template <typename T>
struct has_output_operator {
  // This template is only instantiable if U provides operator<< with ostream.
  // Its return type is uint8_t.
  template <typename U>
  static auto __check_operator(U u)
      -> decltype(*(std::ostream*)nullptr << *u, uint8_t{0});
  // This is a fallback implementation, returning uint16_t. If the template
  // above is instantiable, is has precedence over this varargs function.
  static uint16_t __check_operator(...);

  using ptr_t = typename std::add_pointer<T>::type;
  static constexpr bool value = sizeof(__check_operator(ptr_t{nullptr})) == 1;
};

namespace detail {

template <typename Func, typename T, typename... Ts>
struct fold_helper {
  static_assert(sizeof...(Ts) == 0, "this is the base case");
  using result_t = typename std::remove_reference<T>::type;
  static constexpr T&& fold(Func func, T&& first) {
    return std::forward<T>(first);
  }
};

template <typename Func, typename T1, typename T2, typename... Ts>
struct fold_helper<Func, T1, T2, Ts...> {
  using folded_t = typename std::result_of<Func(T1, T2)>::type;
  using next_fold_helper = fold_helper<Func, folded_t&&, Ts...>;
  using result_t = typename next_fold_helper::result_t;
  static constexpr result_t fold(Func func, T1&& first, T2&& second,
                                 Ts&&... more) {
    return next_fold_helper::fold(
        func, func(std::forward<T1>(first), std::forward<T2>(second)),
        std::forward<Ts>(more)...);
  }
};

}  // namespace detail

// Fold all arguments from left to right with a given function.
template <typename Func, typename... Ts>
constexpr auto fold(Func func, Ts&&... more) ->
    typename detail::fold_helper<Func, Ts...>::result_t {
  return detail::fold_helper<Func, Ts...>::fold(func,
                                                std::forward<Ts>(more)...);
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_TEMPLATE_UTILS_H
