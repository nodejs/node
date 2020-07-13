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
class VisitorBase;
}  // namespace internal

using WeakCallback = void (*)(const LivenessBroker&, const void*);

/**
 * Visitor passed to trace methods. All managed pointers must have called the
 * visitor's trace method on them.
 */
class Visitor {
 public:
  template <typename T>
  void Trace(const Member<T>& member) {
    const T* value = member.GetRawAtomic();
    CPPGC_DCHECK(value != kSentinelPointer);
    Trace(value);
  }

  template <typename T>
  void Trace(const WeakMember<T>& weak_member) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T must be GarabgeCollected or GarbageCollectedMixin type");

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

  template <typename Persistent,
            std::enable_if_t<Persistent::IsStrongPersistent::value>* = nullptr>
  void TraceRoot(const Persistent& p, const SourceLocation& loc) {
    using PointeeType = typename Persistent::PointeeType;
    static_assert(sizeof(PointeeType),
                  "Persistent's pointee type must be fully defined");
    static_assert(internal::IsGarbageCollectedType<PointeeType>::value,
                  "Persisent's pointee type must be GarabgeCollected or "
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
                  "Persisent's pointee type must be GarabgeCollected or "
                  "GarbageCollectedMixin");
    VisitWeakRoot(p.Get(), TraceTrait<PointeeType>::GetTraceDescriptor(p.Get()),
                  &HandleWeak<WeakPersistent>, &p);
  }

  template <typename T, void (T::*method)(const LivenessBroker&)>
  void RegisterWeakCallbackMethod(const T* obj) {
    RegisterWeakCallback(&WeakCallbackMethodDelegate<T, method>, obj);
  }

  virtual void RegisterWeakCallback(WeakCallback, const void*) {}

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
    const auto* raw = weak->Get();
    if (raw && !info.IsHeapObjectAlive(raw)) {
      // Object is passed down through the marker as const. Alternatives are
      // - non-const Trace method;
      // - mutable pointer in MemberBase;
      const_cast<PointerType*>(weak)->Clear();
    }
  }

  Visitor() = default;

  template <typename T>
  void Trace(const T* t) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T must be GarabgeCollected or GarbageCollectedMixin type");
    if (!t) {
      return;
    }
    Visit(t, TraceTrait<T>::GetTraceDescriptor(t));
  }

  friend class internal::VisitorBase;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_VISITOR_H_
