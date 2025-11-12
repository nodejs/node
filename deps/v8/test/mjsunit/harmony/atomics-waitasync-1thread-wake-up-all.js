// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function test() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  const N = 10;
  let log = [];

  // Create N async waiters.
  for (let i = 0; i < N; ++i) {
    const result = Atomics.waitAsync(i32a, 0, 0);
    assertEquals(true, result.async);
    result.value.then(
      (value) => { assertEquals("ok", value); log.push(i); },
      () => { assertUnreachable(); });
  }
  assertEquals(N, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  // Wake up all waiters.
  let notify_return_value = Atomics.notify(i32a, 0);
  assertEquals(N, notify_return_value);
  assertEquals(0, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(N, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  function continuation() {
    assertEquals(N, log.length);
    for (let i = 0; i < N; ++i) {
        assertEquals(i, log[i]);
    }
  }

  setTimeout(continuation, 0);
})();
