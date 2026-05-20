// Copyright 2019 The Abseil Authors.
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
// File: function_ref.h
// -----------------------------------------------------------------------------
//
// This header file defines the `absl::FunctionRef` type for holding a
// non-owning reference to an object of any invocable type. This function
// reference is typically most useful as a type-erased argument type for
// accepting function types that neither take ownership nor copy the type; using
// the reference type in this case avoids a copy and an allocation. Best
// practices of other non-owning reference-like objects (such as
// `absl::string_view`) apply here.
//
//  An `absl::FunctionRef` is similar in usage to a `std::function` but has the
//  following differences:
//
//  * It doesn't own the underlying object.
//  * It doesn't have a null or empty state.
//  * It never performs deep copies or allocations.
//  * It's much faster and cheaper to construct.
//  * It's trivially copyable and destructable.
//
// Generally, `absl::FunctionRef` should not be used as a return value, data
// member, or to initialize a `std::function`. Such usages will often lead to
// problematic lifetime issues. Once you convert something to an
// `absl::FunctionRef` you cannot make a deep copy later.
//
// This class is suitable for use wherever a "const std::function<>&"
// would be used without making a copy. ForEach functions and other versions of
// the visitor pattern are a good example of when this class should be used.
//
// This class is trivial to copy and should be passed by value.
#ifndef ABSL_FUNCTIONAL_FUNCTION_REF_H_
#define ABSL_FUNCTIONAL_FUNCTION_REF_H_

#include <cassert>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/functional/internal/function_ref.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// FunctionRef
//
// Dummy class declaration to allow the partial specialization based on function
// types below.
template <typename T>
class FunctionRef;

// FunctionRef
//
// An `absl::FunctionRef` is a lightweight wrapper to any invocable object with
// a compatible signature. Generally, an `absl::FunctionRef` should only be used
// as an argument type and should be preferred as an argument over a const
// reference to a `std::function`. `absl::FunctionRef` itself does not allocate,
// although the wrapped invocable may.
//
// Example:
//
//   // The following function takes a function callback by const reference
//   bool Visitor(const std::function<void(my_proto&,
//                                         absl::string_view)>& callback);
//
//   // Assuming that the function is not stored or otherwise copied, it can be
//   // replaced by an `absl::FunctionRef`:
//   bool Visitor(absl::FunctionRef<void(my_proto&, absl::string_view)>
//                  callback);
template <typename R, typename... Args>
class ABSL_ATTRIBUTE_VIEW FunctionRef<R(Args...)> {
 protected:
  // Used to disable constructors for objects that are not compatible with the
  // signature of this FunctionRef.
  template <typename F, typename... U>
  using EnableIfCompatible =
      std::enable_if_t<std::is_invocable_r<R, F, U..., Args...>::value>;

  // Internal constructor to supersede the copying constructor
  template <typename F>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(std::in_place_t, F&& f ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : invoker_(&absl::functional_internal::InvokeObject<F&, R, Args...>) {
    absl::functional_internal::AssertNonNull(f);
    ptr_.obj = &f;
  }

 public:
  // Constructs a FunctionRef from any invocable type.
  template <typename F,
            typename = EnableIfCompatible<std::enable_if_t<
                !std::is_same_v<FunctionRef, absl::remove_cvref_t<F>>, F&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(F&& f ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : FunctionRef(std::in_place, std::forward<F>(f)) {}

  // Overload for function pointers. This eliminates a level of indirection that
  // would happen if the above overload was used (it lets us store the pointer
  // instead of a pointer to a pointer).
  //
  // This overload is also used for references to functions, since references to
  // functions can decay to function pointers implicitly.
  template <typename F, typename = EnableIfCompatible<F*>,
            absl::functional_internal::EnableIf<std::is_function_v<F>> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(F* f ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : invoker_(&absl::functional_internal::InvokeFunction<F*, R, Args...>) {
    assert(f != nullptr);
    ptr_.fun = reinterpret_cast<decltype(ptr_.fun)>(f);
  }

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
  // Similar to the other overloads, but passes the address of a known callable
  // `F` at compile time. This allows calling arbitrary functions while avoiding
  // an indirection.
  // Needs C++20 as `nontype_t` needs C++20 for `auto` template parameters.
  template <auto F, typename = EnableIfCompatible<decltype(F)>>
  FunctionRef(nontype_t<F>) noexcept  // NOLINT(google-explicit-constructor)
      : invoker_(&absl::functional_internal::InvokeFunction<decltype(F), F, R,
                                                            Args...>) {}

  template <
      auto F, typename Obj,
      typename = EnableIfCompatible<decltype(F), std::remove_reference_t<Obj>&>,
      absl::functional_internal::EnableIf<!std::is_rvalue_reference_v<Obj&&>> =
          0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(nontype_t<F>, Obj&& obj ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : invoker_(&absl::functional_internal::InvokeObject<Obj&, decltype(F), F,
                                                          R, Args...>) {
    ptr_.obj = std::addressof(obj);
  }

  template <auto F, typename Obj,
            typename = EnableIfCompatible<decltype(F), Obj*>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(nontype_t<F>, Obj* obj ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : invoker_(&absl::functional_internal::InvokePtr<Obj, decltype(F), F, R,
                                                       Args...>) {
    ptr_.obj = obj;
  }
#endif

  using absl_internal_is_view = std::true_type;

  // Call the underlying object.
  R operator()(Args... args) const {
    return invoker_(ptr_, std::forward<Args>(args)...);
  }

 private:
  absl::functional_internal::VoidPtr ptr_;
  absl::functional_internal::Invoker<R, Args...> invoker_;
};

// Allow const qualified function signatures. Since FunctionRef requires
// constness anyway we can just make this a no-op.
template <typename R, typename... Args>
class ABSL_ATTRIBUTE_VIEW
    FunctionRef<R(Args...) const> : private FunctionRef<R(Args...)> {
  using Base = FunctionRef<R(Args...)>;

  template <typename F, typename... U>
  using EnableIfCompatible =
      typename Base::template EnableIfCompatible<F, U...>;

 public:
  template <
      typename F,
      typename = EnableIfCompatible<std::enable_if_t<
          !std::is_same_v<FunctionRef, absl::remove_cvref_t<F>>, const F&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(const F& f ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : Base(std::in_place_t(), f) {}

  template <typename F, typename = EnableIfCompatible<F*>,
            absl::functional_internal::EnableIf<std::is_function_v<F>> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(F* f ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept : Base(f) {}

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
  template <auto F, typename = EnableIfCompatible<decltype(F)>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(nontype_t<F> arg) noexcept : Base(arg) {}

  template <auto F, typename Obj,
            typename = EnableIfCompatible<decltype(F),
                                          const std::remove_reference_t<Obj>&>,
            absl::functional_internal::EnableIf<
                !std::is_rvalue_reference_v<Obj&&>> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(nontype_t<F> arg,
              Obj&& obj ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : Base(arg, std::forward<Obj>(obj)) {}

  template <auto F, typename Obj,
            typename = EnableIfCompatible<decltype(F), const Obj*>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(nontype_t<F> arg,
              const Obj* obj ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : Base(arg, obj) {}
#endif

  using absl_internal_is_view = std::true_type;

  using Base::operator();
};

template <class F>
FunctionRef(F*) -> FunctionRef<F>;

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
template <auto Func>
FunctionRef(nontype_t<Func>)
    -> FunctionRef<std::remove_pointer_t<decltype(Func)>>;

template <class M, class T, M T::* Func, class U>
FunctionRef(nontype_t<Func>, U&&)
    -> FunctionRef<std::enable_if_t<std::is_member_pointer_v<M T::*>, M>>;

template <class M, class T, M T::* Func, class U>
FunctionRef(nontype_t<Func>, U&&) -> FunctionRef<std::enable_if_t<
    std::is_object_v<M>, std::invoke_result_t<decltype(Func), U&>()>>;

template <class R, class T, class... Args, R (*Func)(T, Args...), class U>
FunctionRef(nontype_t<Func>, U&&) -> FunctionRef<R(Args...)>;
#endif

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_FUNCTIONAL_FUNCTION_REF_H_
