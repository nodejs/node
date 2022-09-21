// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

let mutex = new Atomics.Mutex;
let cv = new Atomics.Condition;

(function TestConditionWaitNotAllowed() {
  assertThrows(() => {
    Atomics.Mutex.lock(mutex, () => {
      %SetAllowAtomicsWait(false);
      Atomics.Condition.wait(cv, mutex);
    });
  });
  %SetAllowAtomicsWait(true);
})();

(function TestConditionMutexNotHeld() {
  // Cannot wait on a mutex not owned by the current thread.
  assertThrows(() => {
    Atomics.Condition.wait(cv, mutex);
  });
})();

(function TestConditionNoWaiters() {
  // Notify returns number of threads woken up.
  assertEquals(0, Atomics.Condition.notify(cv));
})();

(function TestConditionWaitTimeout() {
  Atomics.Mutex.lock(mutex, () => {
    assertEquals(false, Atomics.Condition.wait(cv, mutex, 100));
  });
})();
