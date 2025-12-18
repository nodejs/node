// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_call_count = 0;
  let cleanup = function(holdings) {
    assertEquals("holdings2", holdings);
    ++cleanup_call_count;
  }

  let fg = new FinalizationRegistry(cleanup);
  let key1 = {"k": "key1"};
  let key2 = {"k": "key2"};
  // Create three objects and register them in the FinalizationRegistry. The objects
  // need to be inside a closure so that we can reliably kill them!

  (function() {
    let object1a = {};
    fg.register(object1a, "holdings1a", key1);

    let object1b = {};
    fg.register(object1b, "holdings1b", key1);

    let object2 = {};
    fg.register(object2, "holdings2", key2);

    // Unregister before the GC has a chance to discover the objects.
    let success = fg.unregister(key1);
    assertTrue(success);

    // objects go out of scope.
  })();

  // This GC will reclaim the target objects.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(0, cleanup_call_count);

  // Assert that the cleanup function will be called only for the reference which
  // was not unregistered.
  let timeout_func = function() {
    assertEquals(1, cleanup_call_count);
  }

  setTimeout(timeout_func, 0);

})();
