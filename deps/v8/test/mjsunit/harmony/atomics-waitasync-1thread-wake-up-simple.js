// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function test() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  const result = Atomics.waitAsync(i32a, 0, 0);
  assertEquals(true, result.async);
  assertTrue(result.value instanceof Promise);
  assertEquals(1, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  let resolved = false;
   result.value.then(
    (value) => { assertEquals("ok", value); resolved = true; },
    () => { assertUnreachable(); });
  const notify_return_value = Atomics.notify(i32a, 0, 1);
  assertEquals(1, notify_return_value);
  assertEquals(0, %AtomicsNumWaitersForTesting(i32a, 0));
  assertEquals(1, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));

  setTimeout(()=> {
    assertTrue(resolved);
    assertEquals(0, %AtomicsNumUnresolvedAsyncPromisesForTesting(i32a, 0));
  }, 0);
})();
