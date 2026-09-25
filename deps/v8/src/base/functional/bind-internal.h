// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Originally in Chromium as "base/functional/bind_internal.h"
// Selectively extracted and adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_FUNCTIONAL_BIND_INTERNAL_H_
#define V8_BASE_FUNCTIONAL_BIND_INTERNAL_H_

#include <type_traits>
#include <utility>

// See Chromium's docs/callback.md for user documentation.
//
// Concepts:
//  Functor -- A movable type representing something that should be called.
//             All function pointers and `Callback<>` are functors even if the
//             invocation syntax differs.
//  RunType -- A function type (as opposed to function _pointer_ type) for
//             a `Callback<>::Run()`.  Usually just a convenience typedef.
//  (Bound)Args -- A set of types that stores the arguments.
//
// Types:
//  `FunctorTraits<>` -- Type traits used to determine the correct RunType and
//                       invocation manner for a Functor.  This is where
//                       function signature adapters are applied.

namespace v8::base::internal {

// True if `T` is completely defined.
template <typename T>
concept IsComplete = requires { sizeof(T); } ||
                     // Function types must be included explicitly since you
                     // cannot apply `sizeof()` to a function type.
                     std::is_function_v<std::remove_cvref_t<T>>;

// Packs a list of types to hold them in a single type.
template <typename... Types>
struct TypeList {};

// True when `Functor` has a non-overloaded `operator()()`, e.g.:
//   struct S1 {
//     int operator()(int);
//   };
//   static_assert(HasNonOverloadedCallOp<S1>);
//
//   int i = 0;
//   auto f = [i] {};
//   static_assert(HasNonOverloadedCallOp<decltype(f)>);
//
//   struct S2 {
//     int operator()(int);
//     std::string operator()(std::string);
//   };
//   static_assert(!HasNonOverloadedCallOp<S2>);
//
//   static_assert(!HasNonOverloadedCallOp<void(*)()>);
//
//   struct S3 {};
//   static_assert(!HasNonOverloadedCallOp<S3>);
// ```
template <typename Functor>
concept HasNonOverloadedCallOp = requires { &Functor::operator(); };

// True when `Functor` has an overloaded `operator()()` that can be invoked with
// the provided `BoundArgs`.
//
// Do not decay `Functor` before testing this, lest it give an incorrect result
// for overloads with different ref-qualifiers.
template <typename Functor, typename... BoundArgs>
concept HasOverloadedCallOp = requires {
  // The functor must be invocable with the bound args.
  requires requires(Functor&& f, BoundArgs&&... args) {
    std::forward<Functor>(f)(std::forward<BoundArgs>(args)...);
  };
  // Now exclude invocables that are not cases of overloaded `operator()()`s:
  // * `operator()()` exists, but isn't overloaded
  requires !HasNonOverloadedCallOp<std::decay_t<Functor>>;
  // * Function pointer (doesn't have `operator()()`)
  requires !std::is_pointer_v<std::decay_t<Functor>>;
};

// Implements `ExtractArgs` and `ExtractReturnType`.
template <typename Signature>
struct ExtractArgsImpl;

template <typename R, typename... Args>
struct ExtractArgsImpl<R(Args...)> {
  using ReturnType = R;
  using ArgsList = TypeList<Args...>;
};

// A type-level function that extracts function arguments into a `TypeList`;
// e.g. `ExtractArgs<R(A, B, C)>` -> `TypeList<A, B, C>`.
template <typename Signature>
using ExtractArgs = typename ExtractArgsImpl<Signature>::ArgsList;

// A type-level function that extracts the return type of a function.
// e.g. `ExtractReturnType<R(A, B, C)>` -> `R`.
template <typename Signature>
using ExtractReturnType = typename ExtractArgsImpl<Signature>::ReturnType;

template <typename Callable,
          typename Signature = decltype(&Callable::operator())>
struct ExtractCallableRunTypeImpl;

#define BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS(quals)     \
  template <typename Callable, typename R, typename... Args>          \
  struct ExtractCallableRunTypeImpl<Callable,                         \
                                    R (Callable::*)(Args...) quals> { \
    using Type = R(Args...);                                          \
  }

BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS();
BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS(const);
BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS(noexcept);
BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS(const noexcept);

#undef BIND_INTERNAL_EXTRACT_CALLABLE_RUN_TYPE_WITH_QUALS

// Evaluated to the RunType of the given callable type; e.g.
// `ExtractCallableRunType<decltype([](int, char*) { return 0.1; })>` ->
//     `double(int, char*)`.
template <typename Callable>
using ExtractCallableRunType =
    typename ExtractCallableRunTypeImpl<Callable>::Type;

// `FunctorTraits<>`
//
// See description at top of file. This must be declared here so it can be
// referenced in `DecayedFunctorTraits`.
template <typename Functor, typename... BoundArgs>
struct FunctorTraits;

// Provides functor traits for pre-decayed functor types.
template <typename Functor, typename... BoundArgs>
struct DecayedFunctorTraits;

// Callable types.
// This specialization handles lambdas (captureless and capturing) and functors
// with a call operator. Capturing lambdas and stateful functors are explicitly
// disallowed by `BindHelper<>::Bind()`; e.g.:
// ```
//   // Captureless lambda: Allowed
//   [] { return 42; };
//
//   // Capturing lambda: Disallowed
//   int x;
//   [x] { return x; };
//
//   // Empty class with `operator()()`: Allowed
//   struct Foo {
//     void operator()() const {}
//     // No non-`static` member variables and no virtual functions.
//   };
// ```
template <typename Functor, typename... BoundArgs>
  requires HasNonOverloadedCallOp<Functor>
struct DecayedFunctorTraits<Functor, BoundArgs...> {
  using RunType = ExtractCallableRunType<Functor>;
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = false;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = std::is_empty_v<Functor>;

  template <typename RunFunctor, typename... RunArgs>
  static ExtractReturnType<RunType> Invoke(RunFunctor&& functor,
                                           RunArgs&&... args) {
    return std::forward<RunFunctor>(functor)(std::forward<RunArgs>(args)...);
  }
};

// Functions.
template <typename R, typename... Args, typename... BoundArgs>
struct DecayedFunctorTraits<R (*)(Args...), BoundArgs...> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Function, typename... RunArgs>
  static R Invoke(Function&& function, RunArgs&&... args) {
    return std::forward<Function>(function)(std::forward<RunArgs>(args)...);
  }
};

template <typename R, typename... Args, typename... BoundArgs>
struct DecayedFunctorTraits<R (*)(Args...) noexcept, BoundArgs...>
    : DecayedFunctorTraits<R (*)(Args...), BoundArgs...> {};

// Methods.
template <typename R, typename Receiver, typename... Args,
          typename... BoundArgs>
struct DecayedFunctorTraits<R (Receiver::*)(Args...), BoundArgs...> {
  using RunType = R(Receiver*, Args...);
  static constexpr bool is_method = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Method, typename ReceiverPtr, typename... RunArgs>
  static R Invoke(Method method, ReceiverPtr&& receiver_ptr,
                  RunArgs&&... args) {
    return ((*receiver_ptr).*method)(std::forward<RunArgs>(args)...);
  }
};

template <typename R, typename Receiver, typename... Args,
          typename... BoundArgs>
struct DecayedFunctorTraits<R (Receiver::*)(Args...) const, BoundArgs...>
    : DecayedFunctorTraits<R (Receiver::*)(Args...), BoundArgs...> {
  using RunType = R(const Receiver*, Args...);
};

#define BIND_INTERNAL_DECAYED_FUNCTOR_TRAITS_WITH_CONST_AND_QUALS(constqual, \
                                                                  quals)     \
  template <typename R, typename Receiver, typename... Args,                 \
            typename... BoundArgs>                                           \
  struct DecayedFunctorTraits<R (Receiver::*)(Args...) constqual quals,      \
                              BoundArgs...>                                  \
      : DecayedFunctorTraits<R (Receiver::*)(Args...) constqual,             \
                             BoundArgs...> {}

BIND_INTERNAL_DECAYED_FUNCTOR_TRAITS_WITH_CONST_AND_QUALS(, noexcept);
BIND_INTERNAL_DECAYED_FUNCTOR_TRAITS_WITH_CONST_AND_QUALS(const, noexcept);

#undef BIND_INTERNAL_DECAYED_FUNCTOR_TRAITS_WITH_CONST_AND_QUALS

// For most functors, the traits should not depend on how the functor is passed,
// so decay the functor.
template <typename Functor, typename... BoundArgs>
// This requirement avoids "implicit instantiation of undefined template" errors
// when the underlying `DecayedFunctorTraits<>` cannot be instantiated. Instead,
// this template will also not be instantiated, and the caller can detect and
// handle that.
  requires IsComplete<DecayedFunctorTraits<std::decay_t<Functor>, BoundArgs...>>
struct FunctorTraits<Functor, BoundArgs...>
    : DecayedFunctorTraits<std::decay_t<Functor>, BoundArgs...> {};

// For `overloaded operator()()`s, it's possible the ref qualifiers of the
// functor matter, so be careful to use the undecayed type.
template <typename Functor, typename... BoundArgs>
  requires HasOverloadedCallOp<Functor, BoundArgs...>
struct FunctorTraits<Functor, BoundArgs...> {
  // For an overloaded operator()(), it is not possible to resolve the
  // actual declared type. Since it is invocable with the bound args, make up a
  // signature based on their types.
  using RunType = decltype(std::declval<Functor>()(
      std::declval<BoundArgs>()...))(std::decay_t<BoundArgs>...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = false;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = std::is_empty_v<std::decay_t<Functor>>;

  template <typename RunFunctor, typename... RunArgs>
  static ExtractReturnType<RunType> Invoke(RunFunctor&& functor,
                                           RunArgs&&... args) {
    return std::forward<RunFunctor>(functor)(std::forward<RunArgs>(args)...);
  }
};

}  // namespace v8::base::internal

#endif  // V8_BASE_FUNCTIONAL_BIND_INTERNAL_H_
