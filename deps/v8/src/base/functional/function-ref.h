// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Originally in Chromium as "base/functional/function_ref.h"
// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_FUNCTIONAL_FUNCTION_REF_H_
#define V8_BASE_FUNCTIONAL_FUNCTION_REF_H_

#include <concepts>
#include <type_traits>
#include <utility>

#include "absl/functional/function_ref.h"
#include "src/base/compiler-specific.h"
#include "src/base/functional/bind-internal.h"
#include "src/base/types/is-instantiation.h"

namespace v8::base {

template <typename Signature>
class FunctionRef;

// A non-owning reference to any invocable object (e.g. function pointer, method
// pointer, functor, lambda, et cetera) suitable for use as a type-erased
// argument to ForEach-style functions or other visitor patterns that:
//
// - do not need to copy or take ownership of the argument
// - synchronously call the invocable that was passed as an argument
//
// `base::FunctionRef` makes no heap allocations: it is trivially copyable and
// should be passed by value.
//
// `base::FunctionRef` has no null/empty state: a `base::FunctionRef` is always
// valid to invoke.
//
// The usual lifetime precautions for other non-owning references types (e.g.
// `std::string_view`, `base::span`) also apply to `base::FunctionRef`.
// `base::FunctionRef` should typically be used as an argument; returning a
// `base::FunctionRef` or storing a `base::FunctionRef` as a field is dangerous
// and likely to result in lifetime bugs.
//
// `base::RepeatingCallback` and `base::BindRepeating()` is another common way
// to represent type-erased invocable objects. In contrast, it requires a heap
// allocation and is not trivially copyable. It should be used when there are
// ownership requirements (e.g. partial application of arguments to a function
// stored for asynchronous execution).
//
// Note: `base::FunctionRef` is similar to `absl::FunctionRef<R(Args...)>`, but
// with stricter conversions between function types. Return type conversions are
// allowed (e.g. `int` -> `bool`, `Derived*` -> `Base*`); other than that,
// function parameter types must match exactly, and return values may not be
// silently discarded, e.g. `absl::FunctionRef` allows the following:
//
//   // Silently discards `42`.
//   [] (absl::FunctionRef<void()> r) {
//     r();
//   }([] { return 42; });
//
// But with `base::FunctionRef`:
//
//   // Does not compile!
//   [] (base::FunctionRef<void()> r) {
//     r();
//   }([] { return 42; });
template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
  template <typename Functor,
            typename RunType = internal::FunctorTraits<Functor>::RunType>
  static constexpr bool kCompatibleFunctor =
      std::convertible_to<internal::ExtractReturnType<RunType>, R> &&
      std::same_as<internal::ExtractArgs<RunType>, internal::TypeList<Args...>>;

 public:
  // `LIFETIME_BOUND` is important; since `FunctionRef` retains
  // only a reference to `functor`, `functor` must outlive `this`.
  template <typename Functor>
    requires kCompatibleFunctor<Functor> &&
             // Prevent this constructor from participating in overload
             // resolution if the callable is itself an instantiation of the
             // `FunctionRef` template.
             //
             // If the callable is a `FunctionRef` with exactly the same
             // signature as us, then the copy constructor will be used instead,
             // so this has no effect. (Note that if the constructor argument
             // were `Functor&&`, this exclusion would be necessary to force the
             // choice of the copy constructor over this one for non-const ref
             // args; see https://stackoverflow.com/q/57909923.)
             //
             // If the callable is a `FunctionRef` with some other signature
             // then we choose not to support binding to it at all. Conceivably
             // we could teach our trampoline to deal with this, but this may be
             // the sign of an object lifetime bug, and again it's not clear
             // that this isn't just a mistake on the part of the user.
             (!is_instantiation<std::decay_t<Functor>, FunctionRef>) &&
             // For the same reason as the second case above, prevent
             // construction from `absl::FunctionRef`.
             (!is_instantiation<std::decay_t<Functor>, absl::FunctionRef>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(const Functor& functor V8_LIFETIME_BOUND)
      : wrapped_func_ref_(functor) {}

  // Constructs a reference to the given function pointer. This constructor
  // serves to exclude this case from lifetime analysis, since the underlying
  // code pointed to by a function pointer is safe to invoke even if the
  // lifetime of the pointer provided doesn't outlive us, e.g.:
  //   `const FunctionRef<void(int)> ref = +[](int i) { ... };`
  // Without this constructor, the above code would warn about dangling refs.
  // TODO(pkasting): Also support ptr-to-member-functions; this requires changes
  // in `absl::FunctionRef` or else rewriting this class to not use that one.
  template <typename Func>
    requires kCompatibleFunctor<Func*>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(Func* func) : wrapped_func_ref_(func) {}

  // Null FunctionRefs are not allowed.
  FunctionRef() = delete;

  FunctionRef(const FunctionRef&) V8_NOEXCEPT = default;
  // Reduce the likelihood of lifetime bugs by disallowing assignment.
  FunctionRef& operator=(const FunctionRef&) = delete;

  R operator()(Args... args) const {
    return wrapped_func_ref_(std::forward<Args>(args)...);
  }

 private:
  absl::FunctionRef<R(Args...)> wrapped_func_ref_;
};

}  // namespace v8::base

#endif  // V8_BASE_FUNCTIONAL_FUNCTION_REF_H_
