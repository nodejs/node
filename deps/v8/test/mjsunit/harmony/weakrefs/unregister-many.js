// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_holdings_count = 0;
let cleanup = function(iter) {
  for (holdings of iter) {
    assertEquals("holdings2", holdings);
    ++cleanup_holdings_count;
  }
  ++cleanup_call_count;
}

let fg = new FinalizationGroup(cleanup);
let key1 = {"k": "key1"};
let key2 = {"k": "key2"};
// Create three objects and register them in the FinalizationGroup. The objects
// need to be inside a closure so that we can reliably kill them!

(function() {
  let object1a = {};
  fg.register(object1a, "holdings1a", key1);

  let object1b = {};
  fg.register(object1b, "holdings1b", key1);

  let object2 = {};
  fg.register(object2, "holdings2", key2);

  // Unregister before the GC has a chance to discover the objects.
  fg.unregister(key1);

  // objects go out of scope.
})();

// This GC will reclaim the target objects.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function will be called only for the reference which
// was not unregistered.
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(1, cleanup_holdings_count);
}

setTimeout(timeout_func, 0);
