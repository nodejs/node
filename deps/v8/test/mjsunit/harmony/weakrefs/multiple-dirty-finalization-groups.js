// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_call_count = 0;
  let cleanup = function (holdings) {
    ++cleanup_call_count;
  }

  let fg1 = new FinalizationRegistry(cleanup);
  let fg2 = new FinalizationRegistry(cleanup);

  // Create two objects and register them in FinalizationRegistries. The objects need
  // to be inside a closure so that we can reliably kill them!

  (function () {
    let object1 = {};
    fg1.register(object1, "holdings1");

    let object2 = {};
    fg2.register(object2, "holdings2");

    // object1 and object2 go out of scope.
  })();

  // This GC will discover dirty WeakCells and schedule cleanup.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(0, cleanup_call_count);

  // Assert that the cleanup function was called.
  let timeout_func = function () {
    assertEquals(2, cleanup_call_count);
  }

  setTimeout(timeout_func, 0);

})();
