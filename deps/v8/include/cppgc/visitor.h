// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_VISITOR_H_
#define INCLUDE_CPPGC_VISITOR_H_

#include "cppgc/garbage-collected.h"
#include "cppgc/internal/logging.h"
#include "cppgc/internal/pointer-policies.h"
#include "cppgc/liveness-broker.h"
#include "cppgc/member.h"
#include "cppgc/source-location.h"
#include "cppgc/trace-trait.h"

namespace cppgc {

namespace internal {
template <typename T, typename WeaknessPolicy, typename LocationPolicy,
          typename CheckingPolicy>
class BasicPersistent;
class ConservativeTracingVisitor;
class VisitorBase;
class VisitorFactory;

}  // namespace internal

using WeakCallback = void (*)(const LivenessBroker&, const void*);

/**
 * Visitor passed to trace methods. All managed pointers must have called the
 * Visitor's trace method on them.
 *
 * \code
 * class Foo final : public GarbageCollected<Foo> {
 *  public:
 *   void Trace(Visitor* visitor) const {
 *     visitor->Trace(foo_);
 *     visitor->Trace(weak_foo_);
 *   }
 *  private:
 *   Member<Foo> foo_;
 *   WeakMember<Foo> weak_foo_;
 * };
 * \endcode
 */
class Visitor {
 public:
  class Key {
   private:
    Key() = default;
    friend class internal::VisitorFactory;
  };

  explicit Visitor(Key) {}

  virtual ~Visitor() = default;

  /**
   * Trace method for Member.
   *
   * \param member Member reference retaining an object.
   */
  template <typename T>
  void Trace(const Member<T>& member) {
    const T* value = member.GetRawAtomic();
    CPPGC_DCHECK(value != kSentinelPointer);
    Trace(value);
  }

  /**
   * Trace method for WeakMember.
   *
   * \param weak_member WeakMember reference weakly retaining an object.
   */
  template <typename T>
  void Trace(const WeakMember<T>& weak_member) {
    static_assert(sizeof(T), "Pointee type must be fully defined.");
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T must be GarbageCollected or GarbageCollectedMixin type");

    const T* value = weak_member.GetRawAtomic();

    // Bailout assumes that WeakMember emits write barrier.
    if (!value) {
      return;
    }

    // TODO(chromium:1056170): DCHECK (or similar) for deleted values as they
    // should come in at a different path.
    VisitWeak(value, TraceTrait<T>::GetTraceDescriptor(value),
              &HandleWeak<WeakMember<T>>, &weak_member);
  }

  /**
   * Trace method for inlined objects that are not allocated themselves but
   * otherwise follow managed heap layout and have a Trace() method.
   *
   * \param object reference of the inlined object.
   */
  template <typename T>
  void Trace(const T& object) {
#if V8_ENABLE_CHECKS
    // This object is embedded in potentially multiple nested objects. The
    // outermost object must not be in construction as such objects are (a) not
    // processed immediately, and (b) only processed conservatively if not
    // otherwise possible.
    CheckObjectNotInConstruction(&object);
#endif  // V8_ENABLE_CHECKS
    TraceTrait<T>::Trace(this, &object);
  }

  /**
   * Registers a weak callback method on the object of type T. See
   * LivenessBroker for an usage example.
   *
   * \param object of type T specifying a weak callback method.
   */
  template <typename T, void (T::*method)(const LivenessBroker&)>
  void RegisterWeakCallbackMethod(const T* object) {
    RegisterWeakCallback(&WeakCallbackMethodDelegate<T, method>, object);
  }

  /**
   * Registers a weak callback that is invoked during garbage collection.
   *
   * \param callback to be invoked.
   * \param data custom data that is passed to the callback.
   */
  virtual void RegisterWeakCallback(WeakCallback callback, const void* data) {}

 protected:
  virtual void Visit(const void* self, TraceDescriptor) {}
  virtual void VisitWeak(const void* self, TraceDescriptor, WeakCallback,
                         const void* weak_member) {}
  virtual void VisitRoot(const void*, TraceDescriptor) {}
  virtual void VisitWeakRoot(const void* self, TraceDescriptor, WeakCallback,
                             const void* weak_root) {}

 private:
  template <typename T, void (T::*method)(const LivenessBroker&)>
  static void WeakCallbackMethodDelegate(const LivenessBroker& info,
                                         const void* self) {
    // Callback is registered through a potential const Trace method but needs
    // to be able to modify fields. See HandleWeak.
    (const_cast<T*>(static_cast<const T*>(self))->*method)(info);
  }

  template <typename PointerType>
  static void HandleWeak(const LivenessBroker& info, const void* object) {
    const PointerType* weak = static_cast<const PointerType*>(object);
    // Sentinel values are preserved for weak pointers.
    if (*weak == kSentinelPointer) return;
    const auto* raw = weak->Get();
    if (!info.IsHeapObjectAlive(raw)) {
      weak->ClearFromGC();
    }
  }

  template <typename Persistent,
            std::enable_if_t<Persistent::IsStrongPersistent::value>* = nullptr>
  void TraceRoot(const Persistent& p, const SourceLocation& loc) {
    using PointeeType = typename Persistent::PointeeType;
    static_assert(sizeof(PointeeType),
                  "Persistent's pointee type must be fully defined");
    static_assert(internal::IsGarbageCollectedType<PointeeType>::value,
                  "Persistent's pointee type must be GarbageCollected or "
                  "GarbageCollectedMixin");
    if (!p.Get()) {
      return;
    }
    VisitRoot(p.Get(), TraceTrait<PointeeType>::GetTraceDescriptor(p.Get()));
  }

  template <
      typename WeakPersistent,
      std::enable_if_t<!WeakPersistent::IsStrongPersistent::value>* = nullptr>
  void TraceRoot(const WeakPersistent& p, const SourceLocation& loc) {
    using PointeeType = typename WeakPersistent::PointeeType;
    static_assert(sizeof(PointeeType),
                  "Persistent's pointee type must be fully defined");
    static_assert(internal::IsGarbageCollectedType<PointeeType>::value,
                  "Persistent's pointee type must be GarbageCollected or "
                  "GarbageCollectedMixin");
    VisitWeakRoot(p.Get(), TraceTrait<PointeeType>::GetTraceDescriptor(p.Get()),
                  &HandleWeak<WeakPersistent>, &p);
  }

  template <typename T>
  void Trace(const T* t) {
    static_assert(sizeof(T), "Pointee type must be fully defined.");
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T must be GarbageCollected or GarbageCollectedMixin type");
    if (!t) {
      return;
    }
    Visit(t, TraceTrait<T>::GetTraceDescriptor(t));
  }

#if V8_ENABLE_CHECKS
  V8_EXPORT void CheckObjectNotInConstruction(const void* address);
#endif  // V8_ENABLE_CHECKS

  template <typename T, typename WeaknessPolicy, typename LocationPolicy,
            typename CheckingPolicy>
  friend class internal::BasicPersistent;
  friend class internal::ConservativeTracingVisitor;
  friend class internal::VisitorBase;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_VISITOR_H_
