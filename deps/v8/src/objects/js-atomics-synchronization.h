// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_
#define V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_

#include <atomic>

#include "src/execution/thread-id.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-atomics-synchronization-tq.inc"

namespace detail {
class WaiterQueueNode;
}  // namespace detail

// A non-recursive mutex that is exposed to JS.
//
// It has the following properties:
//   - Slim: 8-12 bytes. Lock state is 4 bytes when
//     V8_SANDBOXED_EXTERNAL_POINTERS, and sizeof(void*) otherwise. Owner
//     thread is an additional 4 bytes.
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
    : public TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex, JSObject> {
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

  V8_EXPORT_PRIVATE static Handle<JSAtomicsMutex> Create(Isolate* isolate);

  // Lock the mutex, blocking if it's currently owned by another thread.
  static inline void Lock(Isolate* requester, Handle<JSAtomicsMutex> mutex);

  V8_WARN_UNUSED_RESULT inline bool TryLock();

  inline void Unlock(Isolate* requester);

  inline bool IsHeld();
  inline bool IsCurrentThreadOwner();

  static constexpr int kEndOfTaggedFieldsOffset = JSObject::kHeaderSize;
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(JSAtomicsMutex)

 private:
  friend class detail::WaiterQueueNode;

  // There are 2 lock bits: whether the lock itself is locked, and whether the
  // associated waiter queue is locked.
  static constexpr int kIsLockedBit = 1 << 0;
  static constexpr int kIsWaiterQueueLockedBit = 1 << 1;
  static constexpr int kLockBitsSize = 2;

#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  using StateT = uint32_t;
  static_assert(sizeof(StateT) == kExternalPointerSlotSize);
#else
  using StateT = uintptr_t;
#endif

  static constexpr StateT kUnlocked = 0;
  static constexpr StateT kLockedUncontended = 1;

  static constexpr StateT kLockBitsMask = (1 << kLockBitsSize) - 1;
  static constexpr StateT kWaiterQueueHeadMask = ~kLockBitsMask;

  inline void SetCurrentThreadAsOwner();
  inline void ClearOwnerThread();

  inline std::atomic<StateT>* AtomicStatePtr();
  inline std::atomic<int32_t>* AtomicOwnerThreadIdPtr();

  bool TryLockExplicit(std::atomic<StateT>* state, StateT& expected);
  bool TryLockWaiterQueueExplicit(std::atomic<StateT>* state, StateT& expected);

  V8_EXPORT_PRIVATE static void LockSlowPath(Isolate* requester,
                                             Handle<JSAtomicsMutex> mutex,
                                             std::atomic<StateT>* state);
  V8_EXPORT_PRIVATE void UnlockSlowPath(Isolate* requester,
                                        std::atomic<StateT>* state);

  using TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex, JSObject>::state;
  using TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex, JSObject>::set_state;
  using TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex,
                                      JSObject>::owner_thread_id;
  using TorqueGeneratedJSAtomicsMutex<JSAtomicsMutex,
                                      JSObject>::set_owner_thread_id;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_H_
