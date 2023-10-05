// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_
#define V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_

#include <atomic>

#include "src/base/platform/time.h"
#include "src/execution/thread-id.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-atomics-synchronization-tq.inc"

namespace detail {
class WaiterQueueNode;
}  // namespace detail

// Base class for JSAtomicsMutex and JSAtomicsCondition
class JSSynchronizationPrimitive
    : public TorqueGeneratedJSSynchronizationPrimitive<
          JSSynchronizationPrimitive, AlwaysSharedSpaceJSObject> {
 public:
  // Synchronization only store raw data as state.
  static constexpr int kEndOfTaggedFieldsOffset = JSObject::kHeaderSize;
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(JSSynchronizationPrimitive)

 protected:
#ifdef V8_COMPRESS_POINTERS
  using StateT = uint32_t;
  static_assert(sizeof(StateT) == sizeof(ExternalPointerHandle));
#else
  using StateT = uintptr_t;
#endif  // V8_COMPRESS_POINTERS

  inline std::atomic<StateT>* AtomicStatePtr();

  using TorqueGeneratedJSSynchronizationPrimitive<
      JSSynchronizationPrimitive, AlwaysSharedSpaceJSObject>::state;
  using TorqueGeneratedJSSynchronizationPrimitive<
      JSSynchronizationPrimitive, AlwaysSharedSpaceJSObject>::set_state;
};

// A non-recursive mutex that is exposed to JS.
//
// It has the following properties:
//   - Slim: 8-12 bytes. Lock state is 4 bytes when V8_COMPRESS_POINTERS, and
//     sizeof(void*) otherwise. Owner thread is an additional 4 bytes.
//   - Fast when uncontended: a single weak CAS.
//   - Possibly unfair under contention.
//   - Moving GC safe. It uses an index into the shared Isolate's external
//     pointer table to store a queue of sleeping threads.
//   - Parks the main thread LocalHeap when the thread is blocked on acquiring
//     the lock. Unparks the main thread LocalHeap when unblocked. This means
//     that the lock can only be used with main thread isolates (including
//     workers) but not with helper threads that have their own LocalHeap.
//
// This mutex manages its own queue of waiting threads under contention, i.e.
// it implements a futex in userland. The algorithm is inspired by WebKit's
// ParkingLot.
class JSAtomicsMutex
    : public TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex,
                                           JSSynchronizationPrimitive> {
 public:
  // A non-copyable wrapper class that provides an RAII-style mechanism for
  // owning the JSAtomicsMutex.
  //
  // The mutex is locked when a LockGuard object is created, and unlocked when
  // the LockGuard object is destructed.
  class V8_NODISCARD LockGuard final {
   public:
    inline LockGuard(Isolate* isolate, Handle<JSAtomicsMutex> mutex);
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    inline ~LockGuard();

   private:
    Isolate* isolate_;
    Handle<JSAtomicsMutex> mutex_;
  };

  // A non-copyable wrapper class that provides an RAII-style mechanism for
  // attempting to take ownership of a JSAtomicsMutex via its TryLock method.
  //
  // The mutex is attempted to be locked via TryLock when a TryLockGuard object
  // is created. If the mutex was acquired, then it is released when the
  // TryLockGuard object is destructed.
  class V8_NODISCARD TryLockGuard final {
   public:
    inline TryLockGuard(Isolate* isolate, Handle<JSAtomicsMutex> mutex);
    TryLockGuard(const TryLockGuard&) = delete;
    TryLockGuard& operator=(const TryLockGuard&) = delete;
    inline ~TryLockGuard();
    bool locked() const { return locked_; }

   private:
    Isolate* isolate_;
    Handle<JSAtomicsMutex> mutex_;
    bool locked_;
  };

  DECL_CAST(JSAtomicsMutex)
  DECL_PRINTER(JSAtomicsMutex)
  EXPORT_DECL_VERIFIER(JSAtomicsMutex)

  // Lock the mutex, blocking if it's currently owned by another thread.
  static inline void Lock(Isolate* requester, Handle<JSAtomicsMutex> mutex);

  V8_WARN_UNUSED_RESULT inline bool TryLock();

  inline void Unlock(Isolate* requester);

  inline bool IsHeld();
  inline bool IsCurrentThreadOwner();

  TQ_OBJECT_CONSTRUCTORS(JSAtomicsMutex)

 private:
  friend class Factory;
  friend class detail::WaiterQueueNode;

  // There are 2 lock bits: whether the lock itself is locked, and whether the
  // associated waiter queue is locked.
  static constexpr int kIsLockedBit = 1 << 0;
  static constexpr int kIsWaiterQueueLockedBit = 1 << 1;
  static constexpr int kLockBitsSize = 2;

  static constexpr StateT kUnlocked = 0;
  static constexpr StateT kLockedUncontended = 1;

  static constexpr StateT kLockBitsMask = (1 << kLockBitsSize) - 1;
  static constexpr StateT kWaiterQueueHeadMask = ~kLockBitsMask;

  inline void SetCurrentThreadAsOwner();
  inline void ClearOwnerThread();

  inline std::atomic<int32_t>* AtomicOwnerThreadIdPtr();

  static bool TryLockExplicit(std::atomic<StateT>* state, StateT& expected);
  static bool TryLockWaiterQueueExplicit(std::atomic<StateT>* state,
                                         StateT& expected);

  V8_EXPORT_PRIVATE static void LockSlowPath(Isolate* requester,
                                             Handle<JSAtomicsMutex> mutex,
                                             std::atomic<StateT>* state);
  V8_EXPORT_PRIVATE void UnlockSlowPath(Isolate* requester,
                                        std::atomic<StateT>* state);

  using TorqueGeneratedJSAtomicsMutex<
      JSAtomicsMutex, JSSynchronizationPrimitive>::owner_thread_id;
  using TorqueGeneratedJSAtomicsMutex<
      JSAtomicsMutex, JSSynchronizationPrimitive>::set_owner_thread_id;
};

// A condition variable that is exposed to JS.
//
// It has the following properties:
//   - Slim: 4-8 bytes. Lock state is 4 bytes when V8_COMPRESS_POINTERS, and
//     sizeof(void*) otherwise.
//   - Moving GC safe. It uses an index into the shared Isolate's external
//     pointer table to store a queue of sleeping threads.
//   - Parks the main thread LocalHeap when waiting. Unparks the main thread
//     LocalHeap after waking up.
//
// This condition variable manages its own queue of waiting threads, like
// JSAtomicsMutex. The algorithm is inspired by WebKit's ParkingLot.
class JSAtomicsCondition
    : public TorqueGeneratedJSAtomicsCondition<JSAtomicsCondition,
                                               JSSynchronizationPrimitive> {
 public:
  DECL_CAST(JSAtomicsCondition)
  DECL_PRINTER(JSAtomicsCondition)
  EXPORT_DECL_VERIFIER(JSAtomicsCondition)

  V8_EXPORT_PRIVATE static bool WaitFor(
      Isolate* requester, Handle<JSAtomicsCondition> cv,
      Handle<JSAtomicsMutex> mutex, base::Optional<base::TimeDelta> timeout);

  static constexpr uint32_t kAllWaiters = UINT32_MAX;

  // Notify {count} waiters. Returns the number of waiters woken up.
  V8_EXPORT_PRIVATE uint32_t Notify(Isolate* requester, uint32_t count);

  Tagged<Object> NumWaitersForTesting(Isolate* isolate);

  TQ_OBJECT_CONSTRUCTORS(JSAtomicsCondition)

 private:
  friend class Factory;
  friend class detail::WaiterQueueNode;

  // There is 1 lock bit: whether the waiter queue is locked.
  static constexpr int kIsWaiterQueueLockedBit = 1 << 0;
  static constexpr int kLockBitsSize = 1;

  static constexpr StateT kEmptyState = 0;
  static constexpr StateT kLockBitsMask = (1 << kLockBitsSize) - 1;
  static constexpr StateT kWaiterQueueHeadMask = ~kLockBitsMask;

  static bool TryLockWaiterQueueExplicit(std::atomic<StateT>* state,
                                         StateT& expected);

  using DequeueAction =
      std::function<detail::WaiterQueueNode*(detail::WaiterQueueNode**)>;
  static detail::WaiterQueueNode* DequeueExplicit(
      Isolate* requester, std::atomic<StateT>* state,
      const DequeueAction& dequeue_action);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_
