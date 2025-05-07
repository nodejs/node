// Copyright 2018 The Abseil Authors.
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

#ifndef ABSL_CONTAINER_INTERNAL_RAW_HASH_MAP_H_
#define ABSL_CONTAINER_INTERNAL_RAW_HASH_MAP_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/throw_delegate.h"
#include "absl/container/internal/common_policy_traits.h"
#include "absl/container/internal/container_memory.h"
#include "absl/container/internal/raw_hash_set.h"  // IWYU pragma: export
#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

template <class Policy, class Hash, class Eq, class Alloc>
class raw_hash_map : public raw_hash_set<Policy, Hash, Eq, Alloc> {
  // P is Policy. It's passed as a template argument to support maps that have
  // incomplete types as values, as in unordered_map<K, IncompleteType>.
  // MappedReference<> may be a non-reference type.
  template <class P>
  using MappedReference = decltype(P::value(
      std::addressof(std::declval<typename raw_hash_map::reference>())));

  // MappedConstReference<> may be a non-reference type.
  template <class P>
  using MappedConstReference = decltype(P::value(
      std::addressof(std::declval<typename raw_hash_map::const_reference>())));

  template <class K>
  using key_arg =
      typename KeyArg<IsTransparent<Eq>::value && IsTransparent<Hash>::value>::
          template type<K, typename Policy::key_type>;

  // NOTE: The mess here is to shorten the code for the (very repetitive)
  // function overloads, and to allow the lifetime-bound overloads to dispatch
  // to the non-lifetime-bound overloads, to ensure there is a single source of
  // truth for each overload set.
  //
  // Enabled if an assignment from the given type would require the
  // source object to remain alive for the life of the element.
  //
  // TODO(b/402804213): Remove these traits and simplify the overloads whenever
  // we have a better mechanism available to handle lifetime analysis.
  template <class K, bool Value, typename = void>
  using LifetimeBoundK = HasValue<
      Value, std::conditional_t<policy_trait_element_is_owner<Policy>::value,
                                std::false_type,
                                type_traits_internal::IsLifetimeBoundAssignment<
                                    typename Policy::key_type, K>>>;
  template <class V, bool Value, typename = void>
  using LifetimeBoundV =
      HasValue<Value, type_traits_internal::IsLifetimeBoundAssignment<
                          typename Policy::mapped_type, V>>;
  template <class K, bool KValue, class V, bool VValue, typename... Dummy>
  using LifetimeBoundKV =
      absl::conjunction<LifetimeBoundK<K, KValue, absl::void_t<Dummy...>>,
                        LifetimeBoundV<V, VValue>>;

 public:
  using key_type = typename Policy::key_type;
  using mapped_type = typename Policy::mapped_type;

  static_assert(!std::is_reference<key_type>::value, "");

  // TODO(b/187807849): Evaluate whether to support reference mapped_type and
  // remove this assertion if/when it is supported.
  static_assert(!std::is_reference<mapped_type>::value, "");

  using iterator = typename raw_hash_map::raw_hash_set::iterator;
  using const_iterator = typename raw_hash_map::raw_hash_set::const_iterator;

  raw_hash_map() {}
  using raw_hash_map::raw_hash_set::raw_hash_set;

  // The last two template parameters ensure that both arguments are rvalues
  // (lvalue arguments are handled by the overloads below). This is necessary
  // for supporting bitfield arguments.
  //
  //   union { int n : 1; };
  //   flat_hash_map<int, int> m;
  //   m.insert_or_assign(n, n);
  //
  // TODO(b/402804213): Remove these macros whenever we have a better mechanism
  // available to handle lifetime analysis.
#define ABSL_INTERNAL_X(Func, Callee, KQual, VQual, KValue, VValue, Tail, ...) \
  template <                                                                   \
      typename K = key_type, class V = mapped_type,                            \
      ABSL_INTERNAL_IF_##KValue##_NOR_##VValue(                                \
          int = (EnableIf<LifetimeBoundKV<K, KValue, V, VValue,                \
                                          IfRRef<int KQual>::AddPtr<K>,        \
                                          IfRRef<int VQual>::AddPtr<V>>>()),   \
          ABSL_INTERNAL_SINGLE_ARG(                                            \
              int &...,                                                        \
              decltype(EnableIf<LifetimeBoundKV<K, KValue, V, VValue>>()) =    \
                  0))>                                                         \
  decltype(auto) Func(                                                         \
      __VA_ARGS__ key_arg<K> KQual k ABSL_INTERNAL_IF_##KValue(                \
          ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this)),                          \
      V VQual v ABSL_INTERNAL_IF_##VValue(ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY( \
          this))) ABSL_ATTRIBUTE_LIFETIME_BOUND {                              \
    return ABSL_INTERNAL_IF_##KValue##_OR_##VValue(                            \
        (this->template Func<K, V, 0>), Callee)(                               \
        std::forward<decltype(k)>(k), std::forward<decltype(v)>(v)) Tail;      \
  }                                                                            \
  static_assert(true, "This is to force a semicolon.")

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  false, false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  false, true, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  true, false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  true, true, ABSL_INTERNAL_SINGLE_ARG());

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, false,
                  false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, false,
                  true, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, true,
                  false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, true,
                  true, ABSL_INTERNAL_SINGLE_ARG());

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, false,
                  false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, false,
                  true, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, true,
                  false, ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, true,
                  true, ABSL_INTERNAL_SINGLE_ARG());

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, false, false,
                  ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, false, true,
                  ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, true, false,
                  ABSL_INTERNAL_SINGLE_ARG());
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, true, true,
                  ABSL_INTERNAL_SINGLE_ARG());

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  false, false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  false, true, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  true, false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, const &,
                  true, true, .first, const_iterator ABSL_INTERNAL_COMMA);

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, false,
                  false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, false,
                  true, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, true,
                  false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, const &, &&, true,
                  true, .first, const_iterator ABSL_INTERNAL_COMMA);

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, false,
                  false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, false,
                  true, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, true,
                  false, .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, const &, true,
                  true, .first, const_iterator ABSL_INTERNAL_COMMA);

  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, false, false,
                  .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, false, true,
                  .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, true, false,
                  .first, const_iterator ABSL_INTERNAL_COMMA);
  ABSL_INTERNAL_X(insert_or_assign, insert_or_assign_impl, &&, &&, true, true,
                  .first, const_iterator ABSL_INTERNAL_COMMA);
#undef ABSL_INTERNAL_X

  // All `try_emplace()` overloads make the same guarantees regarding rvalue
  // arguments as `std::unordered_map::try_emplace()`, namely that these
  // functions will not move from rvalue arguments if insertions do not happen.
  template <class K = key_type, int = EnableIf<LifetimeBoundK<K, false, K *>>(),
            class... Args,
            typename std::enable_if<
                !std::is_convertible<K, const_iterator>::value, int>::type = 0>
  std::pair<iterator, bool> try_emplace(key_arg<K> &&k, Args &&...args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_impl(std::forward<K>(k), std::forward<Args>(args)...);
  }

  template <class K = key_type, class... Args,
            EnableIf<LifetimeBoundK<K, true, K *>> = 0,
            typename std::enable_if<
                !std::is_convertible<K, const_iterator>::value, int>::type = 0>
  std::pair<iterator, bool> try_emplace(
      key_arg<K> &&k ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this),
      Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template try_emplace<K, 0>(std::forward<K>(k),
                                            std::forward<Args>(args)...);
  }

  template <class K = key_type, int = EnableIf<LifetimeBoundK<K, false>>(),
            class... Args,
            typename std::enable_if<
                !std::is_convertible<K, const_iterator>::value, int>::type = 0>
  std::pair<iterator, bool> try_emplace(const key_arg<K> &k, Args &&...args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_impl(k, std::forward<Args>(args)...);
  }
  template <class K = key_type, class... Args,
            EnableIf<LifetimeBoundK<K, true>> = 0,
            typename std::enable_if<
                !std::is_convertible<K, const_iterator>::value, int>::type = 0>
  std::pair<iterator, bool> try_emplace(
      const key_arg<K> &k ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this),
      Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template try_emplace<K, 0>(k, std::forward<Args>(args)...);
  }

  template <class K = key_type, int = EnableIf<LifetimeBoundK<K, false, K *>>(),
            class... Args>
  iterator try_emplace(const_iterator, key_arg<K> &&k,
                       Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace(std::forward<K>(k), std::forward<Args>(args)...).first;
  }
  template <class K = key_type, class... Args,
            EnableIf<LifetimeBoundK<K, true, K *>> = 0>
  iterator try_emplace(const_iterator hint,
                       key_arg<K> &&k ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this),
                       Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template try_emplace<K, 0>(hint, std::forward<K>(k),
                                            std::forward<Args>(args)...);
  }

  template <class K = key_type, int = EnableIf<LifetimeBoundK<K, false>>(),
            class... Args>
  iterator try_emplace(const_iterator, const key_arg<K> &k,
                       Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace(k, std::forward<Args>(args)...).first;
  }
  template <class K = key_type, class... Args,
            EnableIf<LifetimeBoundK<K, true>> = 0>
  iterator try_emplace(const_iterator hint,
                       const key_arg<K> &k
                           ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this),
                       Args &&...args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template try_emplace<K, 0>(hint, std::forward<K>(k),
                                            std::forward<Args>(args)...);
  }

  template <class K = key_type, class P = Policy>
  MappedReference<P> at(const key_arg<K>& key) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto it = this->find(key);
    if (it == this->end()) {
      base_internal::ThrowStdOutOfRange(
          "absl::container_internal::raw_hash_map<>::at");
    }
    return Policy::value(&*it);
  }

  template <class K = key_type, class P = Policy>
  MappedConstReference<P> at(const key_arg<K>& key) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto it = this->find(key);
    if (it == this->end()) {
      base_internal::ThrowStdOutOfRange(
          "absl::container_internal::raw_hash_map<>::at");
    }
    return Policy::value(&*it);
  }

  template <class K = key_type, class P = Policy,
            int = EnableIf<LifetimeBoundK<K, false, K *>>()>
  MappedReference<P> operator[](key_arg<K> &&key)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    // It is safe to use unchecked_deref here because try_emplace
    // will always return an iterator pointing to a valid item in the table,
    // since it inserts if nothing is found for the given key.
    return Policy::value(
        &this->unchecked_deref(try_emplace(std::forward<K>(key)).first));
  }
  template <class K = key_type, class P = Policy, int &...,
            EnableIf<LifetimeBoundK<K, true, K *>> = 0>
  MappedReference<P> operator[](
      key_arg<K> &&key ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template operator[]<K, P, 0>(std::forward<K>(key));
  }

  template <class K = key_type, class P = Policy,
            int = EnableIf<LifetimeBoundK<K, false>>()>
  MappedReference<P> operator[](const key_arg<K> &key)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    // It is safe to use unchecked_deref here because try_emplace
    // will always return an iterator pointing to a valid item in the table,
    // since it inserts if nothing is found for the given key.
    return Policy::value(&this->unchecked_deref(try_emplace(key).first));
  }
  template <class K = key_type, class P = Policy, int &...,
            EnableIf<LifetimeBoundK<K, true>> = 0>
  MappedReference<P> operator[](
      const key_arg<K> &key ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template operator[]<K, P, 0>(key);
  }

 private:
  template <class K, class V>
  std::pair<iterator, bool> insert_or_assign_impl(K&& k, V&& v)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto res = this->find_or_prepare_insert(k);
    if (res.second) {
      this->emplace_at(res.first, std::forward<K>(k), std::forward<V>(v));
    } else {
      Policy::value(&*res.first) = std::forward<V>(v);
    }
    return res;
  }

  template <class K = key_type, class... Args>
  std::pair<iterator, bool> try_emplace_impl(K&& k, Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto res = this->find_or_prepare_insert(k);
    if (res.second) {
      this->emplace_at(res.first, std::piecewise_construct,
                       std::forward_as_tuple(std::forward<K>(k)),
                       std::forward_as_tuple(std::forward<Args>(args)...));
    }
    return res;
  }
};

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CONTAINER_INTERNAL_RAW_HASH_MAP_H_
