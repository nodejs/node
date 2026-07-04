// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// The file provides the IsStrictlyBaseOfAndConvertibleToSTLContainer type
// trait metafunction to assist in working with the _GLIBCXX_DEBUG debug
// wrappers of STL containers.
//
// DO NOT INCLUDE THIS FILE DIRECTLY. Use this file by including
// absl/strings/str_split.h.
//
// IWYU pragma: private, include "absl/strings/str_split.h"

#ifndef ABSL_STRINGS_INTERNAL_STL_TYPE_TRAITS_H_
#define ABSL_STRINGS_INTERNAL_STL_TYPE_TRAITS_H_

#include <stddef.h>

#include <array>
#include <bitset>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

template <typename To, typename From>
using IsCastableToDerivedSTLContainer =
    std::enable_if_t<!std::is_same_v<From, To>, std::is_convertible<From, To>>;

template <typename C>
std::false_type CastableToDerivedSTLContainer(C*);

template <typename C, typename V, size_t N>
IsCastableToDerivedSTLContainer<C, std::array<typename C::value_type, N>>
CastableToDerivedSTLContainer(std::array<V, N>*);

template <typename C, size_t N>
IsCastableToDerivedSTLContainer<C, std::bitset<N>>
CastableToDerivedSTLContainer(std::bitset<N>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::deque<typename C::value_type, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::deque<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::forward_list<typename C::value_type, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::forward_list<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::list<typename C::value_type, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::list<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::map<typename C::key_type, typename C::mapped_type,
                typename C::key_compare, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::map<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::multimap<typename C::key_type, typename C::mapped_type,
                     typename C::key_compare, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::multimap<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::set<typename C::value_type, typename C::key_compare,
                typename C::allocator_type>>
CastableToDerivedSTLContainer(std::set<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::multiset<typename C::value_type, typename C::key_compare,
                     typename C::allocator_type>>
CastableToDerivedSTLContainer(std::multiset<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::unordered_map<typename C::key_type, typename C::mapped_type,
                          typename C::hasher, typename C::key_equal,
                          typename C::allocator_type>>
CastableToDerivedSTLContainer(std::unordered_map<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::unordered_multimap<typename C::key_type, typename C::mapped_type,
                               typename C::hasher, typename C::key_equal,
                               typename C::allocator_type>>
CastableToDerivedSTLContainer(std::unordered_multimap<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::unordered_set<typename C::key_type, typename C::hasher,
                          typename C::key_equal, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::unordered_set<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C,
    std::unordered_multiset<typename C::key_type, typename C::hasher,
                            typename C::key_equal, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::unordered_multiset<U...>*);

template <typename C, typename... U>
IsCastableToDerivedSTLContainer<
    C, std::vector<typename C::value_type, typename C::allocator_type>>
CastableToDerivedSTLContainer(std::vector<U...>*);

template <typename C>
struct IsStrictlyBaseOfAndConvertibleToSTLContainer
    : decltype(strings_internal::CastableToDerivedSTLContainer<
               absl::remove_cvref_t<C>>(
          std::declval<absl::remove_cvref_t<C>*>())) {};

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
#endif  // ABSL_STRINGS_INTERNAL_STL_TYPE_TRAITS_H_
