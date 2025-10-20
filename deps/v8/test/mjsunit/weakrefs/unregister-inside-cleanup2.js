// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_call_count = 0;
  let cleanup = function(holdings) {
    // See which target we're cleaning up and unregister the other one.
    if (holdings == 1) {
      let success = fg.unregister(key2);
      assertTrue(success);
    } else {
      assertSame(holdings, 2);
      let success = fg.unregister(key1);
      assertTrue(success);
    }
    ++cleanup_call_count;
  }

  let fg = new FinalizationRegistry(cleanup);
  let key1 = {"k": "first key"};
  let key2 = {"k": "second key"};
  // Create two objects and register them in the FinalizationRegistry. The objects
  // need to be inside a closure so that we can reliably kill them!

  (function() {
    let object1 = {};
    fg.register(object1, 1, key1);
    let object2 = {};
    fg.register(object2, 2, key2);

    // object1 and object2 go out of scope.
  })();

  // This GC will reclaim target objects and schedule cleanup.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(0, cleanup_call_count);

  // Assert that the cleanup function was called and cleaned up one holdings (but not the other one).
  let timeout_func = function() {
    assertEquals(1, cleanup_call_count);
  }

  setTimeout(timeout_func, 0);

})();
