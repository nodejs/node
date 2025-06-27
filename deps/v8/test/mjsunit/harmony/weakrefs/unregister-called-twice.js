// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_call_count = 0;
  let cleanup = function (holdings) {
    ++cleanup_call_count;
  }

  let fg = new FinalizationRegistry(cleanup);
  let key = { "k": "this is the key" };
  // Create an object and register it in the FinalizationRegistry. The object needs
  // to be inside a closure so that we can reliably kill them!

  (function () {
    let object = {};
    fg.register(object, "holdings", key);

    // Unregister before the GC has a chance to discover the object.
    let success = fg.unregister(key);
    assertTrue(success);

    // Call unregister again (just to assert we handle this gracefully).
    success = fg.unregister(key);
    assertFalse(success);

    // object goes out of scope.
  })();

  // This GC will reclaim the target object.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(0, cleanup_call_count);

  // Assert that the cleanup function won't be called, since the weak reference
  // was unregistered.
  let timeout_func = function () {
    assertEquals(0, cleanup_call_count);
  }

  setTimeout(timeout_func, 0);

})();
