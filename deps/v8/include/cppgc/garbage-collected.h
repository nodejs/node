// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_GARBAGE_COLLECTED_H_
#define INCLUDE_CPPGC_GARBAGE_COLLECTED_H_

#include <type_traits>

#include "cppgc/internal/api-constants.h"
#include "cppgc/platform.h"
#include "cppgc/trace-trait.h"
#include "cppgc/type-traits.h"

namespace cppgc {

class Visitor;

namespace internal {

class GarbageCollectedBase {
 public:
  // Must use MakeGarbageCollected.
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  // The garbage collector is taking care of reclaiming the object. Also,
  // virtual destructor requires an unambiguous, accessible 'operator delete'.
  void operator delete(void*) {
#ifdef V8_ENABLE_CHECKS
    internal::Abort();
#endif  // V8_ENABLE_CHECKS
  }
  void operator delete[](void*) = delete;

 protected:
  GarbageCollectedBase() = default;
};

}  // namespace internal

/**
 * Base class for managed objects. Only descendent types of GarbageCollected
 * can be constructed using MakeGarbageCollected. Must be inherited from as
 * left-most base class.
 *
 * Types inheriting from GarbageCollected must provide a method of
 * signature `void Trace(cppgc::Visitor*) const` that dispatchs all managed
 * pointers to the visitor and delegates to garbage-collected base classes.
 * The method must be virtual if the type is not directly a child of
 * GarbageCollected and marked as final.
 *
 * \code
 * // Example using final class.
 * class FinalType final : public GarbageCollected<FinalType> {
 *  public:
 *   void Trace(cppgc::Visitor* visitor) const {
 *     // Dispatch using visitor->Trace(...);
 *   }
 * };
 *
 * // Example using non-final base class.
 * class NonFinalBase : public GarbageCollected<NonFinalBase> {
 *  public:
 *   virtual void Trace(cppgc::Visitor*) const {}
 * };
 *
 * class FinalChild final : public NonFinalBase {
 *  public:
 *   void Trace(cppgc::Visitor* visitor) const final {
 *     // Dispatch using visitor->Trace(...);
 *     NonFinalBase::Trace(visitor);
 *   }
 * };
 * \endcode
 */
template <typename>
class GarbageCollected : public internal::GarbageCollectedBase {
 public:
  using IsGarbageCollectedTypeMarker = void;

 protected:
  GarbageCollected() = default;
};

/**
 * Base class for managed mixin objects. Such objects cannot be constructed
 * directly but must be mixed into the inheritance hierarchy of a
 * GarbageCollected object.
 *
 * Types inheriting from GarbageCollectedMixin must override a virtual method
 * of signature `void Trace(cppgc::Visitor*) const` that dispatchs all managed
 * pointers to the visitor and delegates to base classes.
 *
 * \code
 * class Mixin : public GarbageCollectedMixin {
 *  public:
 *   void Trace(cppgc::Visitor* visitor) const override {
 *     // Dispatch using visitor->Trace(...);
 *   }
 * };
 * \endcode
 */
class GarbageCollectedMixin : public internal::GarbageCollectedBase {
 public:
  using IsGarbageCollectedMixinTypeMarker = void;

  // Sentinel used to mark not-fully-constructed mixins.
  static constexpr void* kNotFullyConstructedObject = nullptr;

  // Provide default implementation that indicate that the vtable is not yet
  // set up properly. This is used to to get GCInfo objects for mixins so that
  // these objects can be processed later on.
  virtual TraceDescriptor GetTraceDescriptor() const {
    return {kNotFullyConstructedObject, nullptr};
  }

  /**
   * This Trace method must be overriden by objects inheriting from
   * GarbageCollectedMixin.
   */
  virtual void Trace(cppgc::Visitor*) const {}
};

/**
 * Macro defines all methods and markers needed for handling mixins. Must be
 * used on the type that is inheriting from GarbageCollected *and*
 * GarbageCollectedMixin.
 *
 * \code
 * class Mixin : public GarbageCollectedMixin {
 *  public:
 *   void Trace(cppgc::Visitor* visitor) const override {
 *     // Dispatch using visitor->Trace(...);
 *   }
 * };
 *
 * class Foo : public GarbageCollected<Foo>, public Mixin {
 *   USING_GARBAGE_COLLECTED_MIXIN();
 *  public:
 *   void Trace(cppgc::Visitor* visitor) const override {
 *     // Dispatch using visitor->Trace(...);
 *     Mixin::Trace(visitor);
 *   }
 * };
 * \endcode
 */
#define USING_GARBAGE_COLLECTED_MIXIN()                                      \
 public:                                                                     \
  /* Marker is used by clang to check for proper usages of the macro. */     \
  typedef int HasUsingGarbageCollectedMixinMacro;                            \
                                                                             \
  TraceDescriptor GetTraceDescriptor() const override {                      \
    static_assert(                                                           \
        internal::IsSubclassOfTemplate<                                      \
            std::remove_const_t<std::remove_pointer_t<decltype(this)>>,      \
            cppgc::GarbageCollected>::value,                                 \
        "Only garbage collected objects can have garbage collected mixins"); \
    return {this, TraceTrait<std::remove_const_t<                            \
                      std::remove_pointer_t<decltype(this)>>>::Trace};       \
  }                                                                          \
                                                                             \
 private:                                                                    \
  static_assert(true, "Force semicolon.")

/**
 * Merge two or more Mixins into one.
 *
 * \code
 * class A : public GarbageCollectedMixin {};
 * class B : public GarbageCollectedMixin {};
 * class C : public A, public B {
 *   MERGE_GARBAGE_COLLECTED_MIXINS();
 *  public:
 * };
 * \endcode
 */
#define MERGE_GARBAGE_COLLECTED_MIXINS()                \
 public:                                                \
  /* When using multiple mixins the methods become  */  \
  /* ambigous. Providing additional implementations */  \
  /* disambiguate them again.                       */  \
  TraceDescriptor GetTraceDescriptor() const override { \
    return {kNotFullyConstructedObject, nullptr};       \
  }                                                     \
                                                        \
 private:                                               \
  static_assert(true, "Force semicolon.")

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_GARBAGE_COLLECTED_H_
