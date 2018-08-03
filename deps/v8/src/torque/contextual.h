// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CONTEXTUAL_H_
#define V8_TORQUE_CONTEXTUAL_H_

#include <type_traits>

namespace v8 {
namespace internal {
namespace torque {

// Contextual variables store a value in one or more stack-allocated scopes and
// allow global access to the most recent active scope on the current
// call-stack.
template <class Derived, class VarType>
class ContextualVariable {
 public:
  // A {Scope} contains a new object of type {T} and gives
  // ContextualVariable::Get() access to it. Upon destruction, the contextual
  // variable is restored to the state before the {Scope} was created. Scopes
  // have to follow a stack discipline:  A {Scope} has to be destructed before
  // any older scope is destructed.
  class Scope {
   public:
    explicit Scope(VarType x = VarType())
        : current_(std::move(x)), previous_(top_) {
      top_ = &current_;
    }
    ~Scope() {
      // Ensure stack discipline.
      DCHECK_EQ(&current_, top_);
      top_ = previous_;
    }

   private:
    VarType current_;
    VarType* previous_;

    static_assert(std::is_base_of<ContextualVariable, Derived>::value,
                  "Curiously Recurring Template Pattern");
  };

  // Access the most recent active {Scope}. There has to be an active {Scope}
  // for this contextual variable.
  static VarType& Get() {
    DCHECK_NOT_NULL(top_);
    return *top_;
  }

 private:
  static VarType* top_;
};

template <class Derived, class VarType>
VarType* ContextualVariable<Derived, VarType>::top_ = nullptr;

// Usage: DECLARE_CONTEXTUAL_VARIABLE(VarName, VarType)
#define DECLARE_CONTEXTUAL_VARIABLE(VarName, ...) \
  struct VarName                                  \
      : v8::internal::torque::ContextualVariable<VarName, __VA_ARGS__> {};

// By inheriting from {ContextualClass} a class can become a contextual variable
// of itself.
template <class T>
using ContextualClass = ContextualVariable<T, T>;

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CONTEXTUAL_H_
