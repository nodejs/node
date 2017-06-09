// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FUTEX_EMULATION_H_
#define V8_FUTEX_EMULATION_H_

#include <stdint.h>

#include "src/allocation.h"
#include "src/base/atomicops.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

// Support for emulating futexes, a low-level synchronization primitive. They
// are natively supported by Linux, but must be emulated for other platforms.
// This library emulates them on all platforms using mutexes and condition
// variables for consistency.
//
// This is used by the Futex API defined in the SharedArrayBuffer draft spec,
// found here: https://github.com/tc39/ecmascript_sharedmem

namespace v8 {

namespace base {
class TimeDelta;
}  // base

namespace internal {

template <typename T>
class Handle;
class Isolate;
class JSArrayBuffer;

class FutexWaitListNode {
 public:
  FutexWaitListNode()
      : prev_(nullptr),
        next_(nullptr),
        backing_store_(nullptr),
        wait_addr_(0),
        waiting_(false),
        interrupted_(false) {}

  void NotifyWake();

 private:
  friend class FutexEmulation;
  friend class FutexWaitList;

  base::ConditionVariable cond_;
  FutexWaitListNode* prev_;
  FutexWaitListNode* next_;
  void* backing_store_;
  size_t wait_addr_;
  bool waiting_;
  bool interrupted_;

  DISALLOW_COPY_AND_ASSIGN(FutexWaitListNode);
};


class FutexWaitList {
 public:
  FutexWaitList();

  void AddNode(FutexWaitListNode* node);
  void RemoveNode(FutexWaitListNode* node);

 private:
  friend class FutexEmulation;

  FutexWaitListNode* head_;
  FutexWaitListNode* tail_;

  DISALLOW_COPY_AND_ASSIGN(FutexWaitList);
};


class FutexEmulation : public AllStatic {
 public:
  // Pass to Wake() to wake all waiters.
  static const uint32_t kWakeAll = UINT32_MAX;

  // Check that array_buffer[addr] == value, and return "not-equal" if not. If
  // they are equal, block execution on |isolate|'s thread until woken via
  // |Wake|, or when the time given in |rel_timeout_ms| elapses. Note that
  // |rel_timeout_ms| can be Infinity.
  // If woken, return "ok", otherwise return "timed-out". The initial check and
  // the decision to wait happen atomically.
  static Object* Wait(Isolate* isolate, Handle<JSArrayBuffer> array_buffer,
                      size_t addr, int32_t value, double rel_timeout_ms);

  // Wake |num_waiters_to_wake| threads that are waiting on the given |addr|.
  // |num_waiters_to_wake| can be kWakeAll, in which case all waiters are
  // woken. The rest of the waiters will continue to wait. The return value is
  // the number of woken waiters.
  static Object* Wake(Isolate* isolate, Handle<JSArrayBuffer> array_buffer,
                      size_t addr, uint32_t num_waiters_to_wake);

  // Return the number of threads waiting on |addr|. Should only be used for
  // testing.
  static Object* NumWaitersForTesting(Isolate* isolate,
                                      Handle<JSArrayBuffer> array_buffer,
                                      size_t addr);

 private:
  friend class FutexWaitListNode;

  static base::LazyMutex mutex_;
  static base::LazyInstance<FutexWaitList>::type wait_list_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_FUTEX_EMULATION_H_
