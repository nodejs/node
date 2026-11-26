// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_HASH_INTERNAL_CANONICAL_HASH_H_
#define ABSL_HASH_INTERNAL_CANONICAL_HASH_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "absl/base/casts.h"
#include "absl/base/config.h"
#include "absl/hash/hash.h"
#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

// Unsigned integer type of size `N` if `N` is 1, 2, 4, or 8, `void` otherwise.
template <size_t N>
using UintOfSizeT = std::conditional_t<
    N == 1, uint8_t,
    std::conditional_t<
        N == 2, uint16_t,
        std::conditional_t<N == 4, uint32_t,
                           std::conditional_t<N == 8, uint64_t, void>>>>;

// True if `T` can be copied as raw bytes without running a copy constructor or
// destructor. Unlike `std::is_trivially_copyable`, this does not require a
// trivial `operator=`, so it is true for `std::pair<int, int>` while still
// excluding Objective-C ARC pointers.
template <typename T>
using IsTriviallyCopyConstructibleAndDestructible =
    std::bool_constant<std::is_trivially_copy_constructible_v<T> &&
                       std::is_trivially_destructible_v<T>>;

// Returns a canonical `absl::Hash` for `T`. Types with the exact same hash
// implementation (such as `is_uniquely_represented` types of the same size)
// return the same hasher type, which is useful for type erasure.
template <typename T>
constexpr auto CanonicalHasher() {
  if constexpr (is_uniquely_represented<T>::value &&
                IsTriviallyCopyConstructibleAndDestructible<T>::value &&
                FitsIn64BitsPowerOf2<T>::value) {
    return Hash<UintOfSizeT<sizeof(T)>>();
  } else {
    return Hash<T>();
  }
}

// Type-erased key argument passed by value in a single register on common ABIs.
struct EncodedHashKey {
  // Returns true if `key` should be passed by value inside `EncodedHashKey`,
  // avoiding spilling it to the stack at the call site.
  template <class T>
  static constexpr bool ShouldPassByValue() {
    return FitsIn64BitsPowerOf2<T>::value && sizeof(T) <= sizeof(bytes) &&
           alignof(T) <= alignof(EncodedHashKey) &&
           IsTriviallyCopyConstructibleAndDestructible<T>::value;
  }

  template <class T>
  static EncodedHashKey FromValue(const T& v) {
    static_assert(ShouldPassByValue<T>());
    EncodedHashKey res{};
    std::memcpy(res.bytes, &v, sizeof(T));
    return res;
  }

  static EncodedHashKey FromPtr(const void* ptr) { return FromValue(ptr); }

  template <class T>
  const T& ToValue() const {
    static_assert(ShouldPassByValue<T>());
    return *reinterpret_cast<const T*>(bytes);
  }

  template <class T>
  const T* ToPtr() const {
    return absl::bit_cast<const T*>(bytes);
  }

  // Byte array (rather than `const void*`) avoids UB and MSan `noundef` errors
  // when a small key contains padding or uninitialized bytes.
  alignas(void*) unsigned char bytes[sizeof(void*)];
};

using HashForKeyFn = size_t (*)(EncodedHashKey hasher, size_t seed);

// Type erased function to apply `Hasher` (the result of `CanonicalHasher<T>()`)
// to the key encoded by `CanonicalHashKey::data()`.
template <class Hasher>
size_t TypeErasedApplyAbslHashToKeyFn(EncodedHashKey key, size_t seed) {
  using T = typename Hasher::KeyType;
  if constexpr (EncodedHashKey::ShouldPassByValue<T>()) {
    return HashWithSeed().hash(Hasher(), key.ToValue<T>(), seed);
  } else {
    return HashWithSeed().hash(Hasher(), *key.ToPtr<T>(), seed);
  }
}

// No arguments hash function for a specific key that uses
// `CanonicalHasher<Key>`, so that `get_hash_fn()` is shared across all key
// types with the same canonical hasher.
template <class Key>
struct CanonicalHashKey {
  using NonConstK = absl::remove_cvref_t<Key>;
  using Canonical = decltype(CanonicalHasher<NonConstK>());
  using CanonicalKey = typename Canonical::KeyType;

  explicit CanonicalHashKey(const NonConstK& k) : key(k) {}

  // `data()` and `get_hash_fn()` are passed to type erased functions.
  EncodedHashKey data() const {
    // Check `CanonicalKey` to match `TypeErasedApplyAbslHashToKeyFn`.
    if constexpr (EncodedHashKey::ShouldPassByValue<CanonicalKey>()) {
      static_assert(sizeof(NonConstK) == sizeof(CanonicalKey));
      return EncodedHashKey::FromValue(key);
    } else {
      return EncodedHashKey::FromPtr(&key);
    }
  }
  static constexpr HashForKeyFn get_hash_fn() {
    return &TypeErasedApplyAbslHashToKeyFn<Canonical>;
  }

  // Small keys are stored by value so that inlined caller keys stay in
  // registers instead of becoming address-taken via `data()`.
  type_traits_internal::AddConstRefIfExpensiveToCopy<Key> key;
};

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_HASH_INTERNAL_CANONICAL_HASH_H_
