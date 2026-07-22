// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

(async function () {

  let call_count = 0;
  const reentrant_gc = function (holdings) {
    gc();
    call_count++;
  }

  const fg = new FinalizationRegistry(reentrant_gc);

  (function () {
    fg.register({}, 42);
  })();

  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'minor', execution: 'async' });
  assertEquals(0, call_count);

  setTimeout(function () { assertEquals(1, call_count); }, 0);

 })();
