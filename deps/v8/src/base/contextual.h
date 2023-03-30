// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_CONTEXTUAL_H_
#define V8_BASE_CONTEXTUAL_H_

#include <type_traits>

#include "src/base/export-template.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace v8::base {

// {ContextualVariable} provides a clean alternative to a global variable.
// The contextual variable is mutable, and supports managing the value of
// a variable in a well-nested fashion via the {Scope} class.
// {ContextualVariable} only stores a pointer to the current value, which
// is stored in a {Scope} object. The most recent value can be retrieved
// via Get(). Because only {Scope} has actual storage, there must be at
// least one active {Scope} (i.e. in a surrounding C++ scope), whenever Get()
// is called.
// Note that contextual variables must only be used from the same thread,
// i.e. {Scope} and Get() have to be in the same thread.
template <class Derived, class VarType>
class V8_EXPORT_PRIVATE ContextualVariable {
 public:
  using VarT = VarType;

  // A {Scope} contains a new object of type {VarType} and gives
  // ContextualVariable::Get() access to it. Upon destruction, the contextual
  // variable is restored to the state before the {Scope} was created. Scopes
  // have to follow a stack discipline:  A {Scope} has to be destructed before
  // any older scope is destructed.
  class V8_NODISCARD Scope {
   public:
    template <class... Args>
    explicit Scope(Args&&... args)
        : value_(std::forward<Args>(args)...), previous_(Top()) {
      Top() = this;
    }
    ~Scope() {
      // Ensure stack discipline.
      DCHECK_EQ(this, Top());
      Top() = previous_;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    VarType& Value() { return value_; }

   private:
    VarType value_;
    Scope* previous_;

    static_assert(std::is_base_of<ContextualVariable, Derived>::value,
                  "Curiously Recurring Template Pattern");

    DISALLOW_NEW_AND_DELETE()
  };

  static VarType& Get() {
    DCHECK(HasScope());
    return Top()->Value();
  }

  static bool HasScope() { return Top() != nullptr; }

 private:
  inline static thread_local Scope* top_ = nullptr;

  // If there is a linking error for `Top()`, then the contextual variable
  // probably needs to be exported using EXPORT_CONTEXTUAL_VARIABLE.
#if defined(USING_V8_SHARED)
  // Hide the definition from other DLLs/libraries to avoid access to `top_`,
  // since access to thread_local variables from other DLLs/libraries does not
  // work correctly.
  static Scope*& Top();
#else
  static Scope*& Top() { return top_; }
#endif
};

// Usage: DECLARE_CONTEXTUAL_VARIABLE(VarName, VarType)
#define DECLARE_CONTEXTUAL_VARIABLE(VarName, ...) \
  struct VarName : ::v8::base::ContextualVariable<VarName, __VA_ARGS__> {}

// Contextual variables that are accessed in tests need to be
// exported. For this, place the following macro in the global namespace inside
// of a .cc file.
#define EXPORT_CONTEXTUAL_VARIABLE(VarName)                    \
  namespace v8::base {                                         \
  template <>                                                  \
  V8_EXPORT_PRIVATE typename VarName::Scope*&                  \
  ContextualVariable<VarName, typename VarName::VarT>::Top() { \
    return top_;                                               \
  }                                                            \
  }

// By inheriting from {ContextualClass} a class can become a contextual variable
// of itself, which is very similar to a singleton.
template <class T>
using ContextualClass = ContextualVariable<T, T>;

}  // namespace v8::base

#endif  // V8_BASE_CONTEXTUAL_H_
