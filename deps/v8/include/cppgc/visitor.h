// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_VISITOR_H_
#define INCLUDE_CPPGC_VISITOR_H_

#include "cppgc/custom-space.h"
#include "cppgc/ephemeron-pair.h"
#include "cppgc/garbage-collected.h"
#include "cppgc/internal/logging.h"
#include "cppgc/internal/pointer-policies.h"
#include "cppgc/liveness-broker.h"
#include "cppgc/member.h"
#include "cppgc/sentinel-pointer.h"
#include "cppgc/source-location.h"
#include "cppgc/trace-trait.h"
#include "cppgc/type-traits.h"

namespace cppgc {

namespace internal {
template <typename T, typename WeaknessPolicy, typename LocationPolicy,
          typename CheckingPolicy>
class BasicCrossThreadPersistent;
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
class V8_EXPORT Visitor {
 public:
  class Key {
   private:
    Key() = default;
    friend class internal::VisitorFactory;
  };

  explicit Visitor(Key) {}

  virtual ~Visitor() = default;

  /**
   * Trace method for raw pointers. Prefer the versions for managed pointers.
   *
   * \param member Reference retaining an object.
   */
  template <typename T>
  void Trace(const T* t) {
    static_assert(sizeof(T), "Pointee type must be fully defined.");
    static_assert(internal::IsGarbageCollectedOrMixinType<T>::value,
                  "T must be GarbageCollected or GarbageCollectedMixin type");
    if (!t) {
      return;
    }
    Visit(t, TraceTrait<T>::GetTraceDescriptor(t));
  }

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
    static_assert(internal::IsGarbageCollectedOrMixinType<T>::value,
                  "T must be GarbageCollected or GarbageCollectedMixin type");
    static_assert(!internal::IsAllocatedOnCompactableSpace<T>::value,
                  "Weak references to compactable objects are not allowed");

    const T* value = weak_member.GetRawAtomic();

    // Bailout assumes that WeakMember emits write barrier.
    if (!value) {
      return;
    }

    CPPGC_DCHECK(value != kSentinelPointer);
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
   * Trace method for EphemeronPair.
   *
   * \param ephemeron_pair EphemeronPair reference weakly retaining a key object
   * and strongly retaining a value object in case the key object is alive.
   */
  template <typename K, typename V>
  void Trace(const EphemeronPair<K, V>& ephemeron_pair) {
    TraceEphemeron(ephemeron_pair.key, &ephemeron_pair.value);
    RegisterWeakCallbackMethod<EphemeronPair<K, V>,
                               &EphemeronPair<K, V>::ClearValueIfKeyIsDead>(
        &ephemeron_pair);
  }

  /**
   * Trace method for a single ephemeron. Used for tracing a raw ephemeron in
   * which the `key` and `value` are kept separately.
   *
   * \param weak_member_key WeakMember reference weakly retaining a key object.
   * \param member_value Member reference with ephemeron semantics.
   */
  template <typename KeyType, typename ValueType>
  void TraceEphemeron(const WeakMember<KeyType>& weak_member_key,
                      const Member<ValueType>* member_value) {
    const KeyType* key = weak_member_key.GetRawAtomic();
    if (!key) return;

    // `value` must always be non-null.
    CPPGC_DCHECK(member_value);
    const ValueType* value = member_value->GetRawAtomic();
    if (!value) return;

    // KeyType and ValueType may refer to GarbageCollectedMixin.
    TraceDescriptor value_desc =
        TraceTrait<ValueType>::GetTraceDescriptor(value);
    CPPGC_DCHECK(value_desc.base_object_payload);
    const void* key_base_object_payload =
        TraceTrait<KeyType>::GetTraceDescriptor(key).base_object_payload;
    CPPGC_DCHECK(key_base_object_payload);

    VisitEphemeron(key_base_object_payload, value, value_desc);
  }

  /**
   * Trace method for a single ephemeron. Used for tracing a raw ephemeron in
   * which the `key` and `value` are kept separately. Note that this overload
   * is for non-GarbageCollected `value`s that can be traced though.
   *
   * \param key `WeakMember` reference weakly retaining a key object.
   * \param value Reference weakly retaining a value object. Note that
   *   `ValueType` here should not be `Member`. It is expected that
   *   `TraceTrait<ValueType>::GetTraceDescriptor(value)` returns a
   *   `TraceDescriptor` with a null base pointer but a valid trace method.
   */
  template <typename KeyType, typename ValueType>
  void TraceEphemeron(const WeakMember<KeyType>& weak_member_key,
                      const ValueType* value) {
    static_assert(!IsGarbageCollectedOrMixinTypeV<ValueType>,
                  "garbage-collected types must use WeakMember and Member");
    const KeyType* key = weak_member_key.GetRawAtomic();
    if (!key) return;

    // `value` must always be non-null.
    CPPGC_DCHECK(value);
    TraceDescriptor value_desc =
        TraceTrait<ValueType>::GetTraceDescriptor(value);
    // `value_desc.base_object_payload` must be null as this override is only
    // taken for non-garbage-collected values.
    CPPGC_DCHECK(!value_desc.base_object_payload);

    // KeyType might be a GarbageCollectedMixin.
    const void* key_base_object_payload =
        TraceTrait<KeyType>::GetTraceDescriptor(key).base_object_payload;
    CPPGC_DCHECK(key_base_object_payload);

    VisitEphemeron(key_base_object_payload, value, value_desc);
  }

  /**
   * Trace method that strongifies a WeakMember.
   *
   * \param weak_member WeakMember reference retaining an object.
   */
  template <typename T>
  void TraceStrongly(const WeakMember<T>& weak_member) {
    const T* value = weak_member.GetRawAtomic();
    CPPGC_DCHECK(value != kSentinelPointer);
    Trace(value);
  }

  /**
   * Trace method for weak containers.
   *
   * \param object reference of the weak container.
   * \param callback to be invoked.
   * \param data custom data that is passed to the callback.
   */
  template <typename T>
  void TraceWeakContainer(const T* object, WeakCallback callback,
                          const void* data) {
    if (!object) return;
    VisitWeakContainer(object, TraceTrait<T>::GetTraceDescriptor(object),
                       TraceTrait<T>::GetWeakTraceDescriptor(object), callback,
                       data);
  }

  /**
   * Registers a slot containing a reference to an object allocated on a
   * compactable space. Such references maybe be arbitrarily moved by the GC.
   *
   * \param slot location of reference to object that might be moved by the GC.
   */
  template <typename T>
  void RegisterMovableReference(const T** slot) {
    static_assert(internal::IsAllocatedOnCompactableSpace<T>::value,
                  "Only references to objects allocated on compactable spaces "
                  "should be registered as movable slots.");
    static_assert(!IsGarbageCollectedMixinTypeV<T>,
                  "Mixin types do not support compaction.");
    HandleMovableReference(reinterpret_cast<const void**>(slot));
  }

  /**
   * Registers a weak callback that is invoked during garbage collection.
   *
   * \param callback to be invoked.
   * \param data custom data that is passed to the callback.
   */
  virtual void RegisterWeakCallback(WeakCallback callback, const void* data) {}

  /**
   * Defers tracing an object from a concurrent thread to the mutator thread.
   * Should be called by Trace methods of types that are not safe to trace
   * concurrently.
   *
   * \param parameter tells the trace callback which object was deferred.
   * \param callback to be invoked for tracing on the mutator thread.
   * \param deferred_size size of deferred object.
   *
   * \returns false if the object does not need to be deferred (i.e. currently
   * traced on the mutator thread) and true otherwise (i.e. currently traced on
   * a concurrent thread).
   */
  virtual V8_WARN_UNUSED_RESULT bool DeferTraceToMutatorThreadIfConcurrent(
      const void* parameter, TraceCallback callback, size_t deferred_size) {
    // By default tracing is not deferred.
    return false;
  }

 protected:
  virtual void Visit(const void* self, TraceDescriptor) {}
  virtual void VisitWeak(const void* self, TraceDescriptor, WeakCallback,
                         const void* weak_member) {}
  virtual void VisitRoot(const void*, TraceDescriptor, const SourceLocation&) {}
  virtual void VisitWeakRoot(const void* self, TraceDescriptor, WeakCallback,
                             const void* weak_root, const SourceLocation&) {}
  virtual void VisitEphemeron(const void* key, const void* value,
                              TraceDescriptor value_desc) {}
  virtual void VisitWeakContainer(const void* self, TraceDescriptor strong_desc,
                                  TraceDescriptor weak_desc,
                                  WeakCallback callback, const void* data) {}
  virtual void HandleMovableReference(const void**) {}

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
    auto* raw_ptr = weak->GetFromGC();
    // Sentinel values are preserved for weak pointers.
    if (raw_ptr == kSentinelPointer) return;
    if (!info.IsHeapObjectAlive(raw_ptr)) {
      weak->ClearFromGC();
    }
  }

  template <typename Persistent,
            std::enable_if_t<Persistent::IsStrongPersistent::value>* = nullptr>
  void TraceRoot(const Persistent& p, const SourceLocation& loc) {
    using PointeeType = typename Persistent::PointeeType;
    static_assert(sizeof(PointeeType),
                  "Persistent's pointee type must be fully defined");
    static_assert(internal::IsGarbageCollectedOrMixinType<PointeeType>::value,
                  "Persistent's pointee type must be GarbageCollected or "
                  "GarbageCollectedMixin");
    auto* ptr = p.GetFromGC();
    if (!ptr) {
      return;
    }
    VisitRoot(ptr, TraceTrait<PointeeType>::GetTraceDescriptor(ptr), loc);
  }

  template <
      typename WeakPersistent,
      std::enable_if_t<!WeakPersistent::IsStrongPersistent::value>* = nullptr>
  void TraceRoot(const WeakPersistent& p, const SourceLocation& loc) {
    using PointeeType = typename WeakPersistent::PointeeType;
    static_assert(sizeof(PointeeType),
                  "Persistent's pointee type must be fully defined");
    static_assert(internal::IsGarbageCollectedOrMixinType<PointeeType>::value,
                  "Persistent's pointee type must be GarbageCollected or "
                  "GarbageCollectedMixin");
    static_assert(!internal::IsAllocatedOnCompactableSpace<PointeeType>::value,
                  "Weak references to compactable objects are not allowed");
    auto* ptr = p.GetFromGC();
    VisitWeakRoot(ptr, TraceTrait<PointeeType>::GetTraceDescriptor(ptr),
                  &HandleWeak<WeakPersistent>, &p, loc);
  }

#if V8_ENABLE_CHECKS
  void CheckObjectNotInConstruction(const void* address);
#endif  // V8_ENABLE_CHECKS

  template <typename T, typename WeaknessPolicy, typename LocationPolicy,
            typename CheckingPolicy>
  friend class internal::BasicCrossThreadPersistent;
  template <typename T, typename WeaknessPolicy, typename LocationPolicy,
            typename CheckingPolicy>
  friend class internal::BasicPersistent;
  friend class internal::ConservativeTracingVisitor;
  friend class internal::VisitorBase;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_VISITOR_H_
