//
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
//
// -----------------------------------------------------------------------------
// File: bit_gen_ref.h
// -----------------------------------------------------------------------------
//
// This header defines a bit generator "reference" class, for use in interfaces
// that take both Abseil (e.g. `absl::BitGen`) and standard library (e.g.
// `std::mt19937`) bit generators.

#ifndef ABSL_RANDOM_BIT_GEN_REF_H_
#define ABSL_RANDOM_BIT_GEN_REF_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/fast_type_id.h"
#include "absl/meta/type_traits.h"
#include "absl/random/internal/fast_uniform_bits.h"
#include "absl/random/internal/traits.h"
#include "absl/random/mocking_access.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// -----------------------------------------------------------------------------
// absl::BitGenRef
// -----------------------------------------------------------------------------
//
// `absl::BitGenRef` is a type-erasing class that provides a generator-agnostic
// non-owning "reference" interface for use in place of any specific uniform
// random bit generator (URBG). This class may be used for both Abseil
// (e.g. `absl::BitGen`, `absl::InsecureBitGen`) and Standard library (e.g
// `std::mt19937`, `std::minstd_rand`) bit generators.
//
// Like other reference classes, `absl::BitGenRef` does not own the
// underlying bit generator, and the underlying instance must outlive the
// `absl::BitGenRef`.
//
// `absl::BitGenRef` is particularly useful when used with an
// `absl::MockingBitGen` to test specific paths in functions which use random
// values.
//
// Example:
//    void TakesBitGenRef(absl::BitGenRef gen) {
//      int x = absl::Uniform<int>(gen, 0, 1000);
//    }
//
class BitGenRef {
  template <template <class...> class Trait, class AlwaysVoid, class... Args>
  struct detector : std::false_type {};
  template <template <class...> class Trait, class... Args>
  struct detector<Trait, std::void_t<Trait<Args...>>, Args...>
      : std::true_type {};

  template <typename T>
  using has_conversion_operator_t =
      decltype(std::declval<T>().operator BitGenRef());

  template <typename T>
  using HasConversionOperator = detector<has_conversion_operator_t, void, T>;

  // True when BitGenRef binds directly to a URBG, rather than copying
  // (BitGenRef and derived types) or converting (types which provide
  // `operator BitGenRef()`).
  //
  // The conversion operator detection allows for ref-qualification, so that
  // types may prevent binding to temporaries via `operator BitGenRef() &`.
  template <typename URBG>
  static constexpr bool kBindsDirectly =
      !std::is_same_v<URBG, BitGenRef> && !std::is_base_of_v<BitGenRef, URBG> &&
      !HasConversionOperator<URBG&>::value &&
      random_internal::is_urbg<URBG>::value;

 public:
  BitGenRef(const BitGenRef&) = default;
  BitGenRef(BitGenRef&&) = default;
  BitGenRef& operator=(const BitGenRef&) = default;
  BitGenRef& operator=(BitGenRef&&) = default;

  template <typename URBGRef, typename URBG = absl::remove_cvref_t<URBGRef>,
            typename std::enable_if_t<
                kBindsDirectly<URBG> &&
                !std::is_const_v<std::remove_reference_t<URBGRef>>>* = nullptr>
  BitGenRef(URBGRef&& gen ABSL_ATTRIBUTE_LIFETIME_BOUND)  // NOLINT
      : t_erased_gen_ptr_(std::addressof(gen)),
        mock_call_(MockCallFn<URBG>()),
        generate_impl_fn_(ImplFn<URBG>) {}

  // Generating a variate mutates the generator, so a BitGenRef may not bind to
  // a const generator.
  template <typename URBGRef, typename URBG = absl::remove_cvref_t<URBGRef>,
            typename std::enable_if_t<
                kBindsDirectly<URBG> &&
                std::is_const_v<std::remove_reference_t<URBGRef>>>* = nullptr>
  BitGenRef(URBGRef&& gen) = delete;  // NOLINT

  using result_type = uint64_t;

  static constexpr result_type(min)() {
    return (std::numeric_limits<result_type>::min)();
  }

  static constexpr result_type(max)() {
    return (std::numeric_limits<result_type>::max)();
  }

  result_type operator()() { return generate_impl_fn_(t_erased_gen_ptr_); }

 private:
  using impl_fn = result_type (*)(void*);
  using mock_call_fn = bool (*)(void*, FastTypeIdType, void*, void*);

  template <typename URBG>
  static result_type ImplFn(void* ptr) {
    // Ensure that the return values from operator() fill the entire
    // range promised by result_type, min() and max().
    absl::random_internal::FastUniformBits<result_type> fast_uniform_bits;
    return fast_uniform_bits(*static_cast<URBG*>(ptr));
  }

  // Get a type-erased InvokeMock pointer.
  template <typename URBG>
  static bool MockCall(void* gen_ptr, FastTypeIdType key_id, void* args_tuple,
                       void* result) {
    return RandomMockingAccess::InvokeMock(static_cast<URBG*>(gen_ptr), key_id,
                                           args_tuple, result);
  }

  // Returns MockCall<URBG> if URBG supports mocking, otherwise nullptr.
  template <typename URBG>
  static constexpr mock_call_fn MockCallFn() {
    if constexpr (RandomMockingAccess::HasInvokeMock<URBG>::value) {
      return MockCall<URBG>;
    } else {
      return nullptr;
    }
  }

  bool InvokeMock(FastTypeIdType key_id, void* args_tuple, void* result) {
    if (mock_call_ == nullptr) return false;  // avoids an indirect call.
    return mock_call_(t_erased_gen_ptr_, key_id, args_tuple, result);
  }

  void* t_erased_gen_ptr_;
  mock_call_fn mock_call_;
  impl_fn generate_impl_fn_;

  friend class ::absl::RandomMockingAccess;  // for InvokeMock
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_RANDOM_BIT_GEN_REF_H_
