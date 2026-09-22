// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_UNION_H_
#define V8_OBJECTS_UNION_H_

#include "src/common/globals.h"

namespace v8::internal {

class Smi;
class Hole;

// Union<Ts...> represents a union of multiple V8 types.
//
// Unions are required to be non-nested (i.e. no unions of unions), and to
// have each type only once. The UnionOf<Ts...> helper can be used to flatten
// nested unions and remove duplicates.
//
// Inheritance from Unions is forbidden because it messes with `is_subtype`
// checking.
template <typename... Ts>
class Union;

// is_union<T> is a type trait that returns true if T is a union.
template <typename... Ts>
struct is_union : public std::false_type {};
template <typename... Ts>
struct is_union<Union<Ts...>> : public std::true_type {};
template <typename... Ts>
static constexpr bool is_union_v = is_union<Ts...>::value;

namespace detail {

template <typename... Ts>
struct TypeList {
  static constexpr size_t kSize = sizeof...(Ts);
};

template <typename T>
struct ToUnion;
template <>
struct ToUnion<TypeList<>> {
  // A Union over nothing should be invalid.
  using type = void;
};
template <typename T>
struct ToUnion<TypeList<T>> {
  // A Union over a single type should just be that type.
  using type = T;
};
template <typename... Ts>
struct ToUnion<TypeList<Ts...>> {
  using type = Union<Ts...>;
};

#if V8_HAS_BUILTIN_DEDUP_PACK
template <typename T>
struct PopFrontUnion;

template <typename T1, typename T2>
struct PopFrontUnion<Union<T1, T2>> {
  // Popping down to one type should return just that type.
  using type = T2;
};

template <typename Head, typename... Tail>
struct PopFrontUnion<Union<Head, Tail...>> {
  using type = Union<Tail...>;
};
#else
template <typename Accumulator, typename TWithout, typename... InputTypes>
struct UnionWithoutHelper;

// Base case: No input types, return the accumulated types.
template <typename... OutputTs, typename TWithout>
struct UnionWithoutHelper<TypeList<OutputTs...>, TWithout> {
  using type = TypeList<OutputTs...>;
};

// Recursive case: Found Head matching TWithout, drop it and accumulate the
// remainder.
template <typename... OutputTs, typename TWithout, typename... Ts>
struct UnionWithoutHelper<TypeList<OutputTs...>, TWithout, TWithout, Ts...> {
  using type = TypeList<OutputTs..., Ts...>;
};

// Recursive case: Non-matching input, accumulate and continue.
template <typename... OutputTs, typename TWithout, typename Head,
          typename... Ts>
struct UnionWithoutHelper<TypeList<OutputTs...>, TWithout, Head, Ts...> {
  using type = typename UnionWithoutHelper<TypeList<OutputTs..., Head>,
                                           TWithout, Ts...>::type;
};
#endif

}  // namespace detail

template <typename... Ts>
class Union final : public AllStatic {
 public:
  static_assert(((!is_union_v<Ts>) && ...),
                "Cannot have a union of unions -- use the UnionOf<T...> helper "
                "to flatten nested unions");
#if V8_HAS_BUILTIN_DEDUP_PACK
  static_assert(
      sizeof...(Ts) == detail::TypeList<__builtin_dedup_pack<Ts...>...>::kSize,
      "Unions should have each type only once -- use the UnionOf<T...> "
      "helper to deduplicate unions");

  // If we can cheaply deduplicate, we can move the type to remove to the front
  // and then pop it.
  template <typename U>
  using Without = typename detail::PopFrontUnion<
      Union<__builtin_dedup_pack<U, Ts...>...>>::type;
#else
  template <typename U>
  using Without = typename detail::ToUnion<typename detail::UnionWithoutHelper<
      detail::TypeList<>, U, Ts...>::type>::type;
#endif  // V8_HAS_BUILTIN_DEDUP_PACK
};

namespace detail {

#if !V8_HAS_BUILTIN_DEDUP_PACK
template <typename Result, typename... Ts>
struct Deduplicate;

template <typename... ResultTs>
struct Deduplicate<TypeList<ResultTs...>> {
  using type = TypeList<ResultTs...>;
};

template <typename... ResultTs, typename Head, typename... Tail>
struct Deduplicate<TypeList<ResultTs...>, Head, Tail...> {
  using type = std::conditional_t<
      base::has_type_v<Head, ResultTs...>,
      typename Deduplicate<TypeList<ResultTs...>, Tail...>::type,
      typename Deduplicate<TypeList<ResultTs..., Head>, Tail...>::type>;
};
#endif  // !V8_HAS_BUILTIN_DEDUP_PACK

// Use methods and overloading instead of template specializations here to avoid
// the compiler needing to materialize class specializations.
template <typename T>
TypeList<T> AsTypeList(T*);
template <typename... Us>
TypeList<Us...> AsTypeList(Union<Us...>*);

// Define an operator+ over TypeLists instead of concatenating two lists via
// a helper struct to allow defining the concatenation of N TypeLists as a
// fold expression, and avoiding deep recursive template specialization.
template <typename... As, typename... Bs>
TypeList<As..., Bs...> operator+(TypeList<As...>, TypeList<Bs...>);

template <typename FlatUnion>
struct DedupAndFinalize;

template <typename... FlatTs>
struct DedupAndFinalize<TypeList<FlatTs...>> {
  static constexpr bool kHasSmi = (std::is_same_v<Smi, FlatTs> || ...);
#if V8_HAS_BUILTIN_DEDUP_PACK
  using deduped =
      std::conditional_t<kHasSmi,
                         TypeList<__builtin_dedup_pack<Smi, FlatTs...>...>,
                         TypeList<__builtin_dedup_pack<FlatTs...>...>>;
#else
  using deduped =
      std::conditional_t<kHasSmi,
                         typename Deduplicate<TypeList<>, Smi, FlatTs...>::type,
                         typename Deduplicate<TypeList<>, FlatTs...>::type>;
#endif  // V8_HAS_BUILTIN_DEDUP_PACK
  using type = typename ToUnion<deduped>::type;
};

template <typename... InputTypes>
struct FlattenUnionHelper {
  using flat = decltype((AsTypeList(static_cast<InputTypes*>(nullptr)) + ...));
  using type = typename DedupAndFinalize<flat>::type;
};

}  // namespace detail

// UnionOf<Ts...> is a helper that returns a union of multiple V8 types,
// flattening any nested unions and removing duplicate types.
template <typename... Ts>
using UnionOf = typename detail::FlattenUnionHelper<Ts...>::type;

// Unions of unions are flattened.
static_assert(std::is_same_v<Union<Smi, HeapObject>,
                             UnionOf<UnionOf<Smi>, UnionOf<HeapObject>>>);
// Unions with duplicates are deduplicated.
static_assert(std::is_same_v<Union<Smi, HeapObject>,
                             UnionOf<HeapObject, Smi, Smi, HeapObject>>);
// Unions with Smis are normalized to have the Smi be the first element.
static_assert(std::is_same_v<Union<Smi, HeapObject>, UnionOf<HeapObject, Smi>>);

// Union::Without matches expectations.
static_assert(std::is_same_v<Union<Smi, HeapObject>::Without<Smi>, HeapObject>);
static_assert(std::is_same_v<JSAny::Without<Smi>, JSAnyNotSmi>);
static_assert(
    std::is_same_v<JSAny::Without<Smi>::Without<HeapNumber>, JSAnyNotNumber>);

// Union::Without that doesn't have a match is a no-op
static_assert(std::is_same_v<Union<Smi, HeapObject>::Without<HeapNumber>,
                             Union<Smi, HeapObject>>);

}  // namespace v8::internal

#endif  // V8_OBJECTS_UNION_H_
