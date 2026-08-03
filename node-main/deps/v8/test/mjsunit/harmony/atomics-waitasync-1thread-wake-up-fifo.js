// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function test() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  // Create 2 async waiters.
  const result1 = Atomics.waitAsync(i32a, 0, 0);
  const result2 = Atomics.waitAsync(i32a, 0, 0);

  assertEquals(true, result1.async);
  assertEquals(true, result2.async);
  assertEquals(2, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  let log = [];
  result1.value.then(
   (value) => { assertEquals("ok", value); log.push(1); },
   () => { assertUnreachable(); });
  result2.value.then(
    (value) => { assertEquals("ok", value); log.push(2); },
    () => { assertUnreachable(); });

  // Wake up one waiter.
  const notify_return_value = Atomics.notify(i32a, 0, 1);
  assertEquals(1, notify_return_value);
  assertEquals(1, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(1, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  function continuation1() {
    assertEquals(1, %AtomicsNumWaitersForTesting(i32a, 0));
    assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));
    assertEquals([1], log);

    // Wake up one waiter.
    const notify_return_value = Atomics.notify(i32a, 0, 1);
    assertEquals(1, notify_return_value);
    assertEquals(0, %AtomicsNumWaitersForTesting(i32a, 0));
    assertEquals(1, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

    setTimeout(continuation2, 0);
  }

  function continuation2() {
    assertEquals(0, %AtomicsNumWaitersForTesting(i32a, 0));
    assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));
    assertEquals([1, 2], log);
  }

  setTimeout(continuation1, 0);
})();
