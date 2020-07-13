// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_TYPE_TRAITS_H_
#define INCLUDE_CPPGC_TYPE_TRAITS_H_

#include <type_traits>

namespace cppgc {

class Visitor;

namespace internal {

// Pre-C++17 custom implementation of std::void_t.
template <typename... Ts>
struct make_void {
  typedef void type;
};
template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

// Not supposed to be specialized by the user.
template <typename T>
struct IsWeak : std::false_type {};

template <typename T, template <typename... V> class U>
struct IsSubclassOfTemplate {
 private:
  template <typename... W>
  static std::true_type SubclassCheck(U<W...>*);
  static std::false_type SubclassCheck(...);

 public:
  static constexpr bool value =
      decltype(SubclassCheck(std::declval<T*>()))::value;
};

// IsTraceMethodConst is used to verify that all Trace methods are marked as
// const. It is equivalent to IsTraceable but for a non-const object.
template <typename T, typename = void>
struct IsTraceMethodConst : std::false_type {};

template <typename T>
struct IsTraceMethodConst<T, void_t<decltype(std::declval<const T>().Trace(
                                 std::declval<Visitor*>()))>> : std::true_type {
};

template <typename T, typename = void>
struct IsTraceable : std::false_type {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T>
struct IsTraceable<
    T, void_t<decltype(std::declval<T>().Trace(std::declval<Visitor*>()))>>
    : std::true_type {
  // All Trace methods should be marked as const. If an object of type
  // 'T' is traceable then any object of type 'const T' should also
  // be traceable.
  static_assert(IsTraceMethodConst<T>(),
                "Trace methods should be marked as const.");
};

template <typename T>
constexpr bool IsTraceableV = IsTraceable<T>::value;

template <typename T, typename = void>
struct IsGarbageCollectedMixinType : std::false_type {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T>
struct IsGarbageCollectedMixinType<
    T,
    void_t<typename std::remove_const_t<T>::IsGarbageCollectedMixinTypeMarker>>
    : std::true_type {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T, typename = void>
struct IsGarbageCollectedType : IsGarbageCollectedMixinType<T> {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T>
struct IsGarbageCollectedType<
    T, void_t<typename std::remove_const_t<T>::IsGarbageCollectedTypeMarker>>
    : std::true_type {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T>
constexpr bool IsGarbageCollectedTypeV =
    internal::IsGarbageCollectedType<T>::value;

template <typename T>
constexpr bool IsGarbageCollectedMixinTypeV =
    internal::IsGarbageCollectedMixinType<T>::value;

}  // namespace internal

template <typename T>
constexpr bool IsWeakV = internal::IsWeak<T>::value;

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_TYPE_TRAITS_H_
