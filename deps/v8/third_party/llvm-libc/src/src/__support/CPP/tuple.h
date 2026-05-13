//===-- tuple utility -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_TUPLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_TUPLE_H

#include "src/__support/CPP/type_traits/decay.h"
#include "src/__support/CPP/utility/integer_sequence.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <typename... Ts> struct tuple;
template <> struct tuple<> {};

template <typename Head, typename... Tail>
struct tuple<Head, Tail...> : tuple<Tail...> {
  Head head;

  LIBC_INLINE constexpr tuple() = default;

  template <typename OHead, typename... OTail>
  LIBC_INLINE constexpr tuple &operator=(const tuple<OHead, OTail...> &other) {
    head = other.get_head();
    this->get_tail() = other.get_tail();
    return *this;
  }

  LIBC_INLINE constexpr tuple(const Head &h, const Tail &...t)
      : tuple<Tail...>(t...), head(h) {}

  LIBC_INLINE constexpr Head &get_head() { return head; }
  LIBC_INLINE constexpr const Head &get_head() const { return head; }

  LIBC_INLINE constexpr tuple<Tail...> &get_tail() { return *this; }
  LIBC_INLINE constexpr const tuple<Tail...> &get_tail() const { return *this; }
};

template <typename... Ts> LIBC_INLINE constexpr auto make_tuple(Ts &&...args) {
  return tuple<cpp::decay_t<Ts>...>(static_cast<Ts &&>(args)...);
}
template <typename... Ts> LIBC_INLINE constexpr auto tie(Ts &...args) {
  return tuple<Ts &...>(args...);
}

template <size_t Idx, typename Head, typename... Tail>
LIBC_INLINE constexpr auto &get(tuple<Head, Tail...> &t) {
  if constexpr (Idx == 0)
    return t.get_head();
  else
    return get<Idx - 1>(t.get_tail());
}
template <size_t Idx, typename Head, typename... Tail>
LIBC_INLINE constexpr const auto &get(const tuple<Head, Tail...> &t) {
  if constexpr (Idx == 0)
    return t.get_head();
  else
    return get<Idx - 1>(t.get_tail());
}
template <size_t Idx, typename Head, typename... Tail>
LIBC_INLINE constexpr auto &&get(tuple<Head, Tail...> &&t) {
  if constexpr (Idx == 0)
    return static_cast<Head &&>(t.get_head());
  else
    return get<Idx - 1>(static_cast<tuple<Tail...> &&>(t.get_tail()));
}
template <size_t Idx, typename Head, typename... Tail>
LIBC_INLINE constexpr const auto &&get(const tuple<Head, Tail...> &&t) {
  if constexpr (Idx == 0)
    return static_cast<const Head &&>(t.get_head());
  else
    return get<Idx - 1>(static_cast<const tuple<Tail...> &&>(t.get_tail()));
}

template <typename T> struct tuple_size;
template <typename... Ts> struct tuple_size<tuple<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

template <size_t Idx, typename T> struct tuple_element;
template <size_t Idx, typename Head, typename... Tail>
struct tuple_element<Idx, tuple<Head, Tail...>>
    : tuple_element<Idx - 1, tuple<Tail...>> {};
template <typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
  using type = cpp::remove_cv_t<cpp::remove_reference_t<Head>>;
};

namespace internal {
template <typename... As, typename... Bs, size_t... Idx, size_t... J>
LIBC_INLINE constexpr auto
tuple_cat(const tuple<As...> &a, const tuple<Bs...> &b,
          cpp::index_sequence<Idx...>, cpp::index_sequence<J...>) {
  return tuple<As..., Bs...>(get<Idx>(a)..., get<J>(b)...);
}

template <typename First, typename Second, typename... Rest>
LIBC_INLINE constexpr auto tuple_cat(const First &f, const Second &s,
                                     const Rest &...rest) {
  auto concat =
      tuple_cat(f, s, cpp::make_index_sequence<tuple_size<First>::value>{},
                cpp::make_index_sequence<tuple_size<Second>::value>{});
  if constexpr (sizeof...(Rest))
    return tuple_cat(concat, rest...);
  else
    return concat;
}
} // namespace internal

template <typename... Tuples>
LIBC_INLINE constexpr auto tuple_cat(const Tuples &...tuples) {
  static_assert(sizeof...(Tuples) > 0, "need at least one element");
  if constexpr (sizeof...(Tuples) == 1)
    return (tuples, ...);
  else
    return internal::tuple_cat(tuples...);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

// Standard namespace definitions required for structured binding support.
namespace std {

template <class T> struct tuple_size;
template <size_t Idx, class T> struct tuple_element;

template <typename... Ts>
struct tuple_size<LIBC_NAMESPACE::cpp::tuple<Ts...>>
    : LIBC_NAMESPACE::cpp::tuple_size<LIBC_NAMESPACE::cpp::tuple<Ts...>> {};

template <size_t Idx, typename... Ts>
struct tuple_element<Idx, LIBC_NAMESPACE::cpp::tuple<Ts...>>
    : LIBC_NAMESPACE::cpp::tuple_element<Idx,
                                         LIBC_NAMESPACE::cpp::tuple<Ts...>> {};

} // namespace std

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_TUPLE_H
