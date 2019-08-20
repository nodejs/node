// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_holdings_count = 0;
let cleanup = function(iter) {
  for (holdings of iter) {
    // See which target we're iterating over and unregister the other one.
    if (holdings == 1) {
      let success = fg.unregister(key2);
      assertTrue(success);
    } else {
      assertSame(holdings, 2);
      let success = fg.unregister(key1);
      assertTrue(success);
    }
    ++cleanup_holdings_count;
  }
  ++cleanup_call_count;
}

let fg = new FinalizationGroup(cleanup);
let key1 = {"k": "first key"};
let key2 = {"k": "second key"};
// Create two objects and register them in the FinalizationGroup. The objects
// need to be inside a closure so that we can reliably kill them!

(function() {
  let object1 = {};
  fg.register(object1, 1, key1);
  let object2 = {};
  fg.register(object2, 2, key2);

  // object1 and object2 go out of scope.
})();

// This GC will reclaim target objects and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called and iterated one holdings (but not the other one).
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(1, cleanup_holdings_count);
}

setTimeout(timeout_func, 0);
