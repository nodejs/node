// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_call_count = 0;
  let cleanup = function(holdings) {
    assertEquals(holdings, "holdings");

    // There's one more object with the same key that we haven't
    // cleaned up yet so we should be able to unregister the
    // callback for that one.
    let success = fg.unregister(key);
    assertTrue(success);

    ++cleanup_call_count;
  }

  let fg = new FinalizationRegistry(cleanup);
  // Create an object and register it in the FinalizationRegistry. The object needs to be inside
  // a closure so that we can reliably kill them!
  let key = {"k": "this is the key"};

  (function() {
    let object = {};
    let object2 = {};
    fg.register(object, "holdings", key);
    fg.register(object2, "holdings", key);

    // object goes out of scope.
  })();

  // This GC will discover dirty WeakCells and schedule cleanup.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(0, cleanup_call_count);

  // Assert that the cleanup function was called.
  let timeout_func = function() {
    assertEquals(1, cleanup_call_count);
  }

  setTimeout(timeout_func, 0);

})();
